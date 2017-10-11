#pragma once


#include <Thread.h>
#include <FSPublic.h>
#include <string>
#include <SpinLock.h>
using namespace std;
EX_NAMESPACE_USING;
 
typedef long time_base_seconds;
static const int MILLION = 1000000;

#ifndef INT_MAX
#define INT_MAX	0x7FFFFFFF
#endif

#ifndef NULL
#define NULL 0
#endif

 
class Timeval {
public:
	time_base_seconds seconds() const {
		return fTv.tv_sec;
	}
	time_base_seconds seconds() {
		return fTv.tv_sec;
	}
	time_base_seconds useconds() const {
		return fTv.tv_usec;
	}
	time_base_seconds useconds() {
		return fTv.tv_usec;
	}

	int operator>=(Timeval const& arg2) const;
	int operator<=(Timeval const& arg2) const {
		return arg2 >= *this;
	}
	int operator<(Timeval const& arg2) const {
		return !(*this >= arg2);
	}
	int operator>(Timeval const& arg2) const {
		return arg2 < *this;
	}
	int operator==(Timeval const& arg2) const {
		return *this >= arg2 && arg2 >= *this;
	}
	int operator!=(Timeval const& arg2) const {
		return !(*this == arg2);
	}

	void operator+=(class DelayInterval const& arg2);
	void operator-=(class DelayInterval const& arg2);
	// returns ZERO iff arg2 >= arg1

protected:
	Timeval(time_base_seconds seconds, time_base_seconds useconds) {
		fTv.tv_sec = seconds; fTv.tv_usec = useconds;
	}

private:
	time_base_seconds& secs() {
		return (time_base_seconds&)fTv.tv_sec;
	}
	time_base_seconds& usecs() {
		return (time_base_seconds&)fTv.tv_usec;
	}

	struct timeval fTv;
};

class DelayInterval : public Timeval {
public:
	DelayInterval(time_base_seconds seconds, time_base_seconds useconds)
		: Timeval(seconds, useconds) {}
 
};
const DelayInterval DELAY_ZERO(0, 0);
class DelayInterval operator-(Timeval const& arg1, Timeval const& arg2);
class _EventTime : public Timeval {
public:
	_EventTime(unsigned secondsSinceEpoch = 0,
		unsigned usecondsSinceEpoch = 0)
		// We use the Unix standard epoch: January 1, 1970
		: Timeval(secondsSinceEpoch, usecondsSinceEpoch) {}
};
_EventTime TimeNow();


class DelayQueueEntry {
public:
	DelayQueueEntry(DelayInterval delay);
	~DelayQueueEntry() {};
	virtual void handleTimeout() {
		delete this;
	};
private:
	friend class DelayQueue;
	DelayQueueEntry* fNext;
	DelayQueueEntry* fPrev;
	DelayInterval fDeltaTimeRemaining; 
};


class DelayQueue : public DelayQueueEntry 
{
public:
	DelayQueue();
	~DelayQueue();
	void addEntry(DelayQueueEntry* newEntry); // returns a token for the entry
	void removeEntry(DelayQueueEntry* entry); // but doesn't delete it
	void handleAlarm();
	void synchronize();
	DelayInterval const& timeToNextAlarm();
 
private:
	DelayQueueEntry* head() { return fNext; };
	_EventTime fLastSyncTime;
	SpinLock m_lock ;
};

typedef void TaskFunc(void* clientData);

class AlarmHandler : public DelayQueueEntry {
public:
	AlarmHandler(TaskFunc* proc, void* clientData, DelayInterval timeToDelay)
		: DelayQueueEntry(timeToDelay), fProc(proc), fClientData(clientData) {
	}

private: // redefined virtual functions
	virtual void handleTimeout() {
         //_EventTime sendTimeNow = TimeNow();

		(*fProc)(fClientData);
		DelayQueueEntry::handleTimeout();
/*
		DelayInterval timeDiff2 = TimeNow() - sendTimeNow;
	    //if (timeDiff2 > DelayInterval(0, 1000)) //1ms
	    {
			 	struct timeval tvNow;
	gettimeofday(&tvNow, NULL);
	 
	time_t tvNow_t = tvNow.tv_sec;
	char const* ctimeResult = ctime(&tvNow_t);
	if (ctimeResult == NULL) {
		cout <<"??:??:??" << endl;
	}
	else {
		char const* from = &ctimeResult[11];
		int i;
		for (i = 0; i < 8; ++i) {
			cout << from[i];
		}
		cout << "." << tvNow.tv_usec <<" " ;
	}
			 cout << " handleTimeout  use " << timeDiff2.seconds() << "s " 
			 	<< timeDiff2.useconds() / 1000 << " ms(" 
			 	<< timeDiff2.useconds() << "us)" << endl;
	 

	    }*/



	}

private:
	TaskFunc* fProc;
	void* fClientData;
};
void coutTimeString();

struct tclientData {
	int timerId;
	long durationMS;
};

static void testDelayTaskFunc(void * clientData) {
	tclientData data = *(tclientData *)clientData  ;
	coutTimeString();
	cout << "Timer ID " << data.timerId << " out. duration:"<< data.durationMS<<" ms " << endl;
	delete (tclientData *)clientData;
}


class TestTaskScheduler : public EXT_NAMESPACE::Thread
{
public:
	 TestTaskScheduler():AffinityCPU(-1) {};
	 void setAffinity(int vcpu) {AffinityCPU = vcpu;};

	void SingleStep(unsigned maxDelayTime = 0);

	THREAD_FUN main() {

		THREAD_AFFINITY affinity;
		affinity.cpu = 0;
		affinity.core = 0;
		affinity.vcpu = AffinityCPU;
		affinity.mask = 0;
		if (AffinityCPU)
		  ThreadAffinity(&affinity);

		doEventLoop();
		return 0;
	}
	void doEventLoop(char volatile* watchVariable = NULL) {
		// Repeatedly loop, handling readble sockets and timed events:
		int uSecsMaxWait = 1000;//1000us = 1ms accurate !
		while (1) {
			if (watchVariable != NULL && *watchVariable != 0) break;
			SingleStep(uSecsMaxWait);
		}
	}

	void* scheduleDelayedTask(int64_t microseconds, TaskFunc* proc,	void* clientData);

 

	void test(  ) {
		    int mSecs;
			cout << " input time to wait (ms) :";
			cin >> mSecs;
			coutTimeString();
			timerId++;
			cout << "Timer ID "<< timerId <<" start " << endl;
			long uSecsToDelay = mSecs * 1000;//1w us = 10 ms
			 
			tclientData *pData = new tclientData;
			pData->durationMS =  mSecs;
			pData->timerId = timerId;
			scheduleDelayedTask(uSecsToDelay, testDelayTaskFunc, pData);
		 
 
	};
private :
	DelayQueue fDelayQueue;
	long  timerId  ;
	int AffinityCPU;
 
	 
};

