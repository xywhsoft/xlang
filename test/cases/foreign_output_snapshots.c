#include "foreign_output_snapshots.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int roots;
static int releaseCalls;
static const xvalue* expected;
static xmemdebugsnapshot before;
int outputValueCreate(int mode,OutputValueRoot** out)
{(void)mode;*out=xrtCalloc(1,sizeof(**out));if(!*out)return -8;(*out)->refs=1;++roots;return 0;}
int outputValueHold(OutputValueRoot* root)
{assert(root&&root->refs>0);++root->refs;return 0;}
int outputValueDrop(OutputValueRoot* root)
{if(root){assert(root->refs>0);if(!--root->refs){assert(!root->active);xrtFree(root->text);xrtFree(root);--roots;}}return 0;}
static bool destroy(void* p){return outputValueDrop(p)==0;}
static bool begin(void* p,void* storage)
{OutputValueScope* s=storage;s->root=p;outputValueHold(p);++s->root->active;return true;}
static bool end(void* storage)
{OutputValueScope* s=storage;OutputValueRoot* p=s->root;assert(p&&p->active);int fail=p->poison==99;if(p->poison){xrtFree(p->text);p->text=NULL;p->poison=0;}--p->active;s->root=NULL;outputValueDrop(p);if(fail)xrtSetErrorKind(XERR_STATE);return !fail;}
const XL6_ForeignViewDescriptorV1 outputValuePolicy={sizeof(outputValuePolicy),XL6_FOREIGN_VIEW_V1_TAG,"output.values.root",0,1,NULL,NULL,destroy,sizeof(OutputValueScope),_Alignof(OutputValueScope),begin,end,NULL,NULL,XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY,NULL,NULL};
int outputValueRead(OutputValueRoot* root,int mode,OutputValueRecord* out)
{
 assert(root&&root->active&&out&&out->iSize==sizeof(*out));
 xrtFree(root->text);root->text=xrtStrDup("A\xe4\xbd\xa0\xe5\xa5\xbd");if(!root->text)return -8;
 root->poison=mode==99?99:1;out->number=73;out->first=mode==1?NULL:root->text;out->optional=mode==2?NULL:"";out->same=root;
 if(mode==3){out->first=(const char*)1;return -9;}
 return 0;
}
static void releaseError(void* user){(void)user;++releaseCalls;xrtSetErrorKind(XERR_STATE);}
int outputOwnedValue(int mode,xvalue** out)
{
 *out=NULL;if(mode==5)return 0;
 if(mode>=6){*out=xrtValueObject();if(!*out||!xrtValueObjectLifetimeBindOwned(*out,NULL,NULL,releaseError))return -8;if(mode==6)return -9;if(mode==7)xrtSetErrorKind(XERR_STATE);return 0;}
 *out=xrtValueInt(INT64_C(9876543210));if(!*out)return -8;expected=*out;
 if(mode==1)return -9;
 if(mode==2)xrtSetErrorKind(XERR_STATE);
 return 0;
}
int outputMixedValue(xvalue** out,const char** text,const char** optional,int mode)
{
 int status=outputOwnedValue(mode==1?1:mode==4?8:0,out);*text=mode==1?(const char*)1:mode==2||mode==4?NULL:"first";*optional=mode==3?NULL:"";return status;
}
int outputValueIdentity(const xvalue* value){return value==expected;}
void outputValueHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void outputValueHeapEqual(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.LiveCount==before.LiveCount&&after.LiveBytes==before.LiveBytes&&after.InvalidFreeCount==before.InvalidFreeCount&&after.DoubleFreeCount==before.DoubleFreeCount);}
void outputValueBudget(int n){assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}
void outputValueReset(void){xrtMemDebugFailClear();xrtClearError();}
int outputValueLive(void){return roots;}
int outputValueReleaseCalls(void){return releaseCalls;}
void outputValueHotReport(int native)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);printf("Owned value heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",(unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),(unsigned long long)(after.ReallocCount-before.ReallocCount));}
void outputValueNativeHot(void)
{for(int i=0;i<1000;++i){xvalue* value=NULL;assert(outputOwnedValue(0,&value)==0&&outputValueIdentity(value));xrtValueRelease(value);}}
