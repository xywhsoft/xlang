#ifndef XL6_TEST_FOREIGN_CALLBACK_SPAN_H
#define XL6_TEST_FOREIGN_CALLBACK_SPAN_H
#include "src/runtime/xl6_foreign_view.h"
#include <xrt_decl.h>
typedef struct SpanRoot SpanRoot;
typedef struct SpanCall {SpanRoot* root;struct SpanCall* self;} SpanCall;
typedef struct SpanOwner {xrtownershipref reference;bool(*Hold)(const void*);bool(*Drop)(const void*);} SpanOwner;
typedef int(*SpanCallback)(const unsigned char*,size_t,int,void*);
typedef int(*SpanPairCallback)(void*,const char*,int,const void*,size_t);
/* Unlike a borrowed xstrview producer, this callback transfers its XRT-owned
 * string allocation to the native caller, which frees it after consumption. */
typedef xstrview(*SpanTextCallback)(const unsigned char*,size_t,int,void*);
const XL6_ForeignViewDescriptorV1* span_policy_get(void);
/* A policy expression avoids PE data-import decoration in the GCC/TCC host
 * fixture. This is type metadata lookup, not a forwarded native operation. */
#define spanRootPolicy (*span_policy_get())
int span_create(SpanRoot**);
void span_destroy(SpanRoot*);
void span_set(SpanRoot*,SpanCallback,void*,const SpanOwner*);
void span_set_pair(SpanRoot*,SpanPairCallback,void*,const SpanOwner*);
int span_fire(SpanRoot*,int);
int span_fire_pair(SpanRoot*,int);
void span_set_text(SpanRoot*,SpanTextCallback,void*,const SpanOwner*);
void span_set_optional_text(SpanRoot*,SpanTextCallback,void*,const SpanOwner*);
int span_fire_text(SpanRoot*,int);
void span_fail_end_once(void);
void span_text_fail_after(int);
void span_text_reset(void);
void span_text_heap_begin(void);
void span_text_heap_end(void);
void span_text_counts_begin(void);
void span_text_counts_end(int);
int span_live(void);
void span_counter_begin(void);
void span_counter_end(void);
#endif
