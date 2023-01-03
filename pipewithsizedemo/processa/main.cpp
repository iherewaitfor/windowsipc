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
        PIPE_TYPE_BYTE|
        PIPE_READMODE_BYTE|
        PIPE_WAIT,
/*        PIPE_TYPE_MESSAGE |    
        PIPE_READMODE_MESSAGE |  
        PIPE_WAIT, */              
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
        DWORD sizeLen = 0;
        if (ReadFile(hReadPipe, &sizeLen, sizeof(DWORD), &dwReturn, NULL)) {
            if (sizeLen > 0) {
                cout << "size:" << sizeLen << endl;
                if (ReadFile(hReadPipe, szBuffer, sizeLen, &dwReturn, NULL))
                {
                    cout << szBuffer << endl;
                }
            }
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