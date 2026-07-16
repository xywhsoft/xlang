#include "../include/xrt_zip.h"

#include <stdio.h>
#include <string.h>



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
	xbuffer pArchive;
	xbuffer pData;
	xrtzipentry tEntry;
	int bOk = 1;
	bOk &= check(pWriter != NULL && xrtZipArchiveIsOpen(pWriter),
		pWriter != NULL ? xrtZipArchiveError(pWriter)->sMessage : "cannot allocate ZIP writer");
	bOk &= check(xrtZipArchiveAddDirectory(pWriter, (str)"folder"), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveAdd(pWriter, (str)"folder/hello.txt", "hello", 5u,
		FALSE, XRT_ZIP_METHOD_DEFLATE, 9), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveAdd(pWriter, (str)"binary.bin", sBinary, sizeof(sBinary),
		FALSE, XRT_ZIP_METHOD_STORE, 0), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(!xrtZipArchiveAdd(pWriter, (str)"../escape.txt", "bad", 3u,
		FALSE, XRT_ZIP_METHOD_STORE, 0) &&
		xrtZipArchiveError(pWriter)->iCode == XRT_ZIP_ERROR_UNSAFE_PATH,
		"unsafe ZIP path was accepted");
	bOk &= check(xrtZipArchiveSetComment(pWriter, (str)"archive comment"), xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveSetEntryComment(pWriter, (str)"binary.bin", (str)"entry comment"),
		xrtZipArchiveError(pWriter)->sMessage);
	bOk &= check(xrtZipArchiveCount(pWriter) == 3u, "unexpected ZIP entry count before close");
	bOk &= check(xrtZipArchiveClose(pWriter), xrtZipArchiveError(pWriter)->sMessage);
	pArchive = xrtZipArchiveTakeBuffer(pWriter);
	bOk &= check(pArchive != NULL && pArchive->Length > 100u, "cannot take ZIP memory result");
	xrtZipArchiveDestroy(pWriter);

	pReader = xrtZipArchiveOpenMemory(pArchive->Buffer, pArchive->Length, FALSE, NULL);
	bOk &= check(pReader != NULL && xrtZipArchiveIsOpen(pReader),
		pReader != NULL ? xrtZipArchiveError(pReader)->sMessage : "cannot allocate ZIP reader");
	bOk &= check(xrtZipArchiveCount(pReader) == 3u, "unexpected ZIP entry count after reopen");
	bOk &= check(xrtZipArchiveEntryAt(pReader, 2u, &tEntry) &&
		tEntry.sName != NULL && strcmp(tEntry.sName, "binary.bin") == 0 &&
		strcmp(tEntry.sComment, "entry comment") == 0, "ZIP entry metadata mismatch");
	pData = xrtZipArchiveRead(pReader, (str)"folder/hello.txt");
	bOk &= check(pData != NULL && pData->Length == 5u &&
		memcmp(pData->Buffer, "hello", 5u) == 0, "ZIP text entry mismatch");
	xrtBufferDestroy(pData);
	pData = xrtZipArchiveRead(pReader, (str)"binary.bin");
	bOk &= check(pData != NULL && pData->Length == sizeof(sBinary) &&
		memcmp(pData->Buffer, sBinary, sizeof(sBinary)) == 0, "ZIP binary entry mismatch");
	xrtBufferDestroy(pData);
	bOk &= check(xrtZipArchiveClose(pReader), xrtZipArchiveError(pReader)->sMessage);
	xrtZipArchiveDestroy(pReader);
	xrtBufferDestroy(pArchive);
	return bOk;
}



static xbuffer makeArchive(str sName, const void* pData, size_t iSize)
{
	xrtziparchive* pWriter = xrtZipArchiveCreate(NULL);
	xbuffer pResult = NULL;
	if ( pWriter != NULL && xrtZipArchiveIsOpen(pWriter) &&
		xrtZipArchiveAdd(pWriter, sName, pData, iSize, FALSE, XRT_ZIP_METHOD_DEFLATE, 9) &&
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
	xbuffer pPacked;
	xbuffer pData;
	int bOk = 1;
	memset(sLarge, 'A', sizeof(sLarge));

	xrtZipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxEntries = 1u;
	pArchive = xrtZipArchiveCreate(&tOptions);
	bOk &= check(pArchive != NULL && xrtZipArchiveAdd(pArchive, (str)"one", "1", 1u,
		FALSE, XRT_ZIP_METHOD_STORE, 0), "cannot add first limited ZIP entry");
	bOk &= check(!xrtZipArchiveAdd(pArchive, (str)"two", "2", 1u,
		FALSE, XRT_ZIP_METHOD_STORE, 0) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_LIMIT,
		"ZIP entry-count limit was not enforced");
	xrtZipArchiveDestroy(pArchive);

	xrtZipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxItemBytes = 3u;
	pArchive = xrtZipArchiveCreate(&tOptions);
	bOk &= check(!xrtZipArchiveAdd(pArchive, (str)"large", "1234", 4u,
		FALSE, XRT_ZIP_METHOD_STORE, 0) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_LIMIT,
		"ZIP item limit was not enforced while adding");
	xrtZipArchiveDestroy(pArchive);

	pPacked = makeArchive((str)"data.bin", sLarge, sizeof(sLarge));
	bOk &= check(pPacked != NULL, "cannot create ZIP limit fixture");
	xrtZipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 128u;
	tOptions.tLimits.iMaxItemBytes = 128u;
	pArchive = xrtZipArchiveOpenMemory(pPacked->Buffer, pPacked->Length, FALSE, &tOptions);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		"cannot open ZIP limit fixture");
	pData = xrtZipArchiveRead(pArchive, (str)"data.bin");
	bOk &= check(pData == NULL && xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_LIMIT,
		"ZIP output limit was not enforced while reading");
	xrtBufferDestroy(pData);
	xrtZipArchiveDestroy(pArchive);

	if ( pPacked != NULL && pPacked->Length > 8u ) {
		pArchive = xrtZipArchiveOpenMemory(pPacked->Buffer, pPacked->Length - 8u, FALSE, NULL);
		bOk &= check(pArchive != NULL && !xrtZipArchiveIsOpen(pArchive),
			"truncated ZIP archive was accepted");
		xrtZipArchiveDestroy(pArchive);
	}
	xrtBufferDestroy(pPacked);
	return bOk;
}



static int replaceArchiveName(xbuffer pArchive, const char* sOldName, const char* sNewName)
{
	size_t i;
	size_t iLength = strlen(sOldName);
	int iCount = 0;
	if ( pArchive == NULL || strlen(sNewName) != iLength ) { return 0; }
	for ( i = 0u; i + iLength <= pArchive->Length; ++i ) {
		if ( memcmp(pArchive->Buffer + i, sOldName, iLength) == 0 ) {
			memcpy(pArchive->Buffer + i, sNewName, iLength);
			i += iLength - 1u;
			iCount += 1;
		}
	}
	return iCount;
}



static int testUnsafeExtractionAndOverwrite(void)
{
	str sRoot = (str)"build/tmp/zip_security_test";
	str sDestination = (str)"build/tmp/zip_security_test/output";
	str sExisting = (str)"build/tmp/zip_security_test/output/existing.txt";
	xbuffer pPacked;
	xrtziparchive* pArchive;
	ptr pData;
	size_t iSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirDelete(sRoot); }
	bOk &= check(xrtDirCreateAll(sDestination), "cannot create ZIP security test directory");

	pPacked = makeArchive((str)"abcde", "bad", 3u);
	bOk &= check(replaceArchiveName(pPacked, "abcde", "../xy") >= 2,
		"cannot build unsafe ZIP path fixture");
	pArchive = xrtZipArchiveOpenMemory(pPacked->Buffer, pPacked->Length, FALSE, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		"unsafe ZIP fixture itself cannot be opened");
	bOk &= check(!xrtZipArchiveExtractAll(pArchive, sDestination, FALSE) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_UNSAFE_PATH,
		"unsafe ZIP extraction path was accepted");
	xrtZipArchiveDestroy(pArchive);
	xrtBufferDestroy(pPacked);

	pPacked = makeArchive((str)"existing.txt", "new", 3u);
	bOk &= check(xrtFileWriteAllAtomic(sExisting, (str)"old", 3u, XRT_CP_BINARY),
		"cannot create ZIP overwrite fixture");
	pArchive = xrtZipArchiveOpenMemory(pPacked->Buffer, pPacked->Length, FALSE, NULL);
	bOk &= check(!xrtZipArchiveExtractAll(pArchive, sDestination, FALSE) &&
		xrtZipArchiveError(pArchive)->iCode == XRT_ZIP_ERROR_EXISTS,
		"ZIP extraction overwrote a file without permission");
	bOk &= check(xrtZipArchiveExtractAll(pArchive, sDestination, TRUE),
		xrtZipArchiveError(pArchive)->sMessage);
	pData = xrtFileGetAll(sExisting, &iSize);
	bOk &= check(pData != NULL && iSize == 3u && memcmp(pData, "new", 3u) == 0,
		"ZIP overwrite extraction produced the wrong data");
	xrtFree(pData);
	xrtZipArchiveDestroy(pArchive);
	xrtBufferDestroy(pPacked);
	xrtDirDelete(sRoot);
	return bOk;
}



static int testUtf8FileAndExtract(void)
{
	str sRoot = (str)"build/tmp/zip_中文_test";
	str sArchivePath = (str)"build/tmp/zip_中文_test/归档.zip";
	str sExtractPath = (str)"build/tmp/zip_中文_test/输出";
	str sExtracted = (str)"build/tmp/zip_中文_test/输出/目录/内容.txt";
	xrtziparchive* pArchive;
	ptr pData;
	size_t iSize = 0u;
	int bOk = 1;
	if ( xrtDirExists(sRoot) ) { xrtDirDelete(sRoot); }
	bOk &= check(xrtDirCreateAll(sRoot), "cannot create UTF-8 ZIP test directory");
	pArchive = xrtZipArchiveOpenFile(sArchivePath, XRT_ZIP_MODE_WRITE, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		pArchive != NULL ? xrtZipArchiveError(pArchive)->sMessage : "cannot allocate file ZIP writer");
	bOk &= check(xrtZipArchiveAdd(pArchive, (str)"目录/内容.txt", "UTF-8 path", 10u,
		FALSE, XRT_ZIP_METHOD_DEFLATE, 6), xrtZipArchiveError(pArchive)->sMessage);
	bOk &= check(xrtZipArchiveClose(pArchive), xrtZipArchiveError(pArchive)->sMessage);
	xrtZipArchiveDestroy(pArchive);
	bOk &= check(xrtFileExists(sArchivePath), "UTF-8 ZIP file was not saved");

	pArchive = xrtZipArchiveOpenFile(sArchivePath, XRT_ZIP_MODE_READ, NULL);
	bOk &= check(pArchive != NULL && xrtZipArchiveIsOpen(pArchive),
		pArchive != NULL ? xrtZipArchiveError(pArchive)->sMessage : "cannot allocate file ZIP reader");
	bOk &= check(xrtZipArchiveExtractAll(pArchive, sExtractPath, FALSE),
		xrtZipArchiveError(pArchive)->sMessage);
	xrtZipArchiveDestroy(pArchive);
	pData = xrtFileGetAll(sExtracted, &iSize);
	bOk &= check(pData != NULL && iSize == 10u && memcmp(pData, "UTF-8 path", 10u) == 0,
		"UTF-8 ZIP extraction mismatch");
	xrtFree(pData);
	xrtDirDelete(sRoot);
	return bOk;
}



int main(void)
{
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	bOk = testMemoryArchive();
	bOk &= testLimitsAndCorruption();
	bOk &= testUnsafeExtractionAndOverwrite();
	bOk &= testUtf8FileAndExtract();
	xrtUnit();
	if ( bOk ) { puts("test_zip: PASS"); }
	return bOk ? 0 : 1;
}
