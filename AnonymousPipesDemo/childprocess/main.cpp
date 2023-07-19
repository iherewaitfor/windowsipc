#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include "cslock.h"
#include <iostream>
#include <process.h>

#define BUFSIZE 4096 

std::list<std::string> readMsgsList;
CsLock readMsgsListLock;
void dispatchMsgs();//to do 
unsigned int __stdcall ThreadRead(PVOID pM);

// define an write array
std::list<std::string> writeMsgsList;
CsLock writeMsgsListLock;
//写线程事件
//发送队列非空事件、线程退出事件
HANDLE events[2];
unsigned int __stdcall ThreadWrite(PVOID pM);

HANDLE hStdin = INVALID_HANDLE_VALUE;
HANDLE hStdout = INVALID_HANDLE_VALUE;

volatile bool g_mainThreadStop = false;
HANDLE hReadThread;
HANDLE hWriteThread;
 
int main(int argc, char** argv) 
{ 
   DWORD dwWritten;
   
 
   //hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 
   //hStdin = GetStdHandle(STD_INPUT_HANDLE); 
   //if ( 
   //    (hStdout == INVALID_HANDLE_VALUE) || 
   //    (hStdin == INVALID_HANDLE_VALUE) 
   //   ) 
   //   ExitProcess(1); 

   LPSTR pStr = GetCommandLine();
 
   if (argc < 3) {
       return 1;
   }
   std::string strWriteHandle;
   strWriteHandle.assign(argv[1], strlen(argv[1])); // writeHandle=284 

   std::string strReadHandle = argv[2];             // readHandle=288

   size_t offset = strWriteHandle.find_last_of("=");
   strWriteHandle = strWriteHandle.substr(offset + 1); // 284

   offset = strReadHandle.find_last_of("=");
   strReadHandle = strReadHandle.substr(offset + 1); // 284

   hStdout = (HANDLE)atoi(strWriteHandle.c_str());
   hStdin = (HANDLE)atoi(strReadHandle.c_str());
   if (
       (hStdout == INVALID_HANDLE_VALUE) ||
       (hStdin == INVALID_HANDLE_VALUE)
       )
       ExitProcess(1);

   std::string str = "child process start.\n";
   WriteFile(hStdout, str.c_str(), str.length(), &dwWritten, NULL);

   unsigned readThreadID = 0;
   printf("Creating child read thread...\n");
   hReadThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadRead, NULL, 0, &readThreadID);

   unsigned writeThreadID = 0;
   printf("Creating child write thread...\n");
   hWriteThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadWrite, NULL, 0, &writeThreadID);

   // This simple algorithm uses the existence of the pipes to control execution.
   // It relies on the pipe buffers to ensure that no data is lost.
   // Larger applications would use more advanced process control.

   int i = 0;
   while (!g_mainThreadStop)
   { 
       Sleep(1000);
       bool needSetEvent = false;
       {
           //std::string msg = "child msg ";
           //AutoCsLock scop(writeMsgsListLock);
           //writeMsgsList.push_back(msg);
           //if (writeMsgsList.size() == 1) {
           //    needSetEvent = true;
           //}
       }
       if (needSetEvent) {
           SetEvent(events[1]); // not empty
       }
   } 
   return 0;
}

unsigned int __stdcall ThreadRead(PVOID pM) {
    bool bStop = false;
    CHAR chBuf[BUFSIZE] = { 0 };
    while (!bStop) {
        DWORD dwRead;
        BOOL bSuccess = FALSE;
        bSuccess = ReadFile(hStdin, chBuf, BUFSIZE, &dwRead, NULL);
        std::string msg;
        if (!bSuccess) {
            std::cout << "child  ReadFile failed. GetLastError():" << GetLastError() << " hStdin:" << hStdin << std::endl;
            bStop = true;
            break;
        }
        if (dwRead == 0) {
            continue;
        }
        {
            AutoCsLock scopLock(readMsgsListLock);

            msg.assign(chBuf, dwRead);
            readMsgsList.push_back(msg);
        }
        std::cout << "child read msg:" << msg.c_str() << std::endl;
    }
    std::cout << "child Thread read exit." << std::endl;
    return 0;
}
unsigned int __stdcall ThreadWrite(PVOID pM) {

    HANDLE hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return 1;
    }
    events[0] = hEvent;  //工作线程停止事件

    hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
    if (!hEvent)
    {
        DWORD last_error = ::GetLastError();
        last_error;
        return 2;
    }
    events[1] = hEvent;

    DWORD dwWait;
    bool bStop = false;
    while (!bStop) {

        std::list<std::string> tempList;
        {
            AutoCsLock scopLock(writeMsgsListLock);
            if (!writeMsgsList.empty()) {
                tempList.swap(writeMsgsList);
            }
        }
        bool writeOk = true;
        while (!tempList.empty()) {
            std::string msg = tempList.front();
            tempList.pop_front();
            DWORD dwWritten = 0;
            writeOk = WriteFile(hStdout, msg.c_str(), msg.size(), &dwWritten, NULL);
            if (!writeOk) {
                break;
            }
        }
        if (!writeOk) {
            std::cout << "Child WriteFile failed. GetLastError:" << GetLastError() << " hStdout:" << hStdout << std::endl;
            bStop = true;
            break;
        }
        dwWait = WaitForMultipleObjects(
            2,    // number of event objects 
            events,      // array of event objects 
            FALSE,        // does not wait for all 
            INFINITE);    // waits indefinitely
        DWORD waitIndex = dwWait - WAIT_OBJECT_0;
        if (waitIndex > 1 || waitIndex < 0) {
            std::cout << "error waitIndex:" << waitIndex << "  error:" << GetLastError() << std::endl;
            bStop = true;
            break;
        }
        if (waitIndex == 0) { // exit
            std::cout << " child waitIndex:" << 0 << "  exit event." << std::endl;
            bStop = true;
            break;
        }
        if (waitIndex == 1) { // list not empty
            continue;
        }
    }
    std::cout << "child Thread wirte exit." << std::endl;
    return 0;
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
        std::string msg;
        msg = tempReadList.front();
        std::cout << tempReadList.front() << std::endl;
        tempReadList.pop_front();
        //if (msg.compare("childexit") == 1) {
        //    g_mainThreadStop = true;
        //    SetEvent(events[0]);//exit
        //    if (hStdout) {
        //        CloseHandle(hStdout);
        //    }
        //    if (hStdin) {
        //        CloseHandle(hStdin);
        //    }

        //    WaitForSingleObject(hReadThread, 5000);
        //    WaitForSingleObject(hWriteThread, 5000);
        //    break;
        //}
    }
}