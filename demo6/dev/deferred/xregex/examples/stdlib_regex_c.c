/* xregex 扩展包完成前保留的 C 对照示例。 */
#define XREGEX_MODULE_XREGEX
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 从零结尾字符串构造明确长度的借用视图。 */
static xstrview text_view(const char* sText)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = strlen(sText);
	return Text;
}



/* 判断借用视图是否等于零结尾字符串。 */
static bool text_equal(xstrview Text, const char* sExpected)
{
	size_t iExpectedSize = strlen(sExpected);

	return (Text.Size == iExpectedSize) &&
		((iExpectedSize == 0u) || (memcmp(Text.Data, sExpected, iExpectedSize) == 0));
}



/* 使用指定标志编译表达式。 */
static xregex* regex_compile(const char* sPattern, uint32 iFlags)
{
	xregexconfig Config;

	xrtRegexConfigInit(&Config);
	Config.Flags = iFlags;
	return xrtRegexCompileConfig(text_view(sPattern), &Config);
}



/* 输出与 XLang 示例一致的断言结果。 */
static void require_line(const char* sName, bool bPassed)
{
	if ( !bPassed ) {
		fprintf(stderr, "%s=fail\n", sName);
		exit(1);
	}
	printf("%s=ok\n", sName);
}



/* 验证便捷匹配、编译标志和三态结果。 */
static void test_basic(void)
{
	const xstrview Text = text_view("item-2026-ready");
	xregex* pRegex;
	xregexmatcher* pMatcher;
	xregexcapture Capture;
	bool bPassed = true;

	pRegex = regex_compile("^item-[0-9]+-[a-z]+$", 0u);
	bPassed = bPassed && (pRegex != NULL) &&
		(xrtRegexTest(pRegex, Text) == XREGEX_MATCH) &&
		(xrtRegexFullTest(pRegex, Text) == XREGEX_MATCH);
	xrtRegexRelease(pRegex);

	pRegex = regex_compile("^item-[0-9]+-[a-z]+$", XREGEX_IGNORE_CASE);
	bPassed = bPassed && (pRegex != NULL) &&
		(xrtRegexTest(pRegex, text_view("ITEM-2026-READY")) == XREGEX_MATCH);
	xrtRegexRelease(pRegex);

	pRegex = xrtRegexCompile(text_view("[0-9]+"));
	pMatcher = xrtRegexMatcherCreate(pRegex);
	bPassed = bPassed && (pMatcher != NULL) &&
		(xrtRegexMatcherFind(pMatcher, Text, 0u) == XREGEX_MATCH) &&
		xrtRegexMatcherCapture(pMatcher, 0u, &Capture) &&
		(Capture.Span.Begin == 5u) && (Capture.Span.End == 9u);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);

	pRegex = regex_compile("a+", XREGEX_UNGREEDY);
	bPassed = bPassed && (pRegex != NULL) &&
		(xrtRegexTest(pRegex, text_view("aa")) == XREGEX_MATCH);
	xrtRegexRelease(pRegex);

	pRegex = regex_compile("a.b", XREGEX_DOT_ALL);
	bPassed = bPassed && (pRegex != NULL) &&
		(xrtRegexFullTest(pRegex, text_view("a\nb")) == XREGEX_MATCH) &&
		((XREGEX_IGNORE_CASE + XREGEX_MULTILINE +
		  XREGEX_DOT_ALL + XREGEX_UNGREEDY) >= 15);
	xrtRegexRelease(pRegex);
	require_line("basic", bPassed);
}



/* 验证可复用 matcher 和未匹配状态。 */
static void test_match(void)
{
	const xstrview Text = text_view("item-2026-ready");
	xregex* pRegex = xrtRegexCompile(text_view("[0-9]+"));
	xregexmatcher* pFound = xrtRegexMatcherCreate(pRegex);
	xregexmatcher* pShifted = xrtRegexMatcherCreate(pRegex);
	xregexmatcher* pMissing = xrtRegexMatcherCreate(pRegex);
	xregexcapture Found;
	xregexcapture Shifted;
	bool bFound;
	bool bShifted;
	bool bMissing;

	bFound = (pFound != NULL) &&
		(xrtRegexMatcherFind(pFound, Text, 0u) == XREGEX_MATCH) &&
		xrtRegexMatcherCapture(pFound, 0u, &Found);
	require_line("match",
		bFound && Found.Matched &&
		(Found.Span.Begin == 5u) && (Found.Span.End == 9u) &&
		text_equal(Found.Text, "2026"));

	bShifted = (pShifted != NULL) &&
		(xrtRegexMatcherFind(pShifted, text_view("a1 b2"), 2u) == XREGEX_MATCH) &&
		xrtRegexMatcherCapture(pShifted, 0u, &Shifted);
	bMissing = (pMissing != NULL) &&
		(xrtRegexMatcherFind(pMissing, text_view("missing"), 0u) == XREGEX_NONE) &&
		!xrtRegexMatcherMatched(pMissing);
	require_line("lazy",
		bShifted && Shifted.Matched &&
		(Shifted.Span.Begin == 4u) && text_equal(Shifted.Text, "2") &&
		bMissing);

	xrtRegexMatcherFree(pFound);
	xrtRegexMatcherFree(pShifted);
	xrtRegexMatcherFree(pMissing);
	xrtRegexRelease(pRegex);
}



/* 验证批量编译、命中索引和集合引用语义。 */
static xregexset* test_set(void)
{
	const xstrview Patterns[] = {
		XRT_STR_LITERAL("[0-9]+"),
		XRT_STR_LITERAL("ready")
	};
	xregexconfig Config;
	xregexset* pSet;
	xregexsetmatcher* pMatcher;
	bool bPassed;

	xrtRegexConfigInit(&Config);
	Config.Flags = XREGEX_IGNORE_CASE;
	pSet = xrtRegexSetCompileConfig(Patterns, 2u, &Config);
	pMatcher = xrtRegexSetMatcherCreate(pSet);

	bPassed = (pSet != NULL) && (pMatcher != NULL) &&
		(xrtRegexSetTest(pSet, text_view("READY")) == XREGEX_MATCH) &&
		(xrtRegexSetMatcherMatch(pMatcher, text_view("READY"), 0u) == XREGEX_MATCH) &&
		(xrtRegexSetMatcherFirst(pMatcher) == 1u) &&
		(xrtRegexSetMatcherMatch(pMatcher, text_view("code 2026"), 0u) == XREGEX_MATCH) &&
		(xrtRegexSetMatcherFirst(pMatcher) == 0u) &&
		(xrtRegexSetMatcherMatch(pMatcher, text_view("READY 2026"), 0u) == XREGEX_MATCH) &&
		(xrtRegexSetMatcherCount(pMatcher) == 2u) &&
		xrtRegexSetMatcherMatched(pMatcher, 0u) &&
		xrtRegexSetMatcherMatched(pMatcher, 1u);
	require_line("set", bPassed);

	xrtRegexSetMatcherFree(pMatcher);
	return pSet;
}



/* 验证命名捕获、UTF-8 字节范围和捕获元数据。 */
static xregex* test_capture(void)
{
	xregex* pRegex = regex_compile(
		"(?<word>[a-z]+)-([0-9]+)?",
		XREGEX_MULTILINE);
	xregexmatcher* pMatcher = xrtRegexMatcherCreate(pRegex);
	xregexcapture Capture;
	xstrview Name;
	bool bPassed;

	bPassed = (pRegex != NULL) && (pMatcher != NULL) &&
		(xrtRegexCaptureCount(pRegex) == 3u) &&
		xrtRegexCaptureName(pRegex, 1u, &Name) && text_equal(Name, "word") &&
		(xrtRegexCaptureIndex(pRegex, text_view("word")) == 1u) &&
		(xrtRegexMatcherFind(pMatcher, text_view("你abc-42好"), 0u) == XREGEX_MATCH) &&
		xrtRegexMatcherCaptureNamed(pMatcher, text_view("word"), &Capture) &&
		Capture.Matched && text_equal(Capture.Text, "abc") &&
		(Capture.Span.Begin == 3u) && (Capture.Span.End == 6u);
	require_line("capture", bPassed);

	xrtRegexMatcherFree(pMatcher);
	return pRegex;
}



/* 验证编译诊断、可选捕获和不可变对象共享。 */
static void test_diagnostic(xregex* pRegex, xregexset* pSet)
{
	xregexmatcher* pOptional = xrtRegexMatcherCreate(pRegex);
	xregexmatcher* pCloneMatcher;
	xregexcapture Optional;
	xregex* pRegexRef = xrtRegexRef(pRegex);
	xregexset* pSetRef = xrtRegexSetRef(pSet);
	xregex* pInvalid;
	const xerror* pError;
	size_t iErrorOffset = XRT_NPOS;
	bool bPassed;

	bPassed = (pOptional != NULL) &&
		(xrtRegexMatcherFind(pOptional, text_view("abc-"), 0u) == XREGEX_MATCH) &&
		xrtRegexMatcherCapture(pOptional, 2u, &Optional) && !Optional.Matched &&
		(Optional.Text.Data == NULL) && (Optional.Text.Size == 0u);

	xrtClearError();
	pInvalid = xrtRegexCompile(text_view("["));
	pError = xrtGetError();
	bPassed = bPassed && (pInvalid == NULL) && (pError != NULL) &&
		(xrtErrorMessage(pError) != NULL) && (xrtErrorMessage(pError)[0] != '\0') &&
		xrtRegexErrorOffset(pError, &iErrorOffset) && (iErrorOffset != XRT_NPOS);

	pCloneMatcher = xrtRegexMatcherCreate(pRegexRef);
	bPassed = bPassed && (pRegexRef == pRegex) && (pSetRef == pSet) &&
		(pCloneMatcher != NULL) &&
		(xrtRegexMatcherFind(pCloneMatcher, text_view("copy-7"), 0u) == XREGEX_MATCH) &&
		(xrtRegexSetCount(pSetRef) == 2u);
	require_line("diagnostic", bPassed);

	xrtRegexMatcherFree(pOptional);
	xrtRegexMatcherFree(pCloneMatcher);
	xrtRegexRelease(pInvalid);
	xrtRegexRelease(pRegexRef);
	xrtRegexSetRelease(pSetRef);
}



/* 运行与 XLang Regex 示例相同的契约检查。 */
int main(void)
{
	xregexset* pSet;
	xregex* pRegex;

	test_basic();
	test_match();
	pSet = test_set();
	pRegex = test_capture();
	test_diagnostic(pRegex, pSet);
	xrtRegexRelease(pRegex);
	xrtRegexSetRelease(pSet);
	return 0;
}
