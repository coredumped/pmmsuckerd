//
//  main.cpp
//  apnNotificator
//
//  BAsic notification service, the idea is to identify why we are no receiving errors from Apple Push Notification
//  service, what we think is that such notificaions arrive in a asynchronous fashion.
//
//  Created by Juan Guerrero on 4/21/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//
#include <sys/stat.h>
#include <iostream>
#include "APNSNotificationThread.h"
#include "ThreadDispatcher.h"
#include "FetchedMailsCache.h"
#include "PendingNotificationStore.h"

#ifndef DEFAULT_CERTIFICATE_PATH
#define DEFAULT_CERTIFICATE_PATH "/Users/coredumped/Dropbox/MacOSXProjects/pmm_production.pem"
#endif

#ifndef DEFAULT_CERTIFICATE_KEY
#define DEFAULT_CERTIFICATE_KEY DEFAULT_CERTIFICATE_PATH
#endif

#ifndef IPOD_DEVICE_TOKEN
#define IPOD_DEVICE_TOKEN "4ab904318d30a0ecee730b369ea6b4acb9ab67c10023e5b497c25e035e353af5"
#endif


int main(int argc, const char * argv[])
{
	pmm::SharedQueue<pmm::NotificationPayload> notificationQueue;
	std::string certificate = DEFAULT_CERTIFICATE_PATH;
	std::string privateKey = DEFAULT_CERTIFICATE_KEY;
	std::string ipodDevToken = IPOD_DEVICE_TOKEN;
	for (int i = 1; i < argc; i++) {
		certificate = argv[i];
		privateKey = argv[i];
	}
	pmm::Log.open("/tmp/apnNotifier.log");
	pmm::APNSLog.open("/tmp/apns-notifier.log");
	pmm::APNSLog.setTag("APNS Notifier");
	pmm::CacheLog.open("/tmp/mail-cache.log");
	pmm::CacheLog.setTag("MailCache");
	SSL_library_init();
	SSL_load_error_strings();
	struct stat st;
	if (stat(certificate.c_str(), &st) != 0) {
		std::cout << "Unable to find SSL certificate." << std::endl;
		return 1;
	}
	pmm::APNSNotificationThread apnsNotifier;
	apnsNotifier.setCertPath(certificate);
	apnsNotifier.setKeyPath(certificate);
	apnsNotifier.useForProduction();
	apnsNotifier.notificationQueue = &notificationQueue;
	pmm::APNSLog << "Starting thread..." << pmm::NL;
	pmm::ThreadDispatcher::start(apnsNotifier);
	while (true) {
		std::cerr << "A valid notification sent" << std::endl;
		pmm::NotificationPayload np(ipodDevToken, "Hola Francisco :-)");
		np.isSystemNotification = true;
		notificationQueue.add(np);
		//Notification added to queue
		sleep(10);
		pmm::NotificationPayload np2("Device token invalido :-(", ipodDevToken);
		np2.isSystemNotification = true;
		std::cerr << "Invalid notification sent" << std::endl;
		notificationQueue.add(np2);
		sleep(10);
		std::cerr << "Another valid notification sent" << std::endl;
		notificationQueue.add(np);		
		std::cerr << "Waiting..." << std::endl;
		sleep(300);
		pmm::PendingNotificationStore::eraseOldPayloads();
	}
	return 0;
}

