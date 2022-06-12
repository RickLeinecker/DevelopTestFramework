#pragma once
#include "CIO.h"

class CLZ
{
public:
	CLZ();
	~CLZ();

	void setIO(CIO* pCIO)
	{
		this->pCIO = pCIO;
	}

	int lit(unsigned short symbol);

	CIO *pCIO;
};

