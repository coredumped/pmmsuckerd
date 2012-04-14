//
//  APNSNotificationThread.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/26/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_APNSNotificationThread_h
#define PMM_Sucker_APNSNotificationThread_h
#include "GenericThread.h"
#include "SharedQueue.h"
#include "NotificationPayload.h"
#include "GenericException.h"
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "openssl/ssl.h"
#include "MTLogger.h"

#ifndef APPLE_SANDBOX_HOST
#define APPLE_SANDBOX_HOST          "gateway.sandbox.push.apple.com"
#endif
#ifndef APPLE_SANDBOX_PORT
#define APPLE_SANDBOX_PORT          2195
#endif
#ifndef APPLE_SANDBOX_FEEDBACK_HOST
#define APPLE_SANDBOX_FEEDBACK_HOST "feedback.sandbox.push.apple.com"
#endif
#ifndef APPLE_SANDBOX_FEEDBACK_PORT
#define APPLE_SANDBOX_FEEDBACK_PORT 2196
#endif


#ifndef APPLE_HOST
#define APPLE_HOST          		"gateway.push.apple.com"
#endif
#ifndef APPLE_PORT
#define APPLE_PORT         			 2195
#endif
#ifndef APPLE_FEEDBACK_HOST
#define APPLE_FEEDBACK_HOST 		"feedback.push.apple.com"
#endif
#ifndef APPLE_FEEDBACK_PORT
#define APPLE_FEEDBACK_PORT 		2196
#endif


namespace pmm {
	
	extern MTLogger APNSLog;
	
	class SSLException : public GenericException {
	private:
	protected:
		typedef enum {
			SSLOperationAny,
			SSLOperationRead,
			SSLOperationWrite
		}  SSLOperation;
		int sslErrorCode;
		SSLOperation currentOperation;
	public:
		SSLException();
		SSLException(SSL *sslConn, int sslStatus, const std::string &_derrmsg = "");
		int errorCode();
	};
	
	class APNSNotificationThread : public GenericThread {
	private:
		SSL_CTX *sslCTX;
		SSL *apnsConnection;
		void connect2APNS();
		void ifNotConnectedToAPNSThenConnect();
		void disconnectFromAPNS();
		bool _useSandbox;
		
		int _socket;
		struct sockaddr_in _server_addr;
		struct hostent *_host_info;
		int waitTimeBeforeReconnectToAPNS;
		std::map<std::string, std::string> devTokenCache;
		int maxNotificationsPerBurst;
		int maxBurstPauseInterval;
		int maxConnectionInterval;
	protected:
		bool sslInitComplete;
		void initSSL();
		void notifyTo(const std::string &devToken, NotificationPayload &msg);
	public:
		SharedQueue<NotificationPayload> *notificationQueue;
		APNSNotificationThread();
		void setKeyPath(const std::string &keyPath);
		void setCertPath(const std::string &certPath);
		void setCertPassword(const std::string &certPwd);
		~APNSNotificationThread();
		void operator()();
		void useForProduction();
		
		void triggerSimultanousReconnect();
	};
	
}
#endif
