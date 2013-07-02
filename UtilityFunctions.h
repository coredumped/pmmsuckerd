//
//  UtilityFunctions.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/29/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_UtilityFunctions_h
#define PMM_Sucker_UtilityFunctions_h
#include <string>
#include <vector>
#include <map>
#include <sqlite3.h>
#ifdef __linux__
#include <stdint.h>
#endif
#include "MailAccountInfo.h"

namespace pmm {
	
	void buildHTMLEntityMap();

	/** Encodes the given string so it can be used in a URL string */
	void url_encode(std::string &theString);
	
	/** Sends an HTTP GET request to the given url
	 * @param url The URL to send the GET request
	 * @param output Stores the output once the HTTP request has been sent
	 * @return the http status code */
	int httpGetPerform(const std::string &url, std::string &output, void *httpConn = 0);
	int httpPostPerform(const std::string &url, std::map<std::string, std::string> &params, std::string &output, void *httpConn = 0);
	
	/** Converts a device token from string to a binary form
	 * @param devTokenString the device token in string form
	 * @param returns the device token in binary form. */
	void devToken2Binary(std::string devTokenString, std::string &binaryDevToken);
	void binary2DevToken(std::string &devToken, uint32_t binaryToken);
	
	void splitEmailAccounts(std::vector<MailAccountInfo> &mailAccounts, std::vector<MailAccountInfo> &imapAccounts, std::vector<MailAccountInfo> &pop3Accounts);
	
	void nltrim(std::string &s);
	bool tableExists(sqlite3 *dbConn, const std::string &tablename);
	
	void splitString(std::vector<std::string> &_return, const std::string &theString, const std::string &delim);
	
	void jsonTextEncode(std::string &theMsg);
	void stripHTMLTags(const std::string &htmlCode, std::string &output, std::map<std::string, std::string> &htmlProperties, int maxTextSize = 16384);
	
	void stripBlankLines(const std::string &input, std::string &output);
	
	void sqliteEscapeString(const std::string &input, std::string &output);
	
	void configValueGetInt(const std::string &varname, int &val);
	void configValueGetBool(const std::string &varname, bool &val);
	void configValueGetString(const std::string &varname, std::string &val);
	
	bool getBoolFromString(const std::string &str);
	
	void base64Encode(const void *data, size_t size, std::string &b64out);
}

#endif
