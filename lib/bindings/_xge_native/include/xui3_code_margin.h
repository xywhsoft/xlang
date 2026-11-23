#ifndef XL6_XUI3_CODE_MARGIN_H
#define XL6_XUI3_CODE_MARGIN_H
#include <xge3_runtime.h>

/* Numeric storage independent of any editor, model or callbacks. */
static inline bool xl6Xui3CodeMarginLayoutRelease(void* raw)
{
    xuiCodeMarginLayoutRelease((xui_code_margin_layout_t*)raw);
    return true;
}
static inline const XL6_ForeignViewDescriptorV1* xl6Xui3CodeMarginLayoutPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.codeMarginLayout",
        0,1,NULL,NULL,xl6Xui3CodeMarginLayoutRelease,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
    return &value;
}
#define xl6Xui3CodeMarginLayoutPolicy (*xl6Xui3CodeMarginLayoutPolicyGet())
#endif
