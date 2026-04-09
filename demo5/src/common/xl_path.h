#ifndef XL_PATH_H
#define XL_PATH_H

#include "xl_common.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <direct.h>
	#define xlMakeDir(path) _mkdir(path)
#else
	#include <sys/stat.h>
	#include <sys/types.h>
	#define xlMakeDir(path) mkdir(path, 0755)
#endif

static inline int xlPathIsSep(int iCh)
{
	return (iCh == '/') || (iCh == '\\');
}

static inline void xlPathJoin(char* sOut, size_t iOutSize, const char* sBase, const char* sName)
{
	size_t iBaseLen;
	int bNeedSep;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sBase == NULL ) {
		sBase = "";
	}
	if ( sName == NULL ) {
		sName = "";
	}

	iBaseLen = strlen(sBase);
	bNeedSep = (iBaseLen > 0) && !xlPathIsSep((unsigned char)sBase[iBaseLen - 1]);
	snprintf(
		sOut,
		iOutSize,
		"%s%s%s",
		sBase,
		bNeedSep ? "\\" : "",
		sName
	);
}

static inline int xlPathEnsureDir(const char* sPath)
{
	char sTemp[XL_PATH_MAX_LEN];
	size_t iLen;
	size_t i;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	snprintf(sTemp, sizeof(sTemp), "%s", sPath);
	iLen = strlen(sTemp);
	if ( iLen == 0 ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < iLen; ++i ) {
		if ( xlPathIsSep((unsigned char)sTemp[i]) ) {
			char chHold = sTemp[i];
			sTemp[i] = '\0';
			if ( (sTemp[0] != '\0') && (strcmp(sTemp, ".") != 0) ) {
				xlMakeDir(sTemp);
			}
			sTemp[i] = chHold;
		}
	}

	xlMakeDir(sTemp);
	return XL_RC_OK;
}

static inline int xlPathEnsureParentDir(const char* sFilePath)
{
	char sTemp[XL_PATH_MAX_LEN];
	size_t iLen;

	if ( sFilePath == NULL ) {
		return XL_RC_BAD_ARG;
	}

	snprintf(sTemp, sizeof(sTemp), "%s", sFilePath);
	iLen = strlen(sTemp);
	while ( iLen > 0 ) {
		if ( xlPathIsSep((unsigned char)sTemp[iLen - 1]) ) {
			sTemp[iLen - 1] = '\0';
			break;
		}
		--iLen;
	}

	if ( iLen == 0 ) {
		return XL_RC_OK;
	}

	return xlPathEnsureDir(sTemp);
}

#endif
