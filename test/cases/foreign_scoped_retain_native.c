#include "foreign_scoped_retain.h"
#include <assert.h>
static int live,calls,producers;
int scoped_retain_create(int mode,ScopedRetainNode** out)
{*out=xrtCalloc(1,sizeof(**out));if(!*out)return -1;**out=(ScopedRetainNode){1,0,87,mode};++live;return 0;}
int scoped_retain_hold(ScopedRetainNode* node)
{assert(node && node->depth>0);++calls;if(node->mode==1)return -9;++node->refs;return 0;}
int scoped_retain_drop(ScopedRetainNode* node)
{assert(node && node->refs>0);if(!--node->refs){assert(!node->depth);--live;xrtFree(node);}return 0;}
static void produced(ScopedRetainNode* node)
{assert(node && node->depth>0);++producers;if(node->mode==2)assert(xrtMemDebugFailAfter(0));}
ScopedRetainNode* scoped_retain_borrow(ScopedRetainNode* node){produced(node);return node;}
ScopedRetainNode* scoped_retain_maybe(ScopedRetainNode* node,int present){produced(node);return present ? node:NULL;}
int scoped_retain_single(ScopedRetainNode* node,ScopedRetainNode** out){produced(node);*out=node;return 0;}
int scoped_retain_pair(ScopedRetainNode* node,ScopedRetainNode** first,ScopedRetainNode** second)
{produced(node);*first=node;*second=node->mode==3 ? NULL:node;return 0;}
int scoped_retain_value(ScopedRetainNode* node){assert(node && node->depth>0);return node->value;}
int scoped_retain_refs(ScopedRetainNode* node){assert(node && node->depth>0);return node->refs-node->depth;}
int scoped_retain_live(void){return live;}
int scoped_retain_calls(void){return calls;}
int scoped_retain_producers(void){return producers;}
void scoped_retain_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void scoped_retain_reset(void){xrtMemDebugFailClear();}
static bool begin(void* raw,void* storage)
{ScopedRetainNode* node=raw;ScopedRetainScope* scope=storage;assert(!scope->node && !scope->cookie);++node->refs;++node->depth;*scope=(ScopedRetainScope){node,UINT64_C(0x1234)};return true;}
static bool end(void* storage)
{ScopedRetainScope* scope=storage;ScopedRetainNode* node=scope->node;assert(node && scope->cookie==UINT64_C(0x1234) && node->depth>0);--node->depth;*scope=(ScopedRetainScope){0};return scoped_retain_drop(node)==0;}
static bool busy(const void* raw){return ((const ScopedRetainNode*)raw)->depth!=0;}
static bool destroy(void* raw){return scoped_retain_drop(raw)==0;}
const XL6_ForeignViewDescriptorV1 scopedRetainPolicy={sizeof(scopedRetainPolicy),XL6_FOREIGN_VIEW_V1_TAG,"scoped.retain",0,1,NULL,NULL,destroy,sizeof(ScopedRetainScope),_Alignof(ScopedRetainScope),begin,end,NULL,busy,0,NULL,NULL};
