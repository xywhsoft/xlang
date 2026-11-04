#ifndef XL6_TEST_FOREIGN_PACK_H
#define XL6_TEST_FOREIGN_PACK_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
typedef struct PackRoot PackRoot;
typedef struct PackDesc {
 uint32_t size;
 int mode,factor;
 PackRoot* font;
 const char* text;
 int textSize;
 const char* context;
 size_t contextSize;
 const char* language;
 const float* dashes;
 int dashCount;
} PackDesc;
typedef struct PackSmall {int value;} PackSmall;
typedef int (*PackCallback)(void*);
struct PackRoot {
 int refs,value;
 unsigned seen;
 int (*evaluate)(PackRoot*,const PackDesc*,PackCallback,void*,int*);
};
extern const XL6_ForeignViewDescriptorV1 packPolicy;
int pack_create(int,PackRoot**);
void pack_drop(PackRoot*);
int pack_value(PackRoot*);
unsigned pack_seen(PackRoot*);
int pack_free(const PackDesc*,int*);
void pack_void(int*,const PackDesc*);
int pack_make(const PackDesc*,PackRoot**);
int pack_two(const PackSmall*,int*,const PackSmall*,int*);
int pack_calls(void);
int pack_live(void);
void pack_fail_after(int);
void pack_reset(void);
#endif
