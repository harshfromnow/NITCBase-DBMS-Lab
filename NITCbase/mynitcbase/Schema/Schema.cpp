#include "Schema.h"

#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE])
{
  int ret = OpenRelTable::openRel(relName);
  // the OpenRelTable::openRel() function returns the rel-id if successful
  // a valid rel-id will be within the range 0 <= relId < MAX_OPEN and any
  // error codes will be negative
  if (ret >= 0)
  {
    return SUCCESS;
  }

  // otherwise it returns an error message
  return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE])
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  // this function returns the rel-id of a relation if it is open or
  // E_RELNOTOPEN if it is not. we will implement this later.
  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN)
  {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE])
{
  if (strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0 || strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(oldRelName);
  if (relId != E_RELNOTOPEN)
  {
    return E_RELOPEN;
  }

  int retVal = BlockAccess::renameRelation(oldRelName, newRelName);
  return retVal;
}

int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName)
{

  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId != E_RELNOTOPEN)
  {
    return E_RELOPEN;
  }

  int retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
  return retVal;
}

int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[])
{

  // declare variable relNameAsAttribute of type Attribute
  // copy the relName into relNameAsAttribute.sVal
  Attribute relNameAsAttribute;
  strcpy(relNameAsAttribute.sVal, relName);

  // declare a variable targetRelId of type RecId
  // Search the relation catalog (relId given by the constant RELCAT_RELID)
  // for attribute value attribute "RelName" = relNameAsAttribute using
  // BlockAccess::linearSearch() with OP = EQ
  char relnameAttrRelcat[] = RELCAT_ATTR_RELNAME;
  RecId targetRecId;
  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  targetRecId = BlockAccess::linearSearch(RELCAT_RELID, relnameAttrRelcat, relNameAsAttribute, EQ);

  if (targetRecId.slot != -1 || targetRecId.block != -1)
    return E_RELEXIST;

  // compare every pair of attributes of attrNames[] array
  // if any attribute names have same string value,
  //     return E_DUPLICATEATTR (i.e 2 attributes have same value)
  for (int i = 0; i < nAttrs; i++)
  {
    for (int j = 0; j < nAttrs; j++)
    {
      if (i != j && strcmp(attrs[i], attrs[j]) == 0)
        return E_DUPLICATEATTR;
    }
  }

  /* declare relCatRecord of type Attribute which will be used to store the
     record corresponding to the new relation which will be inserted
     into relation catalog */
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  // fill relCatRecord fields as given below
  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
  relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
  relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
  relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = floor((2016 / (16 * nAttrs + 1)));
  // (number of slots is calculated as specified in the physical layer docs)

  // retVal = BlockAccess::insert(RELCAT_RELID(=0), relCatRecord);
  // if BlockAccess::insert fails return retVal
  // (this call could fail if there is no more space in the relation catalog)
  int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);
  if (retVal != SUCCESS)
    return retVal;

  for (int i = 0; i < nAttrs; i++)
  {
    /* declare Attribute attrCatRecord[6] to store the attribute catalog
        record corresponding to i'th attribute of the argument passed*/
    Attribute attrCatRecord[RELCAT_NO_ATTRS];
    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
    attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

    int retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
    if (retVal != SUCCESS)
    {
      Schema::deleteRel(relName);
      return E_DISKFULL;
    }
  }

  return SUCCESS;
}

int Schema::createIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE])
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId < 0)
    return relId; // E_RELNOTOPEN

  // create a bplus tree using BPlusTree::bPlusCreate() and return the value
  return BPlusTree::bPlusCreate(relId, attrName);
}

int Schema::dropIndex(char *relName, char *attrName)
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  // Make sure relation is open
  int relId = OpenRelTable::getRelId(relName);
  if (relId < 0)
    return relId;

  // get the attribute catalog entry corresponding to the attribute
  // using AttrCacheTable::getAttrCatEntry()
  AttrCatEntry attrCatBuf;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);
  if (relId < 0)
    return relId; // E_ATTRNOTEXIST

  int rootBlock = attrCatBuf.rootBlock;

  if (rootBlock == -1)
  {
    return E_NOINDEX;
  }

  // destroy the bplus tree rooted at rootBlock using BPlusTree::bPlusDestroy()
  BPlusTree::bPlusDestroy(rootBlock);

  // set rootBlock = -1 in the attribute cache entry of the attribute using
  // AttrCacheTable::setAttrCatEntry()
  attrCatBuf.rootBlock = -1;
  AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatBuf);

  return SUCCESS;
}

int Schema::deleteRel(char *relName)
{
  if (strcmp(relName, ATTRCAT_RELNAME) == 0 || strcmp(relName, RELCAT_RELNAME) == 0)
    return E_NOTPERMITTED;

  int relId = OpenRelTable::getRelId(relName);
  if (relId != E_RELNOTOPEN)
  {
    return E_RELOPEN;
  }

  int ret = BlockAccess::deleteRelation(relName);

  return ret;

  /*
    the only error that should be returned from deleteRelation() is E_RELNOTEXIST.
    The deleteRelation call may return E_OUTOFBOUND from the call to
    loadBlockAndGetBufferPtr, but if your implementation so far has been
    correct, it should not reach that point. That error could only occur
    if the BlockBuffer was initialized with an invalid block number.
  */
}