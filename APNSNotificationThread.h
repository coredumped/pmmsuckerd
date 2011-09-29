//
//  APNSNotificationThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_APNSNotificationThread_h
#define PMM_Sucker_APNSNotificationThread_h
#include"GenericThread.h"
#include <string>
namespace pmm {
	
	class APNSNotificationThread : public GenericThread {
	private:
	protected:
		void notifyTo(const std::string &devToken, const std::string &msg);
	public:
		APNSNotificationThread();
		~APNSNotificationThread();
		void operator()();
	};
	
}
#endif
