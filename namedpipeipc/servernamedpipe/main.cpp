#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <iostream>

#include <process.h>
#include <list>
#include <map>
#include "namepipeipc.h"

#define BUFSIZE 4096

void dispatchMsgs(std::list<std::string>& msgsList);//to do 

TCHAR sendBuf[BUFSIZE] = { 0 };


int _tmain(VOID)
{
    NamedPipeIpc ipc("\\\\.\\pipe\\mynamedpipe", NamedPipeType::TYPE_SERVER);
    if (!ipc.init()) {
        return -1;
    }

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
            ipc.close();
            std::cout << " main thread exit." << std::endl;
            break;
        }

        std::string msg;
        msg.assign(sendBuf, cWrite);
        ipc.sendMsg(msg);
        //模拟在消息循环时分发已经收到的消息。比如主线程可定时读取消息队列
        dispatchMsgs(ipc.getMsgs());
    } while (true);

    return 0;
}

void dispatchMsgs(std::list<std::string> &tempReadList) {
    // to do , process the recieved msgs;
    // dispatch msg to the listenning bussiness.
    std::cout << " dispatchMsgs " << std::endl;
    while (!tempReadList.empty()) {
        std::cout << tempReadList.front() << std::endl;
        tempReadList.pop_front();
    }
}
