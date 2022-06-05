#ifndef __structs_h__
#define __structs_h__

#pragma pack(1)
struct GZipHeader
{
	unsigned char magic[2];
	unsigned char cm;
	unsigned char flags;
	unsigned char timedate[4];
	unsigned char flags2;
	unsigned char os;
};
#pragma pack()

/*
* Huffman code decoding tables.  count[1..MAXBITS] is the number of symbols of
* each length, which for a canonical code are stepped through in order.
* symbol[] are the symbol values in canonical order, where the number of
* entries is the sum of the counts in count[].  The decoding process can be
* seen in the function decode() below.
*/
struct huffman
{
	short *count;       /* number of symbols of each length */
	short *symbol;      /* canonically ordered symbols */
};

#endif

