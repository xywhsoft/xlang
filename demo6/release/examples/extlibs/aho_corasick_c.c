#include "../../extlibs/aho_corasick/include/xrt_aho_corasick.h"

#include <stdio.h>



/* 即时处理跨分块匹配。 */
static bool printMatch(const xrtahomatch* pMatch, ptr pUserData)
{
	(void)pUserData;
	printf("%.*s @%llu..%llu\n", (int)pMatch->Pattern.Size,
		pMatch->Pattern.Data,
		(unsigned long long)pMatch->iStart,
		(unsigned long long)pMatch->iEnd);
	return true;
}



/* 演示排序搜索和跨分块扫描两条常用路径。 */
int main(void)
{
	static const xstrview arrPatterns[] = {
		XRT_STR_INIT("he"),
		XRT_STR_INIT("she"),
		XRT_STR_INIT("his"),
		XRT_STR_INIT("hers")
	};
	xrtaho* pAutomaton = xrtAhoCreate(NULL);
	xrtahoresult* pResult;
	xrtahostate* pState;
	xrtahoinfo Info;
	size_t i;
	int iExit = 1;

	if ( pAutomaton == NULL ||
		!xrtAhoAddMany(pAutomaton, arrPatterns,
			sizeof(arrPatterns) / sizeof(arrPatterns[0])) ||
		!xrtAhoBuild(pAutomaton) ) {
		goto cleanup;
	}
	xrtAhoInfoInit(&Info);
	if ( !xrtAhoGetInfo(pAutomaton, &Info) ) { goto cleanup; }
	printf("nodes=%llu transitions=%llu allocated=%llu bytes\n",
		(unsigned long long)Info.iNodes,
		(unsigned long long)Info.iTransitions,
		(unsigned long long)Info.iAllocatedBytes);

	puts("sorted search:");
	pResult = xrtAhoSearch(pAutomaton, XRT_STR_LITERAL("ushers"));
	if ( pResult == NULL ) { goto cleanup; }
	for ( i = 0u; i < xrtAhoResultCount(pResult); ++i ) {
		xrtahomatch Match;

		if ( !xrtAhoResultAt(pResult, i, &Match) ) {
			xrtAhoResultDestroy(pResult);
			goto cleanup;
		}
		printMatch(&Match, NULL);
	}
	xrtAhoResultDestroy(pResult);

	puts("incremental scan:");
	pState = xrtAhoStateCreate(pAutomaton);
	if ( pState == NULL ||
		xrtAhoStateFeed(pState, XRT_STR_LITERAL("us"),
			printMatch, NULL, NULL) == XRT_AHO_SCAN_ERROR ||
		xrtAhoStateFeed(pState, XRT_STR_LITERAL("hers"),
			printMatch, NULL, NULL) == XRT_AHO_SCAN_ERROR ) {
		xrtAhoStateDestroy(pState);
		goto cleanup;
	}
	xrtAhoStateDestroy(pState);
	iExit = 0;

cleanup:
	if ( iExit != 0 ) {
		const xerror* pError = xrtGetError();
		fprintf(stderr, "aho_corasick: %s\n", pError != NULL ?
			xrtErrorMessage(pError) : "operation failed");
	}
	xrtAhoDestroy(pAutomaton);
	return iExit;
}
