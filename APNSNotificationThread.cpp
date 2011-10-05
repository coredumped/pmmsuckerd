//
//  APNSNotificationThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <sstream>
#include "openssl/ssl.h"
#include <netdb.h>
#include "APNSNotificationThread.h"
#include "Mutex.h"
#include "UtilityFunctions.h"
#ifndef DEVICE_BINARY_SIZE
#define DEVICE_BINARY_SIZE 32
#endif
#ifndef MAXPAYLOAD_SIZE
#define MAXPAYLOAD_SIZE 256
#endif
#ifndef DEFAULT_WAIT_BEFORE_RECONNECT
#define DEFAULT_WAIT_BEFORE_RECONNECT 15
#endif

namespace pmm {
	
#ifdef DEBUG
	extern Mutex mout;
#endif
	static bool sendPayload(SSL *sslPtr, const char *deviceTokenBinary, const char *payloadBuff, size_t payloadLength)
	{
		bool rtn = true;
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
			
			/* payload length network obrder */
			memcpy(binaryMessagePt, &networkOrderPayloadLength, sizeof(uint16_t));
			binaryMessagePt += sizeof(uint16_t);
			
			/* payload */
			memcpy(binaryMessagePt, payloadBuff, payloadLength);
			binaryMessagePt += payloadLength;
			int sslRetCode;
			if ((sslRetCode = SSL_write(sslPtr, binaryMessageBuff, (int)(binaryMessagePt - binaryMessageBuff))) <= 0){
				throw SSLException(sslPtr, sslRetCode, "Unable to send push notification :-(");
			}
			if(SSL_pending(sslPtr) > 0){
				char apnsRetCode[6];
				SSL_read(sslPtr, (void *)apnsRetCode, 6);
				if (apnsRetCode[1] != 0) {
					std::stringstream errmsg;
					errmsg << "Unable to post notification, error code=" << (int)apnsRetCode[1];
					throw GenericException(errmsg.str());
				}
			}
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
			mout.lock();
			std::cerr << "Loading certificate... " << _certPath << std::endl;
			mout.unlock();
#endif
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
	
	void APNSNotificationThread::connect2APNS(){
		initSSL();
#ifdef DEBUG
		mout.lock();
		std::cerr << "DEBUG: Connecting to APNS server..." << std::endl;
		mout.unlock();
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
		int enable_nosigpipe = 1;
#ifndef __linux__
		setsockopt(_socket, SOL_SOCKET, SO_NOSIGPIPE, (void *)enable_nosigpipe, (socklen_t)sizeof(int));
#endif
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
#ifdef DEBUG
		mout.lock();
		std::cerr << "DEBUG: Successfully connected to APNS service. thread=0x" << std::hex << (long)pthread_self() << std::endl;
		mout.unlock();
#endif
	}
	
	void APNSNotificationThread::ifNotConnectedToAPNSThenConnect(){
		if(_socket == -1) connect2APNS();
	}
	
	void APNSNotificationThread::disconnectFromAPNS(){
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
		waitTimeBeforeReconnectToAPNS = DEFAULT_WAIT_BEFORE_RECONNECT;
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
#ifdef DEBUG
		size_t i;  
#endif
		initSSL();
		connect2APNS();
		while (true) {
#ifdef DEBUG
			i++;
			if(i % 40 == 0){
				mout.lock();
				std::cout << "DEBUG: APNSNotificationThread=0x" << std::hex << (long)pthread_self() << " keepalive still tickling!!! pending=" << notificationQueue->size() << std::endl;
				mout.unlock();
			}
#endif
			//Verify if there are any ending notifications in the notification queue
			while (notificationQueue->size() > 0) {
#ifdef DEBUG
				mout.lock();
				std::cout << "DEBUG: There are " << notificationQueue->size() << " elements in the notification queue." << std::endl;
				mout.unlock();
#endif
				NotificationPayload payload = notificationQueue->extractEntry();
				try {
					notifyTo(payload.deviceToken(), payload);
				} 
				catch (SSLException &sse1){
					if (sse1.errorCode() == SSL_ERROR_ZERO_RETURN) {
						//Connection close, force reconnect
						_socket = -1;
						sleep(waitTimeBeforeReconnectToAPNS);
						connect2APNS();
						notificationQueue->add(payload);
					}
					else{
						throw;
					}
				}
				catch (...) {
					notificationQueue->add(payload);
				}
			}
			usleep(250000);
		}
	}
	
	void APNSNotificationThread::notifyTo(const std::string &devToken, NotificationPayload &msg){
		//Add some code here for god sake!!!
		std::string jsonMsg = msg.toJSON();
#ifdef DEBUG
		mout.lock();
		std::cout << "DEBUG: Sending notification " << jsonMsg << std::endl;
		mout.unlock();
#endif
		if (devTokenCache.find(devToken) == devTokenCache.end()) {
			std::string binaryDevToken;
			devToken2Binary(devToken, binaryDevToken);
			devTokenCache[devToken] = binaryDevToken;
		}
		sendPayload(apnsConnection, devTokenCache[devToken].c_str(), jsonMsg.c_str(), jsonMsg.size());
	}
	
	SSLException::SSLException(){ 
		currentOperation = SSLOperationAny;
	}
	
	SSLException::SSLException(SSL *sslConn, int sslStatus, const std::string &_derrmsg){
		std::stringstream errbuf;
		if (sslConn == NULL) {
			errmsg = _derrmsg;
		}
		else {
			if(_derrmsg.size() > 0) errbuf << _derrmsg << ": ";
			else errbuf << "SSL error occurred ";
			sslErrorCode = SSL_get_error(sslConn, sslStatus);
			errbuf << "code=" << sslErrorCode << " ";
			switch (sslErrorCode) {
				case SSL_ERROR_NONE:
					errbuf << "this is weird, the ssl operation actually succeded but an exception is still thrown, you are a bad programmer.";
					break;
				default:
					errbuf << "unable to complete SSL task";
					break;
			}
			errmsg = errbuf.str();
		}
	}

	int SSLException::errorCode(){
		return sslErrorCode;
	}
}