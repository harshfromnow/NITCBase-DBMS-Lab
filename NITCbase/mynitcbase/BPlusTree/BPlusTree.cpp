#include "BPlusTree.h"

#include <cstring>

int BPlusTree::numOfComparisons;

/**
 * @brief
 * searches the relation specified to find the next record that satisfies the specified condition
 *
 * The condition value is given by the argument attrVal.
 * This function returns the recId of the next record satisfying the condition.
 * The condition that is checked for is the following.
 *
 * @param relId Relation Id of the relation containing the attribute with index
 * @param attrName Attribute/column name (which has an index) to which condition need to be checked with.
 * @param attrVal value of attribute that has to be checked against the operater
 * @param op Conditional Operator ( can be one among EQ , LE , LT , GE , GT , NE )
 * @return
 * Returns the block number and slot number of the record corresponding to the next hit.
 * Returns {-1,-1} if no next hit.
 */
RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op)
{
  // declare searchIndex which will be used to store search index for attrName.
  IndexId searchIndex;

  // get the search index corresponding to attribute with name attrName
  AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

  // declare variables block and index which will be used during search
  int block, index;

  // search is done for the first time
  if (searchIndex.block == -1 || searchIndex.index == -1)
  {

    // start the search from the first entry of root.
    block = attrCatEntry.rootBlock;
    index = 0;

    if (block == -1)
      return RecId{-1, -1};
  }
  // Not first time
  else
  {
    /*a valid searchIndex points to an entry in the leaf index of the attribute's
    B+ Tree which had previously satisfied the op for the given attrVal.*/

    block = searchIndex.block;
    index = searchIndex.index + 1; // search is resumed from the next index.

    // load block into leaf using IndLeaf::IndLeaf().
    IndLeaf leaf(block);

    // declare leafHead which will be used to hold the header of leaf.
    HeadInfo leafHead;
    leaf.getHeader(&leafHead);

    /*
      All the entries in the block has been searched;
      search from the beginning of the next leaf index block.
    */
    if (index >= leafHead.numEntries)
    {

      // update block to rblock of current block and index to 0.
      block = leafHead.rblock;
      index = 0;
      // end of linked list reached - the search is done.
      if (block == -1)
        return RecId{-1, -1};
    }
  }

  /*
    Traverse down through all the internal nodes according to value of attrVal and the operator op
  */

  /* (This section is only needed when
      - search restarts from the root block (when searchIndex is reset by caller)
      - root is not a leaf
      If there was a valid search index, then we are already at a leaf block
      and the test condition in the following loop will fail)
  */

  while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL)
  {

    // load the block into internalBlk using IndInternal::IndInternal().
    IndInternal internalBlk(block);

    HeadInfo intHead;
    internalBlk.getHeader(&intHead);

    // declare intEntry which will be used to store an entry of internalBlk.
    InternalEntry intEntry;

    if (op == NE || op == LE || op == LT)
    {
      /*
      - NE: need to search the entire linked list of leaf indices of the B+ Tree,
      starting from the leftmost leaf index. Thus, always move to the left.

      - LT and LE: the attribute values are arranged in ascending order in the
      leaf indices of the B+ Tree. Values that satisfy these conditions, if
      any exist, will always be found in the left-most leaf index. Thus,
      always move to the left.
      */

      // load entry in the first slot of the block into intEntry
      internalBlk.getEntry(&intEntry, 0);
      block = intEntry.lChild;
    }
    else
    {
      /*
      - EQ, GT and GE: move to the left child of the first entry that is
      greater than (or equal to) attrVal
      (we are trying to find the first entry that satisfies the condition.
      since the values are in ascending order we move to the left child which
      might contain more entries that satisfy the condition)
      */

      /*
        traverse through all entries of internalBlk and find an entry that
        satisfies the condition.
        if op == EQ or GE, then intEntry.attrVal >= attrVal
        if op == GT, then intEntry.attrVal > attrVal
        Hint: the helper function compareAttrs() can be used for comparing
      */
      int ind = 0;
      while (ind < intHead.numEntries)
      {

        internalBlk.getEntry(&intEntry, ind);
        int ret = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
        BPlusTree::numOfComparisons++;
        if (((op == EQ || op == GE) && ret >= 0) || (op == GT && ret > 0))
          break;

        ind++;
      }
      // such an entry is found - move to the left child of that entry
      if (ind != intHead.numEntries)
      {
        block = intEntry.lChild;
      }
      // such an entry is not found - move to the right child of the last entry of the block
      else
      {
        internalBlk.getEntry(&intEntry, intHead.numEntries - 1);
        block = intEntry.rChild;
      }
    }
  }

  // NOTE: `block` now has the block number of a leaf index block.

  /******  Identify the first leaf index entry from the current position
              that satisfies our condition (moving right)             ******/

  while (block != -1)
  {
    // load the block into leafBlk using IndLeaf::IndLeaf().
    IndLeaf leafBlk(block);
    HeadInfo leafHead;
    leafBlk.getHeader(&leafHead);

    // declare leafEntry which will be used to store an entry from leafBlk
    Index leafEntry;
    while (index < leafHead.numEntries)
    {
      leafBlk.getEntry(&leafEntry, index);

      // int cmpVal = /* comparison between leafEntry's attribute value
      //                 and input attrVal using compareAttrs()*/
      int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
      BPlusTree::numOfComparisons++;

      // (entry satisfying the condition found)
      if (
          (op == EQ && cmpVal == 0) ||
          (op == LE && cmpVal <= 0) ||
          (op == LT && cmpVal < 0) ||
          (op == GT && cmpVal > 0) ||
          (op == GE && cmpVal >= 0) ||
          (op == NE && cmpVal != 0))
      {
        searchIndex.block = block;
        searchIndex.index = index;
        AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
        return RecId{leafEntry.block, leafEntry.slot};
      }
      /*future entries will not satisfy EQ, LE, LT since the values
          are arranged in ascending order in the leaves */
      else if ((op == EQ || op == LE || op == LT) && cmpVal > 0)
      {
        return RecId{-1, -1};
      }
      // search next index.
      ++index;
    }

    /*only for NE operation do we have to check the entire linked list;
    for all the other op it is guaranteed that the block being searched
    will have an entry, if it exists, satisying that op. */
    if (op != NE)
      break;

    // For NE operation
    block = leafHead.rblock;
    index = 0;
  }

  // no entry satisying the op was found; return the recId {-1,-1}
  return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE])
{
  int ret;
  // no index allowed for relation and attribute cat
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
    return E_NOTPERMITTED;

  // fetching attribute catalog entry
  AttrCatEntry attrCatBuf;
  ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);
  if (ret != SUCCESS)
    return ret;

  // checking if index already exists
  if (attrCatBuf.rootBlock != -1)
  {
    return SUCCESS;
  }

  /******Creating a new B+ Tree ******/

  // get a free leaf block using constructor 1 to allocate a new block
  IndLeaf rootBlockBuf;

  // (if the block could not be allocated, the appropriate error code
  //  will be stored in the blockNum member field of the object)

  // declare rootBlock to store the blockNumber of the new leaf block
  int rootBlock = rootBlockBuf.getBlockNum();

  // if there is no more disk space for creating an index
  if (rootBlock == E_DISKFULL)
  {
    return E_DISKFULL;
  }

  // load the relation catalog entry into relCatEntry
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  int block = relCatEntry.firstBlk;

  // update attrCatBuf.rootBlock
  attrCatBuf.rootBlock = rootBlock;
  // set the attrCatEntry using AttrCacheTable::setAttrCatEntry()
  ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatBuf);

  /***** Traverse all the blocks in the relation and insert them one
         by one into the B+ Tree *****/
  while (block != -1)
  {
    // declare a RecBuffer object for `block` (using appropriate constructor)
    RecBuffer recBuf(block);
    unsigned char slotMap[relCatEntry.numSlotsPerBlk];
    recBuf.getSlotMap(slotMap);

    // for every occupied slot of the block
    for (int slot = 0; slot < relCatEntry.numSlotsPerBlk; slot++)
    {
      if (slotMap[slot] == SLOT_OCCUPIED)
      {
        // load the record corresponding to the slot into `record`
        Attribute record[relCatEntry.numAttrs];
        recBuf.getRecord(record, slot);

        // declare recId and store the rec-id of this record in it
        RecId recId{block, slot};

        // insert the attribute value corresponding to attrName from the record
        // into the B+ tree using bPlusInsert.
        ret = bPlusInsert(relId, attrName, record[attrCatBuf.offset], recId);

        // (note that bPlusInsert will destroy any existing bplus tree if
        // insert fails i.e when disk is full)
        // (unable to get enough blocks to build the B+ Tree.)
        if (ret == E_DISKFULL)
          return E_DISKFULL;
      }
      HeadInfo head;
      recBuf.getHeader(&head);
      block = head.rblock;
    }
  }
  return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum)
{
  if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
  {
    return E_OUTOFBOUND;
  }

  int type = StaticBuffer::getStaticBlockType(rootBlockNum);

  if (type == IND_LEAF)
  {
    IndLeaf leafBuffer(rootBlockNum);
    leafBuffer.releaseBlock();
    return SUCCESS;
  }
  else if (type == IND_INTERNAL)
  {
    IndInternal internalBuffer(rootBlockNum);
    HeadInfo intBlockHeader;
    internalBuffer.getHeader(&intBlockHeader);
    /*iterate through all the entries of the internalBlk and destroy the lChild
    of the first entry and rChild of all entries using BPlusTree::bPlusDestroy().
    (the rchild of an entry is the same as the lchild of the next entry.
     take care not to delete overlapping children more than once ) */

    // Deleting left child of first entry
    InternalEntry tempEntry;
    internalBuffer.getEntry(&tempEntry, 0);
    BPlusTree::bPlusDestroy(tempEntry.lChild);

    for (int i = 0; i < intBlockHeader.numEntries; i++)
    {
      InternalEntry tempEntry;
      internalBuffer.getEntry(&tempEntry, i);
      BPlusTree::bPlusDestroy(tempEntry.rChild);
    }
    // release the block using BlockBuffer::releaseBlock().
    internalBuffer.releaseBlock();
    return SUCCESS;
  }
  else
  {
    // (block is not an index block.)
    return E_INVALIDBLOCK;
  }
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId)
{
  // get the attribute cache entry corresponding to attrName
  int ret;
  AttrCatEntry attrCatBuf;
  ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);
  if (ret != SUCCESS)
    return ret;

  int rootblockNum = attrCatBuf.rootBlock;

  if (rootblockNum == -1)
  {
    return E_NOINDEX;
  }

  // find the leaf block to which insertion is to be done using the
  int leafBlkNum = BPlusTree::findLeafToInsert(rootblockNum, attrVal, attrCatBuf.attrType);

  // insert the attrVal and recId to the leaf block at blockNum
  Index entry;
  entry.attrVal = attrVal;
  entry.block = recId.block;
  entry.slot = recId.slot;
  ret = BPlusTree::insertIntoLeaf(relId, attrName, leafBlkNum, entry);
  // NOTE: the insertIntoLeaf() function will propagate the insertion to the
  //       required internal nodes by calling the required helper functions
  //       like insertIntoInternal() or createNewRoot()

  if (ret == E_DISKFULL)
  {
    // destroy the existing B+ tree by passing the rootBlock to bPlusDestroy().
    BPlusTree::bPlusDestroy(rootblockNum);
    // update root block to show index does not exist in attr cache
    attrCatBuf.rootBlock = -1;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatBuf);

    return E_DISKFULL;
  }
  return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType)
{
  int blockNum = rootBlock;
  int type = StaticBuffer::getStaticBlockType(blockNum);

  while (type != IND_LEAF)
  {
    IndInternal blockInternal(blockNum);

    HeadInfo header;
    blockInternal.getHeader(&header);

    /* iterate through all the entries, to find the first entry whose
         attribute value >= value to be inserted.
         NOTE: the helper function compareAttrs() declared in BlockBuffer.h
               can be used to compare two Attribute values. */
    int i;
    for (i = 0; i < header.numEntries; i++)
    {
      InternalEntry entry;
      blockInternal.getEntry(&entry, i);
      if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0)
        break;
    }

    // Rightmost block
    if (i == header.numEntries)
    {
      InternalEntry entry;
      blockInternal.getEntry(&entry, i);
      blockNum = entry.rChild;
    }
    // Left child of block that satisfied
    else
    {
      InternalEntry entry;
      blockInternal.getEntry(&entry, i);
      blockNum = entry.lChild;
    }
    type = StaticBuffer::getStaticBlockType(blockNum);
  }
  return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry)
{
  // get the attribute cache entry corresponding to attrName
  int ret;
  AttrCatEntry attrCatBuf;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);

  // declare an IndLeaf instance for the blockNum found
  IndLeaf leafBlk(blockNum);
  HeadInfo blockHeader;
  leafBlk.getHeader(&blockHeader);

  // the following variable will be used to store a list of index entries with
  // existing indices + the new index to insert
  Index indices[blockHeader.numEntries + 1];

  /*
  Iterate through all the entries in the block and copy them to the array indices.
  Also insert `indexEntry` at appropriate position in the indices array maintaining
  the ascending order.
  - use IndLeaf::getEntry() to get the entry
  - use compareAttrs() declared in BlockBuffer.h to compare two Attribute structs
  */
  bool inserted = false;
  int j = 0;
  for (int i = 0; i < blockHeader.numEntries; i++)
  {
    Index tempEntry;
    leafBlk.getEntry(&tempEntry, i);

    if (compareAttrs(indexEntry.attrVal, tempEntry.attrVal, attrCatBuf.attrType) >= 0)
    {
      indices[j++] = tempEntry;
    }
    else if (!inserted)
    {
      indices[j++] = indexEntry;
      indices[j++] = tempEntry;
      inserted = true;
    }
    else
      indices[j++] = tempEntry;
  }
  if (!inserted)
    indices[blockHeader.numEntries] = indexEntry; // last entry

  // leaf block has not reached max limit
  if (blockHeader.numEntries != MAX_KEYS_LEAF)
  {
    blockHeader.numEntries++;
    leafBlk.setHeader(&blockHeader);

    // iterate through all the entries of the array `indices` and populate the
    // entries of block with them using IndLeaf::setEntry().
    for (int i = 0; i < blockHeader.numEntries; i++)
    {
      leafBlk.setEntry(&indices[i], i);
    }
    return SUCCESS;
  }

  // If we reached here, the `indices` array has more than entries than can fit
  // in a single leaf index block. Therefore, we will need to split the entries
  // in `indices` between two leaf blocks. We do this using the splitLeaf() function.
  // This function will return the blockNum of the newly allocated block or
  // E_DISKFULL if there are no more blocks to be allocated.

  int newRightBlk = splitLeaf(blockNum, indices);
  if (newRightBlk == E_DISKFULL)
    return E_DISKFULL;

  // the current leaf block was not the root
  if (blockHeader.pblock != -1)
  {
    // insert the middle value from `indices` into the parent block using the
    // insertIntoInternal() function. (i.e the last value of the left block)
    InternalEntry tempEntry;
    tempEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
    tempEntry.lChild = blockNum;
    tempEntry.rChild = newRightBlk;

    // the middle value will be at index 31 (given by constant MIDDLE_INDEX_LEAF)
    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, tempEntry);
    if (ret != SUCCESS)
      return ret;
  }
  // the current block was the root block and is now split. a new internal index
  // block needs to be allocated and made the root of the tree.
  else
  {
    ret = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
    if (ret != SUCCESS)
      return ret;
  }
  return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[])
{
  // new leaf index block that will be used as the right block in the splitting
  IndLeaf rightBlk;

  // the existing leaf block constructor 2
  IndLeaf leftBlk(leafBlockNum);

  int rightBlkNum = rightBlk.getBlockNum();
  int leftBlkNum = leafBlockNum;

  if (rightBlkNum == E_DISKFULL)
  {
    return E_DISKFULL;
  }

  HeadInfo leftBlkHeader, rightBlkHeader;
  leftBlk.getHeader(&leftBlkHeader);
  rightBlk.getHeader(&rightBlkHeader);

  // set rightBlkHeader with the following values
  rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2; // 32
  rightBlkHeader.pblock = leftBlkHeader.pblock;
  rightBlkHeader.lblock = leftBlkNum;
  rightBlkHeader.rblock = leftBlkHeader.rblock;
  rightBlk.setHeader(&rightBlkHeader);

  // set leftBlkHeader with the following values
  leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2; // 32
  leftBlkHeader.rblock = rightBlkNum;
  leftBlk.setHeader(&leftBlkHeader);

  // set the first 32 entries of leftBlk = the first 32 entries of indices array
  // and set the first 32 entries of newRightBlk = the next 32 entries of
  // indices array using IndLeaf::setEntry().
  for (int i = 0; i < (MAX_KEYS_LEAF + 1) / 2; i++)
  {
    leftBlk.setEntry(&indices[i], i);
  }

  for (int i = (MAX_KEYS_LEAF + 1) / 2; i <= MAX_KEYS_LEAF; i++)
  {
    rightBlk.setEntry(&indices[i], i - (MAX_KEYS_LEAF + 1) / 2);
  }

  return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry)
{
  int ret;
  AttrCatEntry attrCatBuf;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);

  IndInternal intBlock(intBlockNum);
  HeadInfo blockHeader;
  intBlock.getHeader(&blockHeader);

  // declare internalEntries to store all existing entries + the new entry
  InternalEntry internalEntries[blockHeader.numEntries + 1];

  /*
  Iterate through all the entries in the block and copy them to the array
  `internalEntries`. Insert `indexEntry` at appropriate position in the
  array maintaining the ascending order.
      - use IndInternal::getEntry() to get the entry
      - use compareAttrs() to compare two structs of type Attribute

  Update the lChild of the internalEntry immediately following the newly added
  entry to the rChild of the newly added entry.
  */
  bool inserted = false;
  int j = 0;
  for (int i = 0; i < blockHeader.numEntries; i++)
  {
    InternalEntry tempEntry;
    intBlock.getEntry(&tempEntry, i);

    if (compareAttrs(intEntry.attrVal, tempEntry.attrVal, attrCatBuf.attrType) >= 0)
    {
      internalEntries[j++] = tempEntry;
    }
    else if (!inserted)
    {
      tempEntry.lChild = intEntry.rChild;
      internalEntries[j++] = intEntry;
      internalEntries[j++] = tempEntry;
      inserted = true;
    }
    else
      internalEntries[j++] = tempEntry;
  }
  if (!inserted)
    internalEntries[blockHeader.numEntries] = intEntry;

  if (blockHeader.numEntries != MAX_KEYS_INTERNAL)
  {
    blockHeader.numEntries++;
    intBlock.setHeader(&blockHeader);
    // iterate through all entries in internalEntries array and populate the
    // entries of intBlk with them using IndInternal::setEntry().
    for (int i = 0; i < blockHeader.numEntries; i++)
    {
      intBlock.setEntry(&internalEntries[i], i);
    }
    return SUCCESS;
  }

  // If we reached here, the `internalEntries` array has more than entries than
  // can fit in a single internal index block. Therefore, we will need to split
  // the entries in `internalEntries` between two internal index blocks. We do
  // this using the splitInternal() function.
  // This function will return the blockNum of the newly allocated block or
  // E_DISKFULL if there are no more blocks to be allocated.

  int newRightBlk = splitInternal(intBlockNum, internalEntries);

  if (newRightBlk == E_DISKFULL)
  {

    // Using bPlusDestroy(), destroy the right subtree, rooted at intEntry.rChild.
    // This corresponds to the tree built up till now that has not yet been
    // connected to the existing B+ Tree
    bPlusDestroy(intEntry.rChild);
    return E_DISKFULL;
  }

  if (blockHeader.pblock != -1)
  {
    // insert the middle value from `internalEntries` into the parent block
    // using the insertIntoInternal() function (recursively).
    InternalEntry internEntry;
    internEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
    internEntry.lChild = intBlockNum;
    internEntry.rChild = newRightBlk;

    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, internEntry);
    if (ret != SUCCESS)
      return ret;
  }
  else
  {
    // the current block was the root block and is now split. a new internal index
    // block needs to be allocated and made the root of the tree.
    // To do this, call the createNewRoot() function with the following arguments
    ret = createNewRoot(relId, attrName,
                        internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
                        intBlockNum, newRightBlk);
    if (ret != SUCCESS)
      return ret;
  }
  return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[])
{
  IndInternal rightBlk;
  IndInternal leftBlk(intBlockNum);

  int rightBlkNum = rightBlk.getBlockNum();
  int leftBlkNum = intBlockNum;

  //(failed to obtain a new internal index block because the disk is full)
  if (rightBlkNum == E_DISKFULL)
  {
    return E_DISKFULL;
  }

  // get the headers of left block and right block
  HeadInfo leftBlkHeader, rightBlkHeader;
  rightBlk.getHeader(&rightBlkHeader);
  leftBlk.getHeader(&leftBlkHeader);

  rightBlkHeader.numEntries = (MAX_KEYS_INTERNAL) / 2; // (MAX_KEYS_INTERNAL)/2 = 50
  rightBlkHeader.pblock = leftBlkHeader.pblock;
  rightBlk.setHeader(&rightBlkHeader);

  leftBlkHeader.numEntries = (MAX_KEYS_INTERNAL) / 2; // (MAX_KEYS_INTERNAL)/2 = 50
  leftBlk.setHeader(&leftBlkHeader);

  /*
  - set the first 50 entries of leftBlk = index 0 to 49 of internalEntries
    array
  - set the first 50 entries of newRightBlk = entries from index 51 to 100
    of internalEntries array using IndInternal::setEntry().
    (index 50 will be moving to the parent internal index block)
  */
  for (int i = 0; i < (MAX_KEYS_INTERNAL) / 2; i++)
  {
    leftBlk.setEntry(&internalEntries[i], i);
    rightBlk.setEntry(&internalEntries[i + (MAX_KEYS_INTERNAL) / 2 + 1], i);
  }

  int type = StaticBuffer::getStaticBlockType(internalEntries[0].rChild);

  // Reassigning parents from left Child to Right Child for the entries that got shifted
  for (int i = (MAX_KEYS_INTERNAL) / 2; i <= MAX_KEYS_INTERNAL; i++)
  {
    BlockBuffer child(internalEntries[i].rChild);

    HeadInfo childheader;
    child.getHeader(&childheader);
    childheader.pblock = rightBlkNum;
    child.setHeader(&childheader);
  }
  return rightBlkNum;
}

/// Initializes new rootBlock of type Internal Node. Sets up LChild, RChild pointers and entry.
/// Also sets up the parent pointers for LChild and RChild. Reassigns rootBlock in attrCache.
int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild)
{

  int ret;
  AttrCatEntry attrCatBuf;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);

  IndInternal newRootBlk;

  int newRootBlkNum = newRootBlk.getBlockNum();

  // (failed to obtain an empty internal index block because the disk is full)
  if (newRootBlkNum == E_DISKFULL)
  {
    // Using bPlusDestroy(), destroy the right subtree, rooted at rChild.
    // This corresponds to the tree built up till now that has not yet been
    // connected to the existing B+ Tree
    BPlusTree::bPlusDestroy(rChild);
    return E_DISKFULL;
  }

  // update the header of the new block with numEntries = 1 using
  // BlockBuffer::getHeader() and BlockBuffer::setHeader()
  HeadInfo newBlkHeader;
  newRootBlk.getHeader(&newBlkHeader);
  newBlkHeader.numEntries = 1;
  newRootBlk.setHeader(&newBlkHeader);

  InternalEntry internalEntry;
  internalEntry.lChild = lChild;
  internalEntry.rChild = rChild;
  internalEntry.attrVal = attrVal;
  newRootBlk.setEntry(&internalEntry, 0);

  BlockBuffer lChildBlk(lChild);
  BlockBuffer rChildBlk(rChild);

  HeadInfo lChildHeader, rChildHeader;

  lChildBlk.getHeader(&lChildHeader);
  lChildHeader.pblock = newRootBlkNum;
  lChildBlk.setHeader(&lChildHeader);

  rChildBlk.getHeader(&rChildHeader);
  rChildHeader.pblock = newRootBlkNum;
  rChildBlk.setHeader(&rChildHeader);

  attrCatBuf.rootBlock = newRootBlkNum;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatBuf);

  return SUCCESS;
}