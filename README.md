# Windows IPC
## 命名管道通信

本例的代码位置位于[https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo](https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo)。
进程A代码在[https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processa](https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processa), 进程B代码在[https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processb](https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processb)


在进程A中使用[createnamedpipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew)创建两个命名管道，分别用于读写。然后在进程B中使用[CreateFile]()打开两个管道，用于写和读。从而实现进程双向通信。

进程A代码
```C++
#define READ_PIPE   "\\\\.\\pipe\\ReadPipe"
#define WRITE_PIPE  "\\\\.\\pipe\\WritePipe"      //   管道命名

#include <Windows.h>
#include <iostream>
using namespace std;

typedef struct _USER_CONTEXT_ 
{
    HANDLE hPipe;
    HANDLE hEvent;
}USER_CONTEXT,*PUSER_CONTEXT;


USER_CONTEXT  Context[2];

HANDLE hThread[2] = {0};

BOOL  WritePipe();
BOOL  ReadPipe();

BOOL  bOk = FALSE;

DWORD WINAPI WritePipeThread(LPVOID LPParam);
DWORD WINAPI ReadPipeThread(LPVOID LPParam);
int main()
{
    int nRetCode = 0;
    HANDLE hPipe = NULL;
    if (WritePipe()==FALSE) //创建本进程写管道
    {
        return -1;
    }
    if (ReadPipe()==FALSE)  //创建本进程读管道
    {

        return -1;
    }

    int iIndex = 0;
    while (TRUE)
    {    
        if (bOk==TRUE)
        {
            SetEvent(Context[0].hEvent); // 触发事件，以便让写线程开始接入输入
            SetEvent(Context[1].hEvent); //触发事件，以便让读线程开始从管道读

            Sleep(1);
        }
        
        iIndex = WaitForMultipleObjects(2,hThread,TRUE,5000);

        if (iIndex==WAIT_TIMEOUT)
        {
            continue;
        }

        else
        {
            break;
        }

    }

    int i = 0;
    for (i=0;i<2;i++)
    {
        CloseHandle(Context[i].hEvent);
        CloseHandle(Context[i].hPipe);
    }
    CloseHandle(hThread[0]);
    CloseHandle(hThread[1]);
    cout<<"Exit"<<endl;
    return nRetCode;
}


BOOL  WritePipe()
{
    HANDLE hWritePipe = NULL;

    hWritePipe = CreateNamedPipe( 
        WRITE_PIPE,             
        PIPE_ACCESS_DUPLEX,       
        PIPE_TYPE_MESSAGE |    
        PIPE_READMODE_MESSAGE |  
        PIPE_WAIT,               
        PIPE_UNLIMITED_INSTANCES, 
        MAX_PATH,         
        MAX_PATH,
        0,                      
        NULL);            


    if (hWritePipe==INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    HANDLE hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

    Context[0].hEvent = hEvent;
    Context[0].hPipe  = hWritePipe;
    hThread[0] = CreateThread(NULL,0,WritePipeThread,NULL,0,NULL);

    return TRUE;
}



BOOL  ReadPipe()
{
    HANDLE hReadPipe = NULL;

    hReadPipe = CreateNamedPipe( 
        READ_PIPE,             
        PIPE_ACCESS_DUPLEX,       
        PIPE_TYPE_MESSAGE |    
        PIPE_READMODE_MESSAGE |  
        PIPE_WAIT,               
        PIPE_UNLIMITED_INSTANCES, 
        MAX_PATH,         
        MAX_PATH,
        0,                      
        NULL);            

    if (hReadPipe==INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    HANDLE hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

    Context[1].hEvent = hEvent;
    Context[1].hPipe  = hReadPipe;
    hThread[1] = CreateThread(NULL,0,ReadPipeThread,NULL,0,NULL);

    return TRUE;

}

DWORD WINAPI ReadPipeThread(LPVOID LPParam)
{

    HANDLE hEvent     = Context[1].hEvent;
    HANDLE hReadPipe  = Context[1].hPipe;
    DWORD  dwReturn   = 0;

    char szBuffer[MAX_PATH] = {0};
    int  iIndex = 0;
    while (TRUE)
    {

        iIndex = WaitForSingleObject(hEvent,30);

        iIndex = iIndex-WAIT_OBJECT_0;

        if (iIndex==WAIT_FAILED||iIndex==0)
        {
            break;
        }

        ZeroMemory(szBuffer, MAX_PATH);
        if (ReadFile(hReadPipe,szBuffer,MAX_PATH,&dwReturn,NULL))
        {
            cout<<szBuffer<<endl;
        }
        else
        {
            if (GetLastError()==ERROR_INVALID_HANDLE)
            {
                break;
            }               
        }
    
    }

    return 0;
}


DWORD WINAPI WritePipeThread(LPVOID LPParam)
{
    HANDLE hEvent     = Context[0].hEvent;
    HANDLE hWritePipe = Context[0].hPipe;
    DWORD  dwReturn   = 0;

    char szBuffer[MAX_PATH] = {0};
    int  iIndex = 0;
    while (TRUE)
    {
        iIndex = WaitForSingleObject(hEvent,30);

        iIndex = iIndex-WAIT_OBJECT_0;

        if (iIndex==WAIT_FAILED||iIndex==0)
        {
            break;
        }

        cin>>szBuffer;   

       if (WriteFile(hWritePipe,szBuffer,strlen(szBuffer),&dwReturn,NULL))
       {

         
       }

       else
       {
           if (GetLastError()==ERROR_INVALID_HANDLE)
           {
               break;
           }   
       }
    }

    return 0;
}
```
进程B代码
```C++
#define WRITE_PIPE   "\\\\.\\pipe\\ReadPipe"
#define READ_PIPE  "\\\\.\\pipe\\WritePipe"
#include <Windows.h>
#include <iostream>

using namespace std;

HANDLE hThread[2] = {0};

DWORD WINAPI  ReadPipeThread(LPARAM LPParam);
DWORD WINAPI  WritePipeThread(LPARAM LPParam);
int main(int argc, TCHAR* argv[], TCHAR* envp[])
{

    HANDLE hReadPipe  = NULL;
    HANDLE hWritePipe = NULL;

    hThread[0] = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ReadPipeThread,NULL,0,NULL);
    hThread[1] = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)WritePipeThread,NULL,0,NULL);
    
    WaitForMultipleObjects(2,hThread,TRUE,INFINITE);

    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);

    CloseHandle(hThread[0]);
    CloseHandle(hThread[1]);
    cout<<"Exit"<<endl;

    
    return -1;
}


DWORD WINAPI WritePipeThread(LPARAM LPParam)
{
    HANDLE hWritePipe = NULL;
    char  szBuffer[MAX_PATH] = {0};
    DWORD dwReturn = 0;

    while(TRUE)
    {
        hWritePipe = CreateFile(WRITE_PIPE,GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,OPEN_EXISTING,0,NULL);


        if (hWritePipe==INVALID_HANDLE_VALUE)
        {
            continue;
        }

        break;
    }
    while (TRUE)
    {
        ZeroMemory(szBuffer, MAX_PATH);
        cin>>szBuffer;
        DWORD inputlen = strlen(szBuffer);
        //if (WriteFile(hWritePipe,szBuffer,MAX_PATH,&dwReturn,NULL))
        if (WriteFile(hWritePipe, szBuffer, inputlen, &dwReturn, NULL))
        {
    
        }
        else
        {
            if (GetLastError()==ERROR_NO_DATA)
            {
                cout<<"Write Failed"<<endl;
                break;
            }
        }
            

    }
    return 0;
}


DWORD WINAPI  ReadPipeThread(LPARAM LPParam)
{

    HANDLE hReadPipe = NULL;
    char  szBuffer[MAX_PATH] = {0};
    DWORD dwReturn = 0;


    while(TRUE)
    {
        hReadPipe = CreateFile(READ_PIPE,GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,OPEN_EXISTING,0,NULL);


        if (hReadPipe==INVALID_HANDLE_VALUE)
        {
            continue;
        }

        break;
    }
    while (TRUE)
    {
        if (ReadFile(hReadPipe,szBuffer,MAX_PATH,&dwReturn,NULL))
        {
            szBuffer[dwReturn] = '\0';
            cout<<szBuffer <<endl;
        }
        else
        {
            cout<<"Read Failed"<<endl;
            break;
        }
    
    }
    return 0;
}
```

### CreateNamedPipeW详解
[https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew)
```C++
HANDLE CreateNamedPipeW(
  [in]           LPCWSTR               lpName,
  [in]           DWORD                 dwOpenMode,
  [in]           DWORD                 dwPipeMode,
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
注意名字的惟一性需要自己去定义。不要用太通用的名字，以免名字冲突。

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

## 命名管道通信带字节流大小pipewithsizedemo
进行A创建读管道时，设置为字节流模式
```C++

    hReadPipe = CreateNamedPipe( 
        READ_PIPE,             
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE|
        PIPE_READMODE_BYTE|
        PIPE_WAIT,           
        PIPE_UNLIMITED_INSTANCES, 
        MAX_PATH,         
        MAX_PATH,
        0,                      
        NULL); 
```
进程B进行写管道时(WritePipeThread)，先把要发的字节流的大小 先写进去。
```C++
        ZeroMemory(szBuffer, MAX_PATH);
        cin>>(szBuffer + sizeof(DWORD));
        DWORD inputlen = strlen(szBuffer + sizeof(DWORD));
        memcpy_s(szBuffer, MAX_PATH, &inputlen, sizeof(DWORD));  // write the size of bytes first.
        if (WriteFile(hWritePipe, szBuffer, inputlen + sizeof(DWORD), &dwReturn, NULL)) // write bytes with size.
        {
    
```
然后进程A从管道读字节时，先读大小。
``` C++
        ZeroMemory(szBuffer, MAX_PATH);
        DWORD sizeLen = 0;
        if (ReadFile(hReadPipe, &sizeLen, sizeof(DWORD), &dwReturn, NULL)) { // read size first.
            if (sizeLen > 0) {
                cout << "size:" << sizeLen << endl;
                if (ReadFile(hReadPipe, szBuffer, sizeLen, &dwReturn, NULL))  // read sizeLen bytes.
                {
                    cout << szBuffer << endl;
                }
            }
        }
```
## 共享内存
本例的代码位置位于[https://github.com/iherewaitfor/windowsipc/tree/main/sharememorydemo](https://github.com/iherewaitfor/windowsipc/tree/main/sharememorydemo)。
进程A代码在[https://github.com/iherewaitfor/windowsipc/tree/main/sharememorydemo/processa](https://github.com/iherewaitfor/windowsipc/tree/main/sharememorydemo/processa), 进程B代码在[https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processb](https://github.com/iherewaitfor/windowsipc/tree/main/pipedemo/processb)

进程A使用[CreateFileMapping](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga)和[MapViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile)创建共享内存，并向共享内存写入内容。然后进程B通过[OpenFileMapping](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-openfilemappinga)和[MapViewOfFile](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile)打开共享内存，并从共享内存中读取内容。本例中暂未考虑线程同步，先运行进程A，然后运行进程B。实际读写时，需要使用线程同步技术对共享内存进行读写。

进程A代码
```C++
#include <Windows.h>
#include <iostream>
using namespace std;

int main(int argc, TCHAR* argv[], TCHAR* envp[])
{
    int nRetCode = 0;

    char szBuffer[] = "Shine";

    HANDLE hMapping = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,0,4096,"ShareMemory");

    LPVOID lpBase = MapViewOfFile(hMapping,FILE_MAP_WRITE|FILE_MAP_READ,0,0,0);

    strcpy((char*)lpBase,szBuffer);

    cout << "I have written \"" << szBuffer << "\" into share memory and sleep for 20 seconds." << endl;


    Sleep(20000);


    UnmapViewOfFile(lpBase);

    CloseHandle(hMapping);


    return nRetCode;
}
```
进程B代码

```C++
#include <Windows.h>
#include <iostream>
int main(int argc, TCHAR* argv[], TCHAR* envp[])
{
    int nRetCode = 0;


    HANDLE hMapping = OpenFileMapping(FILE_MAP_ALL_ACCESS,NULL,"ShareMemory");

    if (hMapping)
    {
        wprintf(L"open FileMapping %s\r\n",L"Success");


        LPVOID lpBase = MapViewOfFile(hMapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,0);

        char szBuffer[20] = {0};

        strcpy(szBuffer,(char*)lpBase);


        printf("what I read from share memory is: \r\n%s",szBuffer);


        UnmapViewOfFile(lpBase);

        CloseHandle(hMapping);

    }

    else
    {
        wprintf(L"%s",L"OpenMapping Error");
    }

    return nRetCode;
}
```

###[CreateFileMapping](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga)
```C++
HANDLE CreateFileMappingA(
  [in]           HANDLE                hFile,
  [in, optional] LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
  [in]           DWORD                 flProtect,
  [in]           DWORD                 dwMaximumSizeHigh,
  [in]           DWORD                 dwMaximumSizeLow,
  [in, optional] LPCSTR                lpName
);
```
参数详解
#### [in]           HANDLE                hFile,
创建共享内存时，传INVALID_HANDLE_VALUE。

A handle to the file from which to create a file mapping object.

The file must be opened with access rights that are compatible with the protection flags that the flProtect parameter specifies. It is not required, but it is recommended that files you intend to map be opened for exclusive access. For more information, see File Security and Access Rights.

If hFile is INVALID_HANDLE_VALUE, the calling process must also specify a size for the file mapping object in the dwMaximumSizeHigh and dwMaximumSizeLow parameters. In this scenario, CreateFileMapping creates a file mapping object of a specified size that is backed by the system paging file instead of by a file in the file system.
####   [in, optional] LPSECURITY_ATTRIBUTES lpFileMappingAttributes
A pointer to a SECURITY_ATTRIBUTES structure that determines whether a returned handle can be inherited by child processes. The lpSecurityDescriptor member of the SECURITY_ATTRIBUTES structure specifies a security descriptor for a new file mapping object.

If lpFileMappingAttributes is NULL, the handle cannot be inherited and the file mapping object gets a default security descriptor. The access control lists (ACL) in the default security descriptor for a file mapping object come from the primary or impersonation token of the creator. For more information, see [File Mapping Security and Access Rights](https://learn.microsoft.com/en-us/windows/desktop/Memory/file-mapping-security-and-access-rights).
####   [in]           DWORD                 flProtect
Specifies the page protection of the file mapping object. All mapped views of the object must be compatible with this protection.
This parameter can be one of the following values.
|Value|Meaning|
|:--|:--|
|PAGE_EXECUTE_READWRITE  0x40|Allows views to be mapped for read-only, copy-on-write, or execute access.  <br>The file handle specified by the hFile parameter must be created with the GENERIC_READ and GENERIC_EXECUTE access rights. <br>Windows Server 2003 and Windows XP:  This value is not available until Windows XP with SP2 and Windows Server 2003 with SP1.|
|PAGE_EXECUTE_READWRITE   0x40|Allows views to be mapped for read-only, copy-on-write, read/write, or execute access.  The file handle that the hFile parameter specifies must be created with the GENERIC_READ, GENERIC_WRITE, and GENERIC_EXECUTE access rights.   Windows Server 2003 and Windows XP:  This value is not available until Windows XP with SP2 and Windows Server 2003 with SP1.|
|PAGE_EXECUTE_WRITECOPY  0x80|Allows views to be mapped for read-only, copy-on-write, or execute access. This value is equivalent to PAGE_EXECUTE_READ.   The file handle that the hFile parameter specifies must be created with the GENERIC_READ and GENERIC_EXECUTE access rights.   Windows Vista:  This value is not available until Windows Vista with SP1.  Windows Server 2003 and Windows XP:  This value is not supported.|
|PAGE_READONLY<br>0x02|Allows views to be mapped for read-only or copy-on-write access. An attempt to write to a specific region results in an access violation. The file handle that the hFile parameter specifies must be created with the GENERIC_READ access right.|
|PAGE_READWRITE <br>0x04| Allows views to be mapped for read-only, copy-on-write, or read/write access. The file handle that the hFile parameter specifies must be created with the GENERIC_READ and GENERIC_WRITE access rights.|
|PAGE_WRITECOPY  0x08|Allows views to be mapped for read-only or copy-on-write access. This value is equivalent to PAGE_READONLY.   The file handle that the hFile parameter specifies must be created with the GENERIC_READ access right.|

#### [in]           DWORD                 dwMaximumSizeHigh
The high-order DWORD of the maximum size of the file mapping object.
####  [in]           DWORD                 dwMaximumSizeLow
The low-order DWORD of the maximum size of the file mapping object.

If this parameter and dwMaximumSizeHigh are 0 (zero), the maximum size of the file mapping object is equal to the current size of the file that hFile identifies.

An attempt to map a file with a length of 0 (zero) fails with an error code of ERROR_FILE_INVALID. Applications should test for files with a length of 0 (zero) and reject those files.

#### [in, optional] LPCSTR                lpName
The name of the file mapping object.

If this parameter matches the name of an existing mapping object, the function requests access to the object with the protection that flProtect specifies.

If this parameter is NULL, the file mapping object is created without a name.

### 参考
[https://learn.microsoft.com/en-US/windows/win32/ipc/interprocess-communications](https://learn.microsoft.com/en-US/windows/win32/ipc/interprocess-communications)

[https://www.cnblogs.com/zibility/p/5657308.html](https://www.cnblogs.com/zibility/p/5657308.html)

[https://learn.microsoft.com/en-us/windows/win32/ipc/pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/pipes)

[https://learn.microsoft.com/en-us/windows/win32/memory/file-mapping](https://learn.microsoft.com/en-us/windows/win32/memory/file-mapping)

[https://learn.microsoft.com/en-us/windows/win32/memory/creating-named-shared-memory](https://learn.microsoft.com/en-us/windows/win32/memory/creating-named-shared-memory)

[https://learn.microsoft.com/en-us/windows/desktop/Memory/file-mapping-security-and-access-rights](https://learn.microsoft.com/en-us/windows/desktop/Memory/file-mapping-security-and-access-rights)