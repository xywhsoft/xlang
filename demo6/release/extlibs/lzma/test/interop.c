#include "../include/xrt_lzma.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv)
{
	xbuffer* pInput = xrtBufferCreate();
	xbuffer* pOutput = NULL;
	xrtlzmaerror Error;
	xrtlzmaoptions Options;
	unsigned char Chunk[65536];
	size_t iRead;
	bool bEncode;
	bool bOk;
	if ( argc != 2 || (strcmp(argv[1], "encode") != 0 &&
		strcmp(argv[1], "decode") != 0) || pInput == NULL ) { return 2; }
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	bEncode = strcmp(argv[1], "encode") == 0;
	xrtLzmaOptionsInit(&Options);
	/* 互通夹具只校验格式；压缩比上限由独立安全边界测试覆盖。 */
	Options.tLimits.iMaxCompressionRatio = 0u;
	while ( (iRead = fread(Chunk, 1u, sizeof(Chunk), stdin)) != 0u ) {
		if ( !xrtBufferAppend(pInput, (xbytesview){ Chunk, iRead }) ) {
			xrtBufferDestroy(pInput);
			return 2;
		}
	}
	if ( ferror(stdin) ) { xrtBufferDestroy(pInput); return 2; }
	bOk = bEncode ? xrtLzmaCompress(pInput->Data, pInput->Size,
		&Options, &pOutput, &Error) : xrtLzmaDecompress(pInput->Data,
		pInput->Size, &Options, &pOutput, &Error);
	xrtBufferDestroy(pInput);
	if ( !bOk ) {
		fprintf(stderr, "lzma: %s\n", Error.sMessage);
		return 1;
	}
	bOk = pOutput->Size == 0u ||
		fwrite(pOutput->Data, 1u, pOutput->Size, stdout) == pOutput->Size;
	xrtBufferDestroy(pOutput);
	return bOk ? 0 : 2;
}
