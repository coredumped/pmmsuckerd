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
#ifndef MAXPAYLOAD_SIZE
#define MAXPAYLOAD_SIZE 256
#endif

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
		_badgeNumber = 1;
	}
	
	NotificationPayload::NotificationPayload(const std::string &devToken_, const std::string &_message, int badgeNumber, const std::string &sndName){
		msg = _message;
		_soundName = sndName;
		devToken = devToken_;
		_badgeNumber = badgeNumber;
		build();
	}
	
	NotificationPayload::NotificationPayload(const NotificationPayload &n){
		msg = n.msg;
		_soundName = n._soundName;
		devToken = n.devToken;
		_badgeNumber = n._badgeNumber;
		build();
		origMailMessage = n.origMailMessage;
	}
	
	NotificationPayload::~NotificationPayload(){
		
	}
	
	void NotificationPayload::build(){
		std::stringstream jsonbuilder;
		std::string encodedMsg = msg;
		msg_encode(encodedMsg);
		jsonbuilder << "{";
		jsonbuilder << "\"aps\":";
		jsonbuilder << "{";
		jsonbuilder << "\"alert\":\"" << encodedMsg << "\",";
		jsonbuilder << "\"sound\":\"" << _soundName << "\",";
		if(_badgeNumber > 0) jsonbuilder << "\"badge\":" << _badgeNumber << ",";
		jsonbuilder << "}";
		jsonbuilder << "}";
		if (jsonbuilder.str().size() > MAXPAYLOAD_SIZE) {
			jsonbuilder.str(std::string());
			jsonbuilder << "{";
			jsonbuilder << "\"aps\":";
			jsonbuilder << "{";
			jsonbuilder << "\"sound\":\"" << _soundName << "\",";
			if(_badgeNumber > 0) jsonbuilder << "\"badge\":" << _badgeNumber << ",";
			jsonbuilder << "}";
			jsonbuilder << "}";
			
			size_t maxMsgLength = jsonbuilder.str().size() - 3;
			jsonbuilder.str(std::string());
			jsonbuilder << "{";
			jsonbuilder << "\"aps\":";
			jsonbuilder << "{";
			jsonbuilder << "\"alert\":\"" << encodedMsg.substr(0, maxMsgLength) << "...\",";
			jsonbuilder << "\"sound\":\"" << _soundName << "\",";
			if(_badgeNumber > 0) jsonbuilder << "\"badge\":" << _badgeNumber << ",";
			jsonbuilder << "}";
			jsonbuilder << "}";			
		}
		jsonRepresentation = jsonbuilder.str();
	}
	
	const std::string &NotificationPayload::toJSON() const {
		return jsonRepresentation;
	}
	
	std::string &NotificationPayload::soundName(){
		return _soundName;
	}

	const std::string &NotificationPayload::soundName() const {
		return _soundName;
	}
	
	std::string &NotificationPayload::message(){
		return msg;
	}

	std::string &NotificationPayload::deviceToken(){
		return devToken;
	}
}
