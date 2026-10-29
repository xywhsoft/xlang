#include "../../extlibs/bz2/include/xrt_bz2.h"

#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 使用同一份 bzip2 后端；正式包仍按清单分别编译。 */
#include "../../extlibs/_bzip2/third_party/bzip2/blocksort.c"
#include "../../extlibs/_bzip2/third_party/bzip2/huffman.c"
#include "../../extlibs/_bzip2/third_party/bzip2/crctable.c"
#include "../../extlibs/_bzip2/third_party/bzip2/randtable.c"
#include "../../extlibs/_bzip2/third_party/bzip2/compress.c"
#include "../../extlibs/_bzip2/third_party/bzip2/decompress.c"
#include "../../extlibs/_bzip2/third_party/bzip2/bzlib.c"
#include "../../extlibs/bz2/xrt_bz2.c"
#endif



int main(void)
{
	static const unsigned char sSource[] = { 'x', 'l', 'a', 'n', 'g', 0u, 0xffu };
	xrtbz2options tOptions;
	xrtbz2error tError;
	xbuffer* pPacked = NULL;
	xbuffer* pPlain = NULL;
	int iResult = 1;
	xrtBz2OptionsInit(&tOptions);
	tOptions.iLevel = 9;
	if ( !xrtBz2Compress(sSource, sizeof(sSource), &tOptions, &pPacked, &tError) ||
		!xrtBz2Decompress(pPacked->Data, pPacked->Size, NULL, &pPlain, &tError) ) {
		fprintf(stderr, "bz2 C example: %s\n", tError.sMessage);
		goto cleanup;
	}
	if ( pPlain->Size != sizeof(sSource) ||
		memcmp(pPlain->Data, sSource, sizeof(sSource)) != 0 ) {
		fprintf(stderr, "bz2 C example: round trip mismatch\n");
		goto cleanup;
	}
	puts("bz2 C example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pPacked);
	return iResult;
}
