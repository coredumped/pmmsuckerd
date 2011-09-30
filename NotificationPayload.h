//
//  NotificationPayload.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/29/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_NotificationPayload_h
#define PMM_Sucker_NotificationPayload_h
#include<string>

namespace pmm {
	class NotificationPayload {
	private:
	protected:
		std::string _soundName;
		std::string msg;
		std::string devToken;
	public:
		NotificationPayload();
		NotificationPayload(const std::string &devToken_, const std::string &_message, const std::string &sndName = "default");
		NotificationPayload(const NotificationPayload &n);
		
		std::string toJSON();
		std::string &soundName();
		std::string &message();
		std::string &deviceToken();
	};
}



#endif
