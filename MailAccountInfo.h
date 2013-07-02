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
#include "Mutex.h"
#include "Atomic.h"

namespace pmm {
	class MailAccountInfo {
	private:
		std::string email_;
		std::string mailboxType_;
		std::string username_;
		std::string password_;
		std::string serverAddress_;
		int serverPort_;
		bool useSSL_;
		std::vector<std::string> devTokens_;
	public:
		MailAccountInfo();
		MailAccountInfo(const std::string &email__, const std::string &mailboxType__, const std::string &username__, 
						const std::string &password__, const std::string &serverAddress__, int serverPort__, 
						const std::vector<std::string> &devTokens__, bool useSSL__, bool usesOAuth__);
		MailAccountInfo(const MailAccountInfo &m);
		const std::string &email() const;
		const std::string &mailboxType() const;
		const std::string &username() const;
		const std::string &password() const;
		const std::string &serverAddress() const;
		int serverPort();
		int serverPort() const;
		const std::vector<std::string> &devTokens() const;
		bool useSSL();
		bool useSSL() const;
		AtomicFlag isEnabled;
		int quota;
		bool devel;
		bool usesOAuth;
		
		void updateInfo(const std::string &password__, const std::string &serverAddress__, int serverPort__, 				
					   const std::vector<std::string> &devTokens__, bool useSSL__, bool usesOAuth__);

		void deviceTokenAdd(const std::string &newDevToken);
		void deviceTokenRemove(const std::string &oldDevToken);
		
		bool operator==(const MailAccountInfo &m) const;
	};
}

#endif
