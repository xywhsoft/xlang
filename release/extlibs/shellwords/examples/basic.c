#include "../include/xrt_shellwords.h"
#include <stdio.h>

int main(void)
{
	xvalue* pWords = xrtShellwordsSplit("tool 'two words' \"你好\"");
	xstrview Argument;
	str sQuoted = NULL;
	str sJoined = NULL;
	str sWindows = NULL;
	int iResult = 1;
	if ( pWords == NULL || xrtValueCount(pWords) != 3u ||
		!xrtValueGetString(xrtValueArrayGet(pWords, 1u), &Argument) ) {
		goto cleanup;
	}
	printf("shellwords: %zu %.*s ", xrtValueCount(pWords),
		(int)Argument.Size, Argument.Data);
	if ( !xrtValueGetString(xrtValueArrayGet(pWords, 2u), &Argument) ) {
		goto cleanup;
	}
	printf("%.*s\n", (int)Argument.Size, Argument.Data);
	sQuoted = xrtShellwordsQuote("A=B");
	sJoined = xrtShellwordsJoin(pWords);
	sWindows = xrtShellwordsQuoteWindows("a b\\");
	if ( sQuoted == NULL || sJoined == NULL || sWindows == NULL ) { goto cleanup; }
	printf("quote: %s\njoin: %s\nwindows: %s\n", sQuoted, sJoined, sWindows);
	iResult = 0;
cleanup:
	xrtFree(sQuoted);
	xrtFree(sJoined);
	xrtFree(sWindows);
	xrtValueRelease(pWords);
	return iResult;
}

/* 单文件范例可直接运行；正式包仍由 manifest 编译原生实现。 */
#include "../shellwords_native.c"
