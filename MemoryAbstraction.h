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
		if(available<0) {
			available = count;
		}
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
		//Move everything at location to the right
		memmove(A+location+1,A+location,(len-location)*sizeof(T));
		A[location] = key;
	}
	len++;
}


//Represents a B-tree
template<typename T, uint64_t KeyCount = 1024>
class BTree {
public:
	class Key {
	public:
		T val;
		uint64_t left;
		uint64_t right;
		bool operator<(const Key& other) const {
			return val<other.val;
		}
		bool operator<(const T& other) const {
			return val<other;
		}
		bool operator==(const T& other) const {
			return val == other;
		}
		bool operator==(const Key& other) const {
			return val == other.val;
		}
		operator T&() {
			//TODO: Warning -- potentially unsafe
			return val;
		}
		Key() {
			left = 0;
			right = 0;
		}
		Key(const T& val) {
			this->val = val;
			left = 0;
			right = 0;
		}
	};
	class Node {
		public:
			//The length of the keys array
			int length;
			//Keys within this node (plus one auxillary node used for the sole
			//purpose of splitting)
			Key keys[KeyCount+1];
			uint64_t parent;
			Node() {
				parent = 0;
				memset(keys,0,sizeof(keys));
				length = 0;
			}
		};
	//Node is a leaf if it has no children
		bool IsLeaf(const Node& val) {
			for(size_t i = 0;i<KeyCount+1;i++) {
				if(val.keys[i].left || val.keys[i].right) {
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
	BTree(MemoryAllocator* allocator, uint64_t rootPtr) {
		this->allocator = allocator;
		if(rootPtr !=0) {
		this->root = Reference<Node>(allocator->str,rootPtr);
		}else {
			//Allocate our root node
			this->root = allocator->Allocate<Node>();
		}
	}
	BTree() {
		this->allocator = 0;
	}
	bool Find(T& value, Reference<Node>& nodeptr, int& keyIndex) {
		nodeptr = root;
		Node current = root;
		int& marker = keyIndex;
		int offset = 0;
		while(true) {
			Key mkey = value;
			if(BinarySearch(current.keys+offset,current.length-offset,mkey,marker) !=-1) {
				value = current.keys[marker+offset];
				marker = marker+offset;
				return true;
			}
			//Are there sub-trees?
			if(IsLeaf(current)) {
				marker = marker+offset;
				return false;
			}
			//Find the appropriate sub-tree and traverse it
			if(value<current.keys[marker+offset]) {
				//Take the left sub-tree
				nodeptr = Reference<Node>(allocator->str,current.keys[marker+offset].left);
				current = nodeptr;
			}else {
				//Take the right sub-tree
				nodeptr = Reference<Node>(allocator->str,current.keys[marker+offset].right);
				current = nodeptr;
			}
		}
		marker = marker+offset;
		return false;
	}
	//Finds the first element matching the specified key
	bool Find(T& value) {
		Reference<Node> nodeptr;
		int keyIndex;
		return Find(value,nodeptr,keyIndex);
	}
	void Insert(T value, Reference<Node> root, bool treatAsLeaf = false) {
		//Find a leaf node
		Reference<Node> current = root;
		while(!IsLeaf(current)) {
			if(treatAsLeaf) {
				break;
			}
			//Scan for the insertion point
			Node node = current;
			int marker;
			Key mkey = value;
			BinarySearch(node.keys,node.length,mkey,marker);
			value = mkey;
			if(value<node.keys[marker]) {
				//Proceed along the left sub-tree
				current = Reference<Node>(allocator->str,node.keys[marker].left);
			}else {
				//Proceed along the right sub-tree
				current = Reference<Node>(allocator->str,node.keys[marker].right);
			}
		}
		Node node = current;
		//We've found a leaf!
		if(node.length<KeyCount) {
			//We can just do the insertion
			Key mkey = value;
			BinaryInsert(node.keys,node.length,mkey);
			//Update the file
			current = node;
			return;
		}
		//Full node -- needs split; don't keep on disk anymore
		allocator->Free(current);

		//The node is full. Insert the new value and then split this node
		Key mkey = value;
		BinaryInsert(node.keys,node.length,mkey);
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
		memcpy(left.keys,node.keys,medianIdx*sizeof(T));
		memcpy(right.keys,node.keys+medianIdx+1,medianIdx*sizeof(T));
		left.parent = node.parent;
		right.parent = node.parent;
		//Insert the left and right trees into the parent node
		if(node.parent == 0) {
			//We are at the root, add a new root, with the left and right nodes as children.
			Reference<Node> newRoot = allocator->Allocate<Node>();
			Node nroot = newRoot;
			this->root = newRoot;
			left.parent = newRoot.offset;
			right.parent = newRoot.offset;
			node.parent = newRoot.offset;
			nroot.keys[0] = medianValue;
			nroot.keys[0].left = leftPtr.offset;
			nroot.keys[0].right = rightPtr.offset;
			nroot.length = 1;
			//Update root
			newRoot = nroot;
			//Update left
			leftPtr = left;
			//Update right
			rightPtr = right;
			return;
		}
			Reference<Node> parentPtr = Reference<Node>(allocator->str,left.parent);
			Node parent = parentPtr;
			//Insert the median into the parent (which may cause it to split as well, TODO causing memory)
			//addresses of parent to be possibly invalidated.
			Insert(medianValue,parentPtr,true);
			//TODO: Is there a more efficient way of doing this rather than a full traversal of the tree?
			int tkey;
			bool found = Find(medianValue,parentPtr,tkey);
			if(!found) {
				throw "up";
			}
			//Refresh the parent
			parent = parentPtr;
			//Make left and right sub-trees children of parent
			//Compute the insertion marker for the left sub-tree
			int marker;
			BinarySearch(parent.keys,parent.length,left.keys[0],marker);
			if(left.keys[0]<parent.keys[marker]) {
				//Insert to left of key
				parent.keys[marker].left = leftPtr.offset;
			}else {
				//Insert to right of key
				parent.keys[marker].right = leftPtr.offset;
			}
			BinarySearch(parent.keys,parent.length,right.keys[0],marker);
			if (right.keys[0] < parent.keys[marker]) {
				//Insert to left of key
				parent.keys[marker].left = rightPtr.offset;
			} else {
				//Insert to right of key
				parent.keys[marker].right = rightPtr.offset;
			}
			//Update nodes
			parentPtr = parent;
			leftPtr = left;
			rightPtr = right;

		}
	void Insert(const T& val) {
		Insert(val,root);
	}



	//EXPERIMENTAL DELETE


	bool Delete(const T& value) {
		Reference<Node> nodeptr;
		int keyIndex;
		T val = value;
		if(Find(val,nodeptr,keyIndex)) {
			Node node = nodeptr;
			//If we are a leaf, there is nothing complex that needs to be done here
			//the element can simply be removed.
			if(IsLeaf(nodeptr)) {
				//Remove element from node
				node.length--;
				memmove(node.keys+keyIndex,node.keys+keyIndex+1,(node.length-keyIndex)*sizeof(T));
			}else {
				//Removing the element is a bit more complicated
				//Sub-trees will need to be adjusted
				//TODO: Implement
				throw "up";
			}
			if(node.length == 0) {
				//We don't have enough keys.
				//TODO: Implement
				throw "up";
			}


			//Write changes to disk
			nodeptr = node;
			return true;
		}
		return false;
	}
	//END EXPERIMENTAL DELETE





	//Helper functions
	bool Update(const T& value) {
		Reference<Node> nodeptr;
		int keyIndex;
		T val = value;
		if(Find(val,nodeptr,keyIndex)) {
			Node node = nodeptr;
			node.keys[keyIndex] = value;
			nodeptr = node;
			return true;
		}
		return false;
	}
	template<typename F>
	void Traverse(Node root, const F& callback) {
		for(int i = 0;i<root.length;i++) {
			//Traverse left sub-tree if exists
			if(root.keys[i].left) {
				Traverse(Reference<Node>(allocator->str,root.keys[i].left),callback);
			}
			//Callback
			callback(root.keys[i].val);
			//Traverse right sub-tree if exists
			if(root.keys[i].right) {
				Traverse(Reference<Node>(allocator->str,root.keys[i].right),callback);
			}
		}

	}
	template<typename F>
	void Traverse(const F& callback) {
		Traverse(root,callback);
	}
	//End helper functions


};



}



#endif /* MEMORYABSTRACTION_H_ */
