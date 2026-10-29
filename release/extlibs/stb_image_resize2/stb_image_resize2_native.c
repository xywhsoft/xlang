/* 缩放结果由 XRT Buffer 持有，输入只借用，并在调用 stb 前校验长度。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#include "xrt.h"
#include "include/stb_image_resize2_native.h"

#define STBIR_MALLOC(size, user_data) ((void)(user_data), xrtMalloc(size))
#define STBIR_FREE(ptr, user_data) ((void)(user_data), xrtFree(ptr))
#define STBIR_NO_SIMD
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "third_party/stb_image_resize2.h"

#define XSTBIR_DOMAIN "xlang.extlib.stb_image_resize2"
#define XSTBIR_PIXELS_LIMIT (64u * 1024u * 1024u)

static void xstbir_error(xerrkind Kind, const char* Message)
{
    xrtSetErrorInfo(Kind, XSTBIR_DOMAIN, 0, Message);
}

static stbir_pixel_layout xstbir_layout(int Channels)
{
    if (Channels == 1) return (stbir_pixel_layout)1;
    /* stb_image 的双通道像素是灰度+alpha，不能按两个普通颜色通道缩放。 */
    if (Channels == 2) return STBIR_RA;
    if (Channels == 3) return STBIR_RGB;
    return STBIR_RGBA;
}

int xstbir_available(void)
{
    return 1;
}

const char* xstbir_version(void)
{
    return "stb_image_resize2 2.18";
}

void* xstbir_resize(void* Pixels, size_t Length, int Width, int Height,
    int Channels, int OutWidth, int OutHeight, int InputStride, int Linear)
{
    size_t Row;
    size_t Step;
    size_t OutputSize;
    xbuffer* Output;
    void* Result;

    xrtClearError();
    if (Pixels == NULL || Width <= 0 || Height <= 0 ||
        OutWidth <= 0 || OutHeight <= 0 || Channels < 1 || Channels > 4 ||
        (Linear != 0 && Linear != 1)) {
        xstbir_error(XERR_ARGUMENT, "invalid resize dimensions or pixels");
        return NULL;
    }
    if (Width > 16384 || Height > 16384 ||
        OutWidth > 16384 || OutHeight > 16384 ||
        (size_t)Width > XSTBIR_PIXELS_LIMIT / (size_t)Height / (size_t)Channels ||
        (size_t)OutWidth > XSTBIR_PIXELS_LIMIT / (size_t)OutHeight / (size_t)Channels) {
        xstbir_error(XERR_RANGE, "image exceeds 64 MiB pixel limit");
        return NULL;
    }
    Row = (size_t)Width * (size_t)Channels;
    if (InputStride < 0 || (InputStride != 0 && (size_t)InputStride < Row)) {
        xstbir_error(XERR_RANGE, "input stride is smaller than one pixel row");
        return NULL;
    }
    Step = InputStride == 0 ? Row : (size_t)InputStride;
    if (Height > 1 && Step > (XSTBIR_PIXELS_LIMIT - Row) / (size_t)(Height - 1)) {
        xstbir_error(XERR_RANGE, "input stride exceeds 64 MiB pixel limit");
        return NULL;
    }
    if (Length < (size_t)(Height - 1) * Step + Row) {
        xstbir_error(XERR_RANGE, "input pixel buffer is too short");
        return NULL;
    }

    OutputSize = (size_t)OutWidth * (size_t)OutHeight * (size_t)Channels;
    Output = xrtBufferCreate();
    if (Output == NULL || xrtBufferAdd(Output, OutputSize) == NULL) {
        xrtBufferDestroy(Output);
        xstbir_error(XERR_MEMORY, "could not allocate resized pixels");
        return NULL;
    }
    Result = Linear != 0
        ? stbir_resize_uint8_linear((const unsigned char*)Pixels,
            Width, Height, InputStride, Output->Data,
            OutWidth, OutHeight, 0, xstbir_layout(Channels))
        : stbir_resize_uint8_srgb((const unsigned char*)Pixels,
            Width, Height, InputStride, Output->Data,
            OutWidth, OutHeight, 0, xstbir_layout(Channels));
    if (Result == NULL) {
        int OutOfMemory = xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY;
        xrtBufferDestroy(Output);
        if (OutOfMemory)
            xstbir_error(XERR_MEMORY, "could not allocate resize workspace");
        else
            xstbir_error(XERR_VALUE, "image resize failed");
        return NULL;
    }
    return Output;
}
