#ifndef XL6_TEST_NINE_PATCH_H
#define XL6_TEST_NINE_PATCH_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixtureNinePatchLegacy(xge_texture texture, const xge_nine_patch_style_t* style, xge_rect_t dst, bool simple);
void fixtureNinePatchHeapBegin(void);
void fixtureNinePatchHeapEnd(int mode);
void fixtureNinePatchNativeHot(xge_texture texture, const xge_nine_patch_style_t* style, xge_rect_t dst);
#ifdef __cplusplus
}
#endif
#endif
