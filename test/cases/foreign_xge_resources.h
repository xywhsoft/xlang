#ifndef XL6_TEST_FOREIGN_XGE_RESOURCES_H
#define XL6_TEST_FOREIGN_XGE_RESOURCES_H
#include <xge.h>
#include "src/runtime/xl6_foreign_view.h"
extern const XL6_ForeignViewDescriptorV1 declarativeXgeFontPolicy;
extern const XL6_ForeignViewDescriptorV1 declarativeXgeTexturePolicy;
extern const XL6_ForeignViewDescriptorV1 declarativeXgeImagePolicy;
xbytesview declarative_xge_image_bytes(void);
xstrview declarative_xge_font_path(void);
xbytesview declarative_xge_font_bytes(void);
void declarative_xge_heap_begin(void);
void declarative_xge_heap_end(void);
void declarative_xge_fail_after(int);
void declarative_xge_reset_failure(void);
#endif
