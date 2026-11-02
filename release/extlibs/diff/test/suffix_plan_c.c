/* 只在夹具中观察分配请求；产品代码不增加探针或性能分支。 */
#define XRT_MODULE_HASH64
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <string.h>
static bool trace;
static struct {size_t table,last;unsigned mallocs,after;} observed;
static void* allocate(size_t bytes)
{
    if(trace){observed.last=bytes;++observed.mallocs;if(xrtErrorKind(xrtGetError())==XERR_MEMORY)++observed.after;}
    return xrtMalloc(bytes);
}
static void* zero_allocate(size_t count,size_t size)
{
    if(trace){observed.table=count*size;if(xrtErrorKind(xrtGetError())==XERR_MEMORY)++observed.after;}
    return xrtCalloc(count,size);
}
#undef xrtMalloc
#undef xrtCalloc
#define xrtMalloc allocate
#define xrtCalloc zero_allocate
#include "../diff_native.c"
#undef xrtMalloc
#undef xrtCalloc
static unsigned cases,checks,failures,oomBudgets,oomFailures;
static void require(bool ok,const char* label)
{++checks;if(!ok){if(failures<12)fprintf(stderr,"diff suffix plan: %s\n",label);++failures;}}
static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}
static char* text(unsigned lines)
{
    char* result=xrtMalloc((size_t)lines*16+1);if(!result)return NULL;
    size_t used=0;for(unsigned i=0;i<lines;++i)used+=(size_t)sprintf(result+used,"row-%04u\n",i);
    result[used]=0;return result;
}
static void one(const char* label,const char* old,const char* new,size_t oldCount,size_t newCount,size_t suffix)
{
    xmemdebugsnapshot before,live;xrtClearError();xrtMemDebugSnapshot(&before);
    uint64 leftHash=xrtHash64(old,strlen(old)),rightHash=xrtHash64(new,strlen(new));
    size_t a=oldCount-suffix,b=newCount-suffix,capacity=a+b+suffix;
    size_t table=a && b ? (a+1)*(b+1)*sizeof(uint16_t) : 0;
    size_t expected=(oldCount+newCount)*sizeof(xdiff_line)+table+capacity*sizeof(xdiff_step);
    memset(&observed,0,sizeof(observed));trace=true;
    xdiff_plan plan;bool ok=xdiff_prepare(old,new,&plan);trace=false;
    xrtMemDebugSnapshot(&live);
    require(ok && !xrtGetError(),"prepare succeeds");
    if(ok){
        require(plan.old_count==oldCount && plan.new_count==newCount,"full source counts preserved");
        require(observed.table==table && (plan.table!=NULL)==(table!=0),"matrix contains only unmatched prefix");
        require(capacity ? observed.last==capacity*sizeof(xdiff_step) : observed.mallocs==0,"steps reserve exact safe bound, empty plan allocates nothing");
        require(live.LiveBytes-before.LiveBytes==expected,"prepared live bytes equal arrays plus reduced matrix/steps");
        printf("plan\t%s\t%zu\t%zu\t%zu\t%zu\t%zu\n",label,oldCount,newCount,suffix,observed.table,live.LiveBytes-before.LiveBytes);
        xdiff_unit(&plan);
    }
    require(balanced(&before),"normal plan seven metrics balanced");
    bool completed=false;
    for(unsigned budget=0;budget<8;++budget){
        xrtClearError();xrtMemDebugSnapshot(&before);memset(&observed,0,sizeof(observed));
        require(xrtMemDebugFailAfter(budget),"arm actual plan allocator failure");trace=true;
        ok=xdiff_prepare(old,new,&plan);trace=false;
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++oomBudgets;
        if(hit){require(!ok && xrtErrorKind(xrtGetError())==XERR_MEMORY,"real allocation error propagates");++oomFailures;}
        else{require(ok && !xrtGetError(),"budget scan reaches success");completed=true;}
        require(observed.after==0,"never allocate steps after an earlier allocation failed");
        if(ok)xdiff_unit(&plan);
        require(balanced(&before),"failed preparation releases all partial owners");
        if(completed)break;
    }
    require(completed,"success appears within known bounded allocation count");
    require(leftHash==xrtHash64(old,strlen(old)) && rightHash==xrtHash64(new,strlen(new)),"input bytes unchanged");++cases;
}
int main(void)
{
    if(!xrtMemDebugEnabled())return 2;
    char* warm=xdiff_stats("","");xrtFree(warm);xrtClearError();
    one("empty","","",0,0,0);
    const unsigned sizes[]={1,16,256,1024,2048};
    for(unsigned i=0;i<5;++i){
        unsigned n=sizes[i];char* old=text(n);char* changed=text(n);char* appended=text(n);
        if(!old || !changed || !appended)return 2;
        one("identical",old,old,n,n,n);
        changed[0]='X';one("early-edit",old,changed,n,n,n-1);
        appended[(size_t)n*9-2]='X';one("late-edit",old,appended,n,n,0);
        if(n>1){one("delete-first",old,old+9,n,n-1,n-1);one("insert-first",old+9,old,n-1,n,n-1);}
        xrtFree(old);xrtFree(changed);xrtFree(appended);
    }
    one("duplicate-tie","a\na","a",2,1,1);
    printf("diff suffix plan: %u cases, %u checks, %u actual OOM/%u budgets, %u failures\n",cases,checks,oomFailures,oomBudgets,failures);
    return failures ? 1 : 0;
}
