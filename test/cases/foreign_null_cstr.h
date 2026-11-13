#ifndef FOREIGN_NULL_CSTR_H
#define FOREIGN_NULL_CSTR_H
#include <__xl_runtime/xl6_foreign_view.h>
typedef struct CstrRoot CstrRoot;
struct CstrRoot {int base;int (*apply)(CstrRoot*,int,const char*,int,int*);};
const XL6_ForeignViewDescriptorV1* cstrPolicyGet(void);
#define cstrPolicy (*cstrPolicyGet())
int cstrCreate(CstrRoot** output);
void cstrDrop(CstrRoot* value);
int cstrFree(int prefix,const char* text,int suffix,int* output);
int cstrMethod(CstrRoot* self,int prefix,const char* text,int suffix,int* output);
int cstrCalls(void);
#endif
