#include "xl_native_registry.h"

const char* xlStdCoreModuleSource(void);
const char* xlStdDirModuleSource(void);
const char* xlStdEnvModuleSource(void);
const char* xlStdFmtModuleSource(void);
const char* xlStdFsModuleSource(void);
const char* xlStdHostModuleSource(void);
const char* xlStdIoModuleSource(void);
const char* xlStdMathModuleSource(void);
const char* xlStdPathModuleSource(void);
const char* xlStdPackModuleSource(void);
const char* xlStdRandModuleSource(void);
const char* xlStdProcessModuleSource(void);
const char* xlStdStrModuleSource(void);
const char* xlStdTimeModuleSource(void);
const char* xlStdTaskModuleSource(void);

typedef struct XL_NativeRegistryEntry {
	const char* sModuleKey;
	const char* sBindingName;
	const char* sVirtualPath;
	const char* (*fnGetSource)(void);
} XL_NativeRegistryEntry;

typedef struct XL_NativeTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_NativeTextBuffer;

static const XL_NativeRegistryEntry g_arrNativeModules[] = {
	{ "std.core", "std.core", "<native:std.core>", xlStdCoreModuleSource },
	{ "std.dir", "std.dir", "<native:std.dir>", xlStdDirModuleSource },
	{ "std.env", "std.env", "<native:std.env>", xlStdEnvModuleSource },
	{ "std.fmt", "std.fmt", "<native:std.fmt>", xlStdFmtModuleSource },
	{ "std.fs", "std.fs", "<native:std.fs>", xlStdFsModuleSource },
	{ "std.host", "std.host", "<native:std.host>", xlStdHostModuleSource },
	{ "std.io", "std.io", "<native:std.io>", xlStdIoModuleSource },
	{ "std.math", "std.math", "<native:std.math>", xlStdMathModuleSource },
	{ "std.pack", "std.pack", "<native:std.pack>", xlStdPackModuleSource },
	{ "std.path", "std.path", "<native:std.path>", xlStdPathModuleSource },
	{ "std.rand", "std.rand", "<native:std.rand>", xlStdRandModuleSource },
	{ "std.process", "std.process", "<native:std.process>", xlStdProcessModuleSource },
	{ "std.str", "std.str", "<native:std.str>", xlStdStrModuleSource },
	{ "std.time", "std.time", "<native:std.time>", xlStdTimeModuleSource },
	{ "std.task", "std.task", "<native:std.task>", xlStdTaskModuleSource }
};

static int __xlNativeBufGrow(XL_NativeTextBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pBuf->iCap == 0) ? 256 : pBuf->iCap;
	while ( iNewCap < (pBuf->iLen + iNeed + 1) ) {
		iNewCap *= 2;
	}

	sNewData = (char*)realloc(pBuf->sData, iNewCap);
	if ( sNewData == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pBuf->sData = sNewData;
	pBuf->iCap = iNewCap;
	return XL_RC_OK;
}

static void __xlNativeBufAppend(XL_NativeTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlNativeBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlNativeBufAppendFormat(XL_NativeTextBuffer* pBuf, const char* sFormat, ...)
{
	va_list args;
	va_list args2;
	int iNeed;

	va_start(args, sFormat);
	va_copy(args2, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(args2);
		return;
	}

	if ( __xlNativeBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlNativeBufAppendEscaped(XL_NativeTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlNativeBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlNativeBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlNativeBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlNativeBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlNativeBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlNativeBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];

					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlNativeBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

int xlNativeRegistryFind(const char* sModuleKey, XL_NativeModuleInfo* pInfo)
{
	size_t i;

	if ( (sModuleKey == NULL) || (pInfo == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(pInfo, 0, sizeof(*pInfo));
	for ( i = 0; i < XL_COUNTOF(g_arrNativeModules); ++i ) {
		if ( strcmp(g_arrNativeModules[i].sModuleKey, sModuleKey) == 0 ) {
			pInfo->sModuleKey = g_arrNativeModules[i].sModuleKey;
			pInfo->sBindingName = g_arrNativeModules[i].sBindingName;
			pInfo->sVirtualPath = g_arrNativeModules[i].sVirtualPath;
			pInfo->sSourceText = g_arrNativeModules[i].fnGetSource();
			return XL_RC_OK;
		}
	}

	return XL_RC_FAIL;
}

char* xlNativeRegistryDumpText(void)
{
	XL_NativeTextBuffer buf;
	size_t i;

	memset(&buf, 0, sizeof(buf));
	__xlNativeBufAppend(&buf, "NativeRegistry\n");
	for ( i = 0; i < XL_COUNTOF(g_arrNativeModules); ++i ) {
		__xlNativeBufAppendFormat(
			&buf,
			"- key=%s binding=%s path=%s\n",
			g_arrNativeModules[i].sModuleKey,
			g_arrNativeModules[i].sBindingName,
			g_arrNativeModules[i].sVirtualPath
		);
	}
	return buf.sData;
}

char* xlNativeRegistryDumpJson(void)
{
	XL_NativeTextBuffer buf;
	size_t i;

	memset(&buf, 0, sizeof(buf));
	__xlNativeBufAppend(&buf, "{\n\t\"nativeModules\": [\n");
	for ( i = 0; i < XL_COUNTOF(g_arrNativeModules); ++i ) {
		__xlNativeBufAppend(&buf, "\t\t{\n\t\t\t\"key\": \"");
		__xlNativeBufAppendEscaped(&buf, g_arrNativeModules[i].sModuleKey);
		__xlNativeBufAppend(&buf, "\",\n\t\t\t\"binding\": \"");
		__xlNativeBufAppendEscaped(&buf, g_arrNativeModules[i].sBindingName);
		__xlNativeBufAppend(&buf, "\",\n\t\t\t\"path\": \"");
		__xlNativeBufAppendEscaped(&buf, g_arrNativeModules[i].sVirtualPath);
		__xlNativeBufAppend(&buf, "\"\n\t\t}");
		if ( (i + 1) != XL_COUNTOF(g_arrNativeModules) ) {
			__xlNativeBufAppend(&buf, ",");
		}
		__xlNativeBufAppend(&buf, "\n");
	}
	__xlNativeBufAppend(&buf, "\t]\n}\n");
	return buf.sData;
}
