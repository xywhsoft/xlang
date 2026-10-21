/* netrc 的凭据往返、拒绝边界和私有文件保存。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_netrc.h"
#include <stdio.h>
#include <string.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

static bool text_equals(xvalue* pObject, const char* sKey, const char* sExpected)
{
	xstrview Text;
	xvalue* pValue = xrtValueObjectGet(pObject, xrtStrView(sKey));
	return xrtValueGetString(pValue, &Text) &&
		Text.Size == strlen(sExpected) &&
		memcmp(Text.Data, sExpected, Text.Size) == 0;
}

static bool check_document(xvalue* pDocument)
{
	xvalue* pHosts = xrtValueObjectGet(pDocument, xrtStrView("hosts"));
	xvalue* pMacros = xrtValueObjectGet(pDocument, xrtStrView("macros"));
	xvalue* pAuth = xrtValueObjectGet(pHosts, xrtStrView("macdef"));
	xvalue* pLines = xrtValueObjectGet(pMacros, xrtStrView("machine"));
	xstrview Line;
	return text_equals(pAuth, "login", "default") &&
		text_equals(pAuth, "account", "macdef") &&
		text_equals(pAuth, "password", "machine") &&
		xrtValueCount(pLines) == 1u &&
		xrtValueGetString(xrtValueArrayGet(pLines, 0u), &Line) &&
		Line.Size == 12u && memcmp(Line.Data, " echo deploy", 12u) == 0;
}

static bool test_roundtrip(void)
{
	const char* sSource =
		"machine \"macdef\" login \"default\" account \"macdef\" "
		"password \"machine\"\n"
		"macdef \"machine\" # note\n echo deploy\n\n";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &Error);
	xvalue* pAgain = NULL;
	xbuffer* pOutput = NULL;
	bool bOk = false;
	if ( pDocument == NULL || !check_document(pDocument) ) { goto cleanup; }
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	if ( pOutput == NULL ) { goto cleanup; }
	pAgain = xrtNetrcParseMemory(pOutput->Data, pOutput->Size, NULL, &Error);
	bOk = pAgain != NULL && check_document(pAgain);
cleanup:
	if ( !bOk ) { fprintf(stderr, "roundtrip failed: %s\n", Error.sMessage); }
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pDocument);
	return bOk;
}

static bool test_default_written_last(void)
{
	static const char sSource[] =
		"default login fallback\nmachine host login explicit\n";
	static const char sExpected[] =
		"machine host\n  login explicit\n\ndefault\n  login fallback\n\n";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource,
		sizeof(sSource) - 1u, NULL, &Error);
	xvalue* pAgain = NULL;
	xvalue* pHosts;
	xbuffer* pOutput = NULL;
	bool bOk = false;
	if ( pDocument == NULL ) { goto cleanup; }
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	if ( pOutput == NULL || pOutput->Size != sizeof(sExpected) - 1u ||
		memcmp(pOutput->Data, sExpected, sizeof(sExpected) - 1u) != 0 ) {
		goto cleanup;
	}
	pAgain = xrtNetrcParseMemory(pOutput->Data, pOutput->Size, NULL, &Error);
	pHosts = pAgain != NULL ? xrtValueObjectGet(pAgain, xrtStrView("hosts")) : NULL;
	bOk = pHosts != NULL &&
		text_equals(xrtValueObjectGet(pHosts, xrtStrView("host")),
			"login", "explicit") &&
		text_equals(xrtValueObjectGet(pHosts, xrtStrView("default")),
			"login", "fallback");
cleanup:
	if ( !bOk ) { fprintf(stderr, "netrc default ordering failed: %s\n", Error.sMessage); }
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pDocument);
	return bOk;
}

static bool test_macro_crlf(void)
{
	static const char sSource[] = "macdef run\r\n echo ok\r\n\r\n";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource,
		sizeof(sSource) - 1u, NULL, &Error);
	xvalue* pMacros = pDocument != NULL ?
		xrtValueObjectGet(pDocument, xrtStrView("macros")) : NULL;
	xvalue* pLines = pMacros != NULL ?
		xrtValueObjectGet(pMacros, xrtStrView("run")) : NULL;
	xstrview Line;
	bool bOk = pLines != NULL && xrtValueCount(pLines) == 1u &&
		xrtValueGetString(xrtValueArrayGet(pLines, 0u), &Line) &&
		Line.Size == 8u && memcmp(Line.Data, " echo ok", 8u) == 0;
	xrtValueRelease(pDocument);
	return bOk;
}

/* 从合法凭据派生确定性畸变，约束解析、写出和再次读取的共同边界。 */
static bool test_mutation_case(const char* pData, size_t iSize)
{
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(pData, iSize, NULL, &Error);
	xvalue* pAgain = NULL;
	xbuffer* pOutput = NULL;
	xbuffer* pAgainOutput = NULL;
	const char* sStage = "parse";
	bool bOk;
	if ( pDocument == NULL ) {
		return Error.iCode == XRT_NETRC_ERROR_SYNTAX ||
			Error.iCode == XRT_NETRC_ERROR_DUPLICATE ||
			Error.iCode == XRT_NETRC_ERROR_UTF8;
	}
	bOk = Error.iCode == XRT_NETRC_OK;
	if ( bOk ) {
		sStage = "stringify";
		pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
		bOk = pOutput != NULL && Error.iCode == XRT_NETRC_OK;
	}
	if ( bOk ) {
		sStage = "reparse";
		pAgain = xrtNetrcParseMemory(pOutput->Data, pOutput->Size, NULL, &Error);
		bOk = pAgain != NULL && Error.iCode == XRT_NETRC_OK;
	}
	if ( bOk ) {
		sStage = "canonical stringify";
		pAgainOutput = xrtNetrcStringify(pAgain, NULL, &Error);
		bOk = pAgainOutput != NULL && Error.iCode == XRT_NETRC_OK &&
			pAgainOutput->Size == pOutput->Size &&
			memcmp(pAgainOutput->Data, pOutput->Data, pOutput->Size) == 0;
	}
	if ( !bOk ) {
		fprintf(stderr, "netrc mutation %s failed: code=%d message=%s\n",
			sStage, Error.iCode, Error.sMessage);
	}
	xrtBufferDestroy(pAgainOutput);
	xrtValueRelease(pAgain);
	xrtBufferDestroy(pOutput);
	xrtValueRelease(pDocument);
	return bOk;
}

static bool test_mutation_roundtrip(void)
{
	static const char sSource[] =
		"machine \"macdef\" login \"default\" password \"a\\\\b\"\n"
		"macdef run # note\n echo deploy\n\n";
	static const char sChanges[] = {
		'\0', '\n', '\r', '#', '\\', '"', '\'', '\t', ' ', (char)0xff
	};
	char sMutated[sizeof(sSource)];
	size_t i, j;
	for ( i = 0u; i < sizeof(sSource); ++i ) {
		if ( !test_mutation_case(sSource, i) ) {
			fprintf(stderr, "netrc truncation %zu failed\n", i);
			return false;
		}
	}
	for ( i = 0u; i < sizeof(sSource) - 1u; ++i ) {
		for ( j = 0u; j < sizeof(sChanges); ++j ) {
			memcpy(sMutated, sSource, sizeof(sSource));
			sMutated[i] = sChanges[j];
			if ( !test_mutation_case(sMutated, sizeof(sSource) - 1u) ) {
				fprintf(stderr, "netrc mutation %zu/%zu failed\n", i, j);
				return false;
			}
		}
	}
	return true;
}

static bool test_rejections(void)
{
	static const struct {
		const char* Data;
		size_t Size;
		int Code;
	} Cases[] = {
		{ "machine h login a\0b\n", sizeof("machine h login a\0b\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "# comment\0hidden\nmachine h login a\n",
			sizeof("# comment\0hidden\nmachine h login a\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "machine h login a # comment\0hidden\n",
			sizeof("machine h login a # comment\0hidden\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "machine h password \"a\nb\"\n", sizeof("machine h password \"a\nb\"\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "machine h password \"a\"b\n", sizeof("machine h password \"a\"b\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "machine default login a\n", sizeof("machine default login a\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "macdef deploy extra\n echo deploy\n\n",
			sizeof("macdef deploy extra\n echo deploy\n\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "macdef deploy # note\0hidden\n echo deploy\n\n",
			sizeof("macdef deploy # note\0hidden\n echo deploy\n\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "macdef deploy\n line\0bad\n\n",
			sizeof("macdef deploy\n line\0bad\n\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "macdef run\n\recho deploy\n\n",
			sizeof("macdef run\n\recho deploy\n\n") - 1u,
			XRT_NETRC_ERROR_SYNTAX },
		{ "machine h login a\nmachine h login b\n",
			sizeof("machine h login a\nmachine h login b\n") - 1u,
			XRT_NETRC_ERROR_DUPLICATE }
	};
	size_t i;
	for ( i = 0u; i < sizeof(Cases) / sizeof(Cases[0]); ++i ) {
		xrtnetrcerror Error;
		xvalue* pDocument = xrtNetrcParseMemory(Cases[i].Data,
			Cases[i].Size, NULL, &Error);
		bool bOk = pDocument == NULL && Error.iCode == Cases[i].Code;
		xrtValueRelease(pDocument);
		if ( !bOk ) {
			fprintf(stderr, "reject case %zu failed: code %d\n", i, Error.iCode);
			return false;
		}
	}
	return true;
}

static bool test_scanner_error_locations(void)
{
	static const struct {
		const char* Text;
		uint64 Column;
	} Cases[] = {
		{ "machine \"unfinished", 9u },
		{ "macdef \"unfinished", 8u },
		{ "machine h login \"unfinished", 17u }
	};
	size_t i;
	for ( i = 0u; i < sizeof(Cases) / sizeof(Cases[0]); ++i ) {
		xrtnetrcerror Error;
		xvalue* pDocument = xrtNetrcParseMemory(Cases[i].Text,
			strlen(Cases[i].Text), NULL, &Error);
		bool bOk = pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_SYNTAX &&
			Error.iLine == 1u && Error.iColumn == Cases[i].Column &&
			strstr(Error.sMessage, "unterminated quoted netrc token") != NULL;
		xrtValueRelease(pDocument);
		if ( !bOk ) {
			fprintf(stderr, "scanner error case %zu: code=%d column=%llu message=%s\n",
				i, Error.iCode, (unsigned long long)Error.iColumn, Error.sMessage);
			return false;
		}
	}
	{
		static const char sComment[] = "# note\0hidden\nmachine h login a\n";
		xrtnetrcerror Error;
		xvalue* pDocument = xrtNetrcParseMemory(sComment,
			sizeof(sComment) - 1u, NULL, &Error);
		bool bOk = pDocument == NULL &&
			Error.iCode == XRT_NETRC_ERROR_SYNTAX &&
			Error.iLine == 1u && Error.iColumn == 7u &&
			Error.iOffset == 6u;
		xrtValueRelease(pDocument);
		if ( !bOk ) { return false; }
	}
	{
		static const char sMacroComment[] =
			"macdef deploy # note\0hidden\n echo deploy\n\n";
		xrtnetrcerror Error;
		xvalue* pDocument = xrtNetrcParseMemory(sMacroComment,
			sizeof(sMacroComment) - 1u, NULL, &Error);
		bool bOk = pDocument == NULL &&
			Error.iCode == XRT_NETRC_ERROR_SYNTAX &&
			Error.iLine == 1u && Error.iColumn == 21u &&
			Error.iOffset == 20u;
		xrtValueRelease(pDocument);
		if ( !bOk ) { return false; }
	}
	{
		static const char sBareCr[] =
			"machine h login a\rmachine h login b\n";
		xrtnetrcerror Error;
		xvalue* pDocument = xrtNetrcParseMemory(sBareCr,
			sizeof(sBareCr) - 1u, NULL, &Error);
		bool bOk = pDocument == NULL &&
			Error.iCode == XRT_NETRC_ERROR_DUPLICATE &&
			Error.iLine == 2u && Error.iColumn == 1u &&
			Error.iOffset == 18u;
		xrtValueRelease(pDocument);
		if ( !bOk ) {
			fprintf(stderr, "bare CR diagnostic: code=%d line=%llu column=%llu offset=%llu\n",
				Error.iCode, (unsigned long long)Error.iLine,
				(unsigned long long)Error.iColumn,
				(unsigned long long)Error.iOffset);
			return false;
		}
	}
	{
		static const char sCommentCr[] =
			"machine h login a # note\rmachine h login b\n";
		xrtnetrcerror Error;
		xvalue* pDocument = xrtNetrcParseMemory(sCommentCr,
			sizeof(sCommentCr) - 1u, NULL, &Error);
		bool bOk = pDocument == NULL &&
			Error.iCode == XRT_NETRC_ERROR_DUPLICATE &&
			Error.iLine == 2u && Error.iColumn == 1u &&
			Error.iOffset == (uint64)(strchr(sCommentCr, '\r') - sCommentCr + 1);
		xrtValueRelease(pDocument);
		if ( !bOk ) {
			fprintf(stderr, "comment CR boundary: code=%d line=%llu column=%llu\n",
				Error.iCode, (unsigned long long)Error.iLine,
				(unsigned long long)Error.iColumn);
			return false;
		}
	}
	return true;
}

static bool test_writer_validation(void)
{
	const char* sSource = "machine h login a password p\nmacdef run\n echo ok\n\n";
	xrtnetrcerror Error;
	xrtnetrcoptions Options;
	xvalue* pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &Error);
	xvalue* pHosts;
	xvalue* pAuth;
	xvalue* pMacros;
	xvalue* pLines;
	xbuffer* pOutput;
	xvalue* pBad;
	bool bOk = false;
	if ( pDocument == NULL ) { return false; }
	xrtNetrcOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 3u;
	pOutput = xrtNetrcStringify(pDocument, &Options, &Error);
	if ( pOutput != NULL || Error.iCode != XRT_NETRC_ERROR_LIMIT ) {
		xrtBufferDestroy(pOutput);
		goto cleanup;
	}
	pHosts = xrtValueObjectGet(pDocument, xrtStrView("hosts"));
	pAuth = xrtValueObjectGet(pHosts, xrtStrView("h"));
	pBad = xrtValueString((xstrview){ "a\0b", 3u });
	if ( pBad == NULL || !xrtValueObjectSetTake(pAuth, xrtStrView("password"), &pBad) ) {
		xrtValueRelease(pBad);
		goto cleanup;
	}
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	if ( pOutput != NULL || Error.iCode != XRT_NETRC_ERROR_TYPE ) {
		xrtBufferDestroy(pOutput);
		goto cleanup;
	}
	pBad = xrtValueString(xrtStrView("p"));
	if ( pBad == NULL || !xrtValueObjectSetTake(pAuth, xrtStrView("password"), &pBad) ) {
		xrtValueRelease(pBad);
		goto cleanup;
	}
	{
		const char sInvalidUtf8[] = { (char)0xff };
		pBad = xrtValueString((xstrview){ sInvalidUtf8, sizeof(sInvalidUtf8) });
		if ( pBad == NULL || !xrtValueObjectSetTake(pAuth,
			xrtStrView("password"), &pBad) ) {
			xrtValueRelease(pBad);
			goto cleanup;
		}
		pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
		if ( pOutput != NULL || Error.iCode != XRT_NETRC_ERROR_UTF8 ) {
			xrtBufferDestroy(pOutput);
			goto cleanup;
		}
	}
	pBad = xrtValueString(xrtStrView("p"));
	if ( pBad == NULL || !xrtValueObjectSetTake(pAuth, xrtStrView("password"), &pBad) ) {
		xrtValueRelease(pBad);
		goto cleanup;
	}
	/* 写出器不能成功返回却悄悄丢弃调用方新增的凭据字段。 */
	pBad = xrtValueString(xrtStrView("hidden"));
	if ( pBad == NULL || !xrtValueObjectSetTake(pAuth, xrtStrView("token"), &pBad) ) {
		xrtValueRelease(pBad);
		goto cleanup;
	}
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	if ( pOutput != NULL || Error.iCode != XRT_NETRC_ERROR_TYPE ) {
		xrtBufferDestroy(pOutput);
		goto cleanup;
	}
	if ( !xrtValueObjectRemove(pAuth, xrtStrView("token")) ) { goto cleanup; }
	pBad = xrtValueString(xrtStrView("hidden"));
	if ( pBad == NULL || !xrtValueObjectSetTake(pDocument, xrtStrView("secret"), &pBad) ) {
		xrtValueRelease(pBad);
		goto cleanup;
	}
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	if ( pOutput != NULL || Error.iCode != XRT_NETRC_ERROR_TYPE ) {
		xrtBufferDestroy(pOutput);
		goto cleanup;
	}
	if ( !xrtValueObjectRemove(pDocument, xrtStrView("secret")) ) { goto cleanup; }
	pMacros = xrtValueObjectGet(pDocument, xrtStrView("macros"));
	pLines = xrtValueObjectGet(pMacros, xrtStrView("run"));
	pBad = xrtValueString(xrtStrView("  "));
	if ( pBad == NULL || !xrtValueArrayAppendTake(pLines, &pBad) ) {
		xrtValueRelease(pBad);
		goto cleanup;
	}
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	bOk = pOutput == NULL && Error.iCode == XRT_NETRC_ERROR_TYPE;
	xrtBufferDestroy(pOutput);
cleanup:
	if ( !bOk ) { fprintf(stderr, "writer validation failed: %s\n", Error.sMessage); }
	xrtValueRelease(pDocument);
	return bOk;
}

static bool test_writer_empty_names(void)
{
	static const char sSource[] = "machine host login user\nmacdef run\n echo ok\n\n";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &Error);
	xvalue* pHosts;
	xvalue* pMacros;
	xvalue* pEntry;
	xbuffer* pOutput;
	bool bOk = false;
	if ( pDocument == NULL ) { return false; }
	pHosts = xrtValueObjectGet(pDocument, xrtStrView("hosts"));
	pMacros = xrtValueObjectGet(pDocument, xrtStrView("macros"));
	pEntry = xrtValueObject();
	if ( pEntry == NULL || !xrtValueObjectSetTake(pHosts, xrtStrView(""),
		&pEntry) ) { xrtValueRelease(pEntry); goto cleanup; }
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	if ( pOutput != NULL || Error.iCode != XRT_NETRC_ERROR_TYPE ) {
		xrtBufferDestroy(pOutput);
		goto cleanup;
	}
	if ( !xrtValueObjectRemove(pHosts, xrtStrView("")) ) { goto cleanup; }
	pEntry = xrtValueArray();
	if ( pEntry == NULL || !xrtValueObjectSetTake(pMacros, xrtStrView(""),
		&pEntry) ) { xrtValueRelease(pEntry); goto cleanup; }
	pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
	bOk = pOutput == NULL && Error.iCode == XRT_NETRC_ERROR_TYPE;
	xrtBufferDestroy(pOutput);
cleanup:
	xrtValueRelease(pDocument);
	if ( !bOk ) { fprintf(stderr, "empty netrc entry name was serialized\n"); }
	return bOk;
}

static bool test_private_save(void)
{
	const char* sPath = "build/test/netrc_中文.test";
	const char* sSource = "machine secret.example login admin password secret\n";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource, strlen(sSource), NULL, &Error);
	xvalue* pLoaded = NULL;
	xvalue* pAfterFailure = NULL;
	xvalue* pAuth;
	xvalue* pBad;
	xfileinfo Info;
	bool bOk = false;
	if ( pDocument == NULL || !xrtDirCreateAll("build/test") ||
		!xrtNetrcSave((str)sPath, pDocument, NULL, &Error) ) { goto cleanup; }
#if !defined(_WIN32) && !defined(_WIN64)
	/* 覆盖既有宽权限文件时，新的凭据文件也必须收紧为私有。 */
	if ( !xrtPathSetMode(sPath, true, 0644u) ||
		!xrtNetrcSave((str)sPath, pDocument, NULL, &Error) ) { goto cleanup; }
#endif
	pLoaded = xrtNetrcLoad((str)sPath, NULL, &Error);
	if ( pLoaded == NULL || !xrtPathStat(sPath, false, &Info) ||
		Info.Type != XFILE_TYPE_FILE ) { goto cleanup; }
#if !defined(_WIN32) && !defined(_WIN64)
	if ( (Info.Available & XFILE_INFO_MODE) == 0u ||
		(Info.Mode & 0077u) != 0u ) { goto cleanup; }
#endif
	pAuth = xrtValueObjectGet(xrtValueObjectGet(pDocument,
		xrtStrView("hosts")), xrtStrView("secret.example"));
	pBad = xrtValueString(xrtStrView("must not be saved"));
	if ( pBad == NULL || !xrtValueObjectSetTake(pAuth, xrtStrView("token"), &pBad) ) {
		xrtValueRelease(pBad);
		goto cleanup;
	}
	if ( xrtNetrcSave((str)sPath, pDocument, NULL, &Error) ||
		Error.iCode != XRT_NETRC_ERROR_TYPE ) { goto cleanup; }
	pAfterFailure = xrtNetrcLoad((str)sPath, NULL, &Error);
	bOk = pAfterFailure != NULL &&
		text_equals(xrtValueObjectGet(xrtValueObjectGet(pAfterFailure,
			xrtStrView("hosts")), xrtStrView("secret.example")),
			"password", "secret");
cleanup:
	if ( !bOk ) { fprintf(stderr, "private save failed: %s\n", Error.sMessage); }
	xrtFileDelete(sPath);
	xrtValueRelease(pAfterFailure);
	xrtValueRelease(pLoaded);
	xrtValueRelease(pDocument);
	return bOk;
}

#if !defined(_WIN32) && !defined(_WIN64)
static bool test_symlink_target(void)
{
	static const char sSource[] =
		"machine host login user password secret\n";
	static const char sOutside[] = "outside file must remain unchanged";
	const char* sTarget = "build/test/netrc_link_target.test";
	const char* sLink = "build/test/netrc_link.test";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource,
		sizeof(sSource) - 1u, NULL, &Error);
	xvalue* pLoaded = NULL;
	bytes pOutside = NULL;
	size_t iSize = 0u;
	xfileinfo Info;
	bool bOk = false;
	if ( pDocument == NULL || !xrtDirCreateAll("build/test") ||
		!xrtFileWriteAll(sTarget, (xbytesview){
			(const uint8*)sOutside, sizeof(sOutside) - 1u }) ||
		symlink("netrc_link_target.test", sLink) != 0 ) { goto cleanup; }
	if ( !xrtNetrcSave((str)sLink, pDocument, NULL, &Error) ) { goto cleanup; }
	pOutside = xrtFileReadAll(sTarget, &iSize);
	pLoaded = xrtNetrcLoad((str)sLink, NULL, &Error);
	bOk = pOutside != NULL && iSize == sizeof(sOutside) - 1u &&
		memcmp(pOutside, sOutside, iSize) == 0 && pLoaded != NULL &&
		xrtPathStat(sLink, false, &Info) && Info.Type == XFILE_TYPE_FILE &&
		(Info.Available & XFILE_INFO_MODE) != 0u &&
		(Info.Mode & 0077u) == 0u;
cleanup:
	if ( !bOk ) fprintf(stderr, "netrc symlink target was followed or save failed\n");
	xrtFree(pOutside);
	xrtValueRelease(pLoaded);
	xrtValueRelease(pDocument);
	xrtFileDelete(sLink);
	xrtFileDelete(sTarget);
	return bOk;
}
#endif

static bool test_save_allocation_failures(void)
{
	static const char sSource[] =
		"machine secret.example login admin password secret\n";
	static const char sOriginal[] = "old credentials";
	const char* sPath = "build/test/netrc_save_alloc.test";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource,
		sizeof(sSource) - 1u, NULL, &Error);
	bool bCompleted = false;
	bool bSaveResourceFailed = false;
	uint64 iTriggered = 0u;
	uint64 i;
	if ( pDocument == NULL || !xrtDirCreateAll("build/test") ||
		!xrtFileWriteAll(sPath, (xbytesview){
			(const uint8*)sOriginal, sizeof(sOriginal) - 1u }) ) {
		xrtValueRelease(pDocument);
		return false;
	}
	for ( i = 0u; i < 128u; ++i ) {
		bool bSaved;
		bool bTriggered;
		bytes pCurrent;
		size_t iSize = 0u;
		if ( !xrtMemDebugFailAfter(i) ) { break; }
		bSaved = xrtNetrcSave((str)sPath, pDocument, NULL, &Error);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) { iTriggered++; }
		if ( bTriggered && strcmp(Error.sMessage,
			"cannot allocate netrc save resources") == 0 ) {
			bSaveResourceFailed = true;
		}
		if ( bTriggered && (bSaved || Error.iCode != XRT_NETRC_ERROR_MEMORY) ) {
			fprintf(stderr, "save allocation failure %llu returned %d\n",
				(unsigned long long)i, Error.iCode);
			break;
		}
		pCurrent = xrtFileReadAll(sPath, &iSize);
		if ( pCurrent == NULL || (bTriggered &&
			(iSize != sizeof(sOriginal) - 1u ||
			 memcmp(pCurrent, sOriginal, iSize) != 0)) ) {
			xrtFree(pCurrent);
			break;
		}
		xrtFree(pCurrent);
		if ( !bTriggered ) { bCompleted = bSaved && Error.iCode == XRT_NETRC_OK; break; }
	}
	xrtFileDelete(sPath);
	xrtValueRelease(pDocument);
	return bCompleted && bSaveResourceFailed && iTriggered >= 4u;
}

static bool test_allocation_failures(void)
{
	static const char sSource[] =
		"machine example login alice password "
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"macdef deploy\n echo ok\n\n";
	uint64 i;
	bool bCompleted = false;
	bool bTokenGrowthFailed = false;
	uint64 iTriggered = 0u;
	for ( i = 0u; i < 160u; ++i ) {
		xreader* pReader = xrtReaderFromMemory(
			(xbytesview){ (const unsigned char*)sSource, sizeof(sSource) - 1u });
		xrtnetrcerror Error;
		xvalue* pDocument;
		bool bTriggered;
		if ( pReader == NULL || !xrtMemDebugFailAfter(i) ) { return false; }
		pDocument = xrtNetrcParse(pReader, true, NULL, &Error);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) { iTriggered++; }
		if ( strcmp(Error.sMessage, "cannot grow netrc token") == 0 ) {
			if ( Error.iLine == 0u || Error.iColumn == 0u ) { return false; }
			bTokenGrowthFailed = true;
		}
		if ( bTriggered && (pDocument != NULL || Error.iCode != XRT_NETRC_ERROR_MEMORY) ) {
			fprintf(stderr, "allocation failure %llu returned %d\n",
				(unsigned long long)i, Error.iCode);
			xrtValueRelease(pDocument);
			return false;
		}
		if ( !bTriggered ) { bCompleted = pDocument != NULL; }
		xrtValueRelease(pDocument);
		if ( bCompleted ) { break; }
	}
	return bCompleted && bTokenGrowthFailed && iTriggered >= 12u;
}

static bool test_writer_allocation_failures(void)
{
	static const char sPrefix[] =
		"machine example login alice password ";
	char sSource[sizeof(sPrefix) + 601u];
	xrtnetrcerror Error;
	xvalue* pDocument;
	bool bCompleted = false;
	uint64 iTriggered = 0u;
	uint64 i;
	memcpy(sSource, sPrefix, sizeof(sPrefix) - 1u);
	memset(sSource + sizeof(sPrefix) - 1u, 'x', 600u);
	sSource[sizeof(sSource) - 2u] = '\n';
	sSource[sizeof(sSource) - 1u] = '\0';
	pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u,
		NULL, &Error);
	if ( pDocument == NULL ) { return false; }
	for ( i = 0u; i < 128u; ++i ) {
		xbuffer* pOutput;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(i) ) { break; }
		pOutput = xrtNetrcStringify(pDocument, NULL, &Error);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) { iTriggered++; }
		if ( bTriggered && (pOutput != NULL ||
			Error.iCode != XRT_NETRC_ERROR_MEMORY) ) {
			fprintf(stderr, "writer allocation failure %llu returned %d\n",
				(unsigned long long)i, Error.iCode);
			xrtBufferDestroy(pOutput);
			xrtValueRelease(pDocument);
			return false;
		}
		if ( !bTriggered ) {
			bCompleted = pOutput != NULL && Error.iCode == XRT_NETRC_OK;
		}
		xrtBufferDestroy(pOutput);
		if ( bCompleted ) { break; }
	}
	xrtValueRelease(pDocument);
	if ( !bCompleted || iTriggered < 3u ) {
		fprintf(stderr, "writer injection coverage: completed=%d triggered=%llu\n",
			(int)bCompleted, (unsigned long long)iTriggered);
	}
	return bCompleted && iTriggered >= 3u;
}

typedef struct {
	const unsigned char* Data;
	size_t Size;
	size_t Offset;
	bool FailRead;
} testreader;

static bool test_read(ptr pContext, ptr pBuffer, size_t iRequest, size_t* pRead)
{
	testreader* pSource = (testreader*)pContext;
	size_t iCount;
	if ( pSource->FailRead ) {
		xrtSetErrorInfo(XERR_IO, "test.netrc", 0, "injected reader failure");
		return false;
	}
	iCount = pSource->Size - pSource->Offset;
	if ( iCount > iRequest ) { iCount = iRequest; }
	if ( iCount != 0u ) {
		memcpy(pBuffer, pSource->Data + pSource->Offset, iCount);
		pSource->Offset += iCount;
	}
	*pRead = iCount;
	return true;
}

static bool test_size(ptr pContext, uint64* pSize)
{
	*pSize = ((testreader*)pContext)->Size;
	return true;
}

static bool test_seeked_reader_limit(void)
{
	static const char sText[] = "XXXXXXXXXXmachine h login a\n";
	const size_t iPrefix = 10u;
	const uint64 iRemaining = sizeof(sText) - 1u - iPrefix;
	xrtnetrcoptions Options;
	xrtnetrcerror Error;
	uint64 iPosition;
	int i;
	xrtNetrcOptionsInit(&Options);
	for ( i = 0; i < 2; ++i ) {
		xreader* pReader = xrtReaderFromMemory((xbytesview){
			(cbytes)sText, sizeof(sText) - 1u });
		xvalue* pDocument;
		if ( pReader == NULL || !xrtReaderSeek(pReader, (int64)iPrefix,
			XSEEK_START, &iPosition) || iPosition != iPrefix ) {
			xrtReaderDestroy(pReader);
			return false;
		}
		Options.tLimits.iMaxInputBytes = i == 0 ? iRemaining : iRemaining - 1u;
		pDocument = xrtNetrcParse(pReader, true, &Options, &Error);
		if ( (i == 0 && (pDocument == NULL || Error.iCode != XRT_NETRC_OK)) ||
			(i == 1 && (pDocument != NULL || Error.iCode != XRT_NETRC_ERROR_LIMIT)) ) {
			xrtValueRelease(pDocument);
			return false;
		}
		xrtValueRelease(pDocument);
	}
	{
		const xreaderops tOps = { test_read, NULL, NULL, test_size, NULL };
		testreader Source = { (const unsigned char*)sText,
			sizeof(sText) - 1u, iPrefix, false };
		xreader* pReader = xrtReaderCreate(&tOps, &Source);
		xvalue* pDocument;
		if ( pReader == NULL ) { return false; }
		Options.tLimits.iMaxInputBytes = iRemaining;
		pDocument = xrtNetrcParse(pReader, true, &Options, &Error);
		if ( pDocument == NULL || Error.iCode != XRT_NETRC_OK ) {
			xrtValueRelease(pDocument);
			return false;
		}
		xrtValueRelease(pDocument);
	}
	return true;
}

static bool test_reader_errors(void)
{
	static const unsigned char sText[] = "machine h login a\n";
	const xreaderops tOps = { test_read, NULL, NULL, NULL, NULL };
	testreader Source = { sText, sizeof(sText) - 1u, 0u, false };
	xrtnetrcoptions Options;
	xrtnetrcerror Error;
	xreader* pReader;
	xvalue* pDocument;

	xrtNetrcOptionsInit(&Options);
	Options.tLimits.iMaxInputBytes = 3u;
	pReader = xrtReaderCreate(&tOps, &Source);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtNetrcParse(pReader, true, &Options, &Error);
	if ( pDocument != NULL || Error.iCode != XRT_NETRC_ERROR_LIMIT ) {
		xrtValueRelease(pDocument);
		return false;
	}
	Source.Offset = 0u;
	Options.tLimits.iMaxInputBytes = 0u; /* XRT 公共合同：0 表示不限制。 */
	pReader = xrtReaderCreate(&tOps, &Source);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtNetrcParse(pReader, true, &Options, &Error);
	if ( pDocument == NULL || Error.iCode != XRT_NETRC_OK ) {
		xrtValueRelease(pDocument);
		return false;
	}
	xrtValueRelease(pDocument);
	Source.Offset = 0u;
	Source.FailRead = true;
	pReader = xrtReaderCreate(&tOps, &Source);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtNetrcParse(pReader, true, NULL, &Error);
	if ( pDocument != NULL || Error.iCode != XRT_NETRC_ERROR_IO ) {
		xrtValueRelease(pDocument);
		return false;
	}
	Source.FailRead = false;
	pReader = xrtReaderCreate(&tOps, &Source);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtNetrcParse(pReader, true, NULL, &Error);
	if ( pDocument == NULL || Error.iCode != XRT_NETRC_OK || xrtGetError() != NULL ) {
		xrtValueRelease(pDocument);
		return false;
	}
	xrtValueRelease(pDocument);
	return true;
}

typedef struct {
	const char* Text;
	size_t Size;
	size_t Offset;
	size_t Written;
	int Closed;
} closestream;

static bool close_probe_read(ptr pContext, ptr pData, size_t iSize, size_t* pRead)
{
	closestream* pProbe = (closestream*)pContext;
	size_t iRemain = pProbe->Size - pProbe->Offset;
	*pRead = iSize < iRemain ? iSize : iRemain;
	if ( *pRead != 0u ) { memcpy(pData, pProbe->Text + pProbe->Offset, *pRead); }
	pProbe->Offset += *pRead;
	return true;
}

static bool close_probe_write(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	closestream* pProbe = (closestream*)pContext;
	(void)pData;
	pProbe->Written += iSize;
	*pWritten = iSize;
	return true;
}

static bool close_probe_fail(ptr pContext)
{
	closestream* pProbe = (closestream*)pContext;
	++pProbe->Closed;
	xrtSetErrorInfo(XERR_IO, "test.netrc", 0, "injected close failure");
	return false;
}

typedef struct {
	xerrkind Kind;
	int Calls;
} writeerrorprobe;

static bool write_probe_error(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	writeerrorprobe* pProbe = (writeerrorprobe*)pContext;
	(void)pData;
	(void)iSize;
	*pWritten = 0u;
	++pProbe->Calls;
	xrtSetErrorInfo(pProbe->Kind, "test.netrc", 0, "injected write failure");
	return false;
}

static bool write_probe_ok(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	writeerrorprobe* pProbe = (writeerrorprobe*)pContext;
	(void)pData;
	*pWritten = iSize;
	++pProbe->Calls;
	return true;
}

static bool flush_probe_error(ptr pContext)
{
	writeerrorprobe* pProbe = (writeerrorprobe*)pContext;
	++pProbe->Calls;
	xrtSetErrorInfo(pProbe->Kind, "test.netrc", 0, "injected flush failure");
	return false;
}

static bool test_writer_error_kind(void)
{
	static const char sSource[] = "machine h login a\n";
	xwriterops Ops = {0};
	writeerrorprobe Probe = {0};
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource,
		sizeof(sSource) - 1u, NULL, &Error);
	bool bOk = pDocument != NULL;
	int i;
	for ( i = 0; bOk && i < 4; ++i ) {
		xwriter* pWriter;
		Ops.Write = i < 2 ? write_probe_error : write_probe_ok;
		Ops.Flush = i < 2 ? NULL : flush_probe_error;
		Probe.Kind = (i % 2) == 0 ? XERR_MEMORY : XERR_IO;
		Probe.Calls = 0;
		pWriter = xrtWriterCreate(&Ops, &Probe);
		if ( pWriter == NULL ) { bOk = false; break; }
		bOk = !xrtNetrcWrite(pDocument, pWriter, false, NULL, &Error) &&
			Probe.Calls == (i < 2 ? 1 : 2) && Error.iCode == ((i % 2) == 0 ?
			XRT_NETRC_ERROR_MEMORY : XRT_NETRC_ERROR_IO);
		if ( !bOk ) {
			fprintf(stderr, "writer error kind %d became %d\n", i, Error.iCode);
		}
		(void)xrtWriterDestroy(pWriter);
	}
	xrtValueRelease(pDocument);
	xrtClearError();
	return bOk;
}

static bool test_owned_stream_close_errors(void)
{
	static const char sSource[] = "machine h login a\n";
	xreaderops ReaderOps = {0};
	xwriterops WriterOps = {0};
	closestream ReadProbe = { sSource, sizeof(sSource) - 1u, 0u, 0u, 0 };
	closestream WriteProbe = {0};
	xrtnetrcerror Error;
	xreader* pReader;
	xwriter* pWriter;
	xvalue* pDocument;
	bool bOk;

	ReaderOps.Read = close_probe_read;
	ReaderOps.Close = close_probe_fail;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtNetrcParse(pReader, true, NULL, &Error);
	bOk = pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_IO &&
		ReadProbe.Offset == ReadProbe.Size && ReadProbe.Closed == 1;
	xrtValueRelease(pDocument);
	if ( !bOk ) { fprintf(stderr, "owned reader close failure was lost\n"); return false; }
	xrtClearError();

	ReadProbe.Offset = 0u;
	ReadProbe.Closed = 0;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtNetrcParse(pReader, false, NULL, &Error);
	bOk = pDocument != NULL && Error.iCode == XRT_NETRC_OK &&
		ReadProbe.Closed == 0;
	bOk = bOk && !xrtReaderDestroy(pReader) && ReadProbe.Closed == 1;
	if ( !bOk ) { xrtValueRelease(pDocument); return false; }
	xrtClearError();

	WriterOps.Write = close_probe_write;
	WriterOps.Close = close_probe_fail;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( pWriter == NULL ) { xrtValueRelease(pDocument); return false; }
	bOk = !xrtNetrcWrite(pDocument, pWriter, true, NULL, &Error) &&
		Error.iCode == XRT_NETRC_ERROR_IO && WriteProbe.Written != 0u &&
		WriteProbe.Closed == 1;
	if ( !bOk ) { fprintf(stderr, "owned writer close failure was lost\n");
		xrtValueRelease(pDocument); return false; }
	xrtClearError();

	WriteProbe.Written = 0u;
	WriteProbe.Closed = 0;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( pWriter == NULL ) { xrtValueRelease(pDocument); return false; }
	bOk = xrtNetrcWrite(pDocument, pWriter, false, NULL, &Error) &&
		Error.iCode == XRT_NETRC_OK && WriteProbe.Closed == 0;
	bOk = bOk && !xrtWriterDestroy(pWriter) && WriteProbe.Closed == 1;
	xrtValueRelease(pDocument);
	xrtClearError();
	if ( !bOk ) { return false; }

	/* 关闭也失败时，不覆盖已经确定的解析或文档类型错误。 */
	ReadProbe.Text = "machine h password \"unterminated";
	ReadProbe.Size = strlen(ReadProbe.Text);
	ReadProbe.Offset = 0u;
	ReadProbe.Closed = 0;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( pReader == NULL ) { return false; }
	pDocument = xrtNetrcParse(pReader, true, NULL, &Error);
	bOk = pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_SYNTAX &&
		ReadProbe.Closed == 1;
	xrtValueRelease(pDocument);
	xrtClearError();
	WriteProbe.Written = 0u;
	WriteProbe.Closed = 0;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( pWriter == NULL ) { return false; }
	bOk = bOk && !xrtNetrcWrite(NULL, pWriter, true, NULL, &Error) &&
		Error.iCode == XRT_NETRC_ERROR_TYPE && WriteProbe.Closed == 1;
	xrtClearError();
	return bOk;
}

static bool test_reader_factory_errors(void)
{
	static const char sText[] = "machine h login a\n";
	xrtnetrcerror Error;
	xvalue* pDocument;
	bool bTriggered;
	if ( !xrtMemDebugFailAfter(0u) ) { return false; }
	pDocument = xrtNetrcParseMemory(sText, sizeof(sText) - 1u, NULL, &Error);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( !bTriggered || pDocument != NULL || Error.iCode != XRT_NETRC_ERROR_MEMORY ) {
		xrtValueRelease(pDocument);
		return false;
	}
	pDocument = xrtNetrcLoad((str)"build/test/absent_netrc_file", NULL, &Error);
	if ( pDocument != NULL || Error.iCode != XRT_NETRC_ERROR_IO ) {
		xrtValueRelease(pDocument);
		return false;
	}
	pDocument = xrtNetrcParse(NULL, false, NULL, &Error);
	return pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
}

static bool test_writer_argument_errors(void)
{
	static const char sSource[] = "machine host login user password secret\n";
	xrtnetrcerror Error;
	xvalue* pDocument = xrtNetrcParseMemory(sSource,
		sizeof(sSource) - 1u, NULL, &Error);
	bool bOk;
	if ( pDocument == NULL ) { return false; }
	bOk = !xrtNetrcWrite(pDocument, NULL, false, NULL, &Error) &&
		Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	bOk = bOk && !xrtNetrcSave(NULL, pDocument, NULL, &Error) &&
		Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	bOk = bOk && !xrtNetrcSave((str)"", pDocument, NULL, &Error) &&
		Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtValueRelease(pDocument);
	return bOk;
}

static bool test_invalid_options(void)
{
	static const char sSource[] = "machine host login user password secret\n";
	xrtnetrcoptions Options;
	xrtnetrcerror Error;
	xvalue* pDocument;
	xbuffer* pOutput;
	bool bOk = true;
	uint32 i;

	xrtNetrcOptionsInit(&Options);
	Options.tLimits.iMaxInputBytes = 3u;
	Options.iVersion++;
	pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u,
		&Options, &Error);
	bOk = pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtValueRelease(pDocument);
	Options.iVersion = XRT_NETRC_OPTIONS_VERSION;
	Options.iSize = sizeof(Options) - 1u;
	pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u,
		&Options, &Error);
	bOk = bOk && pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtValueRelease(pDocument);
	Options.iSize = sizeof(Options);
	Options.tLimits.iVersion++;
	pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u,
		&Options, &Error);
	bOk = bOk && pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtValueRelease(pDocument);
	Options.tLimits.iVersion = XRT_RESOURCE_LIMITS_VERSION;
	Options.tLimits.iSize = sizeof(Options.tLimits) - 1u;
	pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u,
		&Options, &Error);
	bOk = bOk && pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtValueRelease(pDocument);
	Options.tLimits.iSize = sizeof(Options.tLimits);
	pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u,
		&Options, &Error);
	bOk = bOk && pDocument == NULL && Error.iCode == XRT_NETRC_ERROR_LIMIT;
	xrtValueRelease(pDocument);

	pDocument = xrtNetrcParseMemory(sSource, sizeof(sSource) - 1u, NULL, &Error);
	if ( pDocument == NULL ) { return false; }
	xrtNetrcOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 3u;
	Options.iVersion++;
	pOutput = xrtNetrcStringify(pDocument, &Options, &Error);
	bOk = bOk && pOutput == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtBufferDestroy(pOutput);
	Options.iVersion = XRT_NETRC_OPTIONS_VERSION;
	Options.iSize = sizeof(Options) - 1u;
	pOutput = xrtNetrcStringify(pDocument, &Options, &Error);
	bOk = bOk && pOutput == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtBufferDestroy(pOutput);
	Options.iSize = sizeof(Options);
	Options.tLimits.iVersion++;
	pOutput = xrtNetrcStringify(pDocument, &Options, &Error);
	bOk = bOk && pOutput == NULL && Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
	xrtBufferDestroy(pOutput);
	Options.tLimits.iVersion = XRT_RESOURCE_LIMITS_VERSION;
	pOutput = xrtNetrcStringify(pDocument, &Options, &Error);
	bOk = bOk && pOutput == NULL && Error.iCode == XRT_NETRC_ERROR_LIMIT;
	xrtBufferDestroy(pOutput);
	for ( i = 0u; i < 5u; ++i ) {
		xrtNetrcOptionsInit(&Options);
		bOk = bOk && Options.tLimits.iMaxDepth == 0u &&
			Options.tLimits.iMaxCompressionRatio == 0u;
		if ( i == 0u ) { Options.iReserved[0] = 1u; }
		if ( i == 1u ) { Options.tLimits.iReserved = 1u; }
		if ( i == 2u ) { Options.tLimits.iFlags = XRT_RESOURCE_ALLOW_SYMLINKS; }
		if ( i == 3u ) { Options.tLimits.iMaxDepth = 1u; }
		if ( i == 4u ) { Options.tLimits.iMaxCompressionRatio = 1u; }
		pOutput = xrtNetrcStringify(pDocument, &Options, &Error);
		bOk = bOk && pOutput == NULL &&
			Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
		xrtBufferDestroy(pOutput);
		{
			xvalue* pParsed = xrtNetrcParseMemory(sSource,
				sizeof(sSource) - 1u, &Options, &Error);
			bOk = bOk && pParsed == NULL &&
				Error.iCode == XRT_NETRC_ERROR_ARGUMENT;
			xrtValueRelease(pParsed);
		}
	}
	xrtValueRelease(pDocument);
	return bOk;
}

int main(void)
{
	str sPrevious = xrtPathCwd();
	str sTemporary = sPrevious != NULL ?
		xrtDirTemp(NULL, "xlang-netrc-test-", "") : NULL;
	str sWorkspace = sTemporary != NULL ? xrtPathAbs(sTemporary) : NULL;
	bool bOk = sPrevious != NULL && sWorkspace != NULL;
	if ( !bOk ) {
		fprintf(stderr, "test_netrc_boundaries: cannot create isolated test directory\n");
		if ( sTemporary != NULL ) { xrtDirRemoveAll(sTemporary); }
		xrtFree(sWorkspace);
		xrtFree(sTemporary);
		xrtFree(sPrevious);
		return 1;
	}
	if ( !xrtPathSetCwd(sWorkspace) ) {
		fprintf(stderr, "test_netrc_boundaries: cannot enter test directory\n");
		bOk = false;
	} else {
		bOk = test_roundtrip() && test_default_written_last() &&
			test_macro_crlf() &&
			test_mutation_roundtrip() && test_rejections() &&
			test_scanner_error_locations() &&
			test_writer_validation() && test_writer_empty_names() &&
			test_private_save() && test_save_allocation_failures() &&
			test_allocation_failures() && test_writer_allocation_failures() &&
			test_reader_errors() && test_seeked_reader_limit() &&
			test_owned_stream_close_errors() &&
			test_writer_error_kind() &&
			test_reader_factory_errors() &&
			test_writer_argument_errors() && test_invalid_options();
#if !defined(_WIN32) && !defined(_WIN64)
		if ( bOk ) { bOk = test_symlink_target(); }
#endif
		if ( !xrtPathSetCwd(sPrevious) ) {
			fprintf(stderr, "test_netrc_boundaries: cannot restore work directory\n");
			/* 当前目录可能仍在临时目录内，不递归删除它。 */
			xrtFree(sWorkspace);
			xrtFree(sTemporary);
			xrtFree(sPrevious);
			return 1;
		}
	}
	if ( !xrtDirRemoveAll(sWorkspace) ) {
		fprintf(stderr, "test_netrc_boundaries: cannot remove test directory\n");
		bOk = false;
	}
	xrtFree(sWorkspace);
	xrtFree(sTemporary);
	xrtFree(sPrevious);
	if ( bOk ) { puts("test_netrc_boundaries: PASS"); }
	return bOk ? 0 : 1;
}

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#include "../xrt_netrc_writer.c"
