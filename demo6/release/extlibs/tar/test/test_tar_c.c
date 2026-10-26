#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_DIR_TEMP

#include "../include/xrt_tar.h"
#include "../xrt_tar_internal.h"

#include <stdio.h>
#include <string.h>



typedef struct tarprogresscontext {
	int iEntries;
	int iFinalEvents;
	int iCancelAfter;
	bool bCancelFinal;
	uint64 iLastInput;
	uint64 iTotalInput;
	bool bMonotonic;
	bool bFinalFlag;
	bool bTotalKnown;
} tarprogresscontext;

typedef struct tarselectcontext {
	cstr sName;
	int iCalls;
	bool bFail;
} tarselectcontext;



/* 判断普通文件是否与给定字节完全一致。 */
static int fileEquals(cstr sPath, const void* pExpected, size_t iExpected)
{
	bytes pData;
	size_t iSize = 0u;
	int bResult;

	pData = xrtFileReadAll(sPath, &iSize);
	bResult = pData != NULL && iSize == iExpected &&
		(iExpected == 0u || memcmp(pData, pExpected, iExpected) == 0);
	xrtFree(pData);
	return bResult;
}



/* 记录逐条提取进度，并按配置在完整条目边界取消。 */
static bool onExtractProgress(const xrttarentry* pEntry,
	const xrtprogress* pProgress, ptr pUserData)
{
	tarprogresscontext* pContext = (tarprogresscontext*)pUserData;

	if ( pProgress == NULL || pContext == NULL ) { return false; }
	if ( pProgress->iInputBytes < pContext->iLastInput ) {
		pContext->bMonotonic = false;
	}
	pContext->iLastInput = pProgress->iInputBytes;
	pContext->iTotalInput = pProgress->iTotalInputBytes;
	pContext->bTotalKnown =
		(pProgress->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0u;
	if ( pEntry == NULL ) {
		pContext->iFinalEvents += 1;
		pContext->bFinalFlag =
			(pProgress->iFlags & XRT_PROGRESS_FINAL) != 0u;
		return !pContext->bCancelFinal;
	}
	pContext->iEntries += 1;
	return pContext->iCancelAfter == 0 ||
		pContext->iEntries < pContext->iCancelAfter;
}




/* 选择一个精确名称，或模拟选择器内部失败。 */
static xrttarselect selectEntry(const xrttarentry* pEntry, ptr pUserData)
{
	tarselectcontext* pContext = (tarselectcontext*)pUserData;

	pContext->iCalls += 1;
	if ( pContext->bFail ) {
		xrtSetErrorInfo(XERR_STATE, "test.tar", 1,
			"selector callback boom");
		return XRT_TAR_SELECT_ERROR;
	}
	return strcmp(pEntry->sName, pContext->sName) == 0 ?
		XRT_TAR_SELECT_EXTRACT : XRT_TAR_SELECT_SKIP;
}



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_tar: %s\n", sMessage != NULL && sMessage[0] != 0 ? sMessage : "check failed");
	return 0;
}

static xbuffer* makeArchive(const char* sName, const void* pData, size_t iSize)
{
	xrttarwriter* pWriter = xrtTarWriterCreateMemory(NULL);
	xrttarentry tEntry;
	xbuffer* pResult = NULL;
	xrtTarEntryInit(&tEntry);
	tEntry.sName = sName;
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iMode = 0644u;
	if ( pWriter != NULL && xrtTarWriterAdd(pWriter, &tEntry, pData, iSize) &&
		xrtTarWriterClose(pWriter) ) {
		pResult = xrtTarWriterTakeBuffer(pWriter);
	}
	xrtTarWriterDestroy(pWriter);
	return pResult;
}

typedef struct {
	const uint8* Data;
	size_t Size;
	size_t Offset;
	size_t FailAt;
	xerrkind Kind;
	bool FailWrite;
	bool FailFlush;
	xerrkind CloseKind;
	int CloseCalls;
} tarstreamerror;

static bool streamErrorRead(ptr pContext, ptr pData, size_t iRequest, size_t* pRead)
{
	tarstreamerror* pProbe = (tarstreamerror*)pContext;
	size_t iCount = pProbe->Size - pProbe->Offset;
	*pRead = 0u;
	if ( pProbe->Offset >= pProbe->FailAt ) {
		xrtSetErrorInfo(pProbe->Kind, "test.tar", 0, "injected read failure");
		return false;
	}
	if ( iCount > iRequest ) { iCount = iRequest; }
	if ( iCount > pProbe->FailAt - pProbe->Offset ) {
		iCount = pProbe->FailAt - pProbe->Offset;
	}
	if ( iCount != 0u ) { memcpy(pData, pProbe->Data + pProbe->Offset, iCount); }
	pProbe->Offset += iCount;
	*pRead = iCount;
	return true;
}

static bool streamErrorWrite(ptr pContext, const void* pData,
	size_t iRequest, size_t* pWritten)
{
	tarstreamerror* pProbe = (tarstreamerror*)pContext;
	(void)pData;
	*pWritten = 0u;
	if ( pProbe->FailWrite ) {
		xrtSetErrorInfo(pProbe->Kind, "test.tar", 0, "injected write failure");
		return false;
	}
	*pWritten = iRequest;
	return true;
}

static bool streamErrorFlush(ptr pContext)
{
	tarstreamerror* pProbe = (tarstreamerror*)pContext;
	if ( pProbe->FailFlush ) {
		xrtSetErrorInfo(pProbe->Kind, "test.tar", 0, "injected flush failure");
		return false;
	}
	return true;
}

static bool streamErrorClose(ptr pContext)
{
	tarstreamerror* pProbe = (tarstreamerror*)pContext;
	++pProbe->CloseCalls;
	if ( pProbe->CloseKind == XERR_NONE ) { return true; }
	xrtSetErrorInfo(pProbe->CloseKind, "test.tar", 0, "injected close failure");
	return false;
}

/* 底层流的资源故障不能伪装成损坏归档或设备 I/O。 */
static int testStreamErrorKinds(void)
{
	static const char sData[] = "data";
	xreaderops ReaderOps = {0};
	xwriterops WriterOps = {0};
	xbuffer* pPacked = makeArchive("item", sData, sizeof(sData) - 1u);
	int bOk = check(pPacked != NULL, "cannot create TAR stream error fixture");
	int i;
	ReaderOps.Read = streamErrorRead;
	WriterOps.Write = streamErrorWrite;
	WriterOps.Flush = streamErrorFlush;
	for ( i = 0; bOk && i < 2; ++i ) {
		tarstreamerror Probe = {0};
		xreader* pInput;
		xrttarreader* pReader;
		xrttarentry Entry;
		int iExpected = i == 0 ? XRT_TAR_ERROR_MEMORY : XRT_TAR_ERROR_IO;
		Probe.Data = pPacked->Data;
		Probe.Size = pPacked->Size;
		Probe.FailAt = 512u;
		Probe.Kind = i == 0 ? XERR_MEMORY : XERR_IO;
		pInput = xrtReaderCreate(&ReaderOps, &Probe);
		pReader = pInput != NULL ? xrtTarReaderCreate(pInput, false, NULL) : NULL;
		bOk &= check(pReader != NULL && xrtTarReaderNext(pReader, &Entry) &&
			!xrtTarReaderRead(pReader) &&
			xrtTarReaderError(pReader)->iCode == iExpected,
			"TAR reader changed stream failure into format error");
		xrtTarReaderDestroy(pReader);
		(void)xrtReaderDestroy(pInput);
		xrtClearError();
	}
	for ( i = 0; bOk && i < 2; ++i ) {
		tarstreamerror Probe = {0};
		xwriter* pOutput;
		xrttarreader* pReader;
		xrttarentry Entry;
		int iExpected = i == 0 ? XRT_TAR_ERROR_MEMORY : XRT_TAR_ERROR_IO;
		Probe.Kind = i == 0 ? XERR_MEMORY : XERR_IO;
		Probe.FailWrite = true;
		pOutput = xrtWriterCreate(&WriterOps, &Probe);
		pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
		bOk &= check(pOutput != NULL && pReader != NULL &&
			xrtTarReaderNext(pReader, &Entry) &&
			!xrtTarReaderReadTo(pReader, pOutput) &&
			xrtTarReaderError(pReader)->iCode == iExpected,
			"TAR reader changed output writer failure into I/O error");
		xrtTarReaderDestroy(pReader);
		(void)xrtWriterDestroy(pOutput);
		xrtClearError();
	}
	for ( i = 0; bOk && i < 4; ++i ) {
		tarstreamerror Probe = {0};
		xwriter* pOutput;
		xrttarwriter* pWriter;
		xrttarentry Entry;
		int iExpected = (i % 2) == 0 ? XRT_TAR_ERROR_MEMORY : XRT_TAR_ERROR_IO;
		Probe.Kind = (i % 2) == 0 ? XERR_MEMORY : XERR_IO;
		Probe.FailWrite = i < 2;
		Probe.FailFlush = i >= 2;
		pOutput = xrtWriterCreate(&WriterOps, &Probe);
		pWriter = pOutput != NULL ? xrtTarWriterCreate(pOutput, false, NULL) : NULL;
		xrtTarEntryInit(&Entry);
		Entry.sName = "item";
		Entry.iType = XRT_TAR_ENTRY_FILE;
		if ( i < 2 ) {
			bOk &= check(pWriter != NULL &&
				!xrtTarWriterAdd(pWriter, &Entry, sData, sizeof(sData) - 1u) &&
				xrtTarWriterError(pWriter)->iCode == iExpected,
				"TAR writer changed write failure into I/O error");
		} else {
			bOk &= check(pWriter != NULL &&
				xrtTarWriterAdd(pWriter, &Entry, sData, sizeof(sData) - 1u) &&
				!xrtTarWriterClose(pWriter) &&
				xrtTarWriterError(pWriter)->iCode == iExpected,
				"TAR writer changed flush failure into I/O error");
		}
		xrtTarWriterDestroy(pWriter);
		(void)xrtWriterDestroy(pOutput);
		xrtClearError();
	}
	xrtBufferDestroy(pPacked);
	return bOk;
}

/* 文件提取沿用同一 Writer 流程；晚发生的关闭故障不覆盖首次失败。 */
static int testExtractionWriterErrors(void)
{
	static const struct {
		bool FailWrite;
		bool FailFlush;
		xerrkind Kind;
		xerrkind CloseKind;
		int ExpectedCode;
	} arrCases[] = {
		{false, false, XERR_NONE, XERR_NONE, 0},
		{true, false, XERR_MEMORY, XERR_NONE, XRT_TAR_ERROR_MEMORY},
		{true, false, XERR_IO, XERR_NONE, XRT_TAR_ERROR_IO},
		{false, true, XERR_MEMORY, XERR_NONE, XRT_TAR_ERROR_MEMORY},
		{false, true, XERR_IO, XERR_NONE, XRT_TAR_ERROR_IO},
		{false, false, XERR_NONE, XERR_MEMORY, XRT_TAR_ERROR_MEMORY},
		{false, false, XERR_NONE, XERR_IO, XRT_TAR_ERROR_IO},
		{false, true, XERR_MEMORY, XERR_IO, XRT_TAR_ERROR_MEMORY},
		{true, false, XERR_IO, XERR_MEMORY, XRT_TAR_ERROR_IO}
	};
	static const char sData[] = "extract data";
	xwriterops Ops = {0};
	xbuffer* pPacked = makeArchive("item", sData, sizeof(sData) - 1u);
	int bOk = check(pPacked != NULL, "cannot create TAR extraction error fixture");
	size_t i;
	Ops.Write = streamErrorWrite;
	Ops.Flush = streamErrorFlush;
	Ops.Close = streamErrorClose;
	for ( i = 0u; bOk && i < sizeof(arrCases) / sizeof(arrCases[0]); ++i ) {
		tarstreamerror Probe = {0};
		xrttarreader* pReader = xrtTarReaderOpenMemory(
			pPacked->Data, pPacked->Size, NULL);
		xrttarentry Entry;
		xwriter* pOutput;
		bool bExtract;
		Probe.FailWrite = arrCases[i].FailWrite;
		Probe.FailFlush = arrCases[i].FailFlush;
		Probe.Kind = arrCases[i].Kind;
		Probe.CloseKind = arrCases[i].CloseKind;
		pOutput = xrtWriterCreate(&Ops, &Probe);
		bOk &= check(pReader != NULL && pOutput != NULL &&
			xrtTarReaderNext(pReader, &Entry),
			"cannot prepare TAR extraction output Writer");
		if ( !bOk ) {
			(void)xrtWriterDestroy(pOutput);
			xrtTarReaderDestroy(pReader);
			break;
		}
		xrtClearError();
		bExtract = __xrtTarExtractToWriter(pReader, pOutput, "item");
		if ( bExtract != (arrCases[i].ExpectedCode == 0) ||
			Probe.CloseCalls != 1 ||
			xrtTarReaderError(pReader)->iCode != arrCases[i].ExpectedCode ) {
			fprintf(stderr, "TAR extraction case %zu: result=%d close=%d error=%d expected=%d\n",
				i, bExtract, Probe.CloseCalls,
				xrtTarReaderError(pReader)->iCode, arrCases[i].ExpectedCode);
		}
		bOk &= check(bExtract == (arrCases[i].ExpectedCode == 0) &&
			Probe.CloseCalls == 1 &&
			xrtTarReaderError(pReader)->iCode == arrCases[i].ExpectedCode &&
			(arrCases[i].ExpectedCode == 0 || arrCases[i].FailWrite ||
			 strstr(xrtTarReaderError(pReader)->sMessage, "item") != NULL),
			"TAR extraction Writer failure category or close count mismatch");
		xrtTarReaderDestroy(pReader);
		xrtClearError();
	}
	xrtBufferDestroy(pPacked);
	return bOk;
}

typedef struct {
	const uint8* Data;
	size_t Size;
	size_t Offset;
} tarsizereader;

static bool sizeReaderRead(ptr pContext, ptr pData, size_t iRequest, size_t* pRead)
{
	tarsizereader* pSource = (tarsizereader*)pContext;
	size_t iCount = pSource->Size - pSource->Offset;
	if ( iCount > iRequest ) { iCount = iRequest; }
	if ( iCount != 0u ) {
		memcpy(pData, pSource->Data + pSource->Offset, iCount);
		pSource->Offset += iCount;
	}
	*pRead = iCount;
	return true;
}

static bool sizeReaderSize(ptr pContext, uint64* pSize)
{
	*pSize = ((tarsizereader*)pContext)->Size;
	return true;
}

/* TAR 限额从 Reader 当前位置起算，不包含前置的其他数据。 */
static int testSeekedReaderLimit(void)
{
	static const char sPrefix[] = "XXXXXXXXXX";
	const size_t iPrefix = sizeof(sPrefix) - 1u;
	xbuffer* pPacked = makeArchive("ok.txt", "ok", 2u);
	xbuffer* pWrapped = xrtBufferCreate();
	xrttaroptions Options;
	int bOk = pPacked != NULL && pWrapped != NULL;
	int i;
	if ( bOk ) {
		bOk = xrtBufferAppend(pWrapped, XRT_BYTES_LITERAL("XXXXXXXXXX")) &&
			xrtBufferAppend(pWrapped, (xbytesview){ pPacked->Data, pPacked->Size });
	}
	xrtTarOptionsInit(&Options);
	for ( i = 0; bOk && i < 2; ++i ) {
		xreader* pInput = xrtReaderFromMemory((xbytesview){
			pWrapped->Data, pWrapped->Size });
		xrttarreader* pReader;
		xrttarentry Entry;
		uint64 iPosition = 0u;
		if ( pInput == NULL || !xrtReaderSeek(pInput, (int64)iPrefix,
			XSEEK_START, &iPosition) || iPosition != iPrefix ) {
			xrtReaderDestroy(pInput);
			bOk = 0;
			break;
		}
		Options.tLimits.iMaxInputBytes = i == 0 ? pPacked->Size : 511u;
		pReader = xrtTarReaderCreate(pInput, true, &Options);
		if ( pReader == NULL ) {
			xrtReaderDestroy(pInput);
			bOk = 0;
			break;
		}
		if ( i == 0 ) {
			bOk = check(xrtTarReaderNext(pReader, &Entry) &&
				strcmp(Entry.sName, "ok.txt") == 0 &&
				xrtTarReaderSkip(pReader) &&
				!xrtTarReaderNext(pReader, &Entry) && xrtTarReaderEOF(pReader),
				"seeked TAR reader rejected a valid remaining-byte limit");
		} else {
			bOk = check(!xrtTarReaderNext(pReader, &Entry) &&
				xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_LIMIT,
				"seeked TAR reader ignored a too-small remaining-byte limit");
		}
		xrtTarReaderDestroy(pReader);
	}
	if ( bOk ) {
		const xreaderops Ops = { sizeReaderRead, NULL, NULL, sizeReaderSize, NULL };
		tarsizereader Source = { pWrapped->Data, pWrapped->Size, iPrefix };
		xreader* pInput = xrtReaderCreate(&Ops, &Source);
		xrttarreader* pReader;
		xrttarentry Entry;
		Options.tLimits.iMaxInputBytes = pPacked->Size;
		pReader = pInput != NULL ? xrtTarReaderCreate(pInput, true, &Options) : NULL;
		bOk = check(pReader != NULL && xrtTarReaderNext(pReader, &Entry) &&
			strcmp(Entry.sName, "ok.txt") == 0,
			"size-only TAR reader rejected a valid remaining-byte limit");
		if ( pReader != NULL ) { xrtTarReaderDestroy(pReader); }
		else { xrtReaderDestroy(pInput); }
	}
	xrtBufferDestroy(pWrapped);
	xrtBufferDestroy(pPacked);
	return bOk;
}

static int testMemoryAndPax(void)
{
	static const char sLongName[] =
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt";
	const unsigned char sBinary[] = { 0u, 1u, 2u, 0xffu };
	xrttaroptions tOptions;
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry tEntry;
	xbuffer* pPacked;
	xbuffer* pData;
	int iCount = 0;
	int bOk = 1;
	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowSymlinks = true;
	pWriter = xrtTarWriterCreateMemory(&tOptions);
	bOk &= check(pWriter != NULL, "cannot create TAR memory writer");
	bOk &= check(xrtTarWriterAddDirectory(pWriter, "folder", 0755u, 0),
		xrtTarWriterError(pWriter)->sMessage);
	{
		xrttarentry tFile;
		xrtTarEntryInit(&tFile);
		tFile.sName = "folder/hello.txt";
		tFile.iType = XRT_TAR_ENTRY_FILE;
		tFile.iMode = 0644u;
		bOk &= check(xrtTarWriterAdd(pWriter, &tFile, "hello", 5u),
			xrtTarWriterError(pWriter)->sMessage);
		tFile.sName = "binary.bin";
		bOk &= check(xrtTarWriterAdd(pWriter, &tFile, sBinary, sizeof(sBinary)),
			xrtTarWriterError(pWriter)->sMessage);
		tFile.sName = sLongName;
		bOk &= check(xrtTarWriterAdd(pWriter, &tFile, "long", 4u),
			xrtTarWriterError(pWriter)->sMessage);
	}
	bOk &= check(xrtTarWriterAddSymlink(pWriter, "hello-link", "folder/hello.txt", 0777u, 0),
		xrtTarWriterError(pWriter)->sMessage);
	bOk &= check(xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	bOk &= check(pPacked != NULL && pPacked->Size % 512u == 0u, "invalid TAR memory output");
	xrtTarWriterDestroy(pWriter);

	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	while ( xrtTarReaderNext(pReader, &tEntry) ) {
		if ( iCount == 0 ) {
			bOk &= check(tEntry.iType == XRT_TAR_ENTRY_DIRECTORY &&
				strcmp(tEntry.sName, "folder/") == 0, "TAR directory metadata mismatch");
			bOk &= check(xrtTarReaderSkip(pReader), xrtTarReaderError(pReader)->sMessage);
		}
		else if ( iCount == 1 ) {
			pData = xrtTarReaderRead(pReader);
			bOk &= check(pData != NULL && pData->Size == 5u &&
				memcmp(pData->Data, "hello", 5u) == 0, "TAR text data mismatch");
			xrtBufferDestroy(pData);
		}
		else if ( iCount == 2 ) {
			pData = xrtTarReaderRead(pReader);
			bOk &= check(pData != NULL && pData->Size == sizeof(sBinary) &&
				memcmp(pData->Data, sBinary, sizeof(sBinary)) == 0, "TAR binary data mismatch");
			xrtBufferDestroy(pData);
		}
		else if ( iCount == 3 ) {
			bOk &= check(strcmp(tEntry.sName, sLongName) == 0, "TAR PAX path mismatch");
			pData = xrtTarReaderRead(pReader);
			bOk &= check(pData != NULL && pData->Size == 4u, "TAR PAX entry data mismatch");
			xrtBufferDestroy(pData);
		}
		else {
			bOk &= check(tEntry.iType == XRT_TAR_ENTRY_SYMLINK &&
				strcmp(tEntry.sLinkName, "folder/hello.txt") == 0, "TAR link metadata mismatch");
			bOk &= check(xrtTarReaderSkip(pReader), xrtTarReaderError(pReader)->sMessage);
		}
		iCount += 1;
	}
	bOk &= check(iCount == 5 && xrtTarReaderEOF(pReader),
		xrtTarReaderError(pReader)->sMessage);
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	return bOk;
}

static void updateHeaderChecksum(unsigned char* pHeader)
{
	unsigned int iSum = 0u;
	size_t i;
	memset(pHeader + 148u, ' ', 8u);
	for ( i = 0u; i < 512u; ++i ) { iSum += pHeader[i]; }
	snprintf((char*)pHeader + 148u, 8u, "%06o", iSum);
	pHeader[154u] = 0;
	pHeader[155u] = ' ';
}

static int testMalformedPaxFields(void)
{
	static const char sSize[] = "10 size=2\n";
	static const char sNulSize[] = "15 size=2\0junk\n";
	static const char sPlusSize[] = "11 size=+2\n";
	static const char sMTime[] = "14 mtime=-1.5\n";
	static const char sNulMTime[] = "17 mtime=-1\0junk\n";
	static const char sEmptyFraction[] = "12 mtime=1.\n";
	static const char sNulPath[] = "20 path=safe\0suffix\n";
	static const char sNulUnknown[] = "16 x-foo=a\0junk\n";
	static const struct {
		const char* Data;
		size_t Size;
		int Valid;
		uint64 SizeValue;
		int64 MTime;
	} Cases[] = {
		{ sSize, sizeof(sSize) - 1u, 1, 2u, 0 },
		{ sNulSize, sizeof(sNulSize) - 1u, 0, 0u, 0 },
		{ sPlusSize, sizeof(sPlusSize) - 1u, 0, 0u, 0 },
		{ sMTime, sizeof(sMTime) - 1u, 1, 4u, -1 },
		{ sNulMTime, sizeof(sNulMTime) - 1u, 0, 0u, 0 },
		{ sEmptyFraction, sizeof(sEmptyFraction) - 1u, 0, 0u, 0 },
		{ sNulPath, sizeof(sNulPath) - 1u, 0, 0u, 0 },
		{ sNulUnknown, sizeof(sNulUnknown) - 1u, 0, 0u, 0 }
	};
	xbuffer* pOriginal = makeArchive("data", "1234", 4u);
	xbuffer* pPacked = xrtBufferCreate();
	xrttarreader* pReader = NULL;
	xrttarentry Entry;
	size_t i;
	int bOk = 1;
	if ( pOriginal == NULL || pPacked == NULL ||
		!xrtBufferResize(pPacked, pOriginal->Size + 1024u) ) {
		bOk = 0;
		goto done;
	}
	memcpy(pPacked->Data + 1024u, pOriginal->Data, pOriginal->Size);
	for ( i = 0u; i < sizeof(Cases) / sizeof(Cases[0]); ++i ) {
		int bNext;
		int bCase;
		memset(pPacked->Data, 0, 1024u);
		memcpy(pPacked->Data, pOriginal->Data, 512u);
		pPacked->Data[156u] = 'x';
		snprintf((char*)pPacked->Data + 124u, 12u, "%011o",
			(unsigned int)Cases[i].Size);
		updateHeaderChecksum(pPacked->Data);
		memcpy(pPacked->Data + 512u, Cases[i].Data, Cases[i].Size);
		pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
		bNext = pReader != NULL && xrtTarReaderNext(pReader, &Entry);
		if ( Cases[i].Valid ) {
			bCase = bNext && Entry.iSizeBytes == Cases[i].SizeValue &&
				Entry.iMTime == Cases[i].MTime;
		} else {
			bCase = !bNext && pReader != NULL &&
				xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_FORMAT;
		}
		if ( !bCase ) {
			fprintf(stderr, "PAX field case %zu: next=%d error=%d size=%llu mtime=%lld\n",
				i, bNext, pReader != NULL ? xrtTarReaderError(pReader)->iCode : -1,
				bNext ? (unsigned long long)Entry.iSizeBytes : 0u,
				bNext ? (long long)Entry.iMTime : 0);
		}
		bOk &= bCase;
		xrtTarReaderDestroy(pReader);
		pReader = NULL;
	}
done:
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtBufferDestroy(pOriginal);
	return bOk;
}

static int testSecurityAndLimits(void)
{
	cstr sRoot = "build/tmp/tar_安全_test";
	cstr sOutput = "build/tmp/tar_安全_test/output";
	cstr sFile = "build/tmp/tar_安全_test/output/目录/内容.txt";
	xbuffer* pPacked;
	xrttarreader* pReader;
	xrttaroptions tOptions;
	bytes pFileData;
	size_t iFileSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	pPacked = makeArchive("目录/内容.txt", "UTF-8", 5u);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(xrtTarReaderExtractAll(pReader, sOutput, false),
		xrtTarReaderError(pReader)->sMessage);
	pFileData = xrtFileReadAll(sFile, &iFileSize);
	bOk &= check(pFileData != NULL && iFileSize == 5u &&
		memcmp(pFileData, "UTF-8", 5u) == 0, "TAR UTF-8 extraction mismatch");
	xrtFree(pFileData);
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);

	pPacked = makeArchive("abcde", "bad", 3u);
	memcpy(pPacked->Data, "../xy", 5u);
	updateHeaderChecksum((unsigned char*)pPacked->Data);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(!xrtTarReaderExtractAll(pReader, sOutput, true) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_UNSAFE_PATH,
		"unsafe TAR extraction path was accepted");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);

	pPacked = makeArchive("data", "1234", 4u);
	xrtTarOptionsInit(&tOptions);
	tOptions.tLimits.iMaxItemBytes = 3u;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &tOptions);
	{
		xrttarentry tEntry;
		bOk &= check(!xrtTarReaderNext(pReader, &tEntry) &&
			xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_LIMIT,
			"TAR item limit was not enforced");
	}
	xrtTarReaderDestroy(pReader);

	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size - 1u, NULL);
	{
		xrttarentry tEntry;
		bOk &= check(xrtTarReaderNext(pReader, &tEntry), "truncated TAR fixture lost its first entry");
		bOk &= check(xrtTarReaderSkip(pReader) && !xrtTarReaderNext(pReader, &tEntry) &&
			!xrtTarReaderEOF(pReader) && xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_FORMAT,
			"truncated TAR end marker was accepted");
	}
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);
	return bOk;
}

static int testLinksAndSpecialEntries(void)
{
	cstr sRoot = "build/tmp/tar_links_test";
	cstr sOutput = "build/tmp/tar_links_test/output";
	cstr sTarget = "build/tmp/tar_links_test/output/target.txt";
	cstr sLink = "build/tmp/tar_links_test/output/forward.txt";
	xrttaroptions tOptions;
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry tEntry;
	xbuffer* pPacked;
	bytes pData;
	size_t iSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }

	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowHardlinks = true;
	pWriter = xrtTarWriterCreateMemory(&tOptions);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "forward.txt";
	tEntry.sLinkName = "target.txt";
	tEntry.iType = XRT_TAR_ENTRY_HARDLINK;
	tEntry.iMode = 0644u;
	bOk &= check(xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u),
		xrtTarWriterError(pWriter)->sMessage);
	tEntry.sName = "target.txt";
	tEntry.sLinkName = NULL;
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	bOk &= check(xrtTarWriterAdd(pWriter, &tEntry, "linked", 6u),
		xrtTarWriterError(pWriter)->sMessage);
	bOk &= check(xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);

	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(!xrtTarReaderExtractAll(pReader, sOutput, false) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_UNSUPPORTED,
		"TAR hard link was extracted without explicit permission");
	xrtTarReaderDestroy(pReader);
	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }

	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowHardlinks = true;
	tOptions.bPreservePermissions = true;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &tOptions);
	#if defined(_WIN32) || defined(_WIN64)
		bOk &= check(!xrtTarReaderExtractAll(pReader, sOutput, false) &&
			xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_UNSUPPORTED &&
			!xrtDirExists(sOutput),
			"Windows POSIX permissions were not rejected before extraction");
		xrtTarReaderDestroy(pReader);
		tOptions.bPreservePermissions = false;
		pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &tOptions);
	#endif
	bOk &= check(xrtTarReaderExtractAll(pReader, sOutput, false),
		xrtTarReaderError(pReader)->sMessage);
	xrtTarReaderDestroy(pReader);
	pData = xrtFileReadAll(sLink, &iSize);
	bOk &= check(pData != NULL && iSize == 6u && memcmp(pData, "linked", 6u) == 0,
		"forward TAR hard link was not resolved");
	xrtFree(pData);
	pData = xrtFileReadAll(sTarget, &iSize);
	bOk &= check(pData != NULL && iSize == 6u && memcmp(pData, "linked", 6u) == 0,
		"TAR hard-link target was not extracted");
	xrtFree(pData);
	#if !defined(_WIN32) && !defined(_WIN64)
		{
			xfileinfo Info;
			bOk &= check(xrtPathStat(sTarget, true, &Info) &&
				(Info.Available & XFILE_INFO_MODE) != 0u &&
				(Info.Mode & 0777u) == 0644u,
				"TAR file permissions were not restored");
		}
	#endif
	xrtBufferDestroy(pPacked);
	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }

	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowSpecialFiles = true;
	pWriter = xrtTarWriterCreateMemory(&tOptions);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "pipe";
	tEntry.iType = XRT_TAR_ENTRY_FIFO;
	tEntry.iMode = 0600u;
	bOk &= check(xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u),
		xrtTarWriterError(pWriter)->sMessage);
	tEntry.sName = "device";
	tEntry.iType = XRT_TAR_ENTRY_CHARACTER;
	tEntry.iDeviceMajor = 12u;
	tEntry.iDeviceMinor = 34u;
	bOk &= check(xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u),
		xrtTarWriterError(pWriter)->sMessage);
	bOk &= check(xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &tOptions);
	bOk &= check(xrtTarReaderNext(pReader, &tEntry) && tEntry.iType == XRT_TAR_ENTRY_FIFO,
		"TAR FIFO metadata was not preserved");
	bOk &= check(xrtTarReaderSkip(pReader) && xrtTarReaderNext(pReader, &tEntry) &&
		tEntry.iType == XRT_TAR_ENTRY_CHARACTER && tEntry.iDeviceMajor == 12u &&
		tEntry.iDeviceMinor == 34u, "TAR device metadata was not preserved");
	bOk &= check(xrtTarReaderSkip(pReader) && !xrtTarReaderNext(pReader, &tEntry) &&
		xrtTarReaderEOF(pReader), "TAR special-entry archive did not terminate cleanly");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	return bOk;
}






/* 验证参数错误可恢复、流式添加和整帧输出上限。 */
static int testRecoveryAndStreaming(void)
{
	xrttaroptions tOptions;
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry tEntry;
	xreader* pInput;
	xbuffer* pPacked;
	xbuffer* pData;
	int bOk = 1;

	pWriter = xrtTarWriterCreateMemory(NULL);
	bOk &= check(pWriter != NULL, "cannot create TAR recovery writer");
	bOk &= check(xrtTarWriterTakeBuffer(pWriter) == NULL &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_STATE,
		"TAR buffer was taken before close");
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "folder/";
	tEntry.iType = XRT_TAR_ENTRY_DIRECTORY;
	tEntry.iSizeBytes = 3u;
	bOk &= check(!xrtTarWriterAdd(pWriter, &tEntry, "bad", 3u) &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"TAR metadata entry silently discarded its input bytes");
	bOk &= check(!xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u) &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"TAR metadata entry silently discarded its declared size");
	tEntry.iSizeBytes = 0u;
	bOk &= check(!xrtTarWriterAdd(pWriter, &tEntry, "bad", 3u) &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"TAR metadata entry accepted unexpected input bytes");
	tEntry.sName = "../unsafe.txt";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	bOk &= check(!xrtTarWriterAdd(pWriter, &tEntry, "bad", 3u) &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_UNSAFE_PATH,
		"unsafe TAR writer path was accepted");
	tEntry.sName = "stream.txt";
	tEntry.iSizeBytes = 6u;
	pInput = xrtReaderFromMemory(XRT_BYTES_LITERAL("stream"));
	bOk &= check(pInput != NULL &&
		xrtTarWriterAddFrom(pWriter, &tEntry, pInput, 6u),
		xrtTarWriterError(pWriter)->sMessage);
	(void)xrtReaderDestroy(pInput);
	bOk &= check(xrtTarWriterClose(pWriter) && xrtTarWriterClose(pWriter),
		xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);

	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(!xrtTarReaderNext(pReader, NULL) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"null TAR entry output was accepted");
	bOk &= check(xrtTarReaderNext(pReader, &tEntry) &&
		strcmp(tEntry.sName, "stream.txt") == 0,
		"TAR reader did not recover after argument error");
	pData = xrtTarReaderRead(pReader);
	bOk &= check(pData != NULL && pData->Size == 6u &&
		memcmp(pData->Data, "stream", 6u) == 0,
		"streamed TAR entry data mismatch");
	xrtBufferDestroy(pData);
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);

	xrtTarOptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 1536u;
	pWriter = xrtTarWriterCreateMemory(&tOptions);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "too-large.bin";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	pInput = xrtReaderFromMemory(XRT_BYTES_LITERAL("x"));
	bOk &= check(!xrtTarWriterAddFrom(pWriter, &tEntry, pInput, 1537u) &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_LIMIT,
		"TAR streaming output limit was not preflighted");
	(void)xrtReaderDestroy(pInput);
	tEntry.sName = "empty";
	bOk &= check(xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u) &&
		xrtTarWriterClose(pWriter),
		"TAR writer did not recover after output-limit rejection");
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	bOk &= check(pPacked != NULL && pPacked->Size == 1536u,
		"TAR output limit left a partial entry frame");
	xrtBufferDestroy(pPacked);
	xrtTarWriterDestroy(pWriter);
	return bOk;
}






/* 验证文件归档仅显式关闭时提交，并验证目录树便利接口。 */
static int testTransactionAndTree(void)
{
	cstr sRoot = "build/tmp/tar_transaction_test";
	cstr sArchive = "build/tmp/tar_transaction_test/archive.tar";
	cstr sSource = "build/tmp/tar_transaction_test/source";
	cstr sNested = "build/tmp/tar_transaction_test/source/sub";
	cstr sRootFile = "build/tmp/tar_transaction_test/source/root.txt";
	cstr sNestedFile = "build/tmp/tar_transaction_test/source/sub/nested.txt";
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry tEntry;
	xbuffer* pPacked;
	xbuffer* pData;
	int iAppendSeen = 0;
	int iSeen = 0;
	int bOk = 1;

	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	bOk &= check(xrtDirCreateAll(sNested), "cannot create TAR tree fixture");
	bOk &= check(xrtFileWriteAll(sArchive, XRT_BYTES_LITERAL("old")) &&
		xrtFileWriteAll(sRootFile, XRT_BYTES_LITERAL("root")) &&
		xrtFileWriteAll(sNestedFile, XRT_BYTES_LITERAL("nested")),
		"cannot write TAR transaction fixtures");

	pWriter = xrtTarWriterOpenFile(sArchive, NULL);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "new.txt";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	bOk &= check(pWriter != NULL &&
		xrtTarWriterAdd(pWriter, &tEntry, "new", 3u),
		"cannot prepare uncommitted TAR transaction");
	xrtTarWriterDestroy(pWriter);
	bOk &= check(fileEquals(sArchive, "old", 3u),
		"destroy committed a TAR file without explicit close");

	pWriter = xrtTarWriterOpenFile(sArchive, NULL);
	bOk &= check(pWriter != NULL &&
		xrtTarWriterAdd(pWriter, &tEntry, "new", 3u) &&
		xrtTarWriterClose(pWriter),
		"cannot commit TAR file transaction");
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenFile(sArchive, NULL);
	bOk &= check(pReader != NULL && xrtTarReaderNext(pReader, &tEntry) &&
		strcmp(tEntry.sName, "new.txt") == 0,
		"committed TAR transaction is not readable");
	pData = xrtTarReaderRead(pReader);
	bOk &= check(pData != NULL && pData->Size == 3u &&
		memcmp(pData->Data, "new", 3u) == 0,
		"committed TAR transaction data mismatch");
	xrtBufferDestroy(pData);
	xrtTarReaderDestroy(pReader);

	pWriter = xrtTarWriterOpenAppend(sArchive, NULL);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "second.txt";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	bOk &= check(pWriter != NULL &&
		xrtTarWriterAdd(pWriter, &tEntry, "second", 6u) &&
		xrtTarWriterClose(pWriter),
		"cannot append to a TAR file transaction");
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenFile(sArchive, NULL);
	while ( xrtTarReaderNext(pReader, &tEntry) ) {
		pData = xrtTarReaderRead(pReader);
		if ( strcmp(tEntry.sName, "new.txt") == 0 ) {
			iAppendSeen |= 1;
			bOk &= check(pData != NULL && pData->Size == 3u &&
				memcmp(pData->Data, "new", 3u) == 0,
				"TAR append changed an existing entry");
		} else if ( strcmp(tEntry.sName, "second.txt") == 0 ) {
			iAppendSeen |= 2;
			bOk &= check(pData != NULL && pData->Size == 6u &&
				memcmp(pData->Data, "second", 6u) == 0,
				"TAR append produced the wrong new entry");
		}
		xrtBufferDestroy(pData);
	}
	bOk &= check(iAppendSeen == 3 && xrtTarReaderEOF(pReader),
		"TAR append lost an archive entry");
	xrtTarReaderDestroy(pReader);

	pWriter = xrtTarWriterCreateMemory(NULL);
	bOk &= check(xrtTarWriterAddTree(pWriter, "bundle", sSource, true) &&
		xrtTarWriterClose(pWriter),
		"cannot add a recursive TAR directory tree");
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	while ( xrtTarReaderNext(pReader, &tEntry) ) {
		if ( strcmp(tEntry.sName, "bundle/") == 0 ) { iSeen |= 1; }
		else if ( strcmp(tEntry.sName, "bundle/root.txt") == 0 ) { iSeen |= 2; }
		else if ( strcmp(tEntry.sName, "bundle/sub/") == 0 ) { iSeen |= 4; }
		else if ( strcmp(tEntry.sName, "bundle/sub/nested.txt") == 0 ) { iSeen |= 8; }
		bOk &= check(xrtTarReaderSkip(pReader),
			xrtTarReaderError(pReader)->sMessage);
	}
	bOk &= check(iSeen == 15 && xrtTarReaderEOF(pReader),
		"TAR directory tree entries are incomplete");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);
	return bOk;
}






/* 验证跨 Reader/Writer 流式复制保持链接类型和元数据。 */
static int testCopyCurrent(void)
{
	xrttaroptions Options;
	xrttarwriter* pSource;
	xrttarwriter* pDestination;
	xrttarreader* pReader;
	xrttarentry Entry;
	xbuffer* pPacked;
	xbuffer* pCopied;
	int iSeen = 0;
	int bOk = 1;

	xrtTarOptionsInit(&Options);
	Options.bAllowHardlinks = true;
	pSource = xrtTarWriterCreateMemory(&Options);
	xrtTarEntryInit(&Entry);
	Entry.sName = "alias.txt";
	Entry.sLinkName = "target.txt";
	Entry.iType = XRT_TAR_ENTRY_HARDLINK;
	Entry.iMode = 0640u;
	bOk &= check(xrtTarWriterAdd(pSource, &Entry, NULL, 0u),
		xrtTarWriterError(pSource)->sMessage);
	Entry.sName = "target.txt";
	Entry.sLinkName = NULL;
	Entry.iType = XRT_TAR_ENTRY_FILE;
	bOk &= check(xrtTarWriterAdd(pSource, &Entry, "target", 6u) &&
		xrtTarWriterClose(pSource), xrtTarWriterError(pSource)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pSource);
	xrtTarWriterDestroy(pSource);

	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	pDestination = xrtTarWriterCreateMemory(&Options);
	while ( xrtTarReaderNext(pReader, &Entry) ) {
		bOk &= check(xrtTarWriterCopyCurrent(pDestination, pReader),
			xrtTarWriterError(pDestination)->sMessage);
	}
	bOk &= check(xrtTarReaderEOF(pReader) &&
		xrtTarWriterClose(pDestination),
		"cannot finish copied TAR archive");
	pCopied = xrtTarWriterTakeBuffer(pDestination);
	xrtTarWriterDestroy(pDestination);
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);

	pReader = xrtTarReaderOpenMemory(pCopied->Data, pCopied->Size, &Options);
	while ( xrtTarReaderNext(pReader, &Entry) ) {
		if ( strcmp(Entry.sName, "alias.txt") == 0 &&
			Entry.iType == XRT_TAR_ENTRY_HARDLINK &&
			strcmp(Entry.sLinkName, "target.txt") == 0 &&
			Entry.iMode == 0640u ) {
			iSeen |= 1;
		} else if ( strcmp(Entry.sName, "target.txt") == 0 &&
			Entry.iType == XRT_TAR_ENTRY_FILE ) {
			iSeen |= 2;
		}
		bOk &= check(xrtTarReaderSkip(pReader),
			xrtTarReaderError(pReader)->sMessage);
	}
	bOk &= check(iSeen == 3 && xrtTarReaderEOF(pReader),
		"TAR current-entry copy changed metadata or entry type");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pCopied);
	return bOk;
}






/* 验证进度、取消和预先存在的目录链接不能逃逸提取根。 */
static int testProgressAndAnchoredExtraction(void)
{
	cstr sRoot = "build/tmp/tar_progress_test";
	cstr sOutput = "build/tmp/tar_progress_test/output";
	cstr sOutside = "build/tmp/tar_progress_test/outside";
	cstr sEscapeLink = "build/tmp/tar_progress_test/output/link";
	cstr sEscapedFile = "build/tmp/tar_progress_test/outside/escape.txt";
	cstr sSelectedRoot = "build/tmp/tar_progress_test/selected";
	cstr sSelectedFile = "build/tmp/tar_progress_test/outside/selected.txt";
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry tEntry;
	xbuffer* pPacked;
	tarprogresscontext Context;
	int bLinkCreated;
	int bOk = 1;

	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	pWriter = xrtTarWriterCreateMemory(NULL);
	xrtTarEntryInit(&tEntry);
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.sName = "one.txt";
	bOk &= check(xrtTarWriterAdd(pWriter, &tEntry, "1", 1u),
		xrtTarWriterError(pWriter)->sMessage);
	tEntry.sName = "two.txt";
	bOk &= check(xrtTarWriterAdd(pWriter, &tEntry, "22", 2u) &&
		xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);

	memset(&Context, 0, sizeof(Context));
	Context.bMonotonic = true;
	Context.iCancelAfter = 1;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(!xrtTarReaderExtract(pReader, sOutput, false,
		onExtractProgress, &Context) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_CANCELLED &&
		Context.iEntries == 1 && Context.iFinalEvents == 0 && Context.bMonotonic,
		"TAR extraction cancellation did not stop at an entry boundary");
	xrtTarReaderDestroy(pReader);
	xrtDirRemoveAll(sOutput);

	memset(&Context, 0, sizeof(Context));
	Context.bCancelFinal = true;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(xrtTarReaderExtract(pReader, sOutput, false,
		onExtractProgress, &Context) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_OK &&
		Context.iEntries == 2 && Context.iFinalEvents == 1 &&
		Context.bFinalFlag && fileEquals("build/tmp/tar_progress_test/output/two.txt", "22", 2u),
		"TAR final notification incorrectly cancelled completed extraction");
	xrtTarReaderDestroy(pReader);
	xrtDirRemoveAll(sOutput);

	memset(&Context, 0, sizeof(Context));
	Context.bMonotonic = true;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(xrtTarReaderExtract(pReader, sOutput, false,
		onExtractProgress, &Context) && Context.iEntries == 2 &&
		Context.iFinalEvents == 1 && Context.bFinalFlag && Context.bMonotonic,
		"TAR extraction progress contract mismatch");
	xrtTarReaderDestroy(pReader);
	xrtDirRemoveAll(sOutput);

	/* 带前缀的 Reader 应报告归档总长，而不是底层流的总长。 */
	{
		static const char sPrefix[] = "XXXXXXXXXX";
		xbuffer* pWrapped = xrtBufferCreate();
		xreader* pInput = NULL;
		uint64 iPosition = 0u;
		bOk &= check(pWrapped != NULL &&
			xrtBufferAppend(pWrapped, (xbytesview){
				(cbytes)sPrefix, sizeof(sPrefix) - 1u }) &&
			xrtBufferAppend(pWrapped, (xbytesview){
				pPacked->Data, pPacked->Size }),
			"cannot prepare seeked TAR progress input");
		if ( bOk ) {
			pInput = xrtReaderFromMemory((xbytesview){
				pWrapped->Data, pWrapped->Size });
			bOk &= check(pInput != NULL && xrtReaderSeek(pInput,
				(int64)(sizeof(sPrefix) - 1u), XSEEK_START, &iPosition) &&
				iPosition == sizeof(sPrefix) - 1u,
				"cannot seek to TAR archive start");
		}
		if ( bOk ) {
			memset(&Context, 0, sizeof(Context));
			Context.bMonotonic = true;
			pReader = xrtTarReaderCreate(pInput, true, NULL);
			if ( pReader != NULL ) { pInput = NULL; }
			bOk &= check(pReader != NULL && xrtTarReaderExtract(pReader,
				sOutput, false, onExtractProgress, &Context) &&
				Context.bTotalKnown && Context.iTotalInput == pPacked->Size &&
				Context.iEntries == 2 && Context.iFinalEvents == 1,
				"seeked TAR progress includes bytes before the archive");
			if ( pReader != NULL ) { xrtTarReaderDestroy(pReader); }
		}
		if ( pInput != NULL ) { xrtReaderDestroy(pInput); }
		xrtBufferDestroy(pWrapped);
		xrtDirRemoveAll(sOutput);
	}

	bOk &= check(xrtDirCreateAll(sOutput) && xrtDirCreateAll(sOutside),
		"cannot create TAR anchored-extraction fixture");
	bLinkCreated = xrtLinkCreate("../outside", sEscapeLink, true);
	if ( bLinkCreated ) {
		xbuffer* pEscape = makeArchive("link/escape.txt", "escape", 6u);

		pReader = xrtTarReaderOpenMemory(pEscape->Data, pEscape->Size, NULL);
		bOk &= check(!xrtTarReaderExtractAll(pReader, sOutput, true) &&
			!xrtFileExists(sEscapedFile),
			"TAR regular-file extraction escaped through a directory link");
		xrtTarReaderDestroy(pReader);
		xrtBufferDestroy(pEscape);
		bOk &= check(xrtLinkCreate("outside", sSelectedRoot, true),
			"cannot create the caller-selected TAR extraction root link");
		if ( bOk ) {
			xbuffer* pSelected = makeArchive("selected.txt", "selected", 8u);
			pReader = xrtTarReaderOpenMemory(pSelected->Data, pSelected->Size, NULL);
			bOk &= check(xrtTarReaderExtractAll(pReader, sSelectedRoot, false) &&
				fileEquals(sSelectedFile, "selected", 8u),
				"TAR rejected the caller-selected linked extraction root");
			xrtTarReaderDestroy(pReader);
			xrtBufferDestroy(pSelected);
		}
	} else {
		/* Windows 未授予符号链接权限时跳过平台能力相关断言。 */
		xrtClearError();
	}
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);
	return bOk;
}






/* 验证白名单、谓词错误和硬链接依赖都由核心选择提取器统一处理。 */
static int testSelectiveExtraction(void)
{
	cstr sRoot = "build/tmp/tar_select_test";
	cstr sWhereOutput = "build/tmp/tar_select_test/where";
	cstr sNamesOutput = "build/tmp/tar_select_test/names";
	cstr sEscaped = "build/tmp/tar_select_test/xy";
	xrttaroptions Options;
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry Entry;
	xbuffer* pPacked;
	xstrview arrNames[3];
	tarselectcontext Select;
	tarprogresscontext Progress;
	int bOk = 1;

	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	pWriter = xrtTarWriterCreateMemory(NULL);
	xrtTarEntryInit(&Entry);
	Entry.iType = XRT_TAR_ENTRY_FILE;
	Entry.sName = "abcde";
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "bad", 3u),
		xrtTarWriterError(pWriter)->sMessage);
	Entry.sName = "keep.txt";
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "keep", 4u) &&
		xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);
	memcpy(pPacked->Data, "../xy", 5u);
	updateHeaderChecksum((unsigned char*)pPacked->Data);

	memset(&Select, 0, sizeof(Select));
	Select.sName = "keep.txt";
	memset(&Progress, 0, sizeof(Progress));
	Progress.bMonotonic = true;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(xrtTarReaderExtractWhere(pReader, sWhereOutput, false,
		selectEntry, &Select, onExtractProgress, &Progress) &&
		Select.iCalls == 2 && Progress.iEntries == 2 &&
		Progress.iFinalEvents == 1 && Progress.bFinalFlag &&
		fileEquals("build/tmp/tar_select_test/where/keep.txt", "keep", 4u) &&
		!xrtPathExists(sEscaped),
		"TAR selector did not safely skip an unselected unsafe path");
	xrtTarReaderDestroy(pReader);

	arrNames[0] = xrtStrView("keep.txt");
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(xrtTarReaderExtractNames(pReader, sNamesOutput, false,
		arrNames, 1u) &&
		fileEquals("build/tmp/tar_select_test/names/keep.txt", "keep", 4u) &&
		!xrtPathExists(sEscaped),
		"TAR exact-name extraction did not use the safe selector path");
	xrtTarReaderDestroy(pReader);

	memset(&Select, 0, sizeof(Select));
	Select.sName = "keep.txt";
	Select.bFail = true;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(!xrtTarReaderExtractWhere(pReader,
		"build/tmp/tar_select_test/error", false,
		selectEntry, &Select, NULL, NULL) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_CALLBACK &&
		strstr(xrtTarReaderError(pReader)->sMessage,
			"selector callback boom") != NULL,
		"TAR selector callback error was not preserved");
	xrtTarReaderDestroy(pReader);
	xrtClearError();
	xrtBufferDestroy(pPacked);

	xrtTarOptionsInit(&Options);
	Options.bAllowHardlinks = true;
	pWriter = xrtTarWriterCreateMemory(&Options);
	xrtTarEntryInit(&Entry);
	Entry.sName = "alias.txt";
	Entry.sLinkName = "target.txt";
	Entry.iType = XRT_TAR_ENTRY_HARDLINK;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, NULL, 0u),
		xrtTarWriterError(pWriter)->sMessage);
	Entry.sName = "target.txt";
	Entry.sLinkName = NULL;
	Entry.iType = XRT_TAR_ENTRY_FILE;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "target", 6u),
		xrtTarWriterError(pWriter)->sMessage);
	Entry.sName = "third.txt";
	Entry.sLinkName = "alias.txt";
	Entry.iType = XRT_TAR_ENTRY_HARDLINK;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, NULL, 0u) &&
		xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);

	arrNames[0] = xrtStrView("alias.txt");
	bOk &= check(xrtDirCreateAll("build/tmp/tar_select_test/link-only") &&
		xrtFileWriteAll("build/tmp/tar_select_test/link-only/target.txt",
			XRT_BYTES_LITERAL("existing")),
		"cannot prepare pre-existing TAR hard-link target");
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(!xrtTarReaderExtractNames(pReader,
		"build/tmp/tar_select_test/link-only", false, arrNames, 1u) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_FORMAT &&
		fileEquals("build/tmp/tar_select_test/link-only/target.txt",
			"existing", 8u) &&
		!xrtPathExists("build/tmp/tar_select_test/link-only/alias.txt"),
		"TAR selector linked a pre-existing file outside the selection");
	xrtTarReaderDestroy(pReader);

	arrNames[1] = xrtStrView("target.txt");
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(xrtTarReaderExtractNames(pReader,
		"build/tmp/tar_select_test/link-complete", false, arrNames, 2u) &&
		fileEquals("build/tmp/tar_select_test/link-complete/alias.txt",
			"target", 6u),
		"TAR selector did not resolve a complete hard-link selection");
	xrtTarReaderDestroy(pReader);

	arrNames[2] = xrtStrView("third.txt");
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(xrtTarReaderExtractNames(pReader,
		"build/tmp/tar_select_test/link-chain", false, arrNames, 3u) &&
		fileEquals("build/tmp/tar_select_test/link-chain/third.txt",
			"target", 6u),
		"TAR selector did not resolve a forward hard-link chain");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);

	pWriter = xrtTarWriterCreateMemory(&Options);
	xrtTarEntryInit(&Entry);
	Entry.sName = "alias.txt";
	Entry.sLinkName = "target.txt";
	Entry.iType = XRT_TAR_ENTRY_HARDLINK;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, NULL, 0u),
		xrtTarWriterError(pWriter)->sMessage);
	Entry.sName = "target.txt";
	Entry.sLinkName = NULL;
	Entry.iType = XRT_TAR_ENTRY_FILE;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "target", 6u),
		xrtTarWriterError(pWriter)->sMessage);
	Entry.sName = "alias.txt";
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "newest", 6u) &&
		xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(xrtTarReaderExtractWhere(pReader,
		"build/tmp/tar_select_test/link-shadow", true,
		NULL, NULL, NULL, NULL) &&
		fileEquals("build/tmp/tar_select_test/link-shadow/alias.txt",
			"newest", 6u),
		"deferred TAR hard link overwrote a later archive entry");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);
	return bOk;
}



/* 读写入口都必须拒绝未知的资源限制结构版本。 */
static int testOptionVersions(void)
{
	xrttaroptions Options;
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	const uint8 Empty[1] = { 0u };
	int bOk = 1;

	xrtTarOptionsInit(&Options);
	Options.tLimits.iVersion += 1u;
	pWriter = xrtTarWriterCreateMemory(&Options);
	bOk &= check(pWriter != NULL &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"TAR writer accepted an unknown resource limits version");
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(Empty, 0u, &Options);
	bOk &= check(pReader != NULL &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"TAR reader accepted an unknown resource limits version");
	xrtTarReaderDestroy(pReader);

	xrtTarOptionsInit(&Options);
	Options.tLimits.iSize = 0u;
	pWriter = xrtTarWriterCreateMemory(&Options);
	bOk &= check(pWriter != NULL &&
		xrtTarWriterError(pWriter)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"TAR writer accepted a truncated resource limits structure");
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(Empty, 0u, &Options);
	bOk &= check(pReader != NULL &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_ARGUMENT,
		"TAR reader accepted a truncated resource limits structure");
	xrtTarReaderDestroy(pReader);
	for (int iCase = 0; iCase < 2; ++iCase) {
		xrtTarOptionsInit(&Options);
		if ( iCase == 0 ) { Options.tLimits.iReserved = 1u; }
		else { Options.iReserved = 1u; }
		pWriter = xrtTarWriterCreateMemory(&Options);
		bOk &= check(pWriter != NULL && xrtTarWriterError(pWriter)->iCode ==
			XRT_TAR_ERROR_ARGUMENT, "TAR writer accepted a reserved option");
		xrtTarWriterDestroy(pWriter);
		pReader = xrtTarReaderOpenMemory(Empty, 0u, &Options);
		bOk &= check(pReader != NULL && xrtTarReaderError(pReader)->iCode ==
			XRT_TAR_ERROR_ARGUMENT, "TAR reader accepted a reserved option");
		xrtTarReaderDestroy(pReader);
	}
	/* TAR 不解析层级树，也不压缩；无用限额不能伪装成有效保护。 */
	xrtTarOptionsInit(&Options);
	bOk &= check(Options.tLimits.iMaxDepth == 0u &&
		Options.tLimits.iMaxCompressionRatio == 0u,
		"TAR options inherited unsupported resource limits");
	for (int iCase = 0; iCase < 3; ++iCase) {
		xrtTarOptionsInit(&Options);
		if ( iCase == 0 ) { Options.tLimits.iFlags = 1u; }
		else if ( iCase == 1 ) { Options.tLimits.iMaxDepth = 1u; }
		else { Options.tLimits.iMaxCompressionRatio = 1u; }
		pWriter = xrtTarWriterCreateMemory(&Options);
		bOk &= check(pWriter != NULL && xrtTarWriterError(pWriter)->iCode ==
			XRT_TAR_ERROR_ARGUMENT, "TAR writer accepted an unsupported resource limit");
		xrtTarWriterDestroy(pWriter);
		pReader = xrtTarReaderOpenMemory(Empty, 0u, &Options);
		bOk &= check(pReader != NULL && xrtTarReaderError(pReader)->iCode ==
			XRT_TAR_ERROR_ARGUMENT, "TAR reader accepted an unsupported resource limit");
		xrtTarReaderDestroy(pReader);
	}
	return bOk;
}

#if !defined(_WIN32) && !defined(_WIN64)
/* 后续同名条目必须取代旧 mode，旧文件改为链接时不能改动链接目标。 */
static int testShadowedPermissions(void)
{
	const char* sRoot = "build/tmp/tar_mode_shadow_test";
	const char* sOutput = "build/tmp/tar_mode_shadow_test/output";
	xrttaroptions Options;
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry Entry;
	xbuffer* pPacked;
	xfileinfo Info;
	int bOk = 1;

	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	xrtTarOptionsInit(&Options);
	Options.bPreservePermissions = true;
	Options.bAllowSymlinks = true;
	pWriter = xrtTarWriterCreateMemory(&Options);
	xrtTarEntryInit(&Entry);
	Entry.iType = XRT_TAR_ENTRY_FILE;
	Entry.sName = "item";
	Entry.iMode = 0400u;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "old", 3u),
		"cannot add original TAR file");
	Entry.iMode = 0600u;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "new", 3u) &&
		xrtTarWriterClose(pWriter), "cannot add replacement TAR file");
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(xrtTarReaderExtractAll(pReader, sOutput, true) &&
		fileEquals("build/tmp/tar_mode_shadow_test/output/item", "new", 3u) &&
		xrtPathStat("build/tmp/tar_mode_shadow_test/output/item", true, &Info) &&
		(Info.Mode & 0777u) == 0600u,
		"later TAR file did not replace earlier permissions");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);

	pWriter = xrtTarWriterCreateMemory(&Options);
	xrtTarEntryInit(&Entry);
	Entry.iType = XRT_TAR_ENTRY_FILE;
	Entry.sName = "item";
	Entry.iMode = 0400u;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "old", 3u),
		"cannot add TAR file before symlink");
	Entry.iType = XRT_TAR_ENTRY_SYMLINK;
	Entry.sLinkName = "victim";
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, NULL, 0u),
		"cannot add replacement TAR symlink");
	Entry.iType = XRT_TAR_ENTRY_FILE;
	Entry.sName = "victim";
	Entry.sLinkName = NULL;
	Entry.iMode = 0644u;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "safe", 4u) &&
		xrtTarWriterClose(pWriter), "cannot add TAR symlink target");
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(xrtTarReaderExtractAll(pReader, sOutput, true) &&
		xrtPathStat("build/tmp/tar_mode_shadow_test/output/victim", true, &Info) &&
		(Info.Mode & 0777u) == 0644u,
		"old TAR file mode changed a later symlink target");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);

	pWriter = xrtTarWriterCreateMemory(&Options);
	xrtTarEntryInit(&Entry);
	Entry.iType = XRT_TAR_ENTRY_DIRECTORY;
	Entry.sName = "sub/";
	Entry.iMode = 0500u;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, NULL, 0u),
		"cannot add first TAR directory spelling");
	Entry.sName = "sub";
	Entry.iMode = 0700u;
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, NULL, 0u) &&
		xrtTarWriterClose(pWriter), "cannot add second TAR directory spelling");
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(xrtTarReaderExtractAll(pReader, sOutput, true) &&
		xrtPathStat("build/tmp/tar_mode_shadow_test/output/sub", true, &Info) &&
		(Info.Mode & 0777u) == 0700u,
		"TAR directory spelling retained shadowed permissions");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);
	return bOk;
}
#endif

int main(void)
{
	str sPrevious = xrtPathCwd();
	str sTemporary = sPrevious != NULL ?
		xrtDirTemp(NULL, "xlang-tar-test-", "") : NULL;
	str sWorkspace = sTemporary != NULL ? xrtPathAbs(sTemporary) : NULL;
	int bOk = check(sPrevious != NULL && sWorkspace != NULL,
		"cannot create isolated TAR test directory");
	if ( !bOk ) {
		if ( sTemporary != NULL ) { xrtDirRemoveAll(sTemporary); }
		xrtFree(sWorkspace);
		xrtFree(sTemporary);
		xrtFree(sPrevious);
		return 1;
	}
	if ( !check(xrtPathSetCwd(sWorkspace), "cannot enter TAR test directory") ) {
		bOk = 0;
	} else {
		bOk = testOptionVersions();
		bOk &= testMemoryAndPax();
		bOk &= testSeekedReaderLimit();
		bOk &= testMalformedPaxFields();
		bOk &= testSecurityAndLimits();
		bOk &= testLinksAndSpecialEntries();
		bOk &= testRecoveryAndStreaming();
		bOk &= testStreamErrorKinds();
		bOk &= testExtractionWriterErrors();
		bOk &= testTransactionAndTree();
		bOk &= testCopyCurrent();
		bOk &= testProgressAndAnchoredExtraction();
		bOk &= testSelectiveExtraction();
		#if !defined(_WIN32) && !defined(_WIN64)
			bOk &= testShadowedPermissions();
		#endif
		if ( !check(xrtPathSetCwd(sPrevious), "cannot restore TAR test directory") ) {
			/* 当前目录可能仍在临时目录内，不递归删除它。 */
			xrtFree(sWorkspace);
			xrtFree(sTemporary);
			xrtFree(sPrevious);
			return 1;
		}
	}
	bOk &= check(xrtDirRemoveAll(sWorkspace), "cannot remove TAR test directory");
	xrtFree(sWorkspace);
	xrtFree(sTemporary);
	xrtFree(sPrevious);
	if ( bOk ) { puts("test_tar: PASS"); }
	return bOk ? 0 : 1;
}
