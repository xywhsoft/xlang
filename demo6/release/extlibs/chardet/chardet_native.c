/*
 * chardet_native.c - xlang 对 XRT Unicode 编码探测的薄封装。
 *
 * 只报告 XRT 能够严格确认的 Unicode 编码，不把不可靠的字节模式伪装成
 * GBK、Shift-JIS 等结论。调用方需要旧式本地编码猜测时，应使用专门词典库。
 */
#define XRT_MODULE_CHARSET_DETECT
#include "xrt.h"

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

const char* xchardet_detect_name_n(const void* pData, uint64 iSize)
{
    xencodingguess Guess = xrtEncodingGuess((xbytesview){
        (cbytes)pData, (size_t)iSize
    });
    return xchardetName(Guess.Encoding, Guess.BomSize);
}

int64 xchardet_detect_conf_n(const void* pData, uint64 iSize)
{
    xencodingguess Guess = xrtEncodingGuess((xbytesview){
        (cbytes)pData, (size_t)iSize
    });
    return (int64)Guess.Confidence;
}

int64 xchardet_detect_bom_size_n(const void* pData, uint64 iSize)
{
    xencodingguess Guess = xrtEncodingGuess((xbytesview){
        (cbytes)pData, (size_t)iSize
    });
    return (int64)Guess.BomSize;
}
