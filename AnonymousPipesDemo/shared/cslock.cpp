#include "cslock.h"
CsLock::CsLock() {
	InitializeCriticalSectionAndSpinCount(&cs, 0x00000400);
	//InitializeCriticalSection(&cs);
}
CsLock::~CsLock() {
	DeleteCriticalSection(&cs);
}
void CsLock::lock() {
	EnterCriticalSection(&cs);
}
void CsLock::unLock() {
	LeaveCriticalSection(&cs);
}