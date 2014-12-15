//
// File:        rm_filehandle.cc
// Description: RM_FileHandle class implementation
// Authors:     Pierre de Boissset (pdeboisset@enst.fr)
//

#include "rm.h"

RM_FileHandle::RM_FileHandle() :pfHandle(NULL), bFileOpen(false), bHdrChanged(false) {}

RC RM_FileHandle::Open(PF_FileHandle* pfh, int size) {
	if (bFileOpen || pfHandle != NULL) {
		return RM_HANDLEOPEN;
	}
	
	if (pfh == NULL || size <= 0) {
		return RM_FCREATEFAIL;
	}
	
	bFileOpen = true;
	pfHandle = new PF_FileHandle;
	*pfHandle = *pfh ;
	PF_PageHandle ph;
	pfHandle->GetThisPage(0, ph);
	// Needs to be called everytime GetThisPage is called.
	pfHandle->UnpinPage(0);
	
	{ // testing
		char * pData;
		ph.GetData(pData);
		RM_FileHdr hdr;
		memcpy(&hdr, pData, sizeof(hdr));
	}
	
	this->GetFileHeader(ph); // write into hdr

	// std::cerr << "RM_FileHandle::Open hdr.numPages" << hdr.numPages << std::endl;
	bHdrChanged = true;
	return 0;
}

RC RM_FileHandle::GetPF_FileHandle(PF_FileHandle &lvalue) const {
	lvalue = *pfHandle;
	return 0;
}

RC RM_FileHandle::GetNextFreeSlot(PF_PageHandle & ph, PageNum& pageNum, SlotNum& slotNum) {
	RM_PageHdr pHdr(this->GetNumSlots());
	RC rc;
	if ((rc= this->GetNextFreePage(pageNum))
		|| (rc = pfHandle->GetThisPage(pageNum, ph))
		|| (rc = pfHandle->UnpinPage(pageNum))
		|| (rc = this->GetPageHeader(ph, pHdr))) {
		return rc;
	}

	bitmap b(pHdr.freeSlotMap, this->GetNumSlots());
	
	for (int i = 0; i < this->GetNumSlots(); i++) {
		if (b.test(i)) {
			slotNum = i;
			return 0;
		}
	}
	// This page is full
	return -1; // unexpected error
}

RC RM_FileHandle::GetNextFreePage(PageNum& pageNum) {
	PF_PageHandle ph;
	RM_PageHdr pHdr(this->GetNumSlots());
	PageNum p;
	if(hdr.firstFree != RM_PAGE_LIST_END) {
	// this last page on the free list might actually be full
	RC rc;
	if ((rc = pfHandle->GetThisPage(hdr.firstFree, ph))
		|| (rc = ph.GetPageNum(p))
		|| (rc = pfHandle->MarkDirty(p))
		// Needs to be called everytime GetThisPage is called.
		|| (rc = pfHandle->UnpinPage(hdr.firstFree))
		|| (rc = this->GetPageHeader(ph, pHdr))) {
		return rc;
		}
	}
	if (hdr.numPages == 0
		|| hdr.firstFree == RM_PAGE_LIST_END
		|| (pHdr.numFreeSlots == 0)) {

		char *pData;
		if ((rc = pfHandle->AllocatePage(ph))
			|| (rc = ph.GetData(pData))
			|| (rc = ph.GetPageNum(pageNum))) {
			return(rc);
		}
		// Add page header
		RM_PageHdr phdr(this->GetNumSlots());
		phdr.nextFree = RM_PAGE_LIST_END;
		bitmap b(this->GetNumSlots());
		b.set(); // Initially all slots are free
		b.to_char_buf(phdr.freeSlotMap, b.numChars());

		phdr.to_buf(pData);

		// the default behavior of the buffer pool is to pin pages
		// let us make sure that we unpin explicitly after setting
		// things up
		if ((rc = pfHandle->UnpinPage(pageNum))) {
			return rc;
		}

		// add page to the free list
		hdr.firstFree = pageNum;
		hdr.numPages++;
		assert(hdr.numPages > 1); // page num 1 would be header page

		bHdrChanged = true;
		return 0; // pageNum is set correctly
	}
	// return existing free page
	pageNum = hdr.firstFree;
	return 0;
}

RC RM_FileHandle::GetPageHeader(PF_PageHandle ph, RM_PageHeader& pHdr) const {
	char * buf;
    RC rc = ph.GetData(buf);
    pHdr.from_buf(buf);
	return rc;
}

RC RM_FileHandle::SetPageHeader(PF_PageHandle ph, const RM_PageHdr& pHdr) {
	char * buf;
	RC rc;
	if((rc = ph.GetData(buf)))
	return rc;
	pHdr.to_buf(buf);
	return 0;
}

// get header from the first page of a newly opened file
RC RM_FileHandle::GetFileHeader(PF_PageHandle ph) {
	char * buf;
	RC rc = ph.GetData(buf);
	memcpy(&hdr, buf, sizeof(hdr));
	return rc;
}

// persist header into the first page of a file for later
RC RM_FileHandle::SetFileHeader(PF_PageHandle ph) const {
	char * buf;
	RC rc = ph.GetData(buf);
	memcpy(buf, &hdr, sizeof(hdr));
	return rc;
}

RC RM_FileHandle::GetSlotPointer(PF_PageHandle ph, SlotNum s, char *& pData) const {
	RC rc = ph.GetData(pData);
	if (rc >= 0 ) {
		bitmap b(this->GetNumSlots());
		pData = pData + (RM_PageHdr(this->GetNumSlots()).size());
		pData = pData + s * this->fullRecordSize();
	}
	return rc;
}

//Gives the number of slots in one page
int RM_FileHandle::GetNumSlots() const{
    //Makes sure RecordSize is > 0
    if(this->getRecordSize()==0){
        return RM_NULLRECORDSIZE;
    }
    //Increments slotsNb until we find the max value
    int slotsNb = 0;
    while(1>0){
        slotsNb++;
        RM_PageHeader pHeader(slotsNb);
        if(pHeader.size()+ slotsNb*this->getRecordSize() > PF_PAGE_SIZE){
            return slotsNb - 1;
        }
        delete pHeader;
    }
}

int RM_FileHandle::GetNumPages() const {
    return this->fileHeader.getPagesNumber();
}

RM_FileHandle::~RM_FileHandle() {
	if(pfHandle != NULL)
	delete pfHandle;
}

// Given a RID, return the record
RC RM_FileHandle::GetRec (const RID &rid, RM_Record &rec) const {

	PageNum p;
	SlotNum s;
	rid.GetPageNum(p);
	rid.GetSlotNum(s);
	RC rc = 0;
	PF_PageHandle ph;
	RM_PageHdr pHdr(this->GetNumSlots());
	if((rc = pfHandle->GetThisPage(p, ph))
		|| (rc = pfHandle->UnpinPage(p))
		|| (rc = this->GetPageHeader(ph, pHdr))) {
		return rc;
	}
	
	bitmap b(pHdr.freeSlotMap, this->GetNumSlots());
	if(b.test(s)) { // already free
		return RM_NORECATRID;
	}
	
	char * pData = NULL;
	if(rc = this->GetSlotPointer(ph, s, pData)) {
		return rc;
	}
	rec.Set(pData, hdr.extRecordSize, rid);
	return 0;
}

RC RM_FileHandle::InsertRec (const char *pData, RID &rid) {
	if(pData == NULL) {
		return RM_NULLRECORD;
	}
	PF_PageHandle ph;
	RM_PageHdr pHdr(this->GetNumSlots());
	PageNum p;
	SlotNum s;
	RC rc;
	char * pSlot;
	if((rc = this->GetNextFreeSlot(ph, p, s))
		|| (rc = this->GetPageHeader(ph, pHdr))) {
		return rc;
	}
	
	bitmap b(pHdr.freeSlotMap, this->GetNumSlots());
	
	if((rc = this->GetSlotPointer(ph, s, pSlot))) {
		return rc;
	}
	
	rid = RID(p, s);
	memcpy(pSlot, pData, this->fullRecordSize());
	b.reset(s); // slot s is no longer free
	pHdr.numFreeSlots--;
	
	if(pHdr.numFreeSlots == 0) {
		// remove from free list
		hdr.firstFree = pHdr.nextFree;
		pHdr.nextFree = RM_PAGE_FULLY_USED;
	}
	
	b.to_char_buf(pHdr.freeSlotMap, b.numChars());
	rc = this->SetPageHeader(ph, pHdr);
	return rc;
}

RC RM_FileHandle::DeleteRec (const RID &rid) {
	PageNum p;
	SlotNum s;
	rid.GetPageNum(p);
	rid.GetSlotNum(s);
	RC rc = 0;
	PF_PageHandle ph;
	RM_PageHdr pHdr(this->GetNumSlots());
	
	if((rc = pfHandle->GetThisPage(p, ph))
		|| (rc = pfHandle->MarkDirty(p))
		|| (rc = pfHandle->UnpinPage(p))
		|| (rc = this->GetPageHeader(ph, pHdr))) {
		return rc;
	}
	
	bitmap b(pHdr.freeSlotMap, this->GetNumSlots());
	
	if(b.test(s)) { // already free
		return RM_NORECATRID;
	}
	
	b.set(s); // s is now free
	if(pHdr.numFreeSlots == 0) {
		// this page used to be full and used to not be on the free list
		// add it to the free list now.
		pHdr.nextFree = hdr.firstFree;
		hdr.firstFree = p;
	}
	
	pHdr.numFreeSlots++;
	
	b.to_char_buf(pHdr.freeSlotMap, b.numChars());
	rc = this->SetPageHeader(ph, pHdr);
	
	return rc;
}

RC RM_FileHandle::UpdateRec (const RM_Record &rec) {
	RID rid;
	rec.GetRid(rid);
	PageNum p;
	SlotNum s;
	rid.GetPageNum(p);
	rid.GetSlotNum(s);
	PF_PageHandle ph;
	char * pSlot;
	RC rc;

	RM_PageHdr pHdr(this->GetNumSlots());

	if ((rc = pfHandle->GetThisPage(p, ph))
		|| (rc = pfHandle->MarkDirty(p))
		|| (rc = pfHandle->UnpinPage(p))
		|| (rc = this->GetPageHeader(ph, pHdr))) {
		return rc;
	}
	
	bitmap b(pHdr.freeSlotMap, this->GetNumSlots());
	
	if(b.test(s)) { // free - cannot update
		return RM_NORECATRID;
	}
	
	char * pData = NULL;
	rec.GetData(pData);
	
	if(RC rc = this->GetSlotPointer(ph, s, pSlot)) {
		return rc;
	}
	
	memcpy(pSlot, pData, this->fullRecordSize());
	
	return rc;
}

//Allows to get the PF_FileHandle attribute
RC RM_FileHandle::GetPF_FileHandle(PF_FileHandle &pf_FileHandle) const
{
  fileHandle = *pfHandle;
  return 0;
}

// Forces a page (along with any contents stored in this class)
// from the buffer pool to disk. Default value forces all pages.
RC RM_FileHandle::ForcePages (PageNum pageNum) {
	return (!this->IsValidPageNum(pageNum) && pageNum != ALL_PAGES) ? RM_BAD_RID : pfHandle->ForcePages(pageNum);
}

//
// IsValidPageNum
//
// Desc: Internal. Return TRUE if pageNum is a valid page number
// in the file, FALSE otherwise
// In: pageNum - page number to test
// Ret: TRUE or FALSE
//
bool RM_FileHandle::IsValidPageNum(const PageNum pageNum) const {
	return (bFileOpen && pageNum >= 0 && pageNum < hdr.numPages);
}

//
// IsValidRID
//
//
bool RM_FileHandle::IsValidRID(const RID rid) const {
	PageNum p;
	SlotNum s;
	rid.GetPageNum(p);
	rid.GetSlotNum(s);
	return IsValidPageNum(p) && s >= 0 && s < this->GetNumSlots();
}

RC RM_FileHandle::IsValid() const {
	if((pfHandle == NULL) || !bFileOpen) {
		return RM_FNOTOPEN;
	}
	
	if(GetNumSlots() <= 0) {
		return RM_RECSIZEMISMATCH;
	}

	return 0;
}

