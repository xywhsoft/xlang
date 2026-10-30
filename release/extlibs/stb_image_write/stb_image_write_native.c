/* 图片编码只使用内存回调；UTF-8 文件路径由 XRT 统一处理。 */
#include <limits.h>
#include <stdio.h>

#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#include "xrt.h"
#include "include/stb_image_write_native.h"

#define XSTBIW_DOMAIN "xlang.extlib.stb_image_write"
#define XSTBIW_PIXELS_LIMIT (64u * 1024u * 1024u)
#define XSTBIW_OUTPUT_LIMIT (128u * 1024u * 1024u)

/* stb 内置压缩缓冲在扩容失败时仍会写入，PNG 改用 XRT 的有界 zlib 编码。 */
static unsigned char* xstbiw_deflate(unsigned char* Data, int Length,
    int* OutputLength, int Quality)
{
    xdeflateconfig Config;
    size_t Size = 0u;
    bytes Output;
    xrtDeflateConfigInit(&Config);
    Config.Format = XDEFLATE_ZLIB;
    Config.Level = Quality < 0 ? 0 : (Quality > 10 ? 10 : Quality);
    Config.OutputLimit = XSTBIW_OUTPUT_LIMIT;
    Output = xrtDeflateAll((xbytesview){Data, (size_t)Length}, &Config, &Size);
    if (Output == NULL) return NULL;
    if (Size > INT_MAX) {
        xrtFree(Output);
        xrtSetErrorInfo(XERR_RANGE, XSTBIW_DOMAIN, 0, "compressed PNG exceeds limit");
        return NULL;
    }
    *OutputLength = (int)Size;
    return Output;
}

#define STBIW_MALLOC(size) xrtMalloc(size)
#define STBIW_REALLOC(ptr, size) xrtRealloc(ptr, size)
#define STBIW_FREE(ptr) xrtFree(ptr)
#define STBIW_ZLIB_COMPRESS xstbiw_deflate
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

typedef struct xstbiw_sink {
    xbuffer* Buffer;
    bool Failed;
    bool Limited;
} xstbiw_sink;

static void xstbiw_error(xerrkind Kind, const char* Message)
{
    xrtSetErrorInfo(Kind, XSTBIW_DOMAIN, 0, Message);
}

static void xstbiw_append(void* Context, void* Data, int Size)
{
    xstbiw_sink* Sink = (xstbiw_sink*)Context;
    if (Sink->Failed || Size <= 0) return;
    if (Sink->Buffer->Size > XSTBIW_OUTPUT_LIMIT ||
        (size_t)Size > XSTBIW_OUTPUT_LIMIT - Sink->Buffer->Size) {
        Sink->Limited = true;
        Sink->Failed = true;
        return;
    }
    if (!xrtBufferAppend(Sink->Buffer, (xbytesview){ Data, (size_t)Size }))
        Sink->Failed = true;
}

/* 调用前已限制尺寸：RGB 行按四字节对齐，RGBA 使用 V4 头。 */
static size_t xstbiw_bmp_size(int Width, int Height, int Channels)
{
    size_t Row = ((size_t)Width * (Channels == 4 ? 4u : 3u) + 3u) & ~(size_t)3u;
    return (Channels == 4 ? 122u : 54u) + Row * (size_t)Height;
}

static bool xstbiw_validate(int Format, void* Pixels, size_t Length,
    int Width, int Height, int Channels, int Option)
{
    size_t Row;
    size_t Required;

    if (Format < XSTBIW_PNG || Format > XSTBIW_HDR || Pixels == NULL ||
        Width <= 0 || Height <= 0 || Channels < 1 || Channels > 4) {
        xstbiw_error(XERR_ARGUMENT, "invalid image format, dimensions or pixels");
        return false;
    }
    /* 限定 stb 内部的 int 乘法及临时分配规模。 */
    if (Width > 16384 || Height > 16384 ||
        (size_t)Width > XSTBIW_PIXELS_LIMIT / (size_t)Height /
            ((size_t)Channels * (Format == XSTBIW_HDR ? sizeof(float) : 1u))) {
        xstbiw_error(XERR_RANGE, "image exceeds 64 MiB pixel limit");
        return false;
    }
    Row = (size_t)Width * (size_t)Channels;
    if (Format == XSTBIW_HDR) Row *= sizeof(float);
    if (Format == XSTBIW_PNG) {
        size_t Step;
        if (Option < 0 || (Option != 0 && (size_t)Option < Row)) {
            xstbiw_error(XERR_RANGE, "PNG stride is smaller than one pixel row");
            return false;
        }
        Step = Option == 0 ? Row : (size_t)Option;
        if (Height > 1 && Step > (XSTBIW_PIXELS_LIMIT - Row) /
                (size_t)(Height - 1)) {
            xstbiw_error(XERR_RANGE, "PNG stride exceeds 64 MiB pixel limit");
            return false;
        }
        Required = (size_t)(Height - 1) * Step + Row;
    } else {
        if (Format == XSTBIW_JPEG && (Option < 1 || Option > 100)) {
            xstbiw_error(XERR_RANGE, "JPEG quality must be between 1 and 100");
            return false;
        }
        Required = (size_t)Height * Row;
    }
    if (Required > XSTBIW_PIXELS_LIMIT || Length < Required) {
        xstbiw_error(XERR_RANGE, "pixel buffer is too short or exceeds 64 MiB");
        return false;
    }
    /* BMP 的通道展开和行填充能精确计数，超限时不分配输出或扫描源像素。 */
    if (Format == XSTBIW_BMP && xstbiw_bmp_size(Width, Height, Channels) > XSTBIW_OUTPUT_LIMIT) {
        xstbiw_error(XERR_RANGE, "encoded image exceeds 128 MiB");
        return false;
    }
    return true;
}

int xstbiw_available(void)
{
    return 1;
}

const char* xstbiw_version(void)
{
    return "stb_image_write 1.16";
}

void* xstbiw_encode(int Format, void* Pixels, size_t Length,
    int Width, int Height, int Channels, int Option)
{
    xbuffer* Output;
    xstbiw_sink Sink;
    int Ok = 0;

    xrtClearError();
    if (!xstbiw_validate(Format, Pixels, Length, Width, Height, Channels, Option))
        return NULL;
    Output = xrtBufferCreate();
    /* 已知大小的 BMP 只预留一次，不让逐字节回调反复扩容。 */
    if (Output == NULL || (Format == XSTBIW_BMP &&
            !xrtBufferReserve(Output, xstbiw_bmp_size(Width, Height, Channels)))) {
        xrtBufferDestroy(Output);
        xstbiw_error(XERR_MEMORY, "could not allocate encoded image buffer");
        return NULL;
    }
    Sink.Buffer = Output;
    Sink.Failed = false;
    Sink.Limited = false;
    switch (Format) {
    case XSTBIW_PNG:
        Ok = stbi_write_png_to_func(xstbiw_append, &Sink,
            Width, Height, Channels, Pixels, Option);
        break;
    case XSTBIW_JPEG:
        Ok = stbi_write_jpg_to_func(xstbiw_append, &Sink,
            Width, Height, Channels, Pixels, Option);
        break;
    case XSTBIW_BMP:
        Ok = stbi_write_bmp_to_func(xstbiw_append, &Sink,
            Width, Height, Channels, Pixels);
        break;
    case XSTBIW_TGA:
        Ok = stbi_write_tga_to_func(xstbiw_append, &Sink,
            Width, Height, Channels, Pixels);
        break;
    case XSTBIW_HDR:
        Ok = stbi_write_hdr_to_func(xstbiw_append, &Sink,
            Width, Height, Channels, (const float*)Pixels);
        break;
    }
    if (Sink.Failed || !Ok || Output->Size == 0u) {
        xrtBufferDestroy(Output);
        if (Sink.Limited)
            xstbiw_error(XERR_RANGE, "encoded image exceeds 128 MiB");
        else if (xrtGetError() == NULL)
            xstbiw_error(Sink.Failed ? XERR_MEMORY : XERR_VALUE,
                Sink.Failed ? "could not allocate encoded image" :
                              "image encoder failed");
        return NULL;
    }
    return Output;
}

int xstbiw_write(const char* Path, int Format, void* Pixels,
    size_t Length, int Width, int Height, int Channels, int Option)
{
    xbuffer* Encoded;
    bool Ok;
    xrtClearError();
    if (Path == NULL || Path[0] == '\0') {
        xstbiw_error(XERR_ARGUMENT, "empty output path");
        return 0;
    }
    Encoded = (xbuffer*)xstbiw_encode(Format, Pixels, Length,
        Width, Height, Channels, Option);
    if (Encoded == NULL) return 0;
    Ok = xrtFileWriteAtomic(Path, xrtBufferView(Encoded));
    xrtBufferDestroy(Encoded);
    if (!Ok && xrtGetError() == NULL)
        xstbiw_error(XERR_IO, "could not write encoded image");
    return Ok ? 1 : 0;
}
