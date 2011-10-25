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
		std::string myID;
		std::string apiKey;
		time_t expirationTime;
		Mutex timeM;
	protected:
		void performAutoRegister();
	public:
		SuckerSession();
		SuckerSession(const std::string &srvURL);
		
		//Register to the PMM Service server, currently located at Google AppEngine
		bool register2PMM();
		
		//Un-registers from the controller succesfully relinquishing ownership on any monitored mailboxes
		void unregisterFromPMM();
		
		//Asks for membership in order to join the global PMM Service server cluster
		bool reqMembership(const std::string &petition, const std::string &contactEmail = "");

		//Retrieves configured e-mail accounts located at the remote server so we can poll them later
		void retrieveEmailAddresses(std::vector<MailAccountInfo> &emailAddresses, bool performDelta = false);

		//Report quota changes to mailboxes
		bool reportQuotas(std::map<std::string, int> &quotas);
		
		//Get a list of commands to perform as requested by the controller
		int getPendingTasks(std::vector< std::map<std::string, std::map<std::string, std::string> > > &tasksToRun);
		
		//Upload notification messages to server
		void uploadNotificationMessage(const NotificationPayload &np);
	};
	
	namespace Commands {
		extern const char *quotaExceeded;
		extern const char *shutdown;
		extern const char *accountPropertyChanged;
	}
}


#endif
