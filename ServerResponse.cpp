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
	
	MetaDataInfo::MetaDataInfo(){
		param = "";
		value = "";
	}
	
	MetaDataInfo::MetaDataInfo(const std::string &_param, const std::string &_value){
		param = _param;
		value = _value;
	}
	
	ServerResponse::ServerResponse(){
		status = true;
		errorCode = PMM_OK;
	}
	
	ServerResponse::ServerResponse(const std::string &fromJson){
		std::istringstream input(fromJson);
		jsonxx::Object obj;
		jsonxx::Object::parse(input, obj);
		if (obj.has<bool>(statusMember)) {
			if (obj.get<bool>(statusMember) == true) {
				status = true;
				errorCode = PMM_OK;
			}
			else {
				
			}
			if (obj.has<jsonxx::Array>(metaDataMember)) {
				for(unsigned int i = 0; i < obj.get<jsonxx::Array>(metaDataMember).size(); i++){
					for (std::map<std::string, jsonxx::Value *>::const_iterator iter = obj.get<jsonxx::Array>(metaDataMember).get<jsonxx::Object>(i).kv_map().begin(); iter != obj.get<jsonxx::Array>(metaDataMember).get<jsonxx::Object>(i).kv_map().end(); iter++) {
						MetaDataInfo meta;
						meta.param = iter->first;
						meta.value = iter->second->get<std::string>();
					}
					
				}
			}
			if (obj.has<std::string>(errorDescriptionMember)) {
				errorDescription = obj.get<std::string>(errorDescriptionMember);
			}
		}
		else {
			//Throw exception reporting a malformed ServerResponse stream
			throw JSONParseException("Unable to parser server response, at least the \"status\" field must be present");
		}
	}

}