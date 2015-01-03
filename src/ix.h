//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

// Please do not include any other files than the ones below in this file.

#include "tinybase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"

struct Entry {
	void* value;
	RID* rid;
};

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
public:
	static int Order = 5;
	IX_IndexHandle();
	~IX_IndexHandle();

	// Insert a new index entry
	RC InsertEntry(void *pData, const RID &rid);

	// Delete a new index entry
	RC DeleteEntry(void *pData, const RID &rid);

	// Force index files to disk
	RC ForcePages();
protected:
	/*
	 * It's protected so you can't see it from the outside but we can use
	 * it constructor for internal nodes.
	 * Node type sets the eponymous field defines below.
	 */
	IX_IndexHandle(int NodeType, IX_IndexHandle &Parent);
private:
	int NumElements; // enclosed elements
	/*
	 * -1 means it's the root so it can't have parents and order rule doesn't apply.
	 * 0 means it's a middle node.
	 * 1 means it's a leaf node.
	 */
	int NodeType;
	/*
	 * List within child pointers are stored in.
	 */
	LinkList<void> Pointers;
	/*
	 * List within labels are stored in.
	 */
	void *Labels[];

	/*
	 * Pointer to its parent. It's not public so you don't have to define
	 * it for the root (actually you even can't).
	 */
	IX_IndexHandle &Parent;

	/*
	 * Balance the tree.
	 */
	void Balance();

	// Tests whether fan-out + 1 is acceptable
	bool NumAcceptable(int num) {
		return NodeType > -1 ? true : (Order <= num && num <= 2 * Order);
	}

	RC insert(IX_IndexHandle *nodepointer, Entry entry,
			IX_IndexHandle *newchildentry);
	IX_IndexHandle split();
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
	IX_IndexScan();
	~IX_IndexScan();

	// Open index scan
	RC OpenScan(const IX_IndexHandle &indexHandle, CompOp compOp, void *value,
			ClientHint pinHint = NO_HINT);

	// Get the next matching entry return IX_EOF if no more matching
	// entries.
	RC GetNextEntry(RID &rid);

	// Close index scan
	RC CloseScan();
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
	IX_Manager(PF_Manager &pfm);
	~IX_Manager();

	// Create a new Index
	RC CreateIndex(const char *fileName, int indexNo, AttrType attrType,
			int attrLength);

	// Destroy and Index
	RC DestroyIndex(const char *fileName, int indexNo);

	// Open an Index
	RC OpenIndex(const char *fileName, int indexNo,
			IX_IndexHandle &indexHandle);

	// Close an Index
	RC CloseIndex(IX_IndexHandle &indexHandle);
};

//
// Print-error function
//
void IX_PrintError(RC rc) {
	switch (rc) {
	case 0:
		break;
	default:
		break;
	}
}

#endif
