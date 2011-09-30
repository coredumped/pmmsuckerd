//
//  APNSNotificationThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <openssl/ssl.h>
#include <netdb.h>
#include "APNSNotificationThread.h"
#include "Mutex.h"
#ifndef DEVICE_BINARY_SIZE
#define DEVICE_BINARY_SIZE 32
#endif
#ifndef MAXPAYLOAD_SIZE
#define MAXPAYLOAD_SIZE 256
#endif

namespace pmm {
	
#ifdef DEBUG
	static Mutex m;
#endif
	static bool sendPayload(SSL *sslPtr, char *deviceTokenBinary, char *payloadBuff, size_t payloadLength)
	{
		bool rtn = false;
		if (sslPtr && deviceTokenBinary && payloadBuff && payloadLength)
		{
			uint8_t command = 1; /* command number */
			char binaryMessageBuff[sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t) +
								   DEVICE_BINARY_SIZE + sizeof(uint16_t) + MAXPAYLOAD_SIZE];
			/* message format is, |COMMAND|ID|EXPIRY|TOKENLEN|TOKEN|PAYLOADLEN|PAYLOAD| */
			char *binaryMessagePt = binaryMessageBuff;
			uint32_t whicheverOrderIWantToGetBackInAErrorResponse_ID = 1234;
			uint32_t networkOrderExpiryEpochUTC = htonl(time(NULL)+86400); // expire message if not delivered in 1 day
			uint16_t networkOrderTokenLength = htons(DEVICE_BINARY_SIZE);
			uint16_t networkOrderPayloadLength = htons(payloadLength);
			
			/* command */
			*binaryMessagePt++ = command;
			
			/* provider preference ordered ID */
			memcpy(binaryMessagePt, &whicheverOrderIWantToGetBackInAErrorResponse_ID, sizeof(uint32_t));
			binaryMessagePt += sizeof(uint32_t);
			
			/* expiry date network order */
			memcpy(binaryMessagePt, &networkOrderExpiryEpochUTC, sizeof(uint32_t));
			binaryMessagePt += sizeof(uint32_t);
			
			/* token length network order */
			memcpy(binaryMessagePt, &networkOrderTokenLength, sizeof(uint16_t));
			binaryMessagePt += sizeof(uint16_t);
			
			/* device token */
			memcpy(binaryMessagePt, deviceTokenBinary, DEVICE_BINARY_SIZE);
			binaryMessagePt += DEVICE_BINARY_SIZE;
			
			/* payload length network order */
			memcpy(binaryMessagePt, &networkOrderPayloadLength, sizeof(uint16_t));
			binaryMessagePt += sizeof(uint16_t);
			
			/* payload */
			memcpy(binaryMessagePt, payloadBuff, payloadLength);
			binaryMessagePt += payloadLength;
			if (SSL_write(sslPtr, binaryMessageBuff, (binaryMessagePt - binaryMessageBuff)) > 0)
				rtn = true;
		}
		return rtn;
	}
	
	static std::string _keyPath;
	static std::string _certPath;
	static std::string _certPassword;
	
	void APNSNotificationThread::initSSL(){
		if(!sslInitComplete){
			sslCTX = SSL_CTX_new(SSLv3_method());
			if(!sslCTX){
				//Fatal error, stop daemon, notify pmm service inmediately so it can release all e-mail
				//accounts from this pmm sucker inmediatelly
				std::cerr << "Unable to create SSL context" << std::endl;
				abort();
			}
			std::string caPath;
			size_t pos = _keyPath.find_last_of("/");
			if (pos == _keyPath.npos) {
				caPath = ".";
			}
			else {
				caPath = _keyPath.substr(0, pos);
			}
#ifdef DEBUG
			std::cerr << "DEBUG: Using caPath=" << caPath << std::endl;
#endif
			if (SSL_CTX_load_verify_locations(sslCTX, NULL, caPath.c_str()) <= 0) {
				//Without a valid CA location we can0t continue doing anything!!!
				//As before we need to notiy the central service of the issue so it can release e-mail accounts
				std::cerr << "Unable to set CA location" << std::endl;
				abort();
			}
#ifdef DEBUG
			std::cerr << "Loading certificate: " << _certPath << std::endl;
#endif
			if(SSL_CTX_use_certificate_file(sslCTX, _certPath.c_str(), SSL_FILETYPE_PEM) <= 0){
				std::cerr << "Unable to load certificate, can't continue like this" << std::endl;
				abort();
			}
			SSL_CTX_set_default_passwd_cb(sslCTX, (pem_password_cb *)_certPassword.c_str());
			if (SSL_CTX_use_PrivateKey_file(sslCTX, _keyPath.c_str(), SSL_FILETYPE_PEM) <= 0) {
				std::cerr << "Preparation for using keyfile: " << _keyPath << " failed miserably" << std::endl;
				abort();
			}
			if(!SSL_CTX_check_private_key(sslCTX)){
				std::cerr << "Given private key does not match, aborting!!!" << std::endl;
				abort();
			}
		}
	}
	
	void APNSNotificationThread::connect2APNS(){
		initSSL();
#ifdef DEBUG
		std::cerr << "DEBUG: Connecting to APNS server..." << std::endl;
#endif
		_socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);       
		if(_socket == -1)
		{
			std::cerr << "Unable to create socket!!!" << std::endl;
			abort();
		}
		//Lets clear the _server_addr structure
		memset(&_server_addr, '\0', sizeof(_server_addr));
		_server_addr.sin_family      = AF_INET;
		_server_addr.sin_port        = htons(_useSandbox ? APPLE_SANDBOX_PORT : APPLE_PORT);
		_host_info = gethostbyname(_useSandbox ? APPLE_SANDBOX_HOST : APPLE_HOST);
		int enable_nosigpipe = 1;
		setsockopt(_socket, SOL_SOCKET, SO_NOSIGPIPE, (void *)enable_nosigpipe, (socklen_t)sizeof(int));
		if(_host_info)
		{
			struct in_addr *address = (struct in_addr*)_host_info->h_addr_list[0];
			_server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*address));
		}
		else
		{
			std::cerr << "Unable to resolve server hostname" << std::endl;
			abort();
		}
		
		int err = connect(_socket, (struct sockaddr*) &_server_addr, sizeof(_server_addr)); 
		if(err == -1)
		{
			std::cerr << "Socket connection to remote server failed miserably, aborting..." << std::endl;
			abort();
		}    
		
		apnsConnection = SSL_new(sslCTX);
		if(!apnsConnection)
		{
			std::cerr << "SSL socket instantiation failed :-(" << std::endl;
			abort();
		}    
		
		SSL_set_fd(apnsConnection, _socket);
		
		err = SSL_connect(apnsConnection);
		if(err == -1)
		{
			std::cerr << "Real SSL connection to server didn't work at all :-(" << std::endl;
			abort();
		}	
	}
	
	void APNSNotificationThread::ifNotConnectedToAPNSThenConnect(){
		if(_socket == -1) connect2APNS();
	}
	
	void APNSNotificationThread::disconnectFromAPNS(){
		int err = SSL_shutdown(apnsConnection);
		if(err == -1)
		{
			std::cerr << "Unable to shutdown SSL connection, sorry" << std::endl;
			abort();
		}    
		
		err = close(_socket);
		if(err == -1)
		{
			std::cerr << "Unable to close native socker, aborting..." << std::endl;
			abort();
		}    
		
		SSL_free(apnsConnection);    
		SSL_CTX_free(sslCTX);
	}
	
	
	APNSNotificationThread::APNSNotificationThread(){
		sslInitComplete = false;
		_socket = -1;
#ifndef DEBUG
		_useSandbox = true;
#else
		_useSandbox = false;
#endif
	}
	
	APNSNotificationThread::~APNSNotificationThread(){
		
	}
	
	void APNSNotificationThread::setKeyPath(const std::string &keyPath){
		_keyPath = keyPath;
	}
	
	void APNSNotificationThread::setCertPath(const std::string &certPath){
		_certPath = certPath;
	}
	void APNSNotificationThread::setCertPassword(const std::string &certPwd){
		_certPassword = certPwd;
	}
	
	void APNSNotificationThread::operator()(){
		size_t i;
		initSSL();
		connect2APNS();
		while (true) {
#ifdef DEBUG
			i++;
			if(i % 40 == 0){
				m.lock();
				std::cout << "DEBUG: APNSNotificationThread=0x" << std::hex << (long)pthread_self() << " keepalive still tickling!!!" << std::endl;
				m.unlock();
			}
			if (i % 20 == 0 && notificationQueue->size() > 0) {
				std::cout << "DEBUG: There are " << notificationQueue->size() << " elements in the notification queue." << std::endl;
			}
#endif
			//Verify if there are any ending notifications in the notification queue
			while (notificationQueue->size() > 0) {
				NotificationPayload payload = notificationQueue->extractEntry();
				try {
					notifyTo(payload.deviceToken(), payload);
				} catch (...) {
					notificationQueue->add(payload);
				}
			}
			usleep(250000);
		}
	}
	
	void APNSNotificationThread::notifyTo(const std::string &devToken, const NotificationPayload &msg){
		
	}
}