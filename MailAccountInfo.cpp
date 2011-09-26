//
//  MailAccountInfo.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/25/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include "MailAccountInfo.h"

namespace pmm {
	
	MailAccountInfo::MailAccountInfo(){
		
	}
	
	MailAccountInfo::MailAccountInfo(const std::string &email__, const std::string &mailboxType__, const std::string &username__, 
					const std::string &password__, const std::string &serverAddress__, int serverPort__, 
					const std::vector<std::string> &devTokens__, bool useSSL__){
		email_ = email__;
		mailboxType_ = mailboxType__;
		username_ = username__;
		password_ = password__;
		serverAddress_ = serverAddress__;
		serverPort_ = serverPort__;
		devTokens_ = devTokens__;
		useSSL_ = useSSL__;
	}
	
	MailAccountInfo::MailAccountInfo(const MailAccountInfo &m){
		email_ = m.email_;
		mailboxType_ = m.mailboxType_;
		username_ = m.username_;
		password_ = m.password_;
		serverAddress_ = m.serverAddress_;
		serverPort_ = m.serverPort_;
		devTokens_ = m.devTokens_;
		useSSL_ = m.useSSL_;
	}
	
	std::string MailAccountInfo::email(){
		return std::string(email_);
	}
	
	std::string MailAccountInfo::mailboxType(){
		return std::string(mailboxType_);
	}
	
	std::string MailAccountInfo::username(){
		return std::string(username_);
	}
	
	std::string MailAccountInfo::password(){
		return std::string(password_);
	}
	
	std::string MailAccountInfo::serverAddress(){
		return std::string(serverAddress_);
	}
	
	int MailAccountInfo::serverPort(){
		return serverPort_;
	}
	
	std::vector<std::string> MailAccountInfo::devTokens(){
		return std::vector<std::string>(devTokens_);
	}
	
	bool MailAccountInfo::useSSL(){
		return useSSL_;
	}
	
}