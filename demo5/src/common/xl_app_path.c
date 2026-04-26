#include "xl_app_path.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#elif defined(__linux__)
	#include <unistd.h>
#endif

static void __xlAppCopyText(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1u;
	}
	if ( iLen != 0u ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
}

static void __xlAppStripFileName(char* sPath)
{
	size_t iLen;

	if ( sPath == NULL ) {
		return;
	}

	iLen = strlen(sPath);
	while ( iLen > 0u ) {
		if ( xlPathIsSep((unsigned char)sPath[iLen - 1u]) ) {
			sPath[iLen - 1u] = '\0';
			return;
		}
		--iLen;
	}
}

int xlAppPathGetExecutable(char* sOut, size_t iOutSize)
{
	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return XL_RC_BAD_ARG;
	}

#if defined(_WIN32) || defined(_WIN64)
	{
		DWORD iLen;

		iLen = GetModuleFileNameA(NULL, sOut, (DWORD)iOutSize);
		if ( (iLen == 0u) || ((size_t)iLen >= iOutSize) ) {
			sOut[0] = '\0';
			return XL_RC_FAIL;
		}
		return XL_RC_OK;
	}
#elif defined(__linux__)
	{
		ssize_t iLen;

		iLen = readlink("/proc/self/exe", sOut, iOutSize - 1u);
		if ( iLen <= 0 ) {
			sOut[0] = '\0';
			return XL_RC_FAIL;
		}
		sOut[iLen] = '\0';
		return XL_RC_OK;
	}
#else
	__xlAppCopyText(sOut, iOutSize, "");
	return XL_RC_NOT_IMPLEMENTED;
#endif
}

int xlAppPathGetDirectory(char* sOut, size_t iOutSize)
{
	int iRc;

	iRc = xlAppPathGetExecutable(sOut, iOutSize);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	__xlAppStripFileName(sOut);
	return (sOut[0] != '\0') ? XL_RC_OK : XL_RC_FAIL;
}

int xlAppPathGetParentDirectory(char* sOut, size_t iOutSize)
{
	int iRc;

	iRc = xlAppPathGetDirectory(sOut, iOutSize);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	__xlAppStripFileName(sOut);
	return (sOut[0] != '\0') ? XL_RC_OK : XL_RC_FAIL;
}
