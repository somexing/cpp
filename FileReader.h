#pragma once


#include <Thread.h>
#include <FSPublic.h>
#include <string>
#include "UDPSocket.h"
#include "DelayQueue.h"
#include <iostream>
#include <fstream>
#include "SpinLock.h"
#include "TPool.h"
#include "mmap.h" 
#include <tracelogwriter.h>

using namespace std;
//EX_NAMESPACE_USING;
//1s 25frame 300PKT, 1frame 12pkt 1.5K 
const int NUM_PKT_IN_FRAME = 12;//1 frame = 12*1.5KB
const int PKT_LEN =1460;
const int NUM_FRAME_PER_SECOND = 25; //400kBps
const int TEST_MINITE  = 3; // minute 
const int NUM_FRAME =  NUM_FRAME_PER_SECOND * TEST_MINITE *60;  


static int g_PB_sport = 19000;
static int g_PB_dport = 20000;

static char g_PB_sIp[] = "192.168.1.10";  //bind for send KMb

#ifdef _WIN32
  static char g_PB_dIp[] = "10.46.48.52";
#else
  static char g_PB_dIp[] = "192.168.1.11";
#endif



 

class FileReader;
class PlayBack
{
public:
	PlayBack() ;
	//PlayBack(string & filename);
    ~PlayBack() ;
     
    string & getfileName() {return fileName;};

    void createTestFile(string &);
 
    bool readSendAFrame();
	bool hasFrame();
	void performance();
    long getFrameDelay();
	void scheduleSendFrame();
   
    void initFile(string & filename);
	void openFile(string & filename);

    
    void setTrace(int j) {  bTrace = 1; cout<< j << " bTrace " << bTrace << endl;};

    int  getsendDelayUs() {  return sendDelayUs;};
    int  getsendDelaySec() {  return sendDelaySec;};

    int  getscheDelaySec() {  return scheDelaySec;};
    int  getscheDelayUs() {  return scheDelayUs;};

    int  getdiscardCounter()  { return discardCounter;};
    int  getscheduledDelayCounter() { return scheduledDelayCounter;}
    DelayInterval & getscheduledDelayInterval() { return scheduledDelayInterval;}

 private:
 
   int bTrace;
 	UDPSocket socket;
 	string	fileName;

 	long uSecsToDelay;
 	long  uSecsLast;
 	long nowpos, filesize;
 	ifstream m_ifs;
	MMapBuf m_mapfile;
	int isMmap;
 
	int dPort;
 
 	int FramesSendCounter  ;
 	int scheduledFramesCounter;
 	int scheduledFramesCounterMax;
 	int scheduledDelayCounter;
 	int discardCounter;
 	DelayInterval scheduledDelayInterval;
 	
	int TimeStampLast ;
	_EventTime m_startSecondTime;
	_EventTime m_scheTime;
	_EventTime m_sendTimeBefore;

	int sendDelaySec;
	int sendDelayUs;
	int scheDelaySec;
	int scheDelayUs; 

		char pkts_buf[NUM_PKT_IN_FRAME][PKT_LEN];
	char * pkts_buf_mmap[NUM_PKT_IN_FRAME];

    EXT_NAMESPACE::ILogWriter  * m_pLog ;
     //varible place here will happen problem!!!!

};
/*
class myMMapBuf : public EXT_NAMESPACE::MMapBuf
{
	public:
	   int Init(const char *file, INT size, INT FileFlags);
 
   private:
	   EXT_NAMESPACE::MMapBuf buf;


};*/


class FileReader  : public EXT_NAMESPACE::Thread
{
public:
	FileReader()   ;
	virtual ~FileReader(){};

	void addPlayBack(PlayBack *pb );	
 
	THREAD_FUN main() ;

	void setAffinity(int vcpu) {	AffinityCPU = vcpu;};


protected:
    TPool < PlayBack  >  * m_PBPool;

    EXT_NAMESPACE::ILogWriter  * m_pLog ;    
 
    int AffinityCPU;
  
};