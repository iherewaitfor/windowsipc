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
	CRITICAL_SECTION cs;
};

class AutoCsLock {
public:
	explicit AutoCsLock(CsLock *lock) {
		this->lock = lock;
		lock->lock();
	}
	~AutoCsLock() {
		lock->unLock();
	}
private:
	CsLock* lock;
};

#endif // !1


