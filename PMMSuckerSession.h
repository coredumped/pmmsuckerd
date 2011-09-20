//
//  PMMSuckerSession.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/18/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_PMMSuckerSession_h
#define PMM_Sucker_PMMSuckerSession_h
#include<string>

namespace pmm {
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
	class SuckerSession {
	private:
		std::string pmmServiceURL;
		std::string myID;
		std::string apiKey;
	protected:
	public:
		SuckerSession();
		SuckerSession(const std::string &srvURL);
		
		bool register2PMM();
		//void register2PMMAsync();
	};
}


#endif
