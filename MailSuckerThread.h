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
#include "FetchedMailsCache.h"
#include "MTLogger.h"
#include "DataTypes.h"
#include "SharedSet.h"
#include <string>
#include <map>

namespace pmm {
	
	extern const int minimumMailCheckInterval;
	
	struct DevtokenQueueItem {
		std::string email;
		std::string devToken;
	};
	
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
	protected:		
		time_t threadStartTime;
		std::map<std::string, MailboxControl> mailboxControl;
		std::map<std::string, int> serverConnectAttempts;
		int iterationWaitMicroSeconds;
		int maxOpenTime;
		int maxServerReconnects;
		virtual void closeConnection(const MailAccountInfo &m); //Override me
		virtual void openConnection(const MailAccountInfo &m); //Override me
		virtual void checkEmail(const MailAccountInfo &m); //Override me
		virtual void processAccountAdd();
		virtual void processAccountRemove();
		//Device token addition and removal
		virtual void registerDeviceTokens();
		virtual void relinquishDeviceTokens();
	public:
		FetchedMailsCache fetchedMails;
		SharedVector<MailAccountInfo> emailAccounts;
		SharedQueue<NotificationPayload> *notificationQueue;
		SharedQueue<NotificationPayload> *develNotificationQueue;
		SharedQueue<NotificationPayload> *pmmStorageQueue;
		SharedVector<std::string> *quotaUpdateVector;
		SharedQueue<QuotaIncreasePetition> *quotaIncreaseQueue;
		pmm::SharedQueue<pmm::MailAccountInfo> *addAccountQueue;
		pmm::SharedQueue<std::string> *rmAccountQueue;
		
		pmm::SharedQueue<pmm::DevtokenQueueItem> *devTokenAddQueue;
		pmm::SharedQueue<pmm::DevtokenQueueItem> *devTokenRelinquishQueue;
		pmm::SharedSet<std::string> *emails2Disable;

		MailSuckerThread();
		virtual ~MailSuckerThread();
		
		virtual void initialize();
		virtual void operator()();
		
		//Stat counters
		AtomicVar<int> cntRetrievedMessages;
		AtomicVar<int> cntAccountsActive;
		AtomicVar<int> cntAccountsTotal;
	};
	
	bool etpanOperationFailed(int r);
}


#endif
