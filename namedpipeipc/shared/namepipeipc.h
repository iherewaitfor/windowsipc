#ifndef NAMEDPIPEIPC_H
#define NAMEDPIPEIPC_H
#include <windows.h>
#include <string>
#include <list>
#include "cslock.h"

#define BUFSIZE 4096

struct  PipeOverLapped : public OVERLAPPED
{
    HANDLE handleFile;
    TCHAR readBuff[BUFSIZE];
    DWORD cbRead;
    TCHAR writeBuffer[BUFSIZE];
    DWORD cbToWrite;
    PipeOverLapped() {
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
    virtual ~PipeOverLapped() {
        if (hEvent) {
            ::CloseHandle(hEvent);
        }
    }
};

enum NamedPipeType {
	TYPE_SERVER = 0,
	TYPE_CLIENT =1
};
class NamedPipeIpc {
public:
	explicit NamedPipeIpc(const std::string &namePipe, NamedPipeType type);
	virtual ~NamedPipeIpc();
	bool init();
	void close();
	bool sendMsg(const std::string &msg);
	std::list<std::string> getMsgs();

    BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo);
    void handleConnectEvent(int waitIndex);
    bool handleNotEmptyEvent(int waitIndex, bool& bWritting);
    bool handleReadEvent(int waitIndex);
    bool handleWriteEvent(int waitIndex, bool& bWritting);
public:
    //ConnectNamedPipe
    //ReadFile
    //WriteFile
    PipeOverLapped pipeOverlappeds[3];

    //最后的事件：
    //工作线程结束事件、待发送消息队列非空事件
    HANDLE events[3 + 2];
    HANDLE m_hClientPipe;
private:
    bool initNamedpipeServer();
    bool initNamedpipeClient();;

    bool handleReadEventClient(int waitIndex);
    bool handleReadEventServer(int waitIndex);

    bool writeMsg(bool& bWritting);

private:
	std::list<std::string> readMsgsList;
	CsLock readMsgsListLock;

	// define an write array
	std::list<std::string> writeMsgsList;
	CsLock writeMsgsListLock;

	NamedPipeType  m_type;

    std::string m_pipeName;
    HANDLE m_hThread;
    unsigned m_threadID;

    bool m_serverConected;

};
#endif // 


