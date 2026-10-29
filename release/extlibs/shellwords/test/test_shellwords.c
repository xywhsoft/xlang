#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_shellwords.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool expect_word(xvalue* pWords, size_t iIndex, const char* sExpected)
{
	xstrview Text;
	return xrtValueGetString(xrtValueArrayGet(pWords, iIndex), &Text) &&
		Text.Size == strlen(sExpected) &&
		memcmp(Text.Data, sExpected, Text.Size) == 0;
}

static bool test_split(void)
{
	xvalue* pWords = xrtShellwordsSplit("tool 'two words' \"你好\" '' a\\ b");
	bool bOk = pWords != NULL && xrtValueCount(pWords) == 5u &&
		expect_word(pWords, 0u, "tool") &&
		expect_word(pWords, 1u, "two words") &&
		expect_word(pWords, 2u, "你好") &&
		expect_word(pWords, 3u, "") &&
		expect_word(pWords, 4u, "a b");
	xrtValueRelease(pWords);
	if ( !bOk ) { return false; }
	pWords = xrtShellwordsSplit("a\\\nb \"c\\$d\" 'e\\f'");
	bOk = pWords != NULL && xrtValueCount(pWords) == 3u &&
		expect_word(pWords, 0u, "ab") &&
		expect_word(pWords, 1u, "c$d") &&
		expect_word(pWords, 2u, "e\\f");
	xrtValueRelease(pWords);
	if ( !bOk ) { return false; }
	/* POSIX 参数按字节传递；C 接口不应擅自改写非 UTF-8 字节。 */
	pWords = xrtShellwordsSplit("raw\x80\xff");
	bOk = pWords != NULL && xrtValueCount(pWords) == 1u &&
		expect_word(pWords, 0u, "raw\x80\xff");
	xrtValueRelease(pWords);
	return bOk;
}

static bool test_large(void)
{
	char* sLong = (char*)malloc(12001u);
	xvalue* pWords;
	xvalue* pJoinedWords;
	str sQuoted;
	str sWindowsLong;
	str sJoined;
	size_t i;
	bool bOk = false;
	if ( sLong == NULL ) { return false; }
	memset(sLong, 'x', 12000u);
	sLong[12000] = 0;
	sQuoted = xrtShellwordsQuote(sLong);
	sWindowsLong = xrtShellwordsQuoteWindows(sLong);
	pWords = xrtShellwordsSplit(sLong);
	if ( sQuoted == NULL || sWindowsLong == NULL ||
		strlen(sQuoted) != 12000u || strlen(sWindowsLong) != 12000u ||
		pWords == NULL || !expect_word(pWords, 0u, sLong) ) { goto cleanup; }
	pJoinedWords = xrtValueArray();
	if ( pJoinedWords == NULL ) { goto cleanup; }
	{
		xvalue* pItem = xrtValueString((xstrview){ sLong, 12000u });
		if ( pItem == NULL || !xrtValueArrayAppendTake(pJoinedWords, &pItem) ) {
			xrtValueRelease(pItem);
			xrtValueRelease(pJoinedWords);
			goto cleanup;
		}
	}
	for ( i = 0u; i < 600u; ++i ) {
		xvalue* pItem = xrtValueString((xstrview){ "x", 1u });
		if ( pItem == NULL || !xrtValueArrayAppendTake(pJoinedWords, &pItem) ) {
			xrtValueRelease(pItem);
			xrtValueRelease(pJoinedWords);
			goto cleanup;
		}
	}
	sJoined = xrtShellwordsJoin(pJoinedWords);
	xrtValueRelease(pJoinedWords);
	if ( sJoined == NULL ) { goto cleanup; }
	xrtValueRelease(pWords);
	pWords = xrtShellwordsSplit(sJoined);
	bOk = pWords != NULL && xrtValueCount(pWords) == 601u &&
		expect_word(pWords, 0u, sLong) &&
		expect_word(pWords, 600u, "x");
	xrtFree(sJoined);
cleanup:
	free(sLong);
	xrtFree(sQuoted);
	xrtFree(sWindowsLong);
	xrtValueRelease(pWords);
	return bOk;
}

static bool test_quotes(void)
{
	str sFirst = xrtShellwordsQuote("a b");
	str sSecond = xrtShellwordsQuote("it's");
	str sAssignment = xrtShellwordsQuote("A=B");
	str sAppendAssignment = xrtShellwordsQuote("A+=B");
	str sWindows = xrtShellwordsQuoteWindows("a b\\");
	str sWindowsQuote = xrtShellwordsQuoteWindows("a\\\"b");
	bool bOk = sFirst != NULL && sSecond != NULL &&
		sAssignment != NULL && sAppendAssignment != NULL &&
		sWindows != NULL && sWindowsQuote != NULL &&
		strcmp(sFirst, "'a b'") == 0 &&
		strcmp(sSecond, "'it'\\''s'") == 0 &&
		strcmp(sAssignment, "'A=B'") == 0 &&
		strcmp(sAppendAssignment, "'A+=B'") == 0 &&
		strcmp(sWindows, "\"a b\\\\\"") == 0 &&
		strcmp(sWindowsQuote, "\"a\\\\\\\"b\"") == 0;
	xrtFree(sFirst);
	xrtFree(sSecond);
	xrtFree(sAssignment);
	xrtFree(sAppendAssignment);
	xrtFree(sWindows);
	xrtFree(sWindowsQuote);
	return bOk;
}

static bool test_reserved_words(void)
{
	static const char* const Words[] = {
		"case", "do", "done", "elif", "else", "esac", "fi", "for",
		"if", "in", "then", "until", "while", "function", "namespace",
		"select", "time", "coproc", "label:"
	};
	size_t i;
	for ( i = 0u; i < sizeof(Words) / sizeof(Words[0]); ++i ) {
		size_t iSize = strlen(Words[i]);
		str sQuoted = xrtShellwordsQuote(Words[i]);
		bool bOk = sQuoted != NULL && strlen(sQuoted) == iSize + 2u &&
			sQuoted[0] == '\'' && sQuoted[iSize + 1u] == '\'' &&
			memcmp(sQuoted + 1, Words[i], iSize) == 0;
		xrtFree(sQuoted);
		if ( !bOk ) {
			fprintf(stderr, "shell reserved word was not quoted: %s\n", Words[i]);
			return false;
		}
	}
	return true;
}

static bool test_quote_roundtrip(void)
{
	static const char* sUnicode[] = {
		"", "你好 世界", "it's 中文", "cafe\xCC\x81", "🙂\t参数"
	};
	unsigned int i;
	for ( i = 1u; i < 128u; ++i ) {
		char sWord[2] = { (char)i, 0 };
		str sQuoted = xrtShellwordsQuote(sWord);
		xvalue* pWords = sQuoted != NULL ? xrtShellwordsSplit(sQuoted) : NULL;
		bool bOk = pWords != NULL && xrtValueCount(pWords) == 1u &&
			expect_word(pWords, 0u, sWord);
		xrtValueRelease(pWords);
		xrtFree(sQuoted);
		if ( !bOk ) {
			fprintf(stderr, "POSIX quote roundtrip failed for byte %u\n", i);
			return false;
		}
	}
	for ( i = 0u; i < sizeof(sUnicode) / sizeof(sUnicode[0]); ++i ) {
		str sQuoted = xrtShellwordsQuote(sUnicode[i]);
		xvalue* pWords = sQuoted != NULL ? xrtShellwordsSplit(sQuoted) : NULL;
		bool bOk = pWords != NULL && xrtValueCount(pWords) == 1u &&
			expect_word(pWords, 0u, sUnicode[i]);
		xrtValueRelease(pWords);
		xrtFree(sQuoted);
		if ( !bOk ) { return false; }
	}
	return true;
}

/* 邻接的引号、反斜杠与空白会切换不同的词法状态。 */
static bool test_quote_pairs(void)
{
	static const char* Pieces[] = {
		"'", "\"", "\\", " ", "\t", "\n", "$", "`", "#", ";", "x", "你", "🙂"
	};
	size_t i, j;
	for ( i = 0u; i < sizeof(Pieces) / sizeof(Pieces[0]); ++i ) {
		for ( j = 0u; j < sizeof(Pieces) / sizeof(Pieces[0]); ++j ) {
			char sWord[16];
			size_t iFirst = strlen(Pieces[i]);
			size_t iSecond = strlen(Pieces[j]);
			str sQuoted;
			xvalue* pWords;
			bool bOk;
			if ( iFirst + iSecond >= sizeof(sWord) ) { return false; }
			memcpy(sWord, Pieces[i], iFirst);
			memcpy(sWord + iFirst, Pieces[j], iSecond + 1u);
			sQuoted = xrtShellwordsQuote(sWord);
			pWords = sQuoted != NULL ? xrtShellwordsSplit(sQuoted) : NULL;
			bOk = pWords != NULL && xrtValueCount(pWords) == 1u &&
				expect_word(pWords, 0u, sWord);
			xrtValueRelease(pWords);
			xrtFree(sQuoted);
			if ( !bOk ) {
				fprintf(stderr, "POSIX quote pair failed at %zu,%zu\n", i, j);
				return false;
			}
		}
	}
	return true;
}

#ifndef _WIN32
/* 用真实 POSIX shell 核对编码，而非仅用本包的 split 自证往返。 */
static bool test_posix_shell_quote(void)
{
	static const char* words[] = {
		"", "plain", "a b", "it's 中文", "$HOME", "$(exit 7)",
		"line\nnext", "a\\b", "A=B", "raw\x80\xff"
	};
	size_t i;
	for ( i = 0u; i < sizeof(words) / sizeof(words[0]); ++i ) {
		char command[256];
		char output[128];
		str quoted = xrtShellwordsQuote(words[i]);
		FILE* shell;
		size_t length = strlen(words[i]);
		size_t received;
		int command_length;
		int status;
		if ( quoted == NULL ) { return false; }
		command_length = snprintf(command, sizeof(command), "printf '%%s' %s", quoted);
		if ( command_length < 0 || command_length >= (int)sizeof(command) ) {
			xrtFree(quoted);
			return false;
		}
		shell = popen(command, "r");
		xrtFree(quoted);
		if ( shell == NULL ) { return false; }
		received = fread(output, 1u, length + 1u, shell);
		status = pclose(shell);
		if ( status != 0 || received != length ||
			memcmp(output, words[i], length) != 0 ) {
			fprintf(stderr, "POSIX shell quote failed for case %zu\n", i);
			return false;
		}
	}
	return true;
}

/* 一次交给真实 shell 校验整段参数序列，避免仅由 split 自证。 */
static bool test_posix_ascii_join(void)
{
	xvalue* pWords = xrtValueArray();
	char expected[127];
	char output[128];
	char command[1024];
	str joined = NULL;
	FILE* shell = NULL;
	size_t i;
	size_t received;
	int command_length;
	int status;
	bool bOk = false;
	if ( pWords == NULL ) { return false; }
	for ( i = 1u; i < 128u; ++i ) {
		char word[2] = { (char)i, 0 };
		xvalue* pWord = xrtValueString((xstrview){ word, 1u });
		if ( pWord == NULL || !xrtValueArrayAppendTake(pWords, &pWord) ) {
			xrtValueRelease(pWord);
			goto cleanup;
		}
		expected[i - 1u] = (char)i;
	}
	joined = xrtShellwordsJoin(pWords);
	if ( joined == NULL ) { goto cleanup; }
	command_length = snprintf(command, sizeof(command), "printf '%%s' %s", joined);
	if ( command_length < 0 || command_length >= (int)sizeof(command) ) { goto cleanup; }
	shell = popen(command, "r");
	if ( shell == NULL ) { goto cleanup; }
	received = fread(output, 1u, sizeof(output), shell);
	status = pclose(shell);
	shell = NULL;
	bOk = status == 0 && received == sizeof(expected) &&
		memcmp(output, expected, sizeof(expected)) == 0;
cleanup:
	if ( shell != NULL ) { pclose(shell); }
	xrtFree(joined);
	xrtValueRelease(pWords);
	if ( !bOk ) { fputs("POSIX shell ASCII join failed\n", stderr); }
	return bOk;
}

static bool test_posix_command_words(void)
{
	static const char* const Words[] = { "if", "case", "time", "label:" };
	size_t i;
	for ( i = 0u; i < sizeof(Words) / sizeof(Words[0]); ++i ) {
		char sCommand[128];
		str sQuoted = xrtShellwordsQuote(Words[i]);
		int iLength;
		int iStatus;
		if ( sQuoted == NULL ) { return false; }
		iLength = snprintf(sCommand, sizeof(sCommand),
			"sh -n -c \"%s\" >/dev/null 2>&1", sQuoted);
		xrtFree(sQuoted);
		if ( iLength < 0 || iLength >= (int)sizeof(sCommand) ) { return false; }
		iStatus = system(sCommand);
		if ( iStatus != 0 ) {
			fprintf(stderr, "shell command word did not parse: %s\n", Words[i]);
			return false;
		}
	}
	return true;
}
#endif

static bool test_errors(void)
{
	xvalue* pWords = xrtShellwordsSplit("a 'broken");
	str sRecovered;
	if ( pWords == NULL || xrtValueType(pWords) != XVALUE_NULL ||
		xrtGetError() == NULL ) { xrtValueRelease(pWords); return false; }
	xrtValueRelease(pWords);
	pWords = xrtShellwordsSplit("recovered");
	if ( pWords == NULL || !expect_word(pWords, 0u, "recovered") ||
		xrtGetError() != NULL ) { xrtValueRelease(pWords); return false; }
	xrtValueRelease(pWords);
	pWords = xrtShellwordsSplit("a\\");
	if ( pWords == NULL || xrtValueType(pWords) != XVALUE_NULL ||
		xrtGetError() == NULL ) { xrtValueRelease(pWords); return false; }
	xrtValueRelease(pWords);
	xrtClearError();
	pWords = xrtValueArray();
	if ( pWords == NULL ) { return false; }
	{
		xvalue* pBad = xrtValueInt(7);
		str sOutput;
		if ( pBad == NULL || !xrtValueArrayAppendTake(pWords, &pBad) ) {
			xrtValueRelease(pBad);
			xrtValueRelease(pWords);
			return false;
		}
		sOutput = xrtShellwordsJoin(pWords);
		if ( sOutput != NULL || xrtGetError() == NULL ) {
			xrtFree(sOutput);
			xrtValueRelease(pWords);
			return false;
		}
	}
	xrtValueRelease(pWords);
	sRecovered = xrtShellwordsQuote("recovered");
	if ( sRecovered == NULL || strcmp(sRecovered, "recovered") != 0 ||
		xrtGetError() != NULL ) { xrtFree(sRecovered); return false; }
	xrtFree(sRecovered);
	sRecovered = xrtShellwordsQuote(NULL);
	if ( sRecovered != NULL || xrtGetError() == NULL ) {
		xrtFree(sRecovered);
		return false;
	}
	sRecovered = xrtShellwordsQuoteWindows("recovered");
	if ( sRecovered == NULL || strcmp(sRecovered, "recovered") != 0 ||
		xrtGetError() != NULL ) { xrtFree(sRecovered); return false; }
	xrtFree(sRecovered);
	return true;
}

/* 逐点故障只覆盖本包的动态结果路径，失败后同一入口必须可恢复。 */
static bool test_alloc_failures(void)
{
	xvalue* pItems = xrtValueArray();
	size_t i;
	bool bSplitDone = false;
	bool bJoinDone = false;
	bool bSplitFailed = false;
	bool bJoinFailed = false;
	if ( pItems == NULL ) { return false; }
	{
		xvalue* pItem = xrtValueString((xstrview){ "a'b", 3u });
		if ( pItem == NULL || !xrtValueArrayAppendTake(pItems, &pItem) ) {
			xrtValueRelease(pItem);
			xrtValueRelease(pItems);
			return false;
		}
	}
	for ( i = 0u; i < 64u; ++i ) {
		xvalue* pResult;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(i) ) { break; }
		pResult = xrtShellwordsSplit("first 'two words' third");
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			bSplitFailed = true;
			if ( pResult == NULL || xrtValueType(pResult) != XVALUE_NULL ||
				xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
				xrtValueRelease(pResult);
				break;
			}
		} else {
			bSplitDone = pResult != NULL && xrtValueCount(pResult) == 3u &&
				expect_word(pResult, 1u, "two words") && xrtGetError() == NULL;
		}
		xrtValueRelease(pResult);
		if ( !bTriggered ) { break; }
	}
	for ( i = 0u; i < 64u; ++i ) {
		str sResult;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(i) ) { break; }
		sResult = xrtShellwordsJoin(pItems);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			bJoinFailed = true;
			if ( sResult != NULL ||
				xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
				xrtFree(sResult);
				break;
			}
		} else {
			bJoinDone = sResult != NULL && strcmp(sResult, "'a'\\''b'") == 0 &&
				xrtGetError() == NULL;
		}
		xrtFree(sResult);
		if ( !bTriggered ) { break; }
	}
	xrtValueRelease(pItems);
	return bSplitDone && bJoinDone && bSplitFailed && bJoinFailed;
}

static bool test_quote_alloc_failures(str (*pQuote)(cstr), cstr sWord)
{
	size_t i;
	bool bFailed = false;
	for ( i = 0u; i < 64u; ++i ) {
		str sResult;
		bool bTriggered;
		if ( !xrtMemDebugFailAfter(i) ) { return false; }
		sResult = pQuote(sWord);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bTriggered ) {
			bFailed = true;
			if ( sResult != NULL ||
				xrtErrorKind(xrtGetError()) != XERR_MEMORY ) {
				xrtFree(sResult);
				return false;
			}
		} else {
			bool bOk = bFailed && sResult != NULL && xrtGetError() == NULL;
			xrtFree(sResult);
			return bOk;
		}
		xrtFree(sResult);
	}
	return false;
}

int main(void)
{
	bool bOk = test_split() && test_large() && test_quotes() &&
		test_reserved_words() &&
		test_quote_roundtrip() && test_quote_pairs() && test_errors();
#ifndef _WIN32
	bOk = bOk && test_posix_shell_quote() && test_posix_ascii_join() &&
		test_posix_command_words();
#endif
	if ( !bOk || !test_alloc_failures() ||
		!test_quote_alloc_failures(xrtShellwordsQuote, "a'b") ||
		!test_quote_alloc_failures(xrtShellwordsQuoteWindows, "a b\\") ) {
		fputs("test_shellwords: FAIL\n", stderr);
		return 1;
	}
	puts("test_shellwords: PASS");
	return 0;
}
