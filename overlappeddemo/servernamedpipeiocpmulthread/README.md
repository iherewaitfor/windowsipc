- [servernamedpipeiocpmulthread 使用IO完成端口的命名管道示例](#servernamedpipeiocpmulthread-使用io完成端口的命名管道示例)
- [HOW TO RUN](#how-to-run)
- [主线程](#主线程)
- [IOCP](#iocp)
  - [IOCP辅助类](#iocp辅助类)
- [工作线程](#工作线程)
  - [工作线程相关CompletionKey和 PipeOverLapped的IO类型](#工作线程相关completionkey和-pipeoverlapped的io类型)
  - [线程循环](#线程循环)
- [参考](#参考)


# servernamedpipeiocpmulthread 使用IO完成端口的命名管道示例
该示例是命名管道的服务器实现，使用了IO完成端口处理IO操作和工作线程维护。

支持多个客户端同时连。可修改支持的客户端连接数。

设计思路及 技术点可以查看 思维导图[https://kdocs.cn/l/cjLM2qOhtdgb](https://kdocs.cn/l/cjLM2qOhtdgb)


主要逻辑：
连接客户端，和客户端收发消息。IO操作都都在工作线程。有多个工作线程。

# HOW TO RUN
在overlappeddemo目录下，新建build，进入build，运行
```
cmake .. -A win32
```
生成.sln文件。打开编译，把servernamedpipeiocpmulthread设置为启动项。运行即可。

详情请参考[namedpipeipc#how-to-run](https://github.com/iherewaitfor/windowsipc/tree/main/namedpipeipc#how-to-run)

客户端程序可以使用本解决方案中的clientnamedpipe项目。clientnamedpipe.exe。

本服务器支持同时多个客户端连接。

另外客户端还可以使用[namedpipeipc中的clientnamedpipe.exe](https://github.com/iherewaitfor/windowsipc/tree/main/namedpipeipc#how-to-run)


# 主线程
主线程的主要完成两个事项
- 初始化IO完成端口相关操作
  - 创建多个命名管道文件句柄 
  - 创建IO完成端口
  - 把文件句柄关联到该IO完成端口
  - 命名管道开始等待客户端连接。
  - 启动工作线程池
- 主线程循环
  - 不断接收用户输出
  - 支持向不同客户端发送消息
  - 支持退出命令


```C++
int _tmain(VOID)
{
    if (!initNamedpipe()) {
        return -1;
    }
    CIOCP iocp(0);  //创建IO端口
    for (int i = 0; i < INSTANCES; i++) {
        iocp.AssociateDevice(pipeOverlappeds[i* NOTICE_COUNT_PERHANDLE].handleFile, CPKEY_NAMEDPIPE_IO_0 + i); //关联命名管道与IOCP
    }
    for (int i = 0; i < INSTANCES; i++) {
        ConnectToNewClient(pipeOverlappeds[i * NOTICE_COUNT_PERHANDLE].handleFile, &pipeOverlappeds[i * NOTICE_COUNT_PERHANDLE + 0]);
    }

    HANDLE hThreads[THREAD_SIZE];
    unsigned threadIDs[THREAD_SIZE];

    printf("Creating worker thread...\n");
    for (int i = 0; i < THREAD_SIZE; i++) {
        // Create the worker thread.
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, &ThreadOverlapped, &iocp, 0, &threadIDs[i]);
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
            for (int i = 0; i < THREAD_SIZE; i++) {
                iocp.PostStatus(CPKEY_EXIT, 0, NULL);
            }
            WaitForMultipleObjects(INSTANCES, hThreads, true, 5000); // wait the worder threads exit.
            std::cout << " main thread exit." << std::endl;
            break;
        }

        std::string msg;
        msg.assign(sendBuf, cWrite);
        sendMsg(msg, iocp);

        //模拟在消息循环时分发已经收到的消息。比如主线程可定时读取消息队列
        dispatchMsgs();
    } while (true);

    return 0;
}
```

# IOCP
相关核心函数
- CreateIoCompletionPort
  - 创建IO完成端口
  - 把文件与对应的IO完成端口关联。
    - 关联后，该文件完成IO操作时，就会往IO完成端口的完成队列中插入元素。
  - CompletionKey：每个文件句柄唯一。
- GetQueuedCompletionStatus
  - 从IO完成端口完成队列中，取元素。该函数会阻塞
- PostQueuedCompletionStatus
  - 向IO完成端口完成队列中插入元素。会触发GetQueuedCompletionStatus返回。

## IOCP辅助类
把IO完成端口的操作轻封装为一个一个类，便于操作。特别是CreateIoCompletionPort这个函数的两个功能
- 创建 IO完成端口
- 关联文件句柄到IO完成端口
拆成了两个函数Create、AssociateDevice

```C++
class CIOCP {
public:
   CIOCP(int nMaxConcurrency = -1) { 
      m_hIOCP = NULL; 
      if (nMaxConcurrency != -1)
         (void) Create(nMaxConcurrency);
   }
   ~CIOCP() { 
       if (m_hIOCP != NULL) {
           CloseHandle(m_hIOCP);
      }
   }
   BOOL Close() {
      BOOL bResult = CloseHandle(m_hIOCP);
      m_hIOCP = NULL;
      return(bResult);
   }
   BOOL Create(int nMaxConcurrency = 0) {
      m_hIOCP = CreateIoCompletionPort(
         INVALID_HANDLE_VALUE, NULL, 0, nMaxConcurrency);
      return(m_hIOCP != NULL);
   }
   BOOL AssociateDevice(HANDLE hDevice, ULONG_PTR CompKey) {
      BOOL fOk = (CreateIoCompletionPort(hDevice, m_hIOCP, CompKey, 0) 
         == m_hIOCP);
      return(fOk);
   }
   BOOL PostStatus(ULONG_PTR CompKey, DWORD dwNumBytes = 0, 
      OVERLAPPED* po = NULL) {

      BOOL fOk = PostQueuedCompletionStatus(m_hIOCP, dwNumBytes, CompKey, po);
      return(fOk);
   }
   BOOL GetStatus(ULONG_PTR* pCompKey, PDWORD pdwNumBytes,
      OVERLAPPED** ppo, DWORD dwMilliseconds = INFINITE) {

      return(GetQueuedCompletionStatus(m_hIOCP, pdwNumBytes, 
         pCompKey, ppo, dwMilliseconds));
   }
private:
   HANDLE m_hIOCP;
};
```
# 工作线程
工作线程主要是用于执行IO操作。还有就是要处理线程循环，以及如何退出线程循环。

处理线程循环：就处理对应的CompletionKey和OverLapped的IO类型。

退出线程循环，可定义一个退出的CompletionKey，要退出时就PostQueuedCompletionStatus这个退出CompletionKey通知。



工作线程的个数，一般建议值是cpu个数的两倍。

为什么是cpu的个数的两倍，原因是工作线程，除了阻塞在iocp的GetQueuedCompletionStatus之外 ，还可能会在等待其他事件（本项目中没有这种 情况）。为了让一直在运行的线程的个数，最多为cpu的个数，所以设置线程数量比cpu个数多。



## 工作线程相关CompletionKey和 PipeOverLapped的IO类型

- CompletionKey
  - CPKEY_EXIT
  - 每个文件句柄一个CompletionKey：设置成句柄数据下标
- 每个文件句柄对应的IO事件
  - Connect = 1,  连接客户端
  - READ = 2,     读完成
  - WRITE = 3,    写完成
  - WRITELIST_NOT_EMPTY = 4   发送消息队列非空

```C++
enum IO_NOTICE_TYPE{
    UNKNOW = 0,
    Connect = 1,
    READ = 2,
    WRITE = 3,
    WRITELIST_NOT_EMPTY = 4
};
```
PipeOverLapped继承OVERLAPPED,用户存储额外的信息。注意由于现在已是借助iocp进行通知，不需要使用Event，所以设置hEvent = NULL.
注意定义了成员IO_NOTICE_TYPE noticeType，用于区分不同的IO操作类型，以便知道是哪个IO操作完成了。

```C++
struct  PipeOverLapped : public OVERLAPPED
{
    HANDLE handleFile;
    TCHAR readBuff[BUFSIZE];
    DWORD cbRead;
    TCHAR writeBuffer[BUFSIZE];
    DWORD cbToWrite;
    IO_NOTICE_TYPE noticeType;
    bool bWritting;

    PipeOverLapped() {
        Internal = 0;
        InternalHigh = 0;
        Offset = 0;
        OffsetHigh = 0;
        hEvent = NULL; //通过IOCP的完成通知进行通知，不需要使用事件。

        //customer
        handleFile = NULL;
        cbRead = 0;
        cbToWrite = 0;
        noticeType = IO_NOTICE_TYPE::UNKNOW;
        bWritting = false;

        ZeroMemory(readBuff, sizeof(readBuff));
        ZeroMemory(writeBuffer, sizeof(readBuff));
    }
    ~PipeOverLapped() {
    }
};
```

## 线程循环
线程循环阻塞于GetQueuedCompletionStatus。

可使用PostQueuedCompletionStatus向iocp完成队列插入元素。因此当需要退出消息循环时，可以插入CompletionKey为CPKEY_EXIT的元素，以便线程循环退出。
```C++
unsigned int __stdcall ThreadOverlapped(PVOID pM)
{
    printf("beginThread 线程ID号为%4d \n", GetCurrentThreadId());
    CIOCP* iocp = (CIOCP*)pM;
    BOOL bResult = FALSE;
    while (true) {
        // Suspend the thread until an I/O completes
        ULONG_PTR CompletionKey;
        DWORD dwNumBytes;
        PipeOverLapped* pior;
        bResult = iocp->GetStatus(&CompletionKey, &dwNumBytes, (OVERLAPPED**)&pior, INFINITE);
        if (CompletionKey == CPKEY_EXIT) {
            // to do 线程退出
            break;
        }
        if (CompletionKey >= CPKEY_NAMEDPIPE_IO_0 && CompletionKey < NOTICE_COUNT_PERHANDLE * INSTANCES) {
            int indexHandle = CompletionKey - CPKEY_NAMEDPIPE_IO_0;
            if (IO_NOTICE_TYPE::Connect == pior->noticeType) {
                handleConnectEvent(indexHandle * NOTICE_COUNT_PERHANDLE + 0);
            }
            else if (IO_NOTICE_TYPE::READ == pior->noticeType) {
                if (!handleReadEvent(indexHandle * NOTICE_COUNT_PERHANDLE + 1)) {
                    continue;
                }
            }
            else if (IO_NOTICE_TYPE::WRITE == pior->noticeType) {
                //write done
                if (!handleWriteEvent(indexHandle * NOTICE_COUNT_PERHANDLE + 2, pipeOverlappeds[indexHandle*NOTICE_COUNT_PERHANDLE+2].bWritting)) {
                    break;
                }
            }
            else if (IO_NOTICE_TYPE::WRITELIST_NOT_EMPTY == pior->noticeType) {
                if (!handleNotEmptyEvent(indexHandle * NOTICE_COUNT_PERHANDLE + 3, pipeOverlappeds[indexHandle * NOTICE_COUNT_PERHANDLE + 2].bWritting)) {
                    break;
                }
                continue;
            }
        }
    } // end of while
    std::cout << "worker thread exit" << std::endl;

    return 0;
}
```

# 参考

《windows核心编程》

