#ifdef SNAPPY_MEMORY_GUARD
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "xrt_snappy.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

/* 独立于编码器的官方 framing 样本：abc 的 masked CRC32C 为 0x21f1576e。 */
static const unsigned char kAbcFrame[] = {
	0xff, 0x06, 0x00, 0x00, 's', 'N', 'a', 'P', 'p', 'Y',
	0x01, 0x07, 0x00, 0x00, 0x6e, 0x57, 0xf1, 0x21, 'a', 'b', 'c'
};
static const unsigned char kAbcCompressedFrame[] = {
	0xff, 0x06, 0x00, 0x00, 's', 'N', 'a', 'P', 'p', 'Y',
	0x00, 0x09, 0x00, 0x00, 0x6e, 0x57, 0xf1, 0x21,
	0x03, 0x08, 'a', 'b', 'c'
};

typedef struct {
	unsigned calls;
	uint64 input;
	uint64 total;
	bool known;
} progress_state;

static bool recordProgress(const xrtprogress* pProgress, ptr pUserData)
{
	progress_state* pState = (progress_state*)pUserData;
	assert(pProgress->iSize == sizeof(*pProgress) &&
		pProgress->iVersion == XRT_PROGRESS_VERSION);
	pState->calls++;
	pState->input = pProgress->iInputBytes;
	pState->total = pProgress->iTotalInputBytes;
	pState->known = (pProgress->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0u;
	return !pState->known || pState->input <= pState->total;
}

typedef struct {
	const uint8* data;
	size_t size;
	size_t offset;
} unknown_reader;

static bool readUnknown(ptr pContext, ptr pOutput, size_t iRequest, size_t* pRead)
{
	unknown_reader* pSource = (unknown_reader*)pContext;
	size_t iCount = pSource->size - pSource->offset;
	if ( iCount > iRequest ) { iCount = iRequest; }
	if ( iCount != 0u ) {
		memcpy(pOutput, pSource->data + pSource->offset, iCount);
		pSource->offset += iCount;
	}
	*pRead = iCount;
	return true;
}

/* 进度总量按本次读取起点计数，不包含底层 Reader 的前缀。 */
static void testSeekedStreamProgress(void)
{
	static const unsigned char sInput[] = "XXXXXXXXXXabc";
	const uint64 iPrefix = 10u;
	xrtsnappyerror Error;
	xreader* pReader;
	xwriter* pWriter;
	xbuffer* pPacked = xrtBufferCreate();
	xbuffer* pWrapped = xrtBufferCreate();
	xbuffer* pPlain = NULL;
	progress_state Progress = {0};
	uint64 iPosition = 0u;
	assert(pPacked && pWrapped);
	pReader = xrtReaderFromMemory((xbytesview){sInput, sizeof(sInput) - 1u});
	pWriter = xrtWriterFromBuffer(pPacked);
	assert(pReader && pWriter && xrtReaderSeek(pReader, (int64)iPrefix,
		XSEEK_START, &iPosition) && iPosition == iPrefix);
	assert(xrtSnappyCompressStream(pReader, pWriter, NULL,
		recordProgress, &Progress, &Error));
	assert(Progress.calls > 0u && Progress.known &&
		Progress.input == 3u && Progress.total == 3u);
	assert(xrtReaderDestroy(pReader) && xrtWriterDestroy(pWriter));
	assert(xrtSnappyDecompress(pPacked->Data, pPacked->Size,
		NULL, &pPlain, &Error) && pPlain->Size == 3u &&
		memcmp(pPlain->Data, "abc", 3u) == 0);
	xrtBufferDestroy(pPlain);
	pPlain = xrtBufferCreate();
	assert(pPlain != NULL);

	assert(xrtBufferAppend(pWrapped, XRT_BYTES_LITERAL("XXXXXXXXXX")) &&
		xrtBufferAppend(pWrapped, (xbytesview){
			kAbcFrame, sizeof(kAbcFrame) }));
	pReader = xrtReaderFromMemory(xrtBufferView(pWrapped));
	pWriter = xrtWriterFromBuffer(pPlain);
	iPosition = 0u;
	Progress = (progress_state){0};
	assert(pReader && pWriter && xrtReaderSeek(pReader, (int64)iPrefix,
		XSEEK_START, &iPosition) && iPosition == iPrefix);
	assert(xrtSnappyDecompressStream(pReader, pWriter, NULL,
		recordProgress, &Progress, &Error));
	assert(Progress.calls > 0u && Progress.known &&
		Progress.input == sizeof(kAbcFrame) &&
		Progress.total == sizeof(kAbcFrame) &&
		pPlain->Size == 3u && memcmp(pPlain->Data, "abc", 3u) == 0);
	assert(xrtReaderDestroy(pReader) && xrtWriterDestroy(pWriter));
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pWrapped);
	xrtBufferDestroy(pPacked);

	/* 不提供长度与位置的流仍可压缩，只是不声明已知总量。 */
	{
		static const xreaderops Ops = { readUnknown, NULL, NULL, NULL, NULL };
		unknown_reader Source = { (const uint8*)"abc", 3u, 0u };
		xbuffer* pOutput = xrtBufferCreate();
		assert(pOutput != NULL);
		pReader = xrtReaderCreate(&Ops, &Source);
		pWriter = xrtWriterFromBuffer(pOutput);
		Progress = (progress_state){0};
		assert(pReader && pWriter && xrtSnappyCompressStream(pReader, pWriter,
			NULL, recordProgress, &Progress, &Error));
		assert(Progress.calls > 0u && Progress.input == 3u &&
			!Progress.known && Progress.total == 0u && xrtGetError() == NULL);
		assert(xrtReaderDestroy(pReader) && xrtWriterDestroy(pWriter));
		xrtBufferDestroy(pOutput);
	}
}

static void testGoldenFrame(void)
{
	xbuffer* pPacked = NULL;
	xbuffer* pPlain = NULL;
	xrtsnappyerror Error;
	xbytesview View;
	assert(xrtSnappyCompress("abc", 3u, NULL, &pPacked, &Error));
	View = xrtBufferView(pPacked);
	assert(View.Size == sizeof(kAbcFrame));
	assert(memcmp(View.Data, kAbcFrame, sizeof(kAbcFrame)) == 0);
	assert(xrtSnappyDecompress(kAbcFrame, sizeof(kAbcFrame), NULL, &pPlain, &Error));
	View = xrtBufferView(pPlain);
	assert(View.Size == 3u && memcmp(View.Data, "abc", 3u) == 0);
	xrtBufferDestroy(pPlain);
	pPlain = NULL;
	assert(xrtSnappyDecompress(kAbcCompressedFrame, sizeof(kAbcCompressedFrame),
		NULL, &pPlain, &Error));
	View = xrtBufferView(pPlain);
	assert(View.Size == 3u && memcmp(View.Data, "abc", 3u) == 0);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pPacked);
}

/* 一次写入大输入时，编码器只应缓存当前 64 KiB 数据块。 */
static void testLargeUpdate(void)
{
	static unsigned char sData[4u * 1024u * 1024u];
	xrtsnappycompressor* pEncoder;
	xrtsnappyerror Error;
	xbuffer* pPacked;
	xbuffer* pPlain = NULL;
#ifdef SNAPPY_MEMORY_GUARD
	xmemdebugsnapshot Snapshot;
	assert(xrtMemDebugReset());
#endif
	memset(sData, 'A', sizeof(sData));
	pEncoder = xrtSnappyCompressorCreate(NULL);
	assert(pEncoder != NULL);
	assert(xrtSnappyCompressorUpdate(pEncoder, sData, sizeof(sData)));
#ifdef SNAPPY_MEMORY_GUARD
	xrtMemDebugSnapshot(&Snapshot);
	assert(Snapshot.Enabled && Snapshot.PeakBytes < 2u * 1024u * 1024u);
#endif
	assert(xrtSnappyCompressorFinish(pEncoder));
	pPacked = xrtSnappyCompressorTakeBuffer(pEncoder);
	assert(pPacked != NULL);
	xrtSnappyCompressorDestroy(pEncoder);
	assert(xrtSnappyDecompress(pPacked->Data, pPacked->Size,
		NULL, &pPlain, &Error));
	assert(pPlain->Size == sizeof(sData));
	assert(memcmp(pPlain->Data, sData, sizeof(sData)) == 0);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pPacked);
}

/* 可跳过 chunk 不产出原文，用来单独约束解压输入缓冲的峰值。 */
static void testBoundedSkippableFrame(const unsigned char* pFrame, size_t iSize)
{
	xrtsnappydecompressor* pDecoder;
	xbuffer* pPlain;
#ifdef SNAPPY_MEMORY_GUARD
	xmemdebugsnapshot Snapshot;
	assert(xrtMemDebugReset());
#endif
	pDecoder = xrtSnappyDecompressorCreate(NULL);
	assert(pDecoder != NULL);
	assert(xrtSnappyDecompressorUpdate(pDecoder, pFrame, iSize));
#ifdef SNAPPY_MEMORY_GUARD
	xrtMemDebugSnapshot(&Snapshot);
	assert(Snapshot.Enabled && Snapshot.PeakBytes < 2u * 1024u * 1024u);
#endif
	assert(xrtSnappyDecompressorFinish(pDecoder));
	assert(xrtSnappyDecompressorMembers(pDecoder) == 1u);
	pPlain = xrtSnappyDecompressorTakeBuffer(pDecoder);
	assert(pPlain != NULL && pPlain->Size == 0u);
	xrtBufferDestroy(pPlain);
	xrtSnappyDecompressorDestroy(pDecoder);
}

static void testLargeSkippableUpdate(void)
{
	static unsigned char sFrame[10u + 64u * (4u + 65536u)];
	size_t i;
	memcpy(sFrame, kAbcFrame, 10u);
	for ( i = 0u; i < 64u; ++i ) {
		size_t iOffset = 10u + i * (4u + 65536u);
		sFrame[iOffset] = 0x80u;
		sFrame[iOffset + 1u] = 0u;
		sFrame[iOffset + 2u] = 0u;
		sFrame[iOffset + 3u] = 1u;
	}
	testBoundedSkippableFrame(sFrame, sizeof(sFrame));
	sFrame[10] = 0x80u;
	sFrame[11] = 0u;
	sFrame[12] = 0u;
	sFrame[13] = 0x40u;
	testBoundedSkippableFrame(sFrame, 10u + 4u + 4u * 1024u * 1024u);
}

static void testStreamAndLimits(void)
{
	unsigned char sData[65536u + 13u];
	xbuffer* pPacked = NULL;
	xbuffer* pPlain = NULL;
	xrtsnappyerror Error;
	xrtsnappyoptions Options;
	xrtsnappydecompressor* pDecoder;
	xbytesview View;
	size_t i;
	memset(sData, 'A', sizeof(sData));
	assert(xrtSnappyCompress(sData, sizeof(sData), NULL, &pPacked, &Error));
	View = xrtBufferView(pPacked);
	assert(View.Size < sizeof(sData));
	assert(View.Data[10] == 0x00u);
	pDecoder = xrtSnappyDecompressorCreate(NULL);
	assert(pDecoder != NULL);
	for ( i = 0u; i < View.Size; ++i ) {
		assert(xrtSnappyDecompressorUpdate(pDecoder, View.Data + i, 1u));
	}
	assert(xrtSnappyDecompressorFinish(pDecoder));
	assert(xrtSnappyDecompressorFinished(pDecoder));
	assert(xrtSnappyDecompressorMembers(pDecoder) == 1u);
	pPlain = xrtSnappyDecompressorTakeBuffer(pDecoder);
	assert(pPlain != NULL);
	assert(xrtBufferView(pPlain).Size == sizeof(sData));
	assert(memcmp(xrtBufferView(pPlain).Data, sData, sizeof(sData)) == 0);
	xrtBufferDestroy(pPlain);
	xrtSnappyDecompressorDestroy(pDecoder);

	xrtSnappyOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 100u;
	assert(!xrtSnappyDecompress(View.Data, View.Size, &Options, &pPlain, &Error));
	assert(Error.iCode == XRT_SNAPPY_ERROR_LIMIT);
	assert(pPlain == NULL);
	xrtBufferDestroy(pPacked);
}

static void testInvalidAndSkippable(void)
{
	unsigned char sFrame[sizeof(kAbcFrame)];
	unsigned char sWithPadding[sizeof(kAbcFrame) + 7u];
	unsigned char sJoined[sizeof(kAbcFrame) * 2u];
	xbuffer* pOutput = NULL;
	xrtsnappyerror Error;
	xrtsnappydecompressor* pDecoder;

	memcpy(sFrame, kAbcFrame, sizeof(sFrame));
	sFrame[14] ^= 1u;
	assert(!xrtSnappyDecompress(sFrame, sizeof(sFrame), NULL, &pOutput, &Error));
	assert(Error.iCode == XRT_SNAPPY_ERROR_DATA);
	assert(!xrtSnappyDecompress(kAbcFrame, sizeof(kAbcFrame) - 1u,
		NULL, &pOutput, &Error));
	assert(Error.iCode == XRT_SNAPPY_ERROR_TRUNCATED);
	assert(!xrtSnappyDecompress("abc", 3u, NULL, &pOutput, &Error));

	memcpy(sWithPadding, kAbcFrame, 10u);
	sWithPadding[10] = 0xfeu;
	sWithPadding[11] = 3u;
	sWithPadding[12] = sWithPadding[13] = 0u;
	memcpy(sWithPadding + 14u, "xyz", 3u);
	memcpy(sWithPadding + 17u, kAbcFrame + 10u, sizeof(kAbcFrame) - 10u);
	assert(xrtSnappyDecompress(sWithPadding, sizeof(sWithPadding),
		NULL, &pOutput, &Error));
	assert(xrtBufferView(pOutput).Size == 3u);
	xrtBufferDestroy(pOutput);
	pOutput = NULL;
	pDecoder = xrtSnappyDecompressorCreate(NULL);
	assert(pDecoder != NULL);
	for ( size_t i = 0u; i < sizeof(sWithPadding); ++i ) {
		assert(xrtSnappyDecompressorUpdate(pDecoder, sWithPadding + i, 1u));
	}
	assert(xrtSnappyDecompressorFinish(pDecoder));
	pOutput = xrtSnappyDecompressorTakeBuffer(pDecoder);
	assert(pOutput != NULL && pOutput->Size == 3u &&
		memcmp(pOutput->Data, "abc", 3u) == 0);
	xrtBufferDestroy(pOutput);
	xrtSnappyDecompressorDestroy(pDecoder);
	pOutput = NULL;
	pDecoder = xrtSnappyDecompressorCreate(NULL);
	assert(pDecoder != NULL);
	assert(xrtSnappyDecompressorUpdate(pDecoder, sWithPadding, 16u));
	assert(!xrtSnappyDecompressorFinish(pDecoder));
	assert(xrtSnappyDecompressorError(pDecoder)->iCode == XRT_SNAPPY_ERROR_TRUNCATED);
	xrtSnappyDecompressorDestroy(pDecoder);

	memcpy(sJoined, kAbcFrame, sizeof(kAbcFrame));
	memcpy(sJoined + sizeof(kAbcFrame), kAbcFrame, sizeof(kAbcFrame));
	pDecoder = xrtSnappyDecompressorCreate(NULL);
	assert(xrtSnappyDecompressorUpdate(pDecoder, sJoined, sizeof(sJoined)));
	assert(xrtSnappyDecompressorFinish(pDecoder));
	assert(xrtSnappyDecompressorMembers(pDecoder) == 2u);
	pOutput = xrtSnappyDecompressorTakeBuffer(pDecoder);
	assert(xrtBufferView(pOutput).Size == 6u);
	assert(memcmp(xrtBufferView(pOutput).Data, "abcabc", 6u) == 0);
	xrtBufferDestroy(pOutput);
	xrtSnappyDecompressorDestroy(pDecoder);
}

static void testMemberLimit(void)
{
	unsigned char sJoined[sizeof(kAbcFrame) * 2u];
	xrtsnappyoptions Options;
	xrtsnappyerror Error;
	xrtsnappydecompressor* pDecoder;
	xbuffer* pOutput = NULL;

	memcpy(sJoined, kAbcFrame, sizeof(kAbcFrame));
	memcpy(sJoined + sizeof(kAbcFrame), kAbcFrame, sizeof(kAbcFrame));
	xrtSnappyOptionsInit(&Options);
	Options.tLimits.iMaxEntries = 1u;
	assert(xrtSnappyDecompress(kAbcFrame, sizeof(kAbcFrame),
		&Options, &pOutput, &Error));
	xrtBufferDestroy(pOutput);
	pOutput = NULL;
	assert(!xrtSnappyDecompress(sJoined, sizeof(sJoined),
		&Options, &pOutput, &Error));
	assert(pOutput == NULL && Error.iCode == XRT_SNAPPY_ERROR_LIMIT);
	Options.tLimits.iMaxEntries = 0u;
	assert(xrtSnappyDecompress(sJoined, sizeof(sJoined),
		&Options, &pOutput, &Error));
	assert(xrtBufferView(pOutput).Size == 6u);
	xrtBufferDestroy(pOutput);
	pOutput = NULL;
	Options.tLimits.iMaxEntries = 1u;

	pDecoder = xrtSnappyDecompressorCreate(&Options);
	assert(pDecoder != NULL);
	assert(xrtSnappyDecompressorUpdate(pDecoder, sJoined, sizeof(kAbcFrame)));
	assert(xrtSnappyDecompressorMembers(pDecoder) == 1u);
	assert(!xrtSnappyDecompressorUpdate(pDecoder,
		sJoined + sizeof(kAbcFrame), sizeof(kAbcFrame)));
	assert(xrtSnappyDecompressorError(pDecoder)->iCode == XRT_SNAPPY_ERROR_LIMIT);
	xrtSnappyDecompressorDestroy(pDecoder);
}

/* 选项 ABI 必须在读取输入或创建输出前完成验证。 */
static void testInvalidOptions(void)
{
	xrtsnappyoptions Options;
	xrtsnappyerror Error;
	xrtsnappycompressor* pEncoder;
	xrtsnappydecompressor* pDecoder;
	xbuffer* pOutput;
	xbuffer* pSink;
	xreader* pReader;
	xwriter* pWriter;
	int iVariant;
	xrtSnappyOptionsInit(&Options);
	assert(Options.tLimits.iMaxNodes == 0u &&
		Options.tLimits.iMaxDepth == 0u);
	for ( iVariant = 0; iVariant < 9; ++iVariant ) {
		xrtSnappyOptionsInit(&Options);
		switch ( iVariant ) {
			case 0: Options.iVersion++; break;
			case 1: Options.iSize = sizeof(Options) - 1u; break;
			case 2: Options.tLimits.iVersion++; break;
			case 3: Options.tLimits.iSize = 0u; break;
			case 4: Options.sReserved[0] = 1u; break;
			case 5: Options.tLimits.iReserved = 1u; break;
			case 6: Options.tLimits.iFlags = 1u; break;
			case 7: Options.tLimits.iMaxNodes = 1u; break;
			default: Options.tLimits.iMaxDepth = 1u; break;
		}
		pEncoder = xrtSnappyCompressorCreate(&Options);
		assert(pEncoder != NULL &&
			xrtSnappyCompressorError(pEncoder)->iCode == XRT_SNAPPY_ERROR_ARGUMENT);
		xrtSnappyCompressorDestroy(pEncoder);
		pDecoder = xrtSnappyDecompressorCreate(&Options);
		assert(pDecoder != NULL &&
			xrtSnappyDecompressorError(pDecoder)->iCode == XRT_SNAPPY_ERROR_ARGUMENT);
		xrtSnappyDecompressorDestroy(pDecoder);
		pOutput = NULL;
		assert(!xrtSnappyCompress("x", 1u, &Options, &pOutput, &Error));
		assert(pOutput == NULL && Error.iCode == XRT_SNAPPY_ERROR_ARGUMENT);
		assert(!xrtSnappyDecompress("x", 1u, &Options, &pOutput, &Error));
		assert(pOutput == NULL && Error.iCode == XRT_SNAPPY_ERROR_ARGUMENT);
		pSink = xrtBufferCreate();
		assert(pSink != NULL);
		pReader = xrtReaderFromMemory((xbytesview){ (const uint8*)"x", 1u });
		pWriter = xrtWriterFromBuffer(pSink);
		assert(pReader != NULL && pWriter != NULL);
		assert(!xrtSnappyCompressStream(pReader, pWriter, &Options,
			NULL, NULL, &Error) && Error.iCode == XRT_SNAPPY_ERROR_ARGUMENT);
		assert(xrtReaderDestroy(pReader) && xrtWriterDestroy(pWriter));
		pReader = xrtReaderFromMemory((xbytesview){ (const uint8*)"x", 1u });
		pWriter = xrtWriterFromBuffer(pSink);
		assert(pReader != NULL && pWriter != NULL);
		assert(!xrtSnappyDecompressStream(pReader, pWriter, &Options,
			NULL, NULL, &Error) && Error.iCode == XRT_SNAPPY_ERROR_ARGUMENT);
		assert(xrtReaderDestroy(pReader) && xrtWriterDestroy(pWriter));
		assert(xrtBufferView(pSink).Size == 0u);
		xrtBufferDestroy(pSink);
	}
}

int main(void)
{
	testLargeUpdate();
	testLargeSkippableUpdate();
	testInvalidOptions();
	testGoldenFrame();
	testStreamAndLimits();
	testMemberLimit();
	testSeekedStreamProgress();
	testInvalidAndSkippable();
	return 0;
}
