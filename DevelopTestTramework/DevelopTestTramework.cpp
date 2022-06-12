// DevelopTestTramework.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "structs.h"
#include <io.h>
#include <malloc.h>
#include <stdlib.h>
#include "Huffman.h"

static char test[] = "D:\\work\\Data Compression Dev\\TestData\\Data18.def";

// Look at:
//   https://www.daylight.com/meetings/mug00/Sayle/gzip.html#:~:text=Stored%20blocks%20are%20allowed%20to,size%20of%20the%20gzip%20header.

/*/ <summary>
/// This function simply allocates memory and loads a file into it.
/// </summary>
/// <param name="filePath">File path of the file to load.</param>
/// <param name="len">Once function is complete, this will contain the file length.</param>
/// <returns>Allocated/populate memory buffer.</returns>
*/
void *load(const char *filePath, int *len)
{
	/* Assume 0. */
	*len = 0;

	/* Attempt to open the file. */
	FILE *fp = fopen(filePath, "rb");
	/* If file did not open return NULL indicating error. */
	if (fp == NULL)
	{
		return NULL;
	}

	/* Populate *len with the file length. This line will not port to gss Linux. */
	*len = (int)_filelength(_fileno(fp));

	/* Allocate the membory. */
	void *ret = malloc(*len);
	/* Check to see if memory allocated. */
	if (ret == NULL)
	{
		/* Set len to 0. */
		*len = 0;
		/* Close the file. */
		fclose(fp);
		/* Bail out return NULL to indicate error. */
		return NULL;
	}

	/* Read the data. */
	fread(ret, *len, sizeof(char), fp);
	/* Close the file. */
	fclose(fp);

	/* Return the allocated/populated buffer. */
	return ret;
}

/* Typeical start of program. */
void main(int argc, char* argv[])
{
	int len;

	/* Call load in order to get the allocated/populated buffer. */
	unsigned char *fileBuffer = (unsigned char *)load( test, &len );
	/* Check for load() failure. */
	if (fileBuffer == NULL)
	{
		/* Show error */
		printf("Could not open input file.\n");
		/* Bail out of program.*/
		exit(0);
	}

	/* The first thing we will do is get the gzip buffer. Later
	   we may look at it. */
	struct GZipHeader header;
	memcpy(&header, fileBuffer, sizeof(GZipHeader));

	/* The plan is to overload the constructor with output options. */
	CLZ *lz = new CLZ();
	unsigned char* pOutputBuffer = new unsigned char [100000];
	int size = 100000;
	CIO* io = new CIO( pOutputBuffer, size );
	CHuffman huff( lz, io );
	huff.decompress(&fileBuffer[sizeof(GZipHeader)], len - sizeof(GZipHeader) );
	delete lz;
	delete io;
}
