/*
 * chardet_native.c - xlang 对 XRT Unicode 编码探测的薄封装。
 *
 * 探测复用 XRT 的 BOM/合法性/零字节分布规则，不推测 GBK、Shift-JIS。
 * 探测结果不是正文合法性证明；isUtf8 单独严格校验全部输入。
 */
#include "include/chardet.h"

static const char* xchardetName(xencoding Encoding, size_t iBomSize)
{
    switch ( Encoding ) {
    case XENCODING_UTF8:
        return iBomSize != 0u ? "UTF-8-BOM" : "UTF-8";
    case XENCODING_UTF16_LE:
        return "UTF-16LE";
    case XENCODING_UTF16_BE:
        return "UTF-16BE";
    case XENCODING_UTF32_LE:
        return "UTF-32LE";
    case XENCODING_UTF32_BE:
        return "UTF-32BE";
    default:
        return "UNKNOWN";
    }
}

static xencodingguess xchardetGuess(const void* data, size_t size)
{
    xrtClearError();
    return xrtEncodingGuess((xbytesview){(cbytes)data, size});
}

static const char* xchardetDetect(const void* data, size_t size)
{
    xencodingguess guess = xchardetGuess(data, size);
    return xchardetName(guess.Encoding, guess.BomSize);
}

static bool xchardetUtf8(const void* data, size_t size)
{
    xrtClearError();
    return xrtUtf8Valid((xstrview){(cstr)data, size}, NULL);
}

static bool xchardetAscii(const void* data, size_t size)
{
    xrtClearError();
    /* 非空长度的空指针仍交由 XRT 生成统一参数错误。 */
    if (data == NULL && size != 0) return xchardetUtf8(data, size);
    for (size_t i = 0; i < size; ++i) {
        if (((const unsigned char*)data)[i] >= 128u) return false;
    }
    return true;
}

/* Buffer 和 string 的两组声明保持各自真实 C 类型；不靠冲突原型强转。 */
const char* xchardet_detect_name_n(void* data, size_t size)
{ return xchardetDetect(data, size); }
int xchardet_detect_conf_n(void* data, size_t size)
{ return xchardetGuess(data, size).Confidence; }
int xchardet_detect_bom_size_n(void* data, size_t size)
{ return (int)xchardetGuess(data, size).BomSize; }
bool xchardet_is_utf8_n(void* data, size_t size)
{ return xchardetUtf8(data, size); }
bool xchardet_is_ascii_n(void* data, size_t size)
{ return xchardetAscii(data, size); }

const char* xchardet_detect_name_str(const char* data, size_t size)
{ return xchardetDetect(data, size); }
int xchardet_detect_conf_str(const char* data, size_t size)
{ return xchardetGuess(data, size).Confidence; }
int xchardet_detect_bom_size_str(const char* data, size_t size)
{ return (int)xchardetGuess(data, size).BomSize; }
bool xchardet_is_utf8_str(const char* data, size_t size)
{ return xchardetUtf8(data, size); }
bool xchardet_is_ascii_str(const char* data, size_t size)
{ return xchardetAscii(data, size); }
