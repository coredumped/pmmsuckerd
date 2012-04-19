//
//  APNSFeedbackThread.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 4/15/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <sstream>
#include "openssl/ssl.h"
#include "openssl/err.h"
#include <netdb.h>
#include <cerrno>
#include "APNSFeedbackThread.h"
#include "Mutex.h"
#include "UtilityFunctions.h"
#include "SilentMode.h"
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#ifndef DEVICE_BINARY_SIZE
#define DEVICE_BINARY_SIZE 32
#endif
#ifndef DEFAULT_WAIT_BEFORE_RECONNECT
#define DEFAULT_WAIT_BEFORE_RECONNECT 15
#endif
#ifndef APNS_FEEDBACK_PORT
#define APNS_FEEDBACK_PORT 2196
#endif
#ifndef APNS_FEEDBACK_HOST
#define APNS_FEEDBACK_HOST "feedback.push.apple.com"
#endif
#ifndef APNS_FEEDBACK_HOST
#define APNS_FEEDBACK_HOST "feedback.sandbox.push.apple.com"
#endif
#ifndef APNS_ITEM_BYTE_SIZE
#define APNS_ITEM_BYTE_SIZE 38
#endif

namespace pmm {
	MTLogger fdbckLog;
		
	void APNSFeedbackThread::initSSL(){
		if(!sslInitComplete){
			sslCTX = SSL_CTX_new(SSLv3_method());
			if(!sslCTX){
				//Fatal error, stop daemon, notify pmm service inmediately so it can release all e-mail
				//accounts from this pmm sucker inmediatelly
				std::cerr << "Unable to create SSL context" << std::endl;
				fdbckLog << "Can't create SSL context, aborting application!!!" << pmm::NL;
				abort();
			}
			std::string caPath;
			size_t pos = _certPath.find_last_of("/");
			if (pos == _certPath.npos) {
				caPath = ".";
			}
			else {
				caPath = _certPath.substr(0, pos);
			}
			if (SSL_CTX_load_verify_locations(sslCTX, NULL, caPath.c_str()) <= 0) {
				//Without a valid CA location we can0t continue doing anything!!!
				//As before we need to notiy the central service of the issue so it can release e-mail accounts
				throw SSLException(NULL, 0, "Unable to verify CA location.");
			}
#ifdef DEBUG
			fdbckLog << "Loading certificate: " << _certPath << pmm::NL;
#endif
#warning TODO: Verify that the certificate has not expired, if it has send a very loud panic alert
			if(SSL_CTX_use_certificate_file(sslCTX, _certPath.c_str(), SSL_FILETYPE_PEM) <= 0){
				throw SSLException(NULL, 0, "Unable to load certificate file");
			}
			SSL_CTX_set_default_passwd_cb(sslCTX, (pem_password_cb *)_certPassword.c_str());
			if (SSL_CTX_use_PrivateKey_file(sslCTX, _keyPath.c_str(), SSL_FILETYPE_PEM) <= 0) {
				throw SSLException(NULL, 0, "Can't use given keyfile");
			}
			if(!SSL_CTX_check_private_key(sslCTX)){
				throw SSLException(NULL, 0, "Given private key is not valid, it does not match");
			}
		}
	}
	
	void APNSFeedbackThread::connect2APNS(){
		initSSL();
#ifdef DEBUG
		fdbckLog << "DEBUG: Connecting to APNS server..." << pmm::NL;
#endif
		_socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);       
		if(_socket == -1)
		{
			throw GenericException("Unable to create socket");
		}
		//Lets clear the _server_addr structure
		memset(&_server_addr, '\0', sizeof(_server_addr));
		_server_addr.sin_family      = AF_INET;
		_server_addr.sin_port        = htons(_useSandbox ? APPLE_SANDBOX_PORT : APPLE_PORT);
		_host_info = gethostbyname(_useSandbox ? APPLE_SANDBOX_HOST : APPLE_HOST);
#ifndef __linux__
		int enable_nosigpipe = 1;
		setsockopt(_socket, SOL_SOCKET, SO_NOSIGPIPE, (void *)enable_nosigpipe, (socklen_t)sizeof(int));
#endif
		struct linger lopt;
		lopt.l_onoff = 1;
		lopt.l_linger = 1;
		setsockopt(_socket, SOL_SOCKET, SO_LINGER, (void *)&lopt, sizeof(lopt));
		
		if(_host_info)
		{
			struct in_addr *address = (struct in_addr*)_host_info->h_addr_list[0];
			_server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*address));
		}
		else
		{
			throw GenericException("Can't resolver APNS server hostname");
		}
		
		int err = connect(_socket, (struct sockaddr*) &_server_addr, sizeof(_server_addr)); 
		if(err == -1)
		{
			throw GenericException("Can't connect to remote APNS server: client connection failed.");
		}    
		
		apnsConnection = SSL_new(sslCTX);
		if(!apnsConnection)
		{
			throw SSLException(NULL, 0, "Unable to create SSL connection object.");
		}    
		
		SSL_set_fd(apnsConnection, _socket);
		
		err = SSL_connect(apnsConnection);
		if(err == -1)
		{
			throw SSLException(apnsConnection, err, "APNS SSL Connection");
		}
		fdbckLog << "DEBUG: Successfully connected to APNS service" << pmm::NL;
	}
	
	void APNSFeedbackThread::ifNotConnectedToAPNSThenConnect(){
		if(_socket == -1) connect2APNS();
	}
	
	void APNSFeedbackThread::disconnectFromAPNS(){
		int err = SSL_shutdown(apnsConnection);
		if(err == -1)
		{
			throw SSLException(apnsConnection, err, "SSL shutdown");
		}    
		err = close(_socket);
		if(err == -1)
		{
			throw GenericException("Can't close socket client APNS socket");
		}    
		SSL_free(apnsConnection);    
		fdbckLog << "Disconnected from APNS feedback" << pmm::NL;
	}
	
	void APNSFeedbackThread::useForProduction(){
		_useSandbox = false;
		pmm::fdbckLog << "Setting this notification thread as a production notifier" << pmm::NL;
	}
	
	APNSFeedbackThread::APNSFeedbackThread(){
		sslInitComplete = false;
		_socket = -1;
#ifdef DEBUG
		_useSandbox = true;
#else
		_useSandbox = false;
#endif
	}
	
	APNSFeedbackThread::~APNSFeedbackThread(){
		disconnectFromAPNS();
		SSL_CTX_free(sslCTX);
	}
	
	void APNSFeedbackThread::setKeyPath(const std::string &keyPath){
		_keyPath = keyPath;
	}
	
	void APNSFeedbackThread::setCertPath(const std::string &certPath){
		_certPath = certPath;
	}
	void APNSFeedbackThread::setCertPassword(const std::string &certPwd){
		_certPassword = certPwd;
	}
	
	Mutex itM;
	std::set<std::string> __invalidTokens;
	
	void APNSFeedbackThread::operator()(){
		pmm::Log << "Starting APNSFeedbackThread..." << pmm::NL;
#ifdef DEBUG
		fdbckLog << "Masking SIGPIPE..." << pmm::NL;
#endif
		sigset_t bSignal;
		sigemptyset(&bSignal);
		sigaddset(&bSignal, SIGPIPE);
		if(pthread_sigmask(SIG_BLOCK, &bSignal, NULL) != 0){
			fdbckLog << "WARNING: Unable to block SIGPIPE, if a persistent APNS connection is cut unexpectedly we might crash!!!" << pmm::NL;
		}
		pmm::Log << "APNSFeedbackThread main loop started!!!" << pmm::NL;
		while (true) {
			//Verify if there are any ending notifications in the notification queue
			fdbckLog << "Retrieving device tokens..." << pmm::NL;
			initSSL();
			connect2APNS();
			itM.lock();
			__invalidTokens.clear();
			itM.unlock();
			//Read remote data here
			int bytes2read = 0;
			fdbckLog << "Trying to read data from feedback service..." << pmm::NL;
			while ((bytes2read = SSL_pending(apnsConnection)) > 0) {
				if (bytes2read >= APNS_ITEM_BYTE_SIZE) {
					while (true) {
						char binaryTuple[APNS_ITEM_BYTE_SIZE];
						int rRet = SSL_read(apnsConnection, binaryTuple, APNS_ITEM_BYTE_SIZE);
						if(rRet > 0){
							//Here we parse;
							uint32_t timeno;
							uint16_t tleno;
							uint32_t tokno;
							char *binaryPtr = binaryTuple;
							memcpy(&timeno, binaryPtr, sizeof(uint32_t));
							binaryPtr += sizeof(uint32_t);
							memcpy(&tleno, binaryPtr, sizeof(uint16_t));
							binaryPtr += sizeof(uint16_t);
							memcpy(&tokno, binaryPtr, sizeof(uint32_t));
							//time_t timestamp = ntohl(timeno);
							//uint16_t tlen = ntohs(tleno);
							
							//Take tokenN and parse it back to a string
							std::string sToken;
							binary2DevToken(sToken, tokno);
							fdbckLog << "  * Device " << sToken << " no longer has the app installed." << pmm::NL;
							break;
						}
						else {
							int ssl_error_code = SSL_get_error(apnsConnection, rRet);
							if(ssl_error_code != SSL_ERROR_WANT_READ){
								fdbckLog << "Unable to read data from feedback service: ssl_code=" << ssl_error_code << pmm::NL;
								break;
							}
						}
					}
				}
			}
			disconnectFromAPNS();
			sleep(600);
		}
	}
	
	void APNSFeedbackThread::invalidTokens(std::set<std::string> &devTokens){
		itM.lock();
		devTokens = __invalidTokens;
		itM.unlock();
	}
	
	bool APNSFeedbackThread::isTokenInvalid(const std::string &devToken){
		bool isValid = false;
		itM.lock();
		if (__invalidTokens.find(devToken) != __invalidTokens.end()) {
			isValid = true;
		}
		itM.unlock();
		return isValid;
	}

}
