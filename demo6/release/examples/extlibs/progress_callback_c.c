#define XRT_MODULE_IO_FILE

#include "../../extlibs/md5/include/xrt_md5.h"

#include <stdio.h>
#include <string.h>



typedef struct ProgressState {
	int Calls;
	bool Cancel;
	uint64 LastInput;
	uint64 LastTotal;
	uint64 LastOutput;
	uint32 LastFlags;
} ProgressState;

static bool report_progress(const xrtprogress* pProgress, ptr pUserData)
{
	ProgressState* pState = (ProgressState*)pUserData;
	if ( pProgress == NULL || pProgress->iSize < sizeof(xrtprogress) ||
		pProgress->iVersion != XRT_PROGRESS_VERSION ) {
		return false;
	}
	pState->Calls++;
	pState->LastInput = pProgress->iInputBytes;
	pState->LastTotal = pProgress->iTotalInputBytes;
	pState->LastOutput = pProgress->iOutputBytes;
	pState->LastFlags = pProgress->iFlags;
	return !pState->Cancel;
}



int main(void)
{
	static const char sChunk[] = "xlang C progress callback\n";
	static const char sPath[] = "tmp/extlib_progress_callback_c.txt";
	xrtresourcelimits tLimits;
	xrtmd5error tError;
	ProgressState tState = {0};
	xwriter* pWriter = NULL;
	xreader* pReader = NULL;
	uint8 pDigest[XRT_MD5_DIGEST_SIZE];
	uint64 iExpected = 0u;
	int i;
	int iResult = 1;

	pWriter = xrtWriterOpen(sPath);
	if ( pWriter == NULL ) { goto cleanup; }
	for ( i = 0; i < 5000; i++ ) {
		if ( !xrtWriterWriteFull(pWriter, sChunk, sizeof(sChunk) - 1u, NULL) ) {
			goto cleanup;
		}
	}
	xrtWriterDestroy(pWriter);
	pWriter = NULL;

	pReader = xrtReaderOpen((str)sPath);
	if ( pReader == NULL || !xrtReaderSize(pReader, &iExpected) ) { goto cleanup; }
	xrtResourceLimitsInit(&tLimits);
	if ( !xrtMd5File(pReader, &tLimits, pDigest, report_progress, &tState, &tError) ) {
		fprintf(stderr, "MD5 progress failed: %s\n", tError.sMessage);
		goto cleanup;
	}
	xrtReaderDestroy(pReader);
	pReader = NULL;
	if ( tState.Calls == 0 || tState.LastInput != iExpected || tState.LastTotal != iExpected ||
		tState.LastOutput != 0u || (tState.LastFlags & XRT_PROGRESS_FINAL) == 0u ) {
		fprintf(stderr, "invalid final progress event\n");
		goto cleanup;
	}

	memset(&tState, 0, sizeof(tState));
	tState.Cancel = true;
	pReader = xrtReaderOpen((str)sPath);
	if ( pReader == NULL ) { goto cleanup; }
	if ( xrtMd5File(pReader, &tLimits, pDigest, report_progress, &tState, &tError) ||
		tError.iCode != XRT_MD5_ERROR_CANCELLED ) {
		fprintf(stderr, "C cancellation contract failed\n");
		goto cleanup;
	}

	puts("XRT C progress callback: PASS");
	iResult = 0;

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	xrtFileDelete((str)sPath);
	return iResult;
}
