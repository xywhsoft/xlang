/*
    HCL restricted parser.

    The extension is a syntax-aware read-only query layer for HCL
    configuration. It understands blocks, labels, attributes, comments,
    strings and composite expression boundaries. It does not evaluate HCL or
    Terraform expressions, resolve variables, or implement provider schemas.
*/
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define XRT_MODULE_BUFFER
#include "xrt.h"

#if defined(_WIN32) || defined(_WIN64)
#define XHCL_THREAD __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define XHCL_THREAD __thread
#else
#define XHCL_THREAD
#endif

#define XHCL_DOMAIN "xlang.extlib.hcl"
#define XHCL_RESULT_SLOTS 8u

typedef struct xhclinput {
    cstr Data;
    size_t Size;
} xhclinput;

typedef struct xhclspan {
    size_t Begin;
    size_t End;
} xhclspan;

typedef struct xhclblock {
    xhclspan Type;
    xhclspan Labels[8];
    size_t LabelCount;
    xhclspan Body;
} xhclblock;

typedef bool (*xhclattributevisit)(xhclinput Input, xhclspan Name,
    xhclspan Value, void* pData);
typedef bool (*xhclblockvisit)(xhclinput Input, const xhclblock* pBlock,
    void* pData);

static XHCL_THREAD xbuffer g_xhcl_results[XHCL_RESULT_SLOTS];
static XHCL_THREAD size_t g_xhcl_result_index;



static void xhcl_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XHCL_DOMAIN, 0, sMessage);
}



static xhclinput xhcl_input(cstr sText)
{
    xhclinput Input;

    Input.Data = sText != NULL ? sText : "";
    Input.Size = strlen(Input.Data);
    return Input;
}



static bool xhcl_space(char cValue)
{
    return cValue == ' ' || cValue == '\t' || cValue == '\r' || cValue == '\n' ||
        cValue == '\f';
}



static bool xhcl_identifier_start(char cValue)
{
    return (cValue >= 'A' && cValue <= 'Z') || (cValue >= 'a' && cValue <= 'z') ||
        cValue == '_';
}



static bool xhcl_identifier_char(char cValue)
{
    return xhcl_identifier_start(cValue) || (cValue >= '0' && cValue <= '9') ||
        cValue == '-';
}



static xhclspan xhcl_trim(xhclinput Input, xhclspan Span)
{
    while (Span.Begin < Span.End && xhcl_space(Input.Data[Span.Begin])) ++Span.Begin;
    while (Span.End > Span.Begin && xhcl_space(Input.Data[Span.End - 1u])) --Span.End;
    return Span;
}



static bool xhcl_span_cstr_equal(xhclinput Input, xhclspan Span, cstr sText)
{
    size_t iSize = sText != NULL ? strlen(sText) : 0u;

    Span = xhcl_trim(Input, Span);
    return (Span.End - Span.Begin) == iSize &&
        (iSize == 0u || memcmp(Input.Data + Span.Begin, sText, iSize) == 0);
}



static bool xhcl_skip_line_comment(xhclinput Input, size_t* pPos)
{
    while (*pPos < Input.Size && Input.Data[*pPos] != '\n') ++*pPos;
    return true;
}



static bool xhcl_skip_block_comment(xhclinput Input, size_t* pPos)
{
    size_t i = *pPos + 2u;

    while (i + 1u < Input.Size && !(Input.Data[i] == '*' && Input.Data[i + 1u] == '/')) ++i;
    if (i + 1u >= Input.Size) {
        xhcl_error(XERR_PROTOCOL, "HCL block comment is not terminated");
        return false;
    }
    *pPos = i + 2u;
    return true;
}



static bool xhcl_skip_comment(xhclinput Input, size_t* pPos)
{
    if (*pPos >= Input.Size) return true;
    if (Input.Data[*pPos] == '#') return xhcl_skip_line_comment(Input, pPos);
    if (Input.Data[*pPos] == '/' && *pPos + 1u < Input.Size && Input.Data[*pPos + 1u] == '/') {
        return xhcl_skip_line_comment(Input, pPos);
    }
    if (Input.Data[*pPos] == '/' && *pPos + 1u < Input.Size && Input.Data[*pPos + 1u] == '*') {
        return xhcl_skip_block_comment(Input, pPos);
    }
    return false;
}



static bool xhcl_skip_inline_space(xhclinput Input, size_t* pPos)
{
    while (*pPos < Input.Size) {
        if (Input.Data[*pPos] == ' ' || Input.Data[*pPos] == '\t' ||
            Input.Data[*pPos] == '\r' || Input.Data[*pPos] == '\f') {
            ++*pPos;
        } else {
            break;
        }
    }
    return true;
}



static bool xhcl_skip_space(xhclinput Input, size_t* pPos)
{
    while (*pPos < Input.Size) {
        if (xhcl_space(Input.Data[*pPos])) {
            ++*pPos;
        } else if (Input.Data[*pPos] == '#' ||
                   (Input.Data[*pPos] == '/' && *pPos + 1u < Input.Size &&
                    (Input.Data[*pPos + 1u] == '/' || Input.Data[*pPos + 1u] == '*'))) {
            if (!xhcl_skip_comment(Input, pPos)) return false;
        } else {
            break;
        }
    }
    return true;
}



static bool xhcl_skip_string(xhclinput Input, size_t* pPos)
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
    xhcl_error(XERR_PROTOCOL, "HCL string is not terminated");
    return false;
}



static bool xhcl_read_identifier(xhclinput Input, size_t* pPos, xhclspan* pName)
{
    size_t i = *pPos;

    if (i >= Input.Size || !xhcl_identifier_start(Input.Data[i])) {
        xhcl_error(XERR_PROTOCOL, "HCL expected an identifier");
        return false;
    }
    pName->Begin = i++;
    while (i < Input.Size && xhcl_identifier_char(Input.Data[i])) ++i;
    pName->End = i;
    *pPos = i;
    return true;
}



static bool xhcl_read_label(xhclinput Input, size_t* pPos, xhclspan* pLabel)
{
    size_t i = *pPos;

    if (i >= Input.Size) {
        xhcl_error(XERR_PROTOCOL, "HCL block header is incomplete");
        return false;
    }
    if (Input.Data[i] == '"') {
        pLabel->Begin = i;
        if (!xhcl_skip_string(Input, &i)) return false;
        pLabel->End = i;
        *pPos = i;
        return true;
    }
    return xhcl_read_identifier(Input, pPos, pLabel);
}



static bool xhcl_block_end(xhclinput Input, size_t Begin, size_t End, size_t* pEnd)
{
    size_t i = Begin;
    size_t iDepth = 1u;

    while (i < End) {
        char cValue = Input.Data[i];

        if (cValue == '#' || (cValue == '/' && i + 1u < End &&
            (Input.Data[i + 1u] == '/' || Input.Data[i + 1u] == '*'))) {
            if (!xhcl_skip_comment(Input, &i)) return false;
            continue;
        }
        if (cValue == '"' || cValue == '\'') {
            if (!xhcl_skip_string(Input, &i)) return false;
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
    xhcl_error(XERR_PROTOCOL, "HCL block is not terminated");
    return false;
}



/* Reads an attribute expression through its top-level line ending. */
static bool xhcl_expression_end(xhclinput Input, size_t Begin, size_t End, size_t* pEnd)
{
    size_t i = Begin;
    size_t iParen = 0u;
    size_t iBracket = 0u;
    size_t iBrace = 0u;

    while (i < End) {
        char cValue = Input.Data[i];

        if ((cValue == '#' || (cValue == '/' && i + 1u < End &&
             Input.Data[i + 1u] == '/')) &&
            iParen == 0u && iBracket == 0u && iBrace == 0u) {
            *pEnd = i;
            return true;
        }
        if (cValue == '#' || (cValue == '/' && i + 1u < End &&
            (Input.Data[i + 1u] == '/' || Input.Data[i + 1u] == '*'))) {
            if (!xhcl_skip_comment(Input, &i)) return false;
            continue;
        }
        if (cValue == '"' || cValue == '\'') {
            if (!xhcl_skip_string(Input, &i)) return false;
            continue;
        }
        if (cValue == '\\') {
            i += i + 1u < End ? 2u : 1u;
            continue;
        }
        if (cValue == '(') ++iParen;
        else if (cValue == ')') {
            if (iParen == 0u) {
                xhcl_error(XERR_PROTOCOL, "HCL has an unmatched ')' character");
                return false;
            }
            --iParen;
        } else if (cValue == '[') ++iBracket;
        else if (cValue == ']') {
            if (iBracket == 0u) {
                xhcl_error(XERR_PROTOCOL, "HCL has an unmatched ']' character");
                return false;
            }
            --iBracket;
        } else if (cValue == '{') ++iBrace;
        else if (cValue == '}') {
            if (iBrace == 0u) {
                *pEnd = i;
                return true;
            }
            --iBrace;
        } else if (cValue == '\n' && iParen == 0u && iBracket == 0u && iBrace == 0u) {
            *pEnd = i;
            return true;
        }
        ++i;
    }
    if (iParen != 0u || iBracket != 0u || iBrace != 0u) {
        xhcl_error(XERR_PROTOCOL, "HCL expression has an unclosed delimiter");
        return false;
    }
    *pEnd = End;
    return true;
}



static bool xhcl_walk_scope(xhclinput Input, size_t Begin, size_t End,
    xhclattributevisit pAttributeVisit, xhclblockvisit pBlockVisit,
    void* pData, bool bRecursive)
{
    size_t i = Begin;

    while (i < End) {
        xhclspan Name;
        size_t iValue;

        if (!xhcl_skip_space(Input, &i)) return false;
        if (i >= End) break;
        if (!xhcl_read_identifier(Input, &i, &Name)) return false;
        if (!xhcl_skip_inline_space(Input, &i)) return false;
        if (i >= End) {
            xhcl_error(XERR_PROTOCOL, "HCL declaration is incomplete");
            return false;
        }
        if (Input.Data[i] == '=') {
            xhclspan Value;

            ++i;
            if (!xhcl_skip_inline_space(Input, &i)) return false;
            Value.Begin = i;
            if (!xhcl_expression_end(Input, i, End, &iValue)) return false;
            Value.End = iValue;
            Value = xhcl_trim(Input, Value);
            if (Value.Begin == Value.End) {
                xhcl_error(XERR_PROTOCOL, "HCL attribute value is empty");
                return false;
            }
            if (pAttributeVisit != NULL && !pAttributeVisit(Input, Name, Value, pData)) return true;
            i = iValue;
            continue;
        }
        {
            xhclblock Block;
            size_t iBlockEnd;

            memset(&Block, 0, sizeof(Block));
            Block.Type = Name;
            while (i < End && Input.Data[i] != '{') {
                if (Block.LabelCount >= sizeof(Block.Labels) / sizeof(Block.Labels[0])) {
                    xhcl_error(XERR_RANGE, "HCL block has too many labels");
                    return false;
                }
                if (!xhcl_read_label(Input, &i, &Block.Labels[Block.LabelCount++])) return false;
                if (!xhcl_skip_inline_space(Input, &i)) return false;
            }
            if (i >= End || Input.Data[i] != '{') {
                xhcl_error(XERR_PROTOCOL, "HCL block is missing an opening '{'");
                return false;
            }
            if (!xhcl_block_end(Input, i + 1u, End, &iBlockEnd)) return false;
            Block.Body = (xhclspan){ i + 1u, iBlockEnd };
            if (pBlockVisit != NULL && !pBlockVisit(Input, &Block, pData)) return true;
            if (bRecursive && !xhcl_walk_scope(Input, Block.Body.Begin, Block.Body.End,
                    pAttributeVisit, pBlockVisit, pData, true)) return false;
            i = iBlockEnd + 1u;
        }
    }
    return true;
}



static cstr xhcl_store(xhclinput Input, xhclspan Span, bool bDecodeString)
{
    xbuffer* pOutput = &g_xhcl_results[g_xhcl_result_index++ % XHCL_RESULT_SLOTS];
    size_t i;

    Span = xhcl_trim(Input, Span);
    xrtBufferClear(pOutput);
    if (bDecodeString && Span.End > Span.Begin + 1u && Input.Data[Span.Begin] == '"' &&
        Input.Data[Span.End - 1u] == '"') {
        for (i = Span.Begin + 1u; i + 1u < Span.End; ++i) {
            char cValue = Input.Data[i];

            if (cValue == '\\' && i + 2u < Span.End) {
                char Escape = Input.Data[++i];
                if (Escape == 'n') cValue = '\n';
                else if (Escape == 'r') cValue = '\r';
                else if (Escape == 't') cValue = '\t';
                else cValue = Escape;
            }
            if (!xrtBufferAppendByte(pOutput, (uint8)cValue)) goto Memory;
        }
    } else if (!xrtBufferAppend(pOutput, (xbytesview){
            (cbytes)(Input.Data + Span.Begin), Span.End - Span.Begin })) {
        goto Memory;
    }
    if (!xrtBufferAppendByte(pOutput, 0u)) goto Memory;
    return (cstr)pOutput->Data;

Memory:
    xrtBufferClear(pOutput);
    xhcl_error(XERR_MEMORY, "could not allocate HCL result text");
    return "";
}



typedef struct xhclattributecontext {
    cstr Name;
    xhclspan Value;
    bool Found;
} xhclattributecontext;

static bool xhcl_find_attribute_visit(xhclinput Input, xhclspan Name,
    xhclspan Value, void* pData)
{
    xhclattributecontext* pContext = (xhclattributecontext*)pData;

    if (xhcl_span_cstr_equal(Input, Name, pContext->Name)) {
        pContext->Value = Value;
        pContext->Found = true;
        return false;
    }
    return true;
}



typedef struct xhclblockcontext {
    cstr Type;
    size_t Index;
    size_t Current;
    xhclblock Block;
    bool Found;
} xhclblockcontext;

static bool xhcl_find_block_visit(xhclinput Input, const xhclblock* pBlock, void* pData)
{
    xhclblockcontext* pContext = (xhclblockcontext*)pData;

    if (!xhcl_span_cstr_equal(Input, pBlock->Type, pContext->Type)) return true;
    if (pContext->Current++ == pContext->Index) {
        pContext->Block = *pBlock;
        pContext->Found = true;
        return false;
    }
    return true;
}



typedef struct xhclcountcontext {
    cstr Type;
    size_t Count;
} xhclcountcontext;

static bool xhcl_count_block_visit(xhclinput Input, const xhclblock* pBlock, void* pData)
{
    xhclcountcontext* pContext = (xhclcountcontext*)pData;

    if (xhcl_span_cstr_equal(Input, pBlock->Type, pContext->Type)) ++pContext->Count;
    return true;
}



static bool xhcl_count_attribute_visit(xhclinput Input, xhclspan Name,
    xhclspan Value, void* pData)
{
    size_t* pCount = (size_t*)pData;

    (void)Input;
    (void)Name;
    (void)Value;
    ++*pCount;
    return true;
}



cstr xhcl_get(cstr sHcl, cstr sKey)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclattributecontext Context;

    if (sKey == NULL || sKey[0] == 0) {
        xhcl_error(XERR_ARGUMENT, "HCL attribute name is required");
        return "";
    }
    memset(&Context, 0, sizeof(Context));
    Context.Name = sKey;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_find_attribute_visit, NULL,
            &Context, true) && xrtGetError() != NULL) return "";
    if (!Context.Found) {
        xrtClearError();
        return "";
    }
    xrtClearError();
    return xhcl_store(Input, Context.Value, true);
}



cstr xhcl_get_raw(cstr sHcl, cstr sKey)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclattributecontext Context;

    if (sKey == NULL || sKey[0] == 0) {
        xhcl_error(XERR_ARGUMENT, "HCL attribute name is required");
        return "";
    }
    memset(&Context, 0, sizeof(Context));
    Context.Name = sKey;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_find_attribute_visit, NULL,
            &Context, true) && xrtGetError() != NULL) return "";
    if (!Context.Found) {
        xrtClearError();
        return "";
    }
    xrtClearError();
    return xhcl_store(Input, Context.Value, false);
}



long long xhcl_get_int(cstr sHcl, cstr sKey)
{
    cstr sValue = xhcl_get(sHcl, sKey);
    char* pEnd;
    long long iValue;

    if (xrtGetError() != NULL || sValue[0] == 0) return 0;
    errno = 0;
    iValue = strtoll(sValue, &pEnd, 10);
    if (pEnd == sValue || errno == ERANGE || *pEnd != 0) {
        xhcl_error(XERR_VALUE, "HCL attribute is not an integer");
        return 0;
    }
    return iValue;
}



double xhcl_get_float(cstr sHcl, cstr sKey)
{
    cstr sValue = xhcl_get(sHcl, sKey);
    char* pEnd;
    double fValue;

    if (xrtGetError() != NULL || sValue[0] == 0) return 0.0;
    errno = 0;
    fValue = strtod(sValue, &pEnd);
    if (pEnd == sValue || errno == ERANGE || *pEnd != 0) {
        xhcl_error(XERR_VALUE, "HCL attribute is not a float");
        return 0.0;
    }
    return fValue;
}



bool xhcl_get_bool(cstr sHcl, cstr sKey)
{
    cstr sValue = xhcl_get(sHcl, sKey);

    if (xrtGetError() != NULL || sValue[0] == 0) return false;
    if (strcmp(sValue, "true") == 0) return true;
    if (strcmp(sValue, "false") == 0) return false;
    xhcl_error(XERR_VALUE, "HCL attribute is not a bool");
    return false;
}



bool xhcl_has(cstr sHcl, cstr sKey)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclattributecontext Context;

    if (sKey == NULL || sKey[0] == 0) return false;
    memset(&Context, 0, sizeof(Context));
    Context.Name = sKey;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_find_attribute_visit, NULL,
            &Context, true)) {
        xrtClearError();
        return false;
    }
    xrtClearError();
    return Context.Found;
}



long long xhcl_count_blocks(cstr sHcl, cstr sType)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclcountcontext Context;

    if (sType == NULL || sType[0] == 0) {
        xhcl_error(XERR_ARGUMENT, "HCL block type is required");
        return -1;
    }
    Context.Type = sType;
    Context.Count = 0u;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, NULL, xhcl_count_block_visit,
            &Context, true)) return -1;
    xrtClearError();
    return (long long)Context.Count;
}



cstr xhcl_block_label(cstr sHcl, cstr sType, long long iIndex)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclblockcontext Context;

    if (sType == NULL || sType[0] == 0 || iIndex < 0) {
        xhcl_error(XERR_ARGUMENT, "HCL block type and non-negative index are required");
        return "";
    }
    memset(&Context, 0, sizeof(Context));
    Context.Type = sType;
    Context.Index = (size_t)iIndex;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, NULL, xhcl_find_block_visit,
            &Context, true) && xrtGetError() != NULL) return "";
    if (!Context.Found || Context.Block.LabelCount == 0u) {
        xrtClearError();
        return "";
    }
    xrtClearError();
    return xhcl_store(Input, Context.Block.Labels[0], true);
}



long long xhcl_block_label_count(cstr sHcl, cstr sType, long long iIndex)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclblockcontext Context;

    if (sType == NULL || sType[0] == 0 || iIndex < 0) {
        xhcl_error(XERR_ARGUMENT, "HCL block type and non-negative index are required");
        return -1;
    }
    memset(&Context, 0, sizeof(Context));
    Context.Type = sType;
    Context.Index = (size_t)iIndex;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, NULL, xhcl_find_block_visit,
            &Context, true) && xrtGetError() != NULL) return -1;
    xrtClearError();
    return Context.Found ? (long long)Context.Block.LabelCount : 0;
}



cstr xhcl_block_label_at(cstr sHcl, cstr sType, long long iIndex, long long iLabel)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclblockcontext Context;

    if (sType == NULL || sType[0] == 0 || iIndex < 0 || iLabel < 0) {
        xhcl_error(XERR_ARGUMENT, "HCL block type and non-negative indexes are required");
        return "";
    }
    memset(&Context, 0, sizeof(Context));
    Context.Type = sType;
    Context.Index = (size_t)iIndex;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, NULL, xhcl_find_block_visit,
            &Context, true) && xrtGetError() != NULL) return "";
    if (!Context.Found || (uint64)iLabel >= Context.Block.LabelCount) {
        xrtClearError();
        return "";
    }
    xrtClearError();
    return xhcl_store(Input, Context.Block.Labels[(size_t)iLabel], true);
}



bool xhcl_validate(cstr sHcl)
{
    xhclinput Input = xhcl_input(sHcl);

    if (!xhcl_walk_scope(Input, 0u, Input.Size, NULL, NULL, NULL, true)) return false;
    xrtClearError();
    return true;
}



long long xhcl_count_attrs(cstr sHcl)
{
    xhclinput Input = xhcl_input(sHcl);
    size_t iCount = 0u;

    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_count_attribute_visit, NULL,
            &iCount, true)) return -1;
    xrtClearError();
    return (long long)iCount;
}



cstr xhcl_last_error(void)
{
    return xrtErrorMessage(xrtGetError());
}
