#include "analysis/ast_clone.h"
#include "src/parser/xl6_parser_api.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned invalidPoolFrees,poolFrees;
static uint64 live(void)
{xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
bool __real_xrtPoolFree(xpool*,ptr);
bool __wrap_xrtPoolFree(xpool* pool,ptr node)
{
    bool okay=__real_xrtPoolFree(pool,node);
    ++poolFrees;if(!okay){++invalidPoolFrees;fputs("actual invalid pool release\n",stderr);fflush(stderr);exit(1);}
    return okay;
}

static int check(const char* body,unsigned number)
{
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    if(!xl6ParseSource(body,"ast-replacement.xl",&parsed))return 0;
    xls_ast_pool_work work={0};
    XL6_AstNode* pooled=xlsAstClonePooled(parsed.root,NULL,NULL,NULL,SIZE_MAX,&work,NULL);
    if(!pooled || !work.nodes)return 0;
    xl6AstFree(parsed.root);parsed.root=pooled;
    int okay=xl6StdlibExpandModule(parsed.root,&parsed.diags) &&
        xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"replacement",1);
    printf("replacement case=%u semantic=%d pool frees=%u invalid=%u\n",number,okay,poolFrees,invalidPoolFrees);
    fflush(stdout);
    /* A failing producer has already lost allocation identity. Exit at the
     * observed invalid pool release instead of hiding it with teardown damage. */
    if(invalidPoolFrees)return 0;
    xl6ParseResultFree(&parsed);
    return okay && !invalidPoolFrees;
}

int main(void)
{
    xrtMemStatsEnable(true);
    const char* prefix=
        "extern \"native_text\" @no_raise func nativeText() as point\n"
        "extern \"native_free\" @no_raise func nativeFree(point value)\n"
        "func text() as cstring\n return cstring.take(nativeText(),nativeFree)\nend\n";
    const char* cases[]={"class Box\n func M(int value) as int\n return value\n end\nend\nfunc choose(bool flag,Box box) as func(int) as int\n return flag ? box.M : box.M\nend\n"};
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);++i){
        xstrbuf body={0};
        if(!xrtStrBufAppend(&body,xrtStrView(prefix)) || !xrtStrBufAppend(&body,xrtStrView(cases[i])))return 1;
        int okay=check(body.Data,i);if(!okay)return 1;
        uint64 before=live();okay=check(body.Data,i);
        if(!okay || live()!=before){fprintf(stderr,"replacement retained allocations: %llu -> %llu\n",(unsigned long long)before,(unsigned long long)live());return 1;}
        xrtStrBufFree(&body);
    }
    printf("analysis_ast_replacement_test passed: %u valid native pool releases\n",poolFrees);
    return 0;
}
