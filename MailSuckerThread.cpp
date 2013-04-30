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
#include <signal.h>
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
	pmm::AtomicFlag mailboxPollBlocked = false;
	
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
		monitoringStartedOn = openedOn;
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
		monitoringStartedOn = m.monitoringStartedOn;
	}
	
	MailboxControl &MailboxControl::operator=(const MailboxControl &m){
		openedOn = m.openedOn;
		isOpened = m.isOpened;
		email = m.email;
		availableMessages = m.availableMessages;
		lastCheck = m.lastCheck;
		monitoringStartedOn = m.monitoringStartedOn;
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
		cntFailedLoginAttempts = 0;
		mailAccounts2Refresh = NULL;
		if(emails2Disable.contains("none")) pmm::Log << "Initialization of emails2Disable SharedSet is flawed!!!" << pmm::NL;
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
			if(m.mailboxType().find("POP") == 0){
				msg << "Monitoring of " + m.email() + " has been enabled, wait a few minutes while message indexing takes place :-)";
			}
			else msg << "Monitoring of " + m.email() + " has been enabled, you'll start receiving e-mail notifications shortly :-)";
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
			usleep(500);
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
					QuotaDB::removeAccount(m);
					cntAccountsTotal = cntAccountsTotal - 1;
					break;
				}
			}
			if(!found) rmAccountQueue->add(m);
			usleep(500);
		}
	}
	
	bool MailSuckerThread::processAccountUpdate(const MailAccountInfo &m, size_t idx){
		bool somethingChanged = false;
		mailAccounts2Refresh->beginCriticalSection();
		for (size_t i = 0; i < mailAccounts2Refresh->unlockedSize(); i++) {
			if (m.email().compare(mailAccounts2Refresh->atUnlocked(i).email()) == 0) {
				pmm::Log << "Initiating email account update..." << pmm::NL;
				emailAccounts.unlockedErase(idx);
				pmm::Log << "Adding new information for " << m.email() << "..." << pmm::NL;
				emailAccounts.push_back(mailAccounts2Refresh->atUnlocked(i));
				mailAccounts2Refresh->unlockedErase(i);
				pmm::Log << "Information of " << m.email() << " updated succesfully!!!" << pmm::NL;
				nextConnectAttempt.erase(m.email());
				serverConnectAttempts.erase(m.email());
				somethingChanged = true;
				break;
			}
		}
		mailAccounts2Refresh->endCriticalSection();
		return somethingChanged;
	}
	
	void MailSuckerThread::reportFailedLogins(){
		FailedLoginItem email2Report;
		while (emailsFailingLoginsQ.extractEntry(email2Report)) {
			std::vector<std::string> myDevTokens = email2Report.m.devTokens();
			for (size_t i = 0; i < myDevTokens.size(); i++) {
				NotificationPayload np(myDevTokens[i], email2Report.errmsg);
				np.isSystemNotification = true;
				if (email2Report.m.devel) {
					develNotificationQueue->add(np);
				}
				else notificationQueue->add(np);
				std::stringstream errUid;
				errUid << "-pmm-err-" << email2Report.tstamp;
				MailMessage msg;
				msg.fromEmail = "support@fnxsoftware.com";
				msg.dateOfArrival = email2Report.tstamp;
				msg.msgUid = errUid.str();
				msg.serverDate = email2Report.tstamp;
				msg.to = email2Report.m.email();
				msg.from = "PushMeMail";
				msg.subject = email2Report.errmsg;
				np.origMailMessage = msg;
				pmmStorageQueue->add(np);
				if (email2Report.gmailAuthReq) {
					//Send additional e-mail notification for the user here!!!
					gmailAuthRequestedQ->add(email2Report.m.email());
				}
			}
		}
	}
	
	void MailSuckerThread::scheduleFailureReport(const MailAccountInfo &m, const std::string &errmsg, bool isGmailAuthReq){
		FailedLoginItem fItem;
		fItem.errmsg = errmsg;
		fItem.m = m;
		fItem.tstamp = time(0);
		fItem.gmailAuthReq = isGmailAuthReq;
		emailsFailingLoginsQ.add(fItem);
	}
	
	void MailSuckerThread::registerDeviceTokens(){
		DevtokenQueueItem item;
		while (devTokenAddQueue.extractEntry(item)) {
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
		}
	}
	
	void MailSuckerThread::relinquishDeviceTokens(){
		DevtokenQueueItem item;
		while (devTokenRelinquishQueue.extractEntry(item)) {
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				//Verify that this account has that devToken
				std::vector<std::string> tokList = emailAccounts[i].devTokens();
				std::string theEmail = emailAccounts[i].email();
				for (size_t j = 0; j < tokList.size(); j++) {
					if (item.devToken.compare(tokList[j]) == 0) {
						pmm::Log << "INFO: " << theEmail << " will no longer receive notifications on device " << item.devToken << pmm::NL;
						emailAccounts.atUnlocked(i).deviceTokenRemove(item.devToken);
					}
				}
			}
		}
	}
	
	void MailSuckerThread::operator()(){
		if (notificationQueue == NULL) throw GenericException("notificationQueue is still NULL, it must point to a valid notification queue.");
		if (quotaUpdateVector == NULL) throw GenericException("quotaUpdateQueue is still NULL, it must point to a valid notification queue.");
		if (pmmStorageQueue == NULL) throw GenericException("Can't continue like this, the pmmStorageQueue is null!!!");
		if (quotaIncreaseQueue == NULL) throw GenericException("Can't continue like this, the quotaIncreaseQueue is null!!!");
		if (addAccountQueue == NULL) throw GenericException("Can't continue like this, the addAccountQueue is null!!!");
		if (rmAccountQueue == NULL) throw GenericException("Can't continue like this, the rmAccountQueue is null!!!");
		//if (devTokenAddQueue == NULL) throw GenericException("Can't continue like this, the devTokenAddQueue is null!!!");
		//		if (devTokenRelinquishQueue == NULL) throw GenericException("Can't continue like this, the devTokenRelinquishQueue is null!!!");
		if (mailAccounts2Refresh == NULL) throw GenericException("Can't continue with an null mailAccount2Refresh vector");
		for (size_t i = 0; i < emailAccounts.size(); i++) {
			pmm::Log << "MailSuckerThread: Starting monitoring of " << emailAccounts[i].email() << pmm::NL;
			if(emailAccounts[i].quota > 0) cntAccountsActive = cntAccountsActive + 1;
		}
		cntAccountsTotal = (int)emailAccounts.size();
		initialize();
		
		sigset_t bSignal;
		sigemptyset(&bSignal);
		sigaddset(&bSignal, SIGPIPE);
		if(pthread_sigmask(SIG_BLOCK, &bSignal, NULL) != 0){
			pmm::Log << "WARNING: Unable to block SIGPIPE !" << pmm::NL;
		}
		
		while (true) {
			//Process account addition and removals if there is any
			reportFailedLogins();
			registerDeviceTokens();
			processAccountAdd();
			relinquishDeviceTokens();
			processAccountRemove();
			time_t currTime = time(0);
			while (mailboxPollBlocked == true) {
				time_t nowx = time(0);
				if (nowx % 60 == 0) {
					pmm::Log << "Polling operations are blocked." << pmm::NL;
				}
				if (nowx - currTime > 300) {
					mailboxPollBlocked = false;
					pmm::Log << "Unblocking polling operations, the timeout has been exceeded" << pmm::NL;
					break;
				}
				sleep(2);
			}
			currTime = time(0);
			if(emailAccounts.size() == 0 && currTime % 60 == 0) pmm::Log << "There are no e-mail accounts to monitor" << pmm::NL;
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				if(nextConnectAttempt.find(emailAccounts[i].email()) == nextConnectAttempt.end()) nextConnectAttempt[emailAccounts[i].email()] = 0;
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
						incNotif << "We have increased your notification quota on " << p.emailAddress << " by " << p.quotaValue << ".\nThanks for showing us some love!";
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
					else{
						if (rightNow - p.creationTime < 1800) {
							quotaIncreaseQueue->add(p);
						}
						else {
							pmm::Log << "PANIC: Expiring quota increase petition to " << p.emailAddress << " because it is too old!!!" << pmm::NL;
						}
					}
				}
				processAccountUpdate(emailAccounts[i], i);
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
							time_t t0 = time(0);
							checkEmail(emailAccounts[i]);
							mailboxControl[emailAccounts[i].email()].lastCheck = rightNow;
							time_t tx = time(0);
							int tdiff = (int)(tx - t0);
							if (tdiff > 30) {
								pmm::Log << "Check mail interval to server " << emailAccounts[i].serverAddress() << " for account " << emailAccounts[i].email() << " is taking at least " << tdiff << " secs to complete, what is going wrong?" << pmm::NL;
							}
						}
					}
#ifdef DEBUG_ENABLED_EMAIL_ACCOUNTS
					else {
						if(currTime % 600 == 0) pmm::Log << emailAccounts[i].email() << " is not enabled, ignoring..." << pmm::NL;
					}
#endif
				}
				else {
					if(currTime % 600 == 0) pmm::Log << emailAccounts[i].email() << " has no registered devtokens, ignoring..." << pmm::NL;
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
