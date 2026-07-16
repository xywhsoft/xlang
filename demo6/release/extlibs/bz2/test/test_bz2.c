#include "../include/xrt_bz2.h"

#include <stdio.h>
#include <string.h>



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_bz2: %s\n", sMessage != NULL && sMessage[0] != 0 ? sMessage : "check failed");
	return 0;
}



static int testRoundTrip(void)
{
	const unsigned char sInput[] = { 'x', 'l', 'a', 'n', 'g', 0, 0xFF, 0x80 };
	xbuffer pPacked = NULL;
	xbuffer pPlain = NULL;
	xrtbz2error tError;
	int bOk = 1;
	bOk &= check(xrtBz2Compress(sInput, sizeof(sInput), NULL, &pPacked, &tError), tError.sMessage);
	bOk &= check(pPacked != NULL && pPacked->Length > 10u &&
		memcmp(pPacked->Buffer, "BZh9", 4u) == 0, "invalid bz2 header");
	bOk &= check(xrtBz2Decompress(pPacked->Buffer, pPacked->Length, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && pPlain->Length == sizeof(sInput) &&
		memcmp(pPlain->Buffer, sInput, sizeof(sInput)) == 0, "bz2 round trip mismatch");
	xrtBufferDestroy(pPlain);

	/* BZh + 六字节块标记之后是块 CRC，修改它必须触发完整性错误。 */
	pPacked->Buffer[10u] ^= 1;
	pPlain = NULL;
	bOk &= check(!xrtBz2Decompress(pPacked->Buffer, pPacked->Length, NULL, &pPlain, &tError) &&
		pPlain == NULL && tError.iCode == XRT_BZ2_ERROR_DATA, "corrupt bz2 stream was accepted");
	pPacked->Buffer[10u] ^= 1;

	pPlain = NULL;
	bOk &= check(!xrtBz2Decompress(pPacked->Buffer, pPacked->Length - 1u, NULL, &pPlain, &tError) &&
		tError.iCode == XRT_BZ2_ERROR_TRUNCATED, "truncated bz2 stream was accepted");
	xrtBufferDestroy(pPacked);
	return bOk;
}



static int testConcatenatedAndLimits(void)
{
	xbuffer pFirst = NULL;
	xbuffer pSecond = NULL;
	xbuffer pBoth = xrtBufferCreate(64u);
	xbuffer pPlain = NULL;
	xrtbz2options tOptions;
	xrtbz2error tError;
	int bOk = 1;
	bOk &= check(xrtBz2Compress("first", 5u, NULL, &pFirst, &tError), tError.sMessage);
	bOk &= check(xrtBz2Compress("second", 6u, NULL, &pSecond, &tError), tError.sMessage);
	bOk &= check(xrtBufferAppend(pBoth, pFirst->Buffer, pFirst->Length, XBUF_BINARY) &&
		xrtBufferAppend(pBoth, pSecond->Buffer, pSecond->Length, XBUF_BINARY), "concat buffer failed");
	bOk &= check(xrtBz2Decompress(pBoth->Buffer, pBoth->Length, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && pPlain->Length == 11u &&
		memcmp(pPlain->Buffer, "firstsecond", 11u) == 0, "concatenated bz2 member mismatch");
	xrtBufferDestroy(pPlain);

	xrtBz2OptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 10u;
	pPlain = NULL;
	bOk &= check(!xrtBz2Decompress(pBoth->Buffer, pBoth->Length, &tOptions, &pPlain, &tError) &&
		tError.iCode == XRT_BZ2_ERROR_LIMIT, "output limit was not enforced");
	xrtBufferDestroy(pFirst);
	xrtBufferDestroy(pSecond);
	xrtBufferDestroy(pBoth);
	return bOk;
}



static int testIncrementalAndStreams(void)
{
	const char* sText = "incremental bz2 stream data";
	xrtbz2compressor* pCompressor = xrtBz2CompressorCreate(NULL);
	xrtbz2decompressor* pDecompressor;
	xbuffer pPacked;
	xbuffer pPlain;
	xbuffer pStreamPacked = xrtBufferCreate(64u);
	xbuffer pStreamPlain = xrtBufferCreate(64u);
	xreader pReader;
	xwriter pWriter;
	xrtbz2error tError;
	size_t i;
	int bOk = 1;
	bOk &= check(pCompressor != NULL, "cannot create incremental compressor");
	for ( i = 0u; i < strlen(sText); ++i ) {
		bOk &= check(xrtBz2CompressorUpdate(pCompressor, sText + i, 1u),
			"incremental compress update failed");
	}
	bOk &= check(xrtBz2CompressorFinish(pCompressor), "incremental compress finish failed");
	pPacked = xrtBz2CompressorTakeBuffer(pCompressor);
	xrtBz2CompressorDestroy(pCompressor);

	pDecompressor = xrtBz2DecompressorCreate(NULL);
	for ( i = 0u; i < pPacked->Length; ++i ) {
		bOk &= check(xrtBz2DecompressorUpdate(pDecompressor, pPacked->Buffer + i, 1u),
			"incremental decompress update failed");
	}
	bOk &= check(xrtBz2DecompressorFinish(pDecompressor), "incremental decompress finish failed");
	pPlain = xrtBz2DecompressorTakeBuffer(pDecompressor);
	xrtBz2DecompressorDestroy(pDecompressor);
	bOk &= check(pPlain != NULL && pPlain->Length == strlen(sText) &&
		memcmp(pPlain->Buffer, sText, strlen(sText)) == 0, "incremental bz2 mismatch");

	pReader = xrtReaderFromMemory(sText, strlen(sText));
	pWriter = xrtWriterFromBuffer(pStreamPacked, XRT_STREAM_BORROW);
	bOk &= check(xrtBz2CompressStream(pReader, pWriter, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	pReader = xrtReaderFromBuffer(pStreamPacked, XRT_STREAM_BORROW);
	pWriter = xrtWriterFromBuffer(pStreamPlain, XRT_STREAM_BORROW);
	bOk &= check(xrtBz2DecompressStream(pReader, pWriter, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	bOk &= check(pStreamPlain->Length == strlen(sText) &&
		memcmp(pStreamPlain->Buffer, sText, strlen(sText)) == 0, "stream bz2 mismatch");
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
	if ( bOk ) { puts("test_bz2: PASS"); }
	return bOk ? 0 : 1;
}
