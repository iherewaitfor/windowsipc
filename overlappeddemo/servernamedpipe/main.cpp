#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <iostream>

#include <process.h>

#define INSTANCES 4 
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

//最后一个是工作线程结束事件
HANDLE events[INSTANCES*3 +1];

//to do 
// define an read array
// define an write array

// define an CS fro read array
// define an CS for write array

TCHAR sendBuf[BUFSIZE] = { 0 };

BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo);

unsigned int __stdcall ThreadOverlapped(PVOID pM)
{
    printf("线程ID号为%4d的子线程说：Hello World\n", GetCurrentThreadId());

    for (int i = 0; i < INSTANCES; i++) {
        ConnectToNewClient(pipeOverlappeds[i * 3].handleFile, &pipeOverlappeds[i * 3]);
    }
    DWORD dwWait;
    bool bStop = false;
    while (!bStop) {
        dwWait = WaitForMultipleObjects(
            INSTANCES * 3 + 1,    // number of event objects 
            events,      // array of event objects 
            FALSE,        // does not wait for all 
            INFINITE);    // waits indefinitely 

      // dwWait shows which pipe completed the operation. 

        DWORD waitIndex = dwWait - WAIT_OBJECT_0;
        if (waitIndex == INSTANCES * 3 ) {
            bStop = true;
            break;
        }
        if (waitIndex > INSTANCES * 3) {
            std::cout << "error waitIndex:" << waitIndex << "  error:" << GetLastError();
            bStop = true;
            break;
        }
        DWORD index = waitIndex / 3;
        if (waitIndex == 3 * index) {
            //ConnectNamedPipe
            // to read or write
            // to do: read
            PipeOverLapped* pReadOverLapped = &pipeOverlappeds[waitIndex + 1];
            bool fSuccess = ReadFile(
                pReadOverLapped->handleFile,
                pReadOverLapped->readBuff,
                BUFSIZE * sizeof(TCHAR),
                &pReadOverLapped->cbRead,
                pReadOverLapped);

            ResetEvent(events[waitIndex]);

        }
        else if (waitIndex == 3 * index + 1) {
            //read done
            if (pipeOverlappeds[waitIndex].Internal != 0) {
                std::cout << "read failed. GetLastError:" << GetLastError() << std::endl;
                ULONG clientProcessId = 0;
                GetNamedPipeClientProcessId(pipeOverlappeds[waitIndex].handleFile, &clientProcessId);
                std::cout << "read failed. GetLastError:" << GetLastError() << " disconnect the client of process:" << clientProcessId << std::endl;
                DisconnectNamedPipe(pipeOverlappeds[waitIndex].handleFile); //读取错误，断开连接

                PipeOverLapped* pConnectOverLapped = &pipeOverlappeds[waitIndex - 1];
                ConnectToNewClient(pConnectOverLapped->handleFile, pConnectOverLapped); //重新等待连接
                std::cout << "to connect to new client." <<std::endl;


                ResetEvent(events[waitIndex]);
                continue;
            }
            std::cout << pipeOverlappeds[waitIndex].readBuff << std::endl;
            // to do add the data to the read list;
            ZeroMemory(pipeOverlappeds[waitIndex].readBuff, sizeof(pipeOverlappeds[waitIndex].readBuff));


            // test : to write respone immedialy.
            PipeOverLapped* pWriteOverLapped = &pipeOverlappeds[waitIndex + 1];
            StringCchCopy(pWriteOverLapped->writeBuffer, BUFSIZE, TEXT("Default answer from server"));
            pWriteOverLapped->cbToWrite = (lstrlen(pWriteOverLapped->writeBuffer) + 1) * sizeof(TCHAR);

            WriteFile(
                pWriteOverLapped->handleFile,
                pWriteOverLapped->writeBuffer,
                pWriteOverLapped->cbToWrite,
                &pWriteOverLapped->cbToWrite,
                pWriteOverLapped
            );

            ResetEvent(events[waitIndex]);
        }
        else {
            //write done
            ZeroMemory(pipeOverlappeds[waitIndex].writeBuffer, sizeof(pipeOverlappeds[waitIndex].writeBuffer));

            PipeOverLapped* pReadOverLapped = &pipeOverlappeds[waitIndex -1];
            bool fSuccess = ReadFile(
                pReadOverLapped->handleFile,
                pReadOverLapped->readBuff,
                BUFSIZE * sizeof(TCHAR),
                &pReadOverLapped->cbRead,
                pReadOverLapped);

            ResetEvent(events[waitIndex]);
        }        
    }
    std::cout << "worker thread exit" << std::endl;

    return 0;
}
//主函

int _tmain(VOID)
{
    DWORD i, dwWait, cbRet, dwErr;
    BOOL fSuccess;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

    // The initial loop creates several instances of a named pipe 
    // along with an event object for each instance.  An 
    // overlapped ConnectNamedPipe operation is started for 
    // each instance. 

    for (i = 0; i < INSTANCES; i++)
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
            return 0;
        }
        int index = i * 3;
        pipeOverlappeds[index].handleFile = pipeHandle;     //ConnectNamedPipe
        pipeOverlappeds[index+1].handleFile = pipeHandle;   //ReadFile
        pipeOverlappeds[index+2].handleFile = pipeHandle;   //WriteFile
        events[index] = pipeOverlappeds[index].hEvent;
        events[index+1] = pipeOverlappeds[index+1].hEvent;
        events[index+2] = pipeOverlappeds[index+2].hEvent;
    }
    // Create a non-signalled, manual-reset event,
    // thread stop event
    HANDLE hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
    }
    events[INSTANCES * 3] = hEvent;  //工作线程停止事件
    HANDLE hThread;
    unsigned threadID;

    printf("Creating second thread...\n");

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
        DWORD cbToWrite = (lstrlen(sendBuf) + 1) * sizeof(TCHAR);
        _tprintf(TEXT("process %d Sending %d byte message: \"%s\"\n"), GetCurrentProcessId(), cbToWrite, sendBuf);

        int cmpResult = strcmp(sendBuf, "exit");
        if (cmpResult == 0) {
            SetEvent(events[3 * INSTANCES]);
            WaitForSingleObject(hThread, 5000);
            std::cout << " main thread exit." << std::endl;
            break;
        }
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