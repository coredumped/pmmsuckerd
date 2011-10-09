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
		void build();
	protected:
		std::string _soundName;
		std::string msg;
		std::string devToken;
		int _badgeNumber;
		std::string jsonRepresentation;
	public:
		NotificationPayload();
		NotificationPayload(const std::string &devToken_, const std::string &_message, int badgeNumber, const std::string &sndName = "default");
		NotificationPayload(const NotificationPayload &n);
		~NotificationPayload();
		
		//std::string &toJSON();
		const std::string &toJSON() const;
		std::string &soundName();
		std::string &message();
		std::string &deviceToken();
	};
}



#endif
