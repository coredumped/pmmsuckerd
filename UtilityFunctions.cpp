//
//  UtilityFunctions.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/29/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//
#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>
#include <strings.h>
#include <arpa/inet.h>
#include <string.h>
#include "UtilityFunctions.h"
#include "Mutex.h"
#include "GenericException.h"
#include "MTLogger.h"

namespace pmm {
	
	static inline bool isURLEncodable(char c){
		static const char *urlEncodableCharacters = "$&+,/:;=?@<>#%{}|\\^~[]` ";		
		if (c <= 0x1F || c >= 0x7f) {
			return true;
		}
		if (index(urlEncodableCharacters, c) != NULL) {
			return true;
		}
		return false;
	}
	
	void url_encode(std::string &theString){
		std::stringstream urlEncodedString;
		bool replaceHappened = false;
		for (size_t i = 0; i < theString.size(); i++) {
			if (isURLEncodable(theString[i])) {
				size_t _w = urlEncodedString.width();
				int normval = (int)(theString[i] & 0x000000ff);
				urlEncodedString.width(2);
				urlEncodedString.fill('0');
				if(normval >= 0x7f){
					int pfx = (normval >> 6) + 0x000000c0;
					urlEncodedString << "%" << std::hex << std::uppercase << pfx;
					normval = normval & 0x000007f + 0x000080;
				}
				urlEncodedString << "%";
				urlEncodedString << std::hex << std::uppercase << normval;
				urlEncodedString.width(_w);
				replaceHappened = true;
			}
			else {
				char tbuf[2] = { theString[i], 0x00 };
				urlEncodedString << tbuf;
			}
		}
		if(replaceHappened) theString.assign(urlEncodedString.str());
	}
	
	void devToken2Binary(std::string devTokenString, std::string &binaryDevToken){
		char buf[32];
		char *bufptr = buf;
		
		for (size_t i = 0; i < devTokenString.size(); i+=8) {
			std::string hex_s = devTokenString.substr(i, 8);
			int unit = 0;
			sscanf(hex_s.c_str(), "%x", &unit);
			unit = htonl(unit);
			memcpy(bufptr, (void *)&unit, sizeof(unsigned int));
			bufptr += sizeof(unsigned int);
		}
		binaryDevToken.assign(buf, 32);
	}
	
	void binary2DevToken(std::string &devToken, uint32_t binaryToken){
		std::stringstream buf;
		buf << std::hex << binaryToken;
		devToken = buf.str();
	}

	void splitEmailAccounts(std::vector<MailAccountInfo> &mailAccounts, std::vector<MailAccountInfo> &imapAccounts, std::vector<MailAccountInfo> &pop3Accounts){
		for (size_t i = 0; i < mailAccounts.size(); i++) {
			if (mailAccounts[i].mailboxType().compare("IMAP") == 0) {
				imapAccounts.push_back(mailAccounts[i]);
			}
			else {
				pop3Accounts.push_back(mailAccounts[i]);				
			}
		}
	}
	
	void nltrim(std::string &s){
		if(s.size() >= 1 && s[s.size() - 1] == '\n') s = s.substr(0, s.size() - 1);
		if(s.size() >= 1 && s[s.size() - 1] == '\r') s = s.substr(0, s.size() - 1);
	}
	
	bool tableExists(sqlite3 *dbConn, const std::string &tablename){
		std::string sqlCmd = "SELECT name FROM sqlite_master";
		sqlite3_stmt *statement;
		std::stringstream errmsg;
		char *sztail;
		int errCode = sqlite3_prepare_v2(dbConn, sqlCmd.c_str(), (int)sqlCmd.size(), &statement, (const char **)&sztail);
		if (errCode != SQLITE_OK) {
			errmsg << "Unable to execute query " << sqlCmd << " due to: " << sqlite3_errmsg(dbConn);
#ifdef DEBUG
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());
		}
		bool gotTable = false;
		while ((errCode = sqlite3_step(statement)) == SQLITE_ROW) {
			char *theTable = (char *)sqlite3_column_text(statement, 0);
			if (tablename.compare(theTable) == 0) gotTable = true;
		}
		if(errCode != SQLITE_DONE){
			errmsg << "Unable to retrieve values from query: " << sqlCmd << " due to: " << sqlite3_errmsg(dbConn);
			sqlite3_finalize(statement);
#ifdef DEBUG
			pmm::Log << errmsg.str() << pmm::NL;
#endif
			throw GenericException(errmsg.str());			
		}
		sqlite3_finalize(statement);
		return gotTable;
	}
	
	void splitString(std::vector<std::string> &_return, const std::string &theString, const std::string &delim){
		int idx = 0;
		while (true) {
			size_t newPos = theString.find(delim, idx);
			if (newPos != theString.npos) {
				std::string sub = theString.substr(idx, newPos - idx);
				_return.push_back(sub);
				idx = newPos + 1;
			}
			else {
				_return.push_back(theString.substr(idx));
				break;
			}
		}
	}
}