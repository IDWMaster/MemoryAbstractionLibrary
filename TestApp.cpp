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
	//EVEN newer program: TODO: Not written yet -- happens in future event
	//Newer program
	struct stat ms;
		memset(mander,0,sizeof(mander));
		if(stat("test",&ms)) {
			FILE* mptr = fopen("test","wb");
			fwrite(mander,1,sizeof(mander),mptr);
			fclose(mptr);
		}
		int fd = open("test",O_RDWR);
		MemoryMappedFileStream mstr(fd,ms.st_size);
		uint64_t rootptr;
		MemoryAllocator m(&mstr,rootptr,sizeof(mander));

	Reference<BTree<int,2>::Node> mref;
	if(rootptr == 0) {
		mref = m.Allocate<BTree<int,2>::Node>();
	}else {
		mref = Reference<BTree<int,2>::Node>(&mstr,rootptr);
	}
	BTree<int,2> tree(&m,mref);
	tree.Insert(2);
	tree.Insert(5);
	tree.Insert(1);
	tree.Insert(4);

	int searchvalue = 4;
	bool rval = tree.Find(searchvalue);
	searchvalue = 0;
	rval = tree.Find(searchvalue);
	return 0;

	//New program
	int egers[20];
	int32_t len = 0;
	BinaryInsert(egers,len,6);
  	BinaryInsert(egers,len,5);
 	BinaryInsert(egers,len,9);
	BinaryInsert(egers,len,0);
	BinaryInsert(egers,len,4);
	return 0;

/*






	//Old program


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
	MemoryAllocator allocator(&str,rootPtr,sizeof(mander));
	char sptr[2048];

	if(rootPtr) {
		StringObject obj;
		uint64_t ptr = rootPtr;
		int i = 0;
		while(ptr) {
			allocator.str->Read(ptr,obj);
			allocator.str->Read(ptr+sizeof(obj),sptr,obj.length);
			sptr[obj.length] = 0;
			std::cout<<i<<". "<<sptr<<" AT: "<<ptr<<"\n";
			ptr = obj.next;
			i++;
		}
	}

	while(true) {
		std::cout<<"Options: \n0. Add entry\n1. Exit\n2. Delete entry\n";
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
			std::cout<<"Allocated pointer "<<ptr<<"\n";
			allocator.str->Write(ptr,obj);
			allocator.str->Write(ptr+sizeof(StringObject),sptr,count+sizeof(StringObject));
			allocator.SetRootPtr(ptr);
			rootPtr = ptr;
		}
			break;
		case 1:
			return 0;
			break;
		case 2:
		{
			std::cout<<"Enter the value to delete\n";
			int selection;
			std::cin>>selection;
			std::cin.ignore();
			uint64_t ptr = rootPtr;
			uint64_t prevptr = 0;
			StringObject prevobj;
			StringObject mobj;
			for(int i = 0;i<selection;i++) {
				prevptr = ptr;
				allocator.str->Read(ptr,prevobj);
				ptr = prevobj.next;
			}
			allocator.str->Read(ptr,mobj);
			if(prevptr !=0) {
			prevobj.next = mobj.next;
			allocator.str->Write(prevptr,prevobj);
			}else {
				//Remove first element by changing root pointer
				allocator.SetRootPtr(mobj.next);
			}
			allocator.Free(ptr,sizeof(StringObject)+mobj.length);
		}
			break;
		}
	}
	*/

}
