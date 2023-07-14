#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <iostream>
 
#define WAIT_TIMEOUT 5000
#define BUFSIZE 4096
 
struct  FileOverLapped : public OVERLAPPED
{
	HANDLE handleFile;
	TCHAR readBuff[BUFSIZE];
	DWORD cbRead;
	TCHAR writeBuffer[BUFSIZE];
	DWORD cbToWrite;
	FileOverLapped() {
		Internal = 0;
		InternalHigh = 0;
		Offset = 0;
		OffsetHigh = 0;

		//customer
		handleFile = NULL;
		cbRead = 0;
		cbToWrite = 0;

		ZeroMemory(readBuff, sizeof(readBuff));
		ZeroMemory(writeBuffer, sizeof(readBuff));

		// Create a non-signalled, manual-reset event,
		hEvent = ::CreateEvent(0, TRUE, FALSE, 0);
		if (!hEvent)
		{
			DWORD last_error = ::GetLastError();
			last_error;
		}
	}
	virtual ~FileOverLapped() {
		if (hEvent) {
			::CloseHandle(hEvent);
		}
	}
};

 
int _tmain(VOID) 
{ 
	HANDLE hFile  = CreateFile("overlappedtest.txt", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL); //FILE_FLAG_OVERLAPPED

	FileOverLapped writeOverLap;
	writeOverLap.handleFile = hFile;
	const char* wData = "this is write to the test.txt. overlapped  test.";
	memcpy_s(writeOverLap.writeBuffer, BUFSIZE, wData, strlen(wData));
	bool ok = WriteFile(hFile, writeOverLap.writeBuffer, strlen(writeOverLap.writeBuffer), nullptr, &writeOverLap);
	std::cout << "WriteFile ok=" << ok << " GetLastError():" << GetLastError() << std::endl;


	FileOverLapped readOverLap;
	readOverLap.handleFile = hFile;
	 ok = ReadFile(hFile, readOverLap.readBuff, 10, nullptr, &readOverLap);
	std::cout << "ReadFile ok=" << ok << " GetLastError():" << GetLastError() << std::endl;
	if (!ok) {
		if (GetLastError() != ERROR_IO_PENDING) {
			std::cout << "ReadFile faile GetLastError():" << GetLastError() << std::endl;
		}
	}

	HANDLE events[2];
	events[0] = writeOverLap.hEvent;
	events[1] = readOverLap.hEvent;

	bool writeDone = false;
	bool readDone = false;
	do {

		DWORD dw = WaitForMultipleObjects(2, events, false, WAIT_TIMEOUT);
		DWORD result = dw - WAIT_OBJECT_0;
		if (result == 0) // write event
		{
			std::cout << "overlapped write done." << std::endl;
			std::cout << "has wiritted " << writeOverLap.InternalHigh << " bytes" << std::endl;
			writeOverLap.cbToWrite = writeOverLap.InternalHigh;
			writeDone = true;
			ResetEvent(events[0]); //本事件，创建时，使用了bManualReset=true，所以此处手动设置一下信号未触发。
		}
		else if (result == 1)  // read event
		{
			std::cout << "overlapped read done." << std::endl;
			std::cout << "has read " << readOverLap.InternalHigh << " bytes" << std::endl;
			readOverLap.cbRead = readOverLap.InternalHigh;
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
 