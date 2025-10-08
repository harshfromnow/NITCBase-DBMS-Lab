#include "OpenRelTable.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

void clearList(AttrCacheEntry *head)
{
  for (AttrCacheEntry *it = head, *next; it != nullptr; it = next)
  {
    next = it->next;
    free(it);
  }
}

OpenRelTable::OpenRelTable()
{

  // initialize relCache and attrCache with nullptr and tableMetaInfo to be free
  for (int i = 0; i < MAX_OPEN; ++i)
  {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    this->tableMetaInfo[i].free = true;
  }

  /************ Setting up Relation Cache entries ************/
  // (we need to populate relation cache with entries for the relation catalog
  //  and attribute catalog.)

  /**** setting up Relation Catalog relation in the Relation Cache Table****/
  RecBuffer relCatBlock(RELCAT_BLOCK);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  // set the value at RelCacheTable::relCache[ATTRCAT_RELID]
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;

  // Setting up the Students Relation in the Relation Catalogue Table
  // relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT + 1);

  // // set the value at RelCacheTable::relCache[ATTRCAT_RELID]
  // RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  // relCacheEntry.recId.block = RELCAT_BLOCK;
  // relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT + 1;

  // // allocate this on the heap because we want it to persist outside this function
  // RelCacheTable::relCache[ATTRCAT_RELID + 1] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  // *(RelCacheTable::relCache[ATTRCAT_RELID + 1]) = relCacheEntry;

  /************ Setting up Attribute cache entries ************/

  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[RELCAT_NO_ATTRS];

  AttrCacheEntry *listHead = nullptr;
  AttrCacheEntry *attrCacheEntry;
  AttrCacheEntry *prev = nullptr;

  for (int j = 0; j < RELCAT_NO_ATTRS; j++)
  {
    attrCatBlock.getRecord(attrCatRecord, j);
    attrCacheEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    if (j == 0)
      listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = j;

    if (prev != nullptr)
      prev->next = attrCacheEntry;
    prev = attrCacheEntry;
  }
  attrCacheEntry->next = nullptr;

  AttrCacheTable::attrCache[RELCAT_RELID] = listHead;

  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/
  prev = nullptr;

  for (int j = 6; j < RELCAT_NO_ATTRS + 6; j++)
  {
    attrCatBlock.getRecord(attrCatRecord, j);
    attrCacheEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    if (j == 6)
      listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = j;

    if (prev != nullptr)
      prev->next = attrCacheEntry;
    prev = attrCacheEntry;
  }
  attrCacheEntry->next = nullptr;
  AttrCacheTable::attrCache[ATTRCAT_RELID] = listHead;

  /**** setting up Student relation in the Attribute Cache Table ****/
  // prev = nullptr;

  // for (int j = 12; j < 16 ; j++ ) {
  //   attrCatBlock.getRecord(attrCatRecord, j);
  //   attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
  //   if ( j == 12 ) listHead = attrCacheEntry;

  //   AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry );
  //   attrCacheEntry->recId.block = ATTRCAT_BLOCK;
  //   attrCacheEntry->recId.slot = j;

  //   if ( prev!= nullptr ) prev->next = attrCacheEntry;
  //   prev = attrCacheEntry;
  // }
  // attrCacheEntry -> next = nullptr;
  // AttrCacheTable::attrCache[ATTRCAT_RELID + 1] = listHead;

  /************ Setting up tableMetaInfo entries ************/

  // setup meta info for RELCAT and ATTRCAT
  this->tableMetaInfo[RELCAT_RELID].free = false;
  strcpy(this->tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

  this->tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(this->tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{
  /* traverse through the tableMetaInfo array,
    find the entry in the Open Relation Table corresponding to relName.*/
  for (int i = 0; i < MAX_OPEN; ++i)
  {
    if (strcmp(tableMetaInfo[i].relName, relName) == 0)
      return i;
  }
  return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry()
{

  // traverse through the tableMetaInfo array, find a free entry in the Open Relation Table.
  for (int i = 2; i < MAX_OPEN; ++i)
  {
    if (tableMetaInfo[i].free)
      return i;
  }
  return E_CACHEFULL;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE])
{

  int tempRelId = getRelId(relName);
  if (tempRelId != E_RELNOTOPEN)
  {
    return tempRelId;
  }

  tempRelId = getFreeOpenRelTableEntry();

  if (tempRelId == E_CACHEFULL)
  {
    return E_CACHEFULL;
  }

  // let relId be used to store the free slot.
  int relId = tempRelId;

  /****** Setting up Relation Cache entry for the relation ******/

  /* search for the entry with relation name, relName, in the Relation Catalog using
      BlockAccess::linearSearch().
      Care should be taken to reset the searchIndex of the relation RELCAT_RELID
      before calling linearSearch().*/

  // relcatRecId stores the rec-id of the relation `relName` in the Relation Catalog.
  RecId relcatRecId;

  char relNameAttrInRELCAT[] = RELCAT_ATTR_RELNAME;
  Attribute relNameAttr;

  strcpy(relNameAttr.sVal, relName);

  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, relNameAttrInRELCAT, relNameAttr, EQ);

  if (relcatRecId.block == -1 && relcatRecId.slot == -1)
  {
    return E_RELNOTEXIST;
  }

  RecBuffer relCatBlock(RELCAT_BLOCK);
  Attribute relCatRecord[RELCAT_NO_ATTRS];

  relCatBlock.getRecord(relCatRecord, relcatRecId.slot);

  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = relcatRecId.slot;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[relId] = (struct RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[relId]) = relCacheEntry;

  // (the relation is not found in the Relation Catalog.)

  /* read the record entry corresponding to relcatRecId and create a relCacheEntry
      on it using RecBuffer::getRecord() and RelCacheTable::recordToRelCatEntry().
      update the recId field of this Relation Cache entry to relcatRecId.
      use the Relation Cache entry to set the relId-th entry of the RelCacheTable.
    NOTE: make sure to allocate memory for the RelCacheEntry using malloc()
  */

  /****** Setting up Attribute Cache entry for the relation ******/

  AttrCacheEntry *listHead = nullptr;
  AttrCacheEntry *attrCacheEntry;
  AttrCacheEntry *prev = nullptr;

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
  RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relNameAttrInRELCAT, relNameAttr, EQ);

  for (int i = 0; i < relCacheEntry.relCatEntry.numAttrs; i++)
  {

    RecBuffer attrCatBlock(attrcatRecId.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);
    attrCacheEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    if (i == 0)
      listHead = attrCacheEntry;

    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = attrcatRecId.block;
    attrCacheEntry->recId.slot = attrcatRecId.slot;
    // attrCacheEntry->dirty = false;

    if (prev != nullptr)
      prev->next = attrCacheEntry;
    prev = attrCacheEntry;

    attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, relNameAttrInRELCAT, relNameAttr, EQ);
  }
  attrCacheEntry->next = nullptr;

  AttrCacheTable::attrCache[relId] = listHead;

  // set the relIdth entry of the AttrCacheTable to listHead.

  /****** Setting up metadata in the Open Relation Table for the relation******/

  // update the relIdth entry of the tableMetaInfo with free as false and relName as the input.
  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);
  printf("Relid: %d\n", relId);
  return relId;
}

int OpenRelTable::closeRel(int relId)
{
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
  {
    return E_NOTPERMITTED;
  }

  if (relId < 0 || relId >= MAX_OPEN)
  {
    return E_OUTOFBOUND;
  }

  if (tableMetaInfo[relId].free)
  {
    return E_RELNOTOPEN;
  }
  /****** Releasing the Relation Cache entry of the relation ******/

  if (RelCacheTable::relCache[relId]->dirty == true)
  {

    /* Get the Relation Catalog entry from RelCacheTable::relCache*/
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    RelCatEntry relCatEntry = RelCacheTable::relCache[relId]->relCatEntry;
    RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

    // declaring an object of RecBuffer class to write back to the buffer
    RecBuffer relCatBlock(RelCacheTable::relCache[relId]->recId.block);
    relCatBlock.setRecord(relCatRecord, RelCacheTable::relCache[relId]->recId.slot);
  }

  /****** Releasing the Attribute Cache entry of the relation ******/

  for (AttrCacheEntry *curEntry = AttrCacheTable::attrCache[relId];
       curEntry != nullptr;
       curEntry = curEntry->next)
  {
    if (curEntry->dirty == true)
    {
      // get the attrCatEntry
      AttrCatEntry attrCatEntry = curEntry->attrCatEntry;
      union Attribute record[ATTRCAT_NO_ATTRS];

      // Convert into record from struct to write back
      AttrCacheTable::attrCatEntryToRecord(&attrCatEntry, record);

      // Write back to buffer from cache
      RecBuffer attrCatBlk(curEntry->recId.block);
      attrCatBlk.setRecord(record, curEntry->recId.slot);
    }
  }

  // free the memory allocated in the relation and attribute caches which was
  // allocated in the OpenRelTable::openRel() function

  tableMetaInfo[relId].free = true;
  strcpy(tableMetaInfo[relId].relName, "");
  RelCacheTable::relCache[relId] = nullptr;
  AttrCacheTable::attrCache[relId] = nullptr;
  return SUCCESS;
}

OpenRelTable::~OpenRelTable()
{

  // close all open relations from rel-id = 2 onwards.
  for (int i = 2; i < MAX_OPEN; ++i)
  {
    if (!tableMetaInfo[i].free)
    {
      OpenRelTable::closeRel(i);
    }
  }
  /**** Closing the catalog relations in the relation cache ****/

  // releasing the relation cache entry of the attribute catalog
  if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty == true)
  {

    Attribute attrCatInRelCatRecord[RELCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry,
                                       attrCatInRelCatRecord);

    // declaring an object of RecBuffer class to write back to the buffer
    RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;

    // Writing the record back to the block
    RecBuffer relCatBlock(recId.block);
    relCatBlock.setRecord(attrCatInRelCatRecord, recId.slot);
  }
  // free the memory dynamically allocated to this RelCacheEntry
  free(RelCacheTable::relCache[ATTRCAT_RELID]);

  // releasing the relation cache entry of the relation catalog
  if (RelCacheTable::relCache[RELCAT_RELID]->dirty == true)
  {

    Attribute relCatInRelCatRecord[RELCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&RelCacheTable::relCache[RELCAT_RELID]->relCatEntry,
                                       relCatInRelCatRecord);

    RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;
    // declaring an object of RecBuffer class to write back to the buffer
    RecBuffer relCatBlock(recId.block);

    // Write back to the buffer using relCatBlock.setRecord() with recId.slot
    relCatBlock.setRecord(relCatInRelCatRecord, recId.slot);
  }
  // free the memory dynamically allocated for this RelCacheEntry
  free(RelCacheTable::relCache[RELCAT_RELID]);

  // free the memory allocated for the attribute cache entries of the
  // relation catalog and the attribute catalog ??
}
