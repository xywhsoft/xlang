#include "xl_check.h"

int xlCheckJumpLevel(int iLevel)
{
	return iLevel > 0;
}

int xlCheckLoopJumpAllowed(int iLoopDepth, int iLevel)
{
	if ( (iLoopDepth <= 0) || (iLevel <= 0) ) {
		return 0;
	}

	return iLevel <= iLoopDepth;
}

int xlCheckIterableTypeName(const char* sTypeName)
{
	if ( sTypeName == NULL ) {
		return 0;
	}

	return (strcmp(sTypeName, "range") == 0) || (strcmp(sTypeName, "generator") == 0);
}
