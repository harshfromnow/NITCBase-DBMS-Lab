#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

// the declarations for these functions can be found in "BlockBuffer.h"

BlockBuffer::BlockBuffer(int blockNum)
{
  // initialise this.blockNum with the argument
  this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blockType)
{
  // allocate a block on the disk and a buffer in memory to hold the new block of
  // given type using getFreeBlock function and get the return error codes if any.
  int blockTypeConst = -1;
  if (blockType == 'R')
    blockTypeConst = REC;
  if (blockType == 'I')
    blockTypeConst = IND_INTERNAL;
  if (blockType == 'L')
    blockTypeConst = IND_LEAF;

  if (blockTypeConst == -1)
  {
    printf("Invalid Block Type\n");

    return;
  }

  int blockNum = BlockBuffer::getFreeBlock(blockTypeConst);

  this->blockNum = blockNum;
}

// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

RecBuffer::RecBuffer() : BlockBuffer('R') {}

// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head)
{
  // read the block at this.blockNum into a buffer
  unsigned char *buffer;
  // Disk::readBlock(buffer, this->blockNum);
  int ret = loadBlockAndGetBufferPtr(&buffer);
  if (ret != SUCCESS)
    return ret;

  // populate the numEntries, numAttrs and numSlots fields in *head
  memcpy(&head->numSlots, buffer + 24, 4);
  memcpy(&head->numEntries, buffer + 16, 4);
  memcpy(&head->numAttrs, buffer + 20, 4);
  memcpy(&head->rblock, buffer + 12, 4);
  memcpy(&head->lblock, buffer + 8, 4);

  return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head)
{

  unsigned char *bufferPtr;
  // get the starting address of the buffer containing the block using
  int ret = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
    return ret;

  // cast bufferPtr to type HeadInfo*
  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;
  // bufferHeader->blockType = head->blockType;
  bufferHeader->lblock = head->lblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->pblock = head->pblock;
  bufferHeader->numAttrs = head->numAttrs;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->numSlots = head->numSlots;

  ret = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret != SUCCESS)
    return ret;
  return SUCCESS;
}
/*
Used to load a block to the buffer and get a pointer to it.
NOTE: this function expects the caller to allocate memory for the argument (is this so?)
  - in the function, it is simply pointing the buffer pointer to already alocated
  memory, thus it does not require the memory allocated
*/

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr)
{
  // check whether the block is already present in the buffer
  // using StaticBuffer.getBufferNum()
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
  if (bufferNum == E_OUTOFBOUND)
    return E_OUTOFBOUND;

  if (bufferNum == E_BLOCKNOTINBUFFER)
  { // the block is not present in the buffer
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    // no free space found in the buffer (currently)
    // or some other error occurred in the process
    if (bufferNum == E_OUTOFBOUND || bufferNum == FAILURE)
      return FAILURE;

    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}

// load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum)
{
  struct HeadInfo head;

  // get the header using this.getHeader() function
  BlockBuffer::getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  // read the block at this.blockNum into a buffer
  unsigned char *buffer;
  // Disk::readBlock(buffer, this->blockNum);
  int ret = loadBlockAndGetBufferPtr(&buffer);
  if (ret != SUCCESS)
    return ret;

  /* record at slotNum will be at offset HEADER_SIZE + slotMapSize + (recordSize * slotNum)
     - each record will have size attrCount * ATTR_SIZE
     - slotMap will be of size slotCount
  */
  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = buffer + (32 + slotCount + (recordSize * slotNum)); /* calculate buffer + offset */
  ;

  // load the record into the rec data structure
  memcpy(rec, slotPointer, recordSize);

  return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *record, int slotNum)
{
  // get the header using this.getHeader() function
  HeadInfo head;
  BlockBuffer::getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  // read the block at this.blockNum into a buffer
  unsigned char *buffer;
  // Disk::readBlock(buffer, this->blockNum);
  int ret = loadBlockAndGetBufferPtr(&buffer);
  if (ret != SUCCESS)
    return ret;

  /* record at slotNum will be at offset HEADER_SIZE + slotMapSize + (recordSize * slotNum)
     - each record will have size attrCount * ATTR_SIZE
     - slotMap will be of size slotCount
  */
  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = buffer + (32 + slotCount + (recordSize * slotNum)); // calculate buffer + offset

  // load the record into the rec data structure
  memcpy(slotPointer, record, recordSize);

  Disk::writeBlock(buffer, this->blockNum);

  return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap)
{
  unsigned char *bufferPtr;

  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }

  struct HeadInfo head;
  // get the header of the block using getHeader() function
  getHeader(&head);

  int slotCount = head.numSlots;

  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  // copy the values from `slotMapInBuffer` to `slotMap` (size is `slotCount`)
  memcpy(slotMap, slotMapInBuffer, slotCount);

  return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap)
{
  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block using
     loadBlockAndGetBufferPtr(&bufferPtr). */
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }

  // get the header of the block using getHeader() function
  HeadInfo header;
  BlockBuffer::getHeader(&header);

  int numSlots = header.numSlots;

  // the slotmap starts at bufferPtr + HEADER_SIZE. Copy the contents of the
  // argument `slotMap` to the buffer replacing the existing slotmap.
  // Note that size of slotmap is `numSlots`
  memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

  // update dirty bit using StaticBuffer::setDirtyBit
  // if setDirtyBit failed, return the value returned by the call
  ret = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret != SUCCESS)
    return ret;
  return SUCCESS;
  // return SUCCESS
}

int BlockBuffer::getFreeBlock(int blockType)
{

  // iterate through the StaticBuffer::blockAllocMap and find the block number
  // of a free block in the disk.
  int blockNum = -1;
  for (int i = 0; i < DISK_BLOCKS; i++)
  {
    if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK)
    {
      blockNum = i;
      break;
    }
  }
  if (blockNum == -1)
    return E_DISKFULL;

  // set the object's blockNum to the block number of the free block.
  this->blockNum = blockNum;

  // find a free buffer using StaticBuffer::getFreeBuffer()
  StaticBuffer::getFreeBuffer(this->blockNum);

  // initialize the header of the block passing a struct HeadInfo with values
  // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0
  // to the setHeader() function.
  struct HeadInfo header;
  header.pblock = -1;
  header.lblock = -1;
  header.rblock = -1;
  header.numAttrs = 0;
  header.numEntries = 0;
  header.numSlots = 0;
  BlockBuffer::setHeader(&header);

  // update the block type of the block to the input block type using setBlockType().
  BlockBuffer::setBlockType(blockType);

  // return block number of the free block.
  return blockNum;
}

int BlockBuffer::setBlockType(int blockType)
{

  unsigned char *bufferPtr;
  int retval = loadBlockAndGetBufferPtr(&bufferPtr);
  if (retval != SUCCESS)
  {
    return retval;
  }

  // store the input block type in the first 4 bytes of the buffer.
  // (hint: cast bufferPtr to int32_t* and then assign it)
  *((int32_t *)bufferPtr) = blockType;

  // update the StaticBuffer::blockAllocMap entry corresponding to the
  // object's block number to `blockType`.
  StaticBuffer::blockAllocMap[this->blockNum] = blockType;

  // update dirty bit by calling StaticBuffer::setDirtyBit()
  retval = StaticBuffer::setDirtyBit(this->blockNum);
  if (retval != SUCCESS)
  {
    return retval;
  }

  return SUCCESS;
}

int BlockBuffer::getBlockNum()
{
  return this->blockNum;
}

void BlockBuffer::releaseBlock()
{

  // if blockNum is INVALID_BLOCKNUM (-1), or it is invalidated already, do nothing
  if (this->blockNum == INVALID_BLOCKNUM || StaticBuffer::blockAllocMap[this->blockNum] == UNUSED_BLK)
    return;

  // Try to get the buffer number if block is loaded in
  int buffNum = StaticBuffer::getBufferNum(this->blockNum);
  if (buffNum != E_BLOCKNOTINBUFFER)
  {
    StaticBuffer::metainfo[buffNum].free = true;
  }

  // Indicate block is free in block allocation map
  StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;
  this->blockNum = -1;
}

// call the corresponding parent constructor - new index block allocated to disk
IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType) {}

// call the corresponding parent constructor - load a block already initialised as disk block
IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum) {}

// new ind internal block allocated to disk
IndInternal::IndInternal() : IndBuffer('I') {}

// load block
IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum) {}

// new leaf index block
IndLeaf::IndLeaf() : IndBuffer('L') {}

// load block - this is the way to call parent non-default constructor.
IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum) {}

// Gives the indexNumth entry of the block.
int IndInternal::getEntry(void *ptr, int indexNum)
{
  if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL)
    return E_OUTOFBOUND;

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
    return ret;

  // typecast the void pointer to an internal entry pointer
  struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

  /*
  - copy the entries from the indexNum`th entry to *internalEntry
  - make sure that each field is copied individually as in the following code
  - the lChild and rChild fields of InternalEntry are of type int32_t
  - int32_t is a type of int that is guaranteed to be 4 bytes across every
    C++ implementation. sizeof(int32_t) = 4
  */

  /* the indexNum'th entry will begin at an offset of
     HEADER_SIZE + (indexNum * (sizeof(int) + ATTR_SIZE) )        
     from bufferPtr */
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

  memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
  memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
  memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

  return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum)
{

  if ( indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL ) return E_OUTOFBOUND;

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if ( ret != SUCCESS ) return ret;

  // copy the indexNum'th Index entry in buffer to memory ptr using memcpy

  /* the indexNum'th entry will begin at an offset of
     HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE)  from bufferPtr */
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
  memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

  return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum)
{
  return 0;
}

int IndLeaf::setEntry(void *ptr, int indexNum)
{
  return 0;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType)
{
  int diff;
  if (attrType == STRING)
    diff = strcmp(attr1.sVal, attr2.sVal);
  else
    diff = attr1.nVal - attr2.nVal;

  if (diff > 0)
    return 1;
  else if (diff < 0)
    return -1;
  else
    return 0;
}