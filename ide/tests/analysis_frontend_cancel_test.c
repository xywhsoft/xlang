#include "analysis/frontend_cancel.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_ir.h"
#include "analysis/check.h"
#include "analysis/signature.h"
#include <stdio.h>
#include <string.h>

static xatomic32 checks;
#define CHECK(value) do { xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED); if(!(value)){fprintf(stderr,"analysis_frontend_cancel_test:%d: %s\n",__LINE__,#value);return 0;} } while(0)
typedef struct trigger {
    enum xls_frontend_cancel_kind kind;
    uint64_t after,calls,observed;
    int fired,once;
    xls_frontend_cancel_work last;
} trigger;
static int predicate(void* user)
{
    trigger* t=user;++t->calls;t->last=xlsFrontendCancelWork();
    if(t->last.kind!=t->kind || t->last.points[t->kind]<t->after)return 0;
    if(t->fired && t->once)return 0;
    t->fired=1;t->observed=t->last.points[t->kind];return 1;
}
static int never(void* user) {++*(unsigned*)user;return 0;}
static int scopes(void)
{
    CHECK(!xlsActiveFrontendCancel && !xlsFrontendCancelObserved());
    xls_frontend_cancel_scope a,b;xls_frontend_cancel_work work;
    xlsFrontendCancelBegin(&a,NULL,NULL);CHECK(!xlsActiveFrontendCancel);
    CHECK(!xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,1));xlsFrontendCancelEnd(&a,&work);CHECK(!work.predicateCalls);
    trigger t={.kind=XLS_CANCEL_IR_NODE,.after=64,.once=1};xlsFrontendCancelBegin(&a,predicate,&t);
    for(unsigned i=0;i<63;++i)CHECK(!xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,0));
    CHECK(!t.calls && xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,0) && t.calls==1 && t.fired);
    CHECK(xlsFrontendCancelPoint(XLS_CANCEL_SEM_FUNCTION,1) && t.calls==1);
    xlsFrontendCancelBegin(&b,NULL,NULL);CHECK(xlsFrontendCancelObserved());xlsFrontendCancelEnd(&b,NULL);
    CHECK(xlsFrontendCancelObserved());xlsFrontendCancelEnd(&a,&work);
    CHECK(work.cancelled && work.points[XLS_CANCEL_IR_NODE]==64 && !xlsActiveFrontendCancel);
    unsigned calls=0;xlsFrontendCancelBegin(&a,never,&calls);t=(trigger){.kind=XLS_CANCEL_SEM_FUNCTION,.after=1};
    xlsFrontendCancelBegin(&b,predicate,&t);CHECK(xlsFrontendCancelPoint(XLS_CANCEL_SEM_FUNCTION,1));
    xlsFrontendCancelEnd(&b,&work);CHECK(work.cancelled && !xlsFrontendCancelObserved());
    CHECK(!xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1) && calls==1);xlsFrontendCancelEnd(&a,NULL);
    return 1;
}
static char* source(unsigned functions)
{
    xstrbuf b;xrtStrBufInit(&b);int okay=xrtStrBufAppend(&b,xrtStrView("\xef\xbb\xbf// 中文😀\r\nfunc identity(int value) as int\r\n return value\r\nend\r\n"));
    for(unsigned i=0;okay && i<functions;++i){
        okay=xrtStrBufAppendFormat(&b,"func work_%u(int value) as int\r\n int local = value+%u\r\n if local > 0 then\r\n",i,i);
        for(unsigned j=0;okay && j<24;++j)okay=xrtStrBufAppend(&b,xrtStrView(" local = identity(local)+1\r\n"));
        okay=okay && xrtStrBufAppend(&b,xrtStrView(" end\r\n return local\r\nend\r\n"));
    }
    okay=okay && xrtStrBufAppend(&b,xrtStrView("func choose(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int doneLocal=choose(7,\"hello\")\r\n return doneLocal\r\nend\r\n"));
    char* result=okay ? xrtStrDup(b.Data) : NULL;xrtStrBufFree(&b);return result;
}
static char* irJson(const XL6_IrModule* ir)
{
    XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,ir);
    char* value=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return value;
}
static int ordinary(const char* path,const char* body)
{
    xls_semantic_input input={path,body,strlen(body)};char error[256];
    xls_semantic_check* baseline=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    CHECK(baseline && xlsSemanticCheckInfo(baseline).accepted);
    char *json=xlsSemanticCheckJson(baseline,NULL),*ir=irJson(xlsSemanticCheckIr(baseline));CHECK(json && ir);
    size_t nodes=xlsSemanticCheckIr(baseline)->nodeCount;
    for(unsigned kind=0;kind<XLS_CANCEL_KIND_COUNT;++kind){
        uint64_t at=kind==XLS_CANCEL_SEM_FUNCTION ? 2 : kind==XLS_CANCEL_IR_PHASE ? 12 : 65;
        if(kind==XLS_CANCEL_SEM_MODULE)at=2;
        for(unsigned once=0;once<2;++once){
            trigger t={.kind=(enum xls_frontend_cancel_kind)kind,.after=at,.once=(int)once};
            xls_semantic_check* cancelled=xlsSemanticCheckCreate(path,&input,1,1,predicate,&t,error,sizeof(error));
            if(cancelled || !t.fired)fprintf(stderr,"cancel kind=%u at=%llu observed=%llu error=%s\n",kind,(unsigned long long)at,(unsigned long long)t.observed,error);
            CHECK(!cancelled && t.fired && strstr(error,"cancelled") && !xlsActiveFrontendCancel);
            CHECK(!xlsFrontendIrInitializerActive());
            CHECK(xlsSemanticCheckValidate(baseline));
            unsigned calls=0;xls_semantic_check* retry=xlsSemanticCheckCreate(path,&input,1,1,never,&calls,error,sizeof(error));
            CHECK(retry && xlsSemanticCheckInfo(retry).accepted && calls);
            char *a=xlsSemanticCheckJson(retry,NULL),*b=irJson(xlsSemanticCheckIr(retry));
            CHECK(a && b && !strcmp(a,json) && !strcmp(b,ir));xrtFree(a);xrtFree(b);xlsSemanticCheckDestroy(retry);
        }
    }
    /* Four pre-arena loops precede the owned incoming-count arena. Cancel
     * inside each of its two loops and after its ordinary release as well. */
    for(unsigned loop=4;loop<7;++loop){
        trigger t={.kind=XLS_CANCEL_IR_VERIFY,.after=loop*nodes+65,.once=1};
        xls_semantic_check* cancelled=xlsSemanticCheckCreate(path,&input,1,1,predicate,&t,error,sizeof(error));
        CHECK(!cancelled && t.fired && strstr(error,"cancelled") && !xlsActiveFrontendCancel);
        CHECK(!xlsFrontendIrInitializerActive());
    }
    /* The statistics loop is after the 27 IR transformation phases and
       before verification. Its cancellation must pop the borrowed table
       scope before freeing the module or returning to a later request. */
    trigger statistics={.kind=XLS_CANCEL_IR_PHASE,.after=32,.once=1};
    xls_semantic_check* interrupted=xlsSemanticCheckCreate(path,&input,1,1,predicate,&statistics,error,sizeof(error));
    CHECK(!interrupted && statistics.fired && strstr(error,"cancelled") && statistics.observed>=32);
    CHECK(!xlsFrontendIrInitializerActive() && !xlsActiveFrontendCancel);
    char* held=irJson(xlsSemanticCheckIr(baseline));CHECK(held && !strcmp(held,ir));xrtFree(held);
    xlsSemanticCheckDestroy(baseline);xrtFree(json);xrtFree(ir);return 1;
}
static int cursor(const char* path,const char* body)
{
    xls_semantic_input input={path,body,strlen(body)};char error[256];size_t bom=3;
    size_t completion=(size_t)(strstr(body,"return doneLocal")-body)+strlen("return done")-bom;
    size_t signature=(size_t)(strstr(body,"\"hello\"")-body)+2-bom;
    xls_semantic_completion* baseline=xlsSemanticCompletionCreate(path,path,completion,&input,1,NULL,NULL,error,sizeof(error));
    xls_signature_help* sig=xlsSignatureCreateInContext(NULL,path,path,signature,&input,1,NULL,NULL,error,sizeof(error));
    CHECK(baseline && xlsSemanticCompletionInfo(baseline).available && sig && xlsSignatureInfo(sig).available);
    char* expected=xlsSemanticCompletionJson(baseline,NULL);CHECK(expected);
    for(unsigned method=0;method<2;++method)for(unsigned kind=0;kind<=XLS_CANCEL_SEM_EXPRESSION;++kind){
        trigger t={.kind=(enum xls_frontend_cancel_kind)kind,.after=kind==XLS_CANCEL_SEM_MODULE || kind==XLS_CANCEL_SEM_FUNCTION ? 2u : 65u,.once=1};
        if(method){xls_signature_help* r=xlsSignatureCreateInContext(NULL,path,path,signature,&input,1,predicate,&t,error,sizeof(error));CHECK(!r && t.fired && strstr(error,"cancelled"));}
        else{xls_semantic_completion* r=xlsSemanticCompletionCreate(path,path,completion,&input,1,predicate,&t,error,sizeof(error));CHECK(!r && t.fired && strstr(error,"cancelled"));}
        CHECK(!xlsActiveFrontendCancel && xlsSemanticCompletionValidate(baseline) && xlsSignatureValidate(sig));
    }
    unsigned calls=0;xls_semantic_completion* retry=xlsSemanticCompletionCreate(path,path,completion,&input,1,never,&calls,error,sizeof(error));
    char* actual=retry ? xlsSemanticCompletionJson(retry,NULL) : NULL;CHECK(actual && !strcmp(actual,expected));
    xrtFree(actual);xrtFree(expected);xlsSemanticCompletionDestroy(retry);xlsSemanticCompletionDestroy(baseline);xlsSignatureRelease(sig);return 1;
}
typedef struct reader {const char *path,*body;int okay;} reader;
static int32 runReader(void* user)
{
    reader* r=user;char error[256];xls_semantic_input input={r->path,r->body,strlen(r->body)};
    trigger t={.kind=XLS_CANCEL_SEM_FUNCTION,.after=3,.once=1};
    xls_semantic_check* cancelled=xlsSemanticCheckCreate(r->path,&input,1,0,predicate,&t,error,sizeof(error));
    r->okay=!cancelled && t.fired && strstr(error,"cancelled") && !xlsActiveFrontendCancel;
    xlsSemanticCheckDestroy(cancelled);
    xls_semantic_check* retry=xlsSemanticCheckCreate(r->path,&input,1,0,NULL,NULL,error,sizeof(error));
    r->okay=r->okay && retry && xlsSemanticCheckInfo(retry).accepted && !xlsFrontendIrInitializerActive();xlsSemanticCheckDestroy(retry);return 0;
}
static int all(void)
{
    if(!scopes())return 0;
    char* path=xrtPathAbs("frontend-cancel-中文.xl");char* body=source(8);CHECK(path && body);
    if(!ordinary(path,body) || !cursor(path,body))return 0;
    reader readers[4];xthread* threads[4];
    for(unsigned i=0;i<4;++i){readers[i]=(reader){path,body,0};threads[i]=xrtThreadCreate(runReader,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    CHECK(!xlsActiveFrontendCancel && !xlsFrontendIrInitializerActive());xrtFree(path);xrtFree(body);
    return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!all())return 1;
    printf("analysis_frontend_cancel_test passed: %u checks;real semantic/IR interruption,one-shot latching,owned verifier cleanup,fresh complete JSON/IR parity,held results,nested scopes and4threads.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
