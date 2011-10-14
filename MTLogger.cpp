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
	MTLogger cerr(&std::cerr);
	MTLogger cout(&std::cout);
	
	MTLogger Log;
	static const char _nlBuf[] = { '\n', 0x00, '\n' };
	std::string NL(_nlBuf, 3);
	
	void MTLogger::initLogline(){
		if (streamMap[pthread_self()].size() == 0) {
			if (outputStream == NULL) {
				outputStream = &std::cerr;
			}
			char buf[128];
			time_t theTime;
			time(&theTime);
			struct tm tmTime;
			gmtime_r(&theTime, &tmTime);
			strftime(buf, 64, "%F %T%z", &tmTime);
			std::stringstream ldata;
			ldata << buf << " thread=0x" << std::hex << (long)pthread_self() << std::dec << ": ";
			streamMap[pthread_self()] = ldata.str();
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
		std::stringstream ldata;
		ldata << streamMap[pthread_self()] << val;
		streamMap[pthread_self()] = ldata.str();
		m.unlock();		
		return *this;
	}
	
	MTLogger &MTLogger::operator<<(long val){
		m.lock();
		initLogline();
		std::stringstream ldata;
		ldata << streamMap[pthread_self()] << val;
		streamMap[pthread_self()] = ldata.str();
		m.unlock();
		return *this;
	}
	
	MTLogger &MTLogger::operator<<(const std::string &s){
		m.lock();
		initLogline();
		if (s.compare(pmm::NL) == 0) {
			//Flush contents
			std::stringstream ldata;
			outputStream->operator<<(streamMap[pthread_self()].c_str());
			outputStream->operator<<("\n");
			outputStream->flush();
			streamMap.erase(pthread_self());
		}
		else {
			std::stringstream ldata;
			ldata << streamMap[pthread_self()] << s;
			streamMap[pthread_self()] = ldata.str();
		}
		m.unlock();
		return *this;
	}
	
	MTLogger &MTLogger::operator<<(double d){
		m.lock();
		initLogline();
		std::stringstream ldata;
		ldata << streamMap[pthread_self()] << d;
		streamMap[pthread_self()] = ldata.str();
		m.unlock();
		return *this;
	}
	
}