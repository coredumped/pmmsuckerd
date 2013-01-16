//
//  MessageGeneratorThread.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 1/15/13.
//
//

#include "MessageGeneratorThread.h"
#include <sstream>
#include<unistd.h>

namespace pmmtest {
	MessageGeneratorThread::MessageGeneratorThread() {
		wait_microsecs = 250;
	}
	
	MessageGeneratorThread::MessageGeneratorThread(int min_generation_interval) {
		wait_microsecs = min_generation_interval;
	}
	
	void MessageGeneratorThread::operator()() {
		std::string emailAccounts[] = {
			"pmmtest_1@testinc.com",
			"pmmtest_2@testinc.com",
			"pmmtest_3@testinc.com",
			"pmmtest_4@testinc.com",
			"pmmtest_5@testinc.com"
		};
		
		std::string msgs[] = {
			"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nulla et laoreet nulla. Integer vitae mi justo. In dignissim, nulla et sagittis ultricies, tellus enim fringilla justo, sit amet semper ipsum dolor eget nisi. Suspendisse vehicula ante sit amet nisi eleifend vestibulum. Cras lobortis mollis fermentum. Duis ac sem ut eros pulvinar aliquet. Cras sed turpis sem, sit amet consectetur orci.",
			"Morbi semper mi iaculis dui tincidunt sed consectetur turpis iaculis. Quisque risus arcu, varius id porta et, ultricies at erat. In augue diam, tincidunt lacinia posuere ut, vehicula at magna. Sed consectetur, sapien tempus suscipit pharetra, sem risus imperdiet lacus, non dapibus enim ipsum ac leo. Aliquam posuere nunc fermentum est pellentesque ultrices. Quisque malesuada odio tincidunt mauris placerat volutpat. Morbi molestie, dui in viverra consequat, tortor nisl suscipit magna, sit amet semper tortor lorem in neque. In hac habitasse platea dictumst.",
			"Donec at erat non elit fermentum ornare. Vestibulum ut ligula sem, sit amet rutrum metus. Nunc porttitor nisi vitae arcu dapibus egestas. Maecenas a mi tellus, porttitor sagittis sapien. Pellentesque iaculis suscipit arcu, vitae facilisis sem volutpat id. Aliquam lacinia elit et ligula varius euismod. Vivamus imperdiet lectus vitae massa rutrum id interdum lacus ultricies. Suspendisse metus lorem, feugiat in hendrerit eget, rutrum in quam. Nullam nec accumsan sem. Aliquam quis lacus et leo dapibus cursus. Cras quis leo vel risus ornare auctor. Vestibulum laoreet pulvinar dapibus.",
			"Integer faucibus fermentum dolor ut gravida. Duis vel lobortis purus. Etiam libero risus, cursus nec porttitor ac, facilisis eu sem. Proin tortor nibh, rutrum non dictum id, accumsan eget quam. Proin dictum lobortis turpis, tempus hendrerit dolor imperdiet non. Donec pellentesque, purus a tempor pretium, magna augue tempus justo, non luctus nulla felis in erat. Nunc placerat odio sit amet nunc porta a pulvinar ante condimentum. Phasellus et lorem quam, vitae congue augue. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; Curabitur euismod massa non tellus tempor porta. Sed imperdiet ullamcorper diam ut dignissim. Duis vitae nisi nec lacus tempus feugiat.",
			"Morbi non turpis at quam ultrices tincidunt. Morbi gravida ipsum vitae nisl hendrerit tempor. Nunc sit amet sem nisi, vel tincidunt mi. Curabitur id lorem lectus, at ultricies erat. Suspendisse a molestie nunc. Ut ut dolor nisi. Vivamus sodales convallis lectus a consequat. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Mauris porta cursus quam, id viverra magna varius quis. Quisque eleifend mattis ligula. Cras porttitor tempor massa, ut hendrerit nulla tempus sit amet. Ut eu tellus est. Suspendisse ac arcu sit amet est convallis eleifend at vel quam."
		};
		int msgs_count = 5;
		int emails_count = 5;
		while (true) {
			time_t now = time(0);
			usleep(wait_microsecs + rand() % 100 - 50);
			pmm::MailMessage msg;
			std::string theMsg = msgs[rand() % msgs_count];
			msg.to = emailAccounts[rand() % emails_count];
			msg.from = "System tester";
			msg.fromEmail = "tester@testinc.net";
			msg.subject = theMsg;
			msg.dateOfArrival = now;
			msg.tzone = 0;
			msg.serverDate = msg.dateOfArrival;
			std::stringstream msgUID;
			msgUID << "msg" << rand() % 9999 << "-" << now;
			msg.msgUid = msgUID.str();
			
			pmm::NotificationPayload np("xx-devtoken", theMsg);
			np.origMailMessage = msg;
			msgQueue->add(np);
			//std::cout << "Message " << msg.msgUid << " added to queue!" << std::endl;
		}
		std::cerr << "Loop has been broken" << std::endl;
	}
}