#ifndef XL6_XGE3_TEXT_RESOURCES_H
#define XL6_XGE3_TEXT_RESOURCES_H
#include "xge3_render_resources.h"
/* Immutable native outputs own their native Font/pixel references. They do
 * not contain language values or closures. Common views retain creator checks. */
XL6_XGE_NATIVE_POLICY(xl6Xge3GlyphRunPolicy,"xge.glyphRun",xge_glyph_run_t,xgeGlyphRunFree)
XL6_XGE_NATIVE_POLICY(xl6Xge3GlyphBitmapPolicy,"xge.glyphBitmap",struct xge_glyph_bitmap_resource_t,xgeGlyphBitmapResourceFree)
#define xl6Xge3GlyphRunPolicy (*xl6Xge3GlyphRunPolicyGet())
#define xl6Xge3GlyphBitmapPolicy (*xl6Xge3GlyphBitmapPolicyGet())
#endif
