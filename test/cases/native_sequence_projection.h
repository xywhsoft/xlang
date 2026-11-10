#ifndef TEST_NATIVE_SEQUENCE_PROJECTION_H
#define TEST_NATIVE_SEQUENCE_PROJECTION_H
#include <stddef.h>
#include <stdbool.h>
#include "src/runtime/xl6_foreign_view_abi.h"
typedef struct NativeProjectionInner { const void* secret; int count; double weight; } NativeProjectionInner;
typedef struct NativeProjectionRow { void* secret; int actualValue; NativeProjectionInner inner; unsigned long long ordinal; void(*callback)(void); } NativeProjectionRow;
#pragma pack(push,1)
typedef struct NativeProjectionPacked { int value; const void* secret; double weight; } NativeProjectionPacked;
#pragma pack(pop)
typedef struct NativeProjectionOwner NativeProjectionOwner;
int nativeProjectionCreate(NativeProjectionOwner**);
int nativeProjectionRetain(const NativeProjectionOwner*);
void nativeProjectionRelease(NativeProjectionOwner*);
int nativeProjectionRows(const NativeProjectionOwner*,const NativeProjectionRow**,size_t*);
int nativeProjectionPacked(const NativeProjectionOwner*,const NativeProjectionPacked**,size_t*);
int nativeProjectionLive(void);
int nativeProjectionFailAfter(int);
int nativeProjectionFailClear(void);
bool nativeProjectionDestroy(void*);
static inline const XL6_ForeignViewDescriptorV1* nativeProjectionPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,
  "fixture.nativeProjection",0,1,NULL,NULL,nativeProjectionDestroy,0,1,NULL,NULL,NULL,NULL,0,nativeProjectionDestroy,NULL};
 return &policy;
}
#define nativeProjectionPolicy (*nativeProjectionPolicyGet())
#endif
