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