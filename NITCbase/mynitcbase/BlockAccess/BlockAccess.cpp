#include "BlockAccess.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op)
{
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // let block and slot denote the record id of the record being currently checked
    int block, slot;

    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // (no hits from previous search; search should start from the
        // first record itself)
        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCatEntry relCatBuf;
        RelCacheTable::getRelCatEntry(relId, &relCatBuf);

        block = relCatBuf.firstBlk;
        slot = 0;
    }
    else
    {
        // (there is a hit from previous search; search should start from
        // the record next to the search index record)
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    /* The following code searches for the next record in the relation
       that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining
       records of the relation
    */
    while (block != -1)
    {
        /* create a RecBuffer object for block (use RecBuffer Constructor for
           existing block) */
        RecBuffer recBuffer(block);

        HeadInfo header;
        recBuffer.getHeader(&header);

        Attribute record[header.numAttrs];
        recBuffer.getRecord(record, slot);

        unsigned char slotMap[header.numSlots];
        recBuffer.getSlotMap(slotMap);

        // If slot >= the number of slots per block(i.e. no more slots in this block)
        if (slot >= header.numSlots)
        {
            block = header.rblock;
            slot = 0;
            continue;
        }

        // if slot is free skip the loop
        // (i.e. check if slot'th entry in slot map of block contains SLOT_UNOCCUPIED)
        if (slotMap[slot] == SLOT_UNOCCUPIED)
        {
            slot++;
            continue;
        }

        // compare record's attribute value to the the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using
            AttrCacheTable::getAttrCatEntry()
        */
        /* use the attribute offset to get the value of the attribute from
           current record */
        AttrCatEntry attrCatBuf;
        int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);

        int cmpVal = compareAttrs(record[attrCatBuf.offset], attrVal, attrCatBuf.attrType);

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if (
            (op == NE && cmpVal != 0) || // if op is "not equal to"
            (op == LT && cmpVal < 0) ||  // if op is "less than"
            (op == LE && cmpVal <= 0) || // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) || // if op is "equal to"
            (op == GT && cmpVal > 0) ||  // if op is "greater than"
            (op == GE && cmpVal >= 0)    // if op is "greater than or equal to"
        )
        {
            RecId searchIndex = {block, slot};
            RelCacheTable::setSearchIndex(relId, &searchIndex);
            return searchIndex;
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1, -1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName; // set newRelationName with newName
    strcpy(newRelationName.sVal, newName);

    RecId searchRes = linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, newRelationName, EQ);
    if (searchRes.slot != -1 || searchRes.block != -1)
    {
        return E_RELEXIST;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // search the relation catalog for an entry with "RelName" = oldRelationName
    Attribute oldRelationName;
    strcpy(oldRelationName.sVal, oldName);

    searchRes = linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, oldRelationName, EQ);
    if (searchRes.slot == -1 && searchRes.block == -1)
    {
        return E_RELNOTEXIST;
    }
    /* get the relation catalog record of the relation to rename using a RecBuffer
       on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
    */
    RecBuffer recBuffer(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    int ret = recBuffer.getRecord(relCatRecord, searchRes.slot);
    if (ret != SUCCESS)
    {
        printf("Record not found in RelCat.\n");
        exit(1);
    }
    /* update the relation name attribute in the record with newName.
       (use RELCAT_REL_NAME_INDEX) */
    // set back the record value using RecBuffer.setRecord
    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, newName);

    ret = recBuffer.setRecord(relCatRecord, searchRes.slot);
    if (ret != SUCCESS)
    {
        printf("Relation name could not be changed in RelCat.\n");
        exit(1);
    }
    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */

    // for i = 0 to numberOfAttributes :
    //     linearSearch on the attribute catalog for relName = oldRelationName
    //     get the record using RecBuffer.getRecord
    //
    //     update the relName field in the record to newName
    //     set back the record using RecBuffer.setRecord
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numOfAttr = relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    for (int i = 0; i < numOfAttr; i++)
    {
        searchRes = linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);

        RecBuffer buf(searchRes.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

        ret = buf.getRecord(attrCatRecord, searchRes.slot);
        if (ret != SUCCESS)
        {
            printf("Failed to get record from this slot in ATTRCAT.\n");
            exit(1);
        }

        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);

        ret = buf.setRecord(attrCatRecord, searchRes.slot);
        if (ret != SUCCESS)
        {
            printf("Failed to change record in this slot in ATTRCAT.\n");
            exit(1);
        }
    }
    return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);

    RecId searchRes = linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

    if (searchRes.slot == -1 && searchRes.block == -1)
    {
        return E_RELNOTEXIST;
    }

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    /* declare variable attrToRenameRecId used to store the attr-cat recId
    of the attribute to rename */
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /* iterate over all Attribute Catalog Entry record corresponding to the
       relation to find the required attribute */
    while (true)
    {
        // linear search on the attribute catalog for RelName = relNameAttr
        RecId searchRecId = linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
        if (searchRecId.slot == -1 && searchRecId.block == -1)
        {
            break;
        }
        /* Get the record from the attribute catalog using RecBuffer.getRecord
          into attrCatEntryRecord */
        RecBuffer attrRecord(searchRecId.block);
        attrRecord.getRecord(attrCatEntryRecord, searchRecId.slot);

        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0)
        {
            attrToRenameRecId.block = searchRecId.block;
            attrToRenameRecId.slot = searchRecId.slot;
        }

        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
            return E_ATTREXIST;
    }

    if (attrToRenameRecId.slot == -1 && attrToRenameRecId.block == -1)
    {
        return E_ATTRNOTEXIST;
    }

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /*   declare a RecBuffer for attrToRenameRecId.block and get the record at
         attrToRenameRecId.slot */
    //   update the AttrName of the record with newName
    //   set back the record with RecBuffer.setRecord
    RecBuffer bufferToRename(attrToRenameRecId.block);
    Attribute recordToRename[ATTRCAT_NO_ATTRS];

    bufferToRename.getRecord(recordToRename, attrToRenameRecId.slot);

    memcpy(recordToRename[ATTRCAT_ATTR_NAME_INDEX].sVal, newName, ATTR_SIZE);

    bufferToRename.setRecord(recordToRename, attrToRenameRecId.slot);

    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record)
{
    // get the relation catalog entry from relation cache
    // ( use RelCacheTable::getRelCatEntry() of Cache Layer)
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    int blockNum = relCatEntry.firstBlk;

    // rec_id will be used to store where the new record will be inserted
    RecId rec_id = {-1, -1};

    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;

    int prevBlockNum = -1; // last element

    /*
        Traversing the linked list of existing record blocks of the relation
        until a free slot is found OR
        until the end of the list is reached
    */
    while (blockNum != -1)
    {
        RecBuffer recBuf(blockNum);

        HeadInfo header;
        recBuf.getHeader(&header);

        unsigned char slotMap[numOfSlots];
        recBuf.getSlotMap(slotMap);

        for (int i = 0; i < numOfSlots; i++)
        {
            if (slotMap[i] == SLOT_UNOCCUPIED)
            {
                rec_id.slot = i;
                rec_id.block = blockNum;
                break;
            }
        }
        if (rec_id.block != -1 && rec_id.slot != -1)
            break;
        prevBlockNum = blockNum;
        blockNum = header.rblock;
    }

    //  if no free slot is found in existing record blocks (rec_id = {-1, -1})
    if (rec_id.slot == -1 && rec_id.block == -1)
    {
        if (relId == RELCAT_RELID)
            return E_MAXRELATIONS;

        RecBuffer newRecBuf;
        int newBlockNum = newRecBuf.getBlockNum();
        if (newBlockNum == E_DISKFULL)
        {
            return E_DISKFULL;
        }

        rec_id.block = newBlockNum;
        rec_id.slot = 0;

        HeadInfo newHeader;
        newHeader.blockType = REC;
        newHeader.pblock = -1;
        newHeader.lblock = prevBlockNum;
        newHeader.rblock = -1;
        newHeader.numEntries = 0;
        newHeader.numAttrs = numOfAttributes;
        newHeader.numSlots = numOfSlots;
        newRecBuf.setHeader(&newHeader);

        unsigned char slotMap[numOfSlots];
        for (int i = 0; i < numOfSlots; i++)
        {
            slotMap[i] = SLOT_UNOCCUPIED;
        }
        newRecBuf.setSlotMap(slotMap);

        if (prevBlockNum != -1)
        {
            RecBuffer prevBlock(prevBlockNum);

            HeadInfo prevHeader;
            prevBlock.getHeader(&prevHeader);
            prevHeader.rblock = rec_id.block;
            prevBlock.setHeader(&prevHeader);
        }
        else
        {
            relCatEntry.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatEntry);
        }
        relCatEntry.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }

    // create a RecBuffer object for rec_id.block
    // insert the record into rec_id'th slot using RecBuffer.setRecord())
    RecBuffer block(rec_id.block);
    block.setRecord(record, rec_id.slot);

    /* update the slot map of the block by marking entry of the slot to
       which record was inserted as occupied) */
    // (ie store SLOT_OCCUPIED in free_slot'th entry of slot map)
    // (use RecBuffer::getSlotMap() and RecBuffer::setSlotMap() functions)
    unsigned char slotMap[numOfSlots];
    block.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    block.setSlotMap(slotMap);

    // increment the numEntries field in the header of the block to
    // which record was inserted
    // (use BlockBuffer::getHeader() and BlockBuffer::setHeader() functions)
    HeadInfo header;
    block.getHeader(&header);
    header.numEntries++;
    block.setHeader(&header);
    // Increment the number of records field in the relation cache entry for
    // the relation. (use RelCacheTable::setRelCatEntry function)
    relCatEntry.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    return SUCCESS;
}

/*
NOTE: This function will copy the result of the search to the `record` argument.
      The caller should ensure that space is allocated for `record` array
      based on the number of attributes in the relation.
*/
int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op)
{
    /* search for the record id (recid) corresponding to the attribute with
    attribute name attrName, with value attrval and satisfying the condition op
    using linearSearch() */

    RecId recId;

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS)
        return ret;

    // Linear search if no root block mentioned
    if (attrCatEntry.rootBlock == -1)
        recId = linearSearch(relId, attrName, attrVal, op);
    // Index exists for this attribute
    else
        recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);

    if (recId.slot == -1 && recId.block == -1)
        return E_NOTFOUND;

    // Fetch the required record
    RecBuffer block(recId.block);
    block.getRecord(record, recId.slot);

    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE])
{
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr; // (stores relName as type union Attribute)
    strcpy(relNameAttr.sVal, relName);

    char relnameAttrConst[] = RELCAT_ATTR_RELNAME;

    RecId rec_id = linearSearch(RELCAT_RELID, relnameAttrConst, relNameAttr, EQ);

    if (rec_id.slot == -1 && rec_id.block == -1)
        return E_RELNOTEXIST;

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    /* store the relation catalog record corresponding to the relation in
       relCatEntryRecord using RecBuffer.getRecord */
    RecBuffer relCatBlock(rec_id.block);
    relCatBlock.getRecord(relCatEntryRecord, rec_id.slot);

    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    /*
     Delete all the record blocks of the relation
    */
    while (firstBlock != -1)
    {
        BlockBuffer tempBlock(firstBlock);

        HeadInfo header;
        tempBlock.getHeader(&header);
        firstBlock = header.rblock;

        tempBlock.releaseBlock();
    }
    /***
        Deleting attribute catalog entries corresponding the relation and index
        blocks corresponding to the relation with relName on its attributes
    ***/

    // reset the searchIndex of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    int numberOfAttributesDeleted = 0;

    while (true)
    {
        char relnameAttrInAttrCat[] = ATTRCAT_ATTR_RELNAME;

        RecId attrCatRecId;
        attrCatRecId = linearSearch(ATTRCAT_RELID, relnameAttrInAttrCat, relNameAttr, EQ);

        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1)
            break;

        numberOfAttributesDeleted++;

        // create a RecBuffer for attrCatRecId.block
        // get the header of the block
        // get the record corresponding to attrCatRecId.slot
        RecBuffer attrCatBlock(attrCatRecId.block);

        HeadInfo header;
        attrCatBlock.getHeader(&header);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, attrCatRecId.slot);

        // declare variable rootBlock which will be used to store the root
        // block field from the attribute catalog record.
        int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
        // (This will be used later to delete any indexes if it exists)

        // Update the Slotmap for the block by setting the slot as SLOT_UNOCCUPIED
        unsigned char slotMap[header.numSlots];
        attrCatBlock.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        attrCatBlock.setSlotMap(slotMap);

        /* Decrement the numEntries in the header of the block corresponding to
           the attribute catalog entry and then set back the header
           using RecBuffer.setHeader */
        header.numEntries--;
        attrCatBlock.setHeader(&header);

        /* If number of entries become 0, releaseBlock is called after fixing
           the linked list.
        */
        if (header.numEntries == 0)
        {
            /* Standard Linked List Delete for a Block
               Get the header of the left block and set it's rblock to this
               block's rblock
            */

            int lBlockNum = header.lblock;

            // lBlockNum will never because -1 because the case that number of records in attribute cat
            // becomes zero can never happen because RELCAT and ATTRCAT will have attributes (12)
            if (lBlockNum != -1)
            {
                RecBuffer leftBlock(lBlockNum);

                HeadInfo leftBlockHeader;
                leftBlock.getHeader(&leftBlockHeader);
                leftBlockHeader.rblock = header.rblock; ///////////////////////////////////////////////////////
                leftBlock.setHeader(&leftBlockHeader);
            }

            // Setting the right block to point to the left if right is not -1
            if (header.rblock != -1)
            {
                int rBlockNum = header.rblock;
                RecBuffer rightBlock(rBlockNum);

                HeadInfo rightBlockHeader;
                rightBlock.getHeader(&rightBlockHeader);
                rightBlockHeader.lblock = lBlockNum; ////////////////////////////////////////////////////////
                rightBlock.setHeader(&rightBlockHeader);
            }
            else
            {
                // (the block being released is the "Last Block" of the relation.)
                RelCatEntry attrCatInRelCatEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatInRelCatEntry);
                attrCatInRelCatEntry.lastBlk = lBlockNum;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatInRelCatEntry);
            }

            // (Since the attribute catalog will never be empty(why?), we do not
            //  need to handle the case of the linked list becoming empty - i.e
            //  every block of the attribute catalog gets released.)

            // call releaseBlock()
            attrCatBlock.releaseBlock();
        }

        // (the following part is only relevant once indexing has been implemented)
        // if index exists for the attribute (rootBlock != -1), call bplus destroy
        // if (rootBlock != -1)
        // {
        // delete the bplus tree rooted at rootBlock using BPlusTree::bPlusDestroy()
        //}
    }

    /*** Delete the entry corresponding to the relation from relation catalog ***/
    // Fetch the header of Relcat block
    HeadInfo header;
    relCatBlock.getHeader(&header);
    header.numEntries--;
    relCatBlock.setHeader(&header);

    unsigned char slotMap[header.numSlots];
    relCatBlock.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_UNOCCUPIED;
    relCatBlock.setSlotMap(slotMap);

    /*** Updating the Relation Cache Table ***/
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    relCatEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);

    /** Update attribute catalog entry (number of records in attribute catalog
        is decreased by numberOfAttributesDeleted) **/
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
    relCatEntry.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);

    return SUCCESS;
}

/*
NOTE: the caller is expected to allocate space for the argument `record` based
      on the size of the relation. This function will only copy the result of
      the projection onto the array pointed to by the argument.
*/
int BlockAccess::project(int relId, Attribute *record)
{
    // get the previous search index of the relation relId from the relation
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // declare block and slot which will be used to store the record id of the
    // slot we need to check.
    int block, slot;

    /* if the current search index record is invalid(i.e. = {-1, -1})
       (this only happens when the caller reset the search index)
    */
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else
    {
        // (a project/search operation is already in progress)
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // The following code finds the next record of the relation
    /* Start from the record id (block, slot) and iterate over the remaining
       records of the relation */
    while (block != -1)
    {
        // create a RecBuffer object for block (using appropriate constructor!)
        RecBuffer recBuffer(block);

        HeadInfo header;
        recBuffer.getHeader(&header);

        unsigned char slotMap[header.numSlots];
        recBuffer.getSlotMap(slotMap);

        if (slot >= header.numSlots)
        {
            block = header.rblock;
            slot = 0;
        }
        // (NOTE: if this is the last block, rblock would be -1. this would
        //        set block = -1 and fail the loop condition )

        // No record in this slot
        else if (slotMap[slot] == SLOT_UNOCCUPIED)
            slot++;
        // Next record is found
        else
            break;
    }

    if (block == -1)
    {
        return E_NOTFOUND;
    }

    // declare nextRecId to store the RecId of the record found
    RecId nextRecId{block, slot};

    RelCacheTable::setSearchIndex(relId, &nextRecId);
    /* Copy the record with record id (nextRecId) to the record buffer (record)
       For this Instantiate a RecBuffer class object by passing the recId and
       call the appropriate method to fetch the record
    */
    RecBuffer recBuffer1(nextRecId.block);
    int response = recBuffer1.getRecord(record, nextRecId.slot);
    if (response != SUCCESS)
    {
        printf("Record not found.\n");
        exit(1);
    }

    return SUCCESS;
}