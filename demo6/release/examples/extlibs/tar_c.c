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
#include "../../extlibs/tar/include/xrt_tar.h"
#ifdef __TINYC__
/* TCC 直接运行范例时，包实现与范例共用一个编译单元。 */
#include "../../extlibs/tar/xrt_tar_common.c"
#include "../../extlibs/tar/xrt_tar_reader.c"
#include "../../extlibs/tar/xrt_tar_writer.c"
#include "../../extlibs/tar/xrt_tar_extract.c"
#endif

#include <stdio.h>
#include <string.h>



int main(void)
{
	str sOutput = NULL;
	str sSelectedPath = NULL;
	xrttarwriter* pWriter = NULL;
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
	sOutput = xrtDirTemp(NULL, "xlang-tar-c-example-", NULL);
	if ( sOutput == NULL ) {
		fprintf(stderr, "tar C example: cannot create temporary directory\n");
		goto cleanup;
	}
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
	if ( pPacked == NULL ) {
		fprintf(stderr, "tar C example: cannot take archive buffer\n");
		goto cleanup;
	}
	pReader = xrtTarReaderOpenMemory(pPacked->Data, pPacked->Size, NULL);
	if ( pReader == NULL || !xrtTarReaderNext(pReader, &tEntry) || !xrtTarReaderSkip(pReader) ||
		!xrtTarReaderNext(pReader, &tEntry) ) {
		fprintf(stderr, "tar C example: %s\n",
			pReader != NULL ? xrtTarReaderError(pReader)->sMessage :
				"cannot create archive reader");
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
	sSelectedPath = xrtPathJoin(sOutput, "docs/hello.txt");
	if ( sSelectedPath == NULL ) { goto cleanup; }
	pSelected = xrtFileReadAll(sSelectedPath, &iSelectedSize);
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
	xrtFree(sSelectedPath);
	if ( sOutput != NULL && !xrtDirRemoveAll(sOutput) ) {
		fprintf(stderr, "tar C example: cannot remove temporary directory\n");
		iResult = 1;
	}
	xrtFree(sOutput);
	return iResult;
}
