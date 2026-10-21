#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_DIR_TEMP
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif

#include "../include/xrt_zip.h"
#include "../xrt_zip_internal.h"

#include <stdio.h>
#include <string.h>

void* xzip_archive_open_file_value(xvalue* pPath, int iMode, int iMethod,
	int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned int iMaxRatio);



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_zip: %s\n", sMessage != NULL && sMessage[0] != 0 ? sMessage : "check failed");
	return 0;
}




static int testMemoryArchive(void)
{
	const unsigned char sBinary[] = { 0u, 1u, 2u, 0xFFu };
	xrtziparchive* pWriter = xrtZipArchiveCreate(NULL);
	xrtziparchive* pReader;
	xbuffer* pArchive;
	xbuffer* pData;
	xrtzipentry tEntry;
	int bOk = 1;
	bOk &= check(pWriter != NULL && xrtZipArchiveIsOpen(pWriter),
		pWriter != NULL ? xrtZipArchiveError(pWriter)->sMessage : "cannot allocate ZIP writer");
	bOk &= check(xrtZipArchiveAddDirectory(pWriter, "folder"), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveAdd(pWriter, "folder/hello.txt", "hello", 5u,
		false, XRT_ZIP_METHOD_DEFLATE, 9), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveAdd(pWriter, "binary.bin", sBinary, sizeof(sBinary),
		false, XRT_ZIP_METHOD_STORE, 0), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(!xrtZipArchiveAdd(pWriter, "../escape.txt", "bad", 3u,
		false, XRT_ZIP_METHOD_STORE, 0) &&
		xrtZipArchiveError(pWriter)->iCode == XRT_ZIP_ERROR_UNSAFE_PATH,
		"unsafe ZIP path was accepted");
	bOk &= check(xrtZipArchiveSetComment(pWriter, "archive comment"), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveSetEntryComment(pWriter, "binary.bin", "entry comment"),
		xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveCount(pWriter) == 3u, "unexpected ZIP entry count before close");
	bOk &= check(xrtZipArchiveClose(pWriter), xrtZipArchiveError(pWriter)->sMessage);
	pArchive = xrtZipArchiveTakeBuffer(pWriter);
	bOk &= check(pArchive != NULL && pArchive->Size > 100u, "cannot take ZIP memory result");
	xrtZipArchiveDestroy(pWriter);

	pReader = xrtZipArchiveOpenMemory(pArchive->Data, pArchive->Size, false, NULL);
	bOk &= check(pReader != NULL && xrtZipArchiveIsOpen(pReader),
		pReader != NULL ? xrtZipArchiveError(pReader)->sMessage : "cannot allocate ZIP reader");
	bOk &= check(xrtZipArchiveCount(pReader) == 3u, "unexpected ZIP entry count after reopen");
	bOk &= check(xrtZipArchiveEntryAt(pReader, 2u, &tEntry) &&
		tEntry.sName != NULL && strcmp(tEntry.sName, "binary.bin") == 0 &&
		strcmp(tEntry.sComment, "entry comment") == 0, "ZIP entry metadata mismatch");
	pData = xrtZipArchiveRead(pReader, "folder/hello.txt");
	bOk &= check(pData != NULL && pData->Size == 5u &&
		memcmp(pData->Data, "hello", 5u) == 0, "ZIP text entry mismatch");
	xrtBufferDestroy(pData);
	pData = xrtZipArchiveRead(pReader, "binary.bin");
	bOk &= check(pData != NULL && pData->Size == sizeof(sBinary) &&
		memcmp(pData->Data, sBinary, sizeof(sBinary)) == 0, "ZIP binary entry mismatch");
	xrtBufferDestroy(pData);
	bOk &= check(xrtZipArchiveClose(pReader), xrtZipArchiveError(pReader)->sMessage);
	xrtZipArchiveDestroy(pReader);
	xrtBufferDestroy(pArchive);
	return bOk;
}



static xbuffer* makeArchive(cstr sName, const void* pData, size_t iSize)
{
	xrtziparchive* pWriter = xrtZipArchiveCreate(NULL);
	xbuffer* pResult = NULL;
	if ( pWriter != NULL && xrtZipArchiveIsOpen(pWriter) &&
		xrtZipArchiveAdd(pWriter, sName, pData, iSize, false, XRT_ZIP_METHOD_DEFLATE, 9) &&
		xrtZipArchiveClose(pWriter) ) {
		pResult = xrtZipArchiveTakeBuffer(pWriter);
	}
	xrtZipArchiveDestroy(pWriter);
	return pResult;
}



static int testLimitsAndCorruption(void)
{
	unsigned char sLarge[4096];
	xrtzipoptions tOptions;
	xrtziparchive* pArchive;
	xbuffer* pPacked;
	xbuffer* pData;
	int bOk = 1;
	memset(sLarge, 'A', sizeof(sLarge));

	xrtZipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxEntries = 1u;
	pArchive = xrtZipArchiveCreate(&tOptions);
	bOk &= check(pArchive != NULL && xrtZipArchiveAdd(pArchive, "one", "1", 1u,
		false, XRT_ZIP_METHOD_STORE, 0), "cannot add first limited ZIP entry");
	bOk &= check(!xrtZipArchiveAdd(pArchive, "two", "2", 1u,
		false, XRT_ZIP_METHOD_STORE, 0) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_LIMIT,
		"ZIP entry-count limit was not enforced");
	xrtZipArchiveDestroy(pArchive);

	xrtZipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxItemBytes = 3u;
	pArchive = xrtZipArchiveCreate(&tOptions);
	bOk &= check(!xrtZipArchiveAdd(pArchive, "large", "1234", 4u,
		false, XRT_ZIP_METHOD_STORE, 0) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_LIMIT,
		"ZIP item limit was not enforced while adding");
	xrtZipArchiveDestroy(pArchive);

	pPacked = makeArchive("data.bin", sLarge, sizeof(sLarge));
	bOk &= check(pPacked != NULL, "cannot create ZIP limit fixture");
	xrtZipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 128u;
	tOptions.tLimits.iMaxItemBytes = 128u;
	pArchive = xrtZipArchiveOpenMemory(pPacked->Data, pPacked->Size, false, &tOptions);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		"cannot open ZIP limit fixture");
	pData = xrtZipArchiveRead(pArchive, "data.bin");
	bOk &= check(pData == NULL && xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_LIMIT,
		"ZIP output limit was not enforced while reading");
	xrtBufferDestroy(pData);
	xrtZipArchiveDestroy(pArchive);

	if ( pPacked != NULL && pPacked->Size > 8u ) {
		pArchive = xrtZipArchiveOpenMemory(pPacked->Data, pPacked->Size - 8u, false, NULL);
		bOk &= check(pArchive != NULL && !xrtZipArchiveIsOpen(pArchive),
			"truncated ZIP archive was accepted");
		xrtZipArchiveDestroy(pArchive);
	}
	xrtBufferDestroy(pPacked);
	return bOk;
}



static int replaceArchiveName(xbuffer* pArchive, const char* sOldName, const char* sNewName)
{
	size_t i;
	size_t iLength = strlen(sOldName);
	int iCount = 0;
	if ( pArchive == NULL || strlen(sNewName) != iLength ) { return 0; }
	for ( i = 0u; i + iLength <= pArchive->Size; ++i ) {
		if ( memcmp(pArchive->Data + i, sOldName, iLength) == 0 ) {
			memcpy(pArchive->Data + i, sNewName, iLength);
			i += iLength - 1u;
			iCount += 1;
		}
	}
	return iCount;
}



static int testUnsafeExtractionAndOverwrite(void)
{
	cstr sRoot = "build/tmp/zip_security_test";
	cstr sDestination = "build/tmp/zip_security_test/output";
	cstr sExisting = "build/tmp/zip_security_test/output/existing.txt";
	xbuffer* pPacked;
	xrtziparchive* pArchive;
	bytes pData;
	size_t iSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	bOk &= check(xrtDirCreateAll(sDestination), "cannot create ZIP security test directory");

	pPacked = makeArchive("abcde", "bad", 3u);
	bOk &= check(replaceArchiveName(pPacked, "abcde", "../xy") >= 2,
		"cannot build unsafe ZIP path fixture");
	pArchive = xrtZipArchiveOpenMemory(pPacked->Data, pPacked->Size, false, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		"unsafe ZIP fixture itself cannot be opened");
	bOk &= check(!xrtZipArchiveExtractAll(pArchive, sDestination, false) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_UNSAFE_PATH,
		"unsafe ZIP extraction path was accepted");
	xrtZipArchiveDestroy(pArchive);
	xrtBufferDestroy(pPacked);

	pPacked = makeArchive("existing.txt", "new", 3u);
	bOk &= check(xrtFileWriteAtomic(sExisting,
		(xbytesview){ (const uint8*)"old", 3u }),
		"cannot create ZIP overwrite fixture");
	pArchive = xrtZipArchiveOpenMemory(pPacked->Data, pPacked->Size, false, NULL);
	bOk &= check(!xrtZipArchiveExtractAll(pArchive, sDestination, false) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_EXISTS,
		"ZIP extraction overwrote a file without permission");
	bOk &= check(xrtZipArchiveExtractAll(pArchive, sDestination, true),
		xrtZipArchiveError(pArchive)->sMessage);
	pData = xrtFileReadAll(sExisting, &iSize);
	bOk &= check(pData != NULL && iSize == 3u && memcmp(pData, "new", 3u) == 0,
		"ZIP overwrite extraction produced the wrong data");
	xrtFree(pData);
	xrtZipArchiveDestroy(pArchive);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);
	return bOk;
}



static int testUtf8FileAndExtract(void)
{
	str sRoot = (str)"build/tmp/zip_中文_test";
	str sArchivePath = (str)"build/tmp/zip_中文_test/归档.zip";
	str sExtractPath = (str)"build/tmp/zip_中文_test/输出";
	str sExtracted = (str)"build/tmp/zip_中文_test/输出/目录/内容.txt";
	xrtziparchive* pArchive;
	xvalue* pPath;
	bytes pData;
	size_t iSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	bOk &= check(xrtDirCreateAll(sRoot), "cannot create UTF-8 ZIP test directory");
	pArchive = xrtZipArchiveOpenFile(sArchivePath, XRT_ZIP_MODE_WRITE, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		pArchive != NULL ? xrtZipArchiveError(pArchive)->sMessage : "cannot allocate file ZIP writer");
	bOk &= check(xrtZipArchiveAdd(pArchive, (str)"目录/内容.txt", "UTF-8 path", 10u,
		false, XRT_ZIP_METHOD_DEFLATE, 6), xrtZipArchiveError(pArchive)->sMessage);
	bOk &= check(xrtZipArchiveClose(pArchive), xrtZipArchiveError(pArchive)->sMessage);
	xrtZipArchiveDestroy(pArchive);
	bOk &= check(xrtFileExists(sArchivePath), "UTF-8 ZIP file was not saved");
	pPath = xrtValueString((xstrview){ sArchivePath, strlen(sArchivePath) });
	pArchive = xzip_archive_open_file_value(pPath, XRT_ZIP_MODE_READ,
		XRT_ZIP_METHOD_DEFLATE, 6, 268435456u, 536870912u,
		268435456u, 100000u, 1000u);
	bOk &= check(pArchive != NULL && xrtZipArchiveCount(pArchive) == 1u,
		"length-aware ZIP path could not open UTF-8 archive");
	xrtZipArchiveDestroy(pArchive);
	xrtValueRelease(pPath);
	pPath = xrtValueString((xstrview){ "test.zip\0suffix", 15u });
	pArchive = xzip_archive_open_file_value(pPath, XRT_ZIP_MODE_READ,
		XRT_ZIP_METHOD_DEFLATE, 6, 268435456u, 536870912u,
		268435456u, 100000u, 1000u);
	bOk &= check(pArchive == NULL && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_VALUE,
		"length-aware ZIP path accepted an embedded NUL");
	xrtZipArchiveDestroy(pArchive);
	xrtValueRelease(pPath);
	pPath = xrtValueString((xstrview){ NULL, 0u });
	pArchive = xzip_archive_open_file_value(pPath, XRT_ZIP_MODE_READ,
		XRT_ZIP_METHOD_DEFLATE, 6, 268435456u, 536870912u,
		268435456u, 100000u, 1000u);
	bOk &= check(pArchive == NULL, "length-aware ZIP path accepted an empty string");
	xrtZipArchiveDestroy(pArchive);
	xrtValueRelease(pPath);
	pPath = xrtValueInt(123);
	pArchive = xzip_archive_open_file_value(pPath, XRT_ZIP_MODE_READ,
		XRT_ZIP_METHOD_DEFLATE, 6, 268435456u, 536870912u,
		268435456u, 100000u, 1000u);
	bOk &= check(pArchive == NULL && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_TYPE,
		"length-aware ZIP path accepted a nonstring value");
	xrtZipArchiveDestroy(pArchive);
	xrtValueRelease(pPath);

	pArchive = xrtZipArchiveOpenFile(sArchivePath, XRT_ZIP_MODE_READ, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		pArchive != NULL ? xrtZipArchiveError(pArchive)->sMessage : "cannot allocate file ZIP reader");
	bOk &= check(xrtZipArchiveExtractAll(pArchive, sExtractPath, false),
		xrtZipArchiveError(pArchive)->sMessage);
	xrtZipArchiveDestroy(pArchive);
	pData = xrtFileReadAll(sExtracted, &iSize);
	bOk &= check(pData != NULL && iSize == 10u && memcmp(pData, "UTF-8 path", 10u) == 0,
		"UTF-8 ZIP extraction mismatch");
	xrtFree(pData);
	xrtDirRemoveAll(sRoot);
	return bOk;
}





/* 验证 Writer 流式读取不依赖整条目便利接口。 */
static int testStreamingRead(void)
{
	uint8 arrInput[131072];
	xbuffer* pPacked;
	xrtziparchive* pArchive;
	xbuffer Output;
	xwriter* pWriter;
	int bOk = 1;
	size_t i;

	for ( i = 0u; i < sizeof(arrInput); ++i ) {
		arrInput[i] = (uint8)((i * 29u) & 0xFFu);
	}
	pPacked = makeArchive("stream.bin", arrInput, sizeof(arrInput));
	bOk &= check(pPacked != NULL, "cannot create streaming ZIP fixture");
	pArchive = pPacked != NULL ? xrtZipArchiveOpenMemory(
		pPacked->Data, pPacked->Size, false, NULL) : NULL;
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		"cannot open streaming ZIP fixture");
	xrtBufferInit(&Output);
	pWriter = xrtWriterFromBuffer(&Output);
	bOk &= check(pWriter != NULL &&
		xrtZipArchiveReadTo(pArchive, "stream.bin", pWriter),
		pArchive != NULL ? xrtZipArchiveError(pArchive)->sMessage :
			"cannot create streaming output Writer");
	bOk &= check(pWriter != NULL && xrtWriterDestroy(pWriter),
		"cannot close streaming output Writer");
	bOk &= check((Output.Size == sizeof(arrInput)) &&
		(memcmp(Output.Data, arrInput, sizeof(arrInput)) == 0),
		"streaming ZIP output mismatch");
	xrtBufferUnit(&Output);
	xrtZipArchiveDestroy(pArchive);
	xrtBufferDestroy(pPacked);
	return bOk;
}


typedef struct {
	xerrkind Kind;
} zipwritefailure;

static bool failZipOutputWrite(ptr pContext, const void* pData,
	size_t iRequest, size_t* pWritten)
{
	zipwritefailure* pFailure = (zipwritefailure*)pContext;
	(void)pData;
	(void)iRequest;
	*pWritten = 0u;
	xrtSetErrorInfo(pFailure->Kind, "test.zip", 0, "injected output failure");
	return false;
}

/* 调用方 Writer 的内存故障不能被归档层改写成设备 I/O。 */
static int testStreamingOutputErrors(void)
{
	static const char sData[] = "stream data";
	xwriterops Ops = {0};
	xbuffer* pPacked = makeArchive("stream.bin", sData, sizeof(sData) - 1u);
	int bOk = check(pPacked != NULL, "cannot create ZIP output error fixture");
	int i;
	Ops.Write = failZipOutputWrite;
	for ( i = 0; bOk && i < 2; ++i ) {
		zipwritefailure Failure;
		xrtziparchive* pArchive = xrtZipArchiveOpenMemory(
			pPacked->Data, pPacked->Size, false, NULL);
		xwriter* pWriter;
		int iExpected;
		Failure.Kind = i == 0 ? XERR_MEMORY : XERR_IO;
		iExpected = i == 0 ? XRT_ZIP_ERROR_MEMORY : XRT_ZIP_ERROR_IO;
		pWriter = xrtWriterCreate(&Ops, &Failure);
		bOk &= check(pArchive != NULL && pWriter != NULL &&
			!xrtZipArchiveReadTo(pArchive, "stream.bin", pWriter) &&
			xrtZipArchiveError(pArchive)->iCode == iExpected,
			"ZIP changed output Writer failure category");
		(void)xrtWriterDestroy(pWriter);
		xrtZipArchiveDestroy(pArchive);
		xrtClearError();
	}
	xrtBufferDestroy(pPacked);
	return bOk;
}


typedef struct {
	xerrkind WriteKind;
	xerrkind FlushKind;
	xerrkind CloseKind;
	int CloseCalls;
} zipextractfailure;

static bool acceptZipExtractWrite(ptr pContext, const void* pData,
	size_t iRequest, size_t* pWritten)
{
	zipextractfailure* pFailure = (zipextractfailure*)pContext;
	(void)pData;
	if ( pFailure->WriteKind != XERR_NONE ) {
		*pWritten = 0u;
		xrtSetErrorInfo(pFailure->WriteKind, "test.zip", 0,
			"injected extraction write failure");
		return false;
	}
	*pWritten = iRequest;
	return true;
}

static bool failZipExtractFlush(ptr pContext)
{
	zipextractfailure* pFailure = (zipextractfailure*)pContext;
	if ( pFailure->FlushKind == XERR_NONE ) { return true; }
	xrtSetErrorInfo(pFailure->FlushKind, "test.zip", 0,
		"injected extraction flush failure");
	return false;
}

static bool failZipExtractClose(ptr pContext)
{
	zipextractfailure* pFailure = (zipextractfailure*)pContext;
	++pFailure->CloseCalls;
	if ( pFailure->CloseKind == XERR_NONE ) { return true; }
	xrtSetErrorInfo(pFailure->CloseKind, "test.zip", 0,
		"injected extraction close failure");
	return false;
}

/* 提取事务必须关闭 Writer，并保留刷新/关闭的原始故障类别。 */
static int testExtractionWriterErrors(void)
{
	static const struct {
		xerrkind WriteKind;
		xerrkind FlushKind;
		xerrkind CloseKind;
		int ExpectedCode;
	} arrCases[] = {
		{XERR_NONE, XERR_NONE, XERR_NONE, 0},
		{XERR_NONE, XERR_MEMORY, XERR_NONE, XRT_ZIP_ERROR_MEMORY},
		{XERR_NONE, XERR_IO, XERR_NONE, XRT_ZIP_ERROR_IO},
		{XERR_NONE, XERR_NONE, XERR_MEMORY, XRT_ZIP_ERROR_MEMORY},
		{XERR_NONE, XERR_NONE, XERR_IO, XRT_ZIP_ERROR_IO},
		{XERR_NONE, XERR_MEMORY, XERR_IO, XRT_ZIP_ERROR_MEMORY},
		{XERR_MEMORY, XERR_NONE, XERR_IO, XRT_ZIP_ERROR_MEMORY},
		{XERR_IO, XERR_NONE, XERR_MEMORY, XRT_ZIP_ERROR_IO}
	};
	static const char sData[] = "extract data";
	xwriterops Ops = {0};
	xbuffer* pPacked = makeArchive("extract.txt", sData, sizeof(sData) - 1u);
	int bOk = check(pPacked != NULL, "cannot create ZIP extraction error fixture");
	size_t i;
	Ops.Write = acceptZipExtractWrite;
	Ops.Flush = failZipExtractFlush;
	Ops.Close = failZipExtractClose;
	for ( i = 0u; bOk && i < sizeof(arrCases) / sizeof(arrCases[0]); ++i ) {
		zipextractfailure Failure = {arrCases[i].WriteKind, arrCases[i].FlushKind,
			arrCases[i].CloseKind, 0};
		xrtziparchive* pArchive = xrtZipArchiveOpenMemory(
			pPacked->Data, pPacked->Size, false, NULL);
		xwriter* pWriter = xrtWriterCreate(&Ops, &Failure);
		bool bExtract;
		xrtClearError();
		bOk &= check(pArchive != NULL && pWriter != NULL,
			"cannot prepare ZIP extraction output Writer");
		if ( !bOk ) {
			(void)xrtWriterDestroy(pWriter);
			xrtZipArchiveDestroy(pArchive);
			break;
		}
		bExtract = __xrtZipExtractEntryToWriter(pArchive, 0u, pWriter, "test output");
		bOk &= check(bExtract == (arrCases[i].ExpectedCode == 0) &&
			Failure.CloseCalls == 1 &&
			xrtZipArchiveError(pArchive)->iCode == arrCases[i].ExpectedCode &&
			(arrCases[i].ExpectedCode == 0 ||
			 strstr(xrtZipArchiveError(pArchive)->sMessage, "test output") != NULL ||
			 arrCases[i].WriteKind != XERR_NONE),
			"ZIP extraction Writer failure category or close count mismatch");
		xrtZipArchiveDestroy(pArchive);
		xrtClearError();
	}
	xrtBufferDestroy(pPacked);
	return bOk;
}





/* 验证磁盘归档、文件 Source、追加、独占和提交上限契约。 */
static int testFileTransactions(void)
{
	cstr sRoot = "build/tmp/zip_file_transactions";
	cstr sSource = "build/tmp/zip_file_transactions/source.bin";
	cstr sArchive = "build/tmp/zip_file_transactions/archive.zip";
	cstr sLimited = "build/tmp/zip_file_transactions/limited.zip";
	static const uint8 arrSource[] = { 0u, 1u, 2u, 3u, 0xFEu, 0xFFu };
	xrtziparchive* pArchive;
	xbuffer* pData;
	xrtzipoptions Options;
	int bOk = 1;

	if ( xrtDirExists(sRoot) ) { xrtDirRemoveAll(sRoot); }
	bOk &= check(xrtDirCreateAll(sRoot), "cannot create ZIP transaction fixture directory");
	bOk &= check(xrtFileWriteAtomic(sSource,
		(xbytesview){ arrSource, sizeof(arrSource) }),
		"cannot create ZIP AddFile fixture");

	pArchive = xrtZipArchiveOpenFile(sArchive, XRT_ZIP_MODE_WRITE, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		pArchive != NULL ? xrtZipArchiveError(pArchive)->sMessage :
			"cannot allocate disk ZIP writer");
	bOk &= check(xrtZipArchiveAddFile(pArchive, "source.bin", sSource,
		false, XRT_ZIP_METHOD_STORE, 0), xrtZipArchiveError(pArchive)->sMessage);
	bOk &= check(xrtZipArchiveClose(pArchive), xrtZipArchiveError(pArchive)->sMessage);
	xrtZipArchiveDestroy(pArchive);

	pArchive = xrtZipArchiveOpenFile(sArchive, XRT_ZIP_MODE_APPEND, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		pArchive != NULL ? xrtZipArchiveError(pArchive)->sMessage :
			"cannot open disk ZIP for append");
	bOk &= check(xrtZipArchiveAdd(pArchive, "second.txt", "second", 6u,
		false, XRT_ZIP_METHOD_DEFLATE, 6), xrtZipArchiveError(pArchive)->sMessage);
	bOk &= check(xrtZipArchiveClose(pArchive), xrtZipArchiveError(pArchive)->sMessage);
	xrtZipArchiveDestroy(pArchive);

	pArchive = xrtZipArchiveOpenFile(sArchive, XRT_ZIP_MODE_READ, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive) &&
		(xrtZipArchiveCount(pArchive) == 2u), "disk ZIP append lost existing entries");
	pData = xrtZipArchiveRead(pArchive, "source.bin");
	bOk &= check(pData != NULL && (pData->Size == sizeof(arrSource)) &&
		(memcmp(pData->Data, arrSource, sizeof(arrSource)) == 0),
		"ZIP AddFile data mismatch");
	xrtBufferDestroy(pData);
	pData = xrtZipArchiveRead(pArchive, "second.txt");
	bOk &= check(pData != NULL && (pData->Size == 6u) &&
		(memcmp(pData->Data, "second", 6u) == 0), "ZIP append data mismatch");
	xrtBufferDestroy(pData);
	xrtZipArchiveDestroy(pArchive);

	pArchive = xrtZipArchiveOpenFile(sArchive, XRT_ZIP_MODE_EXCLUSIVE, NULL);
	bOk &= check(pArchive != NULL && !xrtZipArchiveIsOpen(pArchive) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_EXISTS),
		"exclusive ZIP mode accepted an existing target");
	xrtZipArchiveDestroy(pArchive);

	xrtZipOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 32u;
	pArchive = xrtZipArchiveOpenFile(sLimited, XRT_ZIP_MODE_WRITE, &Options);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		"cannot create output-limited ZIP archive");
	bOk &= check(xrtZipArchiveAdd(pArchive, "entry.txt", "content", 7u,
		false, XRT_ZIP_METHOD_STORE, 0), xrtZipArchiveError(pArchive)->sMessage);
	bOk &= check(!xrtZipArchiveClose(pArchive) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_LIMIT) &&
		!xrtPathExists(sLimited), "ZIP output limit committed a partial target");
	xrtZipArchiveDestroy(pArchive);
	bOk &= check(!xrtPathExists(sLimited),
		"failed ZIP transaction left its target behind");

	xrtDirRemoveAll(sRoot);
	return bOk;
}





/* 验证所有公开失败路径都保留可判定的 ZIP 错误。 */
static int testArgumentErrors(void)
{
	const char arrInvalidUtf8[] = { (char)0xC0, (char)0xAF, 0 };
	xrtzipoptions Options;
	xrtziparchive* pArchive;
	xrtziparchive* pInvalid;
	xbuffer* pResult;
	int bOk = 1;

	xrtZipOptionsInit(&Options);
	Options.iVersion = 0u;
	pInvalid = xrtZipArchiveOpenMemory(NULL, 1u, false, &Options);
	bOk &= check((pInvalid != NULL) && !xrtZipArchiveIsOpen(pInvalid) &&
		(xrtZipArchiveError(pInvalid)->iCode == XRT_ZIP_ERROR_ARGUMENT) &&
		(strstr(xrtZipArchiveError(pInvalid)->sMessage, "options") != NULL),
		"invalid ZIP options were overwritten by a later argument error");
	xrtZipArchiveDestroy(pInvalid);

	for (int iMode = 0; iMode < 10; ++iMode) {
		xrtZipOptionsInit(&Options);
		switch (iMode) {
		case 0: --Options.iSize; break;
		case 1: ++Options.iSize; break;
		case 2: ++Options.iVersion; break;
		case 3: Options.iReserved = 1u; break;
		case 4: --Options.tLimits.iSize; break;
		case 5: ++Options.tLimits.iVersion; break;
		case 6: Options.tLimits.iReserved = 1u; break;
		case 7: Options.tLimits.iMaxNodes = 1u; break;
		case 8: Options.tLimits.iMaxDepth = 1u; break;
		case 9: Options.tLimits.iFlags = 1u; break;
		}
		pInvalid = xrtZipArchiveOpenMemory(NULL, 1u, false, &Options);
		bOk &= check(pInvalid != NULL && !xrtZipArchiveIsOpen(pInvalid) &&
			xrtZipArchiveError(pInvalid)->iCode == XRT_ZIP_ERROR_ARGUMENT &&
			strstr(xrtZipArchiveError(pInvalid)->sMessage, "options") != NULL,
			"invalid ZIP resource options were accepted");
		xrtZipArchiveDestroy(pInvalid);
	}

	pArchive = xrtZipArchiveCreate(NULL);
	bOk &= check((pArchive != NULL) && xrtZipArchiveIsOpen(pArchive),
		"cannot create ZIP argument fixture");
	bOk &= check((xrtZipArchiveTakeBuffer(pArchive) == NULL) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_STATE),
		"taking an open ZIP Buffer did not report a state error");
	bOk &= check(!xrtZipArchiveAdd(pArchive, NULL, "x", 1u,
		false, XRT_ZIP_METHOD_STORE, 0) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"null ZIP entry name did not report an argument error");
	bOk &= check(!xrtZipArchiveAdd(pArchive, arrInvalidUtf8, "x", 1u,
		false, XRT_ZIP_METHOD_STORE, 0) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"invalid UTF-8 ZIP entry name did not report an argument error");
	bOk &= check(!xrtZipArchiveAddDirectory(pArchive, arrInvalidUtf8) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"invalid UTF-8 ZIP directory name did not report an argument error");
	bOk &= check(xrtZipArchiveAdd(pArchive, "entry.txt", "x", 1u,
		false, XRT_ZIP_METHOD_STORE, 0),
		xrtZipArchiveError(pArchive)->sMessage);
	bOk &= check(!xrtZipArchiveReadAtTo(pArchive, 0u, NULL) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"null ZIP output Writer did not report an argument error");
	bOk &= check(!xrtZipArchiveRead(pArchive, NULL) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"null ZIP lookup name was overwritten as not-found");
	bOk &= check(!xrtZipArchiveRead(pArchive, "missing") &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_NOT_FOUND),
		"missing ZIP entry did not report not-found");
	bOk &= check(!xrtZipArchiveDelete(pArchive, NULL) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"null ZIP delete name was overwritten as not-found");
	bOk &= check(!xrtZipArchiveRename(pArchive, "entry.txt", NULL) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"null ZIP rename target did not report an argument error");
	bOk &= check(!xrtZipArchiveSetComment(pArchive, arrInvalidUtf8) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"invalid ZIP archive comment did not report an argument error");
	bOk &= check(!xrtZipArchiveSetEntryComment(pArchive,
		"entry.txt", arrInvalidUtf8) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"invalid ZIP entry comment did not report an argument error");
	bOk &= check(!xrtZipArchiveExtractAll(pArchive, NULL, false) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_ARGUMENT),
		"null ZIP extraction destination did not report an argument error");
	bOk &= check(xrtZipArchiveClose(pArchive),
		xrtZipArchiveError(pArchive)->sMessage);
	pResult = xrtZipArchiveTakeBuffer(pArchive);
	bOk &= check(pResult != NULL, "cannot take ZIP argument fixture");
	bOk &= check((xrtZipArchiveTakeBuffer(pArchive) == NULL) &&
		(xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_STATE),
		"taking a ZIP Buffer twice did not report a state error");
	xrtBufferDestroy(pResult);
	xrtZipArchiveDestroy(pArchive);
	return bOk;
}



int main(void)
{
	str sPrevious = xrtPathCwd();
	str sTemporary = sPrevious != NULL ?
		xrtDirTemp(NULL, "xlang-zip-test-", "") : NULL;
	str sWorkspace = sTemporary != NULL ? xrtPathAbs(sTemporary) : NULL;
	int bOk = check(sPrevious != NULL && sWorkspace != NULL,
		"cannot create isolated ZIP test directory");
	if ( !bOk ) {
		if ( sTemporary != NULL ) { xrtDirRemoveAll(sTemporary); }
		xrtFree(sWorkspace);
		xrtFree(sTemporary);
		xrtFree(sPrevious);
		return 1;
	}
	if ( !check(xrtPathSetCwd(sWorkspace), "cannot enter ZIP test directory") ) {
		bOk = 0;
	} else {
		bOk = testMemoryArchive();
		bOk &= testLimitsAndCorruption();
		bOk &= testUnsafeExtractionAndOverwrite();
		bOk &= testUtf8FileAndExtract();
		bOk &= testStreamingRead();
		bOk &= testStreamingOutputErrors();
		bOk &= testExtractionWriterErrors();
		bOk &= testFileTransactions();
		bOk &= testArgumentErrors();
		if ( !check(xrtPathSetCwd(sPrevious), "cannot restore ZIP test directory") ) {
			/* 当前目录可能仍在临时目录内，不递归删除它。 */
			xrtFree(sWorkspace);
			xrtFree(sTemporary);
			xrtFree(sPrevious);
			return 1;
		}
	}
	bOk &= check(xrtDirRemoveAll(sWorkspace), "cannot remove ZIP test directory");
	xrtFree(sWorkspace);
	xrtFree(sTemporary);
	xrtFree(sPrevious);
	if ( bOk ) { puts("test_zip: PASS"); }
	return bOk ? 0 : 1;
}
