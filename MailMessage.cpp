//
//  MailMessage.cpp
//  PMM Sucker
//
//  Created by Juan Guerrero on 10/14/11.
//  Copyright (c) 2011 fn(x) Software. All rights reserved.
//

#include <iostream>
#include <stdlib.h>
#ifdef __linux__
#include <string.h>
#endif
#include "MTLogger.h"
#include "libetpan/libetpan.h"
#include "MailMessage.h"
#include "UtilityFunctions.h"

namespace pmm {
	namespace MIMEParameter {
		static const char *charset = "charset";
	}
	namespace MIMEContentSubtype {
		static const char *plain = "plain";
	}
	namespace TextEncoding {
		static const char *utf8 = "UTF-8";
	}
	
	static void getMIMEData(struct mailmime_data * data, std::stringstream &outputStream, const std::string &charset)
	{
		if (data->dt_type == MAILMIME_DATA_TEXT) {
			switch (data->dt_encoding) {
				case MAILMIME_MECHANISM_BASE64:
				{
					//Decode base64 encoding
					char *decodedMsg;
					size_t decodedSize, indx = 0;
					int r = mailmime_base64_body_parse(data->dt_data.dt_text.dt_data, data->dt_data.dt_text.dt_length, &indx, &decodedMsg, &decodedSize);
					if (r == MAILIMF_NO_ERROR) {
						outputStream.write(decodedMsg, decodedSize);
						mailmime_decoded_part_free(decodedMsg);
					}
				}
					break;
				case MAILMIME_MECHANISM_QUOTED_PRINTABLE:
				{
					char *decodedMsg;
					size_t indx = 0, decodedSize = 0;
					int r = mailmime_quoted_printable_body_parse(data->dt_data.dt_text.dt_data, data->dt_data.dt_text.dt_length, &indx, &decodedMsg, &decodedSize, 0);
					if (r == MAILIMF_NO_ERROR) {
						size_t indx2 = 0;
						char *newText = 0;
						mailmime_encoded_phrase_parse(charset.c_str(), decodedMsg, decodedSize, &indx2, TextEncoding::utf8, &newText);
						if(newText != 0){
							outputStream << newText;
							free(newText);
						}
						else outputStream.write(decodedMsg, decodedSize);
						mailmime_decoded_part_free(decodedMsg);
					}
				}
					break;
				default:
					//Consider an encoding conversion here!!!!
					outputStream.write(data->dt_data.dt_text.dt_data, data->dt_data.dt_text.dt_length);
					break;
			}
		}
	}

	static void getMIMEMsgBody(struct mailmime * mime, std::stringstream &outputStream)
	{
		clistiter * cur;		
		switch (mime->mm_type) {
			case MAILMIME_SINGLE:
			{
				if (strcasecmp(mime->mm_content_type->ct_subtype, MIMEContentSubtype::plain) == 0) {
					//Now we guess the charset
					clist *params = mime->mm_content_type->ct_parameters;
					std::string charset = TextEncoding::utf8;
					for (clistiter *param = clist_begin(params); param != NULL; param = clist_next(param)) {
						struct mailmime_parameter *theParam = (struct mailmime_parameter *)clist_content(param);
						//std::cout << theParam->pa_name << "=" << theParam->pa_value << std::endl;
						if (strcasecmp(theParam->pa_name, MIMEParameter::charset) == 0) {
							charset = theParam->pa_value;
						}
					}
					getMIMEData(mime->mm_data.mm_single, outputStream, charset);
				}
			}
				break;
			case MAILMIME_MULTIPLE:
				for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ; cur != NULL ; cur = clist_next(cur)) {
					getMIMEMsgBody((struct mailmime *)clist_content(cur), outputStream);
				}
				break;
				
			case MAILMIME_MESSAGE:
				if (mime->mm_data.mm_message.mm_fields) {					
					if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
						getMIMEMsgBody(mime->mm_data.mm_message.mm_msg_mime, outputStream);
					}
					break;
				}
		}
	}

	
	MailMessage::MailMessage(){
		serverDate = 0;
		tzone = 0;
	}
	
	MailMessage::MailMessage(const std::string &_from, const std::string &_subject){
		from = _from;
		subject = _subject;
		tzone = 0;
		serverDate = 0;
	}
	
	MailMessage::MailMessage(const MailMessage &m){
		from = m.from;
		subject = m.subject;
		to = m.to;
		dateOfArrival = m.dateOfArrival;
		msgUid = m.msgUid;
		tzone = m.tzone;
		serverDate = m.serverDate;
	}
	
	bool MailMessage::parse(MailMessage &m, const std::string &rawMessage){ 
		return MailMessage::parse(m, rawMessage.c_str(), rawMessage.size());
	}
	
	bool MailMessage::parse(MailMessage &m, const char *msgBuffer, size_t msgSize){
		size_t indx = 0;
		struct mailimf_fields *fields;
		struct mailmime *result;
		int retCode = mailmime_parse(msgBuffer, msgSize, &indx, &result);
		if(retCode != MAILIMF_NO_ERROR){
			pmm::Log << "Unable to properly parse message";
			if(msgBuffer == 0 || msgSize == 0){
				pmm::Log << " because the given e-mail message is either NULL or empty!!!";
			}
			else {
				pmm::Log << ": " << msgBuffer;
			}
			pmm::Log << pmm::NL;
			return false;
		}
		fields = result->mm_data.mm_message.mm_fields;
		time_t now = time(0);
		m.from = "";
		m.subject = "";
		m.dateOfArrival = now;
		m.tzone = 0;
		bool gotTime = false;
		for (clistiter *iter = clist_begin(fields->fld_list); iter != clist_end(fields->fld_list); iter = iter->next) {
			struct mailimf_field *field = (struct mailimf_field *)clist_content(iter);
			//pmm::Log << "Field type: " << field->fld_type << pmm::NL;
			switch (field->fld_type) {
				case MAILIMF_FIELD_FROM:
				{
					clistiter *iter2 = clist_begin(field->fld_data.fld_from->frm_mb_list->mb_list);
					struct mailimf_mailbox *mbox = (struct mailimf_mailbox *)clist_content(iter2);
					if(mbox->mb_display_name == NULL) m.from = mbox->mb_addr_spec;
					else{
						m.from = mbox->mb_display_name;
					}
					size_t s1pos; 
					if ((s1pos = m.from.find("=?")) != m.from.npos) {
						//Encoded with RFC 2047, let's decode this damn thing!!!
						size_t indx2 = 0;
						char *newFrom;
						int r = mailmime_encoded_phrase_parse(TextEncoding::utf8, m.from.c_str(), m.from.size(), &indx2, TextEncoding::utf8, &newFrom);
						if(r == MAILIMF_NO_ERROR){
							m.from = newFrom;
							free(newFrom);
						}
					}
#ifdef DEBUG_FROM_FIELD
					pmm::Log << "DEBUG: From=\"" << m.from << "\"" << pmm::NL;
#endif
					if (m.from.size() > 0 && m.subject.size() > 0 && gotTime) break;
				}
					break;
				case MAILIMF_FIELD_SUBJECT:
				{
					m.subject = field->fld_data.fld_subject->sbj_value;
					size_t s1pos; 
					if ((s1pos = m.subject.find("=?")) != m.subject.npos) {
						size_t indx2 = 0;
						char *newSubject = 0;
						mailmime_encoded_phrase_parse(TextEncoding::utf8, m.subject.c_str(), m.subject.size(), &indx2, TextEncoding::utf8, &newSubject);
						if(newSubject != 0){
							m.subject = newSubject;
							free(newSubject);
						}
						else {
							pmm::Log << "Unable to decode subject from subject field: " << m.subject << pmm::NL;
							m.subject = "";
						}
					}
#ifdef DEBUG_SUBJECT_DATA
					pmm::Log << "DEBUG: Subject=\"" << m.subject << "\"" << pmm::NL;
#endif
					if (m.from.size() > 0 && m.subject.size() > 0 && gotTime) break;
				}
					break;
				case MAILIMF_FIELD_ORIG_DATE:
				{
					struct mailimf_date_time *origDate = field->fld_data.fld_orig_date->dt_date_time;
					struct tm tDate = { 0 };
					memset(&tDate, 0, sizeof(tm));
					tDate.tm_year = origDate->dt_year - 1900;
					tDate.tm_mon = origDate->dt_month - 1;
					tDate.tm_mday = origDate->dt_day;
					tDate.tm_hour = origDate->dt_hour;
					tDate.tm_min = origDate->dt_min;
					tDate.tm_sec = origDate->dt_sec;
					tDate.tm_gmtoff = origDate->dt_zone;
					m.serverDate = mktime(&tDate);
					m.dateOfArrival = now;
					m.tzone = 0;
#ifdef DEBUG_MSG_TIME_FIELD
					pmm::Log << "DEBUG: Computed dateOfArrival=" << m.dateOfArrival << " remote time="<< m.serverDate << " currTstamp=" << now << " diff=" << (m.dateOfArrival - m.serverDate) << pmm::NL;
#endif
					gotTime = true;
					if (m.from.size() > 0 && m.subject.size() > 0 && gotTime) break;
				}
					break;
				default:
					break;
			}
		}
		if (m.subject.size() <= 384) {
			std::stringstream msgBody;
			getMIMEMsgBody(result, msgBody);
			std::string theBody = msgBody.str();
			while (theBody[0] == '\r' || theBody[0] == '\n') {
				theBody = theBody.substr(1);
			}
			if (theBody.size() > 0) {
				if(m.subject.size() > 0) m.subject.append("\n");
				if(theBody.size() < 512){
					m.subject.append(theBody.c_str(), theBody.size());
				}
				else {
					m.subject.append(theBody.c_str(), 512);
				}
				while (m.subject[0] == '\r' || m.subject[0] == '\n') {
					m.subject = m.subject.substr(1);
				}
			}
		}
		mailmime_free(result);
		return true;
	}
}
