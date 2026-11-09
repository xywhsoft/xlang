#ifndef XL6_XGE3_RESOURCE_H
#define XL6_XGE3_RESOURCE_H
#include "xge3_render_resources.h"
/* Loaded resources hold only native bytes/metadata. The Owned provider copies
 * its result and drops its callback snapshot before publishing the resource. */
XL6_XGE_RECORD_POLICY(xl6Xge3ResourcePolicy,"xge.resource",xge_resource_t,xgeResourceFree)
#define xl6Xge3ResourcePolicy (*xl6Xge3ResourcePolicyGet())
#endif
