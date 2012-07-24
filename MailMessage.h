//
//  MailMessage.h
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/14/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_MailMessage_h
#define PMM_Sucker_MailMessage_h
#include<string>

namespace pmm {
	class MailMessage {
	private:
	protected:
	public:
		std::string to;
		std::string from;
		std::string fromEmail;
		std::string subject;
		std::string htmlMsg;
		time_t dateOfArrival;
		int tzone;
		std::string msgUid;
		time_t serverDate;
		MailMessage();
		MailMessage(const std::string &_from, const std::string &_subject);
		MailMessage(const MailMessage &m);
		static bool parse(MailMessage &m, const std::string &rawMessage);
		static bool parse(MailMessage &m, const char *msgBuffer, size_t msgSize);
	};
}

#endif
