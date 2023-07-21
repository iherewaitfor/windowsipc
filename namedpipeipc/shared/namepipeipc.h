#ifndef NAMEDPIPEIPC_H
#define NAMEDPIPEIPC_H
#include <windows.h>
#include <string>
#include <list>
#include "cslock.h"

enum NamedPipeType {
	TYPE_SERVER = 0,
	TYPE_USER =1
};
class NamedPipeIpc {
public:
	explicit NamedPipeIpc(const std::string &namePipe, NamedPipeType type);
	virtual ~NamedPipeIpc();
	bool init();
	void close();
	bool sendMsg(const std::string &msg);
	std::list<std::string> getMsgs();

private:
	std::list<std::string> readMsgsList;
	CsLock readMsgsListLock;

	// define an write array
	std::list<std::string> writeMsgsList;
	CsLock writeMsgsListLock;

	NamedPipeType  type;
};
#endif // 


