/*
    HCL 受限只读查询器。识别块、标签、属性、注释、字符串和复合表达式边界，
    不执行 Terraform 表达式，也不解析变量或 provider schema。
*/
#include <string.h>

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_CHARSET
#define XRT_MODULE_CHARSET
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#include "xrt.h"

#include "include/hcl_native.h"

#define XHCL_DOMAIN "xlang.extlib.hcl"
#define XHCL_MAX_DEPTH 64u
#define XHCL_MAX_INPUT (8u * 1024u * 1024u)

typedef struct xhclinput {
    cstr Data;
    size_t Size;
    size_t Depth;
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

typedef void (*xhclattributevisit)(xhclinput Input, xhclspan Name,
    xhclspan Value, void* pData);
typedef void (*xhclblockvisit)(xhclinput Input, const xhclblock* pBlock,
    void* pData);

static void xhcl_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XHCL_DOMAIN, 0, sMessage);
}



static xhclinput xhcl_input(cstr sText)
{
    xhclinput Input;

    Input.Data = sText;
    /* C 字符串入口最多探测上限外一个字节，避免超大输入的无界扫描。 */
    Input.Size = 0u;
    if (sText != NULL) {
        for (; Input.Size <= XHCL_MAX_INPUT &&
            sText[Input.Size] != 0; ++Input.Size) {}
    }
    Input.Depth = 0u;
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
        xhcl_error(XERR_VALUE, "HCL block comment is not terminated");
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
        } else if (Input.Data[*pPos] == '/' && *pPos + 1u < Input.Size &&
                   Input.Data[*pPos + 1u] == '*') {
            if (!xhcl_skip_block_comment(Input, pPos)) return false;
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



static int xhcl_hex_digit(char cValue)
{
    if (cValue >= '0' && cValue <= '9') return cValue - '0';
    if (cValue >= 'a' && cValue <= 'f') return cValue - 'a' + 10;
    if (cValue >= 'A' && cValue <= 'F') return cValue - 'A' + 10;
    return -1;
}

static bool xhcl_escape(xhclinput Input, size_t iEnd, size_t* pPos,
    char Quote, uint32* pScalar)
{
    size_t i = *pPos + 1u;
    size_t iDigits = 0u;
    uint32 iScalar = 0u;
    size_t k;

    if (i >= iEnd) goto invalid;
    switch (Input.Data[i]) {
    case 'n': iScalar = '\n'; break;
    case 'r': iScalar = '\r'; break;
    case 't': iScalar = '\t'; break;
    case '"': iScalar = '"'; break;
    case '\\': iScalar = '\\'; break;
    case '\'': if (Quote != '\'') goto invalid; iScalar = '\''; break;
    case 'u': iDigits = 4u; break;
    case 'U': iDigits = 8u; break;
    default: goto invalid;
    }
    if (iDigits != 0u) {
        if (iDigits > iEnd - i - 1u) goto invalid;
        for (k = 0u; k < iDigits; ++k) {
            int iDigit = xhcl_hex_digit(Input.Data[i + k + 1u]);
            if (iDigit < 0) goto invalid;
            iScalar = (iScalar << 4u) | (uint32)iDigit;
        }
        if (iScalar > 0x10ffffu ||
            (iScalar >= 0xd800u && iScalar <= 0xdfffu)) goto invalid;
        i += iDigits;
    }
    *pScalar = iScalar;
    *pPos = i + 1u;
    return true;
invalid:
    xhcl_error(XERR_VALUE, "HCL quoted string has an invalid escape");
    return false;
}

static bool xhcl_skip_string(xhclinput Input, size_t* pPos)
{
    char Quote = Input.Data[*pPos];

    ++*pPos;
    while (*pPos < Input.Size) {
        char cValue = Input.Data[*pPos];

        if (cValue == '\n' || cValue == '\r' || cValue == '\f') {
            xhcl_error(XERR_VALUE, "HCL quoted string cannot contain a line break");
            return false;
        }
        if (cValue == '\\') {
            uint32 iScalar;
            if (!xhcl_escape(Input, Input.Size, pPos, Quote, &iScalar)) return false;
        } else if (cValue == Quote) {
            ++*pPos;
            return true;
        } else {
            ++*pPos;
        }
    }
    xhcl_error(XERR_VALUE, "HCL string is not terminated");
    return false;
}



/* 同一次扫描剥离尾注释并识别单字符串；复合表达式不能按首尾引号解码。 */
static bool xhcl_trim_value(xhclinput Input, xhclspan* pSpan, bool* pString)
{
    size_t i = pSpan->Begin;
    size_t iTrivia = pSpan->End;

    *pString = false;
    Input.Size = pSpan->End;
    while (i < Input.Size) {
        char cValue = Input.Data[i];

        if (xhcl_space(cValue)) {
            if (iTrivia == Input.Size) iTrivia = i;
            ++i;
        } else if (cValue == '#' || (cValue == '/' && i + 1u < Input.Size &&
                   (Input.Data[i + 1u] == '/' || Input.Data[i + 1u] == '*'))) {
            if (iTrivia == Input.Size) iTrivia = i;
            /* 与表达式边界扫描一致：行注释内的引号、转义不再当作 token。 */
            if (!xhcl_skip_comment(Input, &i)) return false;
        } else if (cValue == '"' || cValue == '\'') {
            bool bFirstString = cValue == '"' && i == pSpan->Begin;
            if (!xhcl_skip_string(Input, &i)) return false;
            *pString = bFirstString;
            iTrivia = Input.Size;
        } else {
            *pString = false;
            iTrivia = Input.Size;
            ++i;
        }
    }
    if (iTrivia < pSpan->End) pSpan->End = iTrivia;
    *pSpan = xhcl_trim(Input, *pSpan);
    return true;
}



static bool xhcl_read_identifier(xhclinput Input, size_t* pPos, xhclspan* pName)
{
    size_t i = *pPos;

    if (i >= Input.Size || !xhcl_identifier_start(Input.Data[i])) {
        xhcl_error(XERR_VALUE, "HCL expected an identifier");
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
        xhcl_error(XERR_VALUE, "HCL block header is incomplete");
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
    xhcl_error(XERR_VALUE, "HCL block is not terminated");
    return false;
}



/* 读取属性表达式至顶层行尾，并按嵌套顺序匹配括号。 */
static bool xhcl_expression_end(xhclinput Input, size_t Begin, size_t End, size_t* pEnd)
{
    size_t i = Begin;
    char aExpected[XHCL_MAX_DEPTH];
    size_t iDepth = 0u;

    while (i < End) {
        char cValue = Input.Data[i];

        if ((cValue == '#' || (cValue == '/' && i + 1u < End &&
             Input.Data[i + 1u] == '/')) &&
            iDepth == 0u) {
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
        if (cValue == '(' || cValue == '[' || cValue == '{') {
            if (iDepth == XHCL_MAX_DEPTH) {
                xhcl_error(XERR_RANGE, "HCL expression nesting exceeds 64 levels");
                return false;
            }
            aExpected[iDepth++] = cValue == '(' ? ')' :
                (cValue == '[' ? ']' : '}');
        } else if (cValue == ')' || cValue == ']' || cValue == '}') {
            if (cValue == '}' && iDepth == 0u) {
                *pEnd = i;
                return true;
            }
            if (iDepth == 0u || aExpected[iDepth - 1u] != cValue) {
                xhcl_error(XERR_VALUE, "HCL expression has mismatched delimiters");
                return false;
            }
            --iDepth;
        } else if (cValue == '\n' && iDepth == 0u) {
            *pEnd = i;
            return true;
        }
        ++i;
    }
    if (iDepth != 0u) {
        xhcl_error(XERR_VALUE, "HCL expression has an unclosed delimiter");
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

    if (Input.Data == NULL) {
        xhcl_error(XERR_ARGUMENT, "HCL input pointer is null");
        return false;
    }
    if (Input.Size > XHCL_MAX_INPUT) {
        xhcl_error(XERR_RANGE, "HCL input exceeds 8 MiB");
        return false;
    }
    if (Input.Depth > XHCL_MAX_DEPTH) {
        xhcl_error(XERR_RANGE, "HCL block nesting exceeds 64 levels");
        return false;
    }
    if (Begin == 0u && End == Input.Size &&
        !xrtUtf8Valid((xstrview){ Input.Data, Input.Size }, NULL)) {
        xhcl_error(XERR_VALUE, "HCL text is not valid UTF-8");
        return false;
    }

    while (i < End) {
        xhclspan Name;
        size_t iValue;

        if (!xhcl_skip_space(Input, &i)) return false;
        if (i >= End) break;
        if (!xhcl_read_identifier(Input, &i, &Name)) return false;
        if (!xhcl_skip_inline_space(Input, &i)) return false;
        if (i >= End) {
            xhcl_error(XERR_VALUE, "HCL declaration is incomplete");
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
                xhcl_error(XERR_VALUE, "HCL attribute value is empty");
                return false;
            }
            if (pAttributeVisit != NULL) pAttributeVisit(Input, Name, Value, pData);
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
                xhcl_error(XERR_VALUE, "HCL block is missing an opening '{'");
                return false;
            }
            if (!xhcl_block_end(Input, i + 1u, End, &iBlockEnd)) return false;
            Block.Body = (xhclspan){ i + 1u, iBlockEnd };
            if (pBlockVisit != NULL) pBlockVisit(Input, &Block, pData);
            if (bRecursive) {
                xhclinput Inner = Input;

                ++Inner.Depth;
                if (!xhcl_walk_scope(Inner, Block.Body.Begin, Block.Body.End,
                        pAttributeVisit, pBlockVisit, pData, true)) return false;
            }
            i = iBlockEnd + 1u;
        }
    }
    return true;
}



static char* xhcl_empty(void)
{
    char* pResult = (char*)xrtMalloc(1u);
    if (pResult == NULL) xhcl_error(XERR_MEMORY, "could not allocate HCL result text");
    else pResult[0] = 0;
    return pResult;
}



static char* xhcl_store(xhclinput Input, xhclspan Span, bool bDecodeString)
{
    xbuffer Output;
    char* pResult;
    size_t i;
    bool bString = false;

    (void)xrtBufferInit(&Output);
    Span = xhcl_trim(Input, Span);
    if (bDecodeString && !xhcl_trim_value(Input, &Span, &bString)) {
        xrtBufferUnit(&Output);
        return NULL;
    }
    if (bString) {
        for (i = Span.Begin + 1u; i + 1u < Span.End;) {
            char cValue = Input.Data[i];

            if (cValue == '\\') {
                uint32 iScalar;
                char Utf8[4];
                size_t iBytes;
                if (!xhcl_escape(Input, Span.End - 1u, &i, '"', &iScalar)) {
                    xrtBufferUnit(&Output);
                    return NULL;
                }
                if (iScalar == 0u) {
                    xrtBufferUnit(&Output);
                    xhcl_error(XERR_VALUE, "decoded HCL text contains an embedded NUL");
                    return NULL;
                }
                iBytes = xrtUtf8Encode(iScalar, Utf8);
                if (iBytes == 0u || !xrtBufferAppend(&Output,
                    (xbytesview){ (const uint8*)Utf8, iBytes })) goto Memory;
                continue;
            }
            if (!xrtBufferAppendByte(&Output, (uint8)cValue)) goto Memory;
            ++i;
        }
    } else if (!xrtBufferAppend(&Output, (xbytesview){
            (cbytes)(Input.Data + Span.Begin), Span.End - Span.Begin })) {
        goto Memory;
    }
    if (!xrtBufferAppendByte(&Output, 0u)) goto Memory;
    pResult = (char*)xrtBufferTake(&Output, NULL, NULL);
    if (pResult != NULL) return pResult;

Memory:
    xrtBufferUnit(&Output);
    xhcl_error(XERR_MEMORY, "could not allocate HCL result text");
    return NULL;
}



typedef struct xhclattributecontext {
    cstr Name;
    xhclspan Value;
    bool Found;
} xhclattributecontext;

static void xhcl_find_attribute_visit(xhclinput Input, xhclspan Name,
    xhclspan Value, void* pData)
{
    xhclattributecontext* pContext = (xhclattributecontext*)pData;

    if (!pContext->Found && xhcl_span_cstr_equal(Input, Name, pContext->Name)) {
        pContext->Value = Value;
        pContext->Found = true;
    }
}



typedef struct xhclblockcontext {
    cstr Type;
    size_t Index;
    size_t Current;
    xhclblock Block;
    bool Found;
} xhclblockcontext;

static void xhcl_find_block_visit(xhclinput Input, const xhclblock* pBlock, void* pData)
{
    xhclblockcontext* pContext = (xhclblockcontext*)pData;

    if (pContext->Found || !xhcl_span_cstr_equal(Input, pBlock->Type, pContext->Type)) return;
    if (pContext->Current++ == pContext->Index) {
        pContext->Block = *pBlock;
        pContext->Found = true;
    }
}



typedef struct xhclcountcontext {
    cstr Type;
    size_t Count;
} xhclcountcontext;

static void xhcl_count_block_visit(xhclinput Input, const xhclblock* pBlock, void* pData)
{
    xhclcountcontext* pContext = (xhclcountcontext*)pData;

    if (xhcl_span_cstr_equal(Input, pBlock->Type, pContext->Type)) ++pContext->Count;
}



static void xhcl_count_attribute_visit(xhclinput Input, xhclspan Name,
    xhclspan Value, void* pData)
{
    size_t* pCount = (size_t*)pData;

    (void)Input;
    (void)Name;
    (void)Value;
    ++*pCount;
}



char* xhcl_get(cstr sHcl, cstr sKey)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclattributecontext Context;

    if (sKey == NULL || sKey[0] == 0) {
        xhcl_error(XERR_ARGUMENT, "HCL attribute name is required");
        return NULL;
    }
    memset(&Context, 0, sizeof(Context));
    Context.Name = sKey;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_find_attribute_visit, NULL,
            &Context, true)) return NULL;
    if (!Context.Found) {
        xrtClearError();
        return xhcl_empty();
    }
    xrtClearError();
    return xhcl_store(Input, Context.Value, true);
}



char* xhcl_get_raw(cstr sHcl, cstr sKey)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclattributecontext Context;

    if (sKey == NULL || sKey[0] == 0) {
        xhcl_error(XERR_ARGUMENT, "HCL attribute name is required");
        return NULL;
    }
    memset(&Context, 0, sizeof(Context));
    Context.Name = sKey;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_find_attribute_visit, NULL,
            &Context, true)) return NULL;
    if (!Context.Found) {
        xrtClearError();
        return xhcl_empty();
    }
    xrtClearError();
    return xhcl_store(Input, Context.Value, false);
}



int64_t xhcl_get_int(cstr sHcl, cstr sKey)
{
    cstr sValue = xhcl_get(sHcl, sKey);
    int64 iValue;
    bool bInvalid;

    if (sValue == NULL || xrtGetError() != NULL || sValue[0] == 0) {
        xrtFree((ptr)sValue);
        return 0;
    }
    bInvalid = !xrtIntParse((xstrview){sValue, strlen(sValue)},
        10u, 0u, &iValue);
    xrtFree((ptr)sValue);
    if (bInvalid) {
        xhcl_error(XERR_VALUE, "HCL attribute is not an integer");
        return 0;
    }
    return iValue;
}



double xhcl_get_float(cstr sHcl, cstr sKey)
{
    cstr sValue = xhcl_get(sHcl, sKey);
    double fValue;
    bool bInvalid;

    if (sValue == NULL || xrtGetError() != NULL || sValue[0] == 0) {
        xrtFree((ptr)sValue);
        return 0.0;
    }
    bInvalid = !xrtNumParse((xstrview){sValue, strlen(sValue)}, 0, &fValue);
    xrtFree((ptr)sValue);
    if (bInvalid) {
        xhcl_error(XERR_VALUE, "HCL attribute is not a float");
        return 0.0;
    }
    return fValue;
}



bool xhcl_get_bool(cstr sHcl, cstr sKey)
{
    cstr sValue = xhcl_get(sHcl, sKey);
    bool bValue;

    if (sValue == NULL || xrtGetError() != NULL || sValue[0] == 0) {
        xrtFree((ptr)sValue);
        return false;
    }
    bValue = strcmp(sValue, "true") == 0;
    if (bValue || strcmp(sValue, "false") == 0) {
        xrtFree((ptr)sValue);
        return bValue;
    }
    xrtFree((ptr)sValue);
    xhcl_error(XERR_VALUE, "HCL attribute is not a bool");
    return false;
}



bool xhcl_has(cstr sHcl, cstr sKey)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclattributecontext Context;

    if (sKey == NULL || sKey[0] == 0) {
        xhcl_error(XERR_ARGUMENT, "HCL attribute name is required");
        return false;
    }
    memset(&Context, 0, sizeof(Context));
    Context.Name = sKey;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_find_attribute_visit, NULL,
            &Context, true)) {
        return false;
    }
    xrtClearError();
    return Context.Found;
}



int64_t xhcl_count_blocks(cstr sHcl, cstr sType)
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



char* xhcl_block_label(cstr sHcl, cstr sType, int64_t iIndex)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclblockcontext Context;

    if (sType == NULL || sType[0] == 0 || iIndex < 0) {
        xhcl_error(XERR_ARGUMENT, "HCL block type and non-negative index are required");
        return NULL;
    }
    memset(&Context, 0, sizeof(Context));
    Context.Type = sType;
    Context.Index = (size_t)iIndex;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, NULL, xhcl_find_block_visit,
            &Context, true)) return NULL;
    if (!Context.Found || Context.Block.LabelCount == 0u) {
        xrtClearError();
        return xhcl_empty();
    }
    xrtClearError();
    return xhcl_store(Input, Context.Block.Labels[0], true);
}



int64_t xhcl_block_label_count(cstr sHcl, cstr sType, int64_t iIndex)
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
            &Context, true)) return -1;
    xrtClearError();
    return Context.Found ? (long long)Context.Block.LabelCount : 0;
}



char* xhcl_block_label_at(cstr sHcl, cstr sType, int64_t iIndex, int64_t iLabel)
{
    xhclinput Input = xhcl_input(sHcl);
    xhclblockcontext Context;

    if (sType == NULL || sType[0] == 0 || iIndex < 0 || iLabel < 0) {
        xhcl_error(XERR_ARGUMENT, "HCL block type and non-negative indexes are required");
        return NULL;
    }
    memset(&Context, 0, sizeof(Context));
    Context.Type = sType;
    Context.Index = (size_t)iIndex;
    if (!xhcl_walk_scope(Input, 0u, Input.Size, NULL, xhcl_find_block_visit,
            &Context, true)) return NULL;
    if (!Context.Found || (uint64)iLabel >= Context.Block.LabelCount) {
        xrtClearError();
        return xhcl_empty();
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



int64_t xhcl_count_attrs(cstr sHcl)
{
    xhclinput Input = xhcl_input(sHcl);
    size_t iCount = 0u;

    if (!xhcl_walk_scope(Input, 0u, Input.Size, xhcl_count_attribute_visit, NULL,
            &iCount, true)) return -1;
    xrtClearError();
    return (long long)iCount;
}
