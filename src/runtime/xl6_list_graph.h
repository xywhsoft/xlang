#ifndef XL6_LIST_GRAPH_H
#define XL6_LIST_GRAPH_H
#include "xl6_value_graph_plan.h"
/* Optional real List adapter in the shared XRT memo. Deep copy preserves
 * native policy, codec and all int64 keys. Data without a native sequence
 * rule preserves keys in IntMap; an exact closed rule emits the approved
 * Array format in sorted native iteration order. Never a second graph pass. */
XL6_TEXT_API const XL6_ValueGraphProviderV1* xl6ListGraphProviderV1(void);
#endif
