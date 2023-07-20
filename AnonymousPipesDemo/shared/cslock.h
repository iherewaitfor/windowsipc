#ifndef CSLOCK_H
#define CSLOCK_H
#include "cslock.h"
#include <windows.h>


class CsLock {
public:
	explicit CsLock();
	~CsLock();
	void lock();
	void unLock();
private:
	//disable copy
	CsLock(const CsLock& lock);
	const CsLock & operator = (const CsLock& lock);

private:
	CRITICAL_SECTION cs;
};

class AutoCsLock {
public:
	explicit AutoCsLock(CsLock  &lock):cslock(lock){
		cslock.lock();
	}
	~AutoCsLock() {
		cslock.unLock();
	}
private:
	//disable copy
	AutoCsLock(const AutoCsLock& autolock);
	const AutoCsLock& operator = (const AutoCsLock& autolock);
private:
	CsLock &cslock;
};

#endif // !1


