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
		return pointer;
	}
	void RegisterFreeBlock(uint64_t position, uint64_t sz) {
		int chunkID = (int)log2(sz);
		uint64_t pointer = ReadChunk(chunkID);
		MemoryBlock block;
		block.prev = 0;
		block.size = sz;
		str->Write(position,block);
		if(pointer) {
			//Update linked list
			MemoryBlock other;
			str->Read(pointer,other);
			other.prev = position;
			str->Write(pointer,other);
		}
		//Update root pointer
		WriteChunk(chunkID,position);
	}
	MemoryAllocator(Stream* str, uint64_t& rootPtr) {
		this->str = str;
		uint64_t slen = ReadChunk(0);
		if(slen == 0) {
			slen = str->GetLength();
			WriteChunk(0,slen);
		}
		numberOfChunks = log2(slen)+1;
		rootPtr = ReadChunk(numberOfChunks);
		if(!rootPtr) {
			//Compute space and write free block
			uint64_t freespace = slen-((numberOfChunks+1)*8)-sizeof(MemoryBlock)-32;
			RegisterFreeBlock((numberOfChunks+1)*8,freespace);
		}
	}
	template<typename T>
	Reference<T> Allocate() {
		return Reference<T>(str,AllocateBytes(sizeof(T)));
	}
	template<typename T>
	void Free(const Reference<T>& ptr) {
		int64_t sz = sizeof(T);
		if(sz<sizeof(MemoryBlock)) {
			sz+=sizeof(MemoryBlock);
		}
		RegisterFreeBlock(ptr.offset,sz);
	}
};



}



#endif /* MEMORYABSTRACTION_H_ */
