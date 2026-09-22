#define XRT_MODULE_FILE_TREE

#include "../include/xrt_tar.h"

#include <stdio.h>
#include <string.h>



typedef struct tarprogresscontext {
	int iEntries;
	int iFinalEvents;
	int iCancelAfter;
	uint64 iLastInput;
	bool bMonotonic;
	bool bFinalFlag;
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
	if ( pEntry == NULL ) {
		pContext->iFinalEvents += 1;
		pContext->bFinalFlag =
			(pProgress->iFlags & XRT_PROGRESS_FINAL) != 0u;
		return true;
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
	Context.bMonotonic = true;
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	bOk &= check(xrtTarReaderExtract(pReader, sOutput, false,
		onExtractProgress, &Context) && Context.iEntries == 2 &&
		Context.iFinalEvents == 1 && Context.bFinalFlag && Context.bMonotonic,
		"TAR extraction progress contract mismatch");
	xrtTarReaderDestroy(pReader);
	xrtDirRemoveAll(sOutput);

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
	xstrview arrNames[2];
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
	bOk &= check(xrtTarWriterAdd(pWriter, &Entry, "target", 6u) &&
		xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	xrtTarWriterDestroy(pWriter);

	arrNames[0] = xrtStrView("alias.txt");
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(!xrtTarReaderExtractNames(pReader,
		"build/tmp/tar_select_test/link-only", false, arrNames, 1u) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_FORMAT,
		"TAR selector accepted a hard link without its selected target");
	xrtTarReaderDestroy(pReader);

	arrNames[1] = xrtStrView("target.txt");
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, &Options);
	bOk &= check(xrtTarReaderExtractNames(pReader,
		"build/tmp/tar_select_test/link-complete", false, arrNames, 2u) &&
		fileEquals("build/tmp/tar_select_test/link-complete/alias.txt",
			"target", 6u),
		"TAR selector did not resolve a complete hard-link selection");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirRemoveAll(sRoot);
	return bOk;
}



int main(void)
{
	int bOk;
	bOk = testMemoryAndPax();
	bOk &= testSecurityAndLimits();
	bOk &= testLinksAndSpecialEntries();
	bOk &= testRecoveryAndStreaming();
	bOk &= testTransactionAndTree();
	bOk &= testCopyCurrent();
	bOk &= testProgressAndAnchoredExtraction();
	bOk &= testSelectiveExtraction();
	if ( bOk ) { puts("test_tar: PASS"); }
	return bOk ? 0 : 1;
}
