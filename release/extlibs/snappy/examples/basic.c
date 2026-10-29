#ifdef __TINYC__
/* 单文件直跑时带入编解码器；正式 C 构建继续单独链接这些源文件。 */
#include "../third_party/xrt_snappy.c"
#include "../xrt_snappy.c"
#else
#include "xrt_snappy.h"
#endif

#include <stdio.h>
#include <string.h>

int main(void)
{
	static const char sText[] = "hello snappy";
	xbuffer* pPacked = NULL;
	xbuffer* pPlain = NULL;
	xrtsnappyerror Error;
	xbytesview Plain;
	if ( !xrtSnappyCompress(sText, sizeof(sText) - 1u, NULL, &pPacked, &Error) ) {
		fprintf(stderr, "%s\n", Error.sMessage);
		return 1;
	}
	if ( !xrtSnappyDecompress(xrtBufferView(pPacked).Data,
		xrtBufferView(pPacked).Size, NULL, &pPlain, &Error) ) {
		fprintf(stderr, "%s\n", Error.sMessage);
		xrtBufferDestroy(pPacked);
		return 1;
	}
	Plain = xrtBufferView(pPlain);
	if ( Plain.Size != sizeof(sText) - 1u ||
		memcmp(Plain.Data, sText, Plain.Size) != 0 ) {
		xrtBufferDestroy(pPlain);
		xrtBufferDestroy(pPacked);
		return 1;
	}
	printf("%.*s\n", (int)Plain.Size, (const char*)Plain.Data);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pPacked);
	return 0;
}
