#ifndef XL6_SET_GRAPH_H
#define XL6_SET_GRAPH_H
#include "xl6_value_graph_plan.h"
/* Genuine Set provider in XRT's shared identity memo. Deep clone preserves
 * complete typed admission, projection and identity-key policy. Data output
 * is a compact Array in stable insertion order, not a persisted hash table. */
XL6_TEXT_API const XL6_ValueGraphProviderV1* xl6SetGraphProviderV1(void);
#endif
