//
//  NotificationPayload.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/29/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_NotificationPayload_h
#define PMM_Sucker_NotificationPayload_h
#include <string>
#include <map>
#include "MailMessage.h"

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
		std::map<std::string, std::string> customParams;
		NotificationPayload();
		NotificationPayload(const std::string &devToken_, const std::string &_message, int badgeNumber = 1, const std::string &sndName = "pmm.caf");
		NotificationPayload(const NotificationPayload &n);
		virtual ~NotificationPayload();
		MailMessage origMailMessage;
		
		const std::string &toJSON();
		std::string &soundName();
		const std::string &soundName() const;
		std::string &message();
		std::string &deviceToken();
		bool isSystemNotification;
		void useSilentSound();
		int badge();
		
		int attempts;
	};
	
	class NoQuotaNotificationPayload : public NotificationPayload {
	protected:
	public:
		NoQuotaNotificationPayload();
		NoQuotaNotificationPayload(const std::string &devToken_, const std::string &_email, const std::string &sndName = "default", int badgeNumber = 911);
		NoQuotaNotificationPayload(const NoQuotaNotificationPayload &n);
		virtual ~NoQuotaNotificationPayload();
	};
}



#endif
