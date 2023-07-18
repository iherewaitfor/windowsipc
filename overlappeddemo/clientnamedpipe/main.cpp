#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <iostream>

#include <process.h>
#include <list>
#include <map>
#include "cslock.h"
#include <string>

#define INSTANCES 1 
#define BUFSIZE 512
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

//ReadFile
//WriteFile
PipeOverLapped pipeOverlappeds[INSTANCES * 2];

//最后一个是工作线程结束事件
HANDLE events[INSTANCES * 2 + 2];

//to do
std::list<std::string> readMsgsList;
CsLock readMsgsListLock;
void dispatchMsgs();

std::list<std::string> writeMsgsList;
CsLock writeMsgsListLock;

bool handleNotEmptyEvent(int waitIndex, bool bWritting);
bool handleReadEvent(int waitIndex);
bool handleWriteEvent(int waitIndex, bool &bWritting);
unsigned int __stdcall ThreadOverlapped(PVOID pM);
 
int _tmain(int argc, TCHAR *argv[]) 
{
    HANDLE hThread;
    unsigned threadID;
    printf("Creating workder thread...\n");
    // Create the worker thread.
    hThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadOverlapped, NULL, 0, &threadID);


    TCHAR sendBuf[BUFSIZE] = { 0 };
    DWORD  cbToWrite;
 ////test//////////////////////////////////////////////////////////////
   std::cout << "please input message to server.(input \"exit\" to exit)." << std::endl;
   do {
       // Send a message to the pipe server. 
       ZeroMemory(sendBuf, BUFSIZE);
       char  ch;
       int i = 0;
       while (std::cin >> std::noskipws >> ch) {
           if (ch == '\n') {
               break;
           }
           sendBuf[i++] = ch;
       }
       cbToWrite = (lstrlen(sendBuf) + 1) * sizeof(TCHAR);
       _tprintf(TEXT("process %d Sending %d byte message: \"%s\"\n"), GetCurrentProcessId(), cbToWrite, sendBuf);

       int cmpResult =  strcmp(sendBuf, "exit");
       if (cmpResult == 0) {

           SetEvent(events[2 * INSTANCES]);//exit
           WaitForSingleObject(hThread, 5000);
           break;
       }
       std::string msg;
       msg.assign(sendBuf, cbToWrite);
       {
           AutoCsLock scopeLock(writeMsgsListLock);
           writeMsgsList.push_back(msg);
           if (writeMsgsList.size() == 1) {
               SetEvent(events[INSTANCES * 2 + 1]); // not empty
           }
       }
       dispatchMsgs();

   } while (true);

   printf("\n<End of message, press ENTER to terminate connection and exit>");
   _getch();
  
   return 0; 
}

unsigned int __stdcall ThreadOverlapped(PVOID pM)
{
    printf("线程ID号为%4d的子线程说：Hello World\n", GetCurrentThreadId());
    HANDLE hPipe;
    LPTSTR lpvMessage = TEXT("Default message from client.");
    BOOL   fSuccess = FALSE;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

    // Try to open a named pipe; wait for it, if necessary. 

    while (1)
    {
        hPipe = CreateFile(
            lpszPipename,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            FILE_FLAG_OVERLAPPED,              // default attributes 
            NULL);          // no template file 

      // Break if the pipe handle is valid. 

        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            _tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
            return -1;
        }

        // All pipe instances are busy, so wait for 20 seconds. 

        if (!WaitNamedPipe(lpszPipename, 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return -1;
        }
    }
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
        hPipe,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    for (int i = 0; i < INSTANCES; i++)
    {
        int index = i * 2;
        pipeOverlappeds[index].handleFile = hPipe;     //ReadFile
        pipeOverlappeds[index + 1].handleFile = hPipe;   //WriteFile
        events[index] = pipeOverlappeds[index].hEvent;
        events[index + 1] = pipeOverlappeds[index + 1].hEvent;
    }
    // Create a non-signalled, manual-reset event,
    // thread stop event
    HANDLE hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return -2;
    }
    events[INSTANCES * 2] = hEvent;  //工作线程停止事件

    hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return -1;
    }
    events[INSTANCES * 2  + 1] = hEvent;  //发送消息队列非空事件

    ZeroMemory(pipeOverlappeds[0].readBuff, sizeof(pipeOverlappeds[0].readBuff));
    ReadFile(
        pipeOverlappeds[0].handleFile,
        pipeOverlappeds[0].readBuff,
        BUFSIZE * sizeof(TCHAR),
        &pipeOverlappeds[0].cbRead,
        &pipeOverlappeds[0]);

    bool bWritting = false;
    DWORD dwWait;
    bool bStop = false;
    while (!bStop) {
        dwWait = WaitForMultipleObjects(
            INSTANCES * 2 + 2,    // number of event objects 
            events,      // array of event objects 
            FALSE,        // does not wait for all 
            INFINITE);    // waits indefinitely 

      // dwWait shows which event completed. 

        DWORD waitIndex = dwWait - WAIT_OBJECT_0;
        if (waitIndex > INSTANCES * 2 + 1 || waitIndex < 0) {
            std::cout << "error waitIndex:" << waitIndex << "  error:" << GetLastError();
            bStop = true;
            break;
        }
        if (waitIndex == INSTANCES * 2) { // exit
            bStop = true;
            break;
        }
        if (waitIndex == INSTANCES * 2 + 1) {
            //send msg list is not empty
            if (!handleNotEmptyEvent(waitIndex, bWritting)) {
                break;
            }
            continue;
        }
        DWORD index = waitIndex / 2;
        if (waitIndex == 2 * index) {
            //read done
            if (!handleReadEvent(waitIndex)) {
                break;
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

void sendMsg(int index) {
    AutoCsLock scopLock(writeMsgsListLock);
    std::string msg;
    msg.assign(pipeOverlappeds[index].readBuff, pipeOverlappeds[index].InternalHigh);
    //memcpy()
    //readMsgsList.push_back(msg);
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

bool handleReadEvent(int waitIndex) {
    if (pipeOverlappeds[waitIndex].Internal != 0) {
        std::cout << "read failed. GetLastError:" << GetLastError() << std::endl;

        if (pipeOverlappeds[waitIndex].handleFile != 0) {
            CloseHandle(pipeOverlappeds[waitIndex].handleFile);
            pipeOverlappeds[waitIndex].handleFile = NULL;
        }
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
    ResetEvent(events[waitIndex]);

    ZeroMemory(pipeOverlappeds[waitIndex].readBuff, sizeof(pipeOverlappeds[waitIndex].readBuff));
    ReadFile(
        pipeOverlappeds[waitIndex].handleFile,
        pipeOverlappeds[waitIndex].readBuff,
        BUFSIZE * sizeof(TCHAR),
        &pipeOverlappeds[waitIndex].cbRead,
        &pipeOverlappeds[waitIndex]);
    return true;
}


bool handleWriteEvent(int waitIndex, bool& bWritting) {
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

bool handleNotEmptyEvent(int waitIndex, bool bWritting) {
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
        PipeOverLapped* pWriteOverLapped = &pipeOverlappeds[1];
        ZeroMemory(pWriteOverLapped->writeBuffer, sizeof(pWriteOverLapped->writeBuffer));
        memcpy(pWriteOverLapped->writeBuffer, msg.c_str(), msg.length());
        pWriteOverLapped->cbToWrite = msg.length();
        WriteFile(
            pWriteOverLapped->handleFile,                  // pipe handle 
            pWriteOverLapped->writeBuffer,             // message 
            msg.length(),              // message length 
            &pWriteOverLapped->cbToWrite,             // bytes written 
            pWriteOverLapped);                  // overlapped 
    } while (false);
    ResetEvent(events[waitIndex]);
    return true;
}