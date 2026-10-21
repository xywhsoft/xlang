#define XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_PATH_SAFE
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_SET
#define XRT_MODULE_HASH64
#define XRT_MODULE_NUMBER_INTEGER
#include "../../extlibs/gzip/include/xrt_gzip.h"
#include "../../extlibs/tar/include/xrt_tar.h"
#ifdef __TINYC__
/* TCC 直接运行范例时，按包源码顺序合入实现。 */
#include "../../extlibs/gzip/xrt_gzip.c"
#include "../../extlibs/tar/xrt_tar_common.c"
#include "../../extlibs/tar/xrt_tar_reader.c"
#include "../../extlibs/tar/xrt_tar_writer.c"
#endif

#include <stdio.h>
#include <string.h>



int main(void)
{
	xrttarwriter* pTarWriter;
	xrttarreader* pTarReader = NULL;
	xrttarentry tEntry;
	xrtgziperror tGzipError;
	xbuffer* pTar = NULL;
	xbuffer* pCompressed = NULL;
	xbuffer* pPlain = NULL;
	xbuffer* pData = NULL;
	int iResult = 1;
	pTarWriter = xrtTarWriterCreateMemory(NULL);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = "message.txt";
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iMode = 0644u;
	if ( pTarWriter == NULL ||
		!xrtTarWriterAdd(pTarWriter, &tEntry, "tar.gz", 6u) ||
		!xrtTarWriterClose(pTarWriter) ) { goto cleanup; }
	pTar = xrtTarWriterTakeBuffer(pTarWriter);
	if ( !xrtGzipCompress(pTar->Data, pTar->Size, NULL, &pCompressed, &tGzipError) ||
		!xrtGzipDecompress(pCompressed->Data, pCompressed->Size, NULL, &pPlain, &tGzipError) ) {
		fprintf(stderr, "compressed TAR C example: %s\n", tGzipError.sMessage);
		goto cleanup;
	}
	pTarReader = xrtTarReaderOpenMemory(pPlain->Data, pPlain->Size, NULL);
	if ( !xrtTarReaderNext(pTarReader, &tEntry) ) { goto cleanup; }
	pData = xrtTarReaderRead(pTarReader);
	if ( pData == NULL || pData->Size != 6u || memcmp(pData->Data, "tar.gz", 6u) != 0 ) {
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
	return iResult;
}
