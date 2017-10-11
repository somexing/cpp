#include "DelayQueue.h"
#include "System.h"


void coutTimeString() {


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


	 

}

_EventTime TimeNow() {
	struct timeval tvNow;

	 
	gettimeofday(&tvNow, NULL);

	return  _EventTime(tvNow.tv_sec, tvNow.tv_usec);
}

int Timeval::operator>=(const Timeval& arg2) const {
	return seconds() > arg2.seconds()
		|| (seconds() == arg2.seconds()
			&& useconds() >= arg2.useconds());
}

void Timeval::operator+=(const DelayInterval& arg2) {
	secs() += arg2.seconds(); usecs() += arg2.useconds();
	if (useconds() >= MILLION) {
		usecs() -= MILLION;
		++secs();
	}
}

void Timeval::operator-=(const DelayInterval& arg2) {
	secs() -= arg2.seconds(); usecs() -= arg2.useconds();
	if ((int)useconds() < 0) {
		usecs() += MILLION;
		--secs();
	}
	if ((int)seconds() < 0)
		secs() = usecs() = 0;

}

DelayInterval operator-(const Timeval& arg1, const Timeval& arg2) {
	time_base_seconds secs = arg1.seconds() - arg2.seconds();
	time_base_seconds usecs = arg1.useconds() - arg2.useconds();

	if ((int)usecs < 0) {
		usecs += MILLION;
		--secs;
	}
	if ((int)secs < 0)
		return DELAY_ZERO;
	else
		return DelayInterval(secs, usecs);
}



DelayQueueEntry::DelayQueueEntry(DelayInterval delay)
	: fDeltaTimeRemaining(delay) {
	fNext = fPrev = this;
	//fToken = ++tokenCounter;
}

const DelayInterval ETERNITY(INT_MAX, MILLION - 1);

DelayQueue::DelayQueue() : DelayQueueEntry(ETERNITY) {
	fLastSyncTime = TimeNow();
}


DelayQueue::~DelayQueue()
{
	while (fNext != this) {
		DelayQueueEntry* entryToRemove = fNext;
		removeEntry(entryToRemove);
		delete entryToRemove;
	}
}

void DelayQueue::addEntry(DelayQueueEntry* newEntry) // returns a token for the entry
{

	synchronize();
	
	m_lock.Lock();

	DelayQueueEntry* cur = head();
	while (newEntry->fDeltaTimeRemaining >= cur->fDeltaTimeRemaining) {
		newEntry->fDeltaTimeRemaining -= cur->fDeltaTimeRemaining;
		cur = cur->fNext;
	}

	cur->fDeltaTimeRemaining -= newEntry->fDeltaTimeRemaining;

	// Add "newEntry" to the queue, just before "cur":
	newEntry->fNext = cur;
	newEntry->fPrev = cur->fPrev;
	cur->fPrev = newEntry->fPrev->fNext = newEntry;

	m_lock.UnLock();
}
void DelayQueue::removeEntry(DelayQueueEntry* entry) // but doesn't delete it
{

	if (entry == NULL || entry->fNext == NULL) return;

	m_lock.Lock();
	entry->fNext->fDeltaTimeRemaining += entry->fDeltaTimeRemaining;
	entry->fPrev->fNext = entry->fNext;
	entry->fNext->fPrev = entry->fPrev;
	entry->fNext = entry->fPrev = NULL;
	m_lock.UnLock();
	// in case we should try to remove it again
}
void DelayQueue::handleAlarm()
{
	if (head()->fDeltaTimeRemaining != DELAY_ZERO) synchronize();

	//if (head()->fDeltaTimeRemaining == DELAY_ZERO) {
	while (head()->fDeltaTimeRemaining == DELAY_ZERO) {   //zmx if->while
		// This event is due to be handled:
		DelayQueueEntry* toRemove = head();
		removeEntry(toRemove); // do this first, in case handler accesses queue

		toRemove->handleTimeout();
	 
	}
 
}
void DelayQueue::synchronize() 
{

	// First, figure out how much time has elapsed since the last sync:
	_EventTime timeNow = TimeNow();
	if (timeNow < fLastSyncTime) {
		// The system clock has apparently gone back in time; reset our sync time and return:
		fLastSyncTime = timeNow;
		cout <<__FUNCTION__<< " time go back !!!! " << endl;
		return;
	}
	DelayInterval timeSinceLastSync = timeNow - fLastSyncTime;
	fLastSyncTime = timeNow;

	m_lock.Lock();
	// Then, adjust the delay queue for any entries whose time is up:
	DelayQueueEntry* curEntry = head();
	//this will slow while many node!
	while (timeSinceLastSync >= curEntry->fDeltaTimeRemaining) {
		timeSinceLastSync -= curEntry->fDeltaTimeRemaining;
		curEntry->fDeltaTimeRemaining = DELAY_ZERO;
		curEntry = curEntry->fNext;
	}
	curEntry->fDeltaTimeRemaining -= timeSinceLastSync;
	m_lock.UnLock();

}

DelayInterval const& DelayQueue::timeToNextAlarm() {
	if (head()->fDeltaTimeRemaining == DELAY_ZERO) return DELAY_ZERO; // a common case

	synchronize();
	return head()->fDeltaTimeRemaining;
}

void 	TestTaskScheduler::SingleStep(unsigned maxDelayTime) //us
{
 	DelayInterval const& timeToDelay = fDelayQueue.timeToNextAlarm();

	struct timeval tv_timeToDelay;
	tv_timeToDelay.tv_sec = timeToDelay.seconds();
	tv_timeToDelay.tv_usec = timeToDelay.useconds();
	// Very large "tv_sec" values cause select() to fail.
	// Don't make it any larger than 1 million seconds (11.5 days)

	const long MAX_TV_SEC = MILLION;
	if (tv_timeToDelay.tv_sec > MAX_TV_SEC) {
		tv_timeToDelay.tv_sec = MAX_TV_SEC;
	}
	// Also check our "maxDelayTime" parameter (if it's > 0):
	if (maxDelayTime > 0 &&
		(tv_timeToDelay.tv_sec > (long)maxDelayTime / MILLION ||
			(tv_timeToDelay.tv_sec == (long)maxDelayTime / MILLION &&
				tv_timeToDelay.tv_usec > (long)maxDelayTime%MILLION))) {
		tv_timeToDelay.tv_sec = maxDelayTime / MILLION;
		tv_timeToDelay.tv_usec = maxDelayTime%MILLION;
	}
	 
	int selectResult = 0;
	//cout << tv_timeToDelay.tv_sec << " " << tv_timeToDelay.tv_usec << endl;
   // coutTimeString();
 
		//cout << tv_timeToDelay.tv_sec << "  " << tv_timeToDelay.tv_usec << endl;
#ifdef _WIN32
	OS::msleep(1);//test
#else
	selectResult  = select(0, NULL, NULL, NULL, &tv_timeToDelay); //linux useful!
#endif
	//coutTimeString();
	//cout << "select over "<<endl;//<<tv_timeToDelay.tv_sec << "  " << tv_timeToDelay.tv_usec << endl;
 
	

	if (selectResult < 0)  
		cout << selectResult << endl;

/*
	if (selectResult < 0) {
#if defined(__WIN32__) || defined(_WIN32)
		int err = WSAGetLastError();
		// For some unknown reason, select() in Windoze sometimes fails with WSAEINVAL if
		// it was called with no entries set in "readSet".  If this happens, ignore it:
		if (err == WSAEINVAL)// && readSet.fd_count == 0)
		{
			err = EINTR;
			// To stop this from happening again, create a dummy socket:
		 
		}
		if (err != EINTR) {
#else
		if (errno != EINTR && errno != EAGAIN) {
#endif
		
		}
	 
		


	}
	else
	{
#if defined(__WIN32__) || defined(_WIN32)
		int err = WSAGetLastError();
		if (err != EINTR) {
#else
		if (errno != EINTR && errno != EAGAIN) {
#endif
			//cout << __FUNCTION__ << "else" << endl;
		}
	}
	//cout << __FUNCTION__ << " tv_timeToDelay s:" << tv_timeToDelay.tv_sec << " us:" << tv_timeToDelay.tv_usec << endl;
	*/
	  
	  fDelayQueue.handleAlarm();
 
}


void * TestTaskScheduler::scheduleDelayedTask(int64_t microseconds,
		TaskFunc* proc,
		void* clientData) 
{
		if (microseconds < 0) microseconds = 0;
		DelayInterval timeToDelay((long)(microseconds / MILLION), (long)(microseconds % MILLION));
		AlarmHandler* alarmHandler = new AlarmHandler(proc, clientData, timeToDelay);  // performance?
		fDelayQueue.addEntry(alarmHandler);

		return NULL;// (void*)(alarmHandler->token());
}