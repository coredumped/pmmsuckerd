//
//  MTLogger.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/12/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_MTLogger_h
#define PMM_Sucker_MTLogger_h
#include"Mutex.h"
#include <string>
#include <sstream>
#include <map>
#include <iostream>
#include <fstream>

namespace pmm {
	class MTLogger {
	private:
		Mutex m;
	protected:
		std::map<pthread_t, std::string> streamMap;
		std::ofstream *outputStream;
		void initLogline();
	public:
		MTLogger();
		MTLogger(std::ofstream *outputStream_);
		
		void setOutputStream(std::ofstream *outputStream_);
		
		MTLogger &operator<<(int val);
		MTLogger &operator<<(long val);
		MTLogger &operator<<(const std::string &s);
		MTLogger &operator<<(double d);
	};
		
	extern MTLogger Log;
	
	extern std::string NL;
}


#endif
