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
#include <cstdlib>
#include "UtilityFunctions.h"
#include "Mutex.h"
#include "GenericException.h"
#include "MTLogger.h"

namespace pmm {
	
	std::map<std::string, std::string> _htmlEntityMap;
	void buildHTMLEntityMap(){
		if(_htmlEntityMap.size() == 0){
			
		}
	}
	
	void jsonTextEncode(std::string &theMsg){
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
		theMsg = newString;
	}
	
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
		buf << std::hex;
		buf.width(8);
		char *data = (char *)malloc(sizeof(uint32_t));
		memcpy(data, &binaryToken, sizeof(uint32_t));
		for (int i = 0; i < 4; i++) {
			int elem = data[i];
			buf << ntohl(elem);
		}
		free(data);
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
				idx = (int)newPos + 1;
			}
			else {
				_return.push_back(theString.substr(idx));
				break;
			}
		}
	}
	
	static int getTag(const std::string &input, int offset, std::string &tagName){
		int n = 0;
		tagName = "";
		for (int i = offset; i < input.size() && !isblank(input[i]); i++, n++) {
			tagName.append(1, tolower(input[i]));
		}
		return n;
	}
	
	void stripBlankLines(const std::string &input, std::string &output){
		output = "";
		int offset = 0;
		while(input[offset] == '\n') offset++;
		while(input[offset] == ' ') offset++;
		//while(input[offset] == '\r' && input[offset++] == '\n');
		for (size_t i = offset; i < input.size(); i++) {
			size_t tmp = i;
			while (input[i] == '\n') i++;
			if(i != tmp){
				output.append(1, '\n');
				i--;
				continue;
			}
			tmp = i;
			while (isblank(input[i])) i++;
			if(i != tmp){
				output.append(1, ' ');
				i--;
				continue;
			}
			tmp = i;
			while (i + 1 < input.size() && input[i] == '\r' && input[i + 1] == '\n') i+=2;
			if(i != tmp){
				output.append(1, '\n');
				i--;
				continue;
			}
			output.append(1, input[i]);
		}
	}
	
	static int translateEntities(const std::string &input, int offset, std::string &output){
		std::string entity;
		bool isCode = false;
		int j = 0;
		for (int i = offset + 1; i < input.size() && input[i] != ';'; i++, j++) {
			if (input[i] == '#' && i == offset + 1) isCode = true;
			else {
				entity.append(1, tolower(input[i]));
			}
		}
		if(entity.compare("lt") == 0) output.append(1, '<');
		else if(entity.compare("gt") == 0) output.append(1, '>');
		else if(entity.compare("amp") == 0) output.append(1, '&');
		else if(entity.compare("quot") == 0) output.append(1, '"');
		else if(entity.compare("apos") == 0) output.append(1, '\'');
		return j;
	}
	
	void stripHTMLTags(const std::string &htmlCode, std::string &output, int maxTextSize){
		output = "";
		int j = 0;
		bool ignoreChar = false;
		std::string currentTag;
		bool styleTagOpened = false;
		bool gotNewline = false;
		bool gotBlank = false;
		for (int i = 0; i < htmlCode.size(); i++) {
			if(j > maxTextSize) break;
			if (htmlCode[i] == '<') {
				ignoreChar = true;
				int n = getTag(htmlCode, i, currentTag);
				i += n;
				if(currentTag.compare("br") == 0 || currentTag.compare("div") == 0){
					if(!gotNewline){
						output.append("\n");
						j++;
						gotNewline = true;
					}
				}
				else if(currentTag.compare("span")){
					if (!gotBlank) {
						output.append(" ");
						j++;
						gotBlank = true;
					}
				}
				else if(currentTag.compare("style") == 0) styleTagOpened = true;
				else if(currentTag.compare("/style") == 0) styleTagOpened = false;
				else{
					//gotBlank = false;
					//gotNewline = false;
				}
			}
			else if(htmlCode[i] == '>'){
				ignoreChar = false;
			}
			else if(htmlCode[i] == '&'){
				int n = translateEntities(htmlCode, i, output);
				i += n;
				j += n;
			}
			else if(!ignoreChar){
				if(output.size() == 0 && htmlCode[i] == ' '){
					//Ignore
				}
				else {
					if(!styleTagOpened){
						if(gotNewline && htmlCode[i] == '\r') continue;
						if(gotBlank && htmlCode[i] == ' ') continue;
						if (htmlCode[i] == '\n') {
							if(gotNewline) continue;
							gotNewline = true;
						}
						else gotNewline = false;
						if (isblank(htmlCode[i])) {
							gotBlank = true;
						}
						else gotBlank = false;
						output.append(1, htmlCode[i]);
						j++;
						if(gotNewline){
							//Eat white spaces....
							int tmp = i;
							while (isblank(htmlCode[++i]));
							//while (htmlCode[i++] == '\n');
							if(tmp != i) i--;
						}
					}
				}
			}
		}
	}
}