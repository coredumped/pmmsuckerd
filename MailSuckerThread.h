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
		time_t expirationTimestamp;
	};
	
	struct FailedLoginItem {
		std::string email;
		std::string errmsg;
		time_t tstamp;
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
		std::map<std::string, time_t> nextConnectAttempt;
		int iterationWaitMicroSeconds;
		int maxOpenTime;
		int maxServerReconnects;
		virtual void closeConnection(const MailAccountInfo &m); //Override me
		virtual void openConnection(const MailAccountInfo &m); //Override me
		virtual void checkEmail(const MailAccountInfo &m); //Override me
		virtual void processAccountAdd();
		virtual void processAccountRemove();
		virtual bool processAccountUpdate(const MailAccountInfo &m, size_t idx);
		
		virtual void reportFailedLogins();
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
		
		pmm::SharedQueue<pmm::DevtokenQueueItem> devTokenAddQueue;
		pmm::SharedQueue<pmm::DevtokenQueueItem> devTokenRelinquishQueue;
		pmm::SharedSet<std::string> emails2Disable;
		pmm::SharedVector<pmm::MailAccountInfo> *mailAccounts2Refresh;
		
		pmm::SharedQueue<FailedLoginItem> emailsFailingLoginsQ;
		
		MailSuckerThread();
		virtual ~MailSuckerThread();
		
		virtual void initialize();
		virtual void operator()();
		
		//Stat counters
		AtomicVar<int> cntRetrievedMessages;
		AtomicVar<int> cntAccountsActive;
		AtomicVar<int> cntAccountsTotal;
		AtomicVar<int> cntBytesDownloaded;
		AtomicVar<int> cntFailedLoginAttempts;
	};
	
	bool etpanOperationFailed(int r);
}


#endif
