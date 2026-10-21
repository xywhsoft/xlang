#include "xrt_miniz.h"

/* 测试操作位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Python zlib.compress(b"abc")，独立于本包编码器。 */
static const unsigned char kPythonAbc[] = {
	0x78, 0x9c, 0x4b, 0x4c, 0x4a, 0x06, 0x00, 0x02, 0x4d, 0x01, 0x27
};

typedef struct {
	unsigned calls;
	uint64 input;
	uint64 total;
	bool known;
} stream_progress;

static bool record_progress(const xrtprogress* progress, ptr user)
{
	stream_progress* state = (stream_progress*)user;
	assert(progress->iSize == sizeof(*progress));
	assert(progress->iVersion == XRT_PROGRESS_VERSION);
	state->calls++;
	state->input = progress->iInputBytes;
	state->total = progress->iTotalInputBytes;
	state->known = (progress->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0u;
	return !state->known || state->input <= state->total;
}

typedef struct {
	const uint8* data;
	size_t size;
	size_t offset;
} unknown_reader;

static bool read_unknown(ptr context, ptr output, size_t requested, size_t* read)
{
	unknown_reader* source = (unknown_reader*)context;
	size_t count = source->size - source->offset;
	if ( count > requested ) { count = requested; }
	if ( count != 0u ) {
		memcpy(output, source->data + source->offset, count);
		source->offset += count;
	}
	*read = count;
	return true;
}

/* 流式进度从当前读取位置计数，不能把底层 Reader 的前缀算入总量。 */
static void testSeekedStreamProgress(void)
{
	static const unsigned char input[] = "XXXXXXXXXXabc";
	const uint64 prefix = 10u;
	xrtminizerror error;
	xreader* reader;
	xwriter* writer;
	xbuffer* packed = xrtBufferCreate();
	xbuffer* wrapped = xrtBufferCreate();
	xbuffer* plain = xrtBufferCreate();
	stream_progress progress = {0};
	uint64 position = 0u;
	assert(packed && wrapped && plain);
	reader = xrtReaderFromMemory((xbytesview){input, sizeof(input) - 1u});
	writer = xrtWriterFromBuffer(packed);
	assert(reader && writer && xrtReaderSeek(reader, (int64)prefix,
		XSEEK_START, &position) && position == prefix);
	assert(xrtMinizCompressStream(reader, writer, NULL,
		record_progress, &progress, &error));
	assert(progress.calls > 0u && progress.known &&
		progress.input == 3u && progress.total == 3u);
	assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));

	assert(xrtBufferAppend(wrapped, XRT_BYTES_LITERAL("XXXXXXXXXX")) &&
		xrtBufferAppend(wrapped, xrtBufferView(packed)));
	reader = xrtReaderFromMemory(xrtBufferView(wrapped));
	writer = xrtWriterFromBuffer(plain);
	position = 0u;
	progress = (stream_progress){0};
	assert(reader && writer && xrtReaderSeek(reader, (int64)prefix,
		XSEEK_START, &position) && position == prefix);
	assert(xrtMinizDecompressStream(reader, writer, NULL,
		record_progress, &progress, &error));
	assert(progress.calls > 0u && progress.known &&
		progress.input == packed->Size && progress.total == packed->Size &&
		plain->Size == 3u && memcmp(plain->Data, "abc", 3u) == 0);
	assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
	xrtBufferDestroy(plain);
	xrtBufferDestroy(wrapped);
	xrtBufferDestroy(packed);

	/* 无位置与总长能力的流仍可正常压缩，只是不报告已知总量。 */
	{
		static const xreaderops ops = { read_unknown, NULL, NULL, NULL, NULL };
		unknown_reader source = { (const uint8*)"abc", 3u, 0u };
		xbuffer* output = xrtBufferCreate();
		assert(output != NULL);
		reader = xrtReaderCreate(&ops, &source);
		writer = xrtWriterFromBuffer(output);
		progress = (stream_progress){0};
		assert(reader && writer && xrtMinizCompressStream(reader, writer, NULL,
			record_progress, &progress, &error));
		assert(progress.calls > 0u && progress.input == 3u &&
			!progress.known && progress.total == 0u && xrtGetError() == NULL);
		assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
		xrtBufferDestroy(output);
	}
}

static void testReaderWriter(void)
{
	static const unsigned char input[] = {'x', 'l', 'a', 'n', 'g', 0, 0xff};
	xreader* reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
	xbuffer* packed = xrtBufferCreate();
	xwriter* writer = packed != NULL ? xrtWriterFromBuffer(packed) : NULL;
	xbuffer* plain;
	xrtminizerror error;
	stream_progress progress = {0};
	assert(reader && writer);
	assert(xrtMinizCompressStream(reader, writer, NULL,
		record_progress, &progress, &error));
	assert(progress.calls > 0u && progress.input == sizeof(input) &&
		progress.total == sizeof(input));
	assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));

	reader = xrtReaderFromMemory(xrtBufferView(packed));
	plain = xrtBufferCreate();
	writer = plain != NULL ? xrtWriterFromBuffer(plain) : NULL;
	progress = (stream_progress){0};
	assert(reader && writer);
	assert(xrtMinizDecompressStream(reader, writer, NULL,
		record_progress, &progress, &error));
	assert(progress.calls > 0u && progress.input == xrtBufferView(packed).Size &&
		progress.total == progress.input);
	assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
	assert(xrtBufferView(plain).Size == sizeof(input) &&
		memcmp(xrtBufferView(plain).Data, input, sizeof(input)) == 0);
	xrtBufferDestroy(plain);
	xrtBufferDestroy(packed);
}

static void testReferenceAndStreaming(void)
{
	unsigned char sData[65536u + 13u];
	xbuffer* pOutput = NULL;
	xbuffer* pCompressed = NULL;
	xrtminizerror Error;
	xrtminizdecompressor* pDecoder;
	xrtminizcompressor* pEncoder;
	xbytesview View;
	size_t i;
	assert(xrtMinizDecompress(kPythonAbc, sizeof(kPythonAbc), NULL, &pOutput, &Error));
	View = xrtBufferView(pOutput);
	assert(View.Size == 3u && memcmp(View.Data, "abc", 3u) == 0);
	xrtBufferDestroy(pOutput);

	memset(sData, 'A', sizeof(sData));
	pEncoder = xrtMinizCompressorCreate(NULL);
	assert(pEncoder != NULL);
	assert(xrtMinizCompressorUpdate(pEncoder, sData, 65536u));
	assert(xrtMinizCompressorUpdate(pEncoder, sData + 65536u, 13u));
	assert(xrtMinizCompressorFinish(pEncoder));
	pCompressed = xrtMinizCompressorTakeBuffer(pEncoder);
	assert(pCompressed != NULL);
	xrtMinizCompressorDestroy(pEncoder);
	View = xrtBufferView(pCompressed);
	assert(View.Size < sizeof(sData));
	pDecoder = xrtMinizDecompressorCreate(NULL);
	assert(pDecoder != NULL);
	for ( i = 0u; i < View.Size; ++i ) {
		assert(xrtMinizDecompressorUpdate(pDecoder, View.Data + i, 1u));
	}
	assert(xrtMinizDecompressorFinished(pDecoder));
	assert(xrtMinizDecompressorFinish(pDecoder));
	assert(xrtMinizDecompressorFinished(pDecoder));
	pOutput = xrtMinizDecompressorTakeBuffer(pDecoder);
	assert(pOutput != NULL);
	assert(xrtBufferView(pOutput).Size == sizeof(sData));
	assert(memcmp(xrtBufferView(pOutput).Data, sData, sizeof(sData)) == 0);
	xrtBufferDestroy(pOutput);
	xrtMinizDecompressorDestroy(pDecoder);
	xrtBufferDestroy(pCompressed);
}

static void testErrors(void)
{
	unsigned char sBad[sizeof(kPythonAbc)];
	unsigned char sJoined[sizeof(kPythonAbc) * 2u];
	xrtminizoptions Options;
	xrtminizerror Error;
	xbuffer* pOutput = NULL;
	xbuffer* pEmpty = NULL;
	assert(xrtMinizCompress(NULL, 0u, NULL, &pEmpty, &Error));
	assert(xrtMinizDecompress(xrtBufferView(pEmpty).Data,
		xrtBufferView(pEmpty).Size, NULL, &pOutput, &Error));
	assert(xrtBufferView(pOutput).Size == 0u);
	xrtBufferDestroy(pOutput);
	xrtBufferDestroy(pEmpty);
	pOutput = NULL;

	memcpy(sBad, kPythonAbc, sizeof(sBad));
	sBad[sizeof(sBad) - 1u] ^= 1u;
	assert(!xrtMinizDecompress(sBad, sizeof(sBad), NULL, &pOutput, &Error));
	assert(Error.iCode == XRT_MINIZ_ERROR_DATA);
	assert(!xrtMinizDecompress(kPythonAbc, sizeof(kPythonAbc) - 1u,
		NULL, &pOutput, &Error));
	assert(Error.iCode == XRT_MINIZ_ERROR_TRUNCATED);
	memcpy(sJoined, kPythonAbc, sizeof(kPythonAbc));
	memcpy(sJoined + sizeof(kPythonAbc), kPythonAbc, sizeof(kPythonAbc));
	assert(!xrtMinizDecompress(sJoined, sizeof(sJoined), NULL, &pOutput, &Error));
	assert(Error.iCode == XRT_MINIZ_ERROR_DATA);

	xrtMinizOptionsInit(&Options);
	Options.tLimits.iMaxOutputBytes = 2u;
	assert(!xrtMinizDecompress(kPythonAbc, sizeof(kPythonAbc), &Options, &pOutput, &Error));
	assert(Error.iCode == XRT_MINIZ_ERROR_LIMIT);
	xrtMinizOptionsInit(&Options);
	Options.tLimits.iMaxItemBytes = 2u;
	assert(!xrtMinizDecompress(kPythonAbc, sizeof(kPythonAbc), &Options, &pOutput, &Error));
	assert(pOutput == NULL && Error.iCode == XRT_MINIZ_ERROR_LIMIT);
}

/* 无效的外层或嵌套限额 ABI 不得进入编解码器。 */
static void testInvalidOptions(void)
{
	xrtminizoptions Options;
	xrtminizerror Error;
	xrtminizcompressor* pEncoder;
	xrtminizdecompressor* pDecoder;
	xbuffer* pOutput;
	xbuffer* pPlain;
	xbuffer* pSink;
	xreader* pReader;
	xwriter* pWriter;
	int iVariant;
	xrtMinizOptionsInit(&Options);
	assert(Options.tLimits.iMaxEntries == 0u &&
		Options.tLimits.iMaxNodes == 0u &&
		Options.tLimits.iMaxDepth == 0u);
	for ( iVariant = 0; iVariant < 12; ++iVariant ) {
		xrtMinizOptionsInit(&Options);
		switch ( iVariant ) {
			case 0: Options.iVersion++; break;
			case 1: Options.iSize = sizeof(Options) - 1u; break;
			case 2: Options.tLimits.iVersion++; break;
			case 3: Options.tLimits.iSize = 0u; break;
			case 4: Options.sReserved[0] = 1u; break;
			case 5: Options.tLimits.iReserved = 1u; break;
			case 6: Options.tLimits.iFlags = 1u; break;
			case 7: Options.tLimits.iMaxEntries = 1u; break;
			case 8: Options.tLimits.iMaxNodes = 1u; break;
			case 9: Options.tLimits.iMaxDepth = 1u; break;
			case 10: Options.iLevel = -1; break;
			default: Options.iLevel = 11; break;
		}
		pEncoder = xrtMinizCompressorCreate(&Options);
		assert(pEncoder != NULL &&
			xrtMinizCompressorError(pEncoder)->iCode == XRT_MINIZ_ERROR_ARGUMENT);
		xrtMinizCompressorDestroy(pEncoder);
		pDecoder = xrtMinizDecompressorCreate(&Options);
		assert(pDecoder != NULL &&
			xrtMinizDecompressorError(pDecoder)->iCode == XRT_MINIZ_ERROR_ARGUMENT);
		xrtMinizDecompressorDestroy(pDecoder);
		pOutput = NULL;
		assert(!xrtMinizCompress("x", 1u, &Options, &pOutput, &Error));
		assert(pOutput == NULL && Error.iCode == XRT_MINIZ_ERROR_ARGUMENT);
		assert(!xrtMinizDecompress("x", 1u, &Options, &pOutput, &Error));
		assert(pOutput == NULL && Error.iCode == XRT_MINIZ_ERROR_ARGUMENT);
		pSink = xrtBufferCreate();
		assert(pSink != NULL);
		pReader = xrtReaderFromMemory((xbytesview){ (const uint8*)"x", 1u });
		pWriter = xrtWriterFromBuffer(pSink);
		assert(pReader != NULL && pWriter != NULL);
		assert(!xrtMinizCompressStream(pReader, pWriter, &Options,
			NULL, NULL, &Error) && Error.iCode == XRT_MINIZ_ERROR_ARGUMENT);
		assert(xrtReaderDestroy(pReader) && xrtWriterDestroy(pWriter));
		pReader = xrtReaderFromMemory((xbytesview){ (const uint8*)"x", 1u });
		pWriter = xrtWriterFromBuffer(pSink);
		assert(pReader != NULL && pWriter != NULL);
		assert(!xrtMinizDecompressStream(pReader, pWriter, &Options,
			NULL, NULL, &Error) && Error.iCode == XRT_MINIZ_ERROR_ARGUMENT);
		assert(xrtReaderDestroy(pReader) && xrtWriterDestroy(pWriter));
		assert(xrtBufferView(pSink).Size == 0u);
		xrtBufferDestroy(pSink);
	}
	for ( iVariant = 0; iVariant <= 10; ++iVariant ) {
		xrtMinizOptionsInit(&Options);
		Options.iLevel = iVariant;
		pOutput = NULL;
		pPlain = NULL;
		assert(xrtMinizCompress("abc", 3u, &Options, &pOutput, &Error));
		assert(xrtMinizDecompress(pOutput->Data, pOutput->Size,
			&Options, &pPlain, &Error));
		assert(pPlain->Size == 3u && memcmp(pPlain->Data, "abc", 3u) == 0);
		xrtBufferDestroy(pPlain);
		xrtBufferDestroy(pOutput);
	}
}

int main(int argc, char** argv)
{
	xbuffer* pCompressed = NULL;
	xrtminizerror Error;
	xbytesview View;
	size_t i;
	testReferenceAndStreaming();
	testReaderWriter();
	testSeekedStreamProgress();
	testErrors();
	testInvalidOptions();
	if ( argc > 1 && strcmp(argv[1], "--emit") == 0 ) {
		assert(xrtMinizCompress("abc", 3u, NULL, &pCompressed, &Error));
		View = xrtBufferView(pCompressed);
		for ( i = 0u; i < View.Size; ++i ) { printf("%02x", View.Data[i]); }
		putchar('\n');
		xrtBufferDestroy(pCompressed);
	}
	return 0;
}
