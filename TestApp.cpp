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


class FileEntry {
public:
	char filename[256];
	int metadata;
	FileEntry() {
		memset(filename,0,sizeof(filename));
		metadata = 0;
	}
	FileEntry(const std::string& fname) {
		memset(filename,0,sizeof(filename));
		memcpy(filename,fname.data(),fname.size());
		metadata = 0;
	}
	bool operator<(const FileEntry& other) const {
		return strcmp(filename,other.filename)<0;
	}
	bool operator==(const FileEntry& other) const {
		return strcmp(filename,other.filename) == 0;
	}

};

int main(int argc, char** argv)  {
	//EVEN newer program: TODO: Not written yet -- happens in future event
	//Newer program
	struct stat ms;
		memset(mander,0,sizeof(mander));
		system("rm test");
		if(stat("test",&ms)) {
			FILE* mptr = fopen("test","wb");
			fwrite(mander,1,sizeof(mander),mptr);
			fclose(mptr);
		}
		int fd = open("test",O_RDWR);
		MemoryMappedFileStream mstr(fd,sizeof(mander));
		uint64_t rootptr;
		MemoryAllocator m(&mstr,rootptr,sizeof(mander));

	Reference<BTree<FileEntry,2>::Node> mref;
	if(rootptr == 0) {
		mref = m.Allocate<BTree<FileEntry,2>::Node>();
	}else {
		mref = Reference<BTree<FileEntry,2>::Node>(&mstr,rootptr);
	}
	BTree<FileEntry,2> tree(&m,mref);
	auto printfunc = [&]() {
	  std::cout<<"Printing\n";
	  tree.Traverse([&](const FileEntry& entry){
	    std::cout<<entry.filename<<std::endl;
	  });
	};
	printfunc();
	FileEntry searchvalue;
	searchvalue = std::string("a");
	tree.Insert(searchvalue);
	searchvalue = std::string("b");
	tree.Insert(searchvalue);
	searchvalue = std::string("c");
	tree.Insert(searchvalue);
	searchvalue = std::string("d");
	tree.Insert(searchvalue);
	searchvalue = std::string("e");
	tree.Insert(searchvalue);
	searchvalue = std::string("f");
	tree.Insert(searchvalue);
	searchvalue = std::string("g");
	tree.Insert(searchvalue);
	printfunc();

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
