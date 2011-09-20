//
//  PMMSuckerSession.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/18/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//
#ifdef __APPLE__
//#include <CFArray.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecAccess.h>
#include <Security/SecTrustedApplication.h>
#include <Security/SecACL.h>
#endif
#include <map>
#include <set>
#include <string>
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include "PMMSuckerSession.h"

#ifndef DEFAULT_API_KEY
#define DEFAULT_API_KEY "e63d4e6b515b323e93c649dc5b9fcca0d1487a704c8a336f8fe98c353dc6f17deec9ab455cd8b4c4bd1395e7d463f3549baa7ae5191a6cdc377aa5bbc5366668"
#endif
#ifndef DEFAULT_PMM_SERVICE_URL
#define DEFAULT_PMM_SERVICE_URL "https://pmmservice.appspot.com/pmmsuckerd"
#endif
#ifndef DEFAULT_PMM_SUCKER_USER_AGENT
#define DEFAULT_PMM_SUCKER_USER_AGENT "pmmsucker v=0.0.1"
#endif
#ifndef DEFAULT_DATA_BUFFER_SIZE
#define DEFAULT_DATA_BUFFER_SIZE 4096
#endif

#ifdef __APPLE__
#ifndef DEFAULT_SECURITY_ACCESS_LABEL
#define DEFAULT_SECURITY_ACCESS_LABEL "pmmsuckerd"
#endif
#ifndef DEFAULT_SEC_ITEM_LABEL
#define DEFAULT_SEC_ITEM_LABEL "com.fnxsoftware.pmmsucker"
#endif
#ifndef DEFAULT_SEC_ITEM_DESCRIPTION
#define DEFAULT_SEC_ITEM_DESCRIPTION "PMM Sucker id"
#endif
#ifndef DEFAULT_SEC_ITEM_ACCOUNT
#define DEFAULT_SEC_ITEM_ACCOUNT "suckerId"
#endif
#ifndef DEFAULT_SEC_ITEM_APIKEY
#define DEFAULT_SEC_ITEM_APIKEY "apiKey"
#endif
#ifndef DEFAULT_SEC_SERVICE_NAME
#define DEFAULT_SEC_SERVICE_NAME "pmmservice"
#endif
#endif

namespace pmm {
	
	static const char *suckerUserAgent = DEFAULT_PMM_SUCKER_USER_AGENT;
	
	static void suckerIdGet(std::string &suckerID){
		std::stringstream sId;
#ifdef __APPLE__ 
		//Try to retrieve value from system
		SecAccessRef access = nil;
		//CFStringRef accessLabel = CFStringCreateWithCStringNoCopy(NULL, DEFAULT_SECURITY_ACCESS_LABEL, kCFStringEncodingUTF8, kCFAllocatorNull);
		CFStringRef accessLabel = CFSTR(DEFAULT_SECURITY_ACCESS_LABEL);
		SecTrustedApplicationRef myself;
		OSStatus err = SecTrustedApplicationCreateFromPath(NULL, &myself);
#warning TODO: verify and report issues regarding the err variable status		
		CFArrayRef trustedApp = CFArrayCreate(NULL, (const void **)&myself, 1, &kCFTypeArrayCallBacks);
#warning TODO: verify and report issues regarding the err variable status
		err = SecAccessCreate(accessLabel, trustedApp, &access);
		//Get stored suckerID
		SecKeychainItemRef item = nil, apiKeyItem = nil;
		UInt32 suckerIDItemLength, apiKeyItemLength;
		void *suckerIDItemString, *apiKeyItemString;
#warning TODO: verify and report issues regarding the err variable status
		err = SecKeychainFindGenericPassword(NULL, strlen(DEFAULT_SEC_SERVICE_NAME), DEFAULT_SEC_SERVICE_NAME, 
									   strlen(DEFAULT_SEC_ITEM_ACCOUNT), DEFAULT_SEC_ITEM_ACCOUNT, 
									   &suckerIDItemLength, &suckerIDItemString, &item);
		if (err != noErr) {
			//Create UUID
			CFUUIDRef cfuuid = CFUUIDCreate(NULL);
			CFStringRef cfuuid_s = CFUUIDCreateString(NULL, cfuuid);
			//char cfuuid_buf [CFStringGetLength(cfuuid_s)];
			suckerID = "";
			suckerID.assign((char *)CFStringGetCStringPtr(cfuuid_s, kCFStringEncodingMacRoman), CFStringGetLength(cfuuid_s));
			//Store it in keychain
			err = SecKeychainAddGenericPassword(NULL, strlen(DEFAULT_SEC_SERVICE_NAME), DEFAULT_SEC_SERVICE_NAME, 
												strlen(DEFAULT_SEC_ITEM_ACCOUNT), DEFAULT_SEC_ITEM_ACCOUNT, 
												(UInt32)CFStringGetLength(cfuuid_s), (void *)CFStringGetCStringPtr(cfuuid_s, kCFStringEncodingMacRoman), &item);
			if(item) SecKeychainItemSetAccess(item, access);
#ifdef DEBUG
			std::cerr << "Generating suckerID: " << CFStringGetCStringPtr(cfuuid_s, kCFStringEncodingMacRoman) << std::endl;
#endif
			CFRelease(cfuuid_s);
			CFRelease(cfuuid);
		}
		else {
			suckerID.assign((char *)suckerIDItemString, suckerIDItemLength);
			SecKeychainItemFreeContent(NULL, suckerIDItemString);
		}
#warning TODO: verify and report issues regarding the err variable status
		err = SecKeychainFindGenericPassword(NULL, strlen(DEFAULT_SEC_SERVICE_NAME), DEFAULT_SEC_SERVICE_NAME, 
											 strlen(DEFAULT_SEC_ITEM_APIKEY), DEFAULT_SEC_ITEM_APIKEY, 
											 &apiKeyItemLength, &apiKeyItemString, &apiKeyItem);
		if (err != noErr) {
			SecKeychainAddGenericPassword(NULL, strlen(DEFAULT_SEC_SERVICE_NAME), DEFAULT_SEC_SERVICE_NAME, 
										  strlen(DEFAULT_SEC_ITEM_APIKEY), DEFAULT_SEC_ITEM_APIKEY, 
										  strlen(DEFAULT_API_KEY), DEFAULT_API_KEY, &apiKeyItem);
		}
		if (access) CFRelease(access);
		if (item) CFRelease(item);
		if (apiKeyItem) CFRelease(apiKeyItem);
#else
#error TODO: YOU MUST IMPLEMENT A LINUX VERSION FOR suckerIdGet()
#endif
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
	
	static void url_encode(std::string &theString){
		std::stringstream urlEncodedString;
		bool replaceHappened = false;
		for (size_t i = 0; i < theString.size(); i++) {
			if (isURLEncodable(theString[i])) {
				urlEncodedString << "%" << std::hex << (int)theString[i];
				replaceHappened = true;
			}
			else {
				char tbuf[2] = { theString[i], 0x00 };
				urlEncodedString << tbuf;
			}
		}
		if(replaceHappened) theString.assign(urlEncodedString.str());
	}
	
	class DataBuffer {
	public:
		char *buffer;
		size_t size;
		DataBuffer(){
			size = 0;
			buffer = NULL;
		}
		~DataBuffer() {
			if(buffer != NULL) free(buffer);
		}
		void *appendData(char *data, size_t dataSize){
			if (size == 0) {
				buffer = (char *)malloc(dataSize);
				if (buffer == NULL) return NULL;
				memcpy(buffer, (const void *)data, size);
				size = dataSize;
			}
			else {
				char *tmpbuf;
				tmpbuf = (char *)malloc(size + dataSize);
				if (tmpbuf == NULL) return NULL;
				memcpy(tmpbuf, buffer, size);
				free(buffer);
				memcpy(tmpbuf + size, data, dataSize);
				buffer = tmpbuf;
			}
			return buffer;
		}
	};

	static size_t gotDataFromServer(char *ptr, size_t size, size_t nmemb, void *userdata){
		DataBuffer *realData = (DataBuffer *)userdata;
		if(realData->appendData(ptr, size * nmemb) == NULL) return 0;
		return size * nmemb;
	}

	static void preparePostRequest(CURL *www, std::map<std::string, std::string> &postData, DataBuffer *buffer, const char *dest_url = DEFAULT_PMM_SERVICE_URL){
		std::stringstream encodedPost;
		curl_easy_setopt(www, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(www, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(www, CURLOPT_POST, 1);
		curl_easy_setopt(www, CURLOPT_URL, dest_url);
		curl_easy_setopt(www, CURLOPT_USE_SSL, 1);
		curl_easy_setopt(www, CURLOPT_USERAGENT, suckerUserAgent);
		//Let's url encode the param map
		std::map<std::string, std::string>::iterator keypair;
		for (keypair = postData.begin(); keypair != postData.end(); keypair++) {
			std::string param = keypair->first, value = keypair->second;
			url_encode(param);
			url_encode(value);
			if (encodedPost.str().size() == 0) encodedPost << param << "=" << value;
			else encodedPost << "&" << param << "=" << value;
		}
		curl_easy_setopt(www, CURLOPT_POSTFIELDSIZE, encodedPost.str().size());
		curl_easy_setopt(www, CURLOPT_POSTFIELDS, encodedPost.str().c_str());
		curl_easy_setopt(www, CURLOPT_WRITEDATA, buffer);
		curl_easy_setopt(www, CURLOPT_WRITEFUNCTION, gotDataFromServer);
		curl_easy_setopt(www, CURLOPT_FAILONERROR, 1);
	}
	
	static void executePost(std::map<std::string, std::string> &postData, std::string &output, CURL *wwwx = NULL, const char *dest_url = DEFAULT_PMM_SERVICE_URL){
		CURL *www;
		char errorBuffer[CURL_ERROR_SIZE + 4096];
		if (wwwx == NULL) www = curl_easy_init();
		else www = wwwx;
		DataBuffer serverOutput;
		preparePostRequest(www, postData, &serverOutput);
		curl_easy_setopt(www, CURLOPT_ERRORBUFFER, errorBuffer);
		CURLcode ret = curl_easy_perform(www);
		if(ret == CURLE_OK){
			output.assign(serverOutput.buffer, serverOutput.size);
		}
		else {
#ifdef DEBUG
			std::cerr << "DEBUG: Unable to execute POST request to " << dest_url << ": " << errorBuffer << std::endl;
#endif
			int http_errcode; 
			curl_easy_getinfo(www, CURLINFO_HTTP_CODE, &http_errcode);
			if(wwwx == NULL) curl_easy_cleanup(www);
			throw HTTPException(http_errcode, errorBuffer);
		}
		if(wwwx == NULL) curl_easy_cleanup(www);
	}
	
	SuckerSession::SuckerSession() {
		apiKey = DEFAULT_API_KEY;
		pmmServiceURL = DEFAULT_PMM_SERVICE_URL;
	}
	
	SuckerSession::SuckerSession(const std::string &srvURL) {
		apiKey = DEFAULT_API_KEY;
		pmmServiceURL = srvURL;
	}
	
	bool SuckerSession::register2PMM(){
		if(this->myID.size() == 0) suckerIdGet(this->myID);
		std::map<std::string, std::string> params;
		params["apiKey"] = apiKey;
		params["suckerID"] = this->myID;
#ifdef DEBUG
		std::cerr << "DEBUG: Registering with suckerID=" << this->myID << std::endl;
#endif
		std::string output;
		executePost(params, output);
		//Read and parse returned data
		
		return true;
	}
}