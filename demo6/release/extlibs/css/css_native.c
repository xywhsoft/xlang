/*
    CSS restricted parser.

    This extension intentionally provides syntax-aware rule/declaration lookup,
    not a browser cascade or selector engine.  It handles comments, quoted
    strings, escapes, parentheses, brackets and nested at-rules so source text
    is never searched with a blind strstr().
*/
#include <string.h>

#define XRT_MODULE_BUFFER
#include "xrt.h"

#if defined(_WIN32) || defined(_WIN64)
#define XCSS_THREAD __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define XCSS_THREAD __thread
#else
#define XCSS_THREAD
#endif

#define XCSS_DOMAIN "xlang.extlib.css"
#define XCSS_RESULT_SLOTS 8u

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

static XCSS_THREAD xbuffer g_xcss_results[XCSS_RESULT_SLOTS];
static XCSS_THREAD size_t g_xcss_result_index;



static void xcss_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XCSS_DOMAIN, 0, sMessage);
}



static xcssinput xcss_input(cstr sText)
{
    xcssinput Input;

    Input.Data = sText != NULL ? sText : "";
    Input.Size = strlen(Input.Data);
    return Input;
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



static bool xcss_span_cstr_equal(xcssinput Input, xcssspan Left, cstr sRight)
{
    size_t iSize = sRight != NULL ? strlen(sRight) : 0u;

    Left = xcss_trim(Input, Left);
    return (Left.End - Left.Begin) == iSize &&
        (iSize == 0u || memcmp(Input.Data + Left.Begin, sRight, iSize) == 0);
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
        xcss_error(XERR_PROTOCOL, "CSS comment is not terminated");
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
            if (*pPos < Input.Size) ++*pPos;
        } else if (cValue == Quote) {
            ++*pPos;
            return true;
        } else {
            ++*pPos;
        }
    }
    xcss_error(XERR_PROTOCOL, "CSS string is not terminated");
    return false;
}



/* Finds a delimiter outside string/comment/()[] nesting. */
static bool xcss_next_delimiter(xcssinput Input, size_t Begin, size_t End,
    cstr sDelimiters, size_t* pPos, char* pDelimiter)
{
    size_t i = Begin;
    size_t iParen = 0u;
    size_t iBracket = 0u;

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
        if (cValue == '(') ++iParen;
        else if (cValue == ')') {
            if (iParen == 0u) {
                xcss_error(XERR_PROTOCOL, "CSS has an unmatched ')' character");
                return false;
            }
            --iParen;
        } else if (cValue == '[') ++iBracket;
        else if (cValue == ']') {
            if (iBracket == 0u) {
                xcss_error(XERR_PROTOCOL, "CSS has an unmatched ']' character");
                return false;
            }
            --iBracket;
        } else if (iParen == 0u && iBracket == 0u && strchr(sDelimiters, cValue) != NULL) {
            *pPos = i;
            *pDelimiter = cValue;
            return true;
        }
        ++i;
    }
    if (iParen != 0u || iBracket != 0u) {
        xcss_error(XERR_PROTOCOL, "CSS has an unclosed () or [] expression");
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
    xcss_error(XERR_PROTOCOL, "CSS block is not terminated");
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
    xcssrulevisit pVisit, void* pData)
{
    size_t i = Begin;

    while (i < End) {
        xcssspan Selector;
        xcssspan Body;
        size_t iDelimiter;
        size_t iBlockEnd;
        char Delimiter;

        if (!xcss_skip_space(Input, &i)) return false;
        if (i >= End) break;
        if (Input.Data[i] == '}') {
            xcss_error(XERR_PROTOCOL, "CSS has an unmatched '}' character");
            return false;
        }
        Selector.Begin = i;
        if (!xcss_next_delimiter(Input, i, End, "{;}", &iDelimiter, &Delimiter)) return false;
        Selector.End = iDelimiter;
        Selector = xcss_trim(Input, Selector);
        if (Delimiter == 0 || Delimiter == '}') {
            xcss_error(XERR_PROTOCOL, "CSS rule is missing an opening '{'");
            return false;
        }
        if (Delimiter == ';') {
            /* At-rules such as @charset and @import do not have a body. */
            if (Selector.Begin == Selector.End || Input.Data[Selector.Begin] != '@') {
                xcss_error(XERR_PROTOCOL, "CSS statement outside a rule must start with '@'");
                return false;
            }
            i = iDelimiter + 1u;
            continue;
        }
        if (Selector.Begin == Selector.End) {
            xcss_error(XERR_PROTOCOL, "CSS rule selector is empty");
            return false;
        }
        if (!xcss_block_end(Input, iDelimiter + 1u, End, &iBlockEnd)) return false;
        Body.Begin = iDelimiter + 1u;
        Body.End = iBlockEnd;
        if (Input.Data[Selector.Begin] == '@') {
            bool bNestedRules;

            /* @media/@supports/@layer contain rules; @font-face contains declarations. */
            if (!xcss_block_has_rule(Input, Body, &bNestedRules)) return false;
            if (bNestedRules && !xcss_walk_rules(Input, Body.Begin, Body.End, pVisit, pData)) return false;
        } else if (pVisit != NULL && !pVisit(Input, Selector, Body, pData)) {
            return false;
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
            xcss_error(XERR_PROTOCOL, "CSS declaration is missing ':'");
            return false;
        }
        Name = xcss_trim(Input, (xcssspan){ i, iColon });
        Value = xcss_trim(Input, (xcssspan){ iColon + 1u, iDelimiter });
        if (Name.Begin == Name.End) {
            xcss_error(XERR_PROTOCOL, "CSS declaration property is empty");
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

    if (!xcss_selector_matches(Input, Selector, pContext->Selector)) return xrtGetError() == NULL;
    if (!xcss_declaration_value(Input, Body, pContext->Property,
            &pContext->Value, &pContext->Found)) {
        return false;
    }
    return !pContext->Found;
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
        return false;
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
            xcss_error(XERR_PROTOCOL, "CSS declaration is missing ':'");
            return false;
        }
        Name = xcss_trim(Input, (xcssspan){ i, iColon });
        if (Name.Begin == Name.End) {
            xcss_error(XERR_PROTOCOL, "CSS declaration property is empty");
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

    if (!xcss_selector_matches(Input, Selector, pContext->Selector)) return xrtGetError() == NULL;
    if (!xcss_declaration_count(Input, Body, &pContext->Count)) return false;
    pContext->Found = true;
    return false;
}



static cstr xcss_store(xcssinput Input, xcssspan Span)
{
    xbuffer* pOutput = &g_xcss_results[g_xcss_result_index++ % XCSS_RESULT_SLOTS];

    Span = xcss_trim(Input, Span);
    xrtBufferClear(pOutput);
    if (!xrtBufferAppend(pOutput, (xbytesview){
            (cbytes)(Input.Data + Span.Begin), Span.End - Span.Begin }) ||
        !xrtBufferAppendByte(pOutput, 0u)) {
        xrtBufferClear(pOutput);
        xcss_error(XERR_MEMORY, "could not allocate CSS result text");
        return "";
    }
    return (cstr)pOutput->Data;
}



cstr xcss_get(cstr sCss, cstr sSelector, cstr sProperty)
{
    xcssinput Input = xcss_input(sCss);
    xcssgetcontext Context;

    Context.Selector = sSelector;
    Context.Property = sProperty;
    Context.Found = false;
    if (sSelector == NULL || sProperty == NULL || sSelector[0] == 0 || sProperty[0] == 0) {
        xcss_error(XERR_ARGUMENT, "CSS selector and property are required");
        return "";
    }
    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_get_visit, &Context) &&
        xrtGetError() != NULL) return "";
    if (!Context.Found) {
        xrtClearError();
        return "";
    }
    xrtClearError();
    return xcss_store(Input, Context.Value);
}



long long xcss_count_rules(cstr sCss)
{
    xcsscountcontext Context = { 0u };
    xcssinput Input = xcss_input(sCss);

    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_count_visit, &Context)) return -1;
    xrtClearError();
    return (long long)Context.Count;
}



long long xcss_count_props(cstr sCss, cstr sSelector)
{
    xcssinput Input = xcss_input(sCss);
    xcsspropcountcontext Context;

    if (sSelector == NULL || sSelector[0] == 0) {
        xcss_error(XERR_ARGUMENT, "CSS selector is required");
        return -1;
    }
    memset(&Context, 0, sizeof(Context));
    Context.Selector = sSelector;
    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_prop_count_visit, &Context) &&
        xrtGetError() != NULL) return -1;
    xrtClearError();
    return Context.Found ? (long long)Context.Count : 0;
}



cstr xcss_selector_at(cstr sCss, long long iIndex)
{
    xcssselectorcontext Context;
    xcssinput Input = xcss_input(sCss);

    if (iIndex < 0) {
        xcss_error(XERR_RANGE, "CSS selector index must not be negative");
        return "";
    }
    memset(&Context, 0, sizeof(Context));
    Context.Index = (size_t)iIndex;
    if (!xcss_walk_rules(Input, 0u, Input.Size, xcss_selector_visit, &Context) &&
        xrtGetError() != NULL) return "";
    if (!Context.Found) {
        xrtClearError();
        return "";
    }
    xrtClearError();
    return xcss_store(Input, Context.Result);
}



bool xcss_validate(cstr sCss)
{
    xcssinput Input = xcss_input(sCss);

    if (!xcss_walk_rules(Input, 0u, Input.Size, NULL, NULL)) return false;
    xrtClearError();
    return true;
}



cstr xcss_strip_comments(cstr sCss)
{
    xcssinput Input = xcss_input(sCss);
    xbuffer* pOutput = &g_xcss_results[g_xcss_result_index++ % XCSS_RESULT_SLOTS];
    size_t i = 0u;

    xrtBufferClear(pOutput);
    while (i < Input.Size) {
        size_t iStart = i;

        if (Input.Data[i] == '/' && i + 1u < Input.Size && Input.Data[i + 1u] == '*') {
            if (!xcss_skip_comment(Input, &i) ||
                !xrtBufferAppendByte(pOutput, ' ')) {
                if (xrtGetError() == NULL) xcss_error(XERR_MEMORY, "could not allocate CSS result text");
                xrtBufferClear(pOutput);
                return "";
            }
            continue;
        }
        if (Input.Data[i] == '\'' || Input.Data[i] == '"') {
            if (!xcss_skip_string(Input, &i)) {
                xrtBufferClear(pOutput);
                return "";
            }
        } else {
            ++i;
        }
        if (!xrtBufferAppend(pOutput, (xbytesview){ (cbytes)(Input.Data + iStart), i - iStart })) {
            xcss_error(XERR_MEMORY, "could not allocate CSS result text");
            xrtBufferClear(pOutput);
            return "";
        }
    }
    if (!xrtBufferAppendByte(pOutput, 0u)) {
        xcss_error(XERR_MEMORY, "could not allocate CSS result text");
        xrtBufferClear(pOutput);
        return "";
    }
    xrtClearError();
    return (cstr)pOutput->Data;
}



cstr xcss_last_error(void)
{
    return xrtErrorMessage(xrtGetError());
}
