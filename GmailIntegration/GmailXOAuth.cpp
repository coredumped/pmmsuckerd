//
//  GmailXOAuth.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 6/6/13.
//
//

#include "GmailXOAuth.h"
#include "UtilityFunctions.h"
#include "jsonxx.h"
#include "JSONParseException.h"
#include <map>
#include <sstream>
#include <curl/curl.h>

#ifndef DEFAULT_GAPI_CLIENT_ID
#define DEFAULT_GAPI_CLIENT_ID "521830774038.apps.googleusercontent.com"
//#define DEFAULT_GAPI_CLIENT_ID "521830774038-h9l1oli67hv6p9ie50ib395vv9ok594g.apps.googleusercontent.com"
#endif

#ifndef DEFAULT_GAPI_SECRET
#define DEFAULT_GAPI_SECRET "pfTnXW-BkGNh6g9iRSOxN2S6"
// #define DEFAULT_GAPI_SECRET "bZO8uP2hZPNXifPy51r5tzyg"
#endif

#ifndef DEFAULT_GAPI_ACCESS_TOKEN_REQ_URL
#define DEFAULT_GAPI_ACCESS_TOKEN_REQ_URL "https://accounts.google.com/o/oauth2/auth"
#endif

#ifndef DEFAULT_GAPI_ACCESS_TOKEN_AUTH_URL
#define DEFAULT_GAPI_ACCESS_TOKEN_AUTH_URL "https://accounts.google.com/o/oauth2/token"
#endif

#ifndef DEFAULT_GAPI_REDIRECT_URL
// #define DEFAULT_GAPI_REDIRECT_URL "https://auth.pushmemail.com/oauth2/auth"
#define DEFAULT_GAPI_REDIRECT_URL "urn:ietf:wg:oauth:2.0:oob"
#endif

#ifndef DEFAULT_GMAIL_SCOPE
#define DEFAULT_GMAIL_SCOPE "https://mail.google.com/"
#endif

namespace pmm {
	namespace GAPI {
		const char *ClientID = DEFAULT_GAPI_CLIENT_ID;
		const char *Secret = DEFAULT_GAPI_SECRET;
		const char *RedirectURL = DEFAULT_GAPI_REDIRECT_URL;
		const char *GmailScope = DEFAULT_GMAIL_SCOPE;
	}
	
	GmailXOAuth::GmailXOAuth(){
		GAPIClientID = GAPI::ClientID;
		GAPISecret = GAPI::Secret;
	}
	
	GmailXOAuth::GmailXOAuth(const std::string &client_id, const std::string &secret){
		GAPIClientID = client_id;
		GAPISecret = secret;
	}
	
	void GmailXOAuth::generatePermissionURL(std::string &purl, const std::string &email){
		if (GAPIPermissionURL.size() == 0) {
			std::stringstream urlBuilder;
			urlBuilder << DEFAULT_GAPI_ACCESS_TOKEN_REQ_URL << "?";
			urlBuilder << "client_id=" << GAPIClientID << "&";
			std::string gapiRedirUrl = GAPI::RedirectURL;
			url_encode(gapiRedirUrl);
			urlBuilder << "redirect_uri=" << gapiRedirUrl << "&";
			urlBuilder << "response_type=code&";
			std::string gapiGmailScope = GAPI::GmailScope;
			url_encode(gapiGmailScope);
			urlBuilder << "scope=" << gapiGmailScope << "%20email&";
			std::string umail = email;
			url_encode(umail);
			urlBuilder << "login_hint=" << umail << "&";
			urlBuilder << "access_type=offline";
			//NOTE: Add the user's e-mail as a hint.
			GAPIPermissionURL = urlBuilder.str();
		}
		purl = GAPIPermissionURL;
	}
	
	void GmailXOAuth::generateToken(const std::string &authCode, std::string &token_, std::string &refresh_token_){
		std::stringstream urlBuilder;
		urlBuilder << DEFAULT_GAPI_ACCESS_TOKEN_AUTH_URL << "?";
		urlBuilder << "client_id=" << GAPIClientID << "&";
		std::string gsec = GAPISecret, gcode = authCode;
		url_encode(gsec);
		url_encode(gcode);
		urlBuilder << "client_secret=" << gsec << "&";
		urlBuilder << "code=" << gcode << "&";
		std::string gapiRedirUrl = GAPI::RedirectURL;
		url_encode(gapiRedirUrl);
		urlBuilder << "redirect_uri=" << gapiRedirUrl << "&";
		urlBuilder << "grant_type=authorization_code";
		
		std::map<std::string, std::string> param;
		param["client_id"] = GAPIClientID;
		param["client_secret"] = GAPISecret;
		param["code"] = authCode;
		param["redirect_uri"] = GAPI::RedirectURL;
		param["grant_type"] = "authorization_code";

		//Perform an HTTP POST for the given URL, it'll return the token and refreshtoken, compute the new timeout
		//value for this auth, save it in nextRefreshTStamp
		std::string jsonOutput;
		//int http_error = httpGetPerform(urlBuilder.str(), jsonOutput);
		int http_error = httpPostPerform(DEFAULT_GAPI_ACCESS_TOKEN_AUTH_URL, param, jsonOutput);
		if (http_error == 200) {
			std::istringstream in(jsonOutput);
			jsonxx::Object o;
			if(jsonxx::Object::parse(in, o)){
				GAPIRefreshToken = o.get<std::string>("refresh_token");
				refresh_token_ = GAPIRefreshToken;
				GAPIToken = o.get<std::string>("access_token");
				token_ = GAPIToken;
				expires_in = o.get<jsonxx::number>("expires_in");
				expireTStamp = time(0) + expires_in;
			}
			else {
				std::stringstream err_msg;
				err_msg << "Unable to parse:\n" << jsonOutput;
				throw JSONParseException(err_msg.str());
			}
		}
		else {
			//Should we throw an exception?
			throw HTTPException(http_error, jsonOutput);
		}
	}
	
	const std::string &GmailXOAuth::refreshToken(){
		std::map<std::string, std::string> param;
		param["client_id"] = GAPIClientID;
		param["client_secret"] = GAPISecret;
		param["refresh_token"] = GAPIRefreshToken;
		param["grant_type"] = "refresh_token";
		std::string jsonOutput;
		int http_error = httpPostPerform(DEFAULT_GAPI_ACCESS_TOKEN_AUTH_URL, param, jsonOutput);
		if (http_error == 200) {
			std::istringstream in(jsonOutput);
			jsonxx::Object o;
			if(jsonxx::Object::parse(in, o)){
				GAPIToken = o.get<std::string>("access_token");
				expires_in = o.get<jsonxx::number>("expires_in");
				expireTStamp = time(0) + expires_in;
			}
			else {
				std::stringstream err_msg;
				err_msg << "Unable to parse:\n" << jsonOutput;
				throw JSONParseException(err_msg.str());
			}
		}
		else {
			throw HTTPException(http_error, jsonOutput);
		}
		return GAPIToken;
	}
	
	const std::string &GmailXOAuth::refreshToken(const std::string &refresh_token_) {
		GAPIRefreshToken = refresh_token_;
		refreshToken();
		return GAPIToken;
	}
	
	void GmailXOAuth::generateIMAPAuthString(const std::string &email, std::string &str_){
		if (time(0) - 60 > expireTStamp) {
			refreshToken();
		}
		std::stringstream auth_string;
		auth_string << "user=" << email << "\1auth=Bearer " << GAPIToken << "\1\1";
		str_ = auth_string.str();
	}

	void GmailXOAuth::generateIMAPAuthStringB64(const std::string &email, std::string &str_) {
		std::string raw;
		generateIMAPAuthString(email, raw);
		base64Encode((void *)raw.c_str(), raw.size(), str_);
	}
}