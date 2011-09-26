//
//  MailAccountInfo.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/25/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_MailAccountInfo_h
#define PMM_Sucker_MailAccountInfo_h
#include <string>
#include <vector>
#include <map>

namespace pmm {
	class MailAccountInfo {
	private:
		std::string email_;
		std::string mailboxType_;
		std::string username_;
		std::string password_;
		std::string serverAddress_;
		int serverPort_;
		std::vector<std::string> devTokens_;
		bool useSSL_;
	public:
		MailAccountInfo();
		MailAccountInfo(const std::string &email__, const std::string &mailboxType__, const std::string &username__, 
						const std::string &password__, const std::string &serverAddress__, int serverPort__, 
						const std::vector<std::string> &devTokens__, bool useSSL__);
		MailAccountInfo(const MailAccountInfo &m);
		std::string email();
		std::string mailboxType();
		std::string username();
		std::string password();
		std::string serverAddress();
		int serverPort();
		std::vector<std::string> devTokens();
		bool useSSL();
	};
}

#endif
