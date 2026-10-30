/* 结果、所有权与复制预算分开检查；旧实现结果正确但会额外复制 move 子树。 */
#define XRT_MODULE_JSON_READ
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/json_patch.h"
#include <stdio.h>
#include <string.h>

static unsigned clone_calls;
static xvalue* observe_clone(const xvalue* value)
{
    ++clone_calls;
    return xrtValueDeepClone(value);
}

typedef struct { const char *doc, *patch, *expected; unsigned clones; } test_case;
#define DOC "{\"src\":{\"items\":[1,{\"name\":\"old\"}]},\"dst\":0}"
#define OP(kind,from,to) "[{\"op\":\"" kind "\",\"from\":\"" from "\",\"path\":\"" to "\"}]"
static const test_case cases[] = {
    {DOC,OP("move","/src","/dst"),"{\"dst\":{\"items\":[1,{\"name\":\"old\"}]}}",1},
    {DOC,OP("copy","/src","/dst"),"{\"src\":{\"items\":[1,{\"name\":\"old\"}]},\"dst\":{\"items\":[1,{\"name\":\"old\"}]}}",2},
    {DOC,OP("move","/src",""),"{\"items\":[1,{\"name\":\"old\"}]}",1},
    {DOC,OP("copy","/src",""),"{\"items\":[1,{\"name\":\"old\"}]}",2},
    {DOC,OP("move","/src","/src"),DOC,1},
    {DOC,OP("copy","/src","/src"),DOC,2},
    {DOC,OP("move","",""),DOC,1},
    {"[1,2,3]",OP("move","/0","/2"),"[2,3,1]",1},
    {"[1,2,3]",OP("move","/2","/0"),"[3,1,2]",1},
    {"[1,2,3]",OP("move","/0","/-"),"[2,3,1]",1},
    {"{\"a/b\":{\"~\":7}}",OP("move","/a~1b","/out"),"{\"out\":{\"~\":7}}",1},
    {"[{\"value\":9},0]",OP("move","/0",""),"{\"value\":9}",1},
    {DOC,OP("move","/src","/missing/child"),NULL,1},
    {DOC,OP("move","/src","/src/child"),NULL,1},
    {DOC,"[{\"op\":\"move\",\"from\":\"/src\",\"path\":\"/dst\"},{\"op\":\"remove\",\"path\":\"/missing\"}]",NULL,1},
    {DOC,"[{\"op\":\"copy\",\"from\":\"/src\",\"path\":\"/dst\"},{\"op\":\"replace\",\"path\":\"/dst/items/1/name\",\"value\":\"new\"}]",
        "{\"src\":{\"items\":[1,{\"name\":\"old\"}]},\"dst\":{\"items\":[1,{\"name\":\"new\"}]}}",3}
};

static xvalue* parse(const char* text)
{
    return xrtJsonParse((xstrview){text,strlen(text)});
}

static bool balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount;
}

static bool run_case(const test_case* c, bool fault, bool input_first, unsigned* excess)
{
    xmemdebugsnapshot base;
    xrtClearError(); xrtMemDebugSnapshot(&base);
    xvalue *doc=parse(c->doc), *patch=parse(c->patch), *expected=c->expected ? parse(c->expected) : NULL;
    xvalue *original=parse(c->doc), *operations=parse(c->patch);
    bool ok=doc && patch && original && operations && (!c->expected || expected);
    bool completed=false, injected=false;
    for (unsigned point=0; ok && point<(fault ? 256u : 1u); ++point) {
        xrtClearError();
        clone_calls=0;
        if (fault && !xrtMemDebugFailAfter(point)) { ok=false; break; }
        xvalue* result=xjsonpatch_apply(doc,patch);
        bool triggered=fault && xrtMemDebugFailTriggered();
        if (fault) xrtMemDebugFailClear();
        xerrkind kind=xrtErrorKind(xrtGetError());
        if (triggered) { injected=true; ok=xrtValueType(result)==XVALUE_NULL && kind==XERR_MEMORY; }
        else {
            ok=c->expected ? (kind==0 && xrtValueEqual(result,expected)) :
                (kind==XERR_VALUE && xrtValueType(result)==XVALUE_NULL);
            if (!fault && clone_calls!=c->clones) ++*excess;
        }
        xrtClearError();
        ok=ok && xrtValueEqual(doc,original) && xrtValueEqual(patch,operations);
        if (input_first && !fault) {
            xrtValueRelease(doc); doc=NULL; xrtValueRelease(patch); patch=NULL;
            if (c->expected) ok=ok && xrtValueEqual(result,expected);
        }
        xrtValueRelease(result);
        if (triggered) {
            // 同一输入立即重试，检查半成品释放和原子性。
            result=xjsonpatch_apply(doc,patch);
            kind=xrtErrorKind(xrtGetError());
            ok=ok && (c->expected ? (kind==0 && xrtValueEqual(result,expected)) :
                (kind==XERR_VALUE && xrtValueType(result)==XVALUE_NULL));
            xrtValueRelease(result);
        } else { completed=true; break; }
    }
    xrtValueRelease(doc); xrtValueRelease(patch); xrtValueRelease(expected);
    xrtValueRelease(original); xrtValueRelease(operations);
    return balanced(base) && ok && completed && (!fault || injected);
}

/* 只报告同次调用的实际分配次数，不把局部成本外推成应用性能。 */
static int costs(void)
{
    static const unsigned sizes[]={0,1,8,64,512,4096};
    for (unsigned i=0; i<sizeof(sizes)/sizeof(sizes[0]); ++i) for (unsigned copy=0; copy<2; ++copy) {
        xvalue *doc=xrtValueObject(), *array=xrtValueArray();
        xvalue* patch=parse(copy ? OP("copy","/src","/dst") : OP("move","/src","/dst"));
        bool ok=doc && array && patch;
        for (unsigned n=0; ok && n<sizes[i]; ++n)
            ok=xrtValueArrayAppendNew(array,xrtValueString(XRT_STR_LITERAL("0123456789abcdef-payload")));
        if (!ok || !xrtValueObjectSetTake(doc,XRT_STR_LITERAL("src"),&array)) {
            xrtValueRelease(array); xrtValueRelease(doc); xrtValueRelease(patch); return 2;
        }
        xmemdebugsnapshot before,after;
        clone_calls=0; xrtClearError(); xrtMemDebugSnapshot(&before);
        xvalue* result=xjsonpatch_apply(doc,patch);
        xrtMemDebugSnapshot(&after);
        ok=result && !xjsonpatch_has_error();
        if (ok) printf("%u\t%s\t%u\t%llu\n",sizes[i],copy ? "copy" : "move",clone_calls,
            (unsigned long long)(after.AllocCount-before.AllocCount));
        xrtValueRelease(result); xrtValueRelease(doc); xrtValueRelease(patch);
        if (!ok) return 2;
    }
    return ferror(stdout) ? 2 : 0;
}

int main(int argc,char** argv)
{
    unsigned failures=0, excess=0, checks=0, faults=0;
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE,"test.json.patch",0,"warm XRT slot"); xrtClearError();
    if (argc==2 && strcmp(argv[1],"--costs")==0) return costs();
    if (argc!=1) return 2;
    for (unsigned i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
        for (unsigned order=0; order<2; ++order) {
            ++checks;
            if (!run_case(&cases[i],false,order!=0,&excess)) { ++failures; fprintf(stderr,"move case %u order %u failed\n",i,order); }
        }
        ++faults;
        if (!run_case(&cases[i],true,false,&excess)) { ++failures; fprintf(stderr,"move OOM case %u failed\n",i); }
    }
    printf("json.patch move: %u cases, %u OOM paths, %u semantic failures, %u clone-budget failures\n",checks,faults,failures,excess);
    return failures!=0 || excess!=0;
}

#define xrtValueDeepClone observe_clone
#include "../json_patch_native.c"
