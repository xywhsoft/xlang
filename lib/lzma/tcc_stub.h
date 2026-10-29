#ifndef TCC_STUB_H
#define TCC_STUB_H

#ifndef __TINYC__
	#error This header is for TCC compiler only
#endif

#include "7zTypes.h"

void Z7_FASTCALL z7_x86_cpuid_subFunc(UInt32 p[4], UInt32 func, UInt32 subFunc)
{
	(void)p;
	(void)func;
	(void)subFunc;
}

#endif
