/* RESP2 编解码：连接由 XL 的 TcpStream 持有，此处只处理长度明确的字节序列。 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "include/redis_native.h"

#define XR_DOMAIN "xlang.extlib.redis"
#define XR_MAX_BYTES (64u * 1024u * 1024u)
#define XR_MAX_ITEMS 100000u
#define XR_MAX_DEPTH 64u

typedef enum xrstatus {
    XR_OK,
    XR_INCOMPLETE,
    XR_INVALID
} xrstatus;

typedef struct xrreader {
    const char* Data;
    size_t Size;
    size_t Pos;
    size_t Nodes;
} xrreader;

static void xr_error(xerrkind Kind, const char* Message)
{
    if (xrtGetError() == NULL) xrtSetErrorInfo(Kind, XR_DOMAIN, 0, Message);
}

static xvalue* xr_text(const char* Data, size_t Size)
{
    return xrtValueString((xstrview){ Data, Size });
}

static xvalue* xr_blob(const char* Data, size_t Size)
{
    return xrtUtf8Valid((xstrview){ Data, Size }, NULL) ? xr_text(Data, Size)
        : xrtValueBytes((xbytesview){ (cbytes)Data, Size });
}

static bool xr_append(xbuffer* Output, const char* Data, size_t Size)
{
    if (Size > XR_MAX_BYTES || Output->Size > XR_MAX_BYTES - Size) {
        xr_error(XERR_RANGE, "RESP command exceeds 64 MiB");
        return false;
    }
    return xrtBufferAppend(Output, (xbytesview){ (cbytes)Data, Size });
}

static bool xr_decimal(xbuffer* Output, char Prefix, size_t Number)
{
    char Line[64];
    int Size = snprintf(Line, sizeof(Line), "%c%llu\r\n", Prefix,
        (unsigned long long)Number);
    return Size > 0 && (size_t)Size < sizeof(Line) &&
        xr_append(Output, Line, (size_t)Size);
}

static bool xr_encode_into(xvalue* Args, xbuffer* Output, bool AllowBytes)
{
    size_t Count;
    size_t i;

    if (Args == NULL || xrtValueType(Args) != XVALUE_ARRAY) {
        xr_error(XERR_ARGUMENT, "RESP command requires an array");
        return false;
    }
    Count = xrtValueCount(Args);
    if (Count == 0u || Count > XR_MAX_ITEMS) {
        xr_error(XERR_RANGE, "RESP command argument count is out of range");
        return false;
    }
    if (!xr_decimal(Output, '*', Count)) return false;
    for (i = 0u; i < Count; ++i) {
        const void* Data;
        size_t Size;
        xvalue* Item = xrtValueArrayAt(Args, (int64)i);
        if (Item != NULL && xrtValueType(Item) == XVALUE_STRING) {
            xstrview Text;
            if (!xrtValueGetString(Item, &Text)) return false;
            Data = Text.Data;
            Size = Text.Size;
        } else if (AllowBytes && Item != NULL &&
                   xrtValueType(Item) == XVALUE_BYTES) {
            xbytesview Bytes;
            if (!xrtValueGetBytes(Item, &Bytes)) return false;
            Data = Bytes.Data;
            Size = Bytes.Size;
        } else {
            xr_error(XERR_TYPE, AllowBytes ?
                "RESP command arguments must be strings or bytes" :
                "RESP command arguments must be strings");
            return false;
        }
        if (!xr_decimal(Output, '$', Size) ||
            !xr_append(Output, Data, Size) ||
            !xr_append(Output, "\r\n", 2u)) return false;
    }
    return true;
}

void* xr_encode_bytes(xvalue* Args)
{
    xbuffer* Output;

    xrtClearError();
    Output = xrtBufferCreate();
    if (Output != NULL && xr_encode_into(Args, Output, true)) return Output;
    xrtBufferDestroy(Output);
    xr_error(XERR_MEMORY, "could not encode RESP command");
    return NULL;
}

xvalue* xr_encode(xvalue* Args)
{
    xbuffer Output;
    size_t Size;
    str Owned;
    xvalue* Result;

    xrtClearError();
    (void)xrtBufferInit(&Output);
    if (!xr_encode_into(Args, &Output, false)) goto Failed;
    Size = Output.Size;
    if (!xrtBufferAppendByte(&Output, 0u)) goto Failed;
    Owned = (str)xrtBufferTake(&Output, NULL, NULL);
    if (Owned == NULL) goto Failed;
    Result = xrtValueStringTake(&Owned, Size);
    xrtFree(Owned);
    if (Result != NULL) return Result;

Failed:
    xrtBufferUnit(&Output);
    xr_error(XERR_MEMORY, "could not encode RESP command");
    return xrtValueNull();
}

/* 十进制检查放在有符号范围内，避免超长长度字段溢出后变成小正数。 */
static bool xr_integer(const char* Data, size_t Size, int64* Result)
{
    uint64 Number = 0u;
    uint64 Limit = (uint64)INT64_MAX;
    size_t i = 0u;
    bool Negative = false;

    if (Size != 0u && (Data[0] == '-' || Data[0] == '+')) {
        Negative = Data[0] == '-';
        i = 1u;
    }
    if (Negative) ++Limit;
    if (i == Size) return false;
    for (; i < Size; ++i) {
        uint8 Digit;
        if (Data[i] < '0' || Data[i] > '9') return false;
        Digit = (uint8)(Data[i] - '0');
        if (Number > (Limit - Digit) / 10u) return false;
        Number = Number * 10u + Digit;
    }
    *Result = Negative ? (Number == (uint64)INT64_MAX + 1u ? INT64_MIN : -(int64)Number)
                       : (int64)Number;
    return true;
}

static xrstatus xr_line(xrreader* Reader, const char** Data, size_t* Size)
{
    size_t i;
    for (i = Reader->Pos; i < Reader->Size; ++i) {
        if (Reader->Data[i] == '\n') return XR_INVALID;
        if (Reader->Data[i] != '\r') continue;
        if (i + 1u == Reader->Size) return XR_INCOMPLETE;
        if (Reader->Data[i + 1u] != '\n') return XR_INVALID;
        *Data = Reader->Data + Reader->Pos;
        *Size = i - Reader->Pos;
        Reader->Pos = i + 2u;
        return XR_OK;
    }
    return XR_INCOMPLETE;
}

/* Take 接口仅在成功时转移所有权；所有失败路径由此函数统一清理。 */
static xvalue* xr_tagged(const char* Type, xvalue* Payload)
{
    xvalue* Result = xrtValueObject();
    xvalue* Tag = xr_text(Type, strlen(Type));
    if (Result == NULL || Tag == NULL || Payload == NULL ||
        !xrtValueObjectSetTake(Result, XRT_STR_LITERAL("type"), &Tag) ||
        !xrtValueObjectSetTake(Result, XRT_STR_LITERAL("value"), &Payload)) {
        xrtValueRelease(Tag);
        xrtValueRelease(Payload);
        xrtValueRelease(Result);
        return NULL;
    }
    return Result;
}

static xrstatus xr_parse(xrreader* Reader, unsigned Depth, bool Build, xvalue** Out)
{
    const char* Line;
    size_t LineSize;
    char Type;
    int64 Number;
    xrstatus Status;
    xvalue* Payload = NULL;
    const char* Name;

    if (Depth >= XR_MAX_DEPTH) return XR_INVALID;
    if (Reader->Pos == Reader->Size) return XR_INCOMPLETE;
    if (Reader->Nodes >= XR_MAX_ITEMS) return XR_INVALID;
    ++Reader->Nodes;
    Type = Reader->Data[Reader->Pos++];
    if (Type != '+' && Type != '-' && Type != ':' && Type != '$' && Type != '*')
        return XR_INVALID;
    Status = xr_line(Reader, &Line, &LineSize);
    if (Status != XR_OK) return Status;

    if (Type == '+' || Type == '-') {
        Name = Type == '+' ? "simple" : "error";
        if (Build) Payload = xr_blob(Line, LineSize);
    } else {
        if (!xr_integer(Line, LineSize, &Number)) return XR_INVALID;
        if (Type == ':') {
            Name = "integer";
            if (Build) Payload = xrtValueInt(Number);
        } else if (Number == -1) {
            Name = "null";
            if (Build) Payload = xrtValueRetain(xrtValueNull());
        } else if (Number < 0 || (uint64)Number > XR_MAX_BYTES ||
                   (Type == '*' && (uint64)Number > XR_MAX_ITEMS)) {
            return XR_INVALID;
        } else if (Type == '$') {
            size_t Size = (size_t)Number;
            Name = "bulk";
            if (Reader->Size - Reader->Pos < Size + 2u) return XR_INCOMPLETE;
            if (Reader->Data[Reader->Pos + Size] != '\r' ||
                Reader->Data[Reader->Pos + Size + 1u] != '\n') return XR_INVALID;
            if (Build) Payload = xr_blob(Reader->Data + Reader->Pos, Size);
            Reader->Pos += Size + 2u;
        } else {
            size_t i;
            Name = "array";
            if (Build) {
                Payload = xrtValueArray();
                if (Payload == NULL) return XR_INVALID;
            }
            for (i = 0u; i < (size_t)Number; ++i) {
                xvalue* Child = NULL;
                Status = xr_parse(Reader, Depth + 1u, Build, &Child);
                if (Status != XR_OK ||
                    (Build && !xrtValueArrayAppendTake(Payload, &Child))) {
                    xrtValueRelease(Child);
                    xrtValueRelease(Payload);
                    return Status == XR_OK ? XR_INVALID : Status;
                }
            }
        }
    }
    if (Build) {
        *Out = xr_tagged(Name, Payload);
        if (*Out == NULL) return XR_INVALID;
    }
    return XR_OK;
}

static bool xr_input_raw(const void* Data, size_t Size, xrreader* Reader)
{
    if (Data == NULL && Size != 0u) {
        xr_error(XERR_ARGUMENT, "RESP reply data is null");
        return false;
    }
    if (Size > XR_MAX_BYTES) {
        xr_error(XERR_RANGE, "RESP reply exceeds 64 MiB");
        return false;
    }
    Reader->Data = Data != NULL ? (const char*)Data : "";
    Reader->Size = Size;
    Reader->Pos = 0u;
    Reader->Nodes = 0u;
    return true;
}

static bool xr_input(const xvalue* Reply, xrreader* Reader)
{
    xstrview Text;
    if (Reply == NULL || !xrtValueGetString(Reply, &Text)) {
        xr_error(XERR_ARGUMENT, "RESP reply must be a string");
        return false;
    }
    return xr_input_raw(Text.Data, Text.Size, Reader);
}

static xvalue* xr_decode_reader(xrreader* Reader)
{
    xrstatus Status;
    xvalue* Item = NULL;
    xvalue* Rest;
    xvalue* Result;

    Status = xr_parse(Reader, 0u, true, &Item);
    if (Status != XR_OK) {
        xr_error(XERR_VALUE, Status == XR_INCOMPLETE ?
            "RESP reply is incomplete" : "RESP reply is malformed");
        return xrtValueNull();
    }
    Result = xr_tagged("reply", Item);
    Rest = xr_blob(Reader->Data + Reader->Pos, Reader->Size - Reader->Pos);
    if (Result == NULL || Rest == NULL ||
        !xrtValueObjectSetTake(Result, XRT_STR_LITERAL("rest"), &Rest)) {
        xrtValueRelease(Rest);
        xrtValueRelease(Result);
        xr_error(XERR_MEMORY, "could not decode RESP reply");
        return xrtValueNull();
    }
    return Result;
}

static bool xr_complete_reader(xrreader* Reader)
{
    xrstatus Status;

    Status = xr_parse(Reader, 0u, false, NULL);
    if (Status == XR_INVALID)
        xr_error(XERR_VALUE, "RESP reply is malformed");
    return Status == XR_OK;
}

xvalue* xr_decode(xvalue* Reply)
{
    xrreader Reader;
    xrtClearError();
    return xr_input(Reply, &Reader) ? xr_decode_reader(&Reader) : xrtValueNull();
}

bool xr_is_complete(xvalue* Reply)
{
    xrreader Reader;
    xrtClearError();
    return xr_input(Reply, &Reader) && xr_complete_reader(&Reader);
}

xvalue* xr_decode_raw(void* Data, size_t Size)
{
    xrreader Reader;
    xrtClearError();
    return xr_input_raw(Data, Size, &Reader) ? xr_decode_reader(&Reader) : xrtValueNull();
}

bool xr_is_complete_raw(void* Data, size_t Size)
{
    xrreader Reader;
    xrtClearError();
    return xr_input_raw(Data, Size, &Reader) && xr_complete_reader(&Reader);
}
