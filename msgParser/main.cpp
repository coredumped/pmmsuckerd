//
//  main.cpp
//  msgParser
//
//  Created by Juan Guerrero on 4/22/12.
//  Copyright (c) 2012 fn(x) Software. All rights reserved.
//
#include <iostream>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include "MailMessage.h"

#ifndef DEFAULT_DIRECTORY
#define DEFAULT_DIRECTORY "/Users/coredumped/Downloads/raw-emails"
#endif

static size_t loadFile(const std::string &theFile, std::string &allData){
	struct stat st;
	if (stat(theFile.c_str(), &st) == 0) {
		std::ifstream inf(theFile.c_str());
		char theData[st.st_size];
		inf.read(theData, st.st_size);
		inf.close();
		allData.assign(theData, st.st_size);
	}
	return 0;
}

static void parseMessage(const std::string &emlPath){
	std::string msg;
	pmm::MailMessage m;
	//Read emal file here
	loadFile(emlPath, msg);
	std::cout << "\n------------- " << emlPath << " ----------" << std::endl;
	pmm::MailMessage::parse(m, msg);
	std::cout << "From:    " << m.from << std::endl;
	std::cout << "Subject: " << m.subject << std::endl;
}

int main(int argc, const char * argv[])
{
	std::string workDirectory = DEFAULT_DIRECTORY;
	for (int i = 1; i < argc; i++) {
		workDirectory = argv[i];
	}
	DIR *tDir = opendir(workDirectory.c_str());
	if (tDir != NULL) {
		struct dirent *d;
		while ((d = readdir(tDir)) != NULL) {
			if (d->d_type == DT_REG) {
				std::string emlPath = d->d_name;
				if (emlPath.find(".eml") != emlPath.npos) {
					std::stringstream fullPath;
					fullPath << workDirectory << "/" << emlPath;
					parseMessage(fullPath.str());
				}
			}
		}
		closedir(tDir);
	}

	
    return 0;
}

