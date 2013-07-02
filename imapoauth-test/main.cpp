//
//  main.cpp
//  imapoauth-test
//
//  Created by Juan V. Guerrero on 5/23/13.
//
//

#include <iostream>
#include <string>
#include <unistd.h>
#include <libetpan/libetpan.h>
#include "GmailXOAuth.h"
#include "UtilityFunctions.h"


int main(int argc, const char * argv[])
{
	std::string url;
	std::string auth_code;
	std::string authToken, refreshToken;
	std::string email = "ryoma.nagare@gmail.com";
	std::string imapAuth;
	pmm::GmailXOAuth xoauh;
	xoauh.generatePermissionURL(url, email);
		
	std::cout << "Visit this URL: " << url << std::endl;
	std::cout << "\nWhen done please type the authorization code here: ";
	std::cin >> auth_code;
	xoauh.generateToken(auth_code, authToken, refreshToken);
	std::cout << "Auth token: " << authToken << std::endl;
	std::cout << "Refresh token: " << refreshToken << std::endl;
	
	xoauh.generateIMAPAuthStringB64(email, imapAuth);
	std::cout << "IMAP Auth: " << imapAuth << std::endl;
	
	mailstream_debug = 256;
	mailimap *imap = mailimap_new(0, 0);
	int r = mailimap_ssl_connect(imap, "imap.gmail.com", 993);
	if (r != MAILIMAP_NO_ERROR){
		r = mailimap_oauth2_authenticate(imap, email.c_str(), authToken.c_str());
		if (r == MAILIMAP_NO_ERROR){
			std::cout << "SUCCESS: ";
			std::cout << imap->imap_response << std::endl;
			if(mailimap_select(imap, "INBOX") != MAILIMAP_NO_ERROR){
				
			}
		}
		else {
			std::cout << "FAILED: ";
			std::cout << imap->imap_response << std::endl;
		}

	}
	else {
		std::cerr << "Unable to connect to Gmail, error=" << r << std::endl;
	}
    return 0;
}

