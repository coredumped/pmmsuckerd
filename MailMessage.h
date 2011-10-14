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
		std::string from;
		std::string subject;
		MailMessage();
		MailMessage(const std::string &_from, const std::string &_subject);
		MailMessage(const MailMessage &m);
		static void parse(MailMessage &m, const std::string &rawMessage);
	};
}

#endif
