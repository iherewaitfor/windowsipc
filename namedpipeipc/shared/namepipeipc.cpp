#include "namepipeipc.h"
NamedPipeIpc::NamedPipeIpc(const std::string& namePipe, NamedPipeType type) {

}
NamedPipeIpc::~NamedPipeIpc() {

}
bool NamedPipeIpc::init() {
	return false;
}
void NamedPipeIpc::close() {
}
bool NamedPipeIpc::sendMsg(const std::string& msg) {
	return false;
}
std::list<std::string> NamedPipeIpc::getMsgs() {
	std::list<std::string>tempMsgs;
	{
		AutoCsLock scopeLock(readMsgsListLock);
		tempMsgs.swap(readMsgsList);
	}
	return tempMsgs;
}