#define XRT_MODULE_IO_BUFFER

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
	xbuffer* pPacked = NULL;
	xbuffer* pPlain = NULL;
	xrtbz2error tError;
	xbytesview Packed;
	int bOk = 1;
	bOk &= check(xrtBz2Compress(sInput, sizeof(sInput), NULL, &pPacked, &tError), tError.sMessage);
	Packed = xrtBufferView(pPacked);
	bOk &= check(Packed.Size > 10u && memcmp(Packed.Data, "BZh9", 4u) == 0, "invalid bz2 header");
	bOk &= check(xrtBz2Decompress(Packed.Data, Packed.Size, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && xrtBufferView(pPlain).Size == sizeof(sInput) &&
		memcmp(xrtBufferView(pPlain).Data, sInput, sizeof(sInput)) == 0, "bz2 round trip mismatch");
	xrtBufferDestroy(pPlain);

	/* BZh + 六字节块标记之后是块 CRC，修改它必须触发完整性错误。 */
	pPacked->Data[10u] ^= 1;
	pPlain = NULL;
	bOk &= check(!xrtBz2Decompress(pPacked->Data, pPacked->Size, NULL, &pPlain, &tError) &&
		pPlain == NULL && tError.iCode == XRT_BZ2_ERROR_DATA, "corrupt bz2 stream was accepted");
	pPacked->Data[10u] ^= 1;

	pPlain = NULL;
	bOk &= check(!xrtBz2Decompress(pPacked->Data, pPacked->Size - 1u, NULL, &pPlain, &tError) &&
		tError.iCode == XRT_BZ2_ERROR_TRUNCATED, "truncated bz2 stream was accepted");
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
	xrtbz2options tOptions;
	xrtbz2error tError;
	int bOk = 1;
	bOk &= check(xrtBz2Compress("first", 5u, NULL, &pFirst, &tError), tError.sMessage);
	bOk &= check(xrtBz2Compress("second", 6u, NULL, &pSecond, &tError), tError.sMessage);
	bOk &= check(pBoth != NULL && pTruncated != NULL &&
		xrtBufferAppend(pBoth, xrtBufferView(pFirst)) &&
		xrtBufferAppend(pBoth, xrtBufferView(pSecond)), "concat buffer failed");
	bOk &= check(xrtBz2Decompress(pBoth->Data, pBoth->Size, NULL, &pPlain, &tError), tError.sMessage);
	bOk &= check(pPlain != NULL && pPlain->Size == 11u &&
		memcmp(pPlain->Data, "firstsecond", 11u) == 0, "concatenated bz2 member mismatch");
	xrtBufferDestroy(pPlain);

	/* 完整首成员不得掩盖后续成员截断。 */
	bOk &= check(xrtBufferAppend(pTruncated, xrtBufferView(pFirst)) &&
		xrtBufferAppend(pTruncated, (xbytesview){ pSecond->Data, pSecond->Size - 1u }),
		"truncated concat buffer failed");
	pPlain = NULL;
	bOk &= check(!xrtBz2Decompress(pTruncated->Data, pTruncated->Size, NULL, &pPlain, &tError) &&
		pPlain == NULL && tError.iCode == XRT_BZ2_ERROR_TRUNCATED,
		"truncated final bz2 member was accepted");

	xrtBz2OptionsInit(&tOptions);
	tOptions.tLimits.iMaxOutputBytes = 10u;
	pPlain = NULL;
	bOk &= check(!xrtBz2Decompress(pBoth->Data, pBoth->Size, &tOptions, &pPlain, &tError) &&
		tError.iCode == XRT_BZ2_ERROR_LIMIT, "output limit was not enforced");
	xrtBufferDestroy(pFirst);
	xrtBufferDestroy(pSecond);
	xrtBufferDestroy(pBoth);
	xrtBufferDestroy(pTruncated);
	return bOk;
}



static int testIncrementalAndStreams(void)
{
	const char* sText = "incremental bz2 stream data";
	xrtbz2compressor* pCompressor = xrtBz2CompressorCreate(NULL);
	xrtbz2decompressor* pDecompressor;
	xbuffer* pPacked;
	xbuffer* pPlain;
	xbuffer* pStreamPacked = xrtBufferCreate();
	xbuffer* pStreamPlain = xrtBufferCreate();
	xreader* pReader;
	xwriter* pWriter;
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
	for ( i = 0u; i < pPacked->Size; ++i ) {
		bOk &= check(xrtBz2DecompressorUpdate(pDecompressor, pPacked->Data + i, 1u),
			"incremental decompress update failed");
	}
	bOk &= check(xrtBz2DecompressorFinish(pDecompressor), "incremental decompress finish failed");
	pPlain = xrtBz2DecompressorTakeBuffer(pDecompressor);
	xrtBz2DecompressorDestroy(pDecompressor);
	bOk &= check(pPlain != NULL && pPlain->Size == strlen(sText) &&
		memcmp(pPlain->Data, sText, strlen(sText)) == 0, "incremental bz2 mismatch");

	pReader = xrtReaderFromMemory((xbytesview){ (const uint8*)sText, strlen(sText) });
	pWriter = xrtWriterFromBuffer(pStreamPacked);
	bOk &= check(xrtBz2CompressStream(pReader, pWriter, NULL, NULL, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	pReader = xrtReaderFromBuffer(pStreamPacked);
	pWriter = xrtWriterFromBuffer(pStreamPlain);
	bOk &= check(xrtBz2DecompressStream(pReader, pWriter, NULL, NULL, NULL, &tError), tError.sMessage);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	bOk &= check(pStreamPlain->Size == strlen(sText) &&
		memcmp(pStreamPlain->Data, sText, strlen(sText)) == 0, "stream bz2 mismatch");
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
	if ( bOk ) { puts("test_bz2: PASS"); }
	return bOk ? 0 : 1;
}
