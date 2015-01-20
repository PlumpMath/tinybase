#include <cstdio>
#include <unistd.h>
#include <iostream>
#include "ix.h"
#include <string.h>
#include <sstream>

using namespace std;

IX_Manager::IX_Manager(PF_Manager &pfm) {
    this->pfManager = &pfm;
}
IX_Manager::~IX_Manager() {
}

// Create a new Index
RC IX_Manager::CreateIndex(const char *fileName, int indexNo, AttrType attrType,
		int attrLength) {
    RC rc = 0;

    //Sanity check: fileName must not be NULL
    if(fileName==NULL){
        return IX_NULLPOINTER;
    }

    //Sanity check on attrType/attrLength
    if(attrType==INT && attrLength!=sizeof(int)) return IX_INVALIDATTR;
    if(attrType==FLOAT && attrLength!=sizeof(float)) return IX_INVALIDATTR;

    //Creates file with PF
    stringstream fileNameForPF;
    fileNameForPF << fileName << "." << indexNo;
    if( (rc = pfManager->CreateFile(fileNameForPF.str().c_str())) ) return rc;

    //Opens it
    PF_FileHandle fileHandle;
    if( (rc = pfManager->OpenFile(fileNameForPF.str().c_str(), fileHandle)) ) return rc;

    //Creates IX_FileHeader for the file
    IX_FileHeader fileHeader;
    fileHeader.attrType = attrType;
    fileHeader.keySize = attrLength;
    fileHeader.rootNb = -1;
    fileHeader.sizePointer = sizeof(PageNum);

    //The number of keys in nodes will be up to order*2 (+ the -1 pointer in intl nodes)
    fileHeader.order = (PF_PAGE_SIZE-sizeof(IX_NodeHeader)-fileHeader.sizePointer)/(fileHeader.sizePointer+fileHeader.keySize);
    fileHeader.order = fileHeader.order >= 5 ? 5 : fileHeader.order; //Limits to 5 the order

    //We make sure an internal node can at least contain one value (+ two pointers)
    if( ((sizeof(IX_NodeHeader)+attrLength+fileHeader.sizePointer*2)>PF_PAGE_SIZE)){
        return IX_PAGELIMIT;
    }
    //We don't need to check for leaf nodes because it's the same with one less pointer
    //We make sure a bucket can at least contain one RID
    if(sizeof(IX_BucketHeader)+sizeof(RID)>PF_PAGE_SIZE){
        return IX_PAGELIMIT;
    }

    //Allocates page for the header
    PF_PageHandle pageHandle;
    if( (rc = fileHandle.AllocatePage(pageHandle)) ) return rc;
    char * pData;
    if( (rc = pageHandle.GetData(pData)) ) return rc;

    //And writes fileHeader to it
    memcpy(pData, &fileHeader, sizeof(IX_FileHeader));
    PageNum nb;
    if( (rc = pageHandle.GetPageNum(nb)) || (rc = fileHandle.MarkDirty(nb))
            || (rc = fileHandle.UnpinPage(nb)) || (rc = fileHandle.ForcePages()) ) return rc;

    return pfManager->CloseFile(fileHandle);
}

// Destroy and Index
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo) {

    //Sanity check: fileName must not be NULL
    if(fileName==NULL){
        return IX_NULLPOINTER;
    }

    //The filename
    stringstream fileNameForPF;
    fileNameForPF << fileName << "." << indexNo;

    //Calls DestroyFile in PF
    return pfManager->DestroyFile(fileNameForPF.str().c_str());
}

// Open an Index
RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
		IX_IndexHandle &indexHandle) {
    RC rc = 0;

    //Sanity check: fileName must not be NULL
    if(fileName==NULL){
        return IX_NULLPOINTER;
    }

    //The filename
    stringstream fileNameForPF;
    fileNameForPF << fileName << "." << indexNo;

    //Checks not already open
    if(indexHandle.bFileOpen) return IX_INDEXOPEN;

    //Opens in PF
    PF_FileHandle fileHandle;
    if( (rc = pfManager->OpenFile(fileNameForPF.str().c_str(), fileHandle)) ) return rc;

    //Marks open
    indexHandle.bFileOpen = true;
    indexHandle.filehandle = new PF_FileHandle(fileHandle);

    //Instanciate fileHeader
    PF_PageHandle pageHandle;
    if( (rc = fileHandle.GetThisPage(0, pageHandle)) || (rc = fileHandle.UnpinPage(0)) ) return rc;
    char* pData;
    if( (rc = pageHandle.GetData(pData)) ) return rc;
    memcpy( &(indexHandle.fh), pData, sizeof(IX_FileHeader)); //Copies from memory

    return 0;
}

// Close an Index
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle) {
    RC rc = 0;
    //Already closed?
    if(!indexHandle.bFileOpen) return IX_CLOSEDFILE;

    //We have to write the file header back
    char * pData ;
    PF_PageHandle pageHandle;
    if( (rc = indexHandle.filehandle->GetThisPage(0, pageHandle)) ) return rc;
    if( (rc = pageHandle.GetData(pData)) ) return rc;

    memcpy( pData, &(indexHandle.fh), sizeof(IX_FileHeader)); //Copies to memory

    if( (rc = indexHandle.filehandle->MarkDirty(0))
            || (rc = indexHandle.filehandle->UnpinPage(0))
            || (rc = indexHandle.filehandle->ForcePages(0))
            || (rc = this->pfManager->CloseFile(*(indexHandle.filehandle))) ) return rc;

    indexHandle.bFileOpen = false;

    return 0;
}
