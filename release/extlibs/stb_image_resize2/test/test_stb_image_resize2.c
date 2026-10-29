#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/stb_image_resize2_native.h"
#include "../third_party/stb_image_resize2.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "stb_image_resize2 C test failed at line %d: %s\n", \
        __LINE__, #expr); return 2; \
} } while (0)

int main(void)
{
    unsigned char Pixels[16] = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255
    };
    unsigned char Padded[24] = {0};
    xbuffer* Identity;
    xbuffer* Srgb;
    xbuffer* Linear;
    xbuffer* FromStride;

    CHECK(xstbir_available() == 1 && xstbir_version()[0] != '\0');
    Identity = (xbuffer*)xstbir_resize(Pixels, sizeof(Pixels),
        2, 2, 4, 2, 2, 0, 0);
    CHECK(Identity != NULL && Identity->Size == sizeof(Pixels));
    CHECK(memcmp(Identity->Data, Pixels, sizeof(Pixels)) == 0);

    Srgb = (xbuffer*)xstbir_resize(Pixels, sizeof(Pixels),
        2, 2, 4, 4, 4, 0, 0);
    Linear = (xbuffer*)xstbir_resize(Pixels, sizeof(Pixels),
        2, 2, 4, 4, 4, 0, 1);
    CHECK(Srgb != NULL && Srgb->Size == 64u);
    CHECK(Linear != NULL && Linear->Size == 64u);

    memcpy(Padded, Pixels, 8u);
    memcpy(Padded + 12u, Pixels + 8u, 8u);
    FromStride = (xbuffer*)xstbir_resize(Padded, sizeof(Padded),
        2, 2, 4, 4, 4, 12, 0);
    CHECK(FromStride != NULL && FromStride->Size == Srgb->Size);
    CHECK(memcmp(FromStride->Data, Srgb->Data, Srgb->Size) == 0);

    {
        unsigned char BlackWhite[2] = {0, 255};
        xbuffer* SrgbMid = (xbuffer*)xstbir_resize(BlackWhite,
            sizeof(BlackWhite), 2, 1, 1, 1, 1, 0, 0);
        xbuffer* LinearMid = (xbuffer*)xstbir_resize(BlackWhite,
            sizeof(BlackWhite), 2, 1, 1, 1, 1, 0, 1);
        CHECK(SrgbMid != NULL && LinearMid != NULL);
        CHECK(SrgbMid->Size == 1u && LinearMid->Size == 1u);
        CHECK(SrgbMid->Data[0] == 188u && LinearMid->Data[0] == 128u);
        xrtBufferDestroy(LinearMid);
        xrtBufferDestroy(SrgbMid);
    }

    {
        unsigned char Gray[96u * 56u];
        xbuffer* Downscaled;
        int x, y;
        for (y = 0; y < 56; ++y) {
            for (x = 0; x < 96; ++x)
                Gray[y * 96 + x] = (unsigned char)((x * 7 + y * 11) & 255);
        }
        Downscaled = (xbuffer*)xstbir_resize(Gray, sizeof(Gray),
            96, 56, 1, 32, 32, 0, 1);
        CHECK(Downscaled != NULL && Downscaled->Size == 32u * 32u);
        xrtBufferDestroy(Downscaled);
    }

    {
        unsigned char GrayAlpha[4] = {255, 0, 0, 255};
        unsigned char Expected[2] = {0, 128};
        xbuffer* Actual;
        Actual = (xbuffer*)xstbir_resize(GrayAlpha, sizeof(GrayAlpha),
            2, 1, 2, 1, 1, 0, 0);
        CHECK(Actual != NULL && Actual->Size == sizeof(Expected));
        CHECK(memcmp(Actual->Data, Expected, sizeof(Expected)) == 0);
        xrtBufferDestroy(Actual);
        Actual = (xbuffer*)xstbir_resize(GrayAlpha, sizeof(GrayAlpha),
            2, 1, 2, 1, 1, 0, 1);
        CHECK(Actual != NULL && Actual->Size == sizeof(Expected));
        CHECK(memcmp(Actual->Data, Expected, sizeof(Expected)) == 0);
        xrtBufferDestroy(Actual);
    }

    CHECK(xstbir_resize(Pixels, 15u, 2, 2, 4, 4, 4, 0, 0) == NULL);
    CHECK(xrtGetError() != NULL);
    {
        xbuffer* Recovered = (xbuffer*)xstbir_resize(Pixels,
            sizeof(Pixels), 2, 2, 4, 2, 2, 0, 0);
        CHECK(Recovered != NULL && xrtGetError() == NULL);
        xrtBufferDestroy(Recovered);
    }
    CHECK(xstbir_resize(Pixels, 16u, 2, 2, 4, 4, 4, 7, 0) == NULL);
    CHECK(xstbir_resize(Pixels, 16u, 2, 2, 4, 4, 4, 0, 2) == NULL);
    CHECK(xstbir_resize(Pixels, 16u, 0, 2, 4, 4, 4, 0, 0) == NULL);
    CHECK(xstbir_resize(Pixels, 16u, 2, 2, 4, 16384, 16384, 0, 0) == NULL);
    CHECK(xstbir_resize(NULL, 16u, 2, 2, 4, 4, 4, 0, 0) == NULL);
    {
        int linear;
        unsigned point;
        for (linear = 0; linear <= 1; ++linear) {
            unsigned failures = 0u;
            unsigned successes = 0u;
            for (point = 0u; point < 64u; ++point) {
                xbuffer* Resized;
                int triggered;
                CHECK(xrtMemDebugFailAfter(point));
                Resized = (xbuffer*)xstbir_resize(Pixels, sizeof(Pixels),
                    2, 2, 4, 4, 4, 0, linear);
                triggered = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                if (triggered) {
                    ++failures;
                    CHECK(Resized == NULL && xrtGetError() != NULL &&
                        xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                } else {
                    ++successes;
                    CHECK(Resized != NULL && xrtGetError() == NULL);
                    xrtBufferDestroy(Resized);
                }
            }
            CHECK(failures >= 3u && successes != 0u);
        }
    }
    xrtBufferDestroy(FromStride);
    xrtBufferDestroy(Linear);
    xrtBufferDestroy(Srgb);
    xrtBufferDestroy(Identity);
    puts("stb_image_resize2 C boundaries: PASS");
    return 0;
}
