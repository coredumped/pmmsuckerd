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
	
	/** A queue object for describing e-mail accounts to be notified in the case of multiple failed login attempts */
	struct FailedLoginItem {
		std::string errmsg;
		time_t tstamp;
		MailAccountInfo m;
		bool gmailAuthReq;
		FailedLoginItem() {
			gmailAuthReq = false;
		}
	};
	
	/** Has information regarding mailbox monitoring parameters a very few stats (if any) */
	class MailboxControl {
	public:
		/** User e-mail being monitored */
		std::string email;
		/** Timestamp holding the point in time when the last succesful login happened. */
		time_t openedOn;
		/** Tells wheter we have an open connection to such mailbox */
		bool isOpened;
		/** Holds the amount of available (unseen) messages in the mailbox */
		int availableMessages __attribute__ ((deprecated));
		time_t lastCheck;
		/** Timestamp holding the moment when this MailboxControl object was first instanced */
		time_t monitoringStartedOn;
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
		virtual void scheduleFailureReport(const MailAccountInfo &m, const std::string &errmsg, bool isGmailAuthReq);
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
		pmm::SharedQueue<std::string> *gmailAuthRequestedQ;
		
		/* Unlimited subscription-related data-structures */
		/** Tells wheter the given mailbox is associated to an in-app subscription */
		pmm::SharedSet<std::string> *subscribedSet;
		
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
