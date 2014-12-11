//
// rm_rid.h
//
//   The Record Id interface
//

#ifndef RM_RID_H
#define RM_RID_H

// We separate the interface of RID from the rest of RM because some
// components will require the use of RID but not the rest of RM.

#include "tinybase.h"

//
// PageNum: uniquely identifies a page in a file
//
typedef int PageNum;

//
// SlotNum: uniquely identifies a record in a page
//
typedef int SlotNum;


//Consts for Page and Slot
static const PageNum NULL_PAGE = -1;
static const SlotNum NULL_SLOT = -1;

//
// RID: Record id interface
//
class RID {
public:
    RID(){                                          // Default constructor
        page = NULL_PAGE;
        slot = NULL_SLOT;
    }
    RID(PageNum pageNum, SlotNum slotNum){
        page = pageNum;
        slot = slotNum;
    }
    ~RID();                                        // Destructor

    RC GetPageNum(PageNum &pageNum) const;         // Return page number
    RC GetSlotNum(SlotNum &slotNum) const;         // Return slot number

    PageNum GetPage(){
        return page;
    }

    SlotNum GetSlot(){
        return slot;
    }

    bool operator==(const RID & rhs) const;

private:
    PageNum page;
    SlotNum slot;
};

#endif
