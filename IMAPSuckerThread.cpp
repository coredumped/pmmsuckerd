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
				/*result = mailimap_idle(imapControl[m.email()].imap);
				if(etpanOperationFailed(result)){
					throw GenericException("Unable to start IDLE!!!");
				}*/
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
		if (mailboxControl[m.email()].isOpened) {
			mailimap *imap = imapControl[m.email()].imap;
#ifdef __linux__
			
			mailstream_low *mlow = mailstream_get_low(imap->imap_stream);
			int fd = mailstream_low_get_fd(mlow);
			//int fd = mailimap_idle_get_fd(imapControl[m.email()].imap);
			struct pollfd pelem;
			pelem.fd = fd;
			pelem.events = POLLIN | POLLOUT;
			int result = poll(&pelem, 1, 1);
#ifdef DEBUG
			if(result > 0){
				mout.lock();
				std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " POLL result is: " << std::hex << pelem.revents << std::endl;
				mout.unlock();
			}
#endif

#else
			
/*			int result = mailimap_idle_done(imapControl[m.email()].imap);
#ifdef DEBUG
			if(result != MAILIMAP_NO_ERROR){
			mout.lock();
			std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " IDLE result is: " << result << std::endl;
			mout.unlock();
			}
#endif			
*/
						
			mailimap_noop(imap);
			struct mailimap_status_att_list *inattlist = mailimap_status_att_list_new_empty();
			mailimap_status_att_list_add(inattlist, MAILIMAP_STATUS_ATT_RECENT);
			struct mailimap_mailbox_data_status *mStatus = 0x00;
			int result = mailimap_status(imap, "INBOX", inattlist, &mStatus);
			if(result == MAILIMAP_NO_ERROR){
#ifdef DEBUG
				clistiter *citer = clist_begin(mStatus->st_info_list);
				mailimap_status_info * nx = (mailimap_status_info *)clist_content(citer);
				if (nx->st_value > 0) {
					mout.lock();
					std::cerr << "IMAPSuckerThread(" << (long)pthread_self() << "): " << m.email() << " recent messages in " << mStatus->st_mailbox << ": " << nx->st_value << " clist has " << clist_count(mStatus->st_info_list) << std::endl; 
					mout.unlock();
				}
#endif
				mailimap_mailbox_data_status_free(mStatus);
			}
#endif
		}
		else {
			openConnection(m);
		}
	}

}