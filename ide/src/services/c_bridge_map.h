#ifndef XIDE_C_BRIDGE_MAP_H
#define XIDE_C_BRIDGE_MAP_H
#include "services/semantic_definition.h"
/* Exact same-query sources, validated before entry. Nominal C type tag and
   typedef records retain actual declaration/type identities and must agree.
   Returns compact owned bindings, or NULL for malformed optional metadata.
   No disk or UI access. */
xvalue* xideCBridgeMapBindings(const xvalue* map,const char* declarations,const xide_semantic_source* sources,const xvalue* files,const int* versions,size_t count,xide_model_error* error);
#endif
