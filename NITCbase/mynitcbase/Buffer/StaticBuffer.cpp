#include "StaticBuffer.h"

// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer()
{
	// copy blockAllocMap blocks from disk to buffer (using readblock() of disk)
	for (int i = 0; i < 4; i++)
	{
		Disk::readBlock(blockAllocMap + i * BLOCK_SIZE, i);
	}

	// initialise all buffers as free
	for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
	{
		metainfo[bufferIndex].free = true;
		metainfo[bufferIndex].dirty = false;
		metainfo[bufferIndex].timeStamp = -1;
		metainfo[bufferIndex].blockNum = -1;
	}
}

// yet to perform write back
StaticBuffer::~StaticBuffer()
{
	// Write back BMAP and dirty blocks
	for (int i = 0; i < 4; i++)
	{
		Disk::writeBlock(blockAllocMap + i * BLOCK_SIZE, i);
	}
	for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
	{
		if (metainfo[bufferIndex].free == false && metainfo[bufferIndex].dirty == true)
		{
			Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
		}
	}
}

int StaticBuffer::getBufferNum(int blockNum)
{
	// Check if blockNum is valid (between zero and DISK_BLOCKS)
	// and return E_OUTOFBOUND if not valid.
	if (blockNum < 0 || blockNum >= DISK_BLOCKS)
		return E_OUTOFBOUND;

	// find and return the bufferIndex which corresponds to blockNum (check metainfo)
	for (int bufferBlock = 0; bufferBlock < BUFFER_CAPACITY; bufferBlock++)
	{
		if (metainfo[bufferBlock].free == false && metainfo[bufferBlock].blockNum == blockNum)
			return bufferBlock;
	}

	// if block is not in the buffer
	return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::getFreeBuffer(int blockNum)
{
	if (blockNum < 0 || blockNum > DISK_BLOCKS)
	{
		return E_OUTOFBOUND;
	}

	// increase the timeStamp in metaInfo of all occupied buffers.
	for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
	{
		if (metainfo[bufferIndex].free == false)
		{
			metainfo[bufferIndex].timeStamp++;
		}
	}
	// let bufferNum be used to store the buffer number of the free/freed buffer.
	int bufferNum = -1;
	// iterate through metainfo and check if there is any buffer free
	for (int i = 0; i < BUFFER_CAPACITY; i++)
	{
		if (metainfo[i].free == true)
		{
			bufferNum = i;
			break;
		}
	}

	// if a free buffer is not available,
	//     find the buffer with the largest timestamp
	//     IF IT IS DIRTY, write back to the disk using Disk::writeBlock()
	//     set bufferNum = index of this buffer
	if (bufferNum == -1)
	{
		int highestTimeStamp = 0;
		for (int i = 0; i < BUFFER_CAPACITY; i++)
		{
			if (metainfo[i].timeStamp > highestTimeStamp)
			{
				highestTimeStamp = metainfo[i].timeStamp;
				bufferNum = i;
			}
		}

		if (metainfo[bufferNum].dirty)
			Disk::writeBlock(StaticBuffer::blocks[bufferNum], metainfo[bufferNum].blockNum);
	}
	// update the metaInfo entry corresponding to bufferNum with
	// free:false, dirty:false, blockNum:the input block number, timeStamp:0.
	metainfo[bufferNum].free = false;
	metainfo[bufferNum].dirty = false;
	metainfo[bufferNum].blockNum = blockNum;
	metainfo[bufferNum].timeStamp = 0;

	return bufferNum;
}

int StaticBuffer::setDirtyBit(int blockNum)
{
	int bufIndex = getBufferNum(blockNum);

	if (bufIndex == E_BLOCKNOTINBUFFER)
		return E_BLOCKNOTINBUFFER;
	else if (bufIndex == E_OUTOFBOUND)
		return E_OUTOFBOUND;
	else
		metainfo[bufIndex].dirty = true;

	return SUCCESS;
}