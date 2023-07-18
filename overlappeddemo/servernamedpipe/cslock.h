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
	explicit AutoCsLock(const CsLock &lock) {
		cslock = lock;
		cslock.lock();
	}
	~AutoCsLock() {
		cslock.unLock();
	}
private:
	CsLock cslock;
};

#endif // !1


