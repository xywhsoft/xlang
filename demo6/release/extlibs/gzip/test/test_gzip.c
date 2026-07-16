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
	xbuffer pPacked = NULL;
	xbuffer pPlain = NULL;
	xrtgziperror tError;
	int bOk = 1;
	bOk &= check(xrtGzipCompress(sInput, sizeof(sInput), NULL, &pPacked, &tError), tError.sMessage);
	bOk &= check(pPacked != NULL && pPacked->Length > 18u &&
		(unsigned char)pPacked->Buffer[0] == 0x1Fu && (unsigned char)pPacked->Buffer[1] == 0x8Bu,
		"invalid gzip header");
	bOk &= check((unsigned char)pPacked->Buffer[4] == 0u && (unsigned char)pPacked->Buffer[5] == 0u &&
		(unsigned char)pPacked->Buffer[6] == 0u && (unsigned char)pPacked->Buffer[7] == 0u,
		"default gzip mtime must be deterministic");
	bOk &= check(xrtGzipDecompress(pPacked->Buffer, pPacked->Length, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && pPlain->Length == sizeof(sInput) &&
		memcmp(pPlain->Buffer, sInput, sizeof(sInput)) == 0, "gzip round trip mismatch");
	xrtBufferDestroy(pPlain);

	// CRC 损坏必须作为数据错误返回，不能产生部分成功结果。
	pPacked->Buffer[pPacked->Length - 8u] ^= 1;
	pPlain = NULL;
	bOk &= check(!xrtGzipDecompress(pPacked->Buffer, pPacked->Length, NULL, &pPlain, &tError) &&
		pPlain == NULL && tError.iCode == XRT_GZIP_ERROR_DATA, "corrupt CRC was accepted");
	pPacked->Buffer[pPacked->Length - 8u] ^= 1;

	pPlain = NULL;
	bOk &= check(!xrtGzipDecompress(pPacked->Buffer, pPacked->Length - 1u, NULL, &pPlain, &tError) &&
		tError.iCode == XRT_GZIP_ERROR_TRUNCATED, "truncated gzip was accepted");
	xrtBufferDestroy(pPacked);
	return bOk;
}



static int testConcatenatedAndLimits(void)
{
	xbuffer pFirst = NULL;
	xbuffer pSecond = NULL;
	xbuffer pBoth = xrtBufferCreate(64u);
	xbuffer pPlain = NULL;
	xrtgzipoptions tOptions;
	xrtgziperror tError;
	int bOk = 1;
	bOk &= check(xrtGzipCompress("first", 5u, NULL, &pFirst, &tError), tError.sMessage);
	bOk &= check(xrtGzipCompress("second", 6u, NULL, &pSecond, &tError), tError.sMessage);
	bOk &= check(xrtBufferAppend(pBoth, pFirst->Buffer, pFirst->Length, XBUF_BINARY) &&
		xrtBufferAppend(pBoth, pSecond->Buffer, pSecond->Length, XBUF_BINARY), "concat buffer failed");
	bOk &= check(xrtGzipDecompress(pBoth->Buffer, pBoth->Length, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && pPlain->Length == 11u && memcmp(pPlain->Buffer, "firstsecond", 11u) == 0,
		"concatenated gzip member mismatch");
	xrtBufferDestroy(pPlain);

	xrtGzipOptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 10u;
	pPlain = NULL;
	bOk &= check(!xrtGzipDecompress(pBoth->Buffer, pBoth->Length, &tOptions, &pPlain, &tError) &&
		tError.iCode == XRT_GZIP_ERROR_LIMIT, "output limit was not enforced");
	xrtBufferDestroy(pFirst);
	xrtBufferDestroy(pSecond);
	xrtBufferDestroy(pBoth);
	return bOk;
}



static int testIncrementalAndStreams(void)
{
	const char* sText = "incremental gzip stream";
	size_t i;
	xrtgzipcompressor* pCompressor = xrtGzipCompressorCreate(NULL);
	xrtgzipdecompressor* pDecompressor;
	xbuffer pPacked;
	xbuffer pPlain;
	xbuffer pStreamPacked = xrtBufferCreate(64u);
	xbuffer pStreamPlain = xrtBufferCreate(64u);
	xreader pReader;
	xwriter pWriter;
	xrtgziperror tError;
	int bOk = 1;
	for ( i = 0; i < strlen(sText); ++i ) {
		bOk &= check(xrtGzipCompressorUpdate(pCompressor, sText + i, 1u), "incremental compress update failed");
	}
	bOk &= check(xrtGzipCompressorFinish(pCompressor), "incremental compress finish failed");
	pPacked = xrtGzipCompressorTakeBuffer(pCompressor);
	xrtGzipCompressorDestroy(pCompressor);

	pDecompressor = xrtGzipDecompressorCreate(NULL);
	for ( i = 0; i < pPacked->Length; ++i ) {
		bOk &= check(xrtGzipDecompressorUpdate(pDecompressor, pPacked->Buffer + i, 1u),
			"incremental decompress update failed");
	}
	bOk &= check(xrtGzipDecompressorFinish(pDecompressor), "incremental decompress finish failed");
	pPlain = xrtGzipDecompressorTakeBuffer(pDecompressor);
	xrtGzipDecompressorDestroy(pDecompressor);
	bOk &= check(pPlain != NULL && pPlain->Length == strlen(sText) &&
		memcmp(pPlain->Buffer, sText, strlen(sText)) == 0, "incremental gzip mismatch");

	pReader = xrtReaderFromMemory(sText, strlen(sText));
	pWriter = xrtWriterFromBuffer(pStreamPacked, XRT_STREAM_BORROW);
	bOk &= check(xrtGzipCompressStream(pReader, pWriter, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	pReader = xrtReaderFromBuffer(pStreamPacked, XRT_STREAM_BORROW);
	pWriter = xrtWriterFromBuffer(pStreamPlain, XRT_STREAM_BORROW);
	bOk &= check(xrtGzipDecompressStream(pReader, pWriter, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	bOk &= check(pStreamPlain->Length == strlen(sText) &&
		memcmp(pStreamPlain->Buffer, sText, strlen(sText)) == 0, "stream gzip mismatch");
	xrtBufferDestroy(pPacked);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pStreamPacked);
	xrtBufferDestroy(pStreamPlain);
	return bOk;
}



int main(void)
{
	int bOk;
	if ( xrtInit() == NULL ) { return 1; }
	bOk = testRoundTrip();
	bOk &= testConcatenatedAndLimits();
	bOk &= testIncrementalAndStreams();
	xrtUnit();
	if ( bOk ) { puts("test_gzip: PASS"); }
	return bOk ? 0 : 1;
}
