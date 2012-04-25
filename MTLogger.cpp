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

#ifndef MAX_LOG_SIZE
#define MAX_LOG_SIZE 1073741824
#endif

namespace pmm {
	
	MTLogger Log;
	static const char _nlBuf[] = { '\n', 0x00, '\n' };
	std::string NL(_nlBuf, 3);
	
	void MTLogger::initLogline(){
		if (streamMap.find(pthread_self()) == streamMap.end()){
			streamMap[pthread_self()] = "";
		}
		if (streamMap[pthread_self()].size() == 0) {
			if (!outputStream.is_open()) {
				logPath = "mtlogger.log";
				outputStream.open(logPath.c_str());
			}
			char buf[128];
			time_t theTime;
			time(&theTime);
			struct tm tmTime;
			gmtime_r(&theTime, &tmTime);
			strftime(buf, 64, "%F %T%z", &tmTime);
			std::stringstream ldata;
			if (tag.size() > 0) {
				ldata << buf << " " << tag << "(Thread=0x" << std::hex << (long)pthread_self() << ")" << std::dec << ": ";	
			}
			else {
				ldata << buf << " [Thread=0x" << std::hex << (long)pthread_self() << std::dec << "]: ";	
			}
			streamMap[pthread_self()] = ldata.str();
		}
	}
	
	MTLogger::MTLogger(){
		//outputStream.open("mtlogger.log");
		writtenBytes = 0;
		maxLogSize = MAX_LOG_SIZE;
	}
	void MTLogger::open(const std::string &path){
		struct stat st;
		if(stat(path.c_str(), &st) == 0){
			//Backup old file
			if (st.st_size > 2 * maxLogSize) {
				unlink(path.c_str());
				std::cerr << path << " is too big (" << (st.st_size / (1024 * 1024)) << " MB), erasing it..." << std::endl;
			}
			else {
				std::string newname = path;
				newname.append(".old");
				rename(path.c_str(), newname.c_str());
			}
		}
		writtenBytes = 0;
		logPath = path;
		if (outputStream.is_open()) {
			outputStream.close();
		}
		outputStream.open(path.c_str(), std::ios_base::app);
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
			if(writtenBytes > maxLogSize){
				//Tricky part we must rotate the log and compress the previous one
				outputStream.close();
				open(logPath);
			}
		}
		else {
			std::stringstream ldata;
			ldata << streamMap[pthread_self()] << s;
			streamMap[pthread_self()] = ldata.str();
			writtenBytes += streamMap[pthread_self()].size();
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
	
	void MTLogger::flush(){
		m.lock();
		initLogline();
		std::stringstream ldata;
		outputStream << streamMap[pthread_self()];
		outputStream.flush();
		streamMap.erase(pthread_self());
		if(writtenBytes > maxLogSize){
			//Tricky part we must rotate the log and compress the previous one
			outputStream.close();
			open(logPath);
		}
		m.unlock();
	}
}