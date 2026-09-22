#include "../../extlibs/zip/include/xrt_zip.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	static const unsigned char sBinary[] = { 0u, 1u, 0xffu };
	xrtziparchive* pWriter;
	xrtziparchive* pReader = NULL;
	xbuffer* pPacked = NULL;
	xbuffer* pText = NULL;
	xbuffer* pBinary = NULL;
	int iResult = 1;
	pWriter = xrtZipArchiveCreate(NULL);
	if ( pWriter == NULL ||
		!xrtZipArchiveAddDirectory(pWriter, "docs") ||
		!xrtZipArchiveAdd(pWriter, "docs/hello.txt", "hello ZIP", 9u,
			false, XRT_ZIP_METHOD_DEFLATE, 6) ||
		!xrtZipArchiveAdd(pWriter, "data.bin", sBinary, sizeof(sBinary),
			false, XRT_ZIP_METHOD_STORE, 0) ||
		!xrtZipArchiveClose(pWriter) ) {
		fprintf(stderr, "zip C example: %s\n",
			pWriter != NULL ? xrtZipArchiveError(pWriter)->sMessage : "cannot create archive");
		goto cleanup;
	}
	pPacked = xrtZipArchiveTakeBuffer(pWriter);
	pReader = xrtZipArchiveOpenMemory(pPacked->Data, pPacked->Size, false, NULL);
	pText = xrtZipArchiveRead(pReader, "docs/hello.txt");
	pBinary = xrtZipArchiveRead(pReader, "data.bin");
	if ( pText == NULL || pText->Size != 9u || memcmp(pText->Data, "hello ZIP", 9u) != 0 ||
		pBinary == NULL || pBinary->Size != sizeof(sBinary) ||
		memcmp(pBinary->Data, sBinary, sizeof(sBinary)) != 0 ) {
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
	return iResult;
}
