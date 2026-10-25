#define main checkMaterializeBaseMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/module_store.h"

static uint64 cmLive(void)
{xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static size_t cmPooled(const XL6_AstNode* node)
{
    if(!node)return 0;
    size_t count=!!node->xlsStorage;count+=cmPooled(node->inferredType)+cmPooled(node->semanticDeclaredType);
    for(size_t i=0;i<node->childCount;++i)count+=cmPooled(node->children[i]);
    return count;
}
static int cmBody(xstrbuf* body,unsigned version)
{
    CHECK(xrtStrBufAppend(body,xrtStrView("\xef\xbb\xbf// 😀\r\nfunc identity(int value) as int\r\n return value\r\nend\r\n")));
    for(unsigned i=0;i<40;++i)CHECK(xrtStrBufAppendFormat(body,"func work_%u(int value) as int\r\n int local=identity(value)+%u\r\n if local>0 then\r\n return local\r\n end\r\n return value\r\nend\r\n",i,i));
    if(version==3)CHECK(xrtStrBufAppend(body,xrtStrView("func incomplete(int\r\n")));
    if(version==4)CHECK(xrtStrBufAppend(body,xrtStrView("func inserted() as int\r\n return 9\r\nend\r\n")));
    CHECK(xrtStrBufAppendFormat(body,"export func compute(int value) as int\r\n string literal=\"a\\0😀\"\r\n return work_0(value)+%u\r\nend\r\nfunc unused(int argument_%u=2) as int\r\n return argument_%u\r\nend\r\n",version<2 ? 0 : 1,version>=6 ? 2 : version>=1 ? 1 : 0,version>=6 ? 2 : version>=1 ? 1 : 0));
    return 1;
}
typedef struct cm_reader {xls_semantic_check* result;const xls_analysis_document* docs;int passed;} cm_reader;
static int32 cmRead(void* user)
{
    cm_reader* reader=user;
    reader->passed=cmPooled(xlsSemanticCheckAst(reader->result)) && fresh(reader->result,reader->docs,2);
    xlsSemanticCheckDestroy(reader->result);return 0;
}
static int cmSequence(size_t budget)
{
    uint64 startLive=cmLive();
    char *entry=xrtPathAbs("materialize-main.xl"),*dep=xrtPathAbs("materialize-dep.xl");CHECK(entry && dep);
    const char* main="import \"materialize-dep.xl\" as dep\r\nfunc main(...) as int\r\n return dep.compute(7)\r\nend\r\n";
    xls_analysis_document docs[2]={{"entry",entry,xlsTextCreate(main,strlen(main),4u*1024u*1024u,NULL),1,1},{"dep",dep,NULL,1,1}};CHECK(docs[0].text);
    xls_check_session* session=xlsCheckSessionCreate(NULL);xls_module_store* store=xlsModuleStoreCreate(NULL);CHECK(session && store);xls_module_scope bank;xlsModuleScopeBegin(&bank,store);xls_semantic_check* held=NULL;
    xls_ast_materialize_work previousWork={0};
    for(unsigned version=0;version<8;++version){
        xstrbuf body={0};CHECK(cmBody(&body,version));xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(body.Data,body.Size,4u*1024u*1024u,NULL);CHECK(docs[1].text);++docs[1].revision;
        xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,budget);
        xls_semantic_check* result=xlsCheckSessionCheck(session,docs,2,"entry",1,NULL,NULL,NULL,0);
        xls_ast_materialize_work outer;xlsAstMaterializeEnd(&scope,&outer);
        CHECK(result && xlsSemanticCheckInfo(result).accepted==(version!=3));
        CHECK(fresh(result,docs,2));xls_ast_materialize_work work=xlsSemanticCheckMaterializationWork(result);
        CHECK(work.calls && work.calls==work.pooledCopies+work.heapCopies && work.chargedBytes<=budget);
        if(version==7)CHECK(!outer.calls && !outer.chargedBytes && !memcmp(&work,&previousWork,sizeof(work)));
        else CHECK(outer.calls==work.calls && outer.chargedBytes==work.chargedBytes);
        previousWork=work;
        size_t pooled=cmPooled(xlsSemanticCheckAst(result));CHECK(pooled<=work.nodes);
        if(budget==131072)printf("partial budget version=%u copies=%llu/%llu fallback=%llu nodes=%llu live=%zu bytes=%llu AST=%zu\n",version,(unsigned long long)work.pooledCopies,(unsigned long long)work.heapCopies,(unsigned long long)work.poolFallbacks,(unsigned long long)work.nodes,pooled,(unsigned long long)work.chargedBytes,sizeof(XL6_AstNode));
        if(!budget)CHECK(!pooled && !work.nodes && work.heapCopies==work.calls && !work.poolFallbacks);
        if(budget==1)CHECK(!pooled && !work.nodes && work.poolFallbacks==work.calls && work.heapCopies==work.calls);
        if(budget==131072 && (version==1 || version==2 || version==5))CHECK(pooled && work.pooledCopies && work.heapCopies && work.poolFallbacks);
        if(budget==64u*1024u*1024u)CHECK(pooled && work.pooledCopies==work.calls && !work.heapCopies && !work.poolFallbacks);
        xls_semantic_input inputs[2]={{entry,xlsTextData(docs[0].text),xlsTextSize(docs[0].text)},{dep,body.Data,body.Size}};
        xls_ast_materialize_scope freshScope;xlsAstMaterializeBegin(&freshScope,64u*1024u*1024u);
        xls_semantic_check* reference=xlsSemanticCheckCreate(entry,inputs,2,1,NULL,NULL,NULL,0);xlsAstMaterializeEnd(&freshScope,NULL);
        CHECK(reference && !cmPooled(xlsSemanticCheckAst(reference)) && !xlsSemanticCheckMaterializationWork(reference).nodes);xlsSemanticCheckDestroy(reference);
        if(version==7)held=xlsSemanticCheckRetain(result);
        xlsSemanticCheckDestroy(result);xrtStrBufFree(&body);
    }
    xlsModuleScopeEnd(&bank);xlsModuleStoreRelease(store);xlsCheckSessionDestroy(session);
    CHECK(held && fresh(held,docs,2));
    if(budget==64u*1024u*1024u){cm_reader readers[4];xthread* threads[4];
        for(unsigned i=0;i<4;++i){readers[i]=(cm_reader){xlsSemanticCheckRetain(held),docs,0};threads[i]=xrtThreadCreate(cmRead,&readers[i],0);CHECK(threads[i]);}
        for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
    }
    xlsSemanticCheckDestroy(held);xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xrtFree(entry);xrtFree(dep);
    printf("check materialization budget=%zu live before=%llu after=%llu\n",budget,(unsigned long long)startLive,(unsigned long long)cmLive());return 1;
}
typedef struct cm_cancel {unsigned calls,at;} cm_cancel;
static int cmCancelled(void* user)
{cm_cancel* state=user;return ++state->calls>=state->at;}
static int cmCancellation(void)
{
    char* path=xrtPathAbs("materialize-cancel.xl");CHECK(path);xstrbuf body={0};CHECK(cmBody(&body,0));
    for(unsigned i=0;i<4;++i){xls_text* text=xlsTextCreate(body.Data,body.Size,4u*1024u*1024u,NULL);CHECK(text);xls_analysis_document doc={"entry",path,text,1,1};xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);
        cm_cancel state={0,(unsigned[]){1,8,64,256}[i]};uint64 before=cmLive();
        xls_semantic_check* cancelled=xlsCheckSessionCheck(session,&doc,1,"entry",1,cmCancelled,&state,NULL,0);CHECK(!cancelled && state.calls>=state.at);
        xlsCheckSessionDestroy(session);xlsTextRelease(text);CHECK(cmLive()<=before);
        CHECK(xlsAstMaterializeBudget()==64u*1024u*1024u);
    }
    xrtStrBufFree(&body);xrtFree(path);return 1;
}
static int cmParserPrefixes(void)
{
    const char* corpus[]={
        "func value(int input=2) as int\n int local=input+1\n if local>0 then\n return local\n else\n return 0\n end\nend\n",
        "func identity<T>(T value) as T\n return value\nend\n",
        "class Box\n int value\n func get() as int\n return this.value\n end\nend\n",
        "func values(int count) as generator<string>\n string owned=\"owned\"\n for i in range(count)\n yield owned+string(i)\n end\nend\n",
        "func closure() as func() as int\n array values=[41]\n return func() as int\n return int(values[0])\n end\nend\n",
        "func guard() as int\n try\n string item=\"owned\"\n return 1\n catch e\n return 2\n finally\n string after=\"after\"\n end\nend\n",
        "func text(int 值) as int\n string 文本=f\"😀={值}\"\n bytes raw=b\"a\\0b\"\n return 值\nend\n",
        "import dep as alias\nfrom dep import compute as run\nfunc main(...) as int\n return run(7)\nend\n",
        "#doc\ndesc:\n docs😀\n#end\nexport func main(...) as int\n return 7\nend\n"
    };
    size_t prefixes=0;
    for(size_t i=0;i<sizeof(corpus)/sizeof(*corpus);++i){size_t size=strlen(corpus[i]);
        for(size_t cut=0;cut<=size;++cut){if(cut<size && ((unsigned char)corpus[i][cut]&0xc0)==0x80)continue;
            char* source=xrtStrDupN(corpus[i],cut);CHECK(source);uint64 before=cmLive();XL6_ParseResult result;xl6ParseResultInit(&result);
            int accepted=xl6ParseSource(source,"parser-prefix.xl",&result);
            if(cut==size)CHECK(accepted && result.root && !result.diags.count);
            xl6ParseResultFree(&result);
            if(cmLive()!=before)fprintf(stderr,"prefix leak corpus=%zu cut=%zu delta=%lld\n",i,cut,(long long)(cmLive()-before));
            CHECK(cmLive()==before);xrtFree(source);++prefixes;
        }
    }
    printf("Original parser UTF-8 boundary prefixes=%zu;each accepted/error/unfinished stack balanced\n",prefixes);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);xrtMemStatsEnable(true);
    if(!cmSequence(0))return 1;
    uint64 before=cmLive();
    for(unsigned i=0;i<2;++i)if(!cmSequence(0) || !cmSequence(1) || !cmSequence(131072) || !cmSequence(64u*1024u*1024u))return 1;
    if(!cmCancellation() || !cmParserPrefixes())return 1;
    CHECK(cmLive()==before);
    printf("analysis_check_materialize_test passed: %u checks;real imported/header/body/global/repair checks,zero/tiny/partial/full pool budgets,fresh AST/index/report/IR parity,retained concurrent readers,cancellation and balanced live storage.\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
