#include "../../extlibs/bz2/include/xrt_bz2.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	static const unsigned char sSource[] = { 'x', 'l', 'a', 'n', 'g', 0u, 0xffu };
	xrtbz2options tOptions;
	xrtbz2error tError;
	xbuffer pPacked = NULL;
	xbuffer pPlain = NULL;
	int iResult = 1;
	if ( xrtInit() == NULL ) { return 1; }
	xrtBz2OptionsInit(&tOptions);
	tOptions.iLevel = 9;
	if ( !xrtBz2Compress(sSource, sizeof(sSource), &tOptions, &pPacked, &tError) ||
		!xrtBz2Decompress(pPacked->Buffer, pPacked->Length, NULL, &pPlain, &tError) ) {
		fprintf(stderr, "bz2 C example: %s\n", tError.sMessage);
		goto cleanup;
	}
	if ( pPlain->Length != sizeof(sSource) ||
		memcmp(pPlain->Buffer, sSource, sizeof(sSource)) != 0 ) {
		fprintf(stderr, "bz2 C example: round trip mismatch\n");
		goto cleanup;
	}
	puts("bz2 C example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pPacked);
	xrtUnit();
	return iResult;
}
