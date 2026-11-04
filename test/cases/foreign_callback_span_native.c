#include "foreign_callback_span.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct SpanRoot {unsigned depth;bool closing;SpanCallback callback;SpanPairCallback pair;SpanTextCallback text;void* data;void* pairData;void* textData;SpanOwner owner,pairOwner,textOwner;};
static int live;static bool failEnd;
static void flush(SpanRoot* root){if(root->closing && !root->depth){--live;xrtFree(root);}}
static bool begin(void* raw,void* storage)
{SpanRoot* root=raw;SpanCall* call=storage;if(root->closing || call->self)return false;call->self=call;call->root=root;++root->depth;return true;}
static bool end(void* storage)
{SpanCall* call=storage;assert(call->self==call && call->root->depth);SpanRoot* root=call->root;bool failed=failEnd;failEnd=false;*call=(SpanCall){0};--root->depth;flush(root);return !failed;}
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
void span_set_text(SpanRoot* root,SpanTextCallback callback,void* data,const SpanOwner* owner)
{
    if(owner && (!callback || !data || !owner->Hold(data))){xrtSetErrorKind(XERR_STATE);return;}
    if(root->closing){if(owner)(void)owner->Drop(data);xrtSetErrorKind(XERR_STATE);return;}
    SpanOwner old=root->textOwner;void* previous=root->textData;
    root->text=callback;root->textData=data;root->textOwner=owner ? *owner : (SpanOwner){0};
    if(old.Drop)(void)old.Drop(previous);
}
void span_set_optional_text(SpanRoot* root,SpanTextCallback callback,void* data,const SpanOwner* owner)
{span_set_text(root,callback,data,owner);}
void span_destroy(SpanRoot* root)
{
    if(!root || root->closing)return;
    SpanOwner a=root->owner,b=root->pairOwner,c=root->textOwner;void* da=root->data;void* db=root->pairData;void* dc=root->textData;
    root->closing=true;root->callback=NULL;root->pair=NULL;root->text=NULL;root->owner=(SpanOwner){0};root->pairOwner=(SpanOwner){0};root->textOwner=(SpanOwner){0};
    if(a.Drop)(void)a.Drop(da);
    if(b.Drop)(void)b.Drop(db);
    if(c.Drop)(void)c.Drop(dc);
    flush(root);
}
static bool destroy(void* raw){span_destroy(raw);return true;}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{const SpanRoot* root=raw;return (!root->owner.reference.Ops || visit(root->owner.reference,user)) && (!root->pairOwner.reference.Ops || visit(root->pairOwner.reference,user)) && (!root->textOwner.reference.Ops || visit(root->textOwner.reference,user));}
static bool busy(const void* raw){return ((const SpanRoot*)raw)->depth!=0;}
static const XL6_ForeignViewDescriptorV1 spanPolicyData={sizeof(spanPolicyData),XL6_FOREIGN_VIEW_V1_TAG,"callback.span",0,1,NULL,NULL,destroy,sizeof(SpanCall),_Alignof(SpanCall),begin,end,trace,busy};
const XL6_ForeignViewDescriptorV1* span_policy_get(void){return &spanPolicyData;}
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
int span_fire_text(SpanRoot* root,int mode)
{
    SpanCall call={0};assert(begin(root,&call));SpanTextCallback callback=root->text;void* data=root->textData;SpanOwner owner=root->textOwner;int result=-1;
    if(callback && owner.Hold(data)){
        static const unsigned char input[]={97,0,98};
        xstrview text=callback(input,sizeof(input),mode,data);
        assert(text.Size==0 || text.Size==3);
        if(mode==1)assert(text.Data && !text.Size); /* Present empty string. */
        if(mode==2)assert(!text.Data && !text.Size); /* Nullable absence. */
        if(!text.Data)assert(!text.Size);
        else if(text.Size){
            static const unsigned char unicode[]={0xE4,0xBD,0xA0};
            assert(!memcmp(text.Data,mode==3 ? (const void*)unicode : (const void*)input,3));
        }
        result=(int)text.Size;
        xrtFree((void*)text.Data); /* Consume BEFORE dropping callback snapshot. */
        (void)owner.Drop(data);
    }
    assert(end(&call));return result;
}
void span_fail_end_once(void){failEnd=true;}
void span_text_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void span_text_reset(void){assert(xrtMemDebugFailAfter(-1));}
static xmemdebugsnapshot textHeapBefore,textCountsBefore;
void span_text_heap_begin(void){assert(xrtMemDebugEnabled());xrtMemDebugSnapshot(&textHeapBefore);}
void span_text_heap_end(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    if(textHeapBefore.LiveCount!=after.LiveCount || textHeapBefore.LiveBytes!=after.LiveBytes || textHeapBefore.InvalidFreeCount!=after.InvalidFreeCount || textHeapBefore.DoubleFreeCount!=after.DoubleFreeCount)
        fprintf(stderr,"text heap: live %llu/%llu bytes %llu/%llu invalid %llu/%llu double %llu/%llu\n",(unsigned long long)textHeapBefore.LiveCount,(unsigned long long)after.LiveCount,(unsigned long long)textHeapBefore.LiveBytes,(unsigned long long)after.LiveBytes,(unsigned long long)textHeapBefore.InvalidFreeCount,(unsigned long long)after.InvalidFreeCount,(unsigned long long)textHeapBefore.DoubleFreeCount,(unsigned long long)after.DoubleFreeCount);
    assert(textHeapBefore.LiveCount==after.LiveCount && textHeapBefore.LiveBytes==after.LiveBytes && textHeapBefore.InvalidFreeCount==after.InvalidFreeCount && textHeapBefore.DoubleFreeCount==after.DoubleFreeCount);
}
void span_text_counts_begin(void){assert(xrtMemDebugEnabled());xrtMemDebugSnapshot(&textCountsBefore);}
void span_text_counts_end(int count){xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.AllocCount-textCountsBefore.AllocCount==(uint64_t)count && after.FreeCount-textCountsBefore.FreeCount==(uint64_t)count && textCountsBefore.ReallocCount==after.ReallocCount);}
static xmemdebugsnapshot before;
void span_counter_begin(void){assert(xrtMemDebugEnabled());xrtMemDebugSnapshot(&before);}
void span_counter_end(void){xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);}
