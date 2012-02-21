##
##  Makefile
##  PMM Sucker
##
##  Created by Juan V. Guerrero on 9/21/11.
##  Copyright (c) 2011 fn(x) Software. All rights reserved.
##
## Use this for building linux based pmmsuckers
## REQUIEREMENTS:
##   libcurl compiled with ssl support
##   libuuid
##

GCC=g++
CFLAGS=-g -I. -I.. -I../../jsonxx `pkg-config libcurl --cflags` -DDEBUG=1
LIBS=`pkg-config libcurl --libs` -luuid -lpthread -lssl -letpan -lsqlite3

all: pmmsuckerd

pmmsuckerd: main.cpp jsonxx.o JSONParseException.o PMMSuckerSession.o ServerResponse.o Mutex.o MailAccountInfo.o \
		GenericThread.o APNSNotificationThread.o ThreadDispatcher.o SharedQueue.o NotificationPayload.o \
		UtilityFunctions.o GenericException.o MailSuckerThread.o IMAPSuckerThread.o FetchedMailsCache.o \
		MTLogger.o MailMessage.o MessageUploaderThread.o DataTypes.o POP3SuckerThread.o SilentMode.o \
		QuotaDB.o UserPreferences.o
	$(GCC) $(CFLAGS) -g -o pmmsuckerd main.cpp jsonxx.o JSONParseException.o PMMSuckerSession.o ServerResponse.o \
				Mutex.o MailAccountInfo.o GenericThread.o APNSNotificationThread.o ThreadDispatcher.o \
				SharedQueue.o NotificationPayload.o UtilityFunctions.o GenericException.o \
				MailSuckerThread.o IMAPSuckerThread.o FetchedMailsCache.o MTLogger.o MailMessage.o \
				MessageUploaderThread.o DataTypes.o POP3SuckerThread.o SilentMode.o QuotaDB.o \
				UserPreferences.o $(LIBS)

jsonxx.o: ../../jsonxx/jsonxx.h ../../jsonxx/jsonxx.cc
	$(GCC) $(CFLAGS) -c -o jsonxx.o ../../jsonxx/jsonxx.cc

JSONParseException.o: ../JSONParseException.h ../JSONParseException.cpp
	$(GCC) $(CFLAGS) -c -o JSONParseException.o ../JSONParseException.cpp

PMMSuckerSession.o: ../PMMSuckerSession.h ../PMMSuckerSession.cpp
	$(GCC) $(CFLAGS) -c -o PMMSuckerSession.o ../PMMSuckerSession.cpp

ServerResponse.o: ../ServerResponse.h ../ServerResponse.cpp
	$(GCC) $(CFLAGS) -c -o ServerResponse.o ../ServerResponse.cpp

Mutex.o: ../Mutex.h ../Mutex.cpp
	$(GCC) $(CFLAGS) -c -o Mutex.o ../Mutex.cpp

MailAccountInfo.o: ../MailAccountInfo.h ../MailAccountInfo.cpp
	$(GCC) $(CFLAGS) -c -o MailAccountInfo.o ../MailAccountInfo.cpp

GenericThread.o: ../GenericThread.cpp ../GenericThread.h
	$(GCC) $(CFLAGS) -c -o GenericThread.o ../GenericThread.cpp

GenericException.o: ../GenericException.cpp ../GenericException.h
	$(GCC) $(CFLAGS) -c -o GenericException.o ../GenericException.cpp

APNSNotificationThread.o: ../APNSNotificationThread.cpp ../APNSNotificationThread.h ../SilentMode.h
	$(GCC) $(CFLAGS) -c -o APNSNotificationThread.o ../APNSNotificationThread.cpp

ThreadDispatcher.o: ../ThreadDispatcher.cpp ../ThreadDispatcher.h ../GenericThread.h
	$(GCC) $(CFLAGS) -c -o ThreadDispatcher.o ../ThreadDispatcher.cpp

SharedQueue.o: ../SharedQueue.cpp ../SharedQueue.h
	$(GCC) $(CFLAGS) -c -o SharedQueue.o ../SharedQueue.cpp

MTLogger.o: ../MTLogger.cpp ../MTLogger.h
	$(GCC) $(CFLAGS) -c -o MTLogger.o ../MTLogger.cpp

MessageUploaderThread.o: ../MessageUploaderThread.cpp ../MessageUploaderThread.h ../MailMessage.h
	$(GCC) $(CFLAGS) -c -o MessageUploaderThread.o ../MessageUploaderThread.cpp

MailMessage.o: ../MailMessage.cpp ../MailMessage.h
	$(GCC) $(CFLAGS) -c -o MailMessage.o ../MailMessage.cpp

NotificationPayload.o: ../NotificationPayload.cpp ../NotificationPayload.h
	$(GCC) $(CFLAGS) -c -o NotificationPayload.o ../NotificationPayload.cpp

UtilityFunctions.o: ../UtilityFunctions.cpp ../UtilityFunctions.h
	$(GCC) $(CFLAGS) -c -o UtilityFunctions.o ../UtilityFunctions.cpp

FetchedMailsCache.o: ../FetchedMailsCache.cpp ../FetchedMailsCache.h
	$(GCC) $(CFLAGS) -c -o FetchedMailsCache.o ../FetchedMailsCache.cpp

MailSuckerThread.o: ../MailSuckerThread.cpp ../MailSuckerThread.h ../SharedQueue.h ../GenericThread.h
	$(GCC) $(CFLAGS) -c -o MailSuckerThread.o ../MailSuckerThread.cpp

IMAPSuckerThread.o: ../IMAPSuckerThread.cpp ../IMAPSuckerThread.h ../MailSuckerThread.h ../FetchedMailsCache.h ../QuotaDB.h ../UserPreferences.h
	$(GCC) $(CFLAGS) -c -o IMAPSuckerThread.o ../IMAPSuckerThread.cpp

POP3SuckerThread.o: ../POP3SuckerThread.cpp ../POP3SuckerThread.h ../MailSuckerThread.h ../FetchedMailsCache.h ../QuotaDB.h ../UserPreferences.h
	$(GCC) $(CFLAGS) -c -o POP3SuckerThread.o ../POP3SuckerThread.cpp

DataTypes.o: ../DataTypes.cpp ../DataTypes.h
	$(GCC) $(CFLAGS) -c -o DataTypes.o ../DataTypes.cpp

SilentMode.o: ../SilentMode.cpp ../SilentMode.h
	$(GCC) $(CFLAGS) -c -o SilentMode.o ../SilentMode.cpp

QuotaDB.o: ../QuotaDB.cpp ../QuotaDB.h
	$(GCC) $(CFLAGS) -c -o QuotaDB.o ../QuotaDB.cpp

UserPreferences.o: ../UserPreferences.cpp ../UserPreferences.h
	$(GCC) $(CFLAGS) -c -o UserPreferences.o ../UserPreferences.cpp

clean:
	rm -f *.o pmmsuckerd

rebuild: clean all

