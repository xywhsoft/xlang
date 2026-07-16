#include "../../extlibs/tar/include/xrt_tar.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	xrttarwriter* pWriter;
	xrttarreader* pReader = NULL;
	xrttarentry tEntry;
	xbuffer pPacked = NULL;
	xbuffer pData = NULL;
	int iResult = 1;
	if ( xrtInit() == NULL ) { return 1; }
	pWriter = xrtTarWriterCreateMemory(NULL);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "docs/hello.txt";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iMode = 0644u;
	if ( pWriter == NULL ||
		!xrtTarWriterAddDirectory(pWriter, (str)"docs", 0755u, 0) ||
		!xrtTarWriterAdd(pWriter, &tEntry, "hello TAR", 9u) ||
		!xrtTarWriterClose(pWriter) ) {
		fprintf(stderr, "tar C example: %s\n",
			pWriter != NULL ? xrtTarWriterError(pWriter)->sMessage : "cannot create archive");
		goto cleanup;
	}
	pPacked = xrtTarWriterTakeBuffer(pWriter);
	pReader = xrtTarReaderOpenMemory(pPacked->Buffer, pPacked->Length, NULL);
	if ( !xrtTarReaderNext(pReader, &tEntry) || !xrtTarReaderSkip(pReader) ||
		!xrtTarReaderNext(pReader, &tEntry) ) {
		fprintf(stderr, "tar C example: %s\n", xrtTarReaderError(pReader)->sMessage);
		goto cleanup;
	}
	pData = xrtTarReaderRead(pReader);
	if ( pData == NULL || pData->Length != 9u ||
		memcmp(pData->Buffer, "hello TAR", 9u) != 0 ) {
		fprintf(stderr, "tar C example: archive contents mismatch\n");
		goto cleanup;
	}
	puts("tar C example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pData);
	xrtTarReaderDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtTarWriterDestroy(pWriter);
	xrtUnit();
	return iResult;
}
