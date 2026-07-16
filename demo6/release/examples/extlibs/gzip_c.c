#include "../../extlibs/gzip/include/xrt_gzip.h"

#include <stdio.h>
#include <string.h>



int main(void)
{
	static const unsigned char sSource[] = { 'x', 'l', 'a', 'n', 'g', 0u, 0xffu };
	xrtgzipoptions tOptions;
	xrtgziperror tError;
	xbuffer pPacked = NULL;
	xbuffer pPlain = NULL;
	int iResult = 1;
	if ( xrtInit() == NULL ) { return 1; }
	xrtGzipOptionsInit(&tOptions);
	tOptions.iLevel = 9;
	if ( !xrtGzipCompress(sSource, sizeof(sSource), &tOptions, &pPacked, &tError) ||
		!xrtGzipDecompress(pPacked->Buffer, pPacked->Length, NULL, &pPlain, &tError) ) {
		fprintf(stderr, "gzip C example: %s\n", tError.sMessage);
		goto cleanup;
	}
	if ( pPlain->Length != sizeof(sSource) ||
		memcmp(pPlain->Buffer, sSource, sizeof(sSource)) != 0 ) {
		fprintf(stderr, "gzip C example: round trip mismatch\n");
		goto cleanup;
	}
	puts("gzip C example: PASS");
	iResult = 0;

cleanup:
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pPacked);
	xrtUnit();
	return iResult;
}
