#ifndef FOREIGN_OUTPUT_SNAPSHOTS_H
#define FOREIGN_OUTPUT_SNAPSHOTS_H
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct OutputValueRoot { int refs,active,poison; char* text; } OutputValueRoot;
typedef struct OutputValueScope { OutputValueRoot* root; } OutputValueScope;
typedef struct OutputValueRecord { unsigned iSize; int number; const char* first; const char* optional; OutputValueRoot* same; } OutputValueRecord;
extern const XL6_ForeignViewDescriptorV1 outputValuePolicy;
int outputValueCreate(int,OutputValueRoot**);
int outputValueHold(OutputValueRoot*);
int outputValueDrop(OutputValueRoot*);
int outputValueRead(OutputValueRoot*,int,OutputValueRecord*);
int outputOwnedValue(int,xvalue**);
int outputMixedValue(xvalue**,const char**,const char**,int);
int outputValueIdentity(const xvalue*);
void outputValueHeapBegin(void);
void outputValueHeapEqual(void);
void outputValueBudget(int);
void outputValueReset(void);
int outputValueLive(void);
int outputValueReleaseCalls(void);
void outputValueHotReport(int);
void outputValueNativeHot(void);
#endif
