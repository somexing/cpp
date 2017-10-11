	#include "FileReader.h"
#include <System.h>
#include "DssApp.h"
#include <SpinLock.h>
#include "EventMonitor.h"
#define  MODULE_NAME   "FileReader" 

static void schedulePBFunc(void * pPB)
{
	PlayBack * pb = (PlayBack *)pPB;
  //  coutTimeString();    cout << "schedulePBFunc start" << endl;
	
	if (pb->hasFrame())
		pb->scheduleSendFrame();//next read
    
   // coutTimeString();cout << "schedulePBFunc over" << endl;
	
	GetApp().getFileReader()->addPlayBack(pb);
 
   // coutTimeString();cout << "addPlayBack over" << endl;
 
}
 


PlayBack::~PlayBack()
{
	if (!isMmap)
	   m_ifs.close();
	else
	{
       m_mapfile.release();
	   m_mapfile.clear();
	}	 
}

PlayBack::PlayBack( ):scheduledDelayInterval(0,0)
{
#ifdef _WIN32
	socket.Create(g_PB_sport++);
#else
	socket.Create(g_PB_sIp, g_PB_sport++);
#endif
	dPort = g_PB_dport++;
	cout << " send to ip : " << g_PB_dIp << " port:" << dPort << endl;

	uSecsToDelay = 0;
	nowpos = 0;
	FramesSendCounter = 0;
    scheduledFramesCounter  = 0;
    discardCounter = 0;
    bTrace = 0;
  

    sendDelaySec = 0;
    sendDelayUs = 0;
	scheDelaySec = 0;
	scheDelayUs = 0;    

    isMmap = true;
  
    	 

    m_pLog  = GetApp().GetLogWriter();

};

 

void PlayBack::openFile(string & filename)
{
	if (isMmap)
	{
		m_mapfile.SetRO(true);  //read only?
		m_mapfile.Init(fileName.c_str(), filesize);
	}
	else
	{
		m_ifs.open(fileName.c_str(), ios::in | ios::binary); //linux use c_str() 
		m_ifs.seekg(0, ios::beg);
	}
 
}



void PlayBack::initFile(string & filename)
{
	 fileName = filename;
	m_ifs.open(fileName.c_str(), ios::in | ios::binary); //linux use c_str() 
	if (!m_ifs)     //if not exists
    {
		createTestFile(fileName);

		m_ifs.open(fileName.c_str(), ios::in | ios::binary); //linux use c_str() 
	}


	m_ifs.seekg(0, ios::end);
	filesize = m_ifs.tellg();


	if (bTrace == 1)
	  cout << "test file " << fileName << " size = " << filesize << endl;

    m_ifs.close();

    openFile(fileName);

}


void PlayBack::createTestFile(string & filename)
{
	ofstream ofile;
	ofile.open(filename.c_str(), ios::out | ios::binary);


	ofile.seekp(0, ios::beg);

	char pkt[PKT_LEN] = "test";

	for (int frame_num = 0; frame_num < NUM_FRAME; frame_num++)
	{
		for (int pkt_num = 0; pkt_num < NUM_PKT_IN_FRAME; pkt_num++)
		{
			ofile.write(pkt, sizeof(pkt));
		}
	}
	ofile.flush();
	ofile.close();
	if (bTrace == 1)
      cout << "file created :" << fileName;

}


void PlayBack::scheduleSendFrame()
{
   long uSecs ;// = getFrameDelay(); //us
   
   if (scheduledFramesCounter == 0) {
 		uSecs = 0;
	}
	else
	{	
		uSecs = 35000 ;//35ms
        if (scheduledFramesCounter % NUM_FRAME_PER_SECOND == 0) //firstframe  aftre second 2
        {
            uSecs = 1000000 - uSecs*(NUM_FRAME_PER_SECOND -1);//35000 ,160000; // 160ms
        }
	}
	uSecsLast = uSecs;
	 
   scheduledFramesCounter ++;

	DelayInterval delayTimeDiff(0,0) ;
	if (scheduledFramesCounter > 1)
	   delayTimeDiff = TimeNow() -  m_scheTime -  DelayInterval(uSecsLast/1000000, uSecsLast%1000000); 

	if (scheduledFramesCounter == 1)
	   m_scheTime = TimeNow();
	else
	   m_scheTime += DelayInterval(uSecsLast/1000000, uSecsLast%1000000);


	if (delayTimeDiff > DelayInterval(0,0)) //delayed
	{
   	    scheduledDelayCounter ++;
		
		scheduledDelayInterval += delayTimeDiff;

        uSecs -= delayTimeDiff.seconds()*1000000 + delayTimeDiff.useconds();   //us
        if (uSecs < 0) 
        	uSecs = 0;
        m_pLog->Trace(LOG_INFO,  MODULE_NAME, __LINE__, 
         	" file %s will send frame %d ttl: %d interval is %ds %dms(%d us), scheduled %d ms fixed to send",
         	getfileName().c_str(),  (scheduledFramesCounter-1) % NUM_FRAME_PER_SECOND + 1,
         	scheduledFramesCounter, delayTimeDiff.seconds(), delayTimeDiff.useconds() / 1000, 
         	delayTimeDiff.useconds(),
         	uSecs/1000 );
	}

	if (bTrace  == 1  )
    {
          coutTimeString();
          cout << " file " <<  getfileName() << " will scheduled "
           << uSecs/1000 << "ms("<<uSecs<<"us) to send frame "
                << (scheduledFramesCounter-1)/ NUM_FRAME_PER_SECOND  +1
                <<"s:" << (scheduledFramesCounter-1) % NUM_FRAME_PER_SECOND + 1    	 
 			    <<" pos:"<<nowpos << " interval delayed is " 
			    << delayTimeDiff.seconds() << "s "
				<< delayTimeDiff.useconds() / 1000 << " ms(" << delayTimeDiff.useconds() << "us), " 
          
				<< endl;
    } 
 

   GetApp().getTestTaskScheduler()->scheduleDelayedTask(uSecs, schedulePBFunc, this);	

    
	 //GetApp().GetEventMonitor()->scheduleDelayedTask(uSecs, schedulePBFunc, this);	
     

}

bool PlayBack::readSendAFrame()
{
	
	//performance
 
    _EventTime sendTimeNow = TimeNow();
    
    DelayInterval timeDiffFramed(0,0);
    if (FramesSendCounter > 1)
	   timeDiffFramed = sendTimeNow -  m_sendTimeBefore;
    m_sendTimeBefore  = sendTimeNow;

    scheDelaySec += timeDiffFramed.seconds();
    scheDelayUs +=  timeDiffFramed.useconds() ;  	
    
    DelayInterval timeDiff = timeDiffFramed -  DelayInterval(0, 40000); //call back list call delay
	if (timeDiffFramed > DelayInterval(0, 40000)) //   each frame  delay 40ms 
	{
		string sInfo = " file %s before send frame  %d ,ttl sended %d. thread run delayed >1ms, delayed %ds %d ms(%dus)";
        m_pLog->Trace(LOG_INFO, MODULE_NAME, __LINE__, sInfo.c_str(), getfileName().c_str(), 
          	           FramesSendCounter % NUM_FRAME_PER_SECOND + 1,
                        FramesSendCounter, timeDiff.seconds(), timeDiff.useconds() / 1000, timeDiff.useconds());
 	}

	if (bTrace  == 1  )
	{	
		    coutTimeString();
			cout << " file " <<  getfileName() <<" before send frame "
			    <<    FramesSendCounter /NUM_FRAME_PER_SECOND + 1
			   << "s:" <<FramesSendCounter % NUM_FRAME_PER_SECOND + 1
			   <<" pos:"<<nowpos 			   
			     <<" after last frame   " << timeDiffFramed.seconds() << "s" 
			 	<< timeDiffFramed.useconds() / 1000 << " ms(" << timeDiffFramed.useconds() << "us)" << endl;
	}

    DelayInterval timeDiffthisSec = sendTimeNow - m_startSecondTime;
    if (timeDiffthisSec > DelayInterval(1, 0)) //   each 25 frames shoulde in 1S send 
	{	
		    if (bTrace  ==1  )
		    {	     
			  coutTimeString();
		
          
			  cout << " file " <<  getfileName() <<" before send frame "
			    <<    FramesSendCounter /NUM_FRAME_PER_SECOND + 1
			   << "s:" <<FramesSendCounter % NUM_FRAME_PER_SECOND + 1			    
			    <<". now is "<< timeDiffthisSec.seconds() << "s " 
			 	<< timeDiffthisSec.useconds() / 1000 << " ms(" << timeDiffthisSec.useconds() << "us)"
			    <<", discard!!!" << endl;
	        }
            nowpos += PKT_LEN * NUM_PKT_IN_FRAME;
            FramesSendCounter ++;//  for performace use;
            discardCounter ++;
			return true; //not end
	}
 
	   


    int ret ;
    for (int pkt_num = 0; pkt_num < NUM_PKT_IN_FRAME; pkt_num++)
	{
		_EventTime timeNow = TimeNow();

	
		if (nowpos + PKT_LEN > filesize)
		{
			nowpos=filesize;
			return false;
		}

		if (!isMmap) 
		{
		   m_ifs.read(pkts_buf[pkt_num], PKT_LEN);//fix PKT_LEN, test

            DelayInterval timeDiff_read = TimeNow() - timeNow;
            if (timeDiff_read > DelayInterval(0, 1000) ) //1ms
            	m_pLog->Trace(LOG_INFO, MODULE_NAME,__LINE__, " file %s direct read a packet >1ms ,use %d s, %d ms(%d us)", 
			                 getfileName().c_str() , timeDiff_read.seconds() ,
				             timeDiff_read.useconds() / 1000 ,  timeDiff_read.useconds() );

		   ret = socket.SendTo(pkts_buf[pkt_num], PKT_LEN, g_PB_dIp, dPort);;
 
		}
		else
		{

            m_mapfile.release();
			pkts_buf_mmap[pkt_num] =(char *) m_mapfile.Get(nowpos, PKT_LEN);
            
            DelayInterval timeDiff_read = TimeNow() - timeNow;
            if (timeDiff_read > DelayInterval(0, 1000) ) //1ms
            {
            	//m_pLog->Trace(LOG_INFO, MODULE_NAME,__LINE__, " file %s mmap read a packet >1ms ,use %d s, %d ms(%d us)", 
			      //           getfileName().c_str() , timeDiff_read.seconds() ,
				    //         timeDiff_read.useconds() / 1000 ,  timeDiff_read.useconds() );

             if (bTrace ==1  )
            	cout<< " file " <<  getfileName() <<" read mapfile " 
            	    <<    FramesSendCounter /NUM_FRAME_PER_SECOND + 1
			       << "s:" <<FramesSendCounter % NUM_FRAME_PER_SECOND + 1	

			    <<" >1ms, use " << timeDiff_read.seconds() << "s " 
			 	<< timeDiff_read.useconds() / 1000 << " ms(" << timeDiff_read.useconds() << "us)" << endl;
            }



     		ret = socket.SendTo(pkts_buf_mmap[pkt_num], PKT_LEN, g_PB_dIp, dPort);;
	 
		}
		
		if (ret != PKT_LEN)
		{
			   cout << "SendTo " << g_PB_dIp << ":" << dPort << " ret:" << ret<<" "<< OS::GetLastErrorCode() << " " << OS::GetLastErrorDesc().c_str() << endl;
		}
		nowpos += PKT_LEN;
	};

 

	
	FramesSendCounter ++;// NUM_PKT_IN_FRAME;

 	DelayInterval timeDiff2 = TimeNow() - sendTimeNow;
	if (timeDiff2 > DelayInterval(0, 1000)) //1ms
	{
		  if (bTrace  ==1 )
		{
			 coutTimeString();
		 
		cout << " file " <<  getfileName() <<" read & send frame " 
		        	    <<    (FramesSendCounter-1) /NUM_FRAME_PER_SECOND + 1
			       << "s:"<<(FramesSendCounter-1) % NUM_FRAME_PER_SECOND + 1	
			    <<" >1ms, use " << timeDiff2.seconds() << "s " 
			 	<< timeDiff2.useconds() / 1000 << " ms(" << timeDiff2.useconds() << "us)" << endl;
			 	 }
		string str = " file %s read & send frame %d > 1ms,use %d s %d ms(%dus)";
		//m_pLog->Trace(LOG_INFO, MODULE_NAME,__LINE__, str.c_str(), getfileName().c_str(), 
		//	FramesSendCounter % NUM_FRAME_PER_SECOND, timeDiff2.seconds() , timeDiff2.useconds() / 1000,
		//	  timeDiff2.useconds());


	}

   	if (nowpos == filesize)
	{
		return false;   //over
	}

	return true;
}



long PlayBack::getFrameDelay()
{
	if (uSecsToDelay == 0) {
		uSecsToDelay = 40000;// us = 40ms; should get From Frame timestamp
		return 0;
	}

	return uSecsToDelay;
}

bool PlayBack::hasFrame()
{
	//return (nowpos < filesize);
	//schedule faster than func
	return (scheduledFramesCounter < NUM_FRAME);
}
void PlayBack::performance()
{
 	
	if (FramesSendCounter == 0)
	{
		 _EventTime timeNow = TimeNow();
		 m_startSecondTime = timeNow;
	}
	else if (FramesSendCounter % NUM_FRAME_PER_SECOND == 0)
	{
		_EventTime timeNow = TimeNow();
		DelayInterval timeDiff = timeNow -  m_startSecondTime;
		m_startSecondTime += DelayInterval(1,0);

        sendDelaySec += timeDiff.seconds()  ;
        sendDelayUs +=  timeDiff.useconds()  ;

		if (timeDiff > DelayInterval(1, 0 )) //1S10ms
		{
			if (bTrace  == 1 )
		    {  
			   coutTimeString();
			   cout << " file " <<  getfileName() << " sended all no."<< FramesSendCounter / NUM_FRAME_PER_SECOND  
			        <<"s frames, performance>1s ! use " << timeDiff.seconds() << "s "
			     	<< timeDiff.useconds() / 1000 << " ms(" << timeDiff.useconds() << "us) ttl sended:" 
				<< FramesSendCounter<< endl;
			}
            string  sInfo = " file %s  sended &d frames, >1s ! use %d s  %d ms( %d us) ttl sended: %d ";
           // m_pLog->Trace(LOG_INFO, MODULE_NAME, __LINE__, sInfo.c_str(), getfileName().c_str(), 
          //	           NUM_FRAME_PER_SECOND,
            //             timeDiff.seconds(), timeDiff.useconds() / 1000, timeDiff.useconds(),
           //              FramesSendCounter);

		}

	}

}
 
 


FileReader::FileReader() :AffinityCPU(-1){
		m_PBPool = new  TPool<PlayBack> (0) ;//no need reserve idle queue
		m_pLog  = GetApp().GetLogWriter();
	};

void FileReader::addPlayBack(PlayBack *pPB)
{
 

	m_PBPool->push_back(pPB);
	/*int poolSize =  m_PBPool->getSize();
	if (poolSize > 1)
	{
 		m_pLog->Trace(LOG_INFO, MODULE_NAME,__LINE__, " file %s addPlayBack, pool size=  %d ", 
			poolSize);
	}*/
 
}





THREAD_FUN FileReader::main()
{
		 
	THREAD_AFFINITY affinity;
		affinity.cpu = 0;
		affinity.core = 0;
		affinity.vcpu = AffinityCPU;
		affinity.mask = 0;
		if (AffinityCPU)
		  ThreadAffinity(&affinity);		 

	struct timeval tv_timeToDelay ;//0.1ms
	while (true)
	{
		PlayBack * pb = m_PBPool->getT();
		while (pb == NULL)
		{
			//OS::msleep(1); //minium sleep 1ms else other thread will not run!
			//tv_timeToDelay = { 0, 10};//0.5ms
           // select(0, NULL, NULL, NULL, &tv_timeToDelay);


			//cout << "."<<endl;
			pb = m_PBPool->getT();
		}

        pb->performance();
		if (!pb->readSendAFrame())
		{

			cout << " file " << pb->getfileName() << " finished!!! ttl send delay "
			    << pb->getscheduledDelayCounter() << " times,delayed "
			    << pb->getscheduledDelayInterval().seconds() << " s, " 
			    << pb->getscheduledDelayInterval().useconds()/1000 << " ms.  send cost  "  
			    << pb->getsendDelaySec() + pb->getsendDelayUs()/1000000 << " s, " 
			    << pb->getsendDelayUs()/1000 << " ms. schedule cost  "  
			    << pb->getscheDelaySec() + pb->getscheDelayUs()/1000000 << " s, " 
			    << pb->getscheDelayUs()/1000 << " ms.  discard frame number :" 
			    << pb->getdiscardCounter() <<endl;
			//should notify delete this pb
		}

	}



	return 0;

}