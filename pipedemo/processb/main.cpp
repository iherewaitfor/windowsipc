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

        cin>>szBuffer;
        if (WriteFile(hWritePipe,szBuffer,MAX_PATH,&dwReturn,NULL))
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