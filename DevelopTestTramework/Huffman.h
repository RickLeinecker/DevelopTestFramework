#pragma once

/* Types of blocks. */
#define STORED 0
#define FIXED 1
#define DYNAMIC 2

/* Errors */
#define DATAEND 1
#define COMPLEMENTNOMATCH 2
#define BADCOUNTS 3
#define INCOMPLETECODESET 4
#define RANOUTOFCODES -10
#define NOLASTLENGTH 5
#define TOOMANYLENGTHS 6
#define NOENDOFBLOCKCODE 7
#define INCOMPLETECODESINGLE 8
#define INCOMPLETECODESINGLE2 9
#define INVALIDFIXEDCODE -11



/*
	Maximums for allocations and loops. It is not useful to change these --
	they are fixed by the deflate format.
*/
#define MAXBITS 15						/* maximum bits in a code */
#define MAXLCODES 286					/* maximum number of literal/length codes */
#define MAXDCODES 30					/* maximum number of distance codes */
#define MAXCODES (MAXLCODES+MAXDCODES)	/* maximum codes lengths to read */
#define FIXLCODES 288					/* number of fixed literal/length codes */

class CHuffman
{
public:
	CHuffman();
	~CHuffman();

	int getBits(int need);
	void decompress(unsigned char *compressedData, int dataSize);

	int bytesIn, byteLength, byteIndex, bitBuffer, bitCount, error;
	unsigned char *dataIn;

private:
	unsigned int getTwoByteValue(unsigned char *data);
	void stored(void);
	int dynamic(void);
	void fixed(void);
	int decode(const struct huffman* h);
	int codes(const struct huffman* lencode, const struct huffman* distcode);
	int construct(struct huffman *h, const short *length, int n);

};

