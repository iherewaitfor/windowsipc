#include "namepipeipc.h"
#include <process.h>
#include <iostream>
#include <stdio.h>
#include <Windows.h>

#define PIPE_TIMEOUT 5000
#define INSTANCES 1 

unsigned int __stdcall ThreadOverlappedServer(PVOID pThis);
unsigned int __stdcall ThreadOverlappedClient(PVOID pThis);

NamedPipeIpc::NamedPipeIpc(const std::string& namePipe, NamedPipeType type):m_pipeName(namePipe), m_type(type){


}

NamedPipeIpc::~NamedPipeIpc() {

}
bool NamedPipeIpc::init() {
    bool success = false;
    if (m_type == TYPE_SERVER) {
        success = initNamedpipeServer();
    }
    else {
        success = initNamedpipeClient();
    }
	return success;
}

bool NamedPipeIpc::initNamedpipeClient() {
    BOOL   fSuccess = FALSE;

    // Try to open a named pipe; wait for it, if necessary. 

    while (1)
    {
        m_hClientPipe = CreateFile(
            this->m_pipeName.c_str(),   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            FILE_FLAG_OVERLAPPED,              // default attributes 
            NULL);          // no template file 

      // Break if the pipe handle is valid. 

        if (m_hClientPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            printf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
            return false;
        }

        // All pipe instances are busy, so wait for 20 seconds. 

        if (!WaitNamedPipe(this->m_pipeName.c_str(), 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return false;
        }
    }
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
        m_hClientPipe,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        printf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return false;
    }

    {
        int index = 1;
        pipeOverlappeds[index].handleFile = m_hClientPipe;     //ReadFile
        pipeOverlappeds[index + 1].handleFile = m_hClientPipe;   //WriteFile
        events[index] = pipeOverlappeds[index].hEvent;
        events[index + 1] = pipeOverlappeds[index + 1].hEvent;
    }
    // Create a non-signalled, manual-reset event,
    // thread stop event
    //退出事件需要叫醒所有的线程，使用手动重置事件
    HANDLE hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return false;
    }
    events[INSTANCES * 3] = hEvent;  //工作线程停止事件

    //只需要叫醒一个线程。使用自动重置事件
    hEvent = ::CreateEvent(0, FALSE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return false;
    }
    events[INSTANCES * 3 + 1] = hEvent;  //发送消息队列非空事件

    hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return false;
    }
    events[0] = hEvent;  //为了与服务器端兼容，用于占位。不使用

    m_hThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadOverlappedClient, this, 0, &m_threadID);
    return true;
}

void NamedPipeIpc::close() {
    SetEvent(events[3 * INSTANCES]);
    WaitForSingleObject(m_hThread, 5000);

}
bool NamedPipeIpc::sendMsg(const std::string& msg) {
    {
        AutoCsLock scopeLock(writeMsgsListLock);
        writeMsgsList.push_back(msg);
        if (writeMsgsList.size() == 1) {
            SetEvent(events[INSTANCES * 3 + 1]); // not empty
        }
    }
	return true;
}
std::list<std::string> NamedPipeIpc::getMsgs() {
	std::list<std::string>tempMsgs;
	{
		AutoCsLock scopeLock(readMsgsListLock);
		tempMsgs.swap(readMsgsList);
	}
	return tempMsgs;
}


bool NamedPipeIpc::initNamedpipeServer() {
    //LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");
    {
        HANDLE pipeHandle = CreateNamedPipeA(
            m_pipeName.c_str(),            // pipe name 
            PIPE_ACCESS_DUPLEX |     // read/write access 
            FILE_FLAG_OVERLAPPED,    // overlapped mode 
            PIPE_TYPE_MESSAGE |      // message-type pipe 
            PIPE_READMODE_MESSAGE |  // message-read mode 
            PIPE_WAIT,               // blocking mode 
            INSTANCES,               // number of instances 
            BUFSIZE * sizeof(TCHAR),   // output buffer size 
            BUFSIZE * sizeof(TCHAR),   // input buffer size 
            PIPE_TIMEOUT,            // client time-out 
            NULL);                   // default security attributes 

        if (pipeHandle == INVALID_HANDLE_VALUE)
        {
            printf("CreateNamedPipe failed with %d.\n", GetLastError());
            return false;
        }
        int index = 0;
        pipeOverlappeds[index].handleFile = pipeHandle;     //ConnectNamedPipe
        pipeOverlappeds[index + 1].handleFile = pipeHandle;   //ReadFile
        pipeOverlappeds[index + 2].handleFile = pipeHandle;   //WriteFile
        events[index] = pipeOverlappeds[index].hEvent;
        events[index + 1] = pipeOverlappeds[index + 1].hEvent;
        events[index + 2] = pipeOverlappeds[index + 2].hEvent;
    }
    // Create a non-signalled, manual-reset event,
    // thread stop event
    HANDLE hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return false;
    }
    events[INSTANCES * 3] = hEvent;  //工作线程停止事件

    hEvent = ::CreateEvent(0, FALSE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return false;
    }
    events[INSTANCES * 3 + 1] = hEvent;  //发送消息队列不为空事件

    printf("Creating worker thread...\n");
    // Create the worker thread.
    m_hThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadOverlappedServer, this, 0, &m_threadID);

    return true;
}

BOOL NamedPipeIpc::ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
    BOOL fConnected, fPendingIO = FALSE;

    // Start an overlapped connection for this pipe instance. 
    fConnected = ConnectNamedPipe(hPipe, lpo);

    // Overlapped ConnectNamedPipe should return zero. 
    if (fConnected)
    {
        printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        return FALSE;
    }

    switch (GetLastError())
    {
            // The overlapped connection in progress. 
        case ERROR_IO_PENDING:
            fPendingIO = TRUE;
            break;

            // Client is already connected, so signal an event. 

        case ERROR_PIPE_CONNECTED:
            if (SetEvent(lpo->hEvent))
                break;

            // If an error occurs during the connect operation... 
        default:
        {
            printf("ConnectNamedPipe failed with %d.\n", GetLastError());
            return FALSE;
        }
    }

    return fPendingIO;
}



unsigned int __stdcall ThreadOverlappedServer(PVOID pThis)
{
    NamedPipeIpc* ipc = (NamedPipeIpc*)pThis;
    printf("beginThread 线程ID号为%4d \n", GetCurrentThreadId());

    for (int i = 0; i < INSTANCES; i++) {
        ipc->ConnectToNewClient(ipc->pipeOverlappeds[i * 3].handleFile, &ipc->pipeOverlappeds[i * 3]);
    }
    bool bWritting = false;
    DWORD dwWait;
    bool bStop = false;
    while (!bStop) {
        dwWait = WaitForMultipleObjects(
            INSTANCES * 3 + 2,    // number of event objects 
            ipc->events,      // array of event objects 
            FALSE,        // does not wait for all 
            INFINITE);    // waits indefinitely 

      // dwWait shows which pipe completed the operation. 

        DWORD waitIndex = dwWait - WAIT_OBJECT_0;
        if (waitIndex == INSTANCES * 3) {
            bStop = true;
            break;
        }
        if (waitIndex == INSTANCES * 3 + 1) {
            //send msg list is not empty
            if (!ipc->handleNotEmptyEvent(waitIndex, bWritting)) {
                break;
            }
            continue;
        }
        if (waitIndex > INSTANCES * 3 + 1) {
            std::cout << "error waitIndex:" << waitIndex << "  error:" << GetLastError();
            bStop = true;
            break;
        }
        DWORD index = waitIndex / 3;
        if (waitIndex == 3 * index) {
            //ConnectNamedPipe
            ipc->handleConnectEvent(waitIndex);
        }
        else if (waitIndex == 3 * index + 1) {
            //read done
            if (!ipc->handleReadEvent(waitIndex)) {
                continue;
            }
        }
        else {
            //write done
            if (!ipc->handleWriteEvent(waitIndex, bWritting)) {
                break;
            }
        }
    }
    std::cout << "worker thread exit" << std::endl;

    return 0;
}


void NamedPipeIpc::handleConnectEvent(int waitIndex)
{
    m_serverConected = true;
    // to read
    PipeOverLapped* pReadOverLapped = &pipeOverlappeds[waitIndex + 1];
    bool fSuccess = ReadFile(
        pReadOverLapped->handleFile,
        pReadOverLapped->readBuff,
        BUFSIZE * sizeof(TCHAR),
        &pReadOverLapped->cbRead,
        pReadOverLapped);
}
bool NamedPipeIpc::handleNotEmptyEvent(int waitIndex, bool& bWritting) {
    if (bWritting) {
        return true;
    }
    writeMsg(bWritting);
    return true;
}

bool NamedPipeIpc::handleReadEvent(int waitIndex) {
    bool success = false;
    if (NamedPipeType::TYPE_SERVER == m_type) {
        success = handleReadEventServer(waitIndex);
    } else {
        success = handleReadEventClient(waitIndex);
    }
    return success;
}

bool NamedPipeIpc::handleReadEventServer(int waitIndex) {
    if (pipeOverlappeds[waitIndex].Internal != 0) {
        std::cout << "read failed. GetLastError:" << GetLastError() << std::endl;
        ULONG clientProcessId = 0;
        GetNamedPipeClientProcessId(pipeOverlappeds[waitIndex].handleFile, &clientProcessId);
        std::cout << "read failed. GetLastError:" << GetLastError() << " disconnect the client of process:" << clientProcessId << std::endl;
        DisconnectNamedPipe(pipeOverlappeds[waitIndex].handleFile); //读取错误，断开连接
        m_serverConected = false;
        // to do: notice the client closed.


        PipeOverLapped* pConnectOverLapped = &pipeOverlappeds[waitIndex - 1];
        ConnectToNewClient(pConnectOverLapped->handleFile, pConnectOverLapped); //重新等待连接
        std::cout << "to connect to new client." << std::endl;


        return false;
    }
    std::cout << pipeOverlappeds[waitIndex].readBuff << std::endl;
    // to do add the data to the read list;
    {
        AutoCsLock scopLock(readMsgsListLock);
        std::string msg;
        msg.assign(pipeOverlappeds[waitIndex].readBuff, pipeOverlappeds[waitIndex].InternalHigh);
        readMsgsList.push_back(msg);
    }
    ZeroMemory(pipeOverlappeds[waitIndex].readBuff, sizeof(pipeOverlappeds[waitIndex].readBuff));

    PipeOverLapped* pReadOverLapped = &pipeOverlappeds[waitIndex];
    bool fSuccess = ReadFile(
        pReadOverLapped->handleFile,
        pReadOverLapped->readBuff,
        BUFSIZE * sizeof(TCHAR),
        &pReadOverLapped->cbRead,
        pReadOverLapped);
    return true;
}

bool NamedPipeIpc::handleReadEventClient(int waitIndex) {
    if (pipeOverlappeds[waitIndex].Internal != 0) {
        std::cout << "read failed. GetLastError:" << GetLastError() << std::endl;

        if (pipeOverlappeds[waitIndex].handleFile != 0) {
            CloseHandle(pipeOverlappeds[waitIndex].handleFile);
            pipeOverlappeds[waitIndex].handleFile = NULL;
        }
        return false;
    }
    std::cout << pipeOverlappeds[waitIndex].readBuff << std::endl;
    // to do add the data to the read list;
    {
        AutoCsLock scopLock(readMsgsListLock);
        std::string msg;
        msg.assign(pipeOverlappeds[waitIndex].readBuff, pipeOverlappeds[waitIndex].InternalHigh);
        readMsgsList.push_back(msg);
    }

    ZeroMemory(pipeOverlappeds[waitIndex].readBuff, sizeof(pipeOverlappeds[waitIndex].readBuff));
    ReadFile(
        pipeOverlappeds[waitIndex].handleFile,
        pipeOverlappeds[waitIndex].readBuff,
        BUFSIZE * sizeof(TCHAR),
        &pipeOverlappeds[waitIndex].cbRead,
        &pipeOverlappeds[waitIndex]);
    return true;
}

bool NamedPipeIpc::handleWriteEvent(int waitIndex, bool& bWritting) {
    ZeroMemory(pipeOverlappeds[waitIndex].writeBuffer, sizeof(pipeOverlappeds[waitIndex].writeBuffer));
    bWritting = false;
    if (pipeOverlappeds[waitIndex].Internal != 0) {
        std::cout << "write failed. GetLastError:" << GetLastError() << std::endl;

        if (pipeOverlappeds[waitIndex].handleFile != 0) {
            CloseHandle(pipeOverlappeds[waitIndex].handleFile);
            pipeOverlappeds[waitIndex].handleFile = NULL;
        }
        return false;
    }
    writeMsg(bWritting);
    return true;
}

bool NamedPipeIpc::writeMsg( bool& bWritting) {
    do {
        if (writeMsgsList.empty()) {
            break;
        }
        //没有在发送,则触发发送。
        std::string msg;
        {
            AutoCsLock scopeLock(writeMsgsListLock);
            msg = writeMsgsList.front();
            writeMsgsList.pop_front();
        }
        PipeOverLapped* pWriteOverLapped = &pipeOverlappeds[2];
        ZeroMemory(pWriteOverLapped->writeBuffer, sizeof(pWriteOverLapped->writeBuffer));
        memcpy(pWriteOverLapped->writeBuffer, msg.c_str(), msg.length());
        pWriteOverLapped->cbToWrite = msg.length();
        WriteFile(
            pWriteOverLapped->handleFile,                  // pipe handle 
            pWriteOverLapped->writeBuffer,             // message 
            pWriteOverLapped->cbToWrite,              // message length 
            &pWriteOverLapped->cbToWrite,             // bytes written 
            pWriteOverLapped);                  // overlapped
        bWritting = true;
    } while (false);
    return true;
}

unsigned int __stdcall ThreadOverlappedClient(PVOID pThis)
{
    NamedPipeIpc* ipc = (NamedPipeIpc*)pThis;
    printf("线程ID号为%4d的子线程说：Hello World\n", GetCurrentThreadId());
    int indexRead = 1;
    
    ZeroMemory(ipc->pipeOverlappeds[indexRead].readBuff, sizeof(ipc->pipeOverlappeds[indexRead].readBuff));
    ReadFile(
        ipc->pipeOverlappeds[indexRead].handleFile,
        ipc->pipeOverlappeds[indexRead].readBuff,
        BUFSIZE * sizeof(TCHAR),
        &ipc->pipeOverlappeds[indexRead].cbRead,
        &ipc->pipeOverlappeds[indexRead]);

    bool bWritting = false;
    DWORD dwWait;
    bool bStop = false;
    while (!bStop) {
        dwWait = WaitForMultipleObjects(
            3+2,    // number of event objects 
            ipc->events,      // array of event objects 
            FALSE,        // does not wait for all 
            INFINITE);    // waits indefinitely 

      // dwWait shows which event completed. 

        DWORD waitIndex = dwWait - WAIT_OBJECT_0;
        if (waitIndex > INSTANCES * 3 + 1 || waitIndex < 0) {
            std::cout << "error waitIndex:" << waitIndex << "  error:" << GetLastError();
            bStop = true;
            break;
        }
        if (waitIndex == INSTANCES * 3) { // exit
            if (ipc->m_hClientPipe) {

                FlushFileBuffers(ipc->m_hClientPipe);
                CloseHandle(ipc->m_hClientPipe);
                ipc->m_hClientPipe = NULL;
            }
            bStop = true;
            break;
        }
        if (waitIndex == INSTANCES * 3 + 1) {
            //send msg list is not empty
            if (!ipc->handleNotEmptyEvent(waitIndex, bWritting)) {
                break;
            }
            continue;
        }
        DWORD index = waitIndex / 3;
        if (waitIndex == 3 * index +1) {
            //read done
            if (!ipc->handleReadEvent(waitIndex)) {
                break;
            }
        }
        else {
            //write done
            if (!ipc->handleWriteEvent(waitIndex, bWritting)) {
                break;
            }
        }
    }
    std::cout << "worker thread exit" << std::endl;
    return 0;
}