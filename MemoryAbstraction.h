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
	T rval;
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
		if((size_t)ptr == -1) {
			ptr = (unsigned char*)mmap(0,sz,PROT_READ | PROT_WRITE, MAP_SHARED, fd,0);
		}
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
		T val;
		Reference<T> rval = Reference<T>(str,Allocate(sizeof(T)));
		rval = val;
		return rval;
	}
	template<typename T>
	void Free(const Reference<T>& ptr) {
		int64_t sz = sizeof(T);

	}
};

template<typename T>
static size_t BinarySearch(const T* A,size_t arrlen, const T& key, int& insertionMarker) {
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
static void BinaryInsert(T* A, int32_t& len, const T& key) {
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
public:
	class Node {
		public:
			//The length of the keys array
			int length;
			//Keys within this node (plus one auxillary node used for the sole
			//purpose of splitting)
			T keys[KeyCount+1];
			//The child nodes (subtrees)
			uint64_t children[KeyCount+1];
			uint64_t parent;
			Node() {
				parent = 0;
				memset(keys,0,sizeof(keys));
				memset(children,0,sizeof(children));
				length = 0;
			}
		};
	//Node is a leaf if it has no children
		bool IsLeaf(const Node& val) {
			for(size_t i = 0;i<KeyCount+1;i++) {
				if(val.children[i]) {
					return false;
				}
			}
			return true;
		}
	Reference<Node> root;
	MemoryAllocator* allocator;
	BTree(MemoryAllocator* allocator, Reference<Node> root) {
		this->allocator = allocator;
		this->root = root;
	}
	bool Find(T& value, Reference<Node> root) {
			Node current = root;
		while(true) {
			int marker;
			if(BinarySearch(current.keys,current.length,value,marker) !=-1) {
				value = current.keys[marker];
				return true;
			}
			//Are there sub-trees?
			if(IsLeaf(current)) {
				return false;
			}
			//Find the appropriate sub-tree and traverse it
			if(value<current.keys[marker]) {
				//Take the left sub-tree
				current = Reference<Node>(allocator->str,current.children[marker]);
			}else {
				//Take the right sub-tree
				current = Reference<Node>(allocator->str,current.children[marker+1]);
			}
		}
		return false;
	}
	bool Find(T& value) {
		return Find(value,root);
	}
	void Insert(T value, Reference<Node> root) {
		//Find a leaf node
		Reference<Node> current = root;
		while(!IsLeaf(current)) {
			//Scan for the insertion point
			Node node = current;
			int marker;
			BinarySearch(node.keys,node.length,value,marker);
			if(value<node.keys[marker]) {
				//Proceed along the left sub-tree
				current = Reference<Node>(allocator->str,node.children[marker]);

			}else {
				//Proceed along the right sub-tree
				current = Reference<Node>(allocator->str,node.children[marker+1]);
			}
		}
		Node node = current;
		//We've found a leaf!
		if(node.length<KeyCount) {
			//We can just do the insertion
			BinaryInsert(node.keys,node.length,value);
			//Update the file
			current = node;
			return;
		}
		//Full node -- needs split; don't keep on disk anymore
		allocator->Free(current);

		//The node is full. Insert the new value and then split this node
		BinaryInsert(node.keys,node.length,value);
		int medianIdx = node.length/2;
		T medianValue = node.keys[medianIdx];
		//Values less than median go in left, greater than go in right node (new nodes allocated)
		Reference<Node> leftPtr = allocator->Allocate<Node>();
		Reference<Node> rightPtr = allocator->Allocate<Node>();
		Node left = leftPtr;
		Node right = rightPtr;
		left.length = medianIdx;
		right.length = medianIdx;
		//Perform copy
		memcpy(left.keys,node.keys,medianIdx);
		memcpy(right.keys,node.keys+medianIdx+1,medianIdx);
		left.parent = node.parent;
		right.parent = node.parent;
		//Insert the left and right trees into the parent node
		if(node.parent == 0) {
			//We are at the root, add a new root above our node and add us as a child
			Reference<Node> newRoot = allocator->Allocate<Node>();
			this->root = newRoot;
			left.parent = newRoot.offset;
			right.parent = newRoot.offset;
		}
			Reference<Node> parentPtr = Reference<Node>(allocator->str,left.parent);
			Node parent = parentPtr;
			//Insert the median into the parent (which may cause it to split as well)
			Insert(medianValue,parentPtr);
			//Refresh the parent
			parent = parentPtr;
			//Make left and right sub-trees children of parent
			//Compute the insertion marker for the left sub-tree
			int marker;
			BinarySearch(parent.keys,parent.length,left.keys[0],marker);
			if(left.keys[0]<parent.keys[marker]) {
				//Insert to left of key
				parent.children[marker] = leftPtr.offset;
			}else {
				//Insert to right of key
				parent.children[marker+1] = leftPtr.offset;
			}
			BinarySearch(parent.keys,parent.length,right.keys[0],marker);
			if (right.keys[0] < parent.keys[marker]) {
				//Insert to left of key
				parent.children[marker] = rightPtr.offset;
			} else {
				//Insert to right of key
				parent.children[marker + 1] = rightPtr.offset;
			}
			//Update nodes
			parentPtr = parent;
			leftPtr = left;
			rightPtr = right;

		}
	void Insert(const T& val) {
		Insert(val,root);
	}

};


}



#endif /* MEMORYABSTRACTION_H_ */
