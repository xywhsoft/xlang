#ifndef FOREIGN_DESCRIPTOR_ARRAYS_H
#define FOREIGN_DESCRIPTOR_ARRAYS_H
#include <stdint.h>
#include <stddef.h>
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct DescriptorPort { uint32_t iSize; const char* label; int number; } DescriptorPort;
typedef struct DescriptorPorts { const DescriptorPort* items; int count; } DescriptorPorts;
typedef struct DescriptorArrays { int tag; const DescriptorPort* inputs; int inputCount; const DescriptorPort* outputs; int outputCount; } DescriptorArrays;
typedef struct DescriptorNested { int tag; DescriptorPorts ports; } DescriptorNested;
typedef struct DescriptorOnly { const DescriptorPort* items; int count; } DescriptorOnly;
typedef struct DescriptorReceiver DescriptorReceiver;
struct DescriptorReceiver { int base; int (*inspect)(DescriptorReceiver*,const DescriptorOnly*); };
int descriptorArraysConsume(const DescriptorArrays*);
int descriptorArraysChecked(const DescriptorArrays*);
int descriptorArraysNested(const DescriptorNested*);
int descriptorArraysMixed(int,const DescriptorOnly*,const int*,int);
DescriptorReceiver* descriptorArraysReceiver(void);
int descriptorArraysInspect(DescriptorReceiver*,const DescriptorOnly*);
int descriptorArraysReenter(const DescriptorOnly*,int(*)(void*),void*);
typedef struct DescriptorOwned { int value,refs,active; } DescriptorOwned;
typedef struct DescriptorOwnedScope { DescriptorOwned* object; } DescriptorOwnedScope;
extern const XL6_ForeignViewDescriptorV1 descriptorOwnedPolicy;
int descriptorOwnedCreate(int,DescriptorOwned**);
void descriptorOwnedDestroy(DescriptorOwned*);
int descriptorOwnedLive(void);
typedef struct DescriptorViewItem { DescriptorOwned* object; const uint8_t* data; uint64_t length; } DescriptorViewItem;
typedef struct DescriptorViews { const DescriptorViewItem* items; int count; } DescriptorViews;
void descriptorArraysRemember(int,const uint8_t*,size_t);
int descriptorArraysViews(const DescriptorViews*,int(*)(void*),void*);
void descriptorArraysReport(int);
void descriptorArraysNativeHot(void);
int descriptorArraysCalls(void);
void descriptorArraysBudget(int);
int descriptorArraysTriggered(void);
void descriptorArraysMark(void);
int descriptorArraysBalanced(void);
#ifdef __cplusplus
}
#endif
#endif
