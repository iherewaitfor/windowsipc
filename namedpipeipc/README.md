- [namedpipeipc 命名管道ipc封装Demo](#namedpipeipc-命名管道ipc封装demo)
- [HOW TO RUN](#how-to-run)
- [servernamedpipe 服务器侧](#servernamedpipe-服务器侧)
  - [主线程](#主线程)
    - [主线程事件循环](#主线程事件循环)
  - [工作线程](#工作线程)
    - [OverlappedIO](#overlappedio)
    - [线程循环等待的事件](#线程循环等待的事件)
    - [线程阻塞函数WaitForMultipleObjects](#线程阻塞函数waitformultipleobjects)
    - [线程循环](#线程循环)
  - [互斥资源](#互斥资源)
- [clientnamedpipe 客户端侧](#clientnamedpipe-客户端侧)
- [参考](#参考)


# namedpipeipc 命名管道ipc封装Demo
本项目示例展示如何使用命名管道进行ipc通道。

其中

- servernamedpipe 为服务器侧
- clientnamedpipe 为客户端侧

使用了OverLappedIO。进程的收发消息运行在工作线程，使用了关键段，事件等线程同步技术。



# HOW TO RUN
先安装Cmake.

在namedpipeipc目录下新建build文件夹，进入build文件下，运行命令行
```
cmake .. -A win32
```
打开生成的.sln文件，全部编译。选择需要调试的项目，比如servernamepipe，点击启动。
cmake的使用可以参考本github上的项目[cmakevisualstudio#使用命令行生成video-studio-的sln文件](https://github.com/iherewaitfor/cmakevisualstudio#%E4%BD%BF%E7%94%A8%E5%91%BD%E4%BB%A4%E8%A1%8C%E7%94%9F%E6%88%90video-studio-%E7%9A%84sln%E6%96%87%E4%BB%B6)

注意先运行servernamedpipe.exe，再运行clientnamedpipe.exe

运行后，输入字符，按回车，便能把消息发送过去了。其中输入"exit"表示退出。
# servernamedpipe 服务器侧
服务器侧主要功能

- 初始化命令管道，启动工作线程，等待客户端连接
- 收发客户端消息
- 执行退出命令

## 主线程
### 主线程事件循环

本项目中，主线程使用一个循环不断接收用户输入，来驱动程序运行。实际应用中，主线程会有消息循环。
主线程可以进行操作
- 给客户端发送消息
- 退出程序

```C++
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
```


模拟分发 接收消息队列中的消息。实际应用中，可以调用业务注册进来的回调函数，给各个业务分发消息。此处只是输出到控制台展示。
```C++

void dispatchMsgs(std::list<std::string> &tempReadList) {
    // to do , process the recieved msgs;
    // dispatch msg to the listenning bussiness.
    std::cout << " dispatchMsgs " << std::endl;
    while (!tempReadList.empty()) {
        std::cout << tempReadList.front() << std::endl;
        tempReadList.pop_front();
    }
}
```

## 工作线程
为了防止主线程被阻塞，把命名管道的IO操作，如收发消息等，放在了工作线程。

主线程发送消息时，只把消息放入发送队列，立即返回。工作线程从发送队列中读取。
工作线程接收到客户端的消息后，放入接收队列。主线程根据自己的消息循环，定时从接收队列读取消息即可。

### OverlappedIO
工作线程使用了重叠IO技术操作管道。
使用OverLappedIO时

要让管道使用OverLappedIO，在创建管道时，需要传入参数FILE_FLAG_OVERLAPPED。


```C++
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
```

后续对管道的IO操作，需要输入OverLappedIO指针，用于返回异步结果

- 连接客户端
```C++
    ConnectNamedPipe(hPipe, lpo);
```
- 读操作
```C++
    ReadFile(
            pReadOverLapped->handleFile,
            pReadOverLapped->readBuff,
            BUFSIZE * sizeof(TCHAR),
            &pReadOverLapped->cbRead,
            pReadOverLapped);
```
写操作
```C++
    WriteFile(
        pWriteOverLapped->handleFile,                  // pipe handle 
        pWriteOverLapped->writeBuffer,             // message 
        pWriteOverLapped->cbToWrite,              // message length 
        &pWriteOverLapped->cbToWrite,             // bytes written 
        pWriteOverLapped);                  // overlapped
```

### 线程循环等待的事件
工作线程的线程循环需要等待的事件如下：

- 管道相关OverlappedIO事件
  - 等待客户端连接ConnectNamedPipe
  - 读完成事件
  - 写完成事件
- 待发送消息队列非空事件
- 线程退出事件

```C++
    pipeOverlappeds[index].handleFile = pipeHandle;     
    pipeOverlappeds[index + 1].handleFile = pipeHandle;   
    pipeOverlappeds[index + 2].handleFile = pipeHandle;   
    events[index] = pipeOverlappeds[index].hEvent;        //ConnectNamedPipe
    events[index + 1] = pipeOverlappeds[index + 1].hEvent;//ReadFile
    events[index + 2] = pipeOverlappeds[index + 2].hEvent;//WriteFile


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
```
### 线程阻塞函数WaitForMultipleObjects
WaitForMultipleObjects，可以用来等待多个事件。当没有事件触发时，线程会阻塞在该函数。

本项目中，是使用只要有任意一个事件触发了，该函数就会返回。

### 线程循环

以下为线程循环。不断地调用WaitForMultipleObjects等待事件触发，驱动线程运行。

直到收到线程退出事件，或者出线IO异常时，退出线程循环。
```C++
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
```


## 互斥资源
- 发送消息队列
- 接收消息队列

同一进程，主线程和工作线程使用关键段来互斥访问，性能较好。

项目对关键段进行了简单的封装。
- CsLock
  - 封装CRITICAL_SECTION
- AutoCsLock
  - 封装CsLock，让其变成一个作用域自动锁。析构时自动解锁。

互斥访问发送消息队列
```C++
        //没有在发送,则触发发送。
        std::string msg;
        {
            AutoCsLock scopeLock(writeMsgsListLock);
            msg = writeMsgsList.front();
            writeMsgsList.pop_front();
        }
```

互斥访问接收消息队列
```C++
    // to do add the data to the read list;
    {
        AutoCsLock scopLock(readMsgsListLock);
        std::string msg;
        msg.assign(pipeOverlappeds[waitIndex].readBuff, pipeOverlappeds[waitIndex].InternalHigh);
        readMsgsList.push_back(msg);
    }
```
# clientnamedpipe 客户端侧
# 参考
[https://github.com/iherewaitfor/cmakevisualstudio#%E4%BD%BF%E7%94%A8%E5%91%BD%E4%BB%A4%E8%A1%8C%E7%94%9F%E6%88%90video-studio-%E7%9A%84sln%E6%96%87%E4%BB%B6](https://github.com/iherewaitfor/cmakevisualstudio#%E4%BD%BF%E7%94%A8%E5%91%BD%E4%BB%A4%E8%A1%8C%E7%94%9F%E6%88%90video-studio-%E7%9A%84sln%E6%96%87%E4%BB%B6)