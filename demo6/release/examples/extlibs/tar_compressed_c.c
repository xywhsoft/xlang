#include "../../extlibs/gzip/include/xrt_gzip.h"
#include "../../extlibs/tar/include/xrt_tar.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	xrttarwriter* pTarWriter;
	xrttarreader* pTarReader = NULL;
	xrttarentry tEntry;
	xrtgziperror tGzipError;
	xbuffer pTar = NULL;
	xbuffer pCompressed = NULL;
	xbuffer pPlain = NULL;
	xbuffer pData = NULL;
	int iResult = 1;
	if ( xrtInit() == NULL ) { return 1; }
	pTarWriter = xrtTarWriterCreateMemory(NULL);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "message.txt";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iMode = 0644u;
	if ( pTarWriter == NULL ||
		!xrtTarWriterAdd(pTarWriter, &tEntry, "tar.gz", 6u) ||
		!xrtTarWriterClose(pTarWriter) ) { goto cleanup; }
	pTar = xrtTarWriterTakeBuffer(pTarWriter);
	if ( !xrtGzipCompress(pTar->Buffer, pTar->Length, NULL, &pCompressed, &tGzipError) ||
		!xrtGzipDecompress(pCompressed->Buffer, pCompressed->Length, NULL, &pPlain, &tGzipError) ) {
		fprintf(stderr, "compressed TAR C example: %s\n", tGzipError.sMessage);
		goto cleanup;
	}
	pTarReader = xrtTarReaderOpenMemory(pPlain->Buffer, pPlain->Length, NULL);
	if ( !xrtTarReaderNext(pTarReader, &tEntry) ) { goto cleanup; }
	pData = xrtTarReaderRead(pTarReader);
	if ( pData == NULL || pData->Length != 6u || memcmp(pData->Buffer, "tar.gz", 6u) != 0 ) {
		goto cleanup;
	}
	puts("compressed TAR C example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pData);
	xrtTarReaderDestroy(pTarReader);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pCompressed);
	xrtBufferDestroy(pTar);
	xrtTarWriterDestroy(pTarWriter);
	xrtUnit();
	return iResult;
}
