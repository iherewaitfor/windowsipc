#include <windows.h> 
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
#include <process.h>
#include <string>
#include <list>
#include <map>
#include "cslock.h"
#include <iostream>

#define BUFSIZE 4096 
 
HANDLE g_hChildStd_IN_Rd = NULL;    //子进程读，父进程需要关闭。需要传给子进程。
HANDLE g_hChildStd_IN_Wr = NULL;    //父进程写。
HANDLE g_hChildStd_OUT_Rd = NULL;   //父进程读。
HANDLE g_hChildStd_OUT_Wr = NULL;   //子进程写，父进程需要关闭。需要传给子进程。
 
void CreateChildProcess(void); 
void ErrorExit(PTSTR); 


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

volatile bool g_mainThreadStop = false;
PROCESS_INFORMATION piProcInfo;

int _tmain(int argc, TCHAR* argv[])
{
    SECURITY_ATTRIBUTES saAttr;

    printf("\n->Start of parent execution.\n");

    // Set the bInheritHandle flag so pipe handles are inherited. 

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT. 

    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
        ErrorExit(TEXT("StdoutRd CreatePipe"));

    // Ensure the read handle to the pipe for STDOUT is not inherited.

    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdout SetHandleInformation"));

    // Create a pipe for the child process's STDIN. 

    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
        ErrorExit(TEXT("Stdin CreatePipe"));

    // Ensure the write handle to the pipe for STDIN is not inherited. 

    if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdin SetHandleInformation"));

    // Create the child process. 

    CreateChildProcess();

    HANDLE hReadThread;
    unsigned readThreadID = 0;
    printf("Creating read thread...\n");
    hReadThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadRead, NULL, 0, &readThreadID);
    
    HANDLE hWriteThread;
    unsigned writeThreadID = 0;
    printf("Creating write thread...\n");
    hWriteThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadWrite, NULL, 0, &writeThreadID);

    TCHAR sendBuf[BUFSIZE] = { 0 };
    DWORD  cWrite;
    while (!g_mainThreadStop) {
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
        cWrite = (lstrlen(sendBuf) + 1) * sizeof(TCHAR);

        int cmpResult = strcmp(sendBuf, "exit");
        if (cmpResult == 0) {
            //make child read failed. make child read thread exit
            if (g_hChildStd_IN_Wr) {
                CloseHandle(g_hChildStd_IN_Wr);
                g_hChildStd_IN_Wr = NULL;
            }
            SetEvent(events[0]);//exit
            if (g_hChildStd_OUT_Rd) {
                CloseHandle(g_hChildStd_OUT_Rd);
                g_hChildStd_OUT_Rd = NULL;
            }
            break;
        }
        if (strcmp(sendBuf, "childexit") == 0) {
            //make child read failed. make child read thread exit
            if (g_hChildStd_IN_Wr) {
                FlushFileBuffers(g_hChildStd_IN_Wr);
                CloseHandle(g_hChildStd_IN_Wr);
                g_hChildStd_IN_Wr = NULL;
            }
        }
        std::string msg = "";
        msg.append(sendBuf, cWrite);
        bool needSetEvent = false;
        {
            AutoCsLock scopeLock(writeMsgsListLock);
            writeMsgsList.push_back(msg);
            if (writeMsgsList.size() == 1) {
                needSetEvent = true;
            }
        }
        if (needSetEvent) {
            SetEvent(events[1]); // not empty
        }
        dispatchMsgs();
    }

    WaitForSingleObject(hReadThread, 5000);
    WaitForSingleObject(hWriteThread, 5000);
    WaitForSingleObject(piProcInfo.hProcess, 50000);

   printf("\n->End of parent execution.\n");

// The remaining open handles are cleaned up when this process terminates. 
// To avoid resource leaks in a larger application, close handles explicitly. 

   return 0; 
} 
 
void CreateChildProcess()
// Create a child process that uses the previously created pipes for STDIN and STDOUT.
{ 
    std::string strCmdLine;
    strCmdLine.append("child ");
    strCmdLine.append(" writeHandle=");
    strCmdLine.append(std::to_string((int)g_hChildStd_OUT_Wr));

    strCmdLine.append(" readHandle=");
    strCmdLine.append(std::to_string((int)g_hChildStd_IN_Rd));
   //TCHAR szCmdline[]=TEXT("child");
   TCHAR *szCmdline = (TCHAR*)strCmdLine.c_str();
   //PROCESS_INFORMATION piProcInfo; 
   STARTUPINFO siStartInfo;
   BOOL bSuccess = FALSE; 
 
// Set up members of the PROCESS_INFORMATION structure. 
 
   ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
 
// Set up members of the STARTUPINFO structure. 
// This structure specifies the STDIN and STDOUT handles for redirection.
 
   ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
   siStartInfo.cb = sizeof(STARTUPINFO); 
   //siStartInfo.hStdError = g_hChildStd_OUT_Wr;
   //siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
   //siStartInfo.hStdInput = g_hChildStd_IN_Rd;
   //siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
 
// Create the child process. 
    
   bSuccess = CreateProcess(NULL, 
      szCmdline,     // command line 
      NULL,          // process security attributes 
      NULL,          // primary thread security attributes 
      TRUE,          // handles are inherited 
      0,             // creation flags 
      NULL,          // use parent's environment 
      NULL,          // use parent's current directory 
      &siStartInfo,  // STARTUPINFO pointer 
      &piProcInfo);  // receives PROCESS_INFORMATION 
   
   // If an error occurs, exit the application. 
   if ( ! bSuccess ) 
      ErrorExit(TEXT("CreateProcess"));
   else 
   {
      // Close handles to the child process and its primary thread.
      // Some applications might keep these handles to monitor the status
      // of the child process, for example. 

      CloseHandle(piProcInfo.hProcess);
      CloseHandle(piProcInfo.hThread);
      
      // Close handles to the stdin and stdout pipes no longer needed by the child process.
      // If they are not explicitly closed, there is no way to recognize that the child process has ended.
      
      CloseHandle(g_hChildStd_OUT_Wr);
      CloseHandle(g_hChildStd_IN_Rd);
   }
}
 
 
void ErrorExit(PTSTR lpszFunction) 

// Format a readable error message, display a message box, 
// and exit from the application.
{ 
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)lpszFunction)+40)*sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(1);
}

unsigned int __stdcall ThreadRead(PVOID pM) {
    bool bStop = false;
    CHAR chBuf[BUFSIZE] = { 0 };
    while (!bStop) {
        DWORD dwRead;
        BOOL bSuccess = FALSE;
        HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        std::string msg;
        if (!bSuccess) {
            std::cout << " ReadFile failed. GetLastError():" << GetLastError() << std::endl;
            g_mainThreadStop = true;
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
        std::cout << " read msg:" << msg.c_str() << std::endl;
    }
    std::cout << " Thread read exit." << std::endl;
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
            while (!writeMsgsList.empty()) {

                tempList.push_back(writeMsgsList.front());
                writeMsgsList.pop_front();
            }
        }
        bool writeOk = true;
        while (!tempList.empty()) {
            std::string msg = tempList.front();
            tempList.pop_front();
            DWORD dwWritten = 0;
            writeOk = WriteFile(g_hChildStd_IN_Wr, msg.c_str(), msg.size(), &dwWritten, NULL);
            if (!writeOk) {
                break;
            }
        }
        if (!writeOk) {
            //write failed, set the handle to invalid.
            //g_hChildStd_IN_Wr = INVALID_HANDLE_VALUE;
            std::cout << "WriteFile failed. GetLastError:" << GetLastError() << " g_hChildStd_IN_Wr:" << g_hChildStd_IN_Wr << std::endl;
            g_mainThreadStop = true;
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
            std::cout << "error waitIndex:" << waitIndex << "  error:" << GetLastError();
            bStop = true;
            break;
        }
        if (waitIndex == 0) { // exit
            std::cout << " waitIndex:" << 0 << "  exit event." << std::endl;
            bStop = true;
            ResetEvent(events[0]);
            break;
        }
        if (waitIndex == 1) { // list not empty
            ResetEvent(events[1]);
            continue;
        }
    }
    std::cout << " Thread write exit." << std::endl;
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
     //   std::cout << tempReadList.front() << std::endl;
        tempReadList.pop_front();
    }
}