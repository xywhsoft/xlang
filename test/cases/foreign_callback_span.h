#ifndef XL6_TEST_FOREIGN_CALLBACK_SPAN_H
#define XL6_TEST_FOREIGN_CALLBACK_SPAN_H
#include "src/runtime/xl6_foreign_view.h"
#include <xrt_decl.h>
typedef struct SpanRoot SpanRoot;
typedef struct SpanCall {SpanRoot* root;struct SpanCall* self;} SpanCall;
typedef struct SpanOwner {xrtownershipref reference;bool(*Hold)(const void*);bool(*Drop)(const void*);} SpanOwner;
typedef int(*SpanCallback)(const unsigned char*,size_t,int,void*);
typedef int(*SpanPairCallback)(void*,const char*,int,const void*,size_t);
extern const XL6_ForeignViewDescriptorV1 spanRootPolicy;
int span_create(SpanRoot**);
void span_destroy(SpanRoot*);
void span_set(SpanRoot*,SpanCallback,void*,const SpanOwner*);
void span_set_pair(SpanRoot*,SpanPairCallback,void*,const SpanOwner*);
int span_fire(SpanRoot*,int);
int span_fire_pair(SpanRoot*,int);
int span_live(void);
void span_counter_begin(void);
void span_counter_end(void);
#endif
