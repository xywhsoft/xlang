/* INI 使用 IO/file，独立 C 测试显式请求完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"

#include <math.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/resource.h>
#endif



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_ini: %s\n", sMessage != NULL && sMessage[0] != 0 ?
		sMessage : "check failed");
	return 0;
}

static int textEquals(const char* sValue, const char* sExpected)
{
	return sValue != NULL && sExpected != NULL && strcmp(sValue, sExpected) == 0;
}

static int sameIniValues(const xrtiniconfig* pLeft, const xrtiniconfig* pRight)
{
	uint32 iSection, iKey;
	if ( xrtIniSectionCount(pLeft) != xrtIniSectionCount(pRight) ) { return 0; }
	for ( iSection = 0u; iSection < xrtIniSectionCount(pLeft); ++iSection ) {
		const char* sSection = xrtIniSectionName(pLeft, iSection);
		if ( sSection == NULL || !xrtIniHasSection(pRight, (str)sSection) ||
			xrtIniKeyCount(pLeft, (str)sSection) !=
			xrtIniKeyCount(pRight, (str)sSection) ) { return 0; }
		for ( iKey = 0u; iKey < xrtIniKeyCount(pLeft, (str)sSection); ++iKey ) {
			const char* sKey = xrtIniKeyName(pLeft, (str)sSection, iKey);
			const char* sLeft = sKey != NULL ?
				xrtIniGetRaw(pLeft, (str)sSection, (str)sKey) : NULL;
			const char* sRight = sKey != NULL ?
				xrtIniGetRaw(pRight, (str)sSection, (str)sKey) : NULL;
			if ( sLeft == NULL || sRight == NULL || strcmp(sLeft, sRight) != 0 ) {
				return 0;
			}
		}
	}
	return 1;
}

/* 畸变输入即使能解析，也不能写出一份自身无法再读取的配置。 */
static int testMutationRoundTrip(void)
{
	static const char* const aSeeds[] = {
		"# note\n[server]\nhost=localhost\nport=8080\n\n[app]\nname=demo\n",
		"root=value\n[empty]\n[utf8]\nname=\xe4\xb8\xad\xe6\x96\x87\n"
	};
	static const unsigned char aChanges[] = { 0u, '\n', '=', '[', '#', 0xffu };
	char sInput[128];
	size_t iSeed, iLength, iPosition, iChange;
	int bOk = 1;
	for ( iSeed = 0u; iSeed < sizeof(aSeeds) / sizeof(aSeeds[0]); ++iSeed ) {
		iLength = strlen(aSeeds[iSeed]);
		for ( iPosition = 0u; iPosition <= iLength; ++iPosition ) {
			for ( iChange = 0u; iChange <= sizeof(aChanges); ++iChange ) {
				xrtiniconfig* pConfig;
				xrtiniconfig* pAgain = NULL;
				xbuffer* pOutput = NULL;
				xbuffer* pCanonical = NULL;
				size_t iSize = iChange == sizeof(aChanges) ? iPosition : iLength;
				memcpy(sInput, aSeeds[iSeed], iLength);
				if ( iChange < sizeof(aChanges) && iPosition == iLength ) { continue; }
				if ( iChange < sizeof(aChanges) ) {
					sInput[iPosition] = (char)aChanges[iChange];
				}
				pConfig = xrtIniParseMemory(sInput, iSize, NULL);
				if ( !check(pConfig != NULL, "INI mutation parser returned no result") ) {
					return 0;
				}
				if ( xrtIniError(pConfig)->iCode == XRT_INI_OK ) {
					pOutput = xrtIniStringify(pConfig);
					if ( pOutput != NULL ) {
						pAgain = xrtIniParseMemory(pOutput->Data, pOutput->Size, NULL);
						bOk &= check(pAgain != NULL &&
							xrtIniError(pAgain)->iCode == XRT_INI_OK,
							"INI writer emitted unreadable mutated input");
						if ( pAgain != NULL && xrtIniError(pAgain)->iCode == XRT_INI_OK ) {
							bOk &= check(sameIniValues(pConfig, pAgain),
								"INI writer changed mutated input values");
							pCanonical = xrtIniStringify(pAgain);
							bOk &= check(pCanonical != NULL &&
								pOutput->Size == pCanonical->Size &&
								(pOutput->Size == 0u || memcmp(pOutput->Data,
									pCanonical->Data, pOutput->Size) == 0),
								"INI canonical output changed after reparse");
						}
					} else {
						bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT,
							"INI writer failed without a format error");
					}
				} else {
					bOk &= check(xrtIniError(pConfig)->iCode != XRT_INI_ERROR_MEMORY,
						"INI mutation exhausted memory without fault injection");
				}
				xrtBufferDestroy(pCanonical);
				xrtIniDestroy(pAgain);
				xrtBufferDestroy(pOutput);
				xrtIniDestroy(pConfig);
				if ( !bOk ) { return 0; }
			}
		}
	}
	return bOk;
}

static int testOptionVersions(void)
{
	static const char sText[] = "name=value\n";
	xrtinioptions Options;
	xrtiniconfig* pConfig;
	int bOk = 1;

	xrtIniOptionsInit(&Options);
	Options.tLimits.iVersion += 1u;
	pConfig = xrtIniCreate(&Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI accepted an unknown resource limits version");
	xrtIniDestroy(pConfig);
	pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, &Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI parser accepted an unknown limits version");
	xrtIniDestroy(pConfig);

	Options.tLimits.iVersion = XRT_RESOURCE_LIMITS_VERSION;
	Options.tLimits.iSize = sizeof(Options.tLimits) - 1u;
	pConfig = xrtIniCreate(&Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI accepted a truncated resource limits struct");
	xrtIniDestroy(pConfig);

	Options.tLimits.iSize = sizeof(Options.tLimits);
	Options.tLimits.iReserved = 1u;
	pConfig = xrtIniCreate(&Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI accepted a nonzero reserved resource field");
	xrtIniDestroy(pConfig);
	pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, &Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI parser accepted a nonzero reserved resource field");
	xrtIniDestroy(pConfig);

	Options.tLimits.iReserved = 0u;
	Options.tLimits.iMaxInputBytes = 3u;
	pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, &Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_LIMIT, "valid INI limits no longer constrain input");
	xrtIniDestroy(pConfig);

	/* 通用资源结构中的键数是全配置限额，未知标志不得静默生效。 */
	xrtIniOptionsInit(&Options);
	bOk &= check(Options.tLimits.iMaxCompressionRatio == 0u,
		"INI options inherited an unused compression ratio limit");
	Options.tLimits.iFlags = 1u;
	pConfig = xrtIniCreate(&Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI accepted an unsupported resource flag");
	xrtIniDestroy(pConfig);
	xrtIniOptionsInit(&Options);
	Options.bReserved = true;
	pConfig = xrtIniCreate(&Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI accepted a reserved option");
	xrtIniDestroy(pConfig);
	xrtIniOptionsInit(&Options);
	Options.tLimits.iMaxCompressionRatio = 1u;
	pConfig = xrtIniCreate(&Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_ARGUMENT, "INI accepted an unused compression ratio limit");
	xrtIniDestroy(pConfig);
	xrtIniOptionsInit(&Options);
	Options.tLimits.iMaxEntries = 1u;
	pConfig = xrtIniParseMemory("a=1\nb=2\n", 8u, &Options);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_LIMIT, "INI parser ignored the total key limit");
	xrtIniDestroy(pConfig);
	pConfig = xrtIniCreate(&Options);
	bOk &= check(pConfig != NULL && xrtIniSet(pConfig, (str)"s", (str)"a", (str)"1") &&
		!xrtIniSet(pConfig, (str)"s", (str)"b", (str)"2") &&
		xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"INI mutation ignored the total key limit");
	bOk &= check(xrtIniRemove(pConfig, (str)"s", (str)"a") &&
		xrtIniSet(pConfig, (str)"s", (str)"b", (str)"2"),
		"INI did not reclaim key capacity after removal");
	xrtIniDestroy(pConfig);
	return bOk;
}

typedef struct {
	const unsigned char* Data;
	size_t Size;
	size_t Offset;
	bool FailRead;
} testreader;

static bool testRead(ptr pContext, ptr pBuffer, size_t iRequest, size_t* pRead)
{
	testreader* pSource = (testreader*)pContext;
	size_t iCount;
	if ( pSource->FailRead ) {
		xrtSetErrorInfo(XERR_IO, "test.ini", 0, "injected reader failure");
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

static bool testSizeOnly(ptr pContext, uint64* pSize)
{
	*pSize = ((testreader*)pContext)->Size;
	return true;
}

/* Reader 的 Size 是总长度，输入限制只应比较当前位置后的字节数。 */
static int testSeekedReaderLimit(void)
{
	static const unsigned char sText[] = "XXXXXXXXXXa=123\n";
	const uint64 iPrefix = 10u;
	const uint64 iRemaining = sizeof(sText) - 1u - iPrefix;
	xrtinioptions Options;
	int i;
	xrtIniOptionsInit(&Options);
	for ( i = 0; i < 2; ++i ) {
		xreader* pReader = xrtReaderFromMemory((xbytesview){
			sText, sizeof(sText) - 1u });
		xrtiniconfig* pConfig;
		uint64 iPosition = 0u;
		int bOk;
		if ( pReader == NULL || !xrtReaderSeek(pReader, (int64)iPrefix,
			XSEEK_START, &iPosition) || iPosition != iPrefix ) {
			xrtReaderDestroy(pReader);
			return 0;
		}
		Options.tLimits.iMaxInputBytes = i == 0 ? iRemaining : iRemaining - 1u;
		pConfig = xrtIniParse(pReader, true, &Options);
		bOk = pConfig != NULL && (i == 0 ?
			(xrtIniError(pConfig)->iCode == XRT_INI_OK &&
			 textEquals(xrtIniGetRaw(pConfig, (str)"", (str)"a"), "123")) :
			xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT);
		if ( !bOk ) { fputs("INI seeked reader limit mismatch\n", stderr); }
		xrtIniDestroy(pConfig);
		if ( !bOk ) { return 0; }
	}
	{
		const xreaderops Ops = { testRead, NULL, NULL, testSizeOnly, NULL };
		testreader Source = { sText, sizeof(sText) - 1u,
			(size_t)iPrefix, false };
		xreader* pReader = xrtReaderCreate(&Ops, &Source);
		xrtiniconfig* pConfig;
		int bOk;
		if ( pReader == NULL ) { return 0; }
		Options.tLimits.iMaxInputBytes = iRemaining;
		pConfig = xrtIniParse(pReader, true, &Options);
		bOk = pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK &&
			textEquals(xrtIniGetRaw(pConfig, (str)"", (str)"a"), "123");
		xrtIniDestroy(pConfig);
		if ( !bOk ) { return 0; }
	}
	return 1;
}

static int testReaderErrors(void)
{
	static const unsigned char sText[] = "a=123\n";
	const xreaderops tOps = { testRead, NULL, NULL, NULL, NULL };
	testreader tSource = { sText, sizeof(sText) - 1u, 0u, false };
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	xreader* pReader;
	int bOk = 1;

	xrtIniOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 3u;
	pReader = xrtReaderCreate(&tOps, &tSource);
	pConfig = xrtIniParse(pReader, true, &tOptions);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"unknown-size INI reader limit was reported as IO");
	xrtIniDestroy(pConfig);

	tSource.Offset = 0u;
	tOptions.tLimits.iMaxInputBytes = 0u; /* XRT 公共合同：0 表示不限制。 */
	pReader = xrtReaderCreate(&tOps, &tSource);
	pConfig = xrtIniParse(pReader, true, &tOptions);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK &&
		textEquals(xrtIniGetRaw(pConfig, (str)"", (str)"a"), "123"),
		"zero INI input limit should allow nonempty input");
	xrtIniDestroy(pConfig);

	tSource.Offset = 0u;
	tSource.FailRead = true;
	pReader = xrtReaderCreate(&tOps, &tSource);
	pConfig = xrtIniParse(pReader, true, NULL);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_IO,
		"INI reader failure was not reported as IO");
	xrtIniDestroy(pConfig);

	tSource.FailRead = false;
	pReader = xrtReaderCreate(&tOps, &tSource);
	pConfig = xrtIniParse(pReader, true, NULL);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK &&
		xrtGetError() == NULL, "successful unknown-size INI read kept a size probe error");
	xrtIniDestroy(pConfig);

	pReader = xrtReaderFromMemory((xbytesview){ sText, sizeof(sText) - 1u });
	bOk &= check(xrtMemDebugFailAfter(1u), "cannot inject INI buffer allocation failure");
	pConfig = xrtIniParse(pReader, true, NULL);
	bOk &= check(xrtMemDebugFailTriggered(), "INI buffer allocation fault was not reached");
	xrtMemDebugFailClear();
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_MEMORY,
		"INI buffer allocation failure was reported as IO");
	xrtIniDestroy(pConfig);
	return bOk;
}

typedef struct {
	const char* Text;
	size_t Size;
	size_t Offset;
	size_t Written;
	int Closed;
} closestream;

static bool closeProbeRead(ptr pContext, ptr pData, size_t iSize, size_t* pRead)
{
	closestream* pProbe = (closestream*)pContext;
	size_t iRemain = pProbe->Size - pProbe->Offset;
	*pRead = iSize < iRemain ? iSize : iRemain;
	if ( *pRead != 0u ) { memcpy(pData, pProbe->Text + pProbe->Offset, *pRead); }
	pProbe->Offset += *pRead;
	return true;
}

static bool closeProbeWrite(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	closestream* pProbe = (closestream*)pContext;
	(void)pData;
	pProbe->Written += iSize;
	*pWritten = iSize;
	return true;
}

static bool closeProbeMemoryWrite(ptr pContext, const void* pData,
	size_t iSize, size_t* pWritten)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	*pWritten = 0u;
	xrtSetErrorInfo(XERR_MEMORY, "test.ini", 0, "injected write allocation failure");
	return false;
}

static bool closeProbeFail(ptr pContext)
{
	closestream* pProbe = (closestream*)pContext;
	++pProbe->Closed;
	xrtSetErrorInfo(XERR_IO, "test.ini", 0, "injected close failure");
	return false;
}

static int testStreamCloseFailures(void)
{
	static const char sSource[] = "a=123\n";
	xreaderops ReaderOps = {0};
	xwriterops WriterOps = {0};
	closestream ReadProbe = { sSource, sizeof(sSource) - 1u, 0u, 0u, 0 };
	closestream WriteProbe = {0};
	xreader* pReader;
	xwriter* pWriter;
	xrtiniconfig* pConfig;
	xrtinioptions Options;
	int bOk = 1;

	ReaderOps.Read = closeProbeRead;
	ReaderOps.Close = closeProbeFail;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( !check(pReader != NULL, "cannot create INI close-failing reader") ) return 0;
	pConfig = xrtIniParse(pReader, true, NULL);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_IO &&
		ReadProbe.Offset == ReadProbe.Size && ReadProbe.Closed == 1,
		"owned INI reader close failure was lost");
	xrtIniDestroy(pConfig);
	xrtClearError();

	ReadProbe.Offset = 0u;
	ReadProbe.Closed = 0;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( !check(pReader != NULL, "cannot create borrowed INI reader") ) return 0;
	pConfig = xrtIniParse(pReader, false, NULL);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK &&
		ReadProbe.Closed == 0, "INI parser closed borrowed reader");
	bOk &= check(!xrtReaderDestroy(pReader) && ReadProbe.Closed == 1,
		"borrowed INI reader ownership changed");
	xrtClearError();

	WriterOps.Write = closeProbeWrite;
	WriterOps.Close = closeProbeFail;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( !check(pWriter != NULL, "cannot create INI close-failing writer") ) {
		xrtIniDestroy(pConfig); return 0;
	}
	xrtSetErrorInfo(XERR_MEMORY, "test.ini", 0, "stale memory error");
	bOk &= check(!xrtIniWrite(pConfig, pWriter, true) &&
		xrtIniError(pConfig)->iCode == XRT_INI_ERROR_IO &&
		WriteProbe.Written != 0u && WriteProbe.Closed == 1,
		"owned INI writer close failure was lost");
	xrtClearError();

	WriteProbe.Written = 0u;
	WriteProbe.Closed = 0;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( !check(pWriter != NULL, "cannot create borrowed INI writer") ) {
		xrtIniDestroy(pConfig); return 0;
	}
	bOk &= check(xrtIniWrite(pConfig, pWriter, false) &&
		xrtIniError(pConfig)->iCode == XRT_INI_OK && WriteProbe.Closed == 0,
		"INI writer closed borrowed stream");
	bOk &= check(!xrtWriterDestroy(pWriter) && WriteProbe.Closed == 1,
		"borrowed INI writer ownership changed");
	xrtClearError();
	WriterOps.Write = closeProbeMemoryWrite;
	WriterOps.Close = NULL;
	pWriter = xrtWriterCreate(&WriterOps, NULL);
	if ( !check(pWriter != NULL, "cannot create memory-failing INI writer") ) {
		xrtIniDestroy(pConfig); return 0;
	}
	bOk &= check(!xrtIniWrite(pConfig, pWriter, false) &&
		xrtIniError(pConfig)->iCode == XRT_INI_ERROR_MEMORY,
		"INI writer allocation failure was reported as IO");
	xrtWriterDestroy(pWriter);
	xrtIniDestroy(pConfig);
	xrtClearError();
	WriterOps.Write = closeProbeWrite;
	WriterOps.Close = closeProbeFail;

	/* 正文失败时，关闭失败不能覆盖首个格式或限额错误。 */
	ReadProbe.Text = "[x\na=1\n";
	ReadProbe.Size = strlen(ReadProbe.Text);
	ReadProbe.Offset = 0u;
	ReadProbe.Closed = 0;
	pReader = xrtReaderCreate(&ReaderOps, &ReadProbe);
	if ( !check(pReader != NULL, "cannot create invalid INI reader") ) return 0;
	pConfig = xrtIniParse(pReader, true, NULL);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode ==
		XRT_INI_ERROR_FORMAT && ReadProbe.Closed == 1,
		"INI close failure replaced format error");
	xrtIniDestroy(pConfig);
	xrtClearError();

	xrtIniOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 3u;
	pConfig = xrtIniParseMemory(sSource, sizeof(sSource) - 1u, &Options);
	if ( !check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK,
		"cannot create output-limited INI config") ) return 0;
	WriteProbe.Written = 0u;
	WriteProbe.Closed = 0;
	pWriter = xrtWriterCreate(&WriterOps, &WriteProbe);
	if ( !check(pWriter != NULL, "cannot create output-limited INI writer") ) {
		xrtIniDestroy(pConfig); return 0;
	}
	bOk &= check(!xrtIniWrite(pConfig, pWriter, true) &&
		xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT &&
		WriteProbe.Written == 0u && WriteProbe.Closed == 1,
		"INI close failure replaced output limit error");
	xrtIniDestroy(pConfig);
	xrtClearError();
	return bOk;
}

static int testParseAndModel(void)
{
	static const unsigned char sText[] =
		"\xef\xbb\xbf" "# leading comment\r\n"
		"global = root\r\n\r\n"
		"[server]\r\n"
		"host = 127.0.0.1\r\n"
		"port: 8080\r\n"
		"enabled = yes\r\n"
		"ratio = 1.25\r\n"
		"message = first line\r\n"
		"    second line\r\n"
		"; before name\r\n"
		"name = \xe6\xb5\x8b\xe8\xaf\x95\r\n";
	xrtiniconfig* pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, NULL);
	int64 iPort = 0;
	double fRatio = 0.0;
	bool bEnabled = false;
	xbuffer* pOutput;
	int bOk = 1;
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK,
		pConfig != NULL ? xrtIniError(pConfig)->sMessage : "cannot create INI config");
	if ( pConfig == NULL || xrtIniError(pConfig)->iCode != XRT_INI_OK ) {
		xrtIniDestroy(pConfig);
		return 0;
	}
	bOk &= check(xrtIniSectionCount(pConfig) == 2u &&
		textEquals(xrtIniSectionName(pConfig, 0u), "") &&
		textEquals(xrtIniSectionName(pConfig, 1u), "server"),
		"INI section order mismatch");
	bOk &= check(xrtIniKeyCount(pConfig, (str)"server") == 6u &&
		textEquals(xrtIniKeyName(pConfig, (str)"server", 0u), "host"),
		"INI key order mismatch");
	bOk &= check(textEquals(xrtIniGetRaw(pConfig, (str)"", (str)"global"), "root"),
		"INI global value mismatch");
	bOk &= check(textEquals(xrtIniGetRaw(pConfig, (str)"server", (str)"message"),
		"first line\nsecond line"), "INI continuation mismatch");
	bOk &= check(xrtIniGetInt(pConfig, (str)"server", (str)"port", &iPort) && iPort == 8080,
		"INI int conversion mismatch");
	bOk &= check(xrtIniGetFloat(pConfig, (str)"server", (str)"ratio", &fRatio) &&
		fabs(fRatio - 1.25) < 0.000001, "INI float conversion mismatch");
	bOk &= check(xrtIniGetBool(pConfig, (str)"server", (str)"enabled", &bEnabled) && bEnabled,
		"INI bool conversion mismatch");

	bOk &= check(xrtIniSet(pConfig, (str)"server", (str)"port", (str)"9090") &&
		xrtIniSet(pConfig, (str)"client", (str)"timeout", (str)"30"),
		"INI set failed");
	bOk &= check(xrtIniRemove(pConfig, (str)"server", (str)"enabled") &&
		!xrtIniHas(pConfig, (str)"server", (str)"enabled"), "INI remove failed");
	pOutput = xrtIniStringify(pConfig);
	{
		str sOutput = pOutput != NULL ?
			xrtStrDupN((const char*)pOutput->Data, pOutput->Size) : NULL;
		bOk &= check(sOutput != NULL &&
			strstr((const char*)sOutput, "# leading comment\n") != NULL &&
			strstr((const char*)sOutput, "; before name\nname = \xe6\xb5\x8b\xe8\xaf\x95\n") != NULL &&
			strstr((const char*)sOutput, "message = first line\n    second line\n") != NULL,
			"INI comments or multiline output were not preserved");
		xrtFree(sOutput);
	}
	xrtBufferDestroy(pOutput);
	bOk &= check(xrtIniRemoveSection(pConfig, (str)"client") &&
		!xrtIniHasSection(pConfig, (str)"client"), "INI section removal failed");
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testWritableCase(const char* sSection, const char* sKey,
	const char* sValue, bool bWritable)
{
	xrtiniconfig* pConfig = xrtIniCreate(NULL);
	xrtiniconfig* pAgain = NULL;
	xbuffer* pOutput = NULL;
	const char* sAgain;
	int bOk = pConfig != NULL && xrtIniSet(pConfig, (str)sSection,
		(str)sKey, (str)sValue);
	if ( bOk ) {
		pOutput = xrtIniStringify(pConfig);
		if ( bWritable ) {
			pAgain = pOutput != NULL ? xrtIniParseMemory(pOutput->Data,
				pOutput->Size, NULL) : NULL;
			sAgain = pAgain != NULL ? xrtIniGetRaw(pAgain,
				(str)sSection, (str)sKey) : NULL;
			bOk = pAgain != NULL && xrtIniError(pAgain)->iCode == XRT_INI_OK &&
				textEquals(sAgain, sValue);
		} else {
			bOk = pOutput == NULL &&
				xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT;
		}
	}
	xrtIniDestroy(pAgain);
	xrtBufferDestroy(pOutput);
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testWritableContract(void)
{
	str sDirectory = NULL;
	str sPath = NULL;
	const char sOld[] = "keep old";
	static const struct {
		const char* Section;
		const char* Key;
		const char* Value;
		bool Writable;
	} Cases[] = {
		{ "s", "a", "first\nsecond", true },
		{ "s", "a", "\nsecond", true },
		{ "s", "a", "", true },
		{ "s", "a", "first\n", false },
		{ "s", "a", "first\n\nsecond", false },
		{ "s", "a", "first\n#comment", false },
		{ "s", "a", "first\n;comment", false },
		{ "s", "a", "first\n second", false },
		{ "s", "a", "first\nsecond ", false },
		{ "s", "a", "first\rsecond", false },
		{ "s", "a", " 42", false },
		{ "s", "a", "42 ", false },
		{ "bad\nsection", "a", "1", false },
		{ " bad ", "a", "1", false },
		{ "s", "bad=key", "1", false },
		{ "s", "bad:key", "1", false },
		{ "s", "#key", "1", false },
		{ "s", "[key]", "]", false }
	};
	xrtiniconfig* pConfig;
	xrtiniconfig* pAgain;
	xbuffer* pOutput;
	int bOk = 1;
	size_t i;
	for ( i = 0u; i < sizeof(Cases) / sizeof(Cases[0]); ++i ) {
		if ( !testWritableCase(Cases[i].Section, Cases[i].Key,
			Cases[i].Value, Cases[i].Writable) ) {
			fprintf(stderr, "INI writable case %zu failed\n", i);
			bOk = 0;
		}
	}
	/* 程序可在具名节之后添加全局键，写盘仍须归入全局节。 */
	pConfig = xrtIniCreate(NULL);
	pOutput = NULL;
	pAgain = NULL;
	if ( pConfig == NULL ||
		!xrtIniSet(pConfig, (str)"server", (str)"port", (str)"8080") ||
		!xrtIniSet(pConfig, (str)"", (str)"global", (str)"root") ) {
		bOk = 0;
	} else {
		pOutput = xrtIniStringify(pConfig);
		pAgain = pOutput != NULL ? xrtIniParseMemory(pOutput->Data,
			pOutput->Size, NULL) : NULL;
		if ( pAgain == NULL || xrtIniError(pAgain)->iCode != XRT_INI_OK ||
			!textEquals(xrtIniGetRaw(pAgain, (str)"", (str)"global"), "root") ||
			!textEquals(xrtIniGetRaw(pAgain, (str)"server", (str)"port"), "8080") ) {
			fprintf(stderr, "INI global section order changed key ownership\n");
			bOk = 0;
		}
	}
	xrtIniDestroy(pAgain);
	xrtBufferDestroy(pOutput);
	xrtIniDestroy(pConfig);
	/* 格式错误在原子写盘之前被拒绝，不碰已有文件。 */
	sDirectory = xrtDirTemp(NULL, "xlang-ini-format-", "");
	sPath = sDirectory != NULL ? xrtPathJoin(sDirectory, "format.ini") : NULL;
	pConfig = xrtIniCreate(NULL);
	if ( sPath == NULL || pConfig == NULL ||
		!xrtIniSet(pConfig, (str)"s", (str)"k", (str)"bad\n") ||
		!xrtFileWriteAll(sPath,
			(xbytesview){ (const uint8*)sOld, sizeof(sOld) - 1u }) ) {
		bOk = 0;
	} else {
		bytes pData;
		size_t iSize = 0u;
		bOk &= check(!xrtIniSave(pConfig, sPath) &&
			xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT,
			"unrepresentable INI value did not reject save");
		pData = xrtFileReadAllLimit(sPath, 64u, &iSize);
		bOk &= check(pData != NULL && iSize == sizeof(sOld) - 1u &&
			memcmp(pData, sOld, iSize) == 0,
			"format-rejected INI save changed the previous file");
		xrtFree(pData);
	}
	if ( sPath != NULL && xrtFileExists(sPath) ) {
		bOk &= check(xrtFileDelete(sPath), "cannot remove INI format test file");
	}
	if ( sDirectory != NULL ) {
		bOk &= check(xrtDirRemove(sDirectory), "cannot remove INI format test directory");
	}
	xrtFree(sPath);
	xrtFree(sDirectory);
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testStrictInt(void)
{
	xrtiniconfig* pConfig = xrtIniCreate(NULL);
	int64 iValue = 77;
	int bOk = check(pConfig != NULL, "cannot create INI int test config");
	if ( pConfig == NULL ) { return 0; }
	bOk &= check(xrtIniSet(pConfig, (str)"s", (str)"n", (str)" 42"),
		"cannot set leading-space INI value");
	bOk &= check(!xrtIniGetInt(pConfig, (str)"s", (str)"n", &iValue) &&
		iValue == 77 && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_CONVERSION,
		"INI int accepted leading whitespace");
	bOk &= check(xrtIniSet(pConfig, (str)"s", (str)"n", (str)"-42") &&
		xrtIniGetInt(pConfig, (str)"s", (str)"n", &iValue) &&
		iValue == -42 && xrtGetError() == NULL,
		"valid INI int did not recover after conversion error");
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testFloatLocale(void)
{
	static const char sText[] = "[server]\nratio=1.25\ncomma=1,25\n";
	const char* candidates[] = { "German_Germany.1252", "de_DE.UTF-8", "fr_FR.UTF-8" };
	const char* oldLocale = setlocale(LC_NUMERIC, NULL);
	str savedLocale = xrtStrDup(oldLocale != NULL ? oldLocale : "C");
	xrtiniconfig* pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, NULL);
	double fValue = 0.0;
	bool foundComma = false;
	int bOk = 1;
	size_t i;

	if (savedLocale == NULL || pConfig == NULL) {
		xrtFree(savedLocale);
		xrtIniDestroy(pConfig);
		return check(0, "INI locale fixture allocation failed");
	}
	for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
		if (setlocale(LC_NUMERIC, candidates[i]) != NULL &&
			strcmp(localeconv()->decimal_point, ",") == 0) {
			foundComma = true;
			break;
		}
	}
	if (foundComma) {
		bOk &= check(xrtIniGetFloat(pConfig, (str)"server", (str)"ratio", &fValue) &&
			fabs(fValue - 1.25) < 0.000001,
			"INI decimal float depends on process locale");
		bOk &= check(!xrtIniGetFloat(pConfig, (str)"server", (str)"comma", &fValue) &&
			xrtIniError(pConfig)->iCode == XRT_INI_ERROR_CONVERSION,
			"INI comma decimal was accepted under process locale");
		bOk &= check(xrtIniGetFloat(pConfig, (str)"server", (str)"ratio", &fValue) &&
			fabs(fValue - 1.25) < 0.000001 && xrtGetError() == NULL,
			"successful INI conversion kept the prior error");
	}
	setlocale(LC_NUMERIC, (const char*)savedLocale);
	xrtFree(savedLocale);
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testInterpolation(void)
{
	const char sText[] =
		"[server]\n"
		"host = localhost\n"
		"port = 8080\n"
		"url = http://${host}:${server.port}/api\n";
	const char sCycle[] = "[x]\na = ${x.b}\nb = ${x.a}\n";
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	str sValue;
	int bOk = 1;
	xrtIniOptionsInit(&tOptions);
	tOptions.bInterpolation = true;
	pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, &tOptions);
	sValue = xrtIniGet(pConfig, (str)"server", (str)"url");
	bOk &= check(sValue != NULL && strcmp((const char*)sValue,
		"http://localhost:8080/api") == 0, "INI interpolation mismatch");
	xrtFree(sValue);
	xrtIniDestroy(pConfig);

	pConfig = xrtIniParseMemory(sCycle, sizeof(sCycle) - 1u, &tOptions);
	sValue = xrtIniGet(pConfig, (str)"x", (str)"a");
	bOk &= check(sValue == NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_INTERPOLATION,
		"INI interpolation cycle was accepted");
	xrtFree(sValue);
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testGetAllocationErrors(void)
{
	const char sText[] = "[s]\nx=123\ny=A${x}B\n";
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	str sValue;
	char sLongValue[8193];
	size_t i;
	bool bTriggered;
	bool bSawOutputFailure = false;
	bool bSawCopyFailure = false;
	int bOk = 1;

	pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, NULL);
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK,
		"cannot prepare non-interpolated INI allocation test");
	if ( pConfig == NULL ) { return 0; }
	bOk &= check(xrtMemDebugFailAfter(0u), "cannot inject INI get allocation failure");
	sValue = xrtIniGet(pConfig, (str)"s", (str)"x");
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	bOk &= check(bTriggered && sValue == NULL &&
		xrtIniError(pConfig)->iCode == XRT_INI_ERROR_MEMORY,
		"INI raw value copy failure was not reported as memory error");
	xrtFree(sValue);
	xrtIniDestroy(pConfig);

	xrtIniOptionsInit(&tOptions);
	tOptions.bInterpolation = true;
	memset(sLongValue, 'z', sizeof(sLongValue) - 1u);
	sLongValue[sizeof(sLongValue) - 1u] = 0;
	pConfig = xrtIniCreate(&tOptions);
	bOk &= check(pConfig != NULL &&
		xrtIniSet(pConfig, (str)"s", (str)"x", (str)sLongValue) &&
		xrtIniSet(pConfig, (str)"s", (str)"y", (str)"A${x}B") &&
		xrtIniError(pConfig)->iCode == XRT_INI_OK,
		"cannot prepare interpolated INI allocation test");
	if ( pConfig == NULL ) { return 0; }
	for ( i = 0u; i < 32u; ++i ) {
		bOk &= check(xrtMemDebugFailAfter(i), "cannot inject INI interpolation failure");
		sValue = xrtIniGet(pConfig, (str)"s", (str)"y");
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( !bTriggered ) {
			bOk &= check(sValue != NULL && strlen((const char*)sValue) == 8194u &&
				sValue[0] == 'A' && sValue[8193] == 'B',
				"INI interpolation did not recover after fault scan");
			xrtFree(sValue);
			break;
		}
		bOk &= check(sValue == NULL &&
			xrtIniError(pConfig)->iCode == XRT_INI_ERROR_MEMORY,
			"INI interpolation allocation failure was misclassified");
		if ( strcmp(xrtIniError(pConfig)->sMessage,
			"cannot grow INI interpolation output") == 0 ) {
			bSawOutputFailure = true;
		}
		if ( strcmp(xrtIniError(pConfig)->sMessage,
			"cannot copy resolved INI value") == 0 ) {
			bSawCopyFailure = true;
		}
		xrtFree(sValue);
	}
	bOk &= check(i < 32u, "INI allocation scan did not reach success");
	bOk &= check(bSawOutputFailure && bSawCopyFailure,
		"INI allocation scan missed output growth or final copy");
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testLeadingCommentAllocation(void)
{
	static const char sText[] = "# leading\nname=value\n";
	size_t i;
	int bOk = 1;
	int bSawSuccess = 0;
	for ( i = 0u; i < 48u; ++i ) {
		xrtiniconfig* pConfig;
		bool bTriggered;
		bOk &= check(xrtMemDebugFailAfter(i),
			"cannot inject leading-comment allocation failure");
		pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, NULL);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			if ( pConfig != NULL &&
				xrtIniError(pConfig)->iCode != XRT_INI_ERROR_MEMORY ) {
				fprintf(stderr, "leading-comment failure at allocation %zu: error %d\n",
					i, xrtIniError(pConfig)->iCode);
			}
			bOk &= check((pConfig == NULL ||
				xrtIniError(pConfig)->iCode == XRT_INI_ERROR_MEMORY) &&
				xrtGetError() != NULL &&
				xrtErrorKind(xrtGetError()) == XERR_MEMORY,
				"leading-comment allocation failure lost its memory error kind");
		} else {
			xbuffer* pOutput;
			bOk &= check(pConfig != NULL &&
				xrtIniError(pConfig)->iCode == XRT_INI_OK &&
				textEquals(xrtIniGetRaw(pConfig, (str)"", (str)"name"), "value"),
				"INI parser did not recover after leading-comment fault scan");
			pOutput = pConfig != NULL ? xrtIniStringify(pConfig) : NULL;
			bOk &= check(pOutput != NULL &&
				pOutput->Size >= sizeof("# leading\n") - 1u &&
				memcmp(pOutput->Data, "# leading\n", sizeof("# leading\n") - 1u) == 0,
				"INI parser dropped the default section's leading comment");
			xrtBufferDestroy(pOutput);
			bSawSuccess = 1;
		}
		xrtIniDestroy(pConfig);
		if ( !bTriggered ) { break; }
	}
	bOk &= check(bSawSuccess, "leading-comment fault scan did not reach success");
	return bOk;
}

static int testErrorsAndLimits(void)
{
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	int bOk = 1;
	{
		const char sDuplicate[] = "[x]\na=1\na=2\n";
		pConfig = xrtIniParseMemory(sDuplicate, sizeof(sDuplicate) - 1u, NULL);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_DUPLICATE,
		"duplicate INI key was accepted");
	xrtIniDestroy(pConfig);
	{
		const char sMalformed[] = "[x\na=1\n";
		pConfig = xrtIniParseMemory(sMalformed, sizeof(sMalformed) - 1u, NULL);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT,
		"malformed INI section was accepted");
	xrtIniDestroy(pConfig);
	{
		const char sNul[] = "[server]\nname=visible\0hidden\n";
		pConfig = xrtIniParseMemory(sNul, sizeof(sNul) - 1u, NULL);
	}
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT,
		"embedded NUL in INI input was silently truncated");
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iLine == 2u &&
		xrtIniError(pConfig)->iColumn == 13u && xrtIniError(pConfig)->iOffset == 21u,
		"embedded NUL error location is incorrect");
	xrtIniDestroy(pConfig);
	{
		const char sInvalidUtf8[] = "\xef\xbb\xbf[one]\r\nname=\xff\n";
		pConfig = xrtIniParseMemory(sInvalidUtf8, sizeof(sInvalidUtf8) - 1u, NULL);
	}
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT &&
		xrtIniError(pConfig)->iLine == 2u &&
		xrtIniError(pConfig)->iColumn == 6u && xrtIniError(pConfig)->iOffset == 15u,
		"invalid UTF-8 error location must include the BOM offset");
	xrtIniDestroy(pConfig);
	{
		const char sInvalidFirstLine[] = "\xef\xbb\xbfname=\xff\n";
		pConfig = xrtIniParseMemory(sInvalidFirstLine,
			sizeof(sInvalidFirstLine) - 1u, NULL);
	}
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT &&
		xrtIniError(pConfig)->iLine == 1u &&
		xrtIniError(pConfig)->iColumn == 6u && xrtIniError(pConfig)->iOffset == 8u,
		"BOM must not count toward the first line's column");
	xrtIniDestroy(pConfig);
	{
		const char sNulSection[] = "[hidden\0alias]\nkey=value\n";
		pConfig = xrtIniParseMemory(sNulSection, sizeof(sNulSection) - 1u, NULL);
	}
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT,
		"embedded NUL in section name was accepted");
	xrtIniDestroy(pConfig);
	pConfig = xrtIniCreate(NULL);
	bOk &= check(!xrtIniSet(pConfig, (str)"orphan", (str)"", (str)"value") &&
		!xrtIniHasSection(pConfig, (str)"orphan"),
		"failed INI set left an empty section behind");
	xrtIniDestroy(pConfig);

	xrtIniOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 3u;
	{
		const char sLimited[] = "a=123\n";
		pConfig = xrtIniParseMemory(sLimited, sizeof(sLimited) - 1u, &tOptions);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"INI input limit was not enforced");
	xrtIniDestroy(pConfig);
	xrtIniOptionsInit(&tOptions);
	tOptions.tLimits.iMaxItemBytes = 2u;
	{
		const char sItem[] = "abc=1\n";
		pConfig = xrtIniParseMemory(sItem, sizeof(sItem) - 1u, &tOptions);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"INI item limit was not enforced");
	xrtIniDestroy(pConfig);
	xrtIniOptionsInit(&tOptions);
	tOptions.tLimits.iMaxNodes = 2u;
	{
		const char sNodes[] = "[x]\na=1\nb=2\n";
		pConfig = xrtIniParseMemory(sNodes, sizeof(sNodes) - 1u, &tOptions);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"INI node limit was not enforced");
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testCaseAndFile(void)
{
	str sDirectory = xrtDirTemp(NULL, "xlang-ini-", "");
	str sPath = NULL;
	str sMissing = NULL;
	xrtinioptions tOptions;
	xrtiniconfig* pConfig = NULL;
	xrtiniconfig* pLoaded = NULL;
	int bOk = 1;
	if ( !check(sDirectory != NULL, "cannot create exclusive INI test directory") ) {
		return 0;
	}
	sPath = xrtPathJoin(sDirectory, "\xe9\x85\x8d\xe7\xbd\xae.ini");
	sMissing = xrtPathJoin(sDirectory, "absent.ini");
	if ( !check(sPath != NULL && sMissing != NULL,
		"cannot create INI test paths") ) {
		bOk = 0;
		goto done;
	}
	xrtIniOptionsInit(&tOptions);
	tOptions.bCaseSensitive = false;
	pConfig = xrtIniParseMemory("[Main]\nName=Value\n", 18u, &tOptions);
	bOk &= check(pConfig != NULL && xrtIniHas(pConfig, (str)"main", (str)"name"),
		"case-insensitive INI lookup failed");
	if ( pConfig == NULL ) { goto done; }
	pLoaded = xrtIniLoad(sMissing, &tOptions);
	bOk &= check(pLoaded != NULL &&
		xrtIniError(pLoaded)->iCode == XRT_INI_ERROR_IO,
		"missing INI file must report IO rather than argument failure");
	xrtIniDestroy(pLoaded);
	pLoaded = NULL;
	pLoaded = xrtIniLoad(NULL, &tOptions);
	bOk &= check(pLoaded != NULL &&
		xrtIniError(pLoaded)->iCode == XRT_INI_ERROR_ARGUMENT,
		"null INI path must remain an argument failure");
	xrtIniDestroy(pLoaded);
	pLoaded = NULL;
	bOk &= check(xrtIniSave(pConfig, sPath), xrtIniError(pConfig)->sMessage);
	if ( bOk ) {
		size_t i;
		bool bReachedSaveSuccess = false;
		for ( i = 0u; i < 64u; ++i ) {
			bool bTriggered, bSaved;
			bOk &= check(xrtMemDebugFailAfter(i),
				"cannot inject INI file save allocation failure");
			bSaved = xrtIniSave(pConfig, sPath);
			bTriggered = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			bOk &= check(bTriggered ? (!bSaved &&
				xrtIniError(pConfig)->iCode == XRT_INI_ERROR_MEMORY) :
				(bSaved && xrtIniError(pConfig)->iCode == XRT_INI_OK),
				"INI file save allocation failure was misclassified");
			xrtClearError();
			if ( !bTriggered ) bReachedSaveSuccess = true;
			if ( !bTriggered || !bOk ) break;
		}
		bOk &= check(bReachedSaveSuccess || !bOk,
			"INI file save fault scan did not reach success");
		if ( !bOk ) goto done;
		bool bReachedSuccess = false;
		for ( i = 0u; i < 64u; ++i ) {
			bool bTriggered;
			bOk &= check(xrtMemDebugFailAfter(i),
				"cannot inject INI file load allocation failure");
			pLoaded = xrtIniLoad(sPath, &tOptions);
			bTriggered = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			if ( bTriggered ) {
				bOk &= check(pLoaded == NULL ||
					xrtIniError(pLoaded)->iCode == XRT_INI_ERROR_MEMORY,
					"INI file load allocation failure was misclassified");
			} else {
				bOk &= check(pLoaded != NULL &&
					xrtIniError(pLoaded)->iCode == XRT_INI_OK &&
					xrtIniHas(pLoaded, (str)"MAIN", (str)"NAME"),
					"INI file load did not recover after allocation faults");
				bReachedSuccess = true;
			}
			xrtIniDestroy(pLoaded);
			pLoaded = NULL;
			if ( !bTriggered || !bOk ) { break; }
		}
		bOk &= check(bReachedSuccess, "INI file load fault scan did not reach success");
	}
	pLoaded = xrtIniLoad(sPath, &tOptions);
	bOk &= check(pLoaded != NULL && xrtIniHas(pLoaded, (str)"MAIN", (str)"NAME"),
		"UTF-8 INI path round trip failed");
done:
	xrtIniDestroy(pLoaded);
	xrtIniDestroy(pConfig);
	if ( sPath != NULL && xrtFileExists(sPath) ) {
		bOk &= check(xrtFileDelete(sPath), "cannot remove INI test file");
	}
	bOk &= check(xrtDirRemove(sDirectory), "cannot remove INI test directory");
	xrtFree(sMissing);
	xrtFree(sPath);
	xrtFree(sDirectory);
	return bOk;
}

#ifndef _WIN32
static int testSaveFailurePreservesFile(void)
{
	str sDirectory = NULL;
	str sPath = NULL;
	const char sOld[] = "keep old";
	xrtiniconfig* pConfig;
	struct rlimit Saved, Limited;
	void (*pHandler)(int);
	bytes pData;
	size_t iSize = 0u;
	int bOk = 1, bSaved;
	pConfig = xrtIniParseMemory("[Main]\nName=Value\n", 18u, NULL);
	if ( !check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK,
		"cannot create INI atomic write fixture") ) return 0;
	sDirectory = xrtDirTemp(NULL, "xlang-ini-atomic-", "");
	sPath = sDirectory != NULL ? xrtPathJoin(sDirectory, "atomic.ini") : NULL;
	if ( !check(sPath != NULL, "cannot create exclusive INI atomic test path") ) {
		bOk = 0;
		goto done;
	}
	bOk &= check(xrtFileWriteAll(sPath,
		(xbytesview){ (const uint8*)sOld, sizeof(sOld) - 1u }),
		"cannot create prior INI file");
	if ( !bOk ) goto done;
	if ( !check(getrlimit(RLIMIT_FSIZE, &Saved) == 0 &&
		Saved.rlim_cur > 4u, "cannot inspect file size limit") ) {
		bOk = 0;
		goto done;
	}
	Limited = Saved;
	Limited.rlim_cur = 4u;
	pHandler = signal(SIGXFSZ, SIG_IGN);
	if ( !check(pHandler != SIG_ERR, "cannot mask file size signal") ) {
		bOk = 0;
		goto done;
	}
	if ( !check(setrlimit(RLIMIT_FSIZE, &Limited) == 0,
		"cannot limit file writes") ) {
		bOk = 0;
		signal(SIGXFSZ, pHandler);
		goto done;
	}
	bSaved = xrtIniSave(pConfig, sPath);
	bOk &= check(setrlimit(RLIMIT_FSIZE, &Saved) == 0,
		"cannot restore file size limit");
	bOk &= check(signal(SIGXFSZ, pHandler) != SIG_ERR,
		"cannot restore file size signal");
	bOk &= check(!bSaved && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_IO,
		"limited INI write must report IO failure");
	pData = xrtFileReadAllLimit(sPath, 64u, &iSize);
	bOk &= check(pData != NULL && iSize == sizeof(sOld) - 1u &&
		memcmp(pData, sOld, iSize) == 0,
		"failed INI write changed the previous file");
	xrtFree(pData);
done:
	if ( sPath != NULL && xrtFileExists(sPath) ) {
		bOk &= check(xrtFileDelete(sPath), "cannot remove INI atomic test file");
	}
	if ( sDirectory != NULL ) {
		bOk &= check(xrtDirRemove(sDirectory), "cannot remove INI atomic test directory");
	}
	xrtFree(sPath);
	xrtFree(sDirectory);
	xrtIniDestroy(pConfig);
	return bOk;
}
#endif



int main(void)
{
	int bOk;
	bOk = testParseAndModel();
	bOk &= testOptionVersions();
	bOk &= testMutationRoundTrip();
	bOk &= testWritableContract();
	bOk &= testStrictInt();
	bOk &= testFloatLocale();
	bOk &= testInterpolation();
	bOk &= testGetAllocationErrors();
	bOk &= testLeadingCommentAllocation();
	bOk &= testErrorsAndLimits();
	bOk &= testReaderErrors();
	bOk &= testSeekedReaderLimit();
	bOk &= testStreamCloseFailures();
	bOk &= testCaseAndFile();
#ifndef _WIN32
	bOk &= testSaveFailurePreservesFile();
#endif
	if ( bOk ) { puts("test_ini: PASS"); }
	return bOk ? 0 : 1;
}

/* 单文件 C smoke：声明、实现与测试共享同一份 XRT 模块配置。 */
#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
