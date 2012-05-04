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
#include "QuotaDB.h"
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
#define DEFAULT_MINIMUM_MAIL_CHECK_INTERVAL 2
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
		threadStartTime = time(0) - 900;
		cntRetrievedMessages = 0;
	}
	
	MailSuckerThread::~MailSuckerThread(){
		
	}
	
	void MailSuckerThread::initialize(){
		
	}
	
	void MailSuckerThread::processAccountAdd(){
		MailAccountInfo m;
		while (addAccountQueue->extractEntry(m)) {
			emailAccounts.push_back(m);
			std::stringstream msg;
			msg << "Monitoring of " + m.email() + " has been enabled :-)";
			std::vector<std::string> myDevTokens = m.devTokens();
			for (size_t i = 0; i < myDevTokens.size(); i++) {
				pmm::NotificationPayload np(myDevTokens[i], msg.str());
				np.isSystemNotification = true;
				if (m.devel) {
					develNotificationQueue->add(np);
				}
				else notificationQueue->add(np);
			}
			cntAccountsTotal = cntAccountsTotal + 1;
			usleep(10000);
		}
	}
	
	void MailSuckerThread::processAccountRemove(){
		if (rmAccountQueue->size() > 0) {
			//Find element
			std::string m;
			rmAccountQueue->extractEntry(m);
			bool found = false;
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				if (m.compare(emailAccounts[i].email()) == 0) {
					emailAccounts.erase(i);
					found = true;
					fetchedMails.removeAllEntriesOfEmail2(m);
#warning TODO: Remove all preferences for this account
					QuotaDB::removeAccount(m);
					cntAccountsTotal = cntAccountsTotal - 1;
					break;
				}
			}
			if(!found) rmAccountQueue->add(m);
			usleep(500);
		}
	}
	
	void MailSuckerThread::registerDeviceTokens(){
		DevtokenQueueItem item;
		time_t t1 = time(0);
		while (devTokenAddQueue->extractEntry(item)) {
			bool found = false;
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				if (item.email.compare(emailAccounts[i].email()) == 0) {
					pmm::Log << "Adding device " << item.devToken << " as recipient for messages of: " << item.email << pmm::NL;
					emailAccounts.atUnlocked(i).deviceTokenAdd(item.devToken);
					found = true;
					usleep(500); //Give another MailSuckerThread a chance to find this guy
					break;
				}
			}
			if (!found) devTokenAddQueue->add(item);
			if (time(0) - t1 > 0) break; //Only inspect the device token registration queue for 1 second.
		}
	}
	
	void MailSuckerThread::relinquishDeviceTokens(){
		DevtokenQueueItem item;
		time_t t1 = time(0);
		while (devTokenRelinquishQueue->extractEntry(item)) {
			bool found = false;
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				//Verify that this account has that devToken
				for (size_t j = 0; j < emailAccounts[i].devTokens().size(); j++) {
					if (item.devToken.compare(emailAccounts[i].devTokens()[j]) == 0) {
						pmm::Log << item.email << " will no longer receive notifications on device " << item.devToken << pmm::NL;
						emailAccounts.atUnlocked(i).deviceTokenRemove(item.devToken);
						found = true;
						usleep(500); //Give another MailSuckerThread a chance to find this guy
						break;
					}
				}
			}
			if (!found) devTokenRelinquishQueue->add(item);
			if (time(0) - t1 > 0) break; //Only inspect the device token registration queue for 1 second.
		}		
	}

	void MailSuckerThread::operator()(){
		if (notificationQueue == NULL) throw GenericException("notificationQueue is still NULL, it must point to a valid notification queue.");
		if (quotaUpdateVector == NULL) throw GenericException("quotaUpdateQueue is still NULL, it must point to a valid notification queue.");
		if (pmmStorageQueue == NULL) throw GenericException("Can't continue like this, the pmmStorageQueue is null!!!");
		if (quotaIncreaseQueue == NULL) throw GenericException("Can't continue like this, the quotaIncreaseQueue is null!!!");
		if (addAccountQueue == NULL) throw GenericException("Can't continue like this, the addAccountQueue is null!!!");
		if (rmAccountQueue == NULL) throw GenericException("Can't continue like this, the rmAccountQueue is null!!!");
		if (devTokenAddQueue == NULL) throw GenericException("Can't continue like this, the devTokenAddQueue is null!!!");
		if (devTokenRelinquishQueue == NULL) throw GenericException("Can't continue like this, the devTokenRelinquishQueue is null!!!");
		for (size_t i = 0; i < emailAccounts.size(); i++) {
			pmm::Log << "MailSuckerThread: Starting monitoring of " << emailAccounts[i].email() << pmm::NL;
			if(emailAccounts[i].quota > 0) cntAccountsActive = cntAccountsActive + 1;
		}
		cntAccountsTotal = emailAccounts.size();
		initialize();
		while (true) {
			//Process account addition and removals if there is any
			registerDeviceTokens();
			processAccountAdd();
			relinquishDeviceTokens();
			processAccountRemove();
			time_t currTime = time(0);
			if(emailAccounts.size() == 0 && currTime % 60 == 0) pmm::Log << "There are no e-mail accounts to monitor" << pmm::NL;
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				time_t rightNow = time(0);
				QuotaIncreasePetition p;
				if(quotaIncreaseQueue->extractEntry(p)){
					//Verify if we should upgrade quotas on an account
					if (emailAccounts[i].email().compare(p.emailAddress) == 0) {
						QuotaDB::set(p.emailAddress, p.quotaValue);
						emailAccounts.beginCriticalSection();
						emailAccounts.atUnlocked(i).quota = p.quotaValue;
						emailAccounts.atUnlocked(i).isEnabled = true;
						emailAccounts.endCriticalSection();
						pmm::Log << "Notifying quota increase of " << p.quotaValue << " to " << p.emailAddress << pmm::NL;
						std::stringstream incNotif;
						incNotif << "We have incremented your notification quota on " << p.emailAddress << " by " << p.quotaValue << ".\nThanks for showing us some love!";
						std::vector<std::string> myDevTokens = emailAccounts[i].devTokens();
						for (size_t npi = 0; npi < myDevTokens.size(); npi++) {
							NotificationPayload np(myDevTokens[npi], incNotif.str());
							np.isSystemNotification = true;
							if (emailAccounts[i].devel) {
								develNotificationQueue->add(np);
							}
							else notificationQueue->add(np);
						}
						
					}
					else quotaIncreaseQueue->add(p);
				}
				if (emailAccounts[i].devTokens().size() > 0) {
					//Monitor mailboxes only if they have at least one associated device
					if (emails2Disable.contains(emailAccounts[i].email())) {
						emailAccounts[i].isEnabled = false;
						emails2Disable.erase(emailAccounts[i].email());
						cntAccountsTotal = cntAccountsTotal - 1;
					}
					if (emailAccounts[i].isEnabled == true) {
						if (QuotaDB::remaning(emailAccounts[i].email()) <= 0) {
							emailAccounts[i].isEnabled = false;
							continue;
						}
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
						if (rightNow - mCtrl.lastCheck >= minimumMailCheckInterval) {
							checkEmail(emailAccounts[i]);
							mailboxControl[emailAccounts[i].email()].lastCheck = rightNow;
						}
					}
#ifdef DEBUG
					else {
						if(currTime % 600 == 0) pmm::Log << emailAccounts[i].email() << " is not enabled, ignoring..." << pmm::NL;
					}
#endif
				}
#ifdef DEBUG
				else {
					if(currTime % 600 == 0) pmm::Log << emailAccounts[i].email() << " has no registered devtokens, ignoring..." << pmm::NL;
				}
#endif
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
