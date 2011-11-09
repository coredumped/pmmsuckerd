//
//  MailSuckerThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/1/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <time.h>
#include <map>
#include "MailSuckerThread.h"
#include "MTLogger.h"
#include "libetpan/libetpan.h"

#ifndef DEFAULT_WAIT_TIME_BETWEEN_MAIL_CHECKS
#define DEFAULT_WAIT_TIME_BETWEEN_MAIL_CHECKS 250000
#endif

#ifndef DEFAULT_MAX_OPEN_TIME
#define DEFAULT_MAX_OPEN_TIME -1
#endif

#ifndef DEFAULT_MAX_SERVER_CONNECT_FAILURES
#define DEFAULT_MAX_SERVER_CONNECT_FAILURES 10 
#endif

#ifndef DEFAULT_MINIMUM_MAIL_CHECK_INTERVAL
#define DEFAULT_MINIMUM_MAIL_CHECK_INTERVAL 3
#endif

namespace pmm {
	
	const int minimumMailCheckInterval = DEFAULT_MINIMUM_MAIL_CHECK_INTERVAL;
	
	bool etpanOperationFailed(int r)
	{
		if (r == MAILIMAP_NO_ERROR)
			return false;
		if (r == MAILIMAP_NO_ERROR_AUTHENTICATED)
			return false;
		if (r == MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
			return false;
		return true;
	}

	
	MailboxControl::MailboxControl(){
		openedOn = time(0);
		isOpened = false;
		availableMessages = 0;
		lastCheck = 0;
	}
	
	MailboxControl::MailboxControl(const MailboxControl &m){
		openedOn = m.openedOn;
		isOpened = m.isOpened;
		email = m.email;
		availableMessages = m.availableMessages;
		lastCheck = m.lastCheck;
	}
	
	MailboxControl &MailboxControl::operator=(const MailboxControl &m){
		openedOn = m.openedOn;
		isOpened = m.isOpened;
		email = m.email;
		availableMessages = m.availableMessages;
		lastCheck = m.lastCheck;
		return *this;
	}
	
	MailSuckerThread::MailSuckerThread(){
		iterationWaitMicroSeconds = DEFAULT_WAIT_TIME_BETWEEN_MAIL_CHECKS;
		maxOpenTime = DEFAULT_MAX_OPEN_TIME;
		notificationQueue = NULL;
		maxServerReconnects = DEFAULT_MAX_SERVER_CONNECT_FAILURES;
		quotaUpdateVector = NULL;
		quotaIncreaseQueue = NULL;
	}
	
	MailSuckerThread::~MailSuckerThread(){
		
	}

	void MailSuckerThread::operator()(){
		if (notificationQueue == NULL) throw GenericException("notificationQueue is still NULL, it must point to a valid notification queue.");
		if (quotaUpdateVector == NULL) throw GenericException("quotaUpdateQueue is still NULL, it must point to a valid notification queue.");
		if (pmmStorageQueue == NULL) throw GenericException("Can't continue like this, the pmmStorageQueue is null!!!");
		if (quotaIncreaseQueue == NULL) throw GenericException("Can't continue like this, the quotaIncreaseQueue is null!!!");
#ifdef DEBUG
		for (size_t i = 0; i < emailAccounts.size(); i++) {
			pmm::Log << "MailSuckerThread: Starting monitoring of " << emailAccounts[i].email() << pmm::NL;
		}
#endif
		while (true) {
			time_t currTime = time(0);
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				QuotaIncreasePetition p;
				if(quotaIncreaseQueue->extractEntry(p)){
					//Verify if we should upgrade quotas on an account
					if (emailAccounts[i].email().compare(p.emailAddress) == 0) {
						emailAccounts.beginCriticalSection();
						emailAccounts.atUnlocked(i).quota = p.quotaValue;
						emailAccounts.atUnlocked(i).isEnabled = true;
						std::stringstream incNotif;
						incNotif << "We have incremented your notification quota by " << p.quotaValue << ".\nThanks for showing us some love!";
						for (size_t npi = 0; npi < emailAccounts.atUnlocked(i).devTokens().size(); npi++) {
							NotificationPayload np(emailAccounts.atUnlocked(i).devTokens()[npi], incNotif.str());
							np.isSystemNotification = true;
							notificationQueue->add(np);
						}
						emailAccounts.endCriticalSection();
					}
					else quotaIncreaseQueue->add(p);
				}
				if (emailAccounts[i].isEnabled == true) {
					if (emailAccounts[i].quota <= 0) {
						emailAccounts[i].isEnabled = false;
						continue;
					}
					MailboxControl mCtrl = mailboxControl[emailAccounts[i].email()];
					if(mCtrl.email.size() == 0){
						//Initial object creation
						mCtrl.email = emailAccounts[i].email();
						mailboxControl[emailAccounts[i].email()].email = emailAccounts[i].email();
					}
					//Maximum time the mailbox connection can be opened, if reched then we close the connection and force a new one.
					if (maxOpenTime > 0 && currTime - mailboxControl[emailAccounts[i].email()].openedOn > maxOpenTime) closeConnection(emailAccounts[i]);
					if (mCtrl.isOpened == false) openConnection(emailAccounts[i]);
					if (mCtrl.lastCheck + minimumMailCheckInterval < time(0)) {
						checkEmail(emailAccounts[i]);
						mailboxControl[emailAccounts[i].email()].lastCheck = time(0);
					}
				}
			}
			usleep(iterationWaitMicroSeconds);
		}
	}
	
	void MailSuckerThread::closeConnection(const MailAccountInfo &m){
		throw GenericException("Method MailSuckerThread::closeConnection is not implemented.");
	}
	
	void MailSuckerThread::openConnection(const MailAccountInfo &m){
		throw GenericException("Method MailSuckerThread::openConnection is not implemented.");
	}

	void MailSuckerThread::checkEmail(const MailAccountInfo &m){
		throw GenericException("Method MailSuckerThread::checkEmail is not implemented.");		
	}
}
