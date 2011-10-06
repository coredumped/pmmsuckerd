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
	
	static void msg_encode(std::string &theMsg){
		std::string newString;
		for (size_t i = 0; i < theMsg.size(); i++) {
			if (theMsg[i] == '\n') {
				newString.append("\\n");
			}
			else if (theMsg[i] == '\r') {
				newString.append("\\r");
			}
			else if (theMsg[i] == '\t') {
				newString.append("\\t");
			}
			else if (theMsg[i] == '\"') {
				newString.append("\\\"");
			}
			else if (theMsg[i] == '\\') {
				newString.append("\\\\");
			}
			else if (theMsg[i] == '\'') {
				newString.append("\\'");
			}
			else {
				char tbuf[2] = { theMsg[i], 0x00};
				newString.append(tbuf);
			}
		}
		if (newString.size() > theMsg.size()) {
			theMsg = newString;
		}
	}

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
		msg_encode(encodedMsg);
#warning TODO: Remember to compute the icon badge before any notification
		jsonbuilder << "{";
		jsonbuilder << "\"aps\":";
		jsonbuilder << "{";
		jsonbuilder << "\"alert\":\"" << encodedMsg << "\",";
		jsonbuilder << "\"sound\":\"" << _soundName << "\",";
		jsonbuilder << "\"badge\":1,";
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
