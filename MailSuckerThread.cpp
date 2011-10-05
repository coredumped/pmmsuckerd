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

#ifndef DEFAULT_WAIT_TIME_BETWEEN_MAIL_CHECKS
#define DEFAULT_WAIT_TIME_BETWEEN_MAIL_CHECKS 250000
#endif

#ifndef DEFAULT_MAX_OPEN_TIME
#define DEFAULT_MAX_OPEN_TIME -1
#endif

#ifndef DEFAULT_MAX_SERVER_CONNECT_FAILURES
#define DEFAULT_MAX_SERVER_CONNECT_FAILURES 10 
#endif

namespace pmm {
	
	MailboxControl::MailboxControl(){
		openedOn = time(0);
		isOpened = false;
	}
	
	MailboxControl::MailboxControl(const MailboxControl &m){
		openedOn = m.openedOn;
		isOpened = m.isOpened;
		email = m.email;
	}
	
	MailSuckerThread::MailSuckerThread(){
		iterationWaitMicroSeconds = DEFAULT_WAIT_TIME_BETWEEN_MAIL_CHECKS;
		maxOpenTime = DEFAULT_MAX_OPEN_TIME;
		notificationQueue = NULL;
		maxServerReconnects = DEFAULT_MAX_SERVER_CONNECT_FAILURES;
	}
	
	MailSuckerThread::~MailSuckerThread(){
		
	}

	void MailSuckerThread::operator()(){
		if (notificationQueue == NULL) {
			throw GenericException("notificationQueue is still NULL, it must point to a valid notification queue.");
		}
		while (true) {
			time_t currTime = time(0);
			for (size_t i = 0; i < emailAccounts.size(); i++) {
				MailboxControl mCtrl = mailboxControl[emailAccounts[i].email()];
				//Maximum time the mailbox connection can be opened, if reched then we close the connection and force a new one.
				if (maxOpenTime > 0 && currTime - mCtrl.openedOn > maxOpenTime) closeConnection(emailAccounts[i]);
				if (mCtrl.isOpened == false) openConnection(emailAccounts[i]);
				checkEmail(emailAccounts[i]);
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
