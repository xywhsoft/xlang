#include "../../extlibs/gzip/include/xrt_gzip.h"

#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 使用包内实现；正式包仍按清单分别编译。 */
#include "../../extlibs/gzip/xrt_gzip.c"
#endif



int main(void)
{
	static const unsigned char sSource[] = { 'x', 'l', 'a', 'n', 'g', 0u, 0xffu };
	xrtgzipoptions tOptions;
	xrtgziperror tError;
	xbuffer* pPacked = NULL;
	xbuffer* pPlain = NULL;
	int iResult = 1;
	xrtGzipOptionsInit(&tOptions);
	tOptions.iLevel = 9;
	if ( !xrtGzipCompress(sSource, sizeof(sSource), &tOptions, &pPacked, &tError) ||
		!xrtGzipDecompress(pPacked->Data, pPacked->Size, NULL, &pPlain, &tError) ) {
		fprintf(stderr, "gzip C example: %s\n", tError.sMessage);
		goto cleanup;
	}
	if ( pPlain->Size != sizeof(sSource) ||
		memcmp(pPlain->Data, sSource, sizeof(sSource)) != 0 ) {
		fprintf(stderr, "gzip C example: round trip mismatch\n");
		goto cleanup;
	}
	puts("gzip C example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pPacked);
	return iResult;
}
