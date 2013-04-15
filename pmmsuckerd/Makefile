##
##  Makefile
##  PMM Sucker
##
##  Created by Juan V. Guerrero on 9/21/11.
##  Copyright (c) 2011 fn(x) Software. All rights reserved.
##
## Use this for building linux based pmmsuckers
## REQUIREMENTS:
##   libcurl compiled with ssl support
##   libuuid
##

GCC=g++
CFLAGS=-g -I. -I.. -I../../jsonxx -I../rpc `pkg-config libcurl --cflags` -DDEBUG=1 -DUSE_RWLOCK=1 -DENABLE_NATE_DOT_COM_WORKAROUND=1
LIBS=`pkg-config libcurl --libs` -luuid -lpthread -lssl -letpan -lsqlite3 -lthrift

all: thriftcheck pmmsuckerd

thriftcheck:


pmmsuckerd: main.cpp jsonxx.o JSONParseException.o PMMSuckerSession.o ServerResponse.o Mutex.o MailAccountInfo.o \
		GenericThread.o APNSNotificationThread.o ThreadDispatcher.o SharedQueue.o NotificationPayload.o \
		UtilityFunctions.o GenericException.o MailSuckerThread.o IMAPSuckerThread.o FetchedMailsCache.o \
		MTLogger.o MailMessage.o MessageUploaderThread.o DataTypes.o POP3SuckerThread.o SilentMode.o \
		QuotaDB.o UserPreferences.o APNSFeedbackThread.o PendingNotificationStore.o RWLock.o Atomic.o \
		ObjectDatastore.o pmmrpc_types.o pmmrpc_constants.o PMMSuckerRPC.o PMMSuckerInfo.o RPCService.o \
		FetchDBSyncThread.o FetchDBRemoteSyncThread.o
	$(GCC) $(CFLAGS) -g -o pmmsuckerd main.cpp jsonxx.o JSONParseException.o PMMSuckerSession.o ServerResponse.o \
				Mutex.o MailAccountInfo.o GenericThread.o APNSNotificationThread.o ThreadDispatcher.o \
				SharedQueue.o NotificationPayload.o UtilityFunctions.o GenericException.o \
				MailSuckerThread.o IMAPSuckerThread.o FetchedMailsCache.o MTLogger.o MailMessage.o \
				MessageUploaderThread.o DataTypes.o POP3SuckerThread.o SilentMode.o QuotaDB.o \
				UserPreferences.o APNSFeedbackThread.o PendingNotificationStore.o RWLock.o \
				ObjectDatastore.o Atomic.o pmmrpc_types.o pmmrpc_constants.o PMMSuckerRPC.o \
				PMMSuckerInfo.o RPCService.o FetchDBSyncThread.o FetchDBRemoteSyncThread.o $(LIBS)

pmmrpc_types.o: ../rpc/pmmrpc_types.h ../rpc/pmmrpc_types.cpp
	$(GCC) $(CFLAGS) -c -o pmmrpc_types.o ../rpc/pmmrpc_types.cpp

pmmrpc_constants.o: ../rpc/pmmrpc_constants.h ../rpc/pmmrpc_constants.cpp
	$(GCC) $(CFLAGS) -c -o pmmrpc_constants.o ../rpc/pmmrpc_constants.cpp

PMMSuckerRPC.o: ../PMMSuckerSession.h ../rpc/pmmrpc_types.h ../rpc/pmmrpc_constants.h
	$(GCC) $(CFLAGS) -c -o PMMSuckerRPC.o ../rpc/PMMSuckerRPC.cpp

RPCService.o: ../rpc/RPCService.h ../rpc/RPCService.cpp ../rpc/PMMSuckerRPC.h ../rpc/pmmrpc_types.h ../rpc/pmmrpc_constants.h
	$(GCC) $(CFLAGS) -c -o RPCService.o ../rpc/RPCService.cpp

FetchDBSyncThread.o: ../FetchDBSyncThread.h ../FetchDBSyncThread.cpp ../rpc/pmmrpc_types.h ../FetchedMailsCache.h
	$(GCC) $(CFLAGS) -c -o FetchDBSyncThread.o ../FetchDBSyncThread.cpp

FetchDBRemoteSyncThread.o: ../FetchDBRemoteSyncThread.h ../FetchDBRemoteSyncThread.cpp ../rpc/pmmrpc_types.h ../rpc/pmmrpc_constants.h ../rpc/PMMSuckerRPC.h
	$(CGG) $(CFLAGS) -c -o FetchDBRemoteSyncThread.o ../FetchDBRemoteSyncThread.cpp

jsonxx.o: ../../jsonxx/jsonxx.h ../../jsonxx/jsonxx.cc
	$(GCC) $(CFLAGS) -c -o jsonxx.o ../../jsonxx/jsonxx.cc

JSONParseException.o: ../JSONParseException.h ../JSONParseException.cpp ../PMMSuckerInfo.h
	$(GCC) $(CFLAGS) -c -o JSONParseException.o ../JSONParseException.cpp

PMMSuckerSession.o: ../PMMSuckerSession.h ../PMMSuckerSession.cpp ../PMMSuckerInfo.h ../Mutex.h ../SharedVector.h ../SharedQueue.h ../MailAccountInfo.h ../NotificationPayload.h
	$(GCC) $(CFLAGS) -c -o PMMSuckerSession.o ../PMMSuckerSession.cpp

ServerResponse.o: ../ServerResponse.h ../ServerResponse.cpp
	$(GCC) $(CFLAGS) -c -o ServerResponse.o ../ServerResponse.cpp

Mutex.o: ../Mutex.h ../Mutex.cpp
	$(GCC) $(CFLAGS) -c -o Mutex.o ../Mutex.cpp

ObjectDatastore.o: ../ObjectDatastore.h ../ObjectDatastore.cpp ../RWLock.h ../MTLogger.h ../UtilityFunctions.h ../Atomic.h
	$(GCC) $(CFLAGS) -c -o ObjectDatastore.o ../ObjectDatastore.cpp

RWLock.o: ../RWLock.h ../RWLock.cpp
	$(GCC) $(CFLAGS) -c -o RWLock.o ../RWLock.cpp

Atomic.o: ../Atomic.h ../Atomic.cpp
	$(GCC) $(CFLAGS) -c -o Atomic.o ../Atomic.cpp

MailAccountInfo.o: ../MailAccountInfo.h ../MailAccountInfo.cpp
	$(GCC) $(CFLAGS) -c -o MailAccountInfo.o ../MailAccountInfo.cpp

GenericThread.o: ../GenericThread.cpp ../GenericThread.h ../Atomic.h ../Mutex.h ../RWLock.h
	$(GCC) $(CFLAGS) -c -o GenericThread.o ../GenericThread.cpp

GenericException.o: ../GenericException.cpp ../GenericException.h
	$(GCC) $(CFLAGS) -c -o GenericException.o ../GenericException.cpp

APNSNotificationThread.o: ../APNSNotificationThread.cpp ../APNSNotificationThread.h ../SilentMode.h ../Mutex.h ../Atomic.h
	$(GCC) $(CFLAGS) -c -o APNSNotificationThread.o ../APNSNotificationThread.cpp

ThreadDispatcher.o: ../ThreadDispatcher.cpp ../ThreadDispatcher.h ../GenericThread.h
	$(GCC) $(CFLAGS) -c -o ThreadDispatcher.o ../ThreadDispatcher.cpp

SharedQueue.o: ../SharedQueue.cpp ../SharedQueue.h
	$(GCC) $(CFLAGS) -c -o SharedQueue.o ../SharedQueue.cpp

MTLogger.o: ../MTLogger.cpp ../MTLogger.h ../Mutex.h
	$(GCC) $(CFLAGS) -c -o MTLogger.o ../MTLogger.cpp

MessageUploaderThread.o: ../MessageUploaderThread.cpp ../MessageUploaderThread.h ../MailMessage.h ../Mutex.h
	$(GCC) $(CFLAGS) -c -o MessageUploaderThread.o ../MessageUploaderThread.cpp

MailMessage.o: ../MailMessage.cpp ../MailMessage.h
	$(GCC) $(CFLAGS) -c -o MailMessage.o ../MailMessage.cpp

NotificationPayload.o: ../NotificationPayload.cpp ../NotificationPayload.h
	$(GCC) $(CFLAGS) -c -o NotificationPayload.o ../NotificationPayload.cpp

UtilityFunctions.o: ../UtilityFunctions.cpp ../UtilityFunctions.h
	$(GCC) $(CFLAGS) -c -o UtilityFunctions.o ../UtilityFunctions.cpp

FetchedMailsCache.o: ../FetchedMailsCache.cpp ../FetchedMailsCache.h ../FetchDBRemoteSyncThread.h
	$(GCC) $(CFLAGS) -c -o FetchedMailsCache.o ../FetchedMailsCache.cpp

MailSuckerThread.o: ../MailSuckerThread.cpp ../MailSuckerThread.h ../SharedQueue.h ../GenericThread.h ../Mutex.h ../RWLock.h ../Atomic.h
	$(GCC) $(CFLAGS) -c -o MailSuckerThread.o ../MailSuckerThread.cpp

IMAPSuckerThread.o: ../IMAPSuckerThread.cpp ../IMAPSuckerThread.h ../MailSuckerThread.h ../FetchedMailsCache.h ../QuotaDB.h ../UserPreferences.h ../SharedSet.h ../Mutex.h ../RWLock.h ../Atomic.h
	$(GCC) $(CFLAGS) -c -o IMAPSuckerThread.o ../IMAPSuckerThread.cpp

POP3SuckerThread.o: ../POP3SuckerThread.cpp ../POP3SuckerThread.h ../MailSuckerThread.h ../FetchedMailsCache.h ../QuotaDB.h ../UserPreferences.h ../SharedSet.h ../Mutex.h ../RWLock.h ../Atomic.h
	$(GCC) $(CFLAGS) -c -o POP3SuckerThread.o ../POP3SuckerThread.cpp

DataTypes.o: ../DataTypes.cpp ../DataTypes.h
	$(GCC) $(CFLAGS) -c -o DataTypes.o ../DataTypes.cpp

SilentMode.o: ../SilentMode.cpp ../SilentMode.h
	$(GCC) $(CFLAGS) -c -o SilentMode.o ../SilentMode.cpp

QuotaDB.o: ../QuotaDB.cpp ../QuotaDB.h
	$(GCC) $(CFLAGS) -c -o QuotaDB.o ../QuotaDB.cpp

UserPreferences.o: ../UserPreferences.cpp ../UserPreferences.h
	$(GCC) $(CFLAGS) -c -o UserPreferences.o ../UserPreferences.cpp

APNSFeedbackThread.o: ../APNSFeedbackThread.cpp ../APNSFeedbackThread.h ../GenericThread.h ../SharedVector.h ../APNSNotificationThread.h ../Mutex.h
	$(GCC) $(CFLAGS) -c -o APNSFeedbackThread.o ../APNSFeedbackThread.cpp

PendingNotificationStore.o: ../PendingNotificationStore.cpp ../PendingNotificationStore.h ../UtilityFunctions.h ../GenericException.h ../SharedQueue.h ../NotificationPayload.h
	$(GCC) $(CFLAGS) -c -o PendingNotificationStore.o ../PendingNotificationStore.cpp

PMMSuckerInfo.o: ../PMMSuckerInfo.h ../PMMSuckerInfo.cpp
	$(GCC) $(CFLAGS) -c -o PMMSuckerInfo.o ../PMMSuckerInfo.cpp

clean:
	rm -f *.o pmmsuckerd

rebuild: clean all

