#include "foreign_global_callback.h"
#include "src/runtime/xl6_callback_failure.h"
#include <assert.h>
#include <limits.h>
typedef struct Slot { GlobalHandler proc;void* user;GlobalOwner owner; } Slot;
static Slot slots[2];
static bool hold(Slot value)
{ return !value.owner.Ref.Data || value.owner.Hold(value.owner.Ref.Data); }
static bool drop(Slot value)
{ return !value.owner.Ref.Data || value.owner.Drop(value.owner.Ref.Data); }
int global_callback_set(GlobalHandler a,void* au,const GlobalOwner* ao,GlobalHandler b,void* bu,const GlobalOwner* bo)
{
    Slot next[2]={0};
    next[0].proc=a;next[0].user=au;if(ao)next[0].owner=*ao;
    next[1].proc=b;next[1].user=bu;if(bo)next[1].owner=*bo;
    for(int i=0;i<2;++i)if((next[i].proc!=NULL)!=(next[i].owner.Ref.Data!=NULL) ||
        (next[i].proc && (!next[i].owner.Hold || !next[i].owner.Drop))){xrtSetErrorKind(XERR_ARGUMENT);return -1;}
    if(!hold(next[0]))return -1;
    if(!hold(next[1])){(void)drop(next[0]);return -1;}
    Slot old[2]={slots[0],slots[1]};slots[0]=next[0];slots[1]=next[1];
    bool second=drop(old[1]),first=drop(old[0]);return first && second ? 0:-1;
}
typedef struct Fire {int count;} Fire;
static void invoke(Fire* state)
{
    GlobalEvent event={23};
    for(int n=0;n<state->count;++n)for(int i=0;i<2;++i){
        Slot snapshot=slots[i];assert(hold(snapshot));
        if(snapshot.proc)snapshot.proc(&event,snapshot.user);
        assert(drop(snapshot));
    }
}
static int32 worker(void* raw)
{invoke(raw);return 0;}
void global_callback_fire(int count,int threaded)
{
    Fire state={count};
    if(!threaded){invoke(&state);return;}
    xthread* thread=xrtThreadCreate(worker,&state,0);
    if(!thread)return;
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
}
int global_callback_pending(void)
{uint64_t p,t;assert(xl6CallbackFailureCountV1(&p,&t));return p>INT_MAX?INT_MAX:(int)p;}
int global_callback_total(void)
{uint64_t p,t;assert(xl6CallbackFailureCountV1(&p,&t));return t>INT_MAX?INT_MAX:(int)t;}
int global_callback_live(void)
{return (slots[0].proc!=NULL)+(slots[1].proc!=NULL);}
