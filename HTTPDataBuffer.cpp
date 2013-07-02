//
//  HTTPDataBuffer.cpp
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 6/7/13.
//
//

#include "HTTPDataBuffer.h"
#include <cstdlib>
#include <cstring>

namespace pmm {
	DataBuffer::DataBuffer(){
		size = 0;
		buffer = NULL;
	}
	
	DataBuffer::~DataBuffer() {
		if(buffer != NULL) free(buffer);
	}
	
	void *DataBuffer::appendData(char *data, size_t dataSize){
		if (size == 0) {
			buffer = (char *)malloc(dataSize);
			if (buffer == NULL) return NULL;
			memcpy(buffer, data, dataSize);
			size = dataSize;
		}
		else {
			char *tmpbuf;
			tmpbuf = (char *)malloc(size + dataSize);
			if (tmpbuf == NULL) return NULL;
			memcpy(tmpbuf, buffer, size);
			free(buffer);
			memcpy(tmpbuf + size, data, dataSize);
			buffer = tmpbuf;
			size += dataSize;
		}
		return buffer;
	}

}