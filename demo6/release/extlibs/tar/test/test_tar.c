#include "../include/xrt_tar.h"

#include <stdio.h>
#include <string.h>



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_tar: %s\n", sMessage != NULL && sMessage[0] != 0 ? sMessage : "check failed");
	return 0;
}

static xbuffer makeArchive(const char* sName, const void* pData, size_t iSize)
{
	xrttarwriter* pWriter = xrtTarWriterCreateMemory(NULL);
	xrttarentry tEntry;
	xbuffer pResult = NULL;
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
	xbuffer pPacked;
	xbuffer pData;
	int iCount = 0;
	int bOk = 1;
	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowSymlinks = TRUE;
	pWriter = xrtTarWriterCreateMemory(&tOptions);
	bOk &= check(pWriter != NULL, "cannot create TAR memory writer");
	bOk &= check(xrtTarWriterAddDirectory(pWriter, (str)"folder", 0755u, 0),
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
	bOk &= check(xrtTarWriterAddSymlink(pWriter, (str)"hello-link", (str)"folder/hello.txt", 0777u, 0),
		xrtTarWriterError(pWriter)->sMessage);
	bOk &= check(xrtTarWriterClose(pWriter), xrtTarWriterError(pWriter)->sMessage);
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	bOk &= check(pPacked != NULL && pPacked->Length % 512u == 0u, "invalid TAR memory output");
	xrtTarWriterDestroy(pWriter);

	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, NULL);
	while ( xrtTarReaderNext(pReader, &tEntry) ) {
		if ( iCount == 0 ) {
			bOk &= check(tEntry.iType == XRT_TAR_ENTRY_DIRECTORY &&
				strcmp(tEntry.sName, "folder/") == 0, "TAR directory metadata mismatch");
			bOk &= check(xrtTarReaderSkip(pReader), xrtTarReaderError(pReader)->sMessage);
		}
		else if ( iCount == 1 ) {
			pData = xrtTarReaderRead(pReader);
			bOk &= check(pData != NULL && pData->Length == 5u &&
				memcmp(pData->Buffer, "hello", 5u) == 0, "TAR text data mismatch");
			xrtBufferDestroy(pData);
		}
		else if ( iCount == 2 ) {
			pData = xrtTarReaderRead(pReader);
			bOk &= check(pData != NULL && pData->Length == sizeof(sBinary) &&
				memcmp(pData->Buffer, sBinary, sizeof(sBinary)) == 0, "TAR binary data mismatch");
			xrtBufferDestroy(pData);
		}
		else if ( iCount == 3 ) {
			bOk &= check(strcmp(tEntry.sName, sLongName) == 0, "TAR PAX path mismatch");
			pData = xrtTarReaderRead(pReader);
			bOk &= check(pData != NULL && pData->Length == 4u, "TAR PAX entry data mismatch");
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
	str sRoot = (str)"build/tmp/tar_安全_test";
	str sOutput = (str)"build/tmp/tar_安全_test/output";
	str sFile = (str)"build/tmp/tar_安全_test/output/目录/内容.txt";
	xbuffer pPacked;
	xrttarreader* pReader;
	xrttaroptions tOptions;
	ptr pFileData;
	size_t iFileSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirDelete(sRoot); }
	pPacked = makeArchive("目录/内容.txt", "UTF-8", 5u);
	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, NULL);
	bOk &= check(xrtTarReaderExtractAll(pReader, sOutput, FALSE),
		xrtTarReaderError(pReader)->sMessage);
	pFileData = xrtFileGetAll(sFile, &iFileSize);
	bOk &= check(pFileData != NULL && iFileSize == 5u &&
		memcmp(pFileData, "UTF-8", 5u) == 0, "TAR UTF-8 extraction mismatch");
	xrtFree(pFileData);
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);

	pPacked = makeArchive("abcde", "bad", 3u);
	memcpy(pPacked->Buffer, "../xy", 5u);
	updateHeaderChecksum((unsigned char*)pPacked->Buffer);
	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, NULL);
	bOk &= check(!xrtTarReaderExtractAll(pReader, sOutput, TRUE) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_UNSAFE_PATH,
		"unsafe TAR extraction path was accepted");
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);

	pPacked = makeArchive("data", "1234", 4u);
	xrtTarOptionsInit(&tOptions);
	tOptions.tLimits.iMaxItemBytes = 3u;
	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, &tOptions);
	{
		xrttarentry tEntry;
		bOk &= check(!xrtTarReaderNext(pReader, &tEntry) &&
			xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_LIMIT,
			"TAR item limit was not enforced");
	}
	xrtTarReaderDestroy(pReader);

	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length - 1u, NULL);
	{
		xrttarentry tEntry;
		bOk &= check(xrtTarReaderNext(pReader, &tEntry), "truncated TAR fixture lost its first entry");
		bOk &= check(xrtTarReaderSkip(pReader) && !xrtTarReaderNext(pReader, &tEntry) &&
			!xrtTarReaderEOF(pReader) && xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_FORMAT,
			"truncated TAR end marker was accepted");
	}
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtDirDelete(sRoot);
	return bOk;
}

static int testLinksAndSpecialEntries(void)
{
	str sRoot = (str)"build/tmp/tar_links_test";
	str sOutput = (str)"build/tmp/tar_links_test/output";
	str sTarget = (str)"build/tmp/tar_links_test/output/target.txt";
	str sLink = (str)"build/tmp/tar_links_test/output/forward.txt";
	xrttaroptions tOptions;
	xrttarwriter* pWriter;
	xrttarreader* pReader;
	xrttarentry tEntry;
	xbuffer pPacked;
	ptr pData;
	size_t iSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirDelete(sRoot); }

	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowHardlinks = TRUE;
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

	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, NULL);
	bOk &= check(!xrtTarReaderExtractAll(pReader, sOutput, FALSE) &&
		xrtTarReaderError(pReader)->iCode == XRT_TAR_ERROR_UNSUPPORTED,
		"TAR hard link was extracted without explicit permission");
	xrtTarReaderDestroy(pReader);
	if ( xrtDirExists(sRoot) ) { xrtDirDelete(sRoot); }

	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowHardlinks = TRUE;
	tOptions.bPreservePermissions = TRUE;
	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, &tOptions);
	bOk &= check(xrtTarReaderExtractAll(pReader, sOutput, FALSE),
		xrtTarReaderError(pReader)->sMessage);
	xrtTarReaderDestroy(pReader);
	pData = xrtFileGetAll(sLink, &iSize);
	bOk &= check(pData != NULL && iSize == 6u && memcmp(pData, "linked", 6u) == 0,
		"forward TAR hard link was not resolved");
	xrtFree(pData);
	pData = xrtFileGetAll(sTarget, &iSize);
	bOk &= check(pData != NULL && iSize == 6u && memcmp(pData, "linked", 6u) == 0,
		"TAR hard-link target was not extracted");
	xrtFree(pData);
	#if !defined(_WIN32) && !defined(_WIN64)
		bOk &= check((xrtFileGetAttr(sTarget) & 0777) == 0644,
			"TAR file permissions were not restored");
	#endif
	xrtBufferDestroy(pPacked);
	if ( xrtDirExists(sRoot) ) { xrtDirDelete(sRoot); }

	xrtTarOptionsInit(&tOptions);
	tOptions.bAllowSpecialFiles = TRUE;
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
	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, &tOptions);
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

int main(void)
{
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	bOk = testMemoryAndPax();
	bOk &= testSecurityAndLimits();
	bOk &= testLinksAndSpecialEntries();
	xrtUnit();
	if ( bOk ) { puts("test_tar: PASS"); }
	return bOk ? 0 : 1;
}
