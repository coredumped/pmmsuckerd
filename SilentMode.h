//
//  SilentMode.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 12/4/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_SilentMode_h
#define PMM_Sucker_SilentMode_h
#include <string>
#include <time.h>

namespace pmm {
	class SilentMode {
	private:
		static void verifyTables();
	public:
		static void initialize();
		static void set(const std::string &email, int sHour, int sMinute, int eHour, int eMinute);
		static void clear(const std::string &email);
		static bool isInEffect(const std::string &email, struct tm *theTime);
	};
}


#endif
