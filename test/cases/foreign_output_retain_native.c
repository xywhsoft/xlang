/* A real borrowed-output producer: its outputs own no references. */
#include "foreign_output_retain.h"
#include <__xl_runtime/xl6_foreign_view.h>
#include <assert.h>
static int live,calls;
int output_retain_create(int mode,OutputRetainNode** result)
{*result=xrtCalloc(1,sizeof(**result));if(!*result)return -1;(*result)->refs=1;(*result)->mode=mode;(*result)->value=83;++live;return 0;}
int output_retain_hold(OutputRetainNode* node)
{
 assert(node && node->refs>0);++calls;
 if(node->mode==1)return -7;
 if(node->mode==2){assert(xrtMemDebugFailAfter(0));xrtSetErrorKind(XERR_STATE);return -8;}
 ++node->refs;
 if(node->mode==3)xrtSetErrorKind(XERR_STATE);
 if(node->mode==4)assert(xrtMemDebugFailAfter(0));
 return 0;
}
int output_retain_hold_positive(OutputRetainNode* node)
{int status=output_retain_hold(node);return status==0 ? node->refs:status;}
int output_retain_drop(OutputRetainNode* node)
{if(!node)return 0;assert(node->refs>0);if(--node->refs==0){--live;xrtFree(node);}return 0;}
int output_retain_values(OutputRetainNode* node,OutputRetainNode** first,int* number,OutputRetainNode** second,xstrview* text)
{
 assert(node && node->refs>0);*first=node->mode==6 ? NULL:node;*second=node->mode==7 ? NULL:node;*number=91;
 *text=(xstrview){"a\0\xe4\xbd\xa0",5};return node->mode==5 ? -9:0;
}
int output_retain_value(OutputRetainNode* node){assert(node && node->refs>0);return node->value;}
int output_retain_refs(OutputRetainNode* node){assert(node && node->refs>0);return node->refs;}
int output_retain_live(void){return live;}
int output_retain_calls(void){return calls;}
void output_retain_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void output_retain_reset(void){assert(xrtMemDebugFailAfter(-1));}
static bool destroy(void* raw){return output_retain_drop(raw)==0;}
const XL6_ForeignViewDescriptorV1 outputRetainPolicy={sizeof(outputRetainPolicy),XL6_FOREIGN_VIEW_V1_TAG,"output.retain",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL};
