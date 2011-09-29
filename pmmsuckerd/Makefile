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
CFLAGS=-g -I. -I.. -I../../jsonxx `pkg-config libcurl --cflags`
LIBS=`pkg-config libcurl --libs` -luuid -lpthread

all: pmmsuckerd

pmmsuckerd: main.cpp jsonxx.o JSONParseException.o PMMSuckerSession.o ServerResponse.o Mutex.o MailAccountInfo.o GenericThread.o APNSNotificationThread.o ThreadDispatcher.o
	$(GCC) $(CFLAGS) -g -o pmmsuckerd main.cpp jsonxx.o JSONParseException.o PMMSuckerSession.o ServerResponse.o Mutex.o MailAccountInfo.o GenericThread.o APNSNotificationThread.o ThreadDispatcher.o $(LIBS)

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

APNSNotificationThread.o: ../APNSNotificationThread.cpp ../APNSNotificationThread.h
	$(GCC) $(CFLAGS) -c -o APNSNotificationThread.o ../APNSNotificationThread.cpp

ThreadDispatcher.o: ../ThreadDispatcher.cpp ../ThreadDispatcher.h
	$(GCC) $(CFLAGS) -c -o ThreadDispatcher.o ../ThreadDispatcher.cpp

clean:
	rm -f *.o pmmsuckerd

rebuild: clean all

