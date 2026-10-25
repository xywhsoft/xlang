/*
    CSS 受限语法查询器。只解析规则和声明的边界，不实现浏览器层叠或选择器引擎；
    注释、引号、转义、括号和嵌套 at-rule 均按语法跳过。
*/
#include <string.h>

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/css_native.h"

#define XCSS_DOMAIN "xlang.extlib.css"
#define XCSS_MAX_NESTING 64u
#define XCSS_MAX_GROUP_NESTING 64u

typedef struct xcssinput {
    cstr Data;
    size_t Size;
} xcssinput;

typedef struct xcssspan {
    size_t Begin;
    size_t End;
} xcssspan;

typedef bool (*xcssrulevisit)(xcssinput Input, xcssspan Selector,
    xcssspan Body, void* pData);

static bool xcss_declaration_count(xcssinput Input, xcssspan Body, size_t* pCount);
static bool xcss_skip_comment(xcssinput Input, size_t* pPos);

static void xcss_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XCSS_DOMAIN, 0, sMessage);
}



static xcssinput xcss_input(cstr sText)
{
    xcssinput Input;

    Input.Data = sText;
    Input.Size = sText != NULL ? strlen(sText) : 0u;
    return Input;
}

static bool xcss_utf8(xcssinput Input)
{
    if (Input.Data == NULL) {
        xcss_error(XERR_ARGUMENT, "CSS input pointer is null");
        return false;
    }
    if (xrtUtf8Valid((xstrview){Input.Data, Input.Size}, NULL)) return true;
    xcss_error(XERR_VALUE, "CSS text must be valid UTF-8");
    return false;
}



static bool xcss_space(char cValue)
{
    return cValue == ' ' || cValue == '\t' || cValue == '\r' || cValue == '\n' ||
        cValue == '\f';
}



static xcssspan xcss_trim(xcssinput Input, xcssspan Span)
{
    while (Span.Begin < Span.End && xcss_space(Input.Data[Span.Begin])) ++Span.Begin;
    while (Span.End > Span.Begin && xcss_space(Input.Data[Span.End - 1u])) --Span.End;
    return Span;
}



/* 注释只能位于名称 token 两侧；夹在名称中间不能拼成另一个标识符。 */
static bool xcss_skip_name_trivia(xcssinput Input, xcssspan Span, size_t* pPos)
{
    while (*pPos < Span.End) {
        if (xcss_space(Input.Data[*pPos])) {
            ++*pPos;
        } else if (Input.Data[*pPos] == '/' && *pPos + 1u < Span.End &&
                   Input.Data[*pPos + 1u] == '*') {
            if (!xcss_skip_comment(Input, pPos) || *pPos > Span.End) return false;
        } else {
            break;
        }
    }
    return true;
}



static bool xcss_span_cstr_equal(xcssinput Input, xcssspan Left, cstr sRight)
{
    size_t iSize = sRight != NULL ? strlen(sRight) : 0u;
    size_t i = Left.Begin;

    if (!xcss_skip_name_trivia(Input, Left, &i) || iSize > Left.End - i ||
        (iSize != 0u && memcmp(Input.Data + i, sRight, iSize) != 0)) return false;
    i += iSize;
    return xcss_skip_name_trivia(Input, Left, &i) && i == Left.End;
}



static bool xcss_skip_comment(xcssinput Input, size_t* pPos)
{
    size_t i = *pPos;

    if (i + 1u >= Input.Size || Input.Data[i] != '/' || Input.Data[i + 1u] != '*') {
        return true;
    }
    i += 2u;
    while (i + 1u < Input.Size &&
           !(Input.Data[i] == '*' && Input.Data[i + 1u] == '/')) {
        ++i;
    }
    if (i + 1u >= Input.Size) {
        xcss_error(XERR_VALUE, "CSS comment is not terminated");
        return false;
    }
    *pPos = i + 2u;
    return true;
}



static bool xcss_skip_space(xcssinput Input, size_t* pPos)
{
    while (*pPos < Input.Size) {
        if (xcss_space(Input.Data[*pPos])) {
            ++*pPos;
        } else if (Input.Data[*pPos] == '/' && *pPos + 1u < Input.Size &&
                   Input.Data[*pPos + 1u] == '*') {
            if (!xcss_skip_comment(Input, pPos)) return false;
        } else {
            break;
        }
    }
    return true;
}



static bool xcss_skip_string(xcssinput Input, size_t* pPos)
{
    char Quote = Input.Data[*pPos];

    ++*pPos;
    while (*pPos < Input.Size) {
        char cValue = Input.Data[*pPos];

        if (cValue == '\\') {
            ++*pPos;
            /* CSS 将反斜杠后的 CRLF 视为一次续行。 */
            if (*pPos < Input.Size && Input.Data[*pPos] == '\r' &&
                *pPos + 1u < Input.Size && Input.Data[*pPos + 1u] == '\n') {
                *pPos += 2u;
            } else if (*pPos < Input.Size) {
                ++*pPos;
            }
        } else if (cValue == '\r' || cValue == '\n' || cValue == '\f') {
            xcss_error(XERR_VALUE, "CSS string contains an unescaped newline");
            return false;
        } else if (cValue == Quote) {
            ++*pPos;
            return true;
        } else {
            ++*pPos;
        }
    }
    xcss_error(XERR_VALUE, "CSS string is not terminated");
    return false;
}



/* 只在字符串、注释和括号之外查找分隔符，并验证括号闭合顺序。 */
static bool xcss_next_delimiter(xcssinput Input, size_t Begin, size_t End,
    cstr sDelimiters, size_t* pPos, char* pDelimiter)
{
    size_t i = Begin;
    char Stack[XCSS_MAX_GROUP_NESTING];
    size_t iDepth = 0u;

    while (i < End) {
        char cValue = Input.Data[i];

        if (cValue == '/' && i + 1u < End && Input.Data[i + 1u] == '*') {
            if (!xcss_skip_comment(Input, &i)) return false;
            continue;
        }
        if (cValue == '\'' || cValue == '"') {
            if (!xcss_skip_string(Input, &i)) return false;
            continue;
        }
        if (cValue == '\\') {
            i += i + 1u < End ? 2u : 1u;
            continue;
        }
        if (cValue == '(' || cValue == '[') {
            if (iDepth == XCSS_MAX_GROUP_NESTING) {
                xcss_error(XERR_RANGE, "CSS group nesting exceeds 64 levels");
                return false;
            }
            Stack[iDepth++] = cValue;
        } else if (cValue == ')' || cValue == ']') {
            if (iDepth == 0u || Stack[iDepth - 1u] != (cValue == ')' ? '(' : '[')) {
                xcss_error(XERR_VALUE, "CSS has mismatched () or [] grouping");
                return false;
            }
            --iDepth;
        } else if (iDepth == 0u && strchr(sDelimiters, cValue) != NULL) {
            *pPos = i;
            *pDelimiter = cValue;
            return true;
        }
        ++i;
    }
    if (iDepth != 0u) {
        xcss_error(XERR_VALUE, "CSS has an unclosed () or [] expression");
        return false;
    }
    *pPos = End;
    *pDelimiter = 0;
    return true;
}



static bool xcss_block_end(xcssinput Input, size_t Begin, size_t End, size_t* pEnd)
{
    size_t i = Begin;
    size_t iDepth = 1u;

    while (i < End) {
        char cValue = Input.Data[i];

        if (cValue == '/' && i + 1u < End && Input.Data[i + 1u] == '*') {
            if (!xcss_skip_comment(Input, &i)) return false;
            continue;
        }
        if (cValue == '\'' || cValue == '"') {
            if (!xcss_skip_string(Input, &i)) return false;
            continue;
        }
        if (cValue == '\\') {
            i += i + 1u < End ? 2u : 1u;
            continue;
        }
        if (cValue == '{') ++iDepth;
        else if (cValue == '}') {
            --iDepth;
            if (iDepth == 0u) {
                *pEnd = i;
                return true;
            }
        }
        ++i;
    }
    xcss_error(XERR_VALUE, "CSS block is not terminated");
    return false;
}



static bool xcss_block_has_rule(xcssinput Input, xcssspan Body, bool* pResult)
{
    size_t iDelimiter;
    char Delimiter;

    if (!xcss_next_delimiter(Input, Body.Begin, Body.End, "{", &iDelimiter, &Delimiter)) {
        return false;
    }
    *pResult = Delimiter == '{';
    return true;
}



static bool xcss_walk_rules(xcssinput Input, size_t Begin, size_t End,
    xcssrulevisit pVisit, void* pData, bool bValidateDeclarations,
    size_t iNesting)
{
    size_t i = Begin;

    if (iNesting > XCSS_MAX_NESTING) {
        xcss_error(XERR_RANGE, "CSS at-rule nesting exceeds 64 levels");
        return false;
    }

    while (i < End) {
        xcssspan Selector;
        xcssspan Body;
        size_t iDelimiter;
        size_t iBlockEnd;
        char Delimiter;

        if (!xcss_skip_space(Input, &i)) return false;
        if (i >= End) break;
        if (Input.Data[i] == '}') {
            xcss_error(XERR_VALUE, "CSS has an unmatched '}' character");
            return false;
        }
        Selector.Begin = i;
        if (!xcss_next_delimiter(Input, i, End, "{;}", &iDelimiter, &Delimiter)) return false;
        Selector.End = iDelimiter;
        Selector = xcss_trim(Input, Selector);
        if (Delimiter == 0 || Delimiter == '}') {
            xcss_error(XERR_VALUE, "CSS rule is missing an opening '{'");
            return false;
        }
        if (Delimiter == ';') {
            /* At-rules such as @charset and @import do not have a body. */
            if (Selector.Begin == Selector.End || Input.Data[Selector.Begin] != '@') {
                xcss_error(XERR_VALUE, "CSS statement outside a rule must start with '@'");
                return false;
            }
            i = iDelimiter + 1u;
            continue;
        }
        if (Selector.Begin == Selector.End) {
            xcss_error(XERR_VALUE, "CSS rule selector is empty");
            return false;
        }
        if (!xcss_block_end(Input, iDelimiter + 1u, End, &iBlockEnd)) return false;
        Body.Begin = iDelimiter + 1u;
        Body.End = iBlockEnd;
        if (Input.Data[Selector.Begin] == '@') {
            bool bNestedRules;
            size_t iCount;

            /* @media/@supports/@layer contain rules; @font-face contains declarations. */
            if (!xcss_block_has_rule(Input, Body, &bNestedRules)) return false;
            if (bNestedRules) {
                if (!xcss_walk_rules(Input, Body.Begin, Body.End, pVisit, pData,
                        bValidateDeclarations, iNesting + 1u)) return false;
            } else if (bValidateDeclarations &&
                       !xcss_declaration_count(Input, Body, &iCount)) return false;
        } else {
            size_t iCount;

            if (bValidateDeclarations &&
                !xcss_declaration_count(Input, Body, &iCount)) return false;
            if (pVisit != NULL && !pVisit(Input, Selector, Body, pData)) return false;
        }
        i = iBlockEnd + 1u;
    }
    return true;
}



typedef struct xcsscountcontext {
    size_t Count;
} xcsscountcontext;

static bool xcss_count_visit(xcssinput Input, xcssspan Selector, xcssspan Body, void* pData)
{
    xcsscountcontext* pContext = (xcsscountcontext*)pData;

    (void)Input;
    (void)Selector;
    (void)Body;
    ++pContext->Count;
    return true;
}



static bool xcss_selector_matches(xcssinput Input, xcssspan Actual, cstr sExpected)
{
    size_t i = Actual.Begin;
    size_t iPart = i;

    while (i <= Actual.End) {
        size_t iDelimiter;
        char Delimiter;

        if (!xcss_next_delimiter(Input, i, Actual.End, ",", &iDelimiter, &Delimiter)) return false;
        if (xcss_span_cstr_equal(Input, (xcssspan){ iPart, iDelimiter }, sExpected)) return true;
        if (Delimiter == 0) break;
        i = iDelimiter + 1u;
        iPart = i;
    }
    return false;
}



static bool xcss_declaration_value(xcssinput Input, xcssspan Body,
    cstr sProperty, xcssspan* pValue, bool* pFound)
{
    size_t i = Body.Begin;

    *pFound = false;
    while (i < Body.End) {
        size_t iDelimiter;
        size_t iColon;
        char Delimiter;
        xcssspan Name;
        xcssspan Value;

        if (!xcss_skip_space(Input, &i)) return false;
        if (i >= Body.End) break;
        if (!xcss_next_delimiter(Input, i, Body.End, ";", &iDelimiter, &Delimiter)) return false;
        if (!xcss_next_delimiter(Input, i, iDelimiter, ":", &iColon, &Delimiter)) return false;
        if (Delimiter == 0) {
            xcss_error(XERR_VALUE, "CSS declaration is missing ':'");
            return false;
        }
        Name = xcss_trim(Input, (xcssspan){ i, iColon });
        Value = xcss_trim(Input, (xcssspan){ iColon + 1u, iDelimiter });
        if (Name.Begin == Name.End) {
            xcss_error(XERR_VALUE, "CSS declaration property is empty");
            return false;
        }
        if (xcss_span_cstr_equal(Input, Name, sProperty)) {
            *pValue = Value;
            *pFound = true;
            return true;
        }
        i = iDelimiter < Body.End ? iDelimiter + 1u : Body.End;
    }
    return true;
}



typedef struct xcssgetcontext {
    cstr Selector;
    cstr Property;
    xcssspan Value;
    bool Found;
} xcssgetcontext;

static bool xcss_get_visit(xcssinput Input, xcssspan Selector, xcssspan Body, void* pData)
{
    xcssgetcontext* pContext = (xcssgetcontext*)pData;

    if (pContext->Found) return true;
    if (!xcss_selector_matches(Input, Selector, pContext->Selector)) return xrtGetError() == NULL;
    if (!xcss_declaration_value(Input, Body, pContext->Property,
            &pContext->Value, &pContext->Found)) {
        return false;
    }
    return true;
}



typedef struct xcssselectorcontext {
    size_t Index;
    size_t Current;
    xcssspan Result;
    bool Found;
} xcssselectorcontext;

static bool xcss_selector_visit(xcssinput Input, xcssspan Selector, xcssspan Body, void* pData)
{
    xcssselectorcontext* pContext = (xcssselectorcontext*)pData;

    (void)Input;
    (void)Body;
    if (pContext->Current++ == pContext->Index) {
        pContext->Result = Selector;
        pContext->Found = true;
    }
    return true;
}



static bool xcss_declaration_count(xcssinput Input, xcssspan Body, size_t* pCount)
{
    size_t i = Body.Begin;

    *pCount = 0u;
    while (i < Body.End) {
        size_t iDelimiter;
        size_t iColon;
        char Delimiter;
        xcssspan Name;

        if (!xcss_skip_space(Input, &i)) return false;
        if (i >= Body.End) break;
        if (!xcss_next_delimiter(Input, i, Body.End, ";", &iDelimiter, &Delimiter) ||
            !xcss_next_delimiter(Input, i, iDelimiter, ":", &iColon, &Delimiter)) return false;
        if (Delimiter == 0) {
            xcss_error(XERR_VALUE, "CSS declaration is missing ':'");
            return false;
        }
        Name = xcss_trim(Input, (xcssspan){ i, iColon });
        if (Name.Begin == Name.End) {
            xcss_error(XERR_VALUE, "CSS declaration property is empty");
            return false;
        }
        ++*pCount;
        i = iDelimiter < Body.End ? iDelimiter + 1u : Body.End;
    }
    return true;
}



typedef struct xcsspropcountcontext {
    cstr Selector;
    size_t Count;
    bool Found;
} xcsspropcountcontext;

static bool xcss_prop_count_visit(xcssinput Input, xcssspan Selector, xcssspan Body, void* pData)
{
    xcsspropcountcontext* pContext = (xcsspropcountcontext*)pData;

    if (pContext->Found) return true;
    if (!xcss_selector_matches(Input, Selector, pContext->Selector)) return xrtGetError() == NULL;
    if (!xcss_declaration_count(Input, Body, &pContext->Count)) return false;
    pContext->Found = true;
    return true;
}



static void* xcss_empty(void)
{
    char* pResult = (char*)xrtMalloc(1u);
    if (pResult == NULL) xcss_error(XERR_MEMORY, "could not allocate CSS result text");
    else pResult[0] = 0;
    return pResult;
}



static void* xcss_store(xcssinput Input, xcssspan Span)
{
    xbuffer Output;
    void* pResult;

    (void)xrtBufferInit(&Output);
    Span = xcss_trim(Input, Span);
    if (!xrtBufferAppend(&Output, (xbytesview){
            (cbytes)(Input.Data + Span.Begin), Span.End - Span.Begin }) ||
        !xrtBufferAppendByte(&Output, 0u)) goto Memory;
    pResult = xrtBufferTake(&Output, NULL, NULL);
    if (pResult != NULL) return pResult;

Memory:
    xrtBufferUnit(&Output);
    xcss_error(XERR_MEMORY, "could not allocate CSS result text");
    return NULL;
}



void* xcss_get(cstr sCss, cstr sSelector, cstr sProperty)
{
    xcssinput Input = xcss_input(sCss);
    xcssgetcontext Context;

    xrtClearError();
    Context.Selector = sSelector;
    Context.Property = sProperty;
    Context.Found = false;
    if (sSelector == NULL || sProperty == NULL || sSelector[0] == 0 || sProperty[0] == 0) {
        xcss_error(XERR_ARGUMENT, "CSS selector and property are required");
        return NULL;
    }
    if (!xcss_utf8(Input) || !xcss_utf8(xcss_input(sSelector)) ||
        !xcss_utf8(xcss_input(sProperty))) return NULL;
    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_get_visit, &Context, true, 0u)) return NULL;
    if (!Context.Found) {
        xrtClearError();
        return xcss_empty();
    }
    xrtClearError();
    return xcss_store(Input, Context.Value);
}



int64_t xcss_count_rules(cstr sCss)
{
    xcsscountcontext Context = { 0u };
    xcssinput Input = xcss_input(sCss);

    xrtClearError();
    if (!xcss_utf8(Input)) return -1;
    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_count_visit, &Context, true, 0u)) return -1;
    xrtClearError();
    return (long long)Context.Count;
}



int64_t xcss_count_props(cstr sCss, cstr sSelector)
{
    xcssinput Input = xcss_input(sCss);
    xcsspropcountcontext Context;

    xrtClearError();
    if (sSelector == NULL || sSelector[0] == 0) {
        xcss_error(XERR_ARGUMENT, "CSS selector is required");
        return -1;
    }
    if (!xcss_utf8(Input) || !xcss_utf8(xcss_input(sSelector))) return -1;
    memset(&Context, 0, sizeof(Context));
    Context.Selector = sSelector;
    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_prop_count_visit, &Context, true, 0u)) return -1;
    xrtClearError();
    return Context.Found ? (long long)Context.Count : 0;
}



void* xcss_selector_at(cstr sCss, int64_t iIndex)
{
    xcssselectorcontext Context;
    xcssinput Input = xcss_input(sCss);

    xrtClearError();
    if (iIndex < 0) {
        xcss_error(XERR_RANGE, "CSS selector index must not be negative");
        return NULL;
    }
    if (!xcss_utf8(Input)) return NULL;
    memset(&Context, 0, sizeof(Context));
    Context.Index = (size_t)iIndex;
    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_selector_visit, &Context, true, 0u)) return NULL;
    if (!Context.Found) {
        xrtClearError();
        return xcss_empty();
    }
    xrtClearError();
    return xcss_store(Input, Context.Result);
}



bool xcss_validate(cstr sCss)
{
    xcssinput Input = xcss_input(sCss);

    xrtClearError();
    if (!xcss_utf8(Input)) return false;
    if (!xcss_walk_rules(Input, 0u, Input.Size, NULL, NULL, true, 0u)) return false;
    xrtClearError();
    return true;
}



void* xcss_strip_comments(cstr sCss)
{
    xcssinput Input = xcss_input(sCss);
    xbuffer Output;
    void* pResult;
    size_t i = 0u;

    xrtClearError();
    if (!xcss_utf8(Input)) return NULL;
    (void)xrtBufferInit(&Output);
    while (i < Input.Size) {
        size_t iStart = i;

        if (Input.Data[i] == '/' && i + 1u < Input.Size && Input.Data[i + 1u] == '*') {
            if (!xcss_skip_comment(Input, &i) ||
                !xrtBufferAppendByte(&Output, ' ')) goto Failed;
            continue;
        }
        if (Input.Data[i] == '\'' || Input.Data[i] == '"') {
            if (!xcss_skip_string(Input, &i)) {
                goto Failed;
            }
        } else {
            ++i;
        }
        if (!xrtBufferAppend(&Output, (xbytesview){
                (cbytes)(Input.Data + iStart), i - iStart })) goto Failed;
    }
    if (!xrtBufferAppendByte(&Output, 0u)) goto Failed;
    xrtClearError();
    pResult = xrtBufferTake(&Output, NULL, NULL);
    if (pResult != NULL) return pResult;

Failed:
    xrtBufferUnit(&Output);
    if (xrtGetError() == NULL) xcss_error(XERR_MEMORY, "could not allocate CSS result text");
    return NULL;
}
