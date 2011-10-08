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
#include <poll.h>
#include "IMAPSuckerThread.h"
#include "libetpan/libetpan.h"

namespace pmm {
	Mutex mout;
	
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
#warning TODO: Find a better way to notify the user that we're unable to connect into their mail server
				std::stringstream errmsg;
				errmsg << "Unable to connect to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped";
				for (size_t i = 0; m.devTokens().size(); i++) {
					NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str(), ++mailboxControl[m.email()].availableMessages));
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
#warning TODO: Find a better way to notify the user that we're unable to login into their mail account
					errmsg << "Unable to LOGIN to " << m.serverAddress() << " monitoring of " << m.email() << " has been stopped";
					for (size_t i = 0; m.devTokens().size(); i++) {
						NotificationPayload msg(NotificationPayload(m.devTokens()[i], errmsg.str(), ++mailboxControl[m.email()].availableMessages));
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
				//Start IMAP IDLE processing...
#ifdef DEBUG
				mout.lock();
				std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): Starting IMAP IDLE for " << m.email() << std::endl;
				mout.unlock();
#endif
				mailstream_debug = 1;
				result = mailimap_select(imapControl[m.email()].imap, "INBOX");
				if(etpanOperationFailed(result)){
					throw GenericException("Unable to select INBOX folder");
				}				
				if(!mailimap_has_idle(imapControl[m.email()].imap)){
					throw GenericException("Can't POLL non IDLE mailboxes.");
				}
				result = mailimap_idle(imapControl[m.email()].imap);
				if(etpanOperationFailed(result)){
					throw GenericException("Unable to start IDLE!!!");
				}
				//Report successfull login
				mailboxControl[m.email()].isOpened = true;
				mailboxControl[m.email()].openedOn = time(0x00);
				//sleep(1);
#ifdef DEBUG
				mout.lock();
				std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " is being succesfully monitored!!!" << std::endl;
				mout.unlock();
#endif
			}
		}
	}
	
	void IMAPSuckerThread::checkEmail(const MailAccountInfo &m){
		std::string theEmail = m.email();
		if (mailboxControl[theEmail].isOpened) {
			mailimap *imap = imapControl[m.email()].imap;
			mailstream_low *mlow = mailstream_get_low(imap->imap_stream);
			int fd = mailstream_low_get_fd(mlow);
			struct pollfd pelem;
			pelem.fd = fd;
			pelem.events = POLLIN;
			int recent = -1;
			while (poll(&pelem, 1, 2) > 0) {
				char *response = mailimap_read_line(imap);
				if (strstr(response, "RECENT") != NULL) {
					//Compute how many recent we have
					int recent;
					std::istringstream input(std::string(response).substr(2));
					input >> recent;
					mailboxControl[theEmail].availableMessages += recent;
				}
			}
			if (recent >= 0) {
				mailimap_idle_done(imap);
				mout.lock();
				std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " POLL result is: 0x" << std::hex << pelem.revents << " (recent=" << recent << ") data: " << response << std::endl;
				mout.unlock();
				std::stringstream msg;
				msg << "To: " << m.email() << ": " << response;
				for (size_t i = 0; i < m.devTokens().size(); i++) {
					NotificationPayload np(m.devTokens()[i], msg.str(), mailboxControl[theEmail].availableMessages);
					notificationQueue->add(np);
				}				
				mailimap_idle(imap);
			}
		}
		else {
			openConnection(m);
		}
	}

}