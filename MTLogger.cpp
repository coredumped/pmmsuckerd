//
//  MTLogger.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/12/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include "MTLogger.h"
#include <iostream>
#include <time.h>

namespace pmm {
	MTLogger cerr(std::cerr);
	MTLogger cout(std::cout);
	
	MSLogger Log;
	static const char _nlBuf[] = { '\n', 0x00, '\n' };
	std::string NL(_nlBuf, 3);
	
	void MTLogger::initLogline(){
		std::string initial = streamMap[pthread_self()].str();
		if (initial.size() == 0) {
			if (outputStream == NULL) {
				outputStream = &std::cerr;
			}
			char buf[128];
			time_t theTime;
			time(&theTime);
			struct tm tmTime;
			gmtime_r(&theTime, &tmTime);
			strftime(buf, 64, "%F %T%z", &tmTime);
			streamMap[pthread_self()] << buf << " thread=0x";
			streamMap[pthread_self()] << std::hex << (long)pthread_self();
			streamMap[pthread_self()] << std::dec;
			streamMap[pthread_self()] << ": ";
		}
	}
	
	MTLogger::MTLogger(){
		outputStream = NULL;
	}
	
	MTLogger::MTLogger(std::ostream *outputStream_){
		outputStream = outputStream_;
	}
	
	void MTLogger::setOutputStream(std::ostream *outputStream_){
		outputStream = outputStream_;		
	}
	
	MTLogger &MTLogger::operator<<(int val){
		m.lock();
		initLogline();
		streamMap[pthread_self()] << val;
		m.unlock();		
	}
	
	MTLogger &MTLogger::operator<<(long val){
		m.lock();
		initLogline();
		streamMap[pthread_self()] << val;
		m.unlock();
	}
	
	MTLogger &MTLogger::operator<<(const std::string &s){
		m.lock();
		initLogline();
		if (s.compare(pmm::NL) == 0) {
			//Flush contents
			outputStream->operator<<(streamMap[pthread_self()].str());
			outputStream->operator<<("\n");
			outputStream->flush();
			streamMap[pthread_self()].str(std::string());
		}
		else {
			streamMap[pthread_self()] << s;
		}
		m.unlock();
	}
	
	MTLogger &MTLogger::operator<<(double d){
		m.lock();
		initLogline();
		streamMap[pthread_self()] << d;
		m.unlock();
	}
	
}