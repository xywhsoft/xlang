#include "test/cases/foreign_native_preparation.h"
#include <__xl_runtime/xl_foreign_error.h>
#include <assert.h>
static int calls,begins,ends,mode,endMode;
int preparation_read(PrepRoot* r){assert(r && r->depth);return ++calls;}
int preparation_pair(PrepRoot* a,PrepRoot* b){assert(a && b && a->depth && b->depth);return ++calls;}
int preparation_create(PrepRoot** out){*out=xrtCalloc(1,sizeof(**out));if(!*out)return -1;(*out)->read=preparation_read;return 0;}
void preparation_destroy(PrepRoot* r){if(!r)return;r->closing=true;if(!r->depth)xrtFree(r);}
void preparation_arm(int m){mode=m;}
int preparation_calls(void){return calls;}
int preparation_begins(void){return begins;}
int preparation_ends(void){return ends;}
static bool begin(void* raw,void* storage){
 PrepRoot* r=raw;PrepScope* s=storage;if(r->closing || s->root)return false;
 s->root=r;++r->depth;++begins;int m=mode;mode=0;
 if(m){
  if(m==2)assert(xrtMemDebugFailAfter(0));
  else xrtSetErrorTake(xrtErrorCreate(XERR_STATE,"preparation-primary",73,"admission callback failed"));
  xl6ForeignCallbackFail();xrtClearError();
  if(m==2)assert(xrtMemDebugFailAfter(-1));
  if(m==3)xrtSetErrorTake(xrtErrorCreate(XERR_RANGE,"preparation-secondary",87,"diagnostic after escaped callback"));
  if(m==4)endMode=1;
 }
 return true;
}
static bool end(void* storage){
 PrepScope* s=storage;PrepRoot* r=s->root;assert(r && r->depth);s->root=NULL;--r->depth;++ends;
 if(r->closing)preparation_destroy(r);
 if(endMode){endMode=0;xrtSetErrorTake(xrtErrorCreate(XERR_RANGE,"preparation-cleanup",89,"secondary cleanup"));return false;}
 return true;
}
static bool destroy(void* raw){preparation_destroy(raw);return true;}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user){(void)raw;(void)visit;(void)user;return true;}
static bool busy(const void* raw){return ((const PrepRoot*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 preparationPolicy={sizeof(preparationPolicy),XL6_FOREIGN_VIEW_V1_TAG,
 "test.preparation",0,1,NULL,NULL,destroy,sizeof(PrepScope),_Alignof(PrepScope),begin,end,trace,busy};
