//
//  Mutex.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 9/25/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#ifndef PMM_Sucker_Mutex_h
#define PMM_Sucker_Mutex_h
#include<pthread.h>
#include "GenericException.h"
namespace pmm {
	//Encapsulates a POSIX threads mutual exclusion device
	class Mutex {
	private:
		pthread_mutex_t theMutex;
		pthread_mutexattr_t attrs;
		char *lFile;
		int lLine;
	public:
		Mutex();
		~Mutex();
		void lock();
		void lockWithInfo(const char *_srcFile = __FILE__, int _srcLine = __LINE__);
		void unlock();
	};
	
	class MutexLockException : public GenericException {
	public:
		std::string srcFile;
		int srcLine;
		MutexLockException();
		MutexLockException(const std::string &_errmsg);
		MutexLockException(const std::string &_errmsg, const char *_srcFile, int _srcLine);
	};
	
	class AtomicFlag : protected Mutex {
	private:
		bool theFlag;
	protected:
	public:
		AtomicFlag();
		AtomicFlag(bool initialValue);
		void set(bool newValue);
		bool get();
		void toggle();
		
		operator bool (){
			bool v;
			lock();
			v = theFlag;
			unlock();
			return v;
		}

		
		bool operator=(bool newValue);
		bool operator==(bool anotherValue);
		bool operator==(AtomicFlag &another);
		
		bool operator!=(bool anotherValue);
		bool operator!=(AtomicFlag &another);
	};
	
	template <class T>
	class AtomicVar : protected Mutex {
	private:
		T theValue;
	protected:
	public:
		AtomicVar(){}
		AtomicVar(T initialValue):T(initialValue){ }
		void set(T newValue){
			lock();
			theValue = newValue;
			unlock();
		}
		T get(){
			T v;
			lock();
			v = theValue;
			unlock();
			return v;
		}
		operator T (){
			T v;
			lock();
			v = theValue;
			unlock();
			return v;
		}
		T operator=(T newValue){
			set(newValue);
			return newValue;
		}
		bool operator==(T anotherValue){
			bool ret = false;
			lock();
			if (theValue == anotherValue) ret = true;
			unlock();
			return ret;
		}
		bool operator==(AtomicVar<T> &another){
			bool ret = false;
			T anotherValue = another.get();
			lock();
			if (theValue == anotherValue) ret = true;
			unlock();
			return ret;			
		}
		
		T operator!=(bool anotherValue){
			bool ret = true;
			lock();
			if (theValue == anotherValue) ret = false;
			unlock();
			return ret;			
		}
		T operator!=(AtomicVar<T> &another){
			bool ret = true;
			T anotherValue = another.get();
			lock();
			if (theValue == anotherValue) ret = false;
			unlock();
			return ret;						
		}
	};

}

#endif
