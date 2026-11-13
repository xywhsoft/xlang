#include "foreign_null_cstr.h"
#include <xrt_decl.h>
#include <string.h>
static int calls;
static bool destroy(void* raw){xrtFree(raw);return true;}
const XL6_ForeignViewDescriptorV1* cstrPolicyGet(void)
{static const XL6_ForeignViewDescriptorV1 d={sizeof(d),XL6_FOREIGN_VIEW_V1_TAG,"fixture.cstr",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};return &d;}
int cstrCreate(CstrRoot** output)
{*output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;(*output)->base=7;(*output)->apply=cstrMethod;return 0;}
void cstrDrop(CstrRoot* value){xrtFree(value);}
int cstrFree(int prefix,const char* text,int suffix,int* output)
{++calls;*output=prefix+suffix+(text?(int)strlen(text):100);return 0;}
int cstrMethod(CstrRoot* self,int prefix,const char* text,int suffix,int* output)
{int status=cstrFree(prefix,text,suffix,output);*output+=self->base;return status;}
int cstrCalls(void){return calls;}
