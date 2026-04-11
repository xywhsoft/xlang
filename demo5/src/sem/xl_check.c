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

	return (strcmp(sTypeName, "range") == 0) ||
		(strcmp(sTypeName, "generator") == 0) ||
		(strcmp(sTypeName, "array") == 0) ||
		(strcmp(sTypeName, "list") == 0) ||
		(strcmp(sTypeName, "set") == 0) ||
		(strcmp(sTypeName, "dict") == 0) ||
		(strncmp(sTypeName, "array<", 6) == 0) ||
		(strncmp(sTypeName, "list<", 5) == 0) ||
		(strncmp(sTypeName, "set<", 4) == 0) ||
		(strncmp(sTypeName, "dict<", 5) == 0);
}
