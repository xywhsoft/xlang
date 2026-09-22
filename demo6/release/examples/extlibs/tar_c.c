#include "../../extlibs/tar/include/xrt_tar.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	cstr sOutput = "build/tmp/tar_c_selected";
	xrttarwriter* pWriter;
	xrttarreader* pReader = NULL;
	xrttarentry tEntry;
	xbuffer* pPacked = NULL;
	xbuffer* pData = NULL;
	bytes pSelected = NULL;
	size_t iSelectedSize = 0u;
	xstrview SelectedName;
	int iResult = 1;
	SelectedName.Data = "docs/hello.txt";
	SelectedName.Size = sizeof("docs/hello.txt") - 1u;
	if ( xrtDirExists(sOutput) ) { xrtDirRemoveAll(sOutput); }
	pWriter = xrtTarWriterCreateMemory(NULL);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "docs/hello.txt";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iMode = 0644u;
	if ( pWriter == NULL ||
		!xrtTarWriterAddDirectory(pWriter, "docs", 0755u, 0) ||
		!xrtTarWriterAdd(pWriter, &tEntry, "hello TAR", 9u) ||
		!xrtTarWriterClose(pWriter) ) {
		fprintf(stderr, "tar C example: %s\n",
			pWriter != NULL ? xrtTarWriterError(pWriter)->sMessage : "cannot create archive");
		goto cleanup;
	}
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	if ( !xrtTarReaderNext(pReader, &tEntry) || !xrtTarReaderSkip(pReader) ||
		!xrtTarReaderNext(pReader, &tEntry) ) {
		fprintf(stderr, "tar C example: %s\n", xrtTarReaderError(pReader)->sMessage);
		goto cleanup;
	}
	pData = xrtTarReaderRead(pReader);
	if ( pData == NULL || pData->Size != 9u ||
		memcmp(pData->Data, "hello TAR", 9u) != 0 ) {
		fprintf(stderr, "tar C example: archive contents mismatch\n");
		goto cleanup;
	}
	xrtTarReaderDestroy(pReader);
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	if ( pReader == NULL || !xrtTarReaderExtractNames(pReader,
		sOutput, false, &SelectedName, 1u) ) {
		fprintf(stderr, "tar C example: %s\n",
			pReader != NULL ? xrtTarReaderError(pReader)->sMessage :
				"cannot create selection reader");
		goto cleanup;
	}
	pSelected = xrtFileReadAll(
		"build/tmp/tar_c_selected/docs/hello.txt", &iSelectedSize);
	if ( pSelected == NULL || iSelectedSize != 9u ||
		memcmp(pSelected, "hello TAR", 9u) != 0 ) {
		fprintf(stderr, "tar C example: selected extraction mismatch\n");
		goto cleanup;
	}
	puts("tar C example: PASS");
	iResult = 0;

cleanup:
	xrtFree(pSelected);
	xrtBufferDestroy(pData);
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtTarWriterDestroy(pWriter);
	if ( xrtDirExists(sOutput) ) { xrtDirRemoveAll(sOutput); }
	return iResult;
}
