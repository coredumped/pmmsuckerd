//
//  ServerResponse.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/21/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_ServerResponse_h
#define PMM_Sucker_ServerResponse_h
#include<string>
#include<vector>
#include<map>
#include"JSONParseException.h"

namespace pmm {
	
	typedef enum  {
		PMM_OK = 0,
		PMM_ERROR_AT_SERVER_SQL = 2,
		PMM_ERROR_NO_PAYLOAD = 666,
		PMM_ERROR_SERVER_SCRIPT = 667,
		PMM_ERROR_AT_CONNECTION = 1000,
		PMM_ERROR_IN_SERVER_QUERY,
		PMM_ERROR_IN_HTTP_LAYER,
		PMM_ERROR_JSON_DESERIALIZATION,
		PMM_ERROR_ACCOUNT_RETRIEVAL,
		PMM_ERROR_ACCOUNT_ADD,
		PMM_ERROR_ACCOUNT_REMOVE,
		PMM_ERROR_ACCOUNT_MODIFY,
		
		PMM_ERROR_LOGIN_FAILED = 1008,
		PMM_ERROR_INTERNAL_CRYPTOGRAPHY = 1009,
		PMM_ERROR_SESSION_TIMEOUT = 1010,
		PMM_ERROR_API_KEY_IS_INVALID = 1011,
		
		PMM_ERROR_USER_ACCOUNT_REMOVAL_FAILED = 1012,
		PMM_ERROR_OCCURRED = 1013,
		PMM_ERROR_USER_ACCOUNT_ALREADY_EXISTS = 1014,
		PMM_ERROR_SUCKER_DENIED = 1015,
		PMM_ERROR_SUCKER_MEMBERSHIP_REQUIRED = 1016,
		PMM_ERROR_OPTYPE_INVALID = 2000
	} PMMResponseErrorCodes; //Always keep this enumerator in synch with the one at PMMResponseErrorCodes.h
	
	typedef std::map<std::string, std::string>::iterator MetaDataItemInterator;
	
	class ServerResponse {
	public:
		bool status;
		long errorCode;
		std::string errorDescription;
		std::map<std::string,std::string> metaData;
		std::string operationType;
		ServerResponse();
		ServerResponse(const std::string &fromJson);
	};
	
	class ServerResponseException : public ServerResponse {
		public:
		ServerResponseException() : ServerResponse(){ }
		ServerResponseException(const std::string &fromJson) : ServerResponse(fromJson){ }
	};
}


#endif
