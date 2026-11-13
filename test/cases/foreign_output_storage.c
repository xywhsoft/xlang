#include "foreign_output_storage.h"
#undef NDEBUG
#include <assert.h>
static unsigned calls;
static xmemdebugsnapshot before;
static void empty(const StorageValue* value)
{
    assert(value && value->size==sizeof(*value) && !value->tag);
    for(int i=0;i<4;++i)assert(!value->words[i]);
}
int storage_read(int code,int partial,const StorageValue* input,StorageValue* output)
{
    ++calls;assert(input && input!=output && input->tag==7 && input->words[0]==9);
    empty(output);output->tag=31;output->words[0]=input->words[0]+1;
    if(!partial)output->words[3]=99;
    return code;
}
int storage_pair(int code,const StorageValue* input,StorageValue* first,StorageValue* second)
{
    ++calls;assert(input && input!=first && input!=second && first!=second);
    empty(first);empty(second);first->tag=41;first->words[0]=input->words[0];second->tag=51;
    return code;
}
unsigned storage_calls(void){return calls;}
void storage_heap_begin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(0));}
void storage_heap_end(void)
{
    xrtMemDebugFailClear();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount && after.ReallocCount==before.ReallocCount);
}
