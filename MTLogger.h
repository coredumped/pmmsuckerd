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

namespace pmm {
	class MTLogger {
	private:
		Mutex m;
	protected:
		std::map<pthread_t, std::string> streamMap;
		std::ostream *outputStream;
		void initLogline();
	public:
		MTLogger();
		MTLogger(std::ostream *outputStream_);
		
		void setOutputStream(std::ostream *outputStream_);
		
		MTLogger &operator<<(int val);
		MTLogger &operator<<(long val);
		MTLogger &operator<<(const std::string &s);
		MTLogger &operator<<(double d);
		
		/*operator std::ostream(){
			return *outputStream;
		}*/
	};
	
	extern MTLogger cerr;
	extern MTLogger cout;
	
	extern MTLogger Log;
	
	extern std::string NL;
}


#endif
