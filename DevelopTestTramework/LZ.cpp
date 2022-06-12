#include "stdafx.h"
#include "LZ.h"

CLZ::CLZ()
{
}

CLZ::~CLZ()
{
}

int CLZ::lit(unsigned short symbol)
{
	unsigned char d = (unsigned char)symbol;
	return pCIO->output(&d, 1);
}
