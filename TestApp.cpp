/*
 * TestApp.cpp
 *
 *  Created on: May 20, 2014
 *      Author: brian
 */




#include "MemoryAbstraction.h"
#include <fcntl.h>
using namespace std;
using namespace MemoryAbstraction;
unsigned char mander[1024*1024*50];
int main(int argc, char** argv) {
	struct stat ms;
	memset(mander,0,sizeof(mander));
	if(stat("test",&ms)) {
		FILE* mptr = fopen("test","wb");
		fwrite(mander,1,sizeof(mander),mptr);
		fclose(mptr);
	}
	int fd = open("test",O_RDWR);
	MemoryMappedFileStream str(fd,ms.st_size);
	uint64_t ptr;
	MemoryAllocator allocator(&str,ptr);
	if(ptr) {
		Reference<int> mint(&str,ptr);
		std::cout<<"Read value "<<mint<<" at offset "<<ptr<<".\n";
	}else {
		Reference<int> mint = allocator.Allocate<int>();
		mint = 5;
		std::cout<<"Wrote value "<<mint<<" at offset "<<mint.offset<<".\n";
		allocator.SetRootPtr(mint);
		MemoryAllocator mt(&str,ptr);
		if(ptr != mint.offset) {
			throw "houston";
		}
	}

}
