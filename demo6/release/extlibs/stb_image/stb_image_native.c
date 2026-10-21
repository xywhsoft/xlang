/* stb_image 只负责解码；文件访问与错误状态统一交给 XRT。 */
#include <limits.h>
#include <stdio.h>

#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#include "xrt.h"
#include "include/stb_image_native.h"

#define STBI_MALLOC(size) xrtMalloc(size)
#define STBI_REALLOC(ptr, size) xrtRealloc(ptr, size)
#define STBI_FREE(ptr) xrtFree(ptr)
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#define XSTBI_DOMAIN "xlang.extlib.stb_image"
#define XSTBI_INPUT_LIMIT (128u * 1024u * 1024u)
#define XSTBI_OUTPUT_LIMIT (256u * 1024u * 1024u)

typedef struct xstbiimage {
    int Width;
    int Height;
    int Channels;
    int IsFloat;
    size_t Size;
    void* Data;
} xstbiimage;

static void xstbi_error(xerrkind Kind, const char* Message)
{
    if (xrtGetError() == NULL)
        xrtSetErrorInfo(Kind, XSTBI_DOMAIN, 0, Message);
}

static bool xstbi_input(void* Data, size_t Size)
{
    if (Data == NULL || Size == 0u) {
        xstbi_error(XERR_ARGUMENT, "image data is empty");
        return false;
    }
    if (Size > XSTBI_INPUT_LIMIT || Size > INT_MAX) {
        xstbi_error(XERR_RANGE, "encoded image exceeds 128 MiB");
        return false;
    }
    return true;
}

static bool xstbi_dimensions(void* Data, size_t Size,
    int Desired, int IsFloat, int* Width, int* Height, int* Channels,
    size_t* OutputSize)
{
    size_t Pixels;
    size_t PixelSize;
    int SourceChannels;

    if (!xstbi_input(Data, Size)) return false;
    if (Desired < 0 || Desired > 4) {
        xstbi_error(XERR_RANGE, "desired channels must be between 0 and 4");
        return false;
    }
    if (!stbi_info_from_memory((const stbi_uc*)Data, (int)Size,
            Width, Height, &SourceChannels)) {
        xstbi_error(XERR_VALUE, stbi_failure_reason());
        return false;
    }
    if (*Width <= 0 || *Height <= 0 || SourceChannels < 1 || SourceChannels > 4) {
        xstbi_error(XERR_VALUE, "image dimensions or channels are invalid");
        return false;
    }
    *Channels = Desired != 0 ? Desired : SourceChannels;
    PixelSize = (size_t)*Channels * (IsFloat != 0 ? sizeof(float) : 1u);
    if ((size_t)*Width > XSTBI_OUTPUT_LIMIT / (size_t)*Height / PixelSize) {
        xstbi_error(XERR_RANGE, "decoded image exceeds 256 MiB");
        return false;
    }
    Pixels = (size_t)*Width * (size_t)*Height;
    *OutputSize = Pixels * PixelSize;
    xrtClearError();
    return true;
}

static void* xstbi_decode(void* Data, size_t Size, int Desired, int IsFloat)
{
    int Width;
    int Height;
    int Channels;
    size_t OutputSize;
    void* Pixels;
    xstbiimage* Image;

    if (!xstbi_dimensions(Data, Size, Desired, IsFloat,
            &Width, &Height, &Channels, &OutputSize)) return NULL;
    Pixels = IsFloat != 0
        ? (void*)stbi_loadf_from_memory((const stbi_uc*)Data, (int)Size,
            &Width, &Height, &Channels, Desired)
        : (void*)stbi_load_from_memory((const stbi_uc*)Data, (int)Size,
            &Width, &Height, &Channels, Desired);
    if (Pixels == NULL) {
        xstbi_error(XERR_VALUE, stbi_failure_reason());
        return NULL;
    }
    Image = (xstbiimage*)xrtCalloc(1u, sizeof(*Image));
    if (Image == NULL) {
        stbi_image_free(Pixels);
        xstbi_error(XERR_MEMORY, "could not allocate image handle");
        return NULL;
    }
    Image->Width = Width;
    Image->Height = Height;
    Image->Channels = Desired != 0 ? Desired : Channels;
    Image->IsFloat = IsFloat != 0;
    Image->Size = OutputSize;
    Image->Data = Pixels;
    xrtClearError();
    return Image;
}

static bytes xstbi_read(const char* Path, size_t* Size)
{
    bytes Data;
    if (Path == NULL || Path[0] == '\0') {
        xstbi_error(XERR_ARGUMENT, "image path is empty");
        return NULL;
    }
    Data = xrtFileReadAllLimit(Path, XSTBI_INPUT_LIMIT, Size);
    if (Data == NULL) xstbi_error(XERR_IO, "could not read image file");
    return Data;
}

bool xstbi_available(void)
{
    return true;
}

const char* xstbi_version(void)
{
    return "stb_image 2.30";
}

void* xstbi_load_memory(void* Data, size_t Size, int Desired, int IsFloat)
{
    xrtClearError();
    return xstbi_decode(Data, Size, Desired, IsFloat);
}

void* xstbi_load(const char* Path, int Desired)
{
    size_t Size;
    bytes Data;
    void* Image;

    xrtClearError();
    Data = xstbi_read(Path, &Size);
    if (Data == NULL) return NULL;
    Image = xstbi_decode(Data, Size, Desired, 0);
    xrtFree(Data);
    return Image;
}

void* xstbi_loadf(const char* Path, int Desired)
{
    size_t Size;
    bytes Data;
    void* Image;

    xrtClearError();
    Data = xstbi_read(Path, &Size);
    if (Data == NULL) return NULL;
    Image = xstbi_decode(Data, Size, Desired, 1);
    xrtFree(Data);
    return Image;
}

int xstbi_is_hdr(const char* Path)
{
    size_t Size;
    bytes Data;
    int Width;
    int Height;
    int Channels;
    size_t OutputSize;
    int Result;

    xrtClearError();
    Data = xstbi_read(Path, &Size);
    if (Data == NULL) return -1;
    if (!xstbi_dimensions(Data, Size, 0, 0,
            &Width, &Height, &Channels, &OutputSize)) {
        xrtFree(Data);
        return -1;
    }
    Result = stbi_is_hdr_from_memory(Data, (int)Size);
    xrtFree(Data);
    xrtClearError();
    return Result;
}

void* xstbi_info_json(const char* Path)
{
    size_t Size;
    bytes Data;
    int Width;
    int Height;
    int Channels;
    size_t OutputSize;
    char* Result;

    xrtClearError();
    Data = xstbi_read(Path, &Size);
    if (Data == NULL) return NULL;
    if (!xstbi_dimensions(Data, Size, 0, 0,
            &Width, &Height, &Channels, &OutputSize)) {
        xrtFree(Data);
        return NULL;
    }
    xrtFree(Data);
    Result = (char*)xrtMalloc(128u);
    if (Result == NULL) {
        xstbi_error(XERR_MEMORY, "could not allocate image metadata");
        return NULL;
    }
    (void)snprintf(Result, 128u,
        "{\"width\":%d,\"height\":%d,\"channels\":%d}",
        Width, Height, Channels);
    return Result;
}

void* xstbi_image_adopt(void* Image)
{
    return Image;
}

int xstbi_image_valid(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    return Image != NULL && Image->Data != NULL;
}

int xstbi_image_width(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    return Image != NULL ? Image->Width : 0;
}

int xstbi_image_height(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    return Image != NULL ? Image->Height : 0;
}

int xstbi_image_channels(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    return Image != NULL ? Image->Channels : 0;
}

int xstbi_image_is_float(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    return Image != NULL ? Image->IsFloat : 0;
}

size_t xstbi_image_byte_size(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    return Image != NULL ? Image->Size : 0u;
}

void* xstbi_image_data(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    return Image != NULL ? Image->Data : NULL;
}

void xstbi_image_release(void* Handle)
{
    xstbiimage* Image = (xstbiimage*)Handle;
    if (Image == NULL) return;
    stbi_image_free(Image->Data);
    xrtFree(Image);
}
