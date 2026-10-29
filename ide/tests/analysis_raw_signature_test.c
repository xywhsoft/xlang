#include "analysis/signature_internal.h"
#include "analysis/input_batch.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_sem.h"
#include "analysis/frontend_entry_cache.h"
#include <stdio.h>
#include <string.h>
#ifndef XLS_RAW_SIGNATURE_HELPERS
#define XLS_RAW_SIGNATURE_HELPERS 32u
#endif
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"raw signature:%d: %s\n",__LINE__,#v);return 0;}}while(0)
#include "analysis_ast_same.h"
static int parity(const xls_signature_help* a,const xls_signature_help* b)
{
    xls_signature_info x=xlsSignatureInfo(a),y=xlsSignatureInfo(b);
    CHECK(sameText(x.path,y.path) && sameText(x.error,y.error) && x.offset==y.offset && x.available==y.available &&
        x.count==y.count && x.activeSignature==y.activeSignature && x.argument==y.argument && x.projected==y.projected);
    for(size_t i=0;i<x.count;++i){xls_signature_item p,q;CHECK(xlsSignatureItem(a,i,&p) && xlsSignatureItem(b,i,&q));
        CHECK(sameText(p.label,q.label) && sameText(p.documentation,q.documentation) && p.parameters==q.parameters && p.activeParameter==q.activeParameter && p.symbol==q.symbol);
        for(size_t j=0;j<p.parameters;++j){xls_signature_parameter m,n;CHECK(xlsSignatureParameter(a,i,j,&m) && xlsSignatureParameter(b,i,j,&n));
            CHECK(sameText(m.name,n.name) && m.labelStart==n.labelStart && m.labelEnd==n.labelEnd && m.optional==n.optional && m.variadic==n.variadic);}
    }
    return 1;
}
static int buildSource(xstrbuf* source,unsigned generation,int closed)
{
    if(!xrtStrBufAppend(source,xrtStrView("\xef\xbb\xbf// 中文😀\r\n")))return 0;
    for(unsigned i=0;i<XLS_RAW_SIGNATURE_HELPERS;++i){
        if(!xrtStrBufAppendFormat(source,"func work_%u(int value) as int\r\n int local=value+%u\r\n if local>0 then\r\n",i,i))return 0;
        for(unsigned j=0;j<24;++j)if(!xrtStrBufAppend(source,xrtStrView(" local=local+1\r\n")))return 0;
        if(!xrtStrBufAppend(source,xrtStrView(" end\r\n return local\r\nend\r\n")))return 0;
    }
    if(!xrtStrBufAppend(source,xrtStrView("func combine(int left,int right=1) as int\r\n return left+right\r\nend\r\nfunc combine(string left,string right) as string\r\n return left+right\r\nend\r\nfunc main(...) as int\r\n int seed=7\r\n")))return 0;
    return (!generation || xrtStrBufAppendFormat(source," int inserted=seed+%u\r\n",generation)) &&
        xrtStrBufAppend(source,xrtStrView(closed ? " return combine(seed, 1)\r\nend\r\n" : " return combine(seed, \r\nend\r\n"));
}
static int verify(int configured,int closed,int budgets)
{
    char* path=xrtPathAbs("raw-signature.xl");CHECK(path);char* entry=xrtStrDup(path);CHECK(entry);
    for(char* p=entry;*p;++p)if(*p=='\\')*p='/';
    char* directory=xrtPathParent(entry);CHECK(directory);
    xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=directory,.analysisProfile="raw-signature-proof",.targetKind="exe",.packagePath=""};
    xls_context* context=configured ? xlsContextCreate(&spec,1024u*1024u,NULL) : NULL;CHECK(!configured || context);
    xls_entry_cache *cache=xlsEntryCacheCreate(32u*1024u*1024u),*fullCache=xlsEntryCacheCreate(32u*1024u*1024u);CHECK(cache && fullCache);
    xls_input_batch* previousBatch=NULL;xls_signature_help *previous=NULL,*held=NULL;
    for(unsigned step=0;step<(budgets ? 8u : 3u);++step){xstrbuf source={0};CHECK(buildSource(&source,step,closed));
        const char* mark=strstr(source.Data,"return combine(seed,");CHECK(mark);size_t at=(size_t)(mark-source.Data)+strlen("return combine(seed,")-3;
        xls_text* text=xlsTextCreate(source.Data,source.Size,8u*1024u*1024u,NULL);CHECK(text);
        xls_analysis_document doc={"query",configured ? entry : path,text,51,step+1};
        xls_input_batch* batch=xlsInputBatchCreateInContext(context,NULL,previousBatch,&doc,1,configured ? NULL : "query",NULL,NULL,NULL,0);CHECK(batch);
        unsigned mode=budgets ? step%4 : 0;
        xls_sem_body_control bodies;xlsFrontendSemBodyBegin(&bodies,mode==1 ? 0 : SIZE_MAX);
        xls_sem_tail_control tails;xlsFrontendSemTailBegin(&tails,mode==2 ? 0 : SIZE_MAX);
        xls_sem_storage_control storage;xlsFrontendSemStorageBegin(&storage,mode==3 ? 4096 : 64u*1024u*1024u);
        char error[256]={0};xls_signature_help* current=xlsSignatureCreateBatch(cache,batch,"query",at,NULL,previous,NULL,NULL,error,sizeof(error));
        xlsFrontendSemStorageEnd(&storage,NULL);xlsFrontendSemTailEnd(&tails);xlsFrontendSemBodyEnd(&bodies);
        if(!current)fprintf(stderr,"signature creation: %s\n",error);
        CHECK(current && xlsSignatureInfo(current).available);
        xls_semantic_completion* semantic=xlsSignatureSemantic(current);xls_raw_native_work raw=xlsSemanticCompletionRawBodyWork(semantic);
        xls_sem_work sem=xlsSemanticCompletionSemWork(semantic);xls_ast_materialize_work copies=xlsSemanticCompletionMaterializationWork(semantic);
        xls_semantic_input input={path,source.Data,source.Size};
        xls_signature_help* fresh=xlsSignatureCreateInContext(context,xlsInputBatchEntry(batch),path,at,&input,1,NULL,NULL,error,sizeof(error));CHECK(fresh && parity(current,fresh));
        if(!step){CHECK(!raw.deferred && raw.registered>=XLS_RAW_SIGNATURE_HELPERS && raw.memoBytes);}
        else{
            if(!budgets || raw.deferred)CHECK(raw.deferred>=XLS_RAW_SIGNATURE_HELPERS && raw.restored==raw.deferred && !raw.retries && !raw.fullMaterialized && sem.proofHits>=XLS_RAW_SIGNATURE_HELPERS);
            if(mode==1 || mode==2)CHECK(!raw.deferred && !raw.retries);
            xls_sem_tail_control control;xlsFrontendSemTailBegin(&control,0);
            xls_signature_help* eager=xlsSignatureCreateBatch(fullCache,batch,"query",at,NULL,previous,NULL,NULL,error,sizeof(error));xlsFrontendSemTailEnd(&control);
            CHECK(eager && parity(current,eager) && !xlsSemanticCompletionRawBodyWork(xlsSignatureSemantic(eager)).deferred);
            xls_ast_materialize_work full=xlsSemanticCompletionMaterializationWork(xlsSignatureSemantic(eager));CHECK(full.nodes);
            if(raw.deferred)CHECK(copies.nodes<full.nodes/2);
            fprintf(stderr,"configured=%d closed=%d generation=%u raw=%llu proof=%llu copies=%llu eager=%llu\n",configured,closed,step,(unsigned long long)raw.deferred,(unsigned long long)sem.proofHits,(unsigned long long)copies.nodes,(unsigned long long)full.nodes);
            CHECK(sameAst(xlsSemanticCompletionAst(semantic),xlsSemanticCompletionAst(xlsSignatureSemantic(eager))));xlsSignatureRelease(eager);
            if(!held)held=xlsSignatureRetain(current);
        }
        CHECK(sameAst(xlsSemanticCompletionAst(semantic),xlsSemanticCompletionAst(xlsSignatureSemantic(fresh))));
        if(raw.deferred)CHECK(xlsSemanticCompletionRawBodyWork(semantic).fullMaterialized && !xlsSemanticCompletionRawBodyWork(semantic).astFailures);
        CHECK(parity(current,fresh));xlsSignatureRelease(fresh);xlsSignatureRelease(previous);xlsInputBatchDestroy(previousBatch);
        previous=current;previousBatch=batch;xlsTextRelease(text);xrtStrBufFree(&source);
    }
    xlsSignatureRelease(previous);xlsInputBatchDestroy(previousBatch);xlsEntryCacheDestroy(cache);xlsEntryCacheDestroy(fullCache);
    CHECK(held && xlsSignatureValidate(held) && xlsSignatureInfo(held).available && xlsSemanticCompletionAst(xlsSignatureSemantic(held)));xlsSignatureRelease(held);
    xlsContextRelease(context);xrtFree(directory);xrtFree(entry);xrtFree(path);return 1;
}
int main(void)
{
    for(int budgets=0;budgets<2;++budgets)for(int configured=0;configured<2;++configured)for(int closed=0;closed<2;++closed)if(!verify(configured,closed,budgets))return 1;
    printf("analysis_raw_signature_test passed: %u checks;helpers=%u;original full signatures and complete AST parity\n",checks,(unsigned)XLS_RAW_SIGNATURE_HELPERS);return 0;
}
