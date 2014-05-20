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
class StringObject {
public:
	uint64_t next;
	uint64_t length;
};
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
	uint64_t rootPtr;
	MemoryAllocator allocator(&str,rootPtr);
	char sptr[2048];

	while(true) {
	if(rootPtr) {
		StringObject obj;
		uint64_t ptr = rootPtr;
		while(ptr) {
			allocator.str->Read(ptr,obj);
			allocator.str->Read(ptr+sizeof(obj),sptr,obj.length);
			std::cout<<sptr<<"\n";
			ptr = obj.next;
		}
	}
		std::cout<<"Options: \n0. Add entry\n1. Exit\n";
		int selection;
		std::cin>>selection;
		std::cin.ignore();
		switch(selection) {
		case 0:
		{
			std::cout<<"Enter string";
			std::cin.getline(sptr,sizeof(sptr));
			int count = strlen(sptr);
			StringObject obj;
			obj.next = rootPtr;
			obj.length = count;
			uint64_t ptr = allocator.Allocate(sizeof(StringObject)+count);
			allocator.str->Write(ptr,obj);
			allocator.str->Write(ptr+sizeof(StringObject),sptr,count+sizeof(StringObject));
			allocator.SetRootPtr(ptr);
			rootPtr = ptr;
		}
			break;
		case 1:
			return 0;
			break;
		}
	}

}
