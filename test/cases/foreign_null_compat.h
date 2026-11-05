#ifndef FOREIGN_NULL_COMPAT_H
#define FOREIGN_NULL_COMPAT_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NullPod {int value;} NullPod;
typedef struct NullRoot NullRoot;
typedef struct NullDesc {uint32_t size;int mode;NullRoot* source;const char* text;} NullDesc;
typedef struct NullInner {uint32_t size;int mode;NullRoot* source;const char* text;} NullInner;
typedef struct NullNested {uint32_t size;NullInner inner;} NullNested;
struct NullRoot {int value;int(*apply)(NullRoot*,const NullDesc*,int,const char*,int*);int(*paired)(NullRoot*,const NullDesc*,const NullNested*,int,const char*,int*);};
extern const XL6_ForeignViewDescriptorV1 nullPolicy;
int null_create(NullRoot**);
int null_create_config(const NullDesc*,NullRoot**);
void null_drop(NullRoot*);
int null_pod(NullRoot*,const NullPod*,int,const char*,int*);
int null_pack(NullRoot*,const NullDesc*,int,const char*,int*);
int null_paired(NullRoot*,const NullDesc*,const NullNested*,int,const char*,int*);
int null_free(const NullPod*,int,const char*,int*);
int null_calls(void);
#ifdef __cplusplus
}
#endif
#endif
