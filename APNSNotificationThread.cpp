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
#include "openssl/err.h"
#include <netdb.h>
#include <cerrno>
#include "APNSNotificationThread.h"
#include "Mutex.h"
#include "UtilityFunctions.h"
#include "SilentMode.h"
#ifndef DEVICE_BINARY_SIZE
#define DEVICE_BINARY_SIZE 32
#endif
#ifndef MAXPAYLOAD_SIZE
#define MAXPAYLOAD_SIZE 256
#endif
#ifndef DEFAULT_WAIT_BEFORE_RECONNECT
#define DEFAULT_WAIT_BEFORE_RECONNECT 15
#endif
#ifndef DEFAULT_MAX_NOTIFICATIONS_PER_BURST
#define DEFAULT_MAX_NOTIFICATIONS_PER_BURST 32
#endif
#ifndef DEFAULT_BURST_PAUSE_INTERVAL
#define DEFAULT_BURST_PAUSE_INTERVAL 10
#endif
#ifndef DEFAULT_MAX_CONNECTION_INTERVAL
#define DEFAULT_MAX_CONNECTION_INTERVAL 7200
#endif
#include <pthread.h>
#include <signal.h>

namespace pmm {
	MTLogger APNSLog;

	static bool sendPayload(SSL *sslPtr, const char *deviceTokenBinary, const char *payloadBuff, size_t payloadLength)
	{
		bool rtn = true;
		if (sslPtr && deviceTokenBinary && payloadBuff && payloadLength)
		{
			uint8_t command = 1; /* command number */
			size_t bufSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t) + DEVICE_BINARY_SIZE + sizeof(uint16_t) + MAXPAYLOAD_SIZE;
			char binaryMessageBuff[bufSize];
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
#ifdef DEBUG
			APNSLog << "DEBUG: Sending " << (int)(binaryMessagePt - binaryMessageBuff) << " bytes payload..." << pmm::NL;
#endif
			if ((sslRetCode = SSL_write(sslPtr, binaryMessageBuff, (int)(binaryMessagePt - binaryMessageBuff))) <= 0){
				//delete binaryMessageBuff;
				throw SSLException(sslPtr, sslRetCode, "Unable to send push notification :-(");
			}
			//delete binaryMessageBuff;
			if (SSL_pending(sslPtr) > 0) {
				char apnsRetCode[6] = {0, 0, 0, 0, 0, 0};
				do{
					sslRetCode = SSL_read(sslPtr, (void *)apnsRetCode, 6);
				}while (sslRetCode == SSL_ERROR_WANT_READ || sslRetCode == SSL_ERROR_WANT_WRITE);
				if (sslRetCode <= 0) {
					throw SSLException(sslPtr, sslRetCode, "Unable to read response code");
				}
#ifdef DEBUG
				APNSLog << "DEBUG: APNS Response: " << (int)apnsRetCode[0] << " " << (int)apnsRetCode[1] << " " << (int)apnsRetCode[2] << " " << (int)apnsRetCode[3] << " " << (int)apnsRetCode[4] << " " << (int)apnsRetCode[5] << pmm::NL; 
#endif
				if (apnsRetCode[1] != 0) {
					std::stringstream errmsg;
					errmsg << "Unable to post notification, error code=" << (int)apnsRetCode[1];
					throw GenericException(errmsg.str());
				}
			}
#ifdef DEBUG
			APNSLog << "DEBUG: payload sent!!!" << pmm::NL;
#endif
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
				APNSLog << "Can't create SSL context, aborting application!!!" << pmm::NL;
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
			APNSLog << "Loading certificate: " << _certPath << pmm::NL;
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
	
	void APNSNotificationThread::connect2APNS(){
		initSSL();
#ifdef DEBUG
		APNSLog << "DEBUG: Connecting to APNS server..." << pmm::NL;
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
		APNSLog << "DEBUG: Successfully connected to APNS service" << pmm::NL;
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
	}
	
	
	APNSNotificationThread::APNSNotificationThread(){
		sslInitComplete = false;
		_socket = -1;
#ifdef DEBUG
		_useSandbox = true;
#else
		_useSandbox = false;
#endif
		waitTimeBeforeReconnectToAPNS = DEFAULT_WAIT_BEFORE_RECONNECT;
		maxNotificationsPerBurst = DEFAULT_MAX_NOTIFICATIONS_PER_BURST;
		maxBurstPauseInterval = DEFAULT_BURST_PAUSE_INTERVAL;
		maxConnectionInterval = DEFAULT_MAX_CONNECTION_INTERVAL;
	}
	
	APNSNotificationThread::~APNSNotificationThread(){
		disconnectFromAPNS();
		SSL_CTX_free(sslCTX);
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
		size_t i = 0;  
#endif
		pmm::Log << "Starting APNSNotificationThread..." << pmm::NL;
#ifdef DEBUG
		APNSLog << "Masking SIGPIPE..." << pmm::NL;
#endif
		sigset_t bSignal;
		sigemptyset(&bSignal);
		sigaddset(&bSignal, SIGPIPE);
		if(pthread_sigmask(SIG_BLOCK, &bSignal, NULL) != 0){
			APNSLog << "WARNING: Unable to block SIGPIPE, if a persistent APNS connection is cut unexpectedly we might crash!!!" << pmm::NL;
		}
		initSSL();
		connect2APNS();
		pmm::Log << "APNSNotificationThread main loop started!!!" << pmm::NL;
		time_t start = time(NULL);
		while (true) {
			time_t currTime = time(NULL);
			if(currTime - start > maxConnectionInterval){
				APNSLog << "Max connection time (" << (currTime - start) << " > " << maxConnectionInterval << ") reached, reconnecting..." << pmm::NL;
				disconnectFromAPNS();
				initSSL();
				connect2APNS();
				APNSLog << "Re-connected succesfully!!!" << pmm::NL;
				start = time(NULL);
			}
#ifdef DEBUG
			if(++i % 2400 == 0){
				APNSLog << "DEBUG: keepalive still tickling!!!" << pmm::NL;
			}
#endif
			//Verify if there are any ending notifications in the notification queue
			NotificationPayload payload;
			int notifyCount = 0;
			while (notificationQueue->extractEntry(payload)) {
#ifdef DEBUG
				APNSLog << "DEBUG: There are " << (int)notificationQueue->size() << " elements in the notification queue." << pmm::NL;
#endif
				//Verify here if we should notify the event or not
#warning TODO: Remeber to add the notification filtering code here
				try {
					time_t theTime;
					time(&theTime);
					struct tm tmTime;
					gmtime_r(&theTime, &tmTime);		
					if(SilentMode::isInEffect(payload.origMailMessage.to, &tmTime)){
						payload.useSilentSound();
					}
					notifyTo(payload.deviceToken(), payload);
				} 
				catch (SSLException &sse1){
					if (sse1.errorCode() == SSL_ERROR_ZERO_RETURN) {
						//Connection close, force reconnect
						_socket = -1;
						disconnectFromAPNS();
						sleep(waitTimeBeforeReconnectToAPNS);
						connect2APNS();
						notificationQueue->add(payload);
					}
					else{
						if(errno == 32){
							APNSLog << "CRITICAL: Got a broken pipe, forcing reconnection timer..." << pmm::NL;
							start = 0;
							notificationQueue->add(payload);
						}
						else {
							APNSLog << "Got SSL error with code=" << sse1.errorCode() << " errno=" << errno << " aborting!!!" << pmm::NL;
							throw;
						}
					}
				}
				catch (...) {
					notificationQueue->add(payload);
				}
				if (++notifyCount > maxNotificationsPerBurst) {
					APNSLog << "Too many (" << notifyCount << ") notifications in a single burst, sleeping for " << maxBurstPauseInterval << " seconds..." << pmm::NL;
					sleep(maxBurstPauseInterval);
					break;
				}
			}
			usleep(250000);
		}
	}
	
	void APNSNotificationThread::notifyTo(const std::string &devToken, NotificationPayload &msg){
		//Add some code here for god sake!!!
		std::string jsonMsg = msg.toJSON();
#ifdef DEBUG
		APNSLog << "DEBUG: Sending notification " << jsonMsg << pmm::NL;
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
				case SSL_ERROR_SYSCALL:
				{
					unsigned long theErr = ERR_get_error();
					if (theErr == 0) {
						if (sslStatus == 0) {
							errbuf << "an EOF has been sent through the PIPE, this violates the SSL protocol!!!!";
						}
						else if(sslStatus == -1){
							errbuf << "socket error=" << errno;
						}
						else {
							errbuf << "syscall related exception, all is lost";
						}
					}
					else {
						char sslErrBuf[4096];
						ERR_error_string_n(theErr, sslErrBuf, 4095);
						errbuf << "ssl error: " << sslErrBuf;
					}
				}
					break;
				default:
				{
					unsigned long theErr = ERR_get_error();
					if (theErr == 0) {
						errbuf << "unable to complete SSL task";
					}
					else {
						char sslErrBuf[4096];
						ERR_error_string_n(theErr, sslErrBuf, 4095);
						errbuf << "ssl error: " << sslErrBuf;						
					}
				}
			}
			errmsg = errbuf.str();
		}
	}
	
	int SSLException::errorCode(){
		return sslErrorCode;
	}
}