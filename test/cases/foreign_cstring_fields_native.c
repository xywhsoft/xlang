#define XRT_MODULE_ALL
#include "lib/xrt/xrt.h"
#include "foreign_cstring_fields.h"
#include <assert.h>
#include <string.h>
static char buffer[64]="original";
static int calls;
static xmemdebugsnapshot before;
static const char* text(int mode)
{
 switch(mode){case 1:return "";case 2:return NULL;case 3:return "a\0b";default:return "borrowed \xe4\xbd\xa0";}
}
CStringFields* cstringFieldsGet(int mode)
{static CStringFields value;++calls;strcpy(buffer,"original");value=(CStringFields){text(mode),buffer};return &value;}
void cstringFieldsChange(void){strcpy(buffer,"changed");}
int cstringFieldsWith(int mode,int (*callback)(const CStringFields*,void*),void* user)
{++calls;CStringFields value={text(mode),buffer};assert(callback);int status=callback(&value,user);value.text=(const char*)(uintptr_t)1;return status;}
void cstringFieldsBudget(int n)
{if(n<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
void cstringFieldsBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);calls=0;}
int cstringFieldsAllocations(void)
{xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);return (int)(now.AllocCount-before.AllocCount);}
int cstringFieldsCalls(void){return calls;}
