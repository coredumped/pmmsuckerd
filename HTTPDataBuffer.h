//
//  HTTPDataBuffer.h
//  PMM Sucker
//
//  Created by Juan V. Guerrero on 6/7/13.
//
//

#ifndef __PMM_Sucker__HTTPDataBuffer__
#define __PMM_Sucker__HTTPDataBuffer__
#include <string>

namespace pmm {
	class DataBuffer {
	public:
		char *buffer;
		size_t size;
		DataBuffer();
		~DataBuffer();
		void *appendData(char *data, size_t dataSize);
	};
}

#endif
