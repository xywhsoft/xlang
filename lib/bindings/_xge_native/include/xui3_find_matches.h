#ifndef XL6_XUI3_FIND_MATCHES_H
#define XL6_XUI3_FIND_MATCHES_H
#include <xge3_runtime.h>

static inline bool xl6Xui3FindMatchesDestroy(void* raw)
{
    xuiFindMatchesRelease((xui_find_matches_t*)raw);
    return true;
}

static inline const XL6_ForeignViewDescriptorV1* xl6Xui3FindMatchesPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={
        sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.findMatches",
        0,1,NULL,NULL,xl6Xui3FindMatchesDestroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL
    };
    return &value;
}
#define xl6Xui3FindMatchesPolicy (*xl6Xui3FindMatchesPolicyGet())
#endif
