#include "xl_host_runtime.h"

#include "../runtime/xlrt_profile.h"
#include "../../lib/xrt/xrt.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
	#include <windows.h>
	#include <Lmcons.h>
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

const char* xlrt_host_user(void)
{
	static char sUser[256];

#if defined(_WIN32) || defined(_WIN64)
	DWORD iSize;

	memset(sUser, 0, sizeof(sUser));
	iSize = (DWORD)sizeof(sUser);
	if ( GetUserNameA(sUser, &iSize) == 0 ) {
		const char* sEnvUser = getenv("USERNAME");
		__xlCopyNativeText(sUser, sizeof(sUser), sEnvUser);
	}
#else
	__xlCopyNativeText(sUser, sizeof(sUser), getenv("USER"));
#endif

	return sUser;
}

const char* xlrt_host_os(void)
{
#if defined(_WIN32) || defined(_WIN64)
	return "windows";
#elif defined(__APPLE__)
	return "macos";
#elif defined(__linux__)
	return "linux";
#else
	return "unknown";
#endif
}

const char* xlrt_host_arch(void)
{
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
	return "x64";
#elif defined(_M_IX86) || defined(__i386__)
	return "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
	return "arm64";
#elif defined(_M_ARM) || defined(__arm__)
	return "arm";
#else
	return "unknown";
#endif
}

const char* xlrt_host_executable(void)
{
	static char sPath[XL_PATH_MAX_LEN];

	memset(sPath, 0, sizeof(sPath));
#if defined(_WIN32) || defined(_WIN64)
	if ( GetModuleFileNameA(NULL, sPath, (DWORD)sizeof(sPath)) == 0u ) {
		sPath[0] = '\0';
	}
#elif defined(__linux__)
	{
		ssize_t iLen = readlink("/proc/self/exe", sPath, sizeof(sPath) - 1u);
		if ( iLen <= 0 ) {
			sPath[0] = '\0';
		} else {
			sPath[iLen] = '\0';
		}
	}
#else
	sPath[0] = '\0';
#endif
	return sPath;
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

int xlrt_env_set(const char* sName, const char* sValue)
{
	if ( (sName == NULL) || (sName[0] == '\0') || (sValue == NULL) ) {
		return 0;
	}

#if defined(_WIN32) || defined(_WIN64)
	return (_putenv_s(sName, sValue) == 0) ? 1 : 0;
#else
	return (setenv(sName, sValue, 1) == 0) ? 1 : 0;
#endif
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

const char* xlrt_env_temp(void)
{
	static char sPath[XL_PATH_MAX_LEN];

	memset(sPath, 0, sizeof(sPath));
#if defined(_WIN32) || defined(_WIN64)
	if ( GetTempPathA((DWORD)sizeof(sPath), sPath) == 0u ) {
		__xlCopyNativeText(sPath, sizeof(sPath), getenv("TEMP"));
	}
#else
	__xlCopyNativeText(sPath, sizeof(sPath), getenv("TMPDIR"));
	if ( sPath[0] == '\0' ) {
		__xlCopyNativeText(sPath, sizeof(sPath), "/tmp");
	}
#endif
	return sPath;
}

const char* xlrt_env_home(void)
{
	static char sPath[XL_PATH_MAX_LEN];

	memset(sPath, 0, sizeof(sPath));
#if defined(_WIN32) || defined(_WIN64)
	__xlCopyNativeText(sPath, sizeof(sPath), getenv("USERPROFILE"));
	if ( sPath[0] == '\0' ) {
		const char* sDrive = getenv("HOMEDRIVE");
		const char* sHomePath = getenv("HOMEPATH");
		if ( (sDrive != NULL) && (sHomePath != NULL) ) {
			snprintf(sPath, sizeof(sPath), "%s%s", sDrive, sHomePath);
		}
	}
#else
	__xlCopyNativeText(sPath, sizeof(sPath), getenv("HOME"));
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
