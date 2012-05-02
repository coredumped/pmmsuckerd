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
#include "PendingNotificationStore.h"
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
#define DEFAULT_BURST_PAUSE_INTERVAL 1
#endif
#ifndef DEFAULT_MAX_CONNECTION_INTERVAL
#define DEFAULT_MAX_CONNECTION_INTERVAL 7200
#endif
#ifndef DEFAULT_NOTIFICATION_WARMUP_TIME
#define DEFAULT_NOTIFICATION_WARMUP_TIME 1
#endif
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h>

namespace pmm {
	MTLogger APNSLog;
	Mutex uuidGenM;
	
	namespace PushErrorCodes {
		uint32_t NoErrorsEncountered    = 0;
		uint32_t ProcessingError        = 1;    
		uint32_t MissingDeviceToken     = 2;
		uint32_t MissingTopic           = 3;
		uint32_t MissingPayload         = 4;
		uint32_t InvalidTokenSize       = 5;
		uint32_t InvalidTopicSize       = 6;
		uint32_t InvalidPayloadSize     = 7;
		uint32_t InvalidToken           = 8;
		uint32_t NoneUnknown            = 255;
		
		static const char *errorString[] = {
			"No errors encountered",
			"Processing error",
			"Missing device token",
			"Missing topic",
			"Missing payload",
			"Invalid token size",
			"Invalid topic size",
			"Invalid payload size",
			"Invalid token"	
		};
	}
	
	static uint32_t _uuidCnt = 0;
	static uint32_t msgUUIDGenerate(){
		uint32_t newUUID = 0;
		uuidGenM.lock();
		if(_uuidCnt == 0) _uuidCnt = time(0);
		_uuidCnt++;
		newUUID = _uuidCnt;
		uuidGenM.unlock();
		return newUUID;
	}
	
	bool APNSNotificationThread::gotErrorFromApple(){
		bool retval = false;
		if (_socket != -1) {
			int sslRetCode;
			char apnsRetCode[6] = {0, 0, 0, 0, 0, 0};
			do{
				sslRetCode = SSL_read(apnsConnection, (void *)apnsRetCode, 6);
			}while (sslRetCode == SSL_ERROR_WANT_READ);
			if (sslRetCode > 0) {
#ifdef APNS_DEBUG
				APNSLog << "DEBUG: APNS Response: " << (int)apnsRetCode[0] << " " << (int)apnsRetCode[1] << " " << (int)apnsRetCode[2] << " " << (int)apnsRetCode[3] << " " << (int)apnsRetCode[4] << " " << (int)apnsRetCode[5] << pmm::NL; 
#endif
				if (apnsRetCode[1] != 0) {
					uint32_t notifID;
					memcpy(&notifID, apnsRetCode + 2, 4);
					std::stringstream errmsg;
					if(apnsRetCode[1] == 255) APNSLog << "PANIC: Unable to post notification (id=" << (int)notifID << "), error code=" << (int)apnsRetCode[1] << pmm::NL;
					else APNSLog << "CRITICAL: Unable to post notification (id=" << (int)notifID << "), error code=" << (int)apnsRetCode[1] << ": " << PushErrorCodes::errorString[apnsRetCode[1]] << pmm::NL;
					PendingNotificationStore::setSentPayloadErrorCode(notifID, apnsRetCode[1]);
					retval = true;
					//At this point we should disconnect!!!
					if (apnsRetCode[0] == PushErrorCodes::InvalidToken) {
						//Retrieve failed device token from database...
						std::string invalidToken;
						if(PendingNotificationStore::getDeviceTokenFromMessage(invalidToken, notifID)) invalidTokens->add(invalidToken);
					}
					cntMessageFailed = cntMessageFailed + 1;
				}
			}
		}
		return retval;
	}

	static bool sendPayload(SSL *sslPtr, const char *deviceTokenBinary, const char *payloadBuff, size_t payloadLength, bool useSandbox, const std::string &devTokenS)
	{
		bool rtn = true;
		if (sslPtr && deviceTokenBinary && payloadBuff && payloadLength)
		{
			uint8_t command = 1; /* command number */
			size_t bufSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t) + DEVICE_BINARY_SIZE + sizeof(uint16_t) + MAXPAYLOAD_SIZE;
			char binaryMessageBuff[bufSize];
			/* message format is, |COMMAND|ID|EXPIRY|TOKENLEN|TOKEN|PAYLOADLEN|PAYLOAD| */
			char *binaryMessagePt = binaryMessageBuff;
			uint32_t whicheverOrderIWantToGetBackInAErrorResponse_ID = msgUUIDGenerate();
			APNSLog << "Just created a payload with ID=" << (int)whicheverOrderIWantToGetBackInAErrorResponse_ID << pmm::NL;
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
#ifdef DEBUG_PAYLOAD_SIZE
			APNSLog << "DEBUG: Sending " << (int)(binaryMessagePt - binaryMessageBuff) << " bytes payload..." << pmm::NL;
#endif
			sslRetCode = SSL_write(sslPtr, binaryMessageBuff, (int)(binaryMessagePt - binaryMessageBuff));
			if (sslRetCode == 0) {
				throw SSLException(sslPtr, sslRetCode, "Unable to send push notification, can't write to socket for some reason!!!");
			}
			PendingNotificationStore::saveSentPayload(devTokenS, payloadBuff, whicheverOrderIWantToGetBackInAErrorResponse_ID);
		}
		return rtn;
	}
		
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
				ERR_print_errors_fp(stderr);
				throw SSLException(NULL, 0, "Unable to load certificate file");
			}
			SSL_CTX_set_default_passwd_cb(sslCTX, (pem_password_cb *)_certPassword.c_str());
			if (SSL_CTX_use_PrivateKey_file(sslCTX, _keyPath.c_str(), SSL_FILETYPE_PEM) <= 0) {
				ERR_print_errors_fp(stderr);
				throw SSLException(NULL, 0, "Can't use given keyfile");
			}
			if(!SSL_CTX_check_private_key(sslCTX)){
				ERR_print_errors_fp(stderr);
				throw SSLException(NULL, 0, "Given private key is not valid, it does not match");
			}
			SSL_CTX_set_mode(sslCTX, SSL_MODE_AUTO_RETRY);
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
		APNSLog << "DEBUG: Successfully connected to APNS service" << pmm::NL;

		//Migrate the socket to blocking mode, magic is here!!!
		int flags = fcntl(_socket, F_GETFL);
		fcntl(_socket, F_SETFL, flags | O_NONBLOCK);
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
	
	void APNSNotificationThread::useForProduction(){
		_useSandbox = false;
		maxBurstPauseInterval = 1;
		maxNotificationsPerBurst = 200;
		pmm::APNSLog << "Setting this notification thread as a production notifier" << pmm::NL;
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
		warmingUP = false;
		cntMessageFailed = 0;
		cntMessageSent = 0;
	}
	
	APNSNotificationThread::~APNSNotificationThread(){
		if(_socket != -1) disconnectFromAPNS();
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
	
	
	static Mutex reconnectMutex;
	static time_t reconnectTime = 0;
	static bool shouldReconnect(){
		bool ret = false;
		reconnectMutex.lock();
		if(time(0) - reconnectTime < 5){ 
			ret = true;
			pmm::APNSLog << "A broken PIPE event was detected, waiting for APNS reconnect..." << pmm::NL;
		}
		reconnectMutex.unlock();
		return ret;
	}
	
	void APNSNotificationThread::triggerSimultanousReconnect(){
		reconnectMutex.lock();
		reconnectTime = time(0);
		reconnectMutex.unlock();
	}
		
	void APNSNotificationThread::operator()(){
		stopExecution = false;
#ifdef DEBUG_THREADS
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
		//Verify if connect delay is needed
		{
			struct stat st;
			if(stat("apns-logout-time.log", &st) == 0){
				int rightNow = time(0);
				int lastRun = time(0);
				//Read file, find las execution value and wait for 5 minutes....
				std::ifstream inf("apns-logout-time.log");
				inf >> lastRun;
				inf.close();
				int deltaT = rightNow - lastRun;
				if (deltaT < 300) {
					warmingUP = true;
					APNSLog << "Warming up thread..." << pmm::NL;
					sleep(DEFAULT_NOTIFICATION_WARMUP_TIME);
					warmingUP = false;
				}
			}
		}
		initSSL();
		connect2APNS();
		pmm::Log << "APNSNotificationThread main loop started!!!" << pmm::NL;
		time_t start = time(NULL);
		std::string lastDevToken;
		std::map<std::string, time_t> lastTimeSentMap;
		while (true) {
			if(_useSandbox){
				time_t currTime = time(NULL);
				if(currTime - start > maxConnectionInterval){
					APNSLog << "Max connection time (" << (currTime - start) << " > " << maxConnectionInterval << ") reached, reconnecting..." << pmm::NL;
					disconnectFromAPNS();
					initSSL();
					connect2APNS();
					APNSLog << "Re-connected succesfully!!!" << pmm::NL;
					start = time(NULL);
				}
			}
#ifdef DEBUG_THREADS
			if(++i % 2400 == 0){
				APNSLog << "DEBUG: keepalive still tickling!!!" << pmm::NL;
			}
#endif
			//Verify if there are any ending notifications in the notification queue
			NotificationPayload payload;
			int notifyCount = 0;
			APNSNotificationThread::gotErrorFromApple();
			if (shouldReconnect()) {
				disconnectFromAPNS();
				_socket = -1;
				sleep(DEFAULT_NOTIFICATION_WARMUP_TIME);
				connect2APNS();
			}
			while (notificationQueue->extractEntry(payload)) {
				if(stopExecution == true){
					APNSLog << "Explicit thread termination asked :-(" << pmm::NL;
					notificationQueue->add(payload);
					sleep(1);
				}
#ifdef DEBUG
				int nSize = notificationQueue->size();
				if((nSize + 1) % 20 == 0) APNSLog << "DEBUG: There are " << nSize << " elements in the notification queue." << pmm::NL;
#endif
				//Verify here if we should notify the event or not
				time_t rightNow;
				rightNow = time(0);
				std::string currentDeviceToken = payload.deviceToken();
				if (lastTimeSentMap.find(currentDeviceToken) == lastTimeSentMap.end()) {
					lastTimeSentMap[currentDeviceToken] = rightNow;
				}
				try {
					APNSNotificationThread::gotErrorFromApple();
					if (shouldReconnect()) {
						disconnectFromAPNS();
						_socket = -1;
						warmingUP = true;
						sleep(DEFAULT_NOTIFICATION_WARMUP_TIME);
						warmingUP = false;
						connect2APNS();
					}
					std::string newInvalidToken = newInvalidDevToken;
					if (newInvalidToken.size() > 0) {
						invalidTokenSet.insert(newInvalidToken);
						newInvalidDevToken = "";
					}
					if (invalidTokenSet.find(payload.deviceToken()) != invalidTokenSet.end()) {
						APNSLog << "CRITICAL: Refusing to post notification to device " << payload.deviceToken() << " this is an invalid device token." << pmm::NL;
#warning Warn the user somehow that her device is holding an invalid device token
						triggerSimultanousReconnect();
					}
					else {
						//time_t theTime;
						//time(&theTime);
						struct tm tmTime;
						gmtime_r(&rightNow, &tmTime);		
						if(SilentMode::isInEffect(payload.origMailMessage.to, &tmTime)){
							payload.useSilentSound();
						}
						if(_useSandbox) APNSLog << "Sending notification to APNs sandbox..." << pmm::NL;
						if (lastDevToken.compare(payload.deviceToken()) == 0) {
							if (rightNow - lastTimeSentMap[currentDeviceToken] < 2) {
								APNSLog << "WARNING: Got another notification for the same device in the same thread too soon, lets try to have another thread notify it..." << pmm::NL;
								sleep(1);
								APNSLog << "WARNING: Sent to another thread..." << pmm::NL;
								notificationQueue->add(payload);
								disconnectFromAPNS();
								sleep(1);
								APNSLog << "WARNING: Waking up after sleeping due to repetitive messages to the same device." << pmm::NL;
								lastDevToken = "";
								connect2APNS();
								continue;
							}
						}
						notifyTo(currentDeviceToken, payload);
						cntMessageSent = cntMessageSent + 1;
						lastDevToken = currentDeviceToken;
						lastTimeSentMap[currentDeviceToken] = rightNow;
					}
				} 
				catch (SSLException &sse1){
					if (sse1.errorCode() == SSL_ERROR_ZERO_RETURN) {
						APNSLog << "CRITICAL: SSLException caught!!! errorCode=" << sse1.errorCode() << " msg=" << sse1.message() << pmm::NL;
						//Connection close, force reconnect
						disconnectFromAPNS();
						_socket = -1;
						sleep(waitTimeBeforeReconnectToAPNS);
						connect2APNS();
						notificationQueue->add(payload);
					}
					else{
						if(errno == 32){
							APNSLog << "CRITICAL: SSLException caught!!! errorCode=" << sse1.errorCode() << " msg=" << sse1.message() << pmm::NL;
							APNSLog << "CRITICAL: Got a broken pipe, forcing reconnection..." << pmm::NL;
							notificationQueue->add(payload);
							disconnectFromAPNS();
							_socket = -1;
							triggerSimultanousReconnect();
							sleep(DEFAULT_NOTIFICATION_WARMUP_TIME);
							connect2APNS();
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
					APNSLog << "Too many (" << notifyCount << ") notifications in a single burst, reconnecting..." << pmm::NL;
					disconnectFromAPNS();
					_socket = -1;					
					//sleep(maxBurstPauseInterval);
					connect2APNS();
					break;
				}
			}
			usleep(250000);
		}
	}
	
	void APNSNotificationThread::notifyTo(const std::string &devToken, NotificationPayload &msg){
		//Add some code here for god sake!!!
		std::string jsonMsg = msg.toJSON();
#ifdef DEBUG_FULL_MESSAGE
		APNSLog << "DEBUG: Sending notification " << jsonMsg << pmm::NL;
#else
		if(msg.origMailMessage.to.size() == 0){
			APNSLog << "DEBUG: Sending message to device " << devToken << ": " << jsonMsg << pmm::NL;
		}
		else {
			APNSLog << "DEBUG: Sending message to device " << devToken << ": " << msg.origMailMessage.to << pmm::NL;
		}
#endif
		if (devTokenCache.find(devToken) == devTokenCache.end()) {
			std::string binaryDevToken;
			devToken2Binary(devToken, binaryDevToken);
			devTokenCache[devToken] = binaryDevToken;
		}
		sendPayload(apnsConnection, devTokenCache[devToken].c_str(), jsonMsg.c_str(), jsonMsg.size(), _useSandbox, devToken);
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