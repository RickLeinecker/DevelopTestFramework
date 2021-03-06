#include "stdafx.h"
#include "Huffman.h"
#include "structs.h"

CHuffman::CHuffman(CLZ *pLZ,CIO *pCIO)
{
	/* Do default initializations. */
	bytesIn = byteLength = byteIndex = bitBuffer = bitCount = 0;
	dataIn = NULL;

	this->pLZ = pLZ;
	this->pCIO = pCIO;
}

CHuffman::~CHuffman()
{
}

/*
* Return need bits from the input stream.  This always leaves less than
* eight bits in the buffer.  getBits() works properly for need == 0.
*
* Format notes:
*
* - Bits are stored in bytes from the least significant bit to the most
*   significant bit.  Therefore bits are dropped from the bottom of the bit
*   buffer, using shift right, and new bytes are appended to the top of the
*   bit buffer, using shift left.
*/
int CHuffman::getBits(int need)
{
	/* Get anything in the bit buffer. */
	int value = bitBuffer;

	/* We may need more than we are asking for. */
	if (bitCount < need)
	{
		/* This means we are out of data. */
		if (byteIndex >= byteLength)
		{
			/* Return with -1 to signify that we are out of data. */
			return -1;
		}
		/* Now combine the current data with the new byte of data. */
		value |= (long)(dataIn[byteIndex++]) << bitCount;
		/* Increment the bit count by 8. */
		bitCount += 8;
	}

	/* Here we shift right to get right of the bits that are being retrieved. */
	bitBuffer = (int)(value >> need);
	/* Adjust the bit count by the need. */
	bitCount -= need;

	/* Return the needed bits and zero out the bits above them. */
	return (int)( value & ( ( 1L << need ) - 1 ) );
}

/*
* Inflate source to dest.  On return, destlen and sourcelen are updated to the
* size of the uncompressed data and the size of the deflate data respectively.
* On success, the return value of puff() is zero.  If there is an error in the
* source data, i.e. it is not in the deflate format, then a negative value is
* returned.  If there is not enough input available or there is not enough
* output space, then a positive error is returned.  In that case, destlen and
* sourcelen are not updated to facilitate retrying from the beginning with the
* provision of more input data or more output space.  In the case of invalid
* inflate data (a negative error), the dest and source pointers are updated to
* facilitate the debugging of deflators.
*
* puff() also has a mode to determine the size of the uncompressed output with
* no output written.  For this dest must be (unsigned char *)0.  In this case,
* the input value of *destlen is ignored, and on return *destlen is set to the
* size of the uncompressed output.
*
* Format notes:
*
* - Three bits are read for each block to determine the kind of block and
*   whether or not it is the last block.  Then the block is decoded and the
*   process repeated if it was not the last block.
*
* - The leftover bits in the last byte of the deflate data after the last
*   block (if it was a fixed or dynamic block) are undefined and have no
*   expected values to check.
*/
void CHuffman::decompress(unsigned char *compressedData, int dataSize)
{
	/* Initialize variables. */
	bytesIn = byteIndex = bitBuffer = bitCount = error = 0;
	/* Store the data size. */
	byteLength = dataSize;
	/* This points to the data. */
	dataIn = compressedData;

	int last, type;

	do
	{
		/* This indicates whether this is the last block. */
		last = getBits(1);
		/* Get the block type. */
		type = getBits(2);

		switch (type)
		{
			case STORED:
				stored();
				break;
			case FIXED:
				fixed();
				break;
			case DYNAMIC:
				dynamic();
				break;
			default:
				break;
		}
	} 
	while (last == 0);

}

/// <summary>
/// Gets a two-byte value from the data block.
/// </summary>
/// <param name="data">Address of the data.</param>
/// <returns>The 16-bit unsigned value.</returns>
unsigned int CHuffman::getTwoByteValue(unsigned char *data)
{
	unsigned int value;

	/* Get the length from the first two bytes. Convert from Big-Endian to Little-Endian. */
	value = (unsigned int)data[0] | ((unsigned int)data[1] << 8);

	return value;
}

/*
* Process a stored block.
*
* Format notes:
*
* - After the two-bit stored block type (00), the stored block length and
*   stored bytes are byte-aligned for fast copying.  Therefore any leftover
*   bits in the byte that has the last bit of the type, as many as seven, are
*   discarded.  The value of the discarded bits are not defined and should not
*   be checked against any expectation.
*
* - The second inverted copy of the stored block length does not have to be
*   checked, but it's probably a good idea to do so anyway.
*
* - A stored block can have zero length.  This is sometimes used to byte-align
*   subsets of the compressed data for random access or partial recovery.
*/
void CHuffman::stored(void)
{

	/* When the data is stored and were are going to simply
	   copy, we discard any leftover bits. */
	bitCount = bitBuffer = 0;

	/* We are out of data. */
	if (byteIndex + 4 > byteLength)
	{
		error = DATAEND;
		return;
	}

	/* Get the length from the first two bytes. Convert from Big-Endian to Little-Endian. */
	unsigned int len = getTwoByteValue(&dataIn[byteIndex]);
	/* Get the complement. */
	unsigned int complement = getTwoByteValue(&dataIn[byteIndex+2]);
	/* Compare */
	if (~complement != len)
	{
		error = COMPLEMENTNOMATCH;
		return;
	}
	/* Skip past these four bytes which contain the block length and its complement. */
	byteIndex += 4;

	/* Check to make sure we have enough data remaining. */
	if (byteIndex + len > len)
	{
		error = DATAEND;
		return;
	}

	/* ToDo */
	/* Here we store the bytes to the destination. This is not implemented at this moment (1-30-2022). */

	// 6-11-2022
	pCIO->output(&dataIn[byteIndex], len);
	byteIndex += len;

	/* Adjust the values. */
	byteIndex += len;

}

/*
* Decode a code from the stream s using huffman table h.  Return the symbol or
* a negative value if there is an error.  If all of the lengths are zero, i.e.
* an empty code, or if the code is incomplete and an invalid code is received,
* then -10 is returned after reading MAXBITS bits.
*
* Format notes:
*
* - The codes as stored in the compressed data are bit-reversed relative to
*   a simple integer ordering of codes of the same lengths.  Hence below the
*   bits are pulled from the compressed data one at a time and used to
*   build the code value reversed from what is in the stream in order to
*   permit simple integer comparisons for decoding.  A table-based decoding
*   scheme (as used in zlib) does not need to do this reversal.
*
* - The first code for the shortest length is all zeros.  Subsequent codes of
*   the same length are simply integer increments of the previous code.  When
*   moving up a length, a zero bit is appended to the code.  For a complete
*   code, the last code of the longest length will be all ones.
*
* - Incomplete codes are handled by this decoder, since they are permitted
*   in the deflate format.  See the format notes for fixed() and dynamic().
*/
int CHuffman::decode(const struct huffman* h)
{
	int len;            /* current number of bits in code */
	int code;           /* len bits being decoded */
	int first;          /* first code of length len */
	int count;          /* number of codes of length len */
	int index;          /* index of first code of length len in symbol table */

	code = first = index = 0;
	for (len = 1; len <= MAXBITS; len++)
	{
		code |= getBits(1);             /* get next bit */
		count = h->count[len];

		if (code - count < first)       /* if length len, return symbol */
		{
			return h->symbol[index + (code - first)];
		}

		index += count;                 /* else update for next length */
		first += count;
		first <<= 1;
		code <<= 1;
	}
	return RANOUTOFCODES;                         /* ran out of codes */
}

/*
* Process a fixed codes block.
*
* Format notes:
*
* - This block type can be useful for compressing small amounts of data for
*   which the size of the code descriptions in a dynamic block exceeds the
*   benefit of custom codes for that block.  For fixed codes, no bits are
*   spent on code descriptions.  Instead the code lengths for literal/length
*   codes and distance codes are fixed.  The specific lengths for each symbol
*   can be seen in the "for" loops below.
*
* - The literal/length code is complete, but has two symbols that are invalid
*   and should result in an error if received.  This cannot be implemented
*   simply as an incomplete code since those two symbols are in the "middle"
*   of the code.  They are eight bits long and the longest literal/length\
*   code is nine bits.  Therefore the code must be constructed with those
*   symbols, and the invalid symbols must be detected after decoding.
*
* - The fixed distance codes also have two invalid symbols that should result
*   in an error if received.  Since all of the distance codes are the same
*   length, this can be implemented as an incomplete code.  Then the invalid
*   codes are detected while decoding.
*/
void CHuffman::fixed(void)
{
	static int virgin = 1;
	static short lencnt[MAXBITS + 1], lensym[FIXLCODES];
	static short distcnt[MAXBITS + 1], distsym[MAXDCODES];
	static struct huffman lencode, distcode;

	/* build fixed huffman tables if first call (may not be thread safe) */
	if (virgin)
	{
		int symbol;
		short lengths[FIXLCODES];

		/* construct lencode and distcode */
		lencode.count = lencnt;
		lencode.symbol = lensym;
		distcode.count = distcnt;
		distcode.symbol = distsym;

		/* literal/length table */
		for (symbol = 0; symbol < 144; symbol++)
		{
			lengths[symbol] = 8;
		}
		for (; symbol < 256; symbol++)
		{
			lengths[symbol] = 9;
		}
		for (; symbol < 280; symbol++)
		{
			lengths[symbol] = 7;
		}
		for (; symbol < FIXLCODES; symbol++)
		{
			lengths[symbol] = 8;
		}
		construct(&lencode, lengths, FIXLCODES);

		/* distance table */
		for (symbol = 0; symbol < MAXDCODES; symbol++)
		{
			lengths[symbol] = 5;
		}
		construct(&distcode, lengths, MAXDCODES);

		/* do this just once */
		virgin = 0;
	}

	/* decode data until end-of-block code */
//	return codes(s, &lencode, &distcode);
	codes(&lencode, &distcode);
}

/*
* Process a dynamic codes block.
*
* Format notes:
*
* - A dynamic block starts with a description of the literal/length and
*   distance codes for that block.  New dynamic blocks allow the compressor to
*   rapidly adapt to changing data with new codes optimized for that data.
*
* - The codes used by the deflate format are "canonical", which means that
*   the actual bits of the codes are generated in an unambiguous way simply
*   from the number of bits in each code.  Therefore the code descriptions
*   are simply a list of code lengths for each symbol.
*
* - The code lengths are stored in order for the symbols, so lengths are
*   provided for each of the literal/length symbols, and for each of the
*   distance symbols.
*
* - If a symbol is not used in the block, this is represented by a zero
*   as the code length.  This does not mean a zero-length code, but rather
*   that no code should be created for this symbol.  There is no way in the
*   deflate format to represent a zero-length code.
*
* - The maximum number of bits in a code is 15, so the possible lengths for
*   any code are 1..15.
*
* - The fact that a length of zero is not permitted for a code has an
*   interesting consequence.  Normally if only one symbol is used for a given
*   code, then in fact that code could be represented with zero bits.  However
*   in deflate, that code has to be at least one bit.  So for example, if
*   only a single distance base symbol appears in a block, then it will be
*   represented by a single code of length one, in particular one 0 bit.  This
*   is an incomplete code, since if a 1 bit is received, it has no meaning,
*   and should result in an error.  So incomplete distance codes of one symbol
*   should be permitted, and the receipt of invalid codes should be handled.
*
* - It is also possible to have a single literal/length code, but that code
*   must be the end-of-block code, since every dynamic block has one.  This
*   is not the most efficient way to create an empty block (an empty fixed
*   block is fewer bits), but it is allowed by the format.  So incomplete
*   literal/length codes of one symbol should also be permitted.
*
* - If there are only literal codes and no lengths, then there are no distance
*   codes.  This is represented by one distance code with zero bits.
*
* - The list of up to 286 length/literal lengths and up to 30 distance lengths
*   are themselves compressed using Huffman codes and run-length encoding.  In
*   the list of code lengths, a 0 symbol means no code, a 1..15 symbol means
*   that length, and the symbols 16, 17, and 18 are run-length instructions.
*   Each of 16, 17, and 18 are follwed by extra bits to define the length of
*   the run.  16 copies the last length 3 to 6 times.  17 represents 3 to 10
*   zero lengths, and 18 represents 11 to 138 zero lengths.  Unused symbols
*   are common, hence the special coding for zero lengths.
*
* - The symbols for 0..18 are Huffman coded, and so that code must be
*   described first.  This is simply a sequence of up to 19 three-bit values
*   representing no code (0) or the code length for that symbol (1..7).
*
* - A dynamic block starts with three fixed-size counts from which is computed
*   the number of literal/length code lengths, the number of distance code
*   lengths, and the number of code length code lengths (ok, you come up with
*   a better name!) in the code descriptions.  For the literal/length and
*   distance codes, lengths after those provided are considered zero, i.e. no
*   code.  The code length code lengths are received in a permuted order (see
*   the order[] array below) to make a short code length code length list more
*   likely.  As it turns out, very short and very long codes are less likely
*   to be seen in a dynamic code description, hence what may appear initially
*   to be a peculiar ordering.
*
* - Given the number of literal/length code lengths (nlen) and distance code
*   lengths (ndist), then they are treated as one long list of nlen + ndist
*   code lengths.  Therefore run-length coding can and often does cross the
*   boundary between the two sets of lengths.
*
* - So to summarize, the code description at the start of a dynamic block is
*   three counts for the number of code lengths for the literal/length codes,
*   the distance codes, and the code length codes.  This is followed by the
*   code length code lengths, three bits each.  This is used to construct the
*   code length code which is used to read the remainder of the lengths.  Then
*   the literal/length code lengths and distance lengths are read as a single
*   set of lengths using the code length codes.  Codes are constructed from
*   the resulting two sets of lengths, and then finally you can start
*   decoding actual compressed data in the block.
*
* - For reference, a "typical" size for the code description in a dynamic
*   block is around 80 bytes.
*/
int CHuffman::dynamic(void)
{
	int err;
	short lengths[MAXCODES];            /* descriptor code lengths */
	short lencnt[MAXBITS + 1], lensym[MAXLCODES];         /* lencode memory */
	short distcnt[MAXBITS + 1], distsym[MAXDCODES];       /* distcode memory */
	struct huffman lencode, distcode;   /* length and distance codes */

	/* permutation of code length codes */
	static short order[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

	/* construct lencode and distcode */
	lencode.count = lencnt;
	lencode.symbol = lensym;
	distcode.count = distcnt;
	distcode.symbol = distsym;

	int nlen = getBits(5) + 257;
	int ndist = getBits(5) + 1;
	int ncode = getBits(4) + 4;

	if (nlen > MAXLCODES || ndist > MAXDCODES)
	{
		error = BADCOUNTS;
		return error;
	}

	/* Read code length code lengths (really), missing lengths are zero. */
	int index;
	for (index = 0; index < ncode; index++)
	{
		lengths[order[index]] = getBits(3);
	}
	for (; index < (sizeof(order)/sizeof(short)); index++)
	{
		lengths[order[index]] = 0;
	}

	/* build huffman table for code lengths codes (use lencode temporarily) */
	err = construct(&lencode, lengths, 19);

	/* require complete code set here */
	if (err != 0)
	{
		error = INCOMPLETECODESET;
		return error;
	}

	/* read length/literal and distance code length tables */
	index = 0;
	while( index < nlen + ndist )
	{
		int symbol;             /* decoded value */
		int len;                /* last length to repeat */

		symbol = decode(&lencode);
		if (symbol < 0)
		{
			error = symbol;
			return error;
		}

		if (symbol < 16)                /* length in 0..15 */
		{
			lengths[index++] = symbol;
		}
		else                           /* repeat instruction */
		{
			len = 0;                    /* assume repeating zeros */
			if (symbol == 16)          /* repeat last length 3..6 times */
			{
				if (index == 0)
				{
					error = NOLASTLENGTH;	/* no last length! */
					return error;
				}
				len = lengths[index - 1];       /* last length */
				symbol = 3 + getBits(2);
			}
			else if (symbol == 17)      /* repeat zero 3..10 times */
			{
				symbol = 3 + getBits(3);
			}
			else                        /* == 18, repeat zero 11..138 times */
			{
				symbol = 11 + getBits(7);
			}

			if (index + symbol > nlen + ndist)
			{
				error = TOOMANYLENGTHS;
				return error;
			}

			while (symbol--)            /* repeat last or zero symbol times */
			{
				lengths[index++] = len;
			}
		}
	}

	/* check for end-of-block code -- there better be one! */
	if (lengths[256] == 0)
	{
		error = NOENDOFBLOCKCODE;
		return error;
	}

	/* build huffman table for literal/length codes */
	err = construct(&lencode, lengths, nlen);

	if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1]))
	{
		error = INCOMPLETECODESINGLE;
		return error;
	}

	/* build huffman table for distance codes */
	err = construct(&distcode, lengths + nlen, ndist);

	if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1]))
	{
		error = INCOMPLETECODESINGLE2;
		return error;
	}

	/* decode data until end-of-block code */
	return codes(&lencode, &distcode);

}

/*
* Decode literal/length and distance codes until an end-of-block code.
*
* Format notes:
*
* - Compressed data that is after the block type if fixed or after the code
*   description if dynamic is a combination of literals and length/distance
*   pairs terminated by and end-of-block code.  Literals are simply Huffman
*   coded bytes.  A length/distance pair is a coded length followed by a
*   coded distance to represent a string that occurs earlier in the
*   uncompressed data that occurs again at the current location.
*
* - Literals, lengths, and the end-of-block code are combined into a single
*   code of up to 286 symbols.  They are 256 literals (0..255), 29 length
*   symbols (257..285), and the end-of-block symbol (256).
*
* - There are 256 possible lengths (3..258), and so 29 symbols are not enough
*   to represent all of those.  Lengths 3..10 and 258 are in fact represented
*   by just a length symbol.  Lengths 11..257 are represented as a symbol and
*   some number of extra bits that are added as an integer to the base length
*   of the length symbol.  The number of extra bits is determined by the base
*   length symbol.  These are in the static arrays below, lens[] for the base
*   lengths and lext[] for the corresponding number of extra bits.
*
* - The reason that 258 gets its own symbol is that the longest length is used
*   often in highly redundant files.  Note that 258 can also be coded as the
*   base value 227 plus the maximum extra value of 31.  While a good deflate
*   should never do this, it is not an error, and should be decoded properly.
*
* - If a length is decoded, including its extra bits if any, then it is
*   followed a distance code.  There are up to 30 distance symbols.  Again
*   there are many more possible distances (1..32768), so extra bits are added
*   to a base value represented by the symbol.  The distances 1..4 get their
*   own symbol, but the rest require extra bits.  The base distances and
*   corresponding number of extra bits are below in the static arrays dist[]
*   and dext[].
*
* - Literal bytes are simply written to the output.  A length/distance pair is
*   an instruction to copy previously uncompressed bytes to the output.  The
*   copy is from distance bytes back in the output stream, copying for length
*   bytes.
*
* - Distances pointing before the beginning of the output data are not
*   permitted.
*
* - Overlapped copies, where the length is greater than the distance, are
*   allowed and common.  For example, a distance of one and a length of 258
*   simply copies the last byte 258 times.  A distance of four and a length of
*   twelve copies the last four bytes three times.  A simple forward copy
*   ignoring whether the length is greater than the distance or not implements
*   this correctly.  You should not use memcpy() since its behavior is not
*   defined for overlapped arrays.  You should not use memmove() or bcopy()
*   since though their behavior -is- defined for overlapping arrays, it is
*   defined to do the wrong thing in this case.
*/
int CHuffman::codes(const struct huffman* lencode, const struct huffman* distcode)
{
	int symbol;         /* decoded symbol */
	int len;            /* length for copy */
	unsigned dist;      /* distance for copy */

	static const short lens[29] = { /* Size base for length codes 257..285 */
		3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
		35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
	static const short lext[29] = { /* Extra bits for length codes 257..285 */
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
		3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
	static const short dists[30] = { /* Offset base for distance codes 0..29 */
		1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
		257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
		8193, 12289, 16385, 24577 };
	static const short dext[30] = { /* Extra bits for distance codes 0..29 */
		0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
		7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
		12, 12, 13, 13 };

	/* decode literals and length/distance pairs */
	do 
	{
		symbol = decode(lencode);

		unsigned short tmp = (unsigned short)symbol;

		if (symbol < 0)
		{
			return symbol;              /* invalid symbol */
		}

		if (symbol < 256) 
		{	/* literal: symbol is the byte */
			/* write out the literal */

			// TODO: Output literal to LZ class.
			pLZ->lit(symbol);
		}
		else if (symbol > 256)
		{	/* length */
			/* get and compute length */
			symbol -= 257;
			if (symbol >= 29)
			{
				return INVALIDFIXEDCODE;             /* invalid fixed code */
			}

			len = lens[symbol] + getBits(lext[symbol]);

			/* get and check distance */
			symbol = decode(distcode);

			if (symbol < 0)
			{
				return symbol;          /* invalid symbol */
			}

			dist = dists[symbol] + getBits(dext[symbol]);

			/* copy length bytes from distance bytes back */

			// TODO: Output len bytes to destination

			while (len--)
			{
			}
		}
		else
		{
//			s->outcnt += len;
		}
	} 
	while (symbol != 256);            /* end of block symbol */

	/* done with a valid fixed or dynamic block */
	return 0;
}

/*
* Given the list of code lengths length[0..n-1] representing a canonical
* Huffman code for n symbols, construct the tables required to decode those
* codes.  Those tables are the number of codes of each length, and the symbols
* sorted by length, retaining their original order within each length.  The
* return value is zero for a complete code set, negative for an over-
* subscribed code set, and positive for an incomplete code set.  The tables
* can be used if the return value is zero or positive, but they cannot be used
* if the return value is negative.  If the return value is zero, it is not
* possible for decode() using that table to return an error--any stream of
* enough bits will resolve to a symbol.  If the return value is positive, then
* it is possible for decode() using that table to return an error for received
* codes past the end of the incomplete lengths.
*
* Not used by decode(), but used for error checking, h->count[0] is the number
* of the n symbols not in the code.  So n - h->count[0] is the number of
* codes.  This is useful for checking for incomplete codes that have more than
* one symbol, which is an error in a dynamic block.
*
* Assumption: for all i in 0..n-1, 0 <= length[i] <= MAXBITS
* This is assured by the construction of the length arrays in dynamic() and
* fixed() and is not verified by construct().
*
* Format notes:
*
* - Permitted and expected examples of incomplete codes are one of the fixed
*   codes and any code with a single symbol which in deflate is coded as one
*   bit instead of zero bits.  See the format notes for fixed() and dynamic().
*
* - Within a given code length, the symbols are kept in ascending order for
*   the code bits definition.
*/
int CHuffman::construct(struct huffman *h, const short *length, int n)
{
	int len, symbol, left;
	short offs[MAXBITS + 1];      /* offsets in symbol table for each length */

	/* count number of codes of each length */
	for (len = 0; len <= MAXBITS; len++)
	{
		h->count[len] = 0;
	}

	for (symbol = 0; symbol < n; symbol++)
	{
		(h->count[length[symbol]])++;   /* assumes lengths are within bounds */
	}

	if (h->count[0] == n) /* no codes! */
	{
		return 0;                       /* complete, but decode() will fail */
	}

	/* check for an over-subscribed or incomplete set of lengths */
	left = 1;                           /* one possible code of zero length */
	for (len = 1; len <= MAXBITS; len++)
	{
		left <<= 1;                     /* one more bit, double codes left */
		left -= h->count[len];          /* deduct count from possible codes */
		if (left < 0)
		{
			return left;                /* over-subscribed--return negative */
		}
	}                                   /* left > 0 means incomplete */

	/* generate offsets into symbol table for each length for sorting */
	offs[1] = 0;
	for (len = 1; len < MAXBITS; len++)
	{
		offs[len + 1] = offs[len] + h->count[len];
	}

	/*
	* put symbols in table sorted by length, by symbol order within each
	* length
	*/
	for (symbol = 0; symbol < n; symbol++)
	if (length[symbol] != 0)
	{
		h->symbol[offs[length[symbol]]++] = symbol;
	}

	/* return zero for complete set, positive for incomplete set */
	return left;
}
