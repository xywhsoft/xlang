#include "foreign_callback_span.h"
#include <assert.h>
struct SpanRoot {unsigned depth;bool closing;SpanCallback callback;SpanPairCallback pair;void* data;void* pairData;SpanOwner owner,pairOwner;};
static int live;
static void flush(SpanRoot* root){if(root->closing && !root->depth){--live;xrtFree(root);}}
static bool begin(void* raw,void* storage)
{SpanRoot* root=raw;SpanCall* call=storage;if(root->closing || call->self)return false;call->self=call;call->root=root;++root->depth;return true;}
static bool end(void* storage)
{SpanCall* call=storage;assert(call->self==call && call->root->depth);SpanRoot* root=call->root;*call=(SpanCall){0};--root->depth;flush(root);return true;}
int span_create(SpanRoot** output){assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));*output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;++live;return 0;}
void span_set(SpanRoot* root,SpanCallback callback,void* data,const SpanOwner* owner)
{
    if(owner && (!callback || !data || !owner->Hold(data))){xrtSetErrorKind(XERR_STATE);return;}
    if(root->closing){if(owner)(void)owner->Drop(data);xrtSetErrorKind(XERR_STATE);return;}
    SpanOwner old=root->owner;void* previous=root->data;
    root->callback=callback;root->data=data;root->owner=owner ? *owner : (SpanOwner){0};
    if(old.Drop)(void)old.Drop(previous);
}
void span_set_pair(SpanRoot* root,SpanPairCallback callback,void* data,const SpanOwner* owner)
{
    if(owner && (!callback || !data || !owner->Hold(data))){xrtSetErrorKind(XERR_STATE);return;}
    if(root->closing){if(owner)(void)owner->Drop(data);xrtSetErrorKind(XERR_STATE);return;}
    SpanOwner old=root->pairOwner;void* previous=root->pairData;
    root->pair=callback;root->pairData=data;root->pairOwner=owner ? *owner : (SpanOwner){0};
    if(old.Drop)(void)old.Drop(previous);
}
void span_destroy(SpanRoot* root)
{
    if(!root || root->closing)return;
    SpanOwner a=root->owner,b=root->pairOwner;void* da=root->data;void* db=root->pairData;
    root->closing=true;root->callback=NULL;root->pair=NULL;root->owner=(SpanOwner){0};root->pairOwner=(SpanOwner){0};
    if(a.Drop)(void)a.Drop(da);
    if(b.Drop)(void)b.Drop(db);
    flush(root);
}
static bool destroy(void* raw){span_destroy(raw);return true;}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{const SpanRoot* root=raw;return (!root->owner.reference.Ops || visit(root->owner.reference,user)) && (!root->pairOwner.reference.Ops || visit(root->pairOwner.reference,user));}
static bool busy(const void* raw){return ((const SpanRoot*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 spanRootPolicy={sizeof(spanRootPolicy),XL6_FOREIGN_VIEW_V1_TAG,"callback.span",0,1,NULL,NULL,destroy,sizeof(SpanCall),_Alignof(SpanCall),begin,end,trace,busy};
int span_fire(SpanRoot* root,int mode)
{
    SpanCall call={0};assert(begin(root,&call));SpanCallback callback=root->callback;void* data=root->data;SpanOwner owner=root->owner;
    int result=-1;
    if(callback && owner.Hold(data)){
        static const unsigned char bytes[]={97,0,98};const unsigned char* input=mode==1 || mode==2 ? NULL:bytes;
        size_t count=mode==1 ? 0:mode==4 ? SIZE_MAX:sizeof(bytes);
        result=callback(input,count,mode,data);(void)owner.Drop(data);
    }
    assert(end(&call));return result;
}
int span_fire_pair(SpanRoot* root,int mode)
{
    SpanCall call={0};assert(begin(root,&call));SpanPairCallback callback=root->pair;void* data=root->pairData;SpanOwner owner=root->pairOwner;
    int result=-1;
    if(callback && owner.Hold(data)){
        static const char first[]={97,0,98};static const unsigned char second[]={0xE4,0xBD,0xA0};
        result=callback(data,mode==2 ? NULL:first,mode==1 ? -1:mode==2 ? 0:3,mode==2 ? NULL:second,mode==2 ? 0:3);(void)owner.Drop(data);
    }
    assert(end(&call));return result;
}
int span_live(void){return live;}
static xmemdebugsnapshot before;
void span_counter_begin(void){assert(xrtMemDebugEnabled());xrtMemDebugSnapshot(&before);}
void span_counter_end(void){xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);}
