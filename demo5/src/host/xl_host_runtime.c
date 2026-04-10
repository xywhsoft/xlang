#include "xl_host_runtime.h"

#include "../runtime/xlrt_profile.h"
#include "../../lib/xrt/xrt.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
	#include <windows.h>
#endif

typedef struct XLRT_NativeErrorState {
	int bHasError;
	int iCode;
	char sKind[64];
	char sMessage[256];
	char sData[XL_PATH_MAX_LEN];
} XLRT_NativeErrorState;

static XLRT_NativeErrorState __gXLNativeErrorState;

static void __xlCopyNativeText(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1;
	}
	if ( iLen != 0 ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
}

const char* xlrt_host_name(void)
{
	static char sName[256];

#if defined(_WIN32) || defined(_WIN64)
	DWORD iSize;

	memset(sName, 0, sizeof(sName));
	iSize = (DWORD)sizeof(sName);
	if ( GetComputerNameA(sName, &iSize) == 0 ) {
		sName[0] = '\0';
	}
#else
	sName[0] = '\0';
#endif

	return sName;
}

int xlrt_attach_generated_runtime(void)
{
	xlrt_native_error_clear();
	if ( xrtThreadIsAttached() ) {
		return 0;
	}

	return (xrtThreadAttachCurrent() != NULL) ? 1 : -1;
}

void xlrt_detach_generated_runtime(int bAttachedHere)
{
	if ( (bAttachedHere > 0) && xrtThreadIsAttached() ) {
		xrtThreadDetachCurrent();
	}
}

const char* xlrt_host_ip(void)
{
	static char sIp[64];

	snprintf(sIp, sizeof(sIp), "%s", "127.0.0.1");
	return sIp;
}

const char* xlrt_env_get(const char* sName)
{
	const char* sValue;

	if ( sName == NULL ) {
		return "";
	}

	sValue = getenv(sName);
	return (sValue != NULL) ? sValue : "";
}

int xlrt_env_exists(const char* sName)
{
	if ( sName == NULL ) {
		return 0;
	}

	return getenv(sName) != NULL;
}

const char* xlrt_env_cwd(void)
{
	static char sPath[XL_PATH_MAX_LEN];

	memset(sPath, 0, sizeof(sPath));
#if defined(_WIN32) || defined(_WIN64)
	if ( GetCurrentDirectoryA((DWORD)sizeof(sPath), sPath) == 0u ) {
		sPath[0] = '\0';
	}
#else
	if ( getcwd(sPath, sizeof(sPath)) == NULL ) {
		sPath[0] = '\0';
	}
#endif
	return sPath;
}

void xlrt_native_error_clear(void)
{
	memset(&__gXLNativeErrorState, 0, sizeof(__gXLNativeErrorState));
}

void xlrt_native_error_raise(const char* sKind, const char* sMessage, int iCode, const char* sData)
{
	__gXLNativeErrorState.bHasError = 1;
	__gXLNativeErrorState.iCode = iCode;
	__xlCopyNativeText(__gXLNativeErrorState.sKind, sizeof(__gXLNativeErrorState.sKind), (sKind != NULL) ? sKind : "NativeError");
	__xlCopyNativeText(__gXLNativeErrorState.sMessage, sizeof(__gXLNativeErrorState.sMessage), sMessage);
	__xlCopyNativeText(__gXLNativeErrorState.sData, sizeof(__gXLNativeErrorState.sData), sData);
}

int xlrt_native_error_has(void)
{
	return __gXLNativeErrorState.bHasError;
}

const char* xlrt_native_error_kind(void)
{
	return (__gXLNativeErrorState.sKind[0] != '\0') ? __gXLNativeErrorState.sKind : "NativeError";
}

const char* xlrt_native_error_message(void)
{
	return __gXLNativeErrorState.sMessage;
}

int xlrt_native_error_code(void)
{
	return __gXLNativeErrorState.iCode;
}

const char* xlrt_native_error_data(void)
{
	return __gXLNativeErrorState.sData;
}
