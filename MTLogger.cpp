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
#include <sys/stat.h>

namespace pmm {
	
	MTLogger Log;
	static const char _nlBuf[] = { '\n', 0x00, '\n' };
	std::string NL(_nlBuf, 3);
	
	void MTLogger::initLogline(){
		if (streamMap[pthread_self()].size() == 0) {
			if (!outputStream.is_open()) {
				outputStream.open("mtlogger.log");
			}
			char buf[128];
			time_t theTime;
			time(&theTime);
			struct tm tmTime;
			gmtime_r(&theTime, &tmTime);
			strftime(buf, 64, "%F %T%z", &tmTime);
			std::stringstream ldata;
			if (tag.size() > 0) {
				ldata << buf << " " << tag << "(0x" << std::hex << (long)pthread_self() << ")" << std::dec << ": ";	
			}
			else {
				ldata << buf << " thread=0x" << std::hex << (long)pthread_self() << std::dec << ": ";	
			}
			streamMap[pthread_self()] = ldata.str();
		}
	}
	
	MTLogger::MTLogger(){
		//outputStream.open("mtlogger.log");
	}
	void MTLogger::open(const std::string &path){
		struct stat st;
		if(stat(path.c_str(), &st) == 0){
			//Backup old file
			std::string newname = path;
			newname.append(".old");
			rename(path.c_str(), newname.c_str());
		}
		outputStream.open(path.c_str());
	}
	
	void MTLogger::setTag(const std::string &_tag){
		tag = _tag;
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
			outputStream << streamMap[pthread_self()];
			outputStream << "\n";
			outputStream.flush();
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