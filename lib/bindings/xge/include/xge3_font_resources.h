#ifndef XL6_XGE3_FONT_RESOURCES_H
#define XL6_XGE3_FONT_RESOURCES_H
#include "xge3_render_resources.h"
/* Native RC owns these opaque CPU objects. Family retains each face and
 * Resolve returns a new native reference; neither owns a language closure. */
XL6_XGE_NATIVE_POLICY(xl6Xge3FontFacePolicy,"xge.fontFace",struct xge_font_face_t,xgeFontFaceFree)
XL6_XGE_NATIVE_POLICY(xl6Xge3FontFamilyPolicy,"xge.fontFamily",struct xge_font_family_t,xgeFontFamilyFree)
#define xl6Xge3FontFacePolicy (*xl6Xge3FontFacePolicyGet())
#define xl6Xge3FontFamilyPolicy (*xl6Xge3FontFamilyPolicyGet())
#endif
