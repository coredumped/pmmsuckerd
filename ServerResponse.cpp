//
//  ServerResponse.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/21/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#include <iostream>
#include <sstream>
#include "ServerResponse.h"
#include "jsonxx.h"

namespace pmm {
	
	static std::string statusMember = "status";
	static std::string metaDataMember = "metaData";
	static std::string errorDescriptionMember = "errorDescription";
		
	ServerResponse::ServerResponse(){
		status = true;
		errorCode = PMM_OK;
	}
	
	ServerResponse::ServerResponse(const std::string &fromJson){
		std::istringstream input(fromJson);
		jsonxx::Object obj;
		jsonxx::Object::parse(input, obj);
		if (obj.has<bool>(statusMember)) {
			if (obj.has<jsonxx::Array>(metaDataMember)) {
				for(unsigned int i = 0; i < obj.get<jsonxx::Array>(metaDataMember).size(); i++){
					for (std::map<std::string, jsonxx::Value *>::const_iterator iter = obj.get<jsonxx::Array>(metaDataMember).get<jsonxx::Object>(i).kv_map().begin(); iter != obj.get<jsonxx::Array>(metaDataMember).get<jsonxx::Object>(i).kv_map().end(); iter++) {
						metaData[iter->first] = iter->second->get<std::string>();
#ifdef DEBUG
						std::cerr << "METADATA: " << iter->first << "=" << metaData[iter->first] << std::endl;
#endif
					}
					
				}
			}
			if (obj.has<std::string>(errorDescriptionMember)) {
				errorDescription = obj.get<std::string>(errorDescriptionMember);
			}
			operationType = obj.get<std::string>("operationType");
			if (obj.get<bool>(statusMember) == true) {
				status = true;
				errorCode = PMM_OK;
			}
			else {
				status = false;
				errorCode = obj.get<jsonxx::number>("errorCode");
				ServerResponseException e;
				e.status = status;
				e.errorCode = errorCode;
				e.metaData = metaData;
				e.errorDescription = errorDescription;
				e.operationType = operationType;
				throw e;
			}
		}
		else {
			//Throw exception reporting a malformed ServerResponse stream
			throw JSONParseException("Unable to parser server response, at least the \"status\" field must be present");
		}
	}

}