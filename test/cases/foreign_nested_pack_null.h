#ifndef FOREIGN_NESTED_PACK_NULL_H
#define FOREIGN_NESTED_PACK_NULL_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NestedRoot NestedRoot;
typedef struct NestedFonts {NestedRoot* source;} NestedFonts;
typedef struct NestedRenderer {uint32_t size;NestedFonts fonts;const char* text;size_t length;} NestedRenderer;
typedef struct NestedView {uint32_t size;NestedRenderer renderer;} NestedView;
typedef struct NestedOuter {uint32_t size;int factor;NestedView view;} NestedOuter;
struct NestedRoot {int value;int(*evaluate)(NestedRoot*,const NestedOuter*,const int*,int*);};
extern const XL6_ForeignViewDescriptorV1 nestedRootPolicy;
int nested_create(NestedRoot**,const void*);
void nested_drop(NestedRoot*);
int nested_call(NestedRoot*,const NestedOuter*,const int*,int*);
int nested_free(const NestedOuter*,const int*,int*);
int nested_null_only(const int*,int*);
int nested_trailing(int,const NestedRoot*,const void*);
int nested_multi(const int*,int*,const void*,int*);
int nested_span(const unsigned char*,size_t,const int*,int*);
int nested_calls(void);
void nested_fail_after(int);
void nested_reset(void);
#ifdef __cplusplus
}
#endif
#endif
