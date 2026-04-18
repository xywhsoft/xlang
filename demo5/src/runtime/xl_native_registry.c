#include "xl_native_registry.h"

const char* xlStdCoreModuleSource(void);
const char* xlStdCryptoModuleSource(void);
const char* xlStdDebugModuleSource(void);
const char* xlStdChannelModuleSource(void);
const char* xlStdClockModuleSource(void);
const char* xlStdDictModuleSource(void);
const char* xlStdDirModuleSource(void);
const char* xlStdEncodeModuleSource(void);
const char* xlStdEnvModuleSource(void);
const char* xlStdErrorModuleSource(void);
const char* xlStdFmtModuleSource(void);
const char* xlStdFsModuleSource(void);
const char* xlStdFutureModuleSource(void);
const char* xlStdHashModuleSource(void);
const char* xlStdHostModuleSource(void);
const char* xlStdIoModuleSource(void);
const char* xlStdIterModuleSource(void);
const char* xlStdJsonModuleSource(void);
const char* xlStdListModuleSource(void);
const char* xlStdAssertModuleSource(void);
const char* xlStdLogModuleSource(void);
const char* xlStdMathModuleSource(void);
const char* xlStdOsModuleSource(void);
const char* xlStdPathModuleSource(void);
const char* xlStdPackModuleSource(void);
const char* xlStdRandModuleSource(void);
const char* xlStdRegexModuleSource(void);
const char* xlStdProcessModuleSource(void);
const char* xlStdQueryModuleSource(void);
const char* xlStdRangeModuleSource(void);
const char* xlStdArrayModuleSource(void);
const char* xlStdSetModuleSource(void);
const char* xlStdStrModuleSource(void);
const char* xlStdSysEnvModuleSource(void);
const char* xlStdSysFileModuleSource(void);
const char* xlStdSysProcessModuleSource(void);
const char* xlStdSysWaitModuleSource(void);
const char* xlStdTemplateModuleSource(void);
const char* xlStdTraceModuleSource(void);
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
	{ "std.crypto", "std.crypto", "<native:std.crypto>", xlStdCryptoModuleSource },
	{ "std.debug", "std.debug", "<native:std.debug>", xlStdDebugModuleSource },
	{ "std.channel", "std.channel", "<native:std.channel>", xlStdChannelModuleSource },
	{ "std.clock", "std.clock", "<native:std.clock>", xlStdClockModuleSource },
	{ "std.dict", "std.dict", "<native:std.dict>", xlStdDictModuleSource },
	{ "std.dir", "std.dir", "<native:std.dir>", xlStdDirModuleSource },
	{ "std.encode", "std.encode", "<native:std.encode>", xlStdEncodeModuleSource },
	{ "std.env", "std.env", "<native:std.env>", xlStdEnvModuleSource },
	{ "std.error", "std.error", "<native:std.error>", xlStdErrorModuleSource },
	{ "std.fmt", "std.fmt", "<native:std.fmt>", xlStdFmtModuleSource },
	{ "std.fs", "std.fs", "<native:std.fs>", xlStdFsModuleSource },
	{ "std.future", "std.future", "<native:std.future>", xlStdFutureModuleSource },
	{ "std.hash", "std.hash", "<native:std.hash>", xlStdHashModuleSource },
	{ "std.host", "std.host", "<native:std.host>", xlStdHostModuleSource },
	{ "std.io", "std.io", "<native:std.io>", xlStdIoModuleSource },
	{ "std.iter", "std.iter", "<native:std.iter>", xlStdIterModuleSource },
	{ "std.json", "std.json", "<native:std.json>", xlStdJsonModuleSource },
	{ "std.list", "std.list", "<native:std.list>", xlStdListModuleSource },
	{ "std.assert", "std.assert", "<native:std.assert>", xlStdAssertModuleSource },
	{ "std.log", "std.log", "<native:std.log>", xlStdLogModuleSource },
	{ "std.math", "std.math", "<native:std.math>", xlStdMathModuleSource },
	{ "std.os", "std.os", "<native:std.os>", xlStdOsModuleSource },
	{ "std.pack", "std.pack", "<native:std.pack>", xlStdPackModuleSource },
	{ "std.path", "std.path", "<native:std.path>", xlStdPathModuleSource },
	{ "std.rand", "std.rand", "<native:std.rand>", xlStdRandModuleSource },
	{ "std.regex", "std.regex", "<native:std.regex>", xlStdRegexModuleSource },
	{ "std.process", "std.process", "<native:std.process>", xlStdProcessModuleSource },
	{ "std.query", "std.query", "<native:std.query>", xlStdQueryModuleSource },
	{ "std.range", "std.range", "<native:std.range>", xlStdRangeModuleSource },
	{ "std.array", "std.array", "<native:std.array>", xlStdArrayModuleSource },
	{ "std.set", "std.set", "<native:std.set>", xlStdSetModuleSource },
	{ "std.str", "std.str", "<native:std.str>", xlStdStrModuleSource },
	{ "std.sys.env", "std.sys.env", "<native:std.sys.env>", xlStdSysEnvModuleSource },
	{ "std.sys.file", "std.sys.file", "<native:std.sys.file>", xlStdSysFileModuleSource },
	{ "std.sys.process", "std.sys.process", "<native:std.sys.process>", xlStdSysProcessModuleSource },
	{ "std.sys.wait", "std.sys.wait", "<native:std.sys.wait>", xlStdSysWaitModuleSource },
	{ "std.template", "std.template", "<native:std.template>", xlStdTemplateModuleSource },
	{ "std.trace", "std.trace", "<native:std.trace>", xlStdTraceModuleSource },
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
