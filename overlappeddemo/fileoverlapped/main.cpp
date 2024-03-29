#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <iostream>
 
#define CONNECTING_STATE 0 
#define READING_STATE 1 
#define WRITING_STATE 2 
#define INSTANCES 4 
#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096
 
typedef struct 
{ 
   OVERLAPPED oOverlap; 
   HANDLE handleFile; 
   TCHAR readBuff[BUFSIZE]; 
   DWORD cbRead;
   TCHAR writeBuffer[BUFSIZE];
   DWORD cbToWrite; 
   DWORD dwState; 
   BOOL fPendingIO; 
} FileOverLapped, *LPFileOverLapped;

 
int _tmain(VOID) 
{ 
	HANDLE hFile  = CreateFile("test.txt", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL); //FILE_FLAG_OVERLAPPED



	FileOverLapped writeOverLap;
	writeOverLap = { 0 };
	writeOverLap.oOverlap = { 0 };
	writeOverLap.oOverlap.hEvent = CreateEvent(NULL, true, false, "WriteEvent");
	writeOverLap.oOverlap.Offset = 0; //注意实际应用写的文件位置要与读的文件位置不重叠。
	const char* wData = "this is write to the test.txt. overlapped  test.";
	memcpy_s(writeOverLap.writeBuffer, BUFSIZE, wData, strlen(wData));
	bool ok = WriteFile(hFile, writeOverLap.writeBuffer, strlen(writeOverLap.writeBuffer), nullptr, &writeOverLap.oOverlap);
	std::cout << "WriteFile ok=" << ok << " GetLastError():" << GetLastError() << std::endl;


	FileOverLapped readOverLap;
	readOverLap = { 0 };
	readOverLap.oOverlap = { 0 };
	readOverLap.oOverlap.hEvent = CreateEvent(NULL, true, false, "ReadEvent");
	readOverLap.oOverlap.Offset = 0;
	 ok = ReadFile(hFile, readOverLap.readBuff, 10, nullptr, &readOverLap.oOverlap);
	std::cout << "ReadFile ok=" << ok << " GetLastError():" << GetLastError() << std::endl;
	if (!ok) {
		if (GetLastError() != ERROR_IO_PENDING) {
			std::cout << "ReadFile faile GetLastError():" << GetLastError() << std::endl;
		}

	}


	HANDLE events[2];
	events[0] = writeOverLap.oOverlap.hEvent;
	events[1] = readOverLap.oOverlap.hEvent;

	bool writeDone = false;
	bool readDone = false;
	do {

		DWORD dw = WaitForMultipleObjects(2, events, false, 5000);
		DWORD result = dw - WAIT_OBJECT_0;
		if (result == 0) // write event
		{
			std::cout << "overlapped write done." << std::endl;
			std::cout << "has wiritted " << writeOverLap.oOverlap.InternalHigh << " bytes" << std::endl;
			writeOverLap.cbToWrite = writeOverLap.oOverlap.InternalHigh;
			writeDone = true;
			ResetEvent(events[0]); //本事件，创建时，使用了bManualReset=true，所以此处手动设置一下信号未触发。
		}
		else if (result == 1)  // read event
		{
			std::cout << "overlapped read done." << std::endl;
			std::cout << "has read " << readOverLap.oOverlap.InternalHigh << " bytes" << std::endl;
			readOverLap.cbRead = readOverLap.oOverlap.InternalHigh;
			readDone = true;
			ResetEvent(events[1]);//本事件，创建时，使用了bManualReset=true，所以此处手动设置一下信号未触发。
		}
		else {
			std::cout << " error dw = " << dw << std::endl;
			break;
		}

	} while (!writeDone || !readDone); //验证读写都完成就退出循环

	getchar();
 
  return 0; 
} 
 