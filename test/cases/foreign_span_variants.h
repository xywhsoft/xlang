#ifndef XL6_FOREIGN_SPAN_VARIANTS_H
#define XL6_FOREIGN_SPAN_VARIANTS_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct SpanVariantsDesc {const float* values;int count;} SpanVariantsDesc;
typedef struct SpanVariantsRoot SpanVariantsRoot;
struct SpanVariantsRoot {
 int (*sum)(SpanVariantsRoot*,const float*,int);
 int (*packed)(SpanVariantsRoot*,const SpanVariantsDesc*,int*);
};
extern const XL6_ForeignViewDescriptorV1 spanVariantsPolicy;
int span_variants_create(SpanVariantsRoot**);
void span_variants_drop(SpanVariantsRoot*);
int span_variants_sum(const float*,int);
int span_variants_live(void);
int span_variants_calls(void);
void span_variants_fail_after(int);
void span_variants_reset(void);
#endif
