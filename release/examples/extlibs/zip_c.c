#include "../../extlibs/zip/include/xrt_zip.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	static const unsigned char sBinary[] = { 0u, 0xffu };
	xrtziparchive* pWriter;
	xrtziparchive* pReader = NULL;
	xbuffer* pPacked = NULL;
	xbuffer* pText = NULL;
	xbuffer* pBinary = NULL;
	xrtzipentry tEntry;
	int iResult = 1;
	pWriter = xrtZipArchiveCreate(NULL);
	if ( pWriter == NULL ||
		!xrtZipArchiveAddDirectory(pWriter, "docs") ||
		!xrtZipArchiveAdd(pWriter, "docs/hello.txt", "hello ZIP", 9u,
			false, XRT_ZIP_METHOD_DEFLATE, 6) ||
		!xrtZipArchiveAdd(pWriter, "data.bin", sBinary, sizeof(sBinary),
			false, XRT_ZIP_METHOD_STORE, 0) ||
		!xrtZipArchiveSetEntryComment(pWriter, "data.bin", "binary example") ||
		!xrtZipArchiveClose(pWriter) ) {
		fprintf(stderr, "zip C example: %s\n",
			pWriter != NULL ? xrtZipArchiveError(pWriter)->sMessage : "cannot create archive");
		goto cleanup;
	}
	pPacked = xrtZipArchiveTakeBuffer(pWriter);
	if ( pPacked == NULL ) { goto cleanup; }
	pReader = xrtZipArchiveOpenMemory(pPacked->Data, pPacked->Size, false, NULL);
	if ( pReader == NULL || !xrtZipArchiveIsOpen(pReader) ) { goto cleanup; }
	pText = xrtZipArchiveRead(pReader, "docs/hello.txt");
	pBinary = xrtZipArchiveRead(pReader, "data.bin");
	if ( pText == NULL || pText->Size != 9u || memcmp(pText->Data, "hello ZIP", 9u) != 0 ||
		pBinary == NULL || pBinary->Size != sizeof(sBinary) ||
		memcmp(pBinary->Data, sBinary, sizeof(sBinary)) != 0 ||
		xrtZipArchiveCount(pReader) != 3u ||
		!xrtZipArchiveEntryAt(pReader, 2u, &tEntry) ||
		strcmp(tEntry.sComment, "binary example") != 0 ) {
		fprintf(stderr, "zip C example: archive contents mismatch\n");
		goto cleanup;
	}
	puts("zip C/XL example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pBinary);
	xrtBufferDestroy(pText);
	xrtZipArchiveDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtZipArchiveDestroy(pWriter);
	return iResult;
}
