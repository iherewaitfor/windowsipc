#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <iostream>

#include <process.h>
#include <list>
#include <map>
#include "cslock.h"

#define INSTANCES 1 
#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

struct  PipeOverLapped : public OVERLAPPED
{
    HANDLE handleFile;
    TCHAR readBuff[BUFSIZE];
    DWORD cbRead;
    TCHAR writeBuffer[BUFSIZE];
    DWORD cbToWrite;
    PipeOverLapped() {
        Internal = 0;
        InternalHigh = 0;
        Offset = 0;
        OffsetHigh = 0;

        //customer
        handleFile = NULL;
        cbRead = 0;
        cbToWrite = 0;

        ZeroMemory(readBuff, sizeof(readBuff));
        ZeroMemory(writeBuffer, sizeof(readBuff));

        // Create a non-signalled, manual-reset event,
        hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
        if (!hEvent)
        {
            DWORD last_error = ::GetLastError();
            last_error;
        }
    }
    virtual ~PipeOverLapped() {
        if (hEvent) {
            ::CloseHandle(hEvent);
        }
    }
};

HANDLE hEvents[INSTANCES];

//ConnectNamedPipe
//ReadFile
//WriteFile
PipeOverLapped pipeOverlappeds[INSTANCES * 3];

//最后的事件：
//工作线程结束事件、待发送消息队列非空事件
HANDLE events[INSTANCES*3 +2];


std::list<std::string> readMsgsList;
CsLock readMsgsListLock;
void dispatchMsgs();//to do 

// define an write array
std::list<std::string> writeMsgsList;
CsLock writeMsgsListLock;

TCHAR sendBuf[BUFSIZE] = { 0 };

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo);

bool initNamedpipe();

void handleConnectEvent(int waitIndex);
bool handleNotEmptyEvent(int waitIndex, bool &bWritting);
bool handleReadEvent(int waitIndex);
bool handleWriteEvent(int waitIndex, bool& bWritting);
unsigned int __stdcall ThreadOverlapped(PVOID pM);

int _tmain(VOID)
{
    if (!initNamedpipe()) {
        return -1;
    }

    HANDLE hThread;
    unsigned threadID;

    printf("Creating worker thread...\n");
    // Create the worker thread.
    hThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadOverlapped, NULL, 0, &threadID);

    do {
        // Send a message to all valid pipe. 
        ZeroMemory(sendBuf, BUFSIZE);
        char  ch;
        int i = 0;
        while (std::cin >> std::noskipws >> ch) {
            if (ch == '\n') {
                break;
            }
            sendBuf[i++] = ch;
        }
        DWORD cWrite = (lstrlen(sendBuf) + 1) * sizeof(TCHAR);

        int cmpResult = strcmp(sendBuf, "exit");
        if (cmpResult == 0) {
            SetEvent(events[3 * INSTANCES]);
            WaitForSingleObject(hThread, 5000);
            std::cout << " main thread exit." << std::endl;
            break;
        }

        std::string msg;
        msg.assign(sendBuf, cWrite);
        {
            AutoCsLock scopeLock(writeMsgsListLock);
            writeMsgsList.push_back(msg);
            if (writeMsgsList.size() == 1) {
                SetEvent(events[INSTANCES * 3 + 1]); // not empty
            }
        }

        //模拟在消息循环时分发已经收到的消息。比如主线程可定时读取消息队列
        dispatchMsgs();
    } while (true);

    return 0;
}


BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
    BOOL fConnected, fPendingIO = FALSE;

    // Start an overlapped connection for this pipe instance. 
    fConnected = ConnectNamedPipe(hPipe, lpo);

    // Overlapped ConnectNamedPipe should return zero. 
    if (fConnected)
    {
        printf("ConnectNamedPipe failed with %d.\n", GetLastError());
        return 0;
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
        return 0;
    }
    }

    return fPendingIO;
}

void dispatchMsgs() {
    std::list<std::string> tempReadList;
    {
        //取出列表后，立即释放锁
        AutoCsLock scopLock(readMsgsListLock);
        tempReadList.swap(readMsgsList);
    }
    // to do , process the recieved msgs;
    // dispatch msg to the listenning bussiness.
    std::cout << " dispatchMsgs " << std::endl;
    while (!tempReadList.empty()) {
        std::cout << tempReadList.front() << std::endl;
        tempReadList.pop_front();
    }
}

unsigned int __stdcall ThreadOverlapped(PVOID pM)
{
    printf("beginThread 线程ID号为%4d \n", GetCurrentThreadId());

    for (int i = 0; i < INSTANCES; i++) {
        ConnectToNewClient(pipeOverlappeds[i * 3].handleFile, &pipeOverlappeds[i * 3]);
    }
    bool bWritting = false;
    DWORD dwWait;
    bool bStop = false;
    while (!bStop) {
        dwWait = WaitForMultipleObjects(
            INSTANCES * 3 + 2,    // number of event objects 
            events,      // array of event objects 
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
            if (!handleNotEmptyEvent(waitIndex, bWritting)) {
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
            handleConnectEvent(waitIndex);
        }
        else if (waitIndex == 3 * index + 1) {
            //read done
            if (!handleReadEvent(waitIndex)) {
                continue;
            }
        }
        else {
            //write done
            if (!handleWriteEvent(waitIndex, bWritting)) {
                break;
            }
        }
    }
    std::cout << "worker thread exit" << std::endl;

    return 0;
}

void handleConnectEvent(int waitIndex) {
    // to read
    PipeOverLapped* pReadOverLapped = &pipeOverlappeds[waitIndex + 1];
    bool fSuccess = ReadFile(
        pReadOverLapped->handleFile,
        pReadOverLapped->readBuff,
        BUFSIZE * sizeof(TCHAR),
        &pReadOverLapped->cbRead,
        pReadOverLapped);
    ResetEvent(events[waitIndex]);
}
bool handleNotEmptyEvent(int waitIndex, bool &bWritting) {
    do {
        if (bWritting || writeMsgsList.empty()) {
            return false;
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
    ResetEvent(events[waitIndex]);
    return true;
}

bool handleReadEvent(int waitIndex) {
    if (pipeOverlappeds[waitIndex].Internal != 0) {
        std::cout << "read failed. GetLastError:" << GetLastError() << std::endl;
        ULONG clientProcessId = 0;
        GetNamedPipeClientProcessId(pipeOverlappeds[waitIndex].handleFile, &clientProcessId);
        std::cout << "read failed. GetLastError:" << GetLastError() << " disconnect the client of process:" << clientProcessId << std::endl;
        DisconnectNamedPipe(pipeOverlappeds[waitIndex].handleFile); //读取错误，断开连接

        PipeOverLapped* pConnectOverLapped = &pipeOverlappeds[waitIndex - 1];
        ConnectToNewClient(pConnectOverLapped->handleFile, pConnectOverLapped); //重新等待连接
        std::cout << "to connect to new client." << std::endl;


        ResetEvent(events[waitIndex]);
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

    ResetEvent(events[waitIndex]);
    PipeOverLapped* pReadOverLapped = &pipeOverlappeds[waitIndex];
    bool fSuccess = ReadFile(
        pReadOverLapped->handleFile,
        pReadOverLapped->readBuff,
        BUFSIZE * sizeof(TCHAR),
        &pReadOverLapped->cbRead,
        pReadOverLapped);
    return true;
}

bool handleWriteEvent(int waitIndex, bool& bWritting) {
    ZeroMemory(pipeOverlappeds[waitIndex].writeBuffer, sizeof(pipeOverlappeds[waitIndex].writeBuffer));
    bWritting = false;
    ResetEvent(events[waitIndex]);
    if (pipeOverlappeds[waitIndex].Internal != 0) {
        std::cout << "write failed. GetLastError:" << GetLastError() << std::endl;

        if (pipeOverlappeds[waitIndex].handleFile != 0) {
            CloseHandle(pipeOverlappeds[waitIndex].handleFile);
            pipeOverlappeds[waitIndex].handleFile = NULL;
        }
        return false;
    }
    PipeOverLapped* pWriteOverLapped = &pipeOverlappeds[waitIndex];
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
        ZeroMemory(pipeOverlappeds[waitIndex].writeBuffer, sizeof(pipeOverlappeds[waitIndex].writeBuffer));
        memcpy(pipeOverlappeds[waitIndex].writeBuffer, msg.c_str(), msg.length());
        pipeOverlappeds[waitIndex].cbToWrite = msg.length();
        WriteFile(
            pipeOverlappeds[waitIndex].handleFile,                  // pipe handle 
            pipeOverlappeds[waitIndex].writeBuffer,             // message 
            pipeOverlappeds[waitIndex].cbToWrite,              // message length 
            &pipeOverlappeds[waitIndex].cbToWrite,             // bytes written 
            &pipeOverlappeds[waitIndex]);                  // overlapped 
        bWritting = true;
    } while (false);
    return true;
}

bool initNamedpipe() {
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");
    for (int i = 0; i < INSTANCES; i++)
    {
        HANDLE pipeHandle = CreateNamedPipe(
            lpszPipename,            // pipe name 
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
        int index = i * 3;
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

    hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return false;
    }
    events[INSTANCES * 3 + 1] = hEvent;  //发送消息队列不为空事件
    return true;
}