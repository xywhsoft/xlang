#include "../../extlibs/zip/include/xrt_zip.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	static const unsigned char sBinary[] = { 0u, 1u, 0xffu };
	xrtziparchive* pWriter;
	xrtziparchive* pReader = NULL;
	xbuffer pPacked = NULL;
	xbuffer pText = NULL;
	xbuffer pBinary = NULL;
	int iResult = 1;
	if ( xrtInit() == NULL ) { return 1; }
	pWriter = xrtZipArchiveCreate(NULL);
	if ( pWriter == NULL ||
		!xrtZipArchiveAddDirectory(pWriter, (str)"docs") ||
		!xrtZipArchiveAdd(pWriter, (str)"docs/hello.txt", "hello ZIP", 9u,
			FALSE, XRT_ZIP_METHOD_DEFLATE, 6) ||
		!xrtZipArchiveAdd(pWriter, (str)"data.bin", sBinary, sizeof(sBinary),
			FALSE, XRT_ZIP_METHOD_STORE, 0) ||
		!xrtZipArchiveClose(pWriter) ) {
		fprintf(stderr, "zip C example: %s\n",
			pWriter != NULL ? xrtZipArchiveError(pWriter)->sMessage : "cannot create archive");
		goto cleanup;
	}
	pPacked = xrtZipArchiveTakeBuffer(pWriter);
	pReader = xrtZipArchiveOpenMemory(pPacked->Buffer, pPacked->Length, FALSE, NULL);
	pText = xrtZipArchiveRead(pReader, (str)"docs/hello.txt");
	pBinary = xrtZipArchiveRead(pReader, (str)"data.bin");
	if ( pText == NULL || pText->Length != 9u || memcmp(pText->Buffer, "hello ZIP", 9u) != 0 ||
		pBinary == NULL || pBinary->Length != sizeof(sBinary) ||
		memcmp(pBinary->Buffer, sBinary, sizeof(sBinary)) != 0 ) {
		fprintf(stderr, "zip C example: archive contents mismatch\n");
		goto cleanup;
	}
	puts("zip C example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pBinary);
	xrtBufferDestroy(pText);
	xrtZipArchiveDestroy(pReader);
	xrtBufferDestroy(pPacked);
	xrtZipArchiveDestroy(pWriter);
	xrtUnit();
	return iResult;
}
