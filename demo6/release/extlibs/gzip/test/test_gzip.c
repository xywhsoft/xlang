#define XRT_MODULE_IO_BUFFER

#include "../include/xrt_gzip.h"

#include <stdio.h>
#include <string.h>



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_gzip: %s\n", sMessage);
	return 0;
}



static int testRoundTrip(void)
{
	const unsigned char sInput[] = { 'x', 'l', 'a', 'n', 'g', 0, 0xFF, 0x80 };
	xbuffer* pPacked = NULL;
	xbuffer* pPlain = NULL;
	xrtgziperror tError;
	int bOk = 1;
	bOk &= check(xrtGzipCompress(sInput, sizeof(sInput), NULL, &pPacked, &tError), tError.sMessage);
	bOk &= check(pPacked != NULL && pPacked->Size > 18u &&
		pPacked->Data[0] == 0x1Fu && pPacked->Data[1] == 0x8Bu,
		"invalid gzip header");
	bOk &= check(pPacked->Data[4] == 0u && pPacked->Data[5] == 0u &&
		pPacked->Data[6] == 0u && pPacked->Data[7] == 0u,
		"default gzip mtime must be deterministic");
	bOk &= check(xrtGzipDecompress(pPacked->Data, pPacked->Size, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && pPlain->Size == sizeof(sInput) &&
		memcmp(pPlain->Data, sInput, sizeof(sInput)) == 0, "gzip round trip mismatch");
	xrtBufferDestroy(pPlain);

	// CRC 损坏必须作为数据错误返回，不能产生部分成功结果。
	pPacked->Data[pPacked->Size - 8u] ^= 1;
	pPlain = NULL;
	bOk &= check(!xrtGzipDecompress(pPacked->Data, pPacked->Size, NULL, &pPlain, &tError) &&
		pPlain == NULL && tError.iCode == XRT_GZIP_ERROR_DATA, "corrupt CRC was accepted");
	pPacked->Data[pPacked->Size - 8u] ^= 1;

	pPlain = NULL;
	{
		bool bRejected = !xrtGzipDecompress(pPacked->Data, pPacked->Size - 1u,
			NULL, &pPlain, &tError);

		bOk &= check(bRejected && pPlain == NULL, "truncated gzip was accepted");
		bOk &= check(!bRejected || tError.iCode == XRT_GZIP_ERROR_TRUNCATED,
			tError.sMessage);
	}
	xrtBufferDestroy(pPacked);
	return bOk;
}



static int testConcatenatedAndLimits(void)
{
	xbuffer* pFirst = NULL;
	xbuffer* pSecond = NULL;
	xbuffer* pBoth = xrtBufferCreate();
	xbuffer* pTruncated = xrtBufferCreate();
	xbuffer* pPlain = NULL;
	xrtgzipoptions tOptions;
	xrtgziperror tError;
	int bOk = 1;
	bOk &= check(xrtGzipCompress("first", 5u, NULL, &pFirst, &tError), tError.sMessage);
	bOk &= check(xrtGzipCompress("second", 6u, NULL, &pSecond, &tError), tError.sMessage);
	bOk &= check(pBoth != NULL && pTruncated != NULL &&
		xrtBufferAppend(pBoth, xrtBufferView(pFirst)) &&
		xrtBufferAppend(pBoth, xrtBufferView(pSecond)), "concat buffer failed");
	bOk &= check(xrtGzipDecompress(pBoth->Data, pBoth->Size, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && pPlain->Size == 11u && memcmp(pPlain->Data, "firstsecond", 11u) == 0,
		"concatenated gzip member mismatch");
	xrtBufferDestroy(pPlain);

	/* 完整首成员不得掩盖后续成员截断。 */
	bOk &= check(xrtBufferAppend(pTruncated, xrtBufferView(pFirst)) &&
		xrtBufferAppend(pTruncated, (xbytesview){ pSecond->Data, pSecond->Size - 1u }),
		"truncated concat buffer failed");
	pPlain = NULL;
	{
		bool bRejected = !xrtGzipDecompress(pTruncated->Data, pTruncated->Size,
			NULL, &pPlain, &tError);

		bOk &= check(bRejected && pPlain == NULL,
			"truncated final gzip member was accepted");
		bOk &= check(!bRejected || tError.iCode == XRT_GZIP_ERROR_TRUNCATED,
			tError.sMessage);
	}

	xrtGzipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 10u;
	pPlain = NULL;
	bOk &= check(!xrtGzipDecompress(pBoth->Data, pBoth->Size, &tOptions, &pPlain, &tError) &&
		tError.iCode == XRT_GZIP_ERROR_LIMIT, "output limit was not enforced");
	xrtBufferDestroy(pFirst);
	xrtBufferDestroy(pSecond);
	xrtBufferDestroy(pBoth);
	xrtBufferDestroy(pTruncated);
	return bOk;
}



static int testIncrementalAndStreams(void)
{
	const char* sText = "incremental gzip stream";
	size_t i;
	xrtgzipcompressor* pCompressor = xrtGzipCompressorCreate(NULL);
	xrtgzipdecompressor* pDecompressor;
	xbuffer* pPacked;
	xbuffer* pPlain;
	xbuffer* pStreamPacked = xrtBufferCreate();
	xbuffer* pStreamPlain = xrtBufferCreate();
	xreader* pReader;
	xwriter* pWriter;
	xrtgziperror tError;
	int bOk = 1;
	for ( i = 0; i < strlen(sText); ++i ) {
		bOk &= check(xrtGzipCompressorUpdate(pCompressor, sText + i, 1u), "incremental compress update failed");
	}
	bOk &= check(xrtGzipCompressorFinish(pCompressor), "incremental compress finish failed");
	pPacked = xrtGzipCompressorTakeBuffer(pCompressor);
	xrtGzipCompressorDestroy(pCompressor);

	pDecompressor = xrtGzipDecompressorCreate(NULL);
	for ( i = 0; i < pPacked->Size; ++i ) {
		bOk &= check(xrtGzipDecompressorUpdate(pDecompressor, pPacked->Data + i, 1u),
			"incremental decompress update failed");
	}
	bOk &= check(xrtGzipDecompressorFinish(pDecompressor), "incremental decompress finish failed");
	pPlain = xrtGzipDecompressorTakeBuffer(pDecompressor);
	xrtGzipDecompressorDestroy(pDecompressor);
	bOk &= check(pPlain != NULL && pPlain->Size == strlen(sText) &&
		memcmp(pPlain->Data, sText, strlen(sText)) == 0, "incremental gzip mismatch");

	pReader = xrtReaderFromMemory((xbytesview){ (const uint8*)sText, strlen(sText) });
	pWriter = xrtWriterFromBuffer(pStreamPacked);
	bOk &= check(xrtGzipCompressStream(pReader, pWriter, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	pReader = xrtReaderFromBuffer(pStreamPacked);
	pWriter = xrtWriterFromBuffer(pStreamPlain);
	bOk &= check(xrtGzipDecompressStream(pReader, pWriter, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	bOk &= check(pStreamPlain->Size == strlen(sText) &&
		memcmp(pStreamPlain->Data, sText, strlen(sText)) == 0, "stream gzip mismatch");
	xrtBufferDestroy(pPacked);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pStreamPacked);
	xrtBufferDestroy(pStreamPlain);
	return bOk;
}



int main(void)
{
	int bOk;
	bOk = testRoundTrip();
	bOk &= testConcatenatedAndLimits();
	bOk &= testIncrementalAndStreams();
	if ( bOk ) { puts("test_gzip: PASS"); }
	return bOk ? 0 : 1;
}
