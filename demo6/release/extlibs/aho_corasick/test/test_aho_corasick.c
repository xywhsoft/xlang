#define XRT_MODULE_ATOMIC
#define XRT_MODULE_STRING
#define XRT_MODULE_UNICODE
#define XRT_AHO_TESTING

#include "../include/xrt_aho_corasick.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 仅在本测试翻译单元启用的模块内故障注入入口。 */
extern void xrtAhoTestFailAfter(size_t iAllocation);



typedef struct testmatches {
	xrtahomatch Items[16];
	size_t Count;
	bool Stop;
} testmatches;



typedef struct expectedmatch {
	size_t Pattern;
	size_t Start;
	size_t End;
} expectedmatch;



/* 输出失败原因并保留所有测试继续执行。 */
static int check(int bCondition, cstr sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_aho_corasick: %s\n",
		sMessage != NULL ? sMessage : "check failed");
	return 0;
}



/* 记录扫描结果并按配置主动停止。 */
static bool collect_match(const xrtahomatch* pMatch, ptr pUserData)
{
	testmatches* pMatches = (testmatches*)pUserData;

	if ( pMatches->Count < (sizeof(pMatches->Items) / sizeof(pMatches->Items[0])) ) {
		pMatches->Items[pMatches->Count] = *pMatch;
	}
	pMatches->Count += 1u;
	return !pMatches->Stop;
}



/* 生成可重复的测试随机数。 */
static uint32 test_random(uint32* pState)
{
	*pState = (*pState * 1664525u) + 1013904223u;
	return *pState;
}



/* 使用高位生成小范围值，避免线性同余低位的短周期。 */
static uint32 test_random_range(uint32* pState, uint32 iLimit)
{
	return (test_random(pState) >> 8u) % iLimit;
}



/* 按公开搜索顺序比较朴素结果。 */
static int expected_compare(const void* pLeft, const void* pRight)
{
	const expectedmatch* pFirst = (const expectedmatch*)pLeft;
	const expectedmatch* pSecond = (const expectedmatch*)pRight;

	if ( pFirst->Start < pSecond->Start ) { return -1; }
	if ( pFirst->Start > pSecond->Start ) { return 1; }
	if ( pFirst->End < pSecond->End ) { return -1; }
	if ( pFirst->End > pSecond->End ) { return 1; }
	if ( pFirst->Pattern < pSecond->Pattern ) { return -1; }
	if ( pFirst->Pattern > pSecond->Pattern ) { return 1; }
	return 0;
}



/* 创建并构建常用四模式自动机。 */
static xrtaho* make_ushers(void)
{
	static const xstrview arrPatterns[] = {
		XRT_STR_INIT("he"),
		XRT_STR_INIT("she"),
		XRT_STR_INIT("his"),
		XRT_STR_INIT("hers")
	};
	xrtaho* pAutomaton = xrtAhoCreate(NULL);

	if ( pAutomaton == NULL ||
		!xrtAhoAddMany(pAutomaton, arrPatterns,
			sizeof(arrPatterns) / sizeof(arrPatterns[0])) ||
		!xrtAhoBuild(pAutomaton) ) {
		xrtAhoDestroy(pAutomaton);
		return NULL;
	}
	return pAutomaton;
}



/* 验证搜索排序、无分配便利路径和结果生命周期。 */
static int test_search_and_lifetime(void)
{
	xrtaho* pAutomaton = make_ushers();
	xrtaho* pReference;
	xrtahoresult* pResult;
	xrtahomatch Match;
	xrtahoinfo Info;
	xstrview Pattern;
	testmatches Scanned;
	bool bValue = false;
	bool bFound = false;
	uint64 iCount = 0u;
	int bOk = 1;

	bOk &= check(pAutomaton != NULL, "cannot build ushers automaton");
	bOk &= check(xrtAhoBuilt(pAutomaton) &&
		xrtAhoPatternCount(pAutomaton) == 4u,
		"automaton metadata mismatch");
	Pattern = xrtAhoPattern(pAutomaton, 3u);
	bOk &= check(Pattern.Size == 4u &&
		memcmp(Pattern.Data, "hers", 4u) == 0,
		"pattern lookup mismatch");
	pReference = xrtAhoRef(pAutomaton);
	bOk &= check(pReference == pAutomaton, "automaton ref failed");
	xrtAhoDestroy(pReference);
	xrtAhoInfoInit(&Info);
	bOk &= check(xrtAhoGetInfo(pAutomaton, &Info) && Info.bBuilt &&
		Info.iPatterns == 4u && Info.iNodes == 10u &&
		Info.iTransitions == 9u && Info.iAllocatedBytes < 4096u,
		"sparse automaton statistics mismatch");
	pResult = xrtAhoSearch(pAutomaton, XRT_STR_LITERAL("ushers"));
	bOk &= check(pResult != NULL && xrtAhoResultCount(pResult) == 3u,
		"search result count mismatch");
	bOk &= check(xrtAhoResultAt(pResult, 0u, &Match) &&
		Match.iStart == 1u && Match.iEnd == 4u &&
		Match.Pattern.Size == 3u && memcmp(Match.Pattern.Data, "she", 3u) == 0,
		"first sorted match mismatch");
	bOk &= check(xrtAhoResultAt(pResult, 1u, &Match) &&
		Match.iStart == 2u && Match.iEnd == 4u,
		"second sorted match mismatch");
	bOk &= check(xrtAhoResultAt(pResult, 2u, &Match) &&
		Match.iStart == 2u && Match.iEnd == 6u,
		"third sorted match mismatch");
	bOk &= check(xrtAhoContains(pAutomaton, XRT_STR_LITERAL("ushers"), &bValue) &&
		bValue, "contains failed");
	bOk &= check(xrtAhoCount(pAutomaton, XRT_STR_LITERAL("ushers"), &iCount) &&
		iCount == 3u, "count failed");
	bOk &= check(xrtAhoFind(pAutomaton, XRT_STR_LITERAL("ushers"),
		&Match, &bFound) && bFound && Match.iStart == 1u,
		"find failed");
	memset(&Scanned, 0, sizeof(Scanned));
	bOk &= check(xrtAhoScan(pAutomaton, XRT_STR_LITERAL("ushers"),
		collect_match, &Scanned) == XRT_AHO_SCAN_COMPLETE &&
		Scanned.Count == 3u, "single-view scan failed");

	/* Result 必须独立保留自动机，销毁顺序不能产生悬空模式。 */
	xrtAhoDestroy(pAutomaton);
	bOk &= check(xrtAhoResultAt(pResult, 0u, &Match) &&
		memcmp(Match.Pattern.Data, "she", 3u) == 0,
		"result did not retain automaton");
	xrtAhoResultDestroy(pResult);
	return bOk;
}



/* 验证左优先最长规则与相邻区间不会被误合并。 */
static int test_non_overlapping_and_transform(void)
{
	static const xstrview arrPatterns[] = {
		XRT_STR_INIT("a"),
		XRT_STR_INIT("ab"),
		XRT_STR_INIT("cd")
	};
	xrtaho* pAutomaton = xrtAhoCreate(NULL);
	xrtahoresult* pResult;
	xrtahomatch Match;
	str sOutput;
	size_t iSize;
	int bOk = 1;

	bOk &= check(pAutomaton != NULL &&
		xrtAhoAddMany(pAutomaton, arrPatterns, 3u) &&
		xrtAhoBuild(pAutomaton), "cannot build transform automaton");
	pResult = xrtAhoSearchNonOverlapping(pAutomaton,
		XRT_STR_LITERAL("abcd"));
	bOk &= check(pResult != NULL && xrtAhoResultCount(pResult) == 2u,
		"non-overlapping result count mismatch");
	bOk &= check(xrtAhoResultAt(pResult, 0u, &Match) &&
		Match.Pattern.Size == 2u && memcmp(Match.Pattern.Data, "ab", 2u) == 0,
		"leftmost-longest selection mismatch");
	xrtAhoResultDestroy(pResult);

	sOutput = xrtAhoReplace(pAutomaton, XRT_STR_LITERAL("abcd"),
		XRT_STR_LITERAL("*"), &iSize);
	bOk &= check(sOutput != NULL && iSize == 2u &&
		memcmp(sOutput, "**", 2u) == 0,
		"adjacent replace intervals were merged");
	xrtFree(sOutput);
	sOutput = xrtAhoHighlight(pAutomaton, XRT_STR_LITERAL("abcd"),
		XRT_STR_LITERAL("<"), XRT_STR_LITERAL(">"), &iSize);
	bOk &= check(sOutput != NULL && iSize == 8u &&
		memcmp(sOutput, "<ab><cd>", 8u) == 0,
		"highlight result mismatch");
	xrtFree(sOutput);
	xrtAhoDestroy(pAutomaton);
	return bOk;
}



/* 验证明文 NUL 不会截断模式或输入。 */
static int test_embedded_zero(void)
{
	static const char arrPattern[] = { 'a', 0, 'b' };
	static const char arrText[] = { 'x', 'a', 0, 'b', 'y' };
	xrtaho* pAutomaton = xrtAhoCreate(NULL);
	xrtahoresult* pResult;
	xrtahomatch Match;
	int bOk = 1;

	bOk &= check(pAutomaton != NULL &&
		xrtAhoAdd(pAutomaton, xrtStrViewN(arrPattern, sizeof(arrPattern))) &&
		xrtAhoBuild(pAutomaton), "cannot build embedded-NUL automaton");
	pResult = xrtAhoSearch(pAutomaton,
		xrtStrViewN(arrText, sizeof(arrText)));
	bOk &= check(pResult != NULL && xrtAhoResultCount(pResult) == 1u &&
		xrtAhoResultAt(pResult, 0u, &Match) &&
		Match.iStart == 1u && Match.iEnd == 4u &&
		Match.Pattern.Size == sizeof(arrPattern) &&
		memcmp(Match.Pattern.Data, arrPattern, sizeof(arrPattern)) == 0,
		"embedded-NUL match mismatch");
	xrtAhoResultDestroy(pResult);
	xrtAhoDestroy(pAutomaton);
	return bOk;
}



/* 验证跨分块状态、主动停止和零消费恢复。 */
static int test_incremental_scan(void)
{
	xrtaho* pAutomaton = make_ushers();
	xrtahostate* pState;
	testmatches Matches;
	xrtahoscanstatus iStatus;
	size_t iConsumed;
	int bOk = 1;

	pState = xrtAhoStateCreate(pAutomaton);
	xrtAhoDestroy(pAutomaton);
	memset(&Matches, 0, sizeof(Matches));
	bOk &= check(pState != NULL, "cannot create incremental state");
	iStatus = xrtAhoStateFeed(pState, XRT_STR_LITERAL("us"),
		collect_match, &Matches, &iConsumed);
	bOk &= check(iStatus == XRT_AHO_SCAN_COMPLETE && iConsumed == 2u &&
		Matches.Count == 0u, "first incremental chunk mismatch");
	iStatus = xrtAhoStateFeed(pState, XRT_STR_LITERAL("hers"),
		collect_match, &Matches, &iConsumed);
	bOk &= check(iStatus == XRT_AHO_SCAN_COMPLETE && iConsumed == 4u &&
		Matches.Count == 3u && xrtAhoStateOffset(pState) == 6u,
		"second incremental chunk mismatch");

	xrtAhoStateReset(pState);
	bOk &= check(xrtAhoStateOffset(pState) == 0u &&
		xrtAhoStateMatches(pState) == 0u,
		"incremental reset counters mismatch");
	memset(&Matches, 0, sizeof(Matches));
	Matches.Stop = true;
	iStatus = xrtAhoStateFeed(pState, XRT_STR_LITERAL("ushers"),
		collect_match, &Matches, &iConsumed);
	bOk &= check(iStatus == XRT_AHO_SCAN_STOPPED && iConsumed == 4u &&
		Matches.Count == 1u, "first stopped scan mismatch");
	iStatus = xrtAhoStateFeed(pState, XRT_STR_LITERAL("rs"),
		collect_match, &Matches, &iConsumed);
	bOk &= check(iStatus == XRT_AHO_SCAN_STOPPED && iConsumed == 0u &&
		Matches.Count == 2u, "pending overlap was not resumed");
	iStatus = xrtAhoStateFeed(pState, XRT_STR_LITERAL("rs"),
		collect_match, &Matches, &iConsumed);
	bOk &= check(iStatus == XRT_AHO_SCAN_STOPPED && iConsumed == 2u &&
		Matches.Count == 3u, "remaining chunk resume mismatch");
	iStatus = xrtAhoStateFeed(pState, XRT_STR_LITERAL(""),
		collect_match, &Matches, &iConsumed);
	bOk &= check(iStatus == XRT_AHO_SCAN_COMPLETE && iConsumed == 0u,
		"incremental completion mismatch");
	xrtAhoStateDestroy(pState);
	return bOk;
}



/* 验证 Unicode 标量遮盖和二进制字节遮盖语义。 */
static int test_mask(void)
{
	static const char arrInvalid[] = { (char)0xff };
	static const char arrBinaryPattern[] = { 0, 1 };
	static const char arrBinaryText[] = { 9, 0, 1, 8 };
	xrtaho* pAutomaton = xrtAhoCreate(NULL);
	xrtaho* pBinary = xrtAhoCreate(NULL);
	str sOutput;
	size_t iSize;
	int bOk = 1;

	bOk &= check(pAutomaton != NULL &&
		xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("你好")) &&
		xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("好世")) &&
		xrtAhoBuild(pAutomaton), "cannot build Unicode mask automaton");
	sOutput = xrtAhoMask(pAutomaton, XRT_STR_LITERAL("你好世界"),
		XRT_STR_LITERAL("*"), &iSize);
	bOk &= check(sOutput != NULL && iSize == strlen("***界") &&
		memcmp(sOutput, "***界", iSize) == 0,
		"Unicode scalar mask mismatch");
	xrtFree(sOutput);
	sOutput = xrtAhoMask(pAutomaton,
		xrtStrViewN(arrInvalid, sizeof(arrInvalid)),
		XRT_STR_LITERAL("*"), &iSize);
	bOk &= check(sOutput == NULL && xrtGetError() != NULL &&
		xrtErrorCode(xrtGetError()) == XRT_AHO_ERROR_UTF8,
		"invalid UTF-8 mask was not rejected");
	xrtClearError();

	bOk &= check(pBinary != NULL &&
		xrtAhoAdd(pBinary,
			xrtStrViewN(arrBinaryPattern, sizeof(arrBinaryPattern))) &&
		xrtAhoBuild(pBinary), "cannot build binary mask automaton");
	sOutput = xrtAhoMaskBytes(pBinary,
		xrtStrViewN(arrBinaryText, sizeof(arrBinaryText)),
		XRT_STR_LITERAL("#"), &iSize);
	bOk &= check(sOutput != NULL && iSize == 4u &&
		sOutput[0] == 9 && sOutput[1] == '#' &&
		sOutput[2] == '#' && sOutput[3] == 8,
		"binary byte mask mismatch");
	xrtFree(sOutput);
	xrtAhoDestroy(pBinary);
	xrtAhoDestroy(pAutomaton);
	return bOk;
}



/* 验证重复模式、状态错误和所有资源限制。 */
static int test_errors_and_limits(void)
{
	xrtahooptions Options;
	xrtaho* pAutomaton;
	uint64 iCount;
	str sOutput;
	size_t iSize;
	int bOk = 1;

	xrtAhoOptionsInit(&Options);
	Options.bIgnoreAsciiCase = true;
	pAutomaton = xrtAhoCreate(&Options);
	bOk &= check(pAutomaton != NULL &&
		xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("Hello")),
		"cannot add case-insensitive pattern");
	bOk &= check(!xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("hello")) &&
		xrtErrorCode(xrtGetError()) == XRT_AHO_ERROR_EXISTS,
		"case-equivalent duplicate was not rejected");
	xrtClearError();
	bOk &= check(xrtAhoBuild(pAutomaton) &&
		!xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("later")) &&
		xrtErrorCode(xrtGetError()) == XRT_AHO_ERROR_STATE,
		"add after build was not rejected");
	xrtClearError();
	xrtAhoDestroy(pAutomaton);

	xrtAhoOptionsInit(&Options);
	Options.iMaxPatternBytes = 2u;
	pAutomaton = xrtAhoCreate(&Options);
	bOk &= check(pAutomaton != NULL &&
		!xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("abc")),
		"pattern byte limit was not enforced");
	xrtClearError();
	xrtAhoDestroy(pAutomaton);

	xrtAhoOptionsInit(&Options);
	Options.iMaxMatches = 1u;
	pAutomaton = xrtAhoCreate(&Options);
	bOk &= check(pAutomaton != NULL &&
		xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("a")) &&
		xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("aa")) &&
		xrtAhoBuild(pAutomaton) &&
		!xrtAhoCount(pAutomaton, XRT_STR_LITERAL("aa"), &iCount),
		"match limit was not enforced");
	xrtClearError();
	xrtAhoDestroy(pAutomaton);

	xrtAhoOptionsInit(&Options);
	Options.iMaxInputBytes = 2u;
	pAutomaton = xrtAhoCreate(&Options);
	bOk &= check(pAutomaton != NULL &&
		xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("a")) &&
		xrtAhoBuild(pAutomaton) &&
		!xrtAhoCount(pAutomaton, XRT_STR_LITERAL("abc"), &iCount),
		"input limit was not enforced");
	xrtClearError();
	xrtAhoDestroy(pAutomaton);

	xrtAhoOptionsInit(&Options);
	Options.iMaxOutputBytes = 2u;
	pAutomaton = xrtAhoCreate(&Options);
	bOk &= check(pAutomaton != NULL && xrtAhoBuild(pAutomaton),
		"cannot build empty output-limit automaton");
	sOutput = xrtAhoReplace(pAutomaton, XRT_STR_LITERAL("abc"),
		XRT_STR_LITERAL("x"), &iSize);
	bOk &= check(sOutput == NULL && xrtErrorCode(xrtGetError()) ==
		XRT_AHO_ERROR_LIMIT, "no-match output limit was not enforced");
	xrtClearError();
	xrtAhoDestroy(pAutomaton);
	return bOk;
}



/* 验证 build 分配失败不会污染可重试的构建状态。 */
static int test_build_retry(void)
{
	xrtaho* pAutomaton = xrtAhoCreate(NULL);
	bool bContains = false;
	int bOk = 1;

	bOk &= check(pAutomaton != NULL &&
		xrtAhoAdd(pAutomaton, XRT_STR_LITERAL("retry")),
		"cannot prepare build retry test");
	xrtAhoTestFailAfter(1u);
	bOk &= check(!xrtAhoBuild(pAutomaton) && !xrtAhoBuilt(pAutomaton),
		"failed build changed automaton state");
	xrtAhoTestFailAfter(0u);
	xrtClearError();
	bOk &= check(xrtAhoBuild(pAutomaton) &&
		xrtAhoContains(pAutomaton, XRT_STR_LITERAL("retry"), &bContains) &&
		bContains, "automaton did not recover after failed build");
	xrtAhoDestroy(pAutomaton);
	return bOk;
}



/* 用朴素逐位置匹配校验随机前后缀、重叠和二进制输入。 */
static int test_random_differential(void)
{
	uint32 iRandom = 0x51a7c0deu;
	size_t iRound;
	int bOk = 1;

	for ( iRound = 0u; iRound < 200u && bOk; ++iRound ) {
		char arrPatternData[8][7];
		size_t arrPatternSize[8];
		xstrview arrPatterns[8];
		char arrText[64];
		expectedmatch arrExpected[512];
		size_t iExpected = 0u;
		size_t i;
		xrtaho* pAutomaton;
		xrtahoresult* pResult;

		for ( i = 0u; i < 8u; ++i ) {
			bool bUnique;
			size_t iAttempt = 0u;

			do {
				size_t j;
				size_t k;

				arrPatternSize[i] = 1u + test_random_range(&iRandom, 6u);
				for ( j = 0u; j < arrPatternSize[i]; ++j ) {
					arrPatternData[i][j] =
						(char)test_random_range(&iRandom, 4u);
				}
				bUnique = true;
				for ( k = 0u; k < i; ++k ) {
					if ( arrPatternSize[k] == arrPatternSize[i] &&
						memcmp(arrPatternData[k], arrPatternData[i],
							arrPatternSize[i]) == 0 ) {
						bUnique = false;
						break;
					}
				}
				iAttempt += 1u;
			} while ( !bUnique && iAttempt < 1000u );
			if ( !bUnique ) {
				bOk = check(false, "cannot generate unique random patterns");
				break;
			}
			arrPatterns[i].Data = arrPatternData[i];
			arrPatterns[i].Size = arrPatternSize[i];
		}
		if ( !bOk ) { break; }
		for ( i = 0u; i < sizeof(arrText); ++i ) {
			arrText[i] = (char)test_random_range(&iRandom, 4u);
		}

		pAutomaton = xrtAhoCreate(NULL);
		bOk &= check(pAutomaton != NULL &&
			xrtAhoAddMany(pAutomaton, arrPatterns, 8u) &&
			xrtAhoBuild(pAutomaton), "cannot build random automaton");
		if ( !bOk ) {
			xrtAhoDestroy(pAutomaton);
			break;
		}
		for ( i = 0u; i < sizeof(arrText); ++i ) {
			size_t j;

			for ( j = 0u; j < 8u; ++j ) {
				if ( arrPatternSize[j] <= (sizeof(arrText) - i) &&
					memcmp(arrText + i, arrPatternData[j],
						arrPatternSize[j]) == 0 ) {
					arrExpected[iExpected].Pattern = j;
					arrExpected[iExpected].Start = i;
					arrExpected[iExpected].End = i + arrPatternSize[j];
					iExpected += 1u;
				}
			}
		}
		qsort(arrExpected, iExpected, sizeof(arrExpected[0]),
			expected_compare);
		pResult = xrtAhoSearch(pAutomaton,
			xrtStrViewN(arrText, sizeof(arrText)));
		bOk &= check(pResult != NULL &&
			xrtAhoResultCount(pResult) == iExpected,
			"random result count mismatch");
		for ( i = 0u; i < iExpected && bOk; ++i ) {
			xrtahomatch Match;

			bOk &= check(xrtAhoResultAt(pResult, i, &Match) &&
				Match.iPattern == arrExpected[i].Pattern &&
				Match.iStart == arrExpected[i].Start &&
				Match.iEnd == arrExpected[i].End,
				"random differential match mismatch");
		}
		xrtAhoResultDestroy(pResult);
		xrtAhoDestroy(pAutomaton);
	}
	return bOk;
}



/* 依次执行契约、边界、故障注入和差分测试。 */
int main(void)
{
	int bOk = 1;

#if defined(XRT_FEATURE_MEMORY_DEBUG)
	bOk &= check(xrtMemDebugEnable(true), "cannot enable XRT memory debugger");
#endif
	bOk &= test_search_and_lifetime();
	bOk &= test_non_overlapping_and_transform();
	bOk &= test_embedded_zero();
	bOk &= test_incremental_scan();
	bOk &= test_mask();
	bOk &= test_errors_and_limits();
	bOk &= test_build_retry();
	bOk &= test_random_differential();
	xrtClearError();
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	{
		xmemdebugsnapshot Snapshot;

		xrtMemDebugSnapshot(&Snapshot);
		bOk &= check(Snapshot.LiveCount == 0u &&
			Snapshot.OverflowCount == 0u && Snapshot.UnderflowCount == 0u &&
			Snapshot.DoubleFreeCount == 0u &&
			Snapshot.InvalidFreeCount == 0u &&
			Snapshot.UseAfterFreeCount == 0u,
			"XRT memory debugger reported a memory safety failure");
		(void)xrtMemDebugEnable(false);
	}
#endif
	if ( !bOk ) { return 1; }
	puts("test_aho_corasick: ok");
	return 0;
}



/* 测试以单翻译单元编译，确保故障注入只存在于测试二进制。 */
#include "../xrt_aho_corasick.c"
