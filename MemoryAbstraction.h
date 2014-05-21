/*
 * The author disclaims copyright to this source code.  In place of
* a legal notice, here is a blessing:
*
*    May you do good and not evil.
*    May you find forgiveness for yourself and forgive others.
*    May you share freely, never taking more than you give.
 */


/*
 * MemoryAbstraction.h
 *
 *  Created on: May 20, 2014
 *      Author: brian
 */

#ifndef MEMORYABSTRACTION_H_
#define MEMORYABSTRACTION_H_
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <algorithm>
#include <string.h>
#include <math.h>
namespace MemoryAbstraction {
class Stream {
public:
	virtual int Read(uint64_t position, void* buffer, int count) = 0;
	virtual void Write(uint64_t position, const void* buffer, int count) = 0;
	virtual uint64_t GetLength() = 0;
	template<typename T>
	int Read(uint64_t position, T& buffer) {
		return Read(position,&buffer,sizeof(buffer));
	}
	template<typename T>
	void Write(uint64_t position, const T& buffer) {
		Write(position,&buffer,sizeof(buffer));
	}
	virtual ~Stream(){};
};
template<typename T>
class Reference {
public:
	Stream* str;
	uint64_t offset;
	Reference(Stream* str, uint64_t offset){
		this->str = str;
		this->offset = offset;
	}
	operator T() {
		T retval;
		str->Read(offset,retval);
		return retval;
	}
	Reference& operator=(const T& other) {
		str->Write(offset,other);
		return *this;
	}
	Reference() {

	}

};
class MemoryMappedFileStream:public Stream {
public:
	int fd;
	uint64_t sz;
	unsigned char* ptr;
	uint64_t GetLength() {
		return sz;
	}
	MemoryMappedFileStream(int fd, uint64_t sz) {
		this->fd = fd;
		this->sz = sz;
		ptr = (unsigned char*)mmap(0,sz,PROT_READ | PROT_WRITE, MAP_SHARED, fd,0);
	}
	int Read(uint64_t position, void* buffer, int count) {
		if(position>sz) {
			return 0;
		}
		int available = std::min((int)(sz-position),count);
		memcpy(buffer,ptr+position,available);
		return available;
	}
	void Write(uint64_t position, const void* buffer, int count) {
		memcpy(ptr+position,buffer,count);
	}
	~MemoryMappedFileStream(){
		munmap(ptr,sz);
		close(fd);
	}
};
typedef struct {
	uint64_t next;
	uint64_t prev;
	uint64_t size;
} MemoryBlock;
class MemoryAllocator {
public:
	Stream* str;
	//Number of chunk sizes
	int numberOfChunks;
	uint64_t ReadChunk(int idx) {
		uint64_t retval;
		str->Read(idx*8,retval);
		return retval;
	}
	void WriteChunk(int idx, uint64_t val) {
		str->Write(idx*8,val);
	}
	void SetRootPtr(uint64_t root) {
			WriteChunk(numberOfChunks,root);
		}
	template<typename T>
	void SetRootPtr(const Reference<T>& root) {
		WriteChunk(numberOfChunks,root.offset);
	}
	uint64_t AllocateBytes(uint64_t size) {
		if(size<sizeof(MemoryBlock)) {
			size+=sizeof(MemoryBlock);
		}
		int chunkID = (int) std::ceil(log2(size));
		bool found = false;
		uint64_t pointer;
		for (; chunkID < numberOfChunks; chunkID++) {
			if ((pointer = ReadChunk(chunkID))) {
				found = true;
				break;
			}
		}
		if (!found) {
			//Not enough space -- insufficient funds to afford a bigger drive
			throw "Insufficient funds";
		}
		//Found a chunk -- remove it from the list of free blocks
		MemoryBlock chunk;
		str->Read(pointer,chunk);
		if(chunk.next) {
			MemoryBlock other;
			str->Read(chunk.next,other);
			other.prev = 0;
			str->Write(chunk.next,other);
		}
		WriteChunk(chunkID,chunk.next);
		//Check if we have additional free space after this block
		if(chunk.size-size>sizeof(MemoryBlock)) {

			uint64_t leftover = chunk.size-size-sizeof(MemoryBlock);
			//Found free space! Register it.
			RegisterFreeBlock(pointer+size,leftover);
		}
		return pointer;
	}
	void RegisterFreeBlock(uint64_t position, uint64_t sz) {
		int chunkID = (int)log2(sz);
		uint64_t pointer = ReadChunk(chunkID);
		MemoryBlock block;
		block.next = pointer;
		block.prev = 0;
		block.size = sz;
		if(pointer) {
			//Update linked list
			MemoryBlock other;
			str->Read(pointer,other);
			other.prev = position;
			str->Write(pointer,other);
		}
		str->Write(position,block);
		//Update root pointer
		WriteChunk(chunkID,position);
	}
	MemoryAllocator(Stream* str, uint64_t& rootPtr, uint64_t mappedLen) {
		this->str = str;
		uint64_t slen = mappedLen;
		numberOfChunks = log2(slen)+1;
		rootPtr = ReadChunk(numberOfChunks);
		if(!rootPtr) {
			//Compute space and write free block
			uint64_t freespace = slen-((numberOfChunks+2)*8)-sizeof(MemoryBlock)-32;
			RegisterFreeBlock((numberOfChunks+2)*8,freespace);
		}
	}

	void Free(uint64_t ptr, uint64_t sz) {
		if(sz<sizeof(MemoryBlock)) {
					sz+=sizeof(MemoryBlock);
				}
				RegisterFreeBlock(ptr,sz);

	}
	uint64_t Allocate(uint64_t sz) {
		return AllocateBytes(sz);
	}
	template<typename T>
	Reference<T> Allocate() {
		return Reference<T>(str,Allocate(sizeof(T)));
	}
	template<typename T>
	void Free(const Reference<T>& ptr) {
		int64_t sz = sizeof(T);

	}
};

template<typename T>
static size_t BinarySearch(T* A,size_t arrlen, const T& key, int& insertionMarker) {
	if(arrlen == 0) {
		insertionMarker = 0;
		return -1;
	}
	int max = arrlen-1;
	int min = 0;
	while(max>=min) {
		insertionMarker = min+((max-min)/2);
		if(A[insertionMarker] == key) {
			return insertionMarker;
		}
		if(A[insertionMarker]<key) {
			min = insertionMarker+1;
		}else {
			max = insertionMarker-1;
		}
	}
	return -1;
}
template<typename T>
static void BinaryInsert(T* A, size_t& len, const T& key) {
	if(len == 0) {
		A[0] = key;
		len++;
		return;
	}
	int location;
	BinarySearch(A,len,key,location);
	if(key<A[location]) {
		//Insert to left of location
		//Move everything at location to the right
		memmove(A+location+1,A+location,(len-location)*sizeof(T));
		//Insert value
		A[location] = key;
	}else {
		//Insert to right of location (this is ALWAYS called at end of list, so memmove is not necessary)
		location++;
		A[location] = key;
	}
	len++;
}


//Represents a B-tree
template<typename T, uint64_t KeyCount = 1024>
class BTree {
private:
	class Node {
	public:
		//The length of the keys array
		int length;
		//Keys within this node
		T keys[KeyCount];
		//The child nodes (subtrees)
		uint64_t children[KeyCount+1];
		Node() {
			memset(keys,0,sizeof(keys));
			memset(children,0,sizeof(children));
			length = 0;
		}
	};
	Reference<Node> root;
	//Node is a leaf if it has no children
	bool IsLeaf(const Node& val) {
		for(size_t i = 0;i<KeyCount+1;i++) {
			if(val.children[i]) {
				return false;
			}
		}
		return true;
	}


public:
	MemoryAllocator* allocator;
	BTree(MemoryAllocator* allocator, Reference<Node> root) {
		this->allocator = allocator;
		this->root = root;
	}
	void Insert(T value) {
		//Find a leaf node
		Reference<Node> current = root;
		while(!IsLeaf(current)) {

		}
	}
};


}



#endif /* MEMORYABSTRACTION_H_ */
