//
//  MailSuckerThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/1/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_MailSuckerThread_h
#define PMM_Sucker_MailSuckerThread_h
#include "GenericThread.h"
#include "GenericException.h"
#include "SharedVector.h"
#include "MailAccountInfo.h"
#include "SharedQueue.h"
#include "NotificationPayload.h"
#include "MTLogger.h"
#include <string>
#include <map>

namespace pmm {
	
	extern const int minimumMailCheckInterval;
	
	class MailboxControl {
	public:
		std::string email;
		time_t openedOn;
		bool isOpened;
		int availableMessages;
		time_t lastCheck;
		MailboxControl();
		MailboxControl(const MailboxControl &m);
		MailboxControl &operator=(const MailboxControl &m);
	};
	
	class MailSuckerThread : public GenericThread {
	private:
	protected:		
		Mutex mutex;
		std::map<std::string, MailboxControl> mailboxControl;
		std::map<std::string, int> serverConnectAttempts;
		int iterationWaitMicroSeconds;
		int maxOpenTime;
		int maxServerReconnects;
		virtual void closeConnection(const MailAccountInfo &m); //Override me
		virtual void openConnection(const MailAccountInfo &m); //Override me
		virtual void checkEmail(const MailAccountInfo &m); //Override me
	public:
		SharedVector<MailAccountInfo> emailAccounts;
		SharedQueue<NotificationPayload> *notificationQueue;
		SharedQueue<NotificationPayload> *pmmStorageQueue;
		SharedVector<std::string> *quotaUpdateVector;

		MailSuckerThread();
		virtual ~MailSuckerThread();
		
		virtual void operator()();
	};
}


#endif
