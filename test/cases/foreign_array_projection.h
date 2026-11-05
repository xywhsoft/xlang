#ifndef FOREIGN_ARRAY_PROJECTION_H
#define FOREIGN_ARRAY_PROJECTION_H
#include <stddef.h>
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct ArrayPod { int x,y; } ArrayPod;
typedef struct ArrayItem { const char* title; int result,semantic; } ArrayItem;
int array_projection_sum(const int*,int);
int array_projection_pair(const int*,const int*,int);
int array_projection_pods(const ArrayPod*,size_t);
int array_projection_items(const ArrayItem*,int);
int array_projection_calls(void);
typedef int (*ArrayCallback)(void*);
typedef struct ArrayRoot { int value,active,refs; } ArrayRoot;
typedef struct ArrayScope { ArrayRoot* root; } ArrayScope;
extern const XL6_ForeignViewDescriptorV1 arrayRootPolicy;
int array_projection_create(int,ArrayRoot**);
void array_projection_destroy(ArrayRoot*);
int array_projection_roots(ArrayRoot*,ArrayRoot* const*,const char* const*,int,ArrayCallback,void*,int*);
int array_projection_live(void);
void array_projection_fail_after(int);
void array_projection_reset_failure(void);
#endif
