#ifndef FOREIGN_NULLABLE_SPAN_H
#define FOREIGN_NULLABLE_SPAN_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NullableSpanRoot {int marker;} NullableSpanRoot;
typedef struct NullableSpanPod {unsigned int value;} NullableSpanPod;
typedef struct NullableSpanDesc {const void* data;size_t count;} NullableSpanDesc;
extern const XL6_ForeignViewDescriptorV1 nullableSpanPolicy;
int nullable_span_create(NullableSpanRoot**);
void nullable_span_drop(NullableSpanRoot*);
int nullable_span_read(NullableSpanRoot*,const void*,size_t,int*);
int nullable_span_probe(NullableSpanRoot*,const void*,size_t,int*);
int nullable_span_words(NullableSpanRoot*,const uint32_t*,size_t,int*);
int nullable_span_write(NullableSpanRoot*,void*,int,int,int*);
int nullable_span_pack(NullableSpanRoot*,const NullableSpanDesc*,int*);
int nullable_span_pack_optional(NullableSpanRoot*,const NullableSpanDesc*,int*);
int nullable_span_callback(NullableSpanRoot*,const void*,size_t,int(*)(void*),void*,int*);
int nullable_span_text_callback(NullableSpanRoot*,const void*,size_t,int(*)(void*),void*,int*);
int nullable_span_calls(void);
void nullable_span_fail_after(int);
void nullable_span_reset(void);
#ifdef __cplusplus
}
#endif
#endif
