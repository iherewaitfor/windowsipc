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