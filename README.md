# Windows IPC
## 命名管道通信

本例的代码位置位于[https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo](https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo)。
进程A代码在[https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processa](https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processa), 进程B代码在[https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processb](https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processb)


在进程A中使用[createnamedpipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew)创建两个命名管道，分别用于读写。然后在进程B中使用[CreateFile]()打开两个管道，用于写和读。从而实现进程双向通信。

### CreateNamedPipeW详解
[https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew)
```C++
HANDLE CreateNamedPipeW(
  [in]           LPCWSTR               lpName,
  [in]           DWORD                 dwOpenMode,



  [in]           DWORD                 nMaxInstances,
  [in]           DWORD                 nOutBufferSize,
  [in]           DWORD                 nInBufferSize,
  [in]           DWORD                 nDefaultTimeOut,
  [in, optional] LPSECURITY_ATTRIBUTES lpSecurityAttributes
);
```
各参数解析

#### [in]           LPCWSTR               lpName,
命令管道的惟一名字。格式要求类似如下格式。
```
\\.\pipe\pipename
```
注意名字的惟一性需要自己去定义。不要用太通道的名字，以名名字冲突。

####   [in]           DWORD                 dwOpenMode,
打开模式
|模式|含义|
|:-----|:------|
|PIPE_ACCESS_DUPLEX   0x00000003|读写。The pipe is bi-directional; both server and client processes can read from and write to the pipe. This mode gives the server the equivalent of GENERIC_READ and GENERIC_WRITE access to the pipe. The client can specify GENERIC_READ or GENERIC_WRITE, or both, when it connects to the pipe using the [CreateFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew) function.|
|PIPE_ACCESS_INBOUND 0x00000001|服务器只读。The flow of data in the pipe goes from client to server only. This mode gives the server the equivalent of GENERIC_READ access to the pipe. The client must specify GENERIC_WRITE access when connecting to the pipe. If the client must read pipe settings by calling the GetNamedPipeInfo or GetNamedPipeHandleState functions, the client must specify GENERIC_WRITE and FILE_READ_ATTRIBUTES access when connecting to the pipe.|
|PIPE_ACCESS_OUTBOUND 0x00000002|服务器只写。The flow of data in the pipe goes from server to client only. This mode gives the server the equivalent of GENERIC_WRITE access to the pipe. The client must specify GENERIC_READ access when connecting to the pipe. If the client must change pipe settings by calling the SetNamedPipeHandleState function, the client must specify GENERIC_READ and FILE_WRITE_ATTRIBUTES access when connecting to the pipe.|
####   [in]           DWORD                 dwPipeMode,
管道模式。

可以指定以下类型模式之一。 必须为管道的每个实例指定相同的类型模式。
|模式|含义|
|:-----|:------|
|PIPE_TYPE_BYTE 0x00000000|数据以字节流的形式写入管道。 此模式不能与PIPE_READMODE_MESSAGE一起使用。 管道不区分在不同写入操作期间写入的字节。|
|PIPE_TYPE_MESSAGE 0x00000004|数据以消息流的形式写入管道。 管道将每次写入操作期间写入的字节视为消息单元。 GetLastError 函数在未完全读取消息时返回ERROR_MORE_DATA。 此模式可用于 PIPE_READMODE_MESSAGE 或 PIPE_READMODE_BYTE。|

可以指定以下读取模式之一。 同一管道的不同实例可以指定不同的读取模式

|模式|含义|
|:-----|:------|
|PIPE_READMODE_BYTE 0x00000000|数据作为字节流从管道中读取。 此模式可用于 PIPE_TYPE_MESSAGE 或 PIPE_TYPE_BYTE。|
|PIPE_READMODE_MESSAGE 0x00000002| 数据作为消息流从管道中读取。 仅当同时指定 PIPE_TYPE_MESSAGE 时，才能使用此模式。|

可以指定以下等待模式之一。 同一管道的不同实例可以指定不同的等待模式。
|模式|含义|
|:-----|:------|
|PIPE_WAIT  0x00000000|已启用阻塞模式。 在 ReadFile、 WriteFile 或 ConnectNamedPipe 函数中指定管道句柄时，在有要读取的数据、写入所有数据或客户端连接之前，不会完成操作。 使用此模式可能意味着在某些情况下无限期等待客户端进程执行操作。 |
|PIPE_NOWAIT  0x00000001|Nonblocking mode is enabled. In this mode, ReadFile, WriteFile, and ConnectNamedPipe always return immediately.  Note that nonblocking mode is supported for compatibility with Microsoft LAN Manager version 2.0 and should not be used to achieve asynchronous I/O with named pipes. For more information on asynchronous pipe I/O, see Synchronous and Overlapped Input and Output.|

可以指定以下远程客户端模式之一。 同一管道的不同实例可以指定不同的远程客户端模式。
|模式|含义|
|:-----|:------|
|PIPE_ACCEPT_REMOTE_CLIENTS 0x00000000| 可以接受来自远程客户端的连接，并针对管道的安全描述符进行检查。|
|PIPE_REJECT_REMOTE_CLIENTS 0x00000008|来自远程客户端的连接会自动被拒绝。|

#### [in] nMaxInstances
为此管道创建的最大实例数。 管道的第一个实例可以指定此值;必须为管道的其他实例指定相同的数字。 可接受的值为 1 到 PIPE_UNLIMITED_INSTANCES ( 255) 。

#### [in] nOutBufferSize
The number of bytes to reserve for the output buffer.
#### [in] nInBufferSize
The number of bytes to reserve for the input buffer.
#### [in] nDefaultTimeOut
The default time-out value, in milliseconds, if the WaitNamedPipe function specifies NMPWAIT_USE_DEFAULT_WAIT. Each instance of a named pipe must specify the same value.

A value of zero will result in a default time-out of 50 milliseconds.

#### [in, optional] lpSecurityAttributes
A pointer to a SECURITY_ATTRIBUTES structure that specifies a security descriptor for the new named pipe and determines whether child processes can inherit the returned handle. If lpSecurityAttributes is NULL, the named pipe gets a default security descriptor and the handle cannot be inherited. The ACLs in the default security descriptor for a named pipe grant full control to the LocalSystem account, administrators, and the creator owner. They also grant read access to members of the Everyone group and the anonymous account.

### 参考

[https://www.cnblogs.com/zibility/p/5657308.html](https://www.cnblogs.com/zibility/p/5657308.html)

[https://learn.microsoft.com/en-us/windows/win32/ipc/pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/pipes)