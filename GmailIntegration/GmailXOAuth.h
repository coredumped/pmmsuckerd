//
//  GmailXOAuth.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 6/6/13.
//
//  Provides a generic means to authenticate Gmail accounts via SASL XOAUTH2
//

#ifndef __PMM_Sucker__GmailXOAuth__
#define __PMM_Sucker__GmailXOAuth__
#include "GenericException.h"
#include <string>
#include <time.h>

namespace pmm {
	/**
	 @class GmailXOAuth
	 Provides the means to interact with Gmail via the XOAUTH 2.0 protocol.
	 How To use:
	 Case A: You don't have a refresh token
	 1. Call generatePermissionUrl and pass it to the user
	 2. The user must open the url in a browser, for the pushmemail app a UIWebview will be opened.
	 3. After the user authorizes the application, an auth code will be issued, PushMeMail should retrieve the code from the given UIWebView
	 4. With the auth code we call generateToken to retrieve our token/refresh token pair
	 
	 Case B:
	 1. Call refreshToken to obtain the auth token and new refresh token
	 
	 After A or B you would call generateIMAPAuthString to retrieve the SASL XOAUTH
	 authentication string to be passed in the initial IMAP login.
	 */
	class GmailXOAuth {
	private:
		std::string GAPIPermissionURL;
		int expires_in;
		time_t expireTStamp;
	protected:
		std::string GAPIClientID;
		std::string GAPISecret;
		std::string GAPIToken;
		std::string GAPIRefreshToken;
	public:
		GmailXOAuth();
		GmailXOAuth(const std::string &client_id, const std::string &secret);
		void generatePermissionURL(std::string &purl, const std::string &email);
		void generateToken(const std::string &authCode, std::string &token_, std::string &refresh_token_);
		const std::string &refreshToken();
		const std::string &refreshToken(const std::string &refresh_token_);
		void generateIMAPAuthString(const std::string &email, std::string &str_);
		void generateIMAPAuthStringB64(const std::string &email, std::string &str_);
	};
	
	namespace GAPI {
		extern const char *ClientID;
		extern const char *Secret;
	}
}

#endif
