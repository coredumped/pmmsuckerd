//
//  NotificationPayload.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/29/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//
#include "NotificationPayload.h"
#include "UtilityFunctions.h"
#include <iostream>
#include <sstream>

namespace pmm {
	NotificationPayload::NotificationPayload(){
		
	}
	
	NotificationPayload::NotificationPayload(const std::string &devToken_, const std::string &_message, const std::string &sndName){
		msg = _message;
		_soundName = sndName;
		devToken = devToken_;
	}
	
	NotificationPayload::NotificationPayload(const NotificationPayload &n){
		msg = n.msg;
		_soundName = n._soundName;
		devToken = n.devToken;
	}
	
	std::string NotificationPayload::toJSON(){
		std::stringstream jsonbuilder;
		std::string encodedMsg = msg;
		url_encode(encodedMsg);
		jsonbuilder << "{";
		jsonbuilder << "\"aps\":";
		jsonbuilder << "{";
		jsonbuilder << "\"alert\":\"" << encodedMsg << "\",";
		jsonbuilder << "\"sound\":\"" << _soundName << "\"";
		jsonbuilder << "}";
		jsonbuilder << "}";
		return std::string(jsonbuilder.str());
	}
	
	std::string &NotificationPayload::soundName(){
		return _soundName;
	}
	
	std::string &NotificationPayload::message(){
		return msg;
	}

	std::string &NotificationPayload::deviceToken(){
		return devToken;
	}
}
