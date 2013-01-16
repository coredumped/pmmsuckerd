//
//  PMMSuckerSession.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/18/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_PMMSuckerSession_h
#define PMM_Sucker_PMMSuckerSession_h
#ifndef DEFAULT_PMM_SERVICE_URL
#define DEFAULT_PMM_SERVICE_URL "https://pmmserver.appspot.com/pmmsuckerd"
///#define DEFAULT_PMM_SERVICE_URL "http://localhost:8888/pmmsuckerd"
#endif
#include<string>
#include "MailAccountInfo.h"
#include "NotificationPayload.h"
#include "Mutex.h"
#include "SharedQueue.h"

namespace pmm {
	/** Exception thrown whenever we received an error response from a remote server */
	class HTTPException {
		private:
		int code;
		std::string msg;
		public:
		HTTPException(){ }
		HTTPException(int _code, const std::string &_msg){ 
			code = _code;
			msg = _msg;
		}
		int errorCode(){ return code; }
		std::string errorMessage(){ return std::string(msg); }
	};
	
	/** Use is to communicate with the remote PMM Service server */
	class SuckerSession {
	private:
		std::string pmmServiceURL;
#ifndef CHEAT_SESSION_MANAGER
		std::string myID;
#endif
		std::string apiKey;
		time_t expirationTime;
		Mutex timeM;
	protected:

	public:
#ifdef CHEAT_SESSION_MANAGER
		std::string myID;
#endif
		bool dummyMode;
		SuckerSession();
		SuckerSession(const std::string &srvURL);

		
		//Register to the PMM Service server, currently located at Google AppEngine
		bool register2PMM();
		
		//Performs automatic re-registration is for some reason we're running out of time :-)
		void performAutoRegister();
		
		//Un-registers from the controller succesfully relinquishing ownership on any monitored mailboxes
		void unregisterFromPMM();
		
		//Asks for membership in order to join the global PMM Service server cluster
		bool reqMembership(const std::string &petition, const std::string &contactEmail = "");

		//Retrieves configured e-mail accounts located at the remote server so we can poll them later
		// @parameter emailAddresses has a vector of addresses to be polled by this pmmSucker
		// @parameter specificEmail is to be used as a filter, if given only information related to that email will be retrieved
		// @parameter performDelta brings all available e-mail accounts that do not have an associated sucker
		void retrieveEmailAddresses(std::vector<MailAccountInfo> &emailAddresses, const std::string &specificEmail = "", bool performDelta = false);
		
		// Retrieves information about an specific e-mail address, this address will automatically be assigned to this sucker
		bool retrieveEmailAddressInfo(MailAccountInfo &m, const std::string &emailAddress);

		//Report quota changes to mailboxes
		bool reportQuotas(std::map<std::string, int> &quotas);
		
		//Get a list of commands to perform as requested by the controller
		int getPendingTasks(std::vector< std::map<std::string, std::map<std::string, std::string> > > &tasksToRun);
		
		//Upload notification messages to server
		void uploadNotificationMessage(const NotificationPayload &np);
		void uploadMultipleNotificationMessages(const std::vector<NotificationPayload> &msgs);
		int uploadMultipleNotificationMessages(pmm::SharedQueue<NotificationPayload> &msgs);
		int uploadMultipleNotificationMessages(pmm::SharedQueue<NotificationPayload> *msgs);
		
		//Checks if a new task has been registered in fnxsoftware.com
		bool fnxHasPendingTasks();
		
		//Retrieves silent mode information or the given accounts
		bool silentModeInfoGet(std::map<std::string, std::map<std::string, int> > &_return, const std::vector<std::string> &emailAccounts);
		bool silentModeInfoGet(std::map<std::string, std::map<std::string, int> > &_return, const std::string &emailAccounts);
		
		//Invalid token handling
		bool reportInvalidDeviceToken(const std::vector<std::string> &tokVec);
		
		//Sends stat information to the fnx server
		void putStat(const std::string &var, double val);
		void putStatMultiple(std::map<std::string, double> &dataMap);
		
		void notifyGmailAdditionalAuth(const std::string &emailAccount, const std::string &languageCode);
	};
	
	namespace Commands {
		extern const char *quotaExceeded;
		extern const char *shutdown;
		extern const char *accountPropertyChanged;
		extern const char *mailAccountQuotaChanged;
		extern const char *newMailAccountRegistered;
		extern const char *relinquishDevToken;
		extern const char *deleteEmailAccount;
		extern const char *refreshDeviceTokenList;
		extern const char *silentModeSet;
		extern const char *silentModeClear;
		extern const char *refreshUserPreference;
		extern const char *broadcastMessage;
		extern const char *refreshEmailAccount;
		extern const char *sendMessageToDevice;
		extern const char *sendNoQuotaMessageToDevice;
	}
}


#endif
