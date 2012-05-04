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
		devel = false;
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
		devel = false;
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
		quota = m.quota;
		isEnabled = m.isEnabled;
		devel = m.devel;
	}
	
	const std::string &MailAccountInfo::email() const {
		return email_;
	}
	
	const std::string &MailAccountInfo::mailboxType() const {
		return mailboxType_;
	}
		
	const std::string &MailAccountInfo::username() const {
		return username_;
	}
	
	const std::string &MailAccountInfo::password() const {
		return password_;
	}
	
	const std::string &MailAccountInfo::serverAddress() const {
		return serverAddress_;
	}
	
	int MailAccountInfo::serverPort(){
		return serverPort_;
	}
	
	int MailAccountInfo::serverPort() const {
		return serverPort_;
	}
		
	const std::vector<std::string> &MailAccountInfo::devTokens() const{
		return devTokens_;
	}

	
	bool MailAccountInfo::useSSL(){
		return useSSL_;
	}

	bool MailAccountInfo::useSSL() const {
		return useSSL_;
	}
	
	void MailAccountInfo::updateInfo(const std::string &password__, const std::string &serverAddress__, int serverPort__, 
									 const std::vector<std::string> &devTokens__, bool useSSL__){
		if(password__.size() > 0) password_ = password__;
		if(serverAddress__.size() > 0) serverAddress_ = serverAddress__;
		if(serverPort__ > 0) serverPort_ = serverPort__;
		if(devTokens__.size() > 0) devTokens_ = devTokens__;
		useSSL_ = useSSL__;
		isEnabled = true;
	}

	void MailAccountInfo::deviceTokenAdd(const std::string &newDevToken){
		for (size_t i = 0; i < devTokens_.size(); i++) {
			if (newDevToken.compare(devTokens_[i]) == 0) {
				return;
			}
		}
		devTokens_.push_back(newDevToken);
	}
	
	void MailAccountInfo::deviceTokenRemove(const std::string &oldDevToken){
		for(size_t i = 0; i < devTokens_.size(); i++){
			if (oldDevToken.compare(devTokens_[i]) == 0) {
				devTokens_.erase(devTokens_.begin() + i);
				break;
			}
		}
	}

	bool MailAccountInfo::operator==(const MailAccountInfo &m) const {
		if (email_.compare(m.email_) == 0 && serverAddress_.compare(m.serverAddress_) == 0) {
			return true;
		}
		return false;
	}
}