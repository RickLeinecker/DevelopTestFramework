#pragma once
#include <stdio.h>
#include <memory.h>

#define MEMORY 1
#define DISK 2

class CIO
{

public:
	CIO(unsigned char * pOutBuffer, int size)
	{
		fp = NULL;
		this->size = size;
		this->pOutBuffer = pOutBuffer;
		dataIndex = 0;
		type = MEMORY;
	}

	CIO(int bufferSize)
	{
		fp = NULL;
		size = bufferSize;
		pOutBuffer = new unsigned char [bufferSize];
		dataIndex = 0;
		type = MEMORY;
	}

	CIO(FILE* fp)
	{
		this->fp = fp;
		type = DISK;
	}

	int outputToMemory(unsigned char* data, int size)
	{
		memcpy(&pOutBuffer[dataIndex], data, size);
		dataIndex += size;
		return size;
	}

	int outputToDisk(unsigned char* data, int size)
	{
		if (fp == NULL)
		{
			return -size;
		}
		return fwrite(data, size, sizeof(char), fp);
	}

	int output(unsigned char* data, int size)
	{
		if (type == MEMORY)
		{
			return outputToMemory(data, size);
		}
		else if (type == DISK)
		{
			return outputToDisk(data, size);
		}
		return 0;
	}

private:
	FILE* fp;
	int size;
	unsigned char* pOutBuffer;
	int dataIndex;
	int type;
};

