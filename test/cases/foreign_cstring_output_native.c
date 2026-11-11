#include "foreign_managed_view_native.c"
#include "foreign_cstring_output.h"
#include <string.h>
#include <stdio.h>
int cstr_seed(ManagedRoot* root)
{
    ManagedChild* child=NULL;int result=managed_child_create(root,0,&child);
    if(!result)memcpy(child->countedText,"A\xe4\xbd\xa0\xe5\xa5\xbd",8);
    return result;
}
int cstr_read(ManagedRoot* root,int mode,const char** output)
{
    assert(root && managed_child_first(root));
    if(mode==1){*output=(const char*)(uintptr_t)1;return -29;}
    if(mode==2){*output=NULL;return 0;}
    if(mode==3){*output="";return 0;}
    *output=managed_child_first(root)->countedText;return 0;
}
int cstr_optional(ManagedRoot* root,int mode,const char** output){return cstr_read(root,mode,output);}
int cstr_get(ManagedRoot* root,const char** output){return cstr_read(root,0,output);}
int cstr_pair(ManagedRoot* root,const char** text,int* number)
{*number=83;return cstr_read(root,0,text);}
int cstr_pair_text(ManagedRoot* root,const char** first,const char** second)
{*second="second";return cstr_read(root,0,first);}
int cstr_first(const char** text,int mode)
{*text=mode==1 ? (const char*)(uintptr_t)1:"free";return mode==1 ? -29:0;}
void cstr_void(const char** text,int mode){assert(!mode);*text="void";}
void cstr_change(ManagedRoot* root)
{ManagedChild* child=managed_child_first(root);assert(child);memcpy(child->countedText,"changed",8);}
void cstr_fail_after(int count){(void)xrtMemDebugFailAfter(count);}
int cstr_span(ManagedRoot* root,const uint8_t* data,int count,const char** output)
{assert(data && count==3 && data[0]=='a' && !data[1] && data[2]=='b');return cstr_read(root,0,output);}
static int cstr_field(CstrReader* reader,ManagedRoot* root,const char** output)
{assert(reader);return cstr_read(root,0,output);}
CstrReader* cstr_reader(void){static CstrReader reader={cstr_field};return &reader;}
void cstr_heap_end(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==counterBefore.LiveCount && after.LiveBytes==counterBefore.LiveBytes &&
        after.DoubleFreeCount==counterBefore.DoubleFreeCount && after.InvalidFreeCount==counterBefore.InvalidFreeCount);
}
void cstr_counter_end(int copies)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    fprintf(stderr,"C-string output counters: calls=%d alloc=%llu free=%llu realloc=%llu\n",copies,
        (unsigned long long)(after.AllocCount-counterBefore.AllocCount),
        (unsigned long long)(after.FreeCount-counterBefore.FreeCount),
        (unsigned long long)(after.ReallocCount-counterBefore.ReallocCount));
    assert(after.AllocCount==counterBefore.AllocCount+(uint64_t)copies &&
        after.FreeCount==counterBefore.FreeCount+(uint64_t)copies &&
        after.ReallocCount==counterBefore.ReallocCount);
}
