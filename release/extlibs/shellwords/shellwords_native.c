#include "include/xrt_shellwords.h"
#include <string.h>

#define XSW_DOMAIN "xlang.extlib.shellwords"

static void __xswError(const char* sMessage)
{
	xrtSetErrorInfo(XERR_VALUE, XSW_DOMAIN, 0, sMessage);
}

static bool __xswPush(xvalue* pWords, xbuffer* pWord)
{
	size_t iSize = pWord->Size;
	str sOwned = (str)xrtBufferTake(pWord, NULL, NULL);
	xvalue* pText = xrtValueStringTake(&sOwned, iSize);
	if ( pText == NULL ) {
		xrtFree(sOwned);
		return false;
	}
	if ( !xrtValueArrayAppendTake(pWords, &pText) ) {
		xrtValueRelease(pText);
		return false;
	}
	return true;
}

static bool __xswSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* 不执行变量展开、通配符或命令替换，只处理词法引号与转义。 */
xvalue* xrtShellwordsSplit(cstr sLine)
{
	xvalue* pWords = NULL;
	xbuffer* pWord = NULL;
	bool bStarted = false;
	char cQuote = 0;
	const char* p;
	xrtClearError();
	if ( sLine == NULL ) {
		__xswError("shellwords input is null");
		return xrtValueNull();
	}
	pWords = xrtValueArray();
	pWord = xrtBufferCreate();
	if ( pWords == NULL || pWord == NULL ) { goto fail; }
	for ( p = sLine; *p != 0; ++p ) {
		char c = *p;
		if ( cQuote == '\'' ) {
			if ( c == '\'' ) { cQuote = 0; }
			else if ( !xrtBufferAppendByte(pWord, (uint8)c) ) { goto fail; }
			continue;
		}
		if ( cQuote == '"' ) {
			if ( c == '"' ) { cQuote = 0; continue; }
			if ( c == '\\' && p[1] != 0 &&
				(p[1] == '$' || p[1] == '`' || p[1] == '"' ||
				p[1] == '\\' || p[1] == '\n') ) {
				++p;
				if ( *p == '\n' ) { continue; }
				c = *p;
			}
			if ( !xrtBufferAppendByte(pWord, (uint8)c) ) { goto fail; }
			continue;
		}
		if ( __xswSpace(c) ) {
			if ( bStarted && !__xswPush(pWords, pWord) ) { goto fail; }
			bStarted = false;
			continue;
		}
		if ( c == '\'' || c == '"' ) {
			cQuote = c;
			bStarted = true;
			continue;
		}
		if ( c == '\\' ) {
			if ( p[1] == 0 ) {
				__xswError("shellwords ends with an escape character");
				goto fail;
			}
			++p;
			if ( *p == '\n' ) { continue; }
			c = *p;
		}
		if ( !xrtBufferAppendByte(pWord, (uint8)c) ) { goto fail; }
		bStarted = true;
	}
	if ( cQuote != 0 ) {
		__xswError("shellwords has unbalanced quotes");
		goto fail;
	}
	if ( bStarted && !__xswPush(pWords, pWord) ) { goto fail; }
	xrtBufferDestroy(pWord);
	return pWords;
fail:
	xrtBufferDestroy(pWord);
	xrtValueRelease(pWords);
	return xrtValueNull();
}

static str __xswTake(xbuffer* pBuffer)
{
	str sResult;
	if ( !xrtBufferAppendByte(pBuffer, 0u) ) { return NULL; }
	sResult = (str)xrtBufferTake(pBuffer, NULL, NULL);
	return sResult;
}

static bool __xswSafe(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
		c == '/' || c == '=' || c == ':' || c == ',' || c == '+';
}

/* 命令位置的保留词即使只含安全字符，也必须保留引用标记。 */
static bool __xswReserved(const char* sWord, size_t iSize)
{
	static const char* const Words[] = {
		"case", "do", "done", "elif", "else", "esac", "fi", "for",
		"if", "in", "then", "until", "while", "function", "namespace",
		"select", "time", "coproc"
	};
	size_t i;
	for ( i = 0u; i < sizeof(Words) / sizeof(Words[0]); ++i ) {
		if ( strlen(Words[i]) == iSize &&
			memcmp(Words[i], sWord, iSize) == 0 ) { return true; }
	}
	return false;
}

static bool __xswAppendQuoted(xbuffer* pOutput, const char* sWord, size_t iSize)
{
	size_t i;
	bool bSafe;
	bool bName;
	bSafe = iSize != 0u && sWord[iSize - 1u] != ':' &&
		!__xswReserved(sWord, iSize);
	bName = iSize != 0u &&
		((sWord[0] >= 'a' && sWord[0] <= 'z') ||
		 (sWord[0] >= 'A' && sWord[0] <= 'Z') || sWord[0] == '_');
	for ( i = 0u; i < iSize; ++i ) {
		if ( !__xswSafe(sWord[i]) ) { bSafe = false; break; }
		/* POSIX 的 NAME=value 和 Bash 的 NAME+=value 都不是普通参数。 */
		if ( bName && (sWord[i] == '=' ||
			(sWord[i] == '+' && i + 1u < iSize && sWord[i + 1u] == '=')) ) {
			bSafe = false;
			break;
		}
		bName = bName &&
			((sWord[i] >= 'a' && sWord[i] <= 'z') ||
			 (sWord[i] >= 'A' && sWord[i] <= 'Z') ||
			 (sWord[i] >= '0' && sWord[i] <= '9') || sWord[i] == '_');
	}
	if ( bSafe ) {
		if ( !xrtBufferAppend(pOutput, (xbytesview){
			(cbytes)sWord, iSize }) ) {
			return false;
		}
	} else {
		size_t iStart = 0u;
		if ( !xrtBufferAppendByte(pOutput, '\'') ) { return false; }
		/* 连续正文一次追加，只在单引号处插入转义，避免逐字节扩容与调用。 */
		for ( i = 0u; i < iSize; ++i ) {
			if ( sWord[i] == '\'' ) {
				if ( i > iStart && !xrtBufferAppend(pOutput,
					(xbytesview){ (cbytes)(sWord + iStart), i - iStart }) ) { return false; }
				if ( !xrtBufferAppend(pOutput, (xbytesview){
					(cbytes)"'\\''", 4u }) ) {
					return false;
				}
				iStart = i + 1u;
			}
		}
		if ( iStart < iSize && !xrtBufferAppend(pOutput,
			(xbytesview){ (cbytes)(sWord + iStart), iSize - iStart }) ) { return false; }
		if ( !xrtBufferAppendByte(pOutput, '\'') ) { return false; }
	}
	return true;
}

static str __xswQuote(const char* sWord, size_t iSize)
{
	xbuffer* pOutput = xrtBufferCreate();
	str sResult = NULL;
	if ( pOutput == NULL ) { return NULL; }
	if ( __xswAppendQuoted(pOutput, sWord, iSize) ) {
		sResult = __xswTake(pOutput);
	}
	xrtBufferDestroy(pOutput);
	return sResult;
}

str xrtShellwordsQuote(cstr sWord)
{
	xrtClearError();
	if ( sWord == NULL ) { __xswError("shellwords word is null"); return NULL; }
	return __xswQuote(sWord, strlen(sWord));
}

static bool __xswSlashes(xbuffer* pOutput, size_t iCount)
{
	while ( iCount-- != 0u ) {
		if ( !xrtBufferAppendByte(pOutput, '\\') ) { return false; }
	}
	return true;
}

/* 仅编码 MSVCRT argv 规则；不负责 cmd.exe 的元字符转义。 */
str xrtShellwordsQuoteWindows(cstr sWord)
{
	xbuffer* pOutput;
	const char* p;
	bool bQuote;
	size_t iSlashes = 0u;
	str sResult = NULL;
	xrtClearError();
	if ( sWord == NULL ) { __xswError("shellwords word is null"); return NULL; }
	bQuote = sWord[0] == 0 || strpbrk(sWord, " \t\r\n\"") != NULL;
	pOutput = xrtBufferCreate();
	if ( pOutput == NULL ) { return NULL; }
	if ( !bQuote ) {
		if ( !xrtBufferAppend(pOutput, (xbytesview){
			(cbytes)sWord, strlen(sWord) }) ) {
			goto cleanup;
		}
		sResult = __xswTake(pOutput);
		goto cleanup;
	}
	if ( !xrtBufferAppendByte(pOutput, '"') ) { goto cleanup; }
	for ( p = sWord; *p != 0; ++p ) {
		if ( *p == '\\' ) { ++iSlashes; continue; }
		if ( *p == '"' ) {
			if ( iSlashes > (SIZE_MAX - 1u) / 2u ||
				!__xswSlashes(pOutput, iSlashes * 2u + 1u) ||
				!xrtBufferAppendByte(pOutput, '"') ) { goto cleanup; }
		} else if ( !__xswSlashes(pOutput, iSlashes) ||
			!xrtBufferAppendByte(pOutput, (uint8)*p) ) { goto cleanup; }
		iSlashes = 0u;
	}
	if ( iSlashes > SIZE_MAX / 2u ||
		!__xswSlashes(pOutput, iSlashes * 2u) ||
		!xrtBufferAppendByte(pOutput, '"') ) { goto cleanup; }
	sResult = __xswTake(pOutput);
cleanup:
	xrtBufferDestroy(pOutput);
	return sResult;
}

str xrtShellwordsJoin(const xvalue* pWords)
{
	xbuffer* pOutput;
	size_t i;
	str sResult = NULL;
	xrtClearError();
	if ( xrtValueType(pWords) != XVALUE_ARRAY ) {
		__xswError("shellwords join requires an array");
		return NULL;
	}
	pOutput = xrtBufferCreate();
	if ( pOutput == NULL ) { return NULL; }
	for ( i = 0u; i < xrtValueCount(pWords); ++i ) {
		xstrview Text;
		xvalue* pValue = xrtValueArrayGet(pWords, i);
		if ( !xrtValueGetString(pValue, &Text) ||
			memchr(Text.Data, 0, Text.Size) != NULL ) {
			__xswError("shellwords join entries must be strings without NUL");
			goto cleanup;
		}
		if ( (i != 0u && !xrtBufferAppendByte(pOutput, ' ')) ||
			!__xswAppendQuoted(pOutput, Text.Data, Text.Size) ) {
			goto cleanup;
		}
	}
	sResult = __xswTake(pOutput);
cleanup:
	xrtBufferDestroy(pOutput);
	return sResult;
}

/* point 返回值由 XLang 的 take_cstr 接管；C 调用方使用上述 xrt API。 */
void* xsw_quote(cstr sWord) { return xrtShellwordsQuote(sWord); }
void* xsw_quote_windows(cstr sWord) { return xrtShellwordsQuoteWindows(sWord); }
void* xsw_join(xvalue* pWords) { return xrtShellwordsJoin(pWords); }
