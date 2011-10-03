//
//  IMAPSuckerThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 10/1/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <map>
#include <time.h>
#include <sstream>
#include "IMAPSuckerThread.h"
#include "libetpan/mailimap.h"

namespace pmm {
	
	static bool etpanOperationFailed(int r)
	{
		if (r == MAILIMAP_NO_ERROR)
			return false;
		if (r == MAILIMAP_NO_ERROR_AUTHENTICATED)
			return false;
		if (r == MAILIMAP_NO_ERROR_NON_AUTHENTICATED)
			return false;
		return true;
	}
	
	IMAPSuckerThread::IMAPControl::IMAPControl(){
		failedLoginAttemptsCount = 0;
		idling = false;
		imap = NULL;
	}
	
	IMAPSuckerThread::IMAPControl::~IMAPControl(){

	}
	
	IMAPSuckerThread::IMAPControl::IMAPControl(const IMAPControl &ic){
		failedLoginAttemptsCount = ic.failedLoginAttemptsCount;
		imap = ic.imap;
	}

	
	IMAPSuckerThread::IMAPSuckerThread(){
		
	}
	IMAPSuckerThread::~IMAPSuckerThread(){
		
	}

	void IMAPSuckerThread::closeConnection(const MailAccountInfo &m){
		if (mailboxControl[m.email()].isOpened) {
			struct mailimap *imap = imapControl[m.email()].imap;
			mailimap_close(imap);
			mailimap_free(imapControl[m.email()].imap);
			imapControl[m.email()].imap = NULL;
		}
		mailboxControl[m.email()].isOpened = false;
	}
	
	void IMAPSuckerThread::openConnection(const MailAccountInfo &m){
		int result;
		if(imapControl[m.email()].imap == NULL) imapControl[m.email()].imap = mailimap_new(0, NULL);
		if (serverConnectAttempts.find(m.serverAddress()) == serverConnectAttempts.end()) serverConnectAttempts[m.serverAddress()] = 0;

		if (m.useSSL()) {
			result = mailimap_ssl_connect(imapControl[m.email()].imap, m.serverAddress().c_str(), m.serverPort());
		}
		else {
			result = mailimap_socket_connect(imapControl[m.email()].imap, m.serverAddress().c_str(), m.serverPort());
		}
		if (etpanOperationFailed(result)) {
			serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
			if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
				//Max reconnect exceeded, notify user
				std::stringstream errmsg;
				errmsg << "Unable to connect to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped";
				for (size_t i = 0; m.devTokens().size(); i++) {
					NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str()));
					notificationQueue->add(msg);
				}
				serverConnectAttempts[m.serverAddress()] = 0;
#warning Add method for relinquishing email account monitoring
			}
			mailboxControl[m.email()].isOpened = false;
		}
		else {
			//Proceed to login stage
			result = mailimap_login(imapControl[m.email()].imap, m.username().c_str(), m.password().c_str());
			if(etpanOperationFailed(result)){
				serverConnectAttempts[m.serverAddress()] = serverConnectAttempts[m.serverAddress()] + 1;
				if (serverConnectAttempts[m.serverAddress()] > maxServerReconnects) {
					//Max reconnect exceeded, notify user
					std::stringstream errmsg;
					errmsg << "Unable to LOGIN to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped";
					for (size_t i = 0; m.devTokens().size(); i++) {
						NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str()));
						notificationQueue->add(msg);
					}
					serverConnectAttempts[m.serverAddress()] = 0;
#warning Add method for relinquishing email account monitoring
				}
				mailimap_free(imapControl[m.email()].imap);
				imapControl[m.email()].imap = NULL;
				mailboxControl[m.email()].isOpened = false;
			}
			else {
				//Report successfull login
				mailboxControl[m.email()].isOpened = true;
			}
		}
	}
	
	void IMAPSuckerThread::checkEmail(const MailAccountInfo &m){
		if (mailboxControl[m.email()].isOpened) {

		}
		else {
			//Connection is off-line, need to reconnect
		}
	}

}