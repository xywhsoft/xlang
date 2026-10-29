#include "../include/xrt_miniz.h"

#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 使用包内编解码；正式包按清单分离编译。 */
#include "../xrt_miniz.c"
#endif

int main(void)
{
	static const char sText[] = "hello miniz";
	xbuffer* pCompressed = NULL;
	xbuffer* pPlain = NULL;
	xrtminizerror Error;
	xbytesview Plain;
	if ( !xrtMinizCompress(sText, sizeof(sText) - 1u, NULL, &pCompressed, &Error) ) {
		fprintf(stderr, "%s\n", Error.sMessage);
		return 1;
	}
	if ( !xrtMinizDecompress(xrtBufferView(pCompressed).Data,
		xrtBufferView(pCompressed).Size, NULL, &pPlain, &Error) ) {
		fprintf(stderr, "%s\n", Error.sMessage);
		xrtBufferDestroy(pCompressed);
		return 1;
	}
	Plain = xrtBufferView(pPlain);
	if ( Plain.Size != sizeof(sText) - 1u ||
		memcmp(Plain.Data, sText, Plain.Size) != 0 ) {
		xrtBufferDestroy(pPlain);
		xrtBufferDestroy(pCompressed);
		return 1;
	}
	printf("%.*s\n", (int)Plain.Size, (const char*)Plain.Data);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pCompressed);
	return 0;
}
