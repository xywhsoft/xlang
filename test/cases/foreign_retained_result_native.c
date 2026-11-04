/* Native borrowed-result producer, without language forwarding wrappers. */
#include <xrt_decl.h>
#include "foreign_retained_result.h"
#include <assert.h>
#include <limits.h>
static int live,calls;
int retained_create(int mode,RetainedNode** output)
{assert(output);*output=NULL;RetainedNode* node=xrtCalloc(1,sizeof(*node));if(!node)return -1;node->refs=1;node->mode=mode;node->value=17;++live;*output=node;return 0;}
int retained_addref(RetainedNode* node)
{
    assert(node && node->refs>0);++calls;
    if(node->mode==1)return 0;
    if(node->mode==2){assert(xrtMemDebugFailAfter(0));xrtSetErrorKind(XERR_STATE);return -2;}
    if(node->refs==INT_MAX)return 0;
    ++node->refs;
    if(node->mode==3)xrtSetErrorKind(XERR_STATE);
    if(node->mode==4)assert(xrtMemDebugFailAfter(0));
    return node->refs;
}
int retained_addref_zero(RetainedNode* node)
{int refs=retained_addref(node);return refs>0 ? 0:-1;}
void retained_release(RetainedNode* node)
{if(!node)return;assert(node->refs>0);if(--node->refs)return;retained_release(node->child);--live;xrtFree(node);}
RetainedNode* retained_borrow(RetainedNode* node){assert(node && node->refs>0);return node;}
RetainedNode* retained_borrow_zero(RetainedNode* node){assert(node && node->refs>0);return node;}
RetainedNode* retained_maybe(RetainedNode* node){assert(node && node->refs>0);return node->child;}
int retained_set_child(RetainedNode* node,RetainedNode* child)
{assert(node && node->refs>0);if(child && retained_addref(child)<=0)return -1;RetainedNode* old=node->child;node->child=child;retained_release(old);return 0;}
int retained_value(RetainedNode* node){assert(node && node->refs>0);return node->value;}
int retained_refs(RetainedNode* node){assert(node && node->refs>0);return node->refs;}
int retained_live(void){return live;}
int retained_calls(void){return calls;}
void retained_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void retained_reset(void){assert(xrtMemDebugFailAfter(-1));}
static bool destroy(void* raw){retained_release(raw);return true;}
const XL6_ForeignViewDescriptorV1 retainedPolicy={sizeof(retainedPolicy),XL6_FOREIGN_VIEW_V1_TAG,"retained.node",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
