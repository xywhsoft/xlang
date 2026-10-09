#ifndef XL6_VALUE_GRAPH_PLAN_H
#define XL6_VALUE_GRAPH_PLAN_H
#include "xl6_text_value.h"

/* Compiler-private resident providers, borrowed for one synchronous graph
 * traversal. No registration, stored context, source function or new memo.
 * Return 0/1/-1 with XRT's output-ownership convention. sequence requests an
 * Array data result for this exact provider/identity, never inferred by tag.
 * Providers inspect captured payloads only after their genuine ops match and
 * recurse only through the supplied Next capability. */
typedef struct XL6_ValueGraphProviderV1 {
    int (*Copy)(const xvaluehandleops*,ptr,ptr,uint64_t,xvaluegraphnextv1*,xvalue**,bool,bool);
} XL6_ValueGraphProviderV1;
typedef struct XL6_ValueGraphSequenceV1 {
    uint64_t identity;
    const XL6_ValueGraphProviderV1* provider;
} XL6_ValueGraphSequenceV1;
typedef struct XL6_ValueGraphPlanV1 {
    size_t size;
    const XL6_ValueGraphProviderV1* const* providers;
    size_t providerCount;
    const XL6_ValueGraphSequenceV1* sequences;
    size_t sequenceCount;
} XL6_ValueGraphPlanV1;

/* An exact native sequence rule selects only its named resident provider.
 * An unrelated handle or non-Array result fails closed. Native sequence IDs
 * cannot also appear in the ordinary wire IntMap/Set sequence table. */
XL6_TEXT_API xvalue* xl6ValueDeepCloneNativeV1(const xvalue*,const XL6_ValueGraphPlanV1*);
XL6_TEXT_API xvalue* xl6ValueDataSnapshotNativeV1(const xvalue*,const XL6_ValueGraphPlanV1*);
XL6_TEXT_API xvalue* xl6ValueDataProjectNativeV1(const xvalue*,xvaluegraphobjectcopyv1,ptr,
    const xvaluegraphsequencev1*,size_t,const XL6_ValueGraphPlanV1*);
#endif
