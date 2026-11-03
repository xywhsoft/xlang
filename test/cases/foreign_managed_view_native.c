#include <xrt_decl.h>
#include "test/cases/foreign_managed_view.h"
#include "test/cases/foreign_expanded_calls.h"
#include <assert.h>
struct ManagedRoot {ManagedWatch* watches;ManagedChild* children;unsigned depth;bool closing,failDestroy;int value;ManagedCallback callback;void* userdata;ManagedCallbackOwner callbackOwner;};
int span_points_write(SpanPoint* data,int count)
{for(int i=0;i<count;++i)data[i]=(SpanPoint){(float)i+1,(float)i+2};return count;}
int span_points_sum(const SpanPoint* data,int count)
{int sum=0;for(int i=0;i<count;++i)sum+=(int)(data[i].x+data[i].y);return sum;}
static int roots,children;
static void unwatch(void* raw)
{
    ManagedWatch* link=raw;
    if(link->previous){*link->previous=link->next;if(link->next)link->next->previous=link->previous;}
    *link=(ManagedWatch){0};
}
static bool attach(ManagedWatch** head,void* raw,void (*notify)(void*),void* user)
{
    ManagedWatch* link=raw;
    if(link->previous || link->next || link->notify || link->user)return false;
    link->next=*head;link->previous=head;link->notify=notify;link->user=user;
    if(*head)(*head)->previous=&link->next;
    *head=link;return true;
}
static void invalidate(ManagedWatch** head)
{
    while(*head){ManagedWatch* link=*head;void (*notify)(void*)=link->notify;void* user=link->user;unwatch(link);notify(user);}
}
static bool rootWatch(void* raw,void* storage,void (*notify)(void*),void* user)
{ManagedRoot* root=raw;return !root->closing && attach(&root->watches,storage,notify,user);}
static bool childWatch(void* raw,void* storage,void (*notify)(void*),void* user)
{ManagedChild* child=raw;return !child->closing && attach(&child->watches,storage,notify,user);}
static void flush(ManagedRoot* root)
{
    if(root->depth)return;
    ManagedChild** slot=&root->children;
    while(*slot){ManagedChild* child=*slot;
        if(child->closing){*slot=child->next;--children;xrtFree(child);}else slot=&child->next;}
    if(root->closing){--roots;xrtFree(root);}
}
void managed_root_destroy(ManagedRoot* root)
{
    if(!root)return;
    if(!root->closing){root->closing=true;managed_set_callback(root,NULL,NULL,NULL);invalidate(&root->watches);
        for(ManagedChild* child=root->children;child;child=child->next){child->closing=true;invalidate(&child->watches);}}
    flush(root);
}
void managed_child_destroy(ManagedChild* child)
{
    if(!child)return;
    ManagedRoot* root=child->root;
    if(!child->closing){child->closing=true;invalidate(&child->watches);}
    flush(root);
}
static bool destroyRoot(void* raw){bool fail=((ManagedRoot*)raw)->failDestroy;managed_root_destroy(raw);return !fail;}
static bool destroyChild(void* raw){managed_child_destroy(raw);return true;}
static bool begin(void* raw,void* storage)
{
    ManagedRoot* root=raw;ManagedScope* scope=storage;
    if(root->closing || scope->self || scope->root)return false;
    scope->self=scope;scope->root=root;++root->depth;return true;
}
static bool end(void* storage)
{
    ManagedScope* scope=storage;
    if(scope->self!=scope || !scope->root || !scope->root->depth)return false;
    ManagedRoot* root=scope->root;*scope=(ManagedScope){0};--root->depth;flush(root);return true;
}
static bool traceRoot(const void* raw,XL6_ModuleOwnershipVisit visit,void* data)
{
    const ManagedRoot* root=raw;
    return !root->callbackOwner.reference.Ops || visit(root->callbackOwner.reference,data);
}
static bool busy(const void* raw){return ((const ManagedRoot*)raw)->depth!=0;}
int managed_root_create(int mode,ManagedRoot** output)
{
    if(mode==2)return -1;
    *output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;
    (*output)->value=42;++roots;
    if(mode==4)(void)xrtMemDebugFailAfter(0);
    return mode==1 ? -1 : 0;
}
int managed_child_create(ManagedRoot* root,int mode,ManagedChild** output)
{
    if(mode==2)return -1;
    ManagedChild* child=xrtCalloc(1,sizeof(*child));if(!child)return -1;
    child->root=root;child->value=17;child->next=root->children;root->children=child;++children;*output=child;
    if(mode==3){managed_child_destroy(child);return -1;}
    if(mode==4)(void)xrtMemDebugFailAfter(0);
    return mode==1 ? -1 : 0;
}
int managed_from_bytes(const uint8_t* input,size_t count,ManagedRoot** output)
{assert(!count || input);int result=managed_root_create(0,output);if(!result)(*output)->value=(int)count;return result;}
int managed_from_root(ManagedRoot* input,ManagedRoot** output)
{assert(input && !input->closing);int result=managed_root_create(0,output);if(!result)input->failDestroy=true;return result;}
int managed_root_read(ManagedRoot* root){return root->value;}
int managed_root_method_read(ManagedRoot* root){return root->value;}
int managed_roots_live(void){return roots;}
int managed_children_live(void){return children;}
int managed_child_read(ManagedChild* child){return child->value;}
ManagedRoot* managed_child_owner(ManagedChild* child){return child->root;}
ManagedChild* managed_child_first(ManagedRoot* root)
{for(ManagedChild* child=root->children;child;child=child->next)if(!child->closing)return child;return NULL;}
void managed_reset_failure(void){(void)xrtMemDebugFailAfter(-1);}
static int reader;
void* managed_reader_open(void){return &reader;}
int managed_reader_value(void* self,ManagedRoot* root)
{assert(self==&reader);return managed_root_read(root);}
ManagedChild* managed_reader_child(void* self,ManagedRoot* root)
{assert(self==&reader);return managed_child_first(root);}
const XL6_ForeignViewDescriptorV1 managedRootPolicy={
    sizeof(managedRootPolicy),XL6_FOREIGN_VIEW_V1_TAG,"managed.test.root",
    sizeof(ManagedWatch),_Alignof(ManagedWatch),rootWatch,unwatch,destroyRoot,
    sizeof(ManagedScope),_Alignof(ManagedScope),begin,end,traceRoot,busy};
const XL6_ForeignViewDescriptorV1 managedChildPolicy={
    sizeof(managedChildPolicy),XL6_FOREIGN_VIEW_V1_TAG,"managed.test.child",
    sizeof(ManagedWatch),_Alignof(ManagedWatch),childWatch,unwatch,destroyChild,0,1,NULL,NULL,NULL,NULL};

void managed_set_callback(ManagedRoot* root,ManagedCallback fn,void* data,const ManagedCallbackOwner* owner)
{
    if(owner && (!fn || !data || !owner->Hold(data))){xrtSetErrorKind(XERR_STATE);return;}
    ManagedCallbackOwner old=root->callbackOwner;void* previous=root->userdata;
    root->callback=fn;root->userdata=data;root->callbackOwner=owner ? *owner : (ManagedCallbackOwner){0};
    if(old.Drop)(void)old.Drop(previous);
}
void managed_reader_set(void* self,ManagedRoot* root,ManagedCallback fn,void* data,const ManagedCallbackOwner* owner)
{assert(self==&reader);managed_set_callback(root,fn,data,owner);}
int managed_reader_span(void* self,ManagedRoot* root,const uint8_t* data,size_t count)
{assert(self==&reader);return managed_span_read(root,data,count);}
int managed_fire(ManagedRoot* root,int value)
{
    ManagedCallback fn=root->callback;void* data=root->userdata;ManagedCallbackOwner owner=root->callbackOwner;
    if(!fn)return 0;
    if(!owner.Hold(data)){xrtSetErrorKind(XERR_STATE);return -1;}
    int result=fn(managed_child_first(root),value,data);
    (void)owner.Drop(data);return result;
}
static xmemdebugsnapshot counterBefore;
void managed_counter_begin(void){xrtMemDebugSnapshot(&counterBefore);}
void managed_counter_end(void)
{ xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
  assert(counterBefore.AllocCount==after.AllocCount && counterBefore.FreeCount==after.FreeCount && counterBefore.ReallocCount==after.ReallocCount); }
int managed_span_read(ManagedRoot* root,const unsigned char* data,size_t count)
{assert(root && (!count || data));int sum=0;for(size_t i=0;i<count;++i)sum+=data[i];return sum;}
int managed_span_write(ManagedRoot* root,unsigned char* data,int count)
{assert(root && count>=0 && (!count || data));for(int i=0;i<count;++i)data[i]++;return count;}
int managed_span_write_callback(ManagedRoot* root,uint8_t* data,int count)
{if(managed_fire(root,1)<0)return -1;return managed_span_write(root,data,count);}
int managed_span_floats(ManagedRoot* root,const float* data,int count)
{assert(root && count>=0 && (!count || data));int sum=0;for(int i=0;i<count;++i)sum+=(int)data[i];return sum;}
