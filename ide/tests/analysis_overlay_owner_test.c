#include "analysis/frontend_overlay.h"
#include "analysis/check_session.h"
#include "analysis/completion_session.h"
#include "analysis/dependency_session.h"
#include "analysis/signature_session.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do {++checks;if(!(v)){fprintf(stderr,"analysis_overlay_owner_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
/* Allocation refusal in the new adapter only. Original Find/loader and the
 * normal product object remain untouched; count every owned-path boundary. */
static unsigned allocations,refuse;
static char* ownedPath(const char* path)
{return ++allocations==refuse ? NULL : xrtPathAbs(path);}
#define xrtPathAbs ownedPath
#define xlsFrontendOverlayBorrow fixtureBorrow
#define xlsFrontendOverlayClear fixtureClear
#include "analysis/frontend_overlay.c"
#undef xlsFrontendOverlayClear
#undef xlsFrontendOverlayBorrow
#undef xrtPathAbs
typedef struct cancel_state {unsigned calls,at;} cancel_state;
static int cancelAt(void* user){cancel_state* c=user;return ++c->calls==c->at;}
static xls_text* text(const char* s){return xlsTextCreate(s,strlen(s),8u*1024u*1024u,NULL);}

static int adapter(void)
{
    char *paths[3]={xrtPathAbs("overlay-owner-a.xl"),xrtPathAbs("overlay-owner-b.xl"),xrtPathAbs("overlay-owner-c.xl")};
    const char* values[]={"\xef\xbb\xbf" "// 中文 😀\r\n","","// retained\n"};
    xls_analysis_document docs[3];
    for(size_t i=0;i<3;++i){CHECK(paths[i]);docs[i]=(xls_analysis_document){values[i][0] ? values[i] : "empty",paths[i],text(values[i]),i+1,1};CHECK(docs[i].text);}
    xls_input_batch* batch=xlsInputBatchCreate(NULL,NULL,docs,3,docs[0].key,NULL,NULL,NULL,0);CHECK(batch);
    XL6_SourceOverlay overlay={0};CHECK(xlsFrontendOverlayBorrow(&overlay,batch,NULL,NULL)==1 && overlay.count==3);
    size_t bytes=0;
    for(size_t i=0;i<3;++i){size_t bom=i ? 0 : 3;const XL6_SourceOverlayItem* row=NULL;CHECK(xlsFrontendOverlayLookup(&overlay,paths[i],&row)==1 && row && row->text==xlsTextData(docs[i].text)+bom && row->size==strlen(values[i])-bom);CHECK(!strcmp(row->path,paths[i]) && !memcmp(row->text,values[i]+bom,row->size+1));bytes+=row->size;}
    CHECK(overlay.bytes==bytes && !xlsFrontendOverlayBorrow(&overlay,batch,NULL,NULL));
    xlsFrontendOverlayClear(&overlay,batch);CHECK(!overlay.count && !overlay.bytes);
    for(size_t i=0;i<3;++i)CHECK(!strcmp(xlsTextData(docs[i].text),values[i]));
    CHECK(xl6SourceOverlaySet(&overlay,paths[0],values[0]+3,strlen(values[0])-3));
    CHECK(overlay.items[0].text!=xlsTextData(docs[0].text)+3);xlsFrontendOverlayClear(&overlay,NULL);
    for(unsigned at=1;at<=3;++at){
        cancel_state c={0,at};CHECK(xlsFrontendOverlayBorrow(&overlay,batch,cancelAt,&c)==-1 && c.calls==at && overlay.count==at-1);
        xlsFrontendOverlayClear(&overlay,batch);CHECK(!overlay.count);
        allocations=0;refuse=at;CHECK(!fixtureBorrow(&overlay,batch,NULL,NULL) && allocations==at && overlay.count==at-1);
        fixtureClear(&overlay,batch);CHECK(!overlay.count);
        for(size_t i=0;i<3;++i)CHECK(!strcmp(xlsTextData(docs[i].text),values[i]));
    }
    allocations=refuse=0;CHECK(fixtureBorrow(&overlay,batch,NULL,NULL));fixtureClear(&overlay,batch);
    /* Same lexical file under two keys remains invalid, as in the original
     * complete checker; this is not an implicit last-overlay-wins map. */
    xls_analysis_document alias[2]={docs[0],docs[0]};alias[1].key="alias";
    xls_input_batch* duplicate=xlsInputBatchCreate(NULL,NULL,alias,2,alias[0].key,NULL,NULL,NULL,0);CHECK(duplicate);
    CHECK(!xlsFrontendOverlayBorrow(&overlay,duplicate,NULL,NULL) && overlay.count==1);xlsFrontendOverlayClear(&overlay,duplicate);xlsInputBatchDestroy(duplicate);
    xlsInputBatchDestroy(batch);
    for(size_t i=0;i<3;++i){CHECK(!strcmp(xlsTextData(docs[i].text),values[i]));xlsTextRelease(docs[i].text);xrtFree(paths[i]);}
    const char invalid[]={'a',0,'b'};CHECK(!xlsTextCreate(invalid,sizeof(invalid),64,NULL));CHECK(!xlsTextCreate("\xc0\x80",2,64,NULL));return 1;
}
static int limits(void)
{
    const size_t n=8u*1024u*1024u;char* bytes=xrtMalloc(n+1);CHECK(bytes);memset(bytes,'x',n);bytes[n]=0;
    xls_text* body=xlsTextCreate(bytes,n,n,NULL);CHECK(body);xrtFree(bytes);
    char *paths[5];xls_analysis_document docs[5];char keys[5][16];
    for(size_t i=0;i<5;++i){snprintf(keys[i],sizeof(keys[i]),"limit%zu.xl",i);paths[i]=xrtPathAbs(keys[i]);CHECK(paths[i]);docs[i]=(xls_analysis_document){keys[i],paths[i],body,i+1,1};}
    xls_input_batch* batch=xlsInputBatchCreate(NULL,NULL,docs,4,keys[0],NULL,NULL,NULL,0);CHECK(batch && xlsInputBatchBytes(batch)==32u*1024u*1024u);
    CHECK(!xlsInputBatchCreate(NULL,NULL,docs,5,keys[0],NULL,NULL,NULL,0));
    XL6_SourceOverlay overlay={0};CHECK(xlsFrontendOverlayBorrow(&overlay,batch,NULL,NULL) && overlay.count==4 && overlay.bytes==32u*1024u*1024u);
    for(size_t i=0;i<4;++i)CHECK(overlay.items[i].text==xlsTextData(body) && overlay.items[i].size==n);
    xlsFrontendOverlayClear(&overlay,batch);xlsInputBatchDestroy(batch);CHECK(xlsTextSize(body)==n && xlsTextData(body)[n-1]=='x');xlsTextRelease(body);
    for(size_t i=0;i<5;++i){xrtFree(paths[i]);}return 1;
}
static int equalString(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int equalSignature(const xls_signature_help* a,const xls_signature_help* b)
{
    xls_signature_info x=xlsSignatureInfo(a),y=xlsSignatureInfo(b);
    if(!equalString(x.path,y.path) || !equalString(x.error,y.error) || x.offset!=y.offset || x.count!=y.count ||
       x.activeSignature!=y.activeSignature || x.argument!=y.argument || x.available!=y.available || x.projected!=y.projected)return 0;
    for(size_t i=0;i<x.count;++i){xls_signature_item u,v;if(!xlsSignatureItem(a,i,&u) || !xlsSignatureItem(b,i,&v) ||
        !equalString(u.label,v.label) || !equalString(u.documentation,v.documentation) || u.parameters!=v.parameters || u.activeParameter!=v.activeParameter || u.symbol!=v.symbol)return 0;
        for(size_t j=0;j<u.parameters;++j){xls_signature_parameter p,q;if(!xlsSignatureParameter(a,i,j,&p) || !xlsSignatureParameter(b,i,j,&q) ||
            !equalString(p.name,q.name) || p.labelStart!=q.labelStart || p.labelEnd!=q.labelEnd || p.optional!=q.optional || p.variadic!=q.variadic)return 0;}
    }return 1;
}
typedef struct held {
    xls_semantic_check* check;xls_semantic_completion* completion;xls_signature_help* signature;xls_dependency_capture* dependency;
    const char *checkJson,*completionJson,*signatureJson;const xls_signature_help* signatureOracle;int okay;
} held;
static int32 readHeld(void* user)
{
    held* h=user;h->okay=1;
    for(unsigned i=0;i<8;++i){
        char *a=xlsSemanticCheckJson(h->check,NULL),*b=xlsSemanticCompletionJson(h->completion,NULL),*c=xlsSignatureInputJson(h->signature,NULL);
        if(!a || !b || !c || strcmp(a,h->checkJson) || strcmp(b,h->completionJson) || strcmp(c,h->signatureJson) ||
           !equalSignature(h->signature,h->signatureOracle) || !xlsSemanticCheckValidate(h->check) || !xlsSemanticCompletionValidate(h->completion) || !xlsSignatureValidate(h->signature) || !xlsDependencyCaptureValidate(h->dependency))h->okay=0;
        xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticStringFree(c);
    }
    xlsSemanticCheckDestroy(h->check);xlsSemanticCompletionDestroy(h->completion);xlsSignatureRelease(h->signature);xlsDependencyCaptureRelease(h->dependency);return 0;
}
static int lifetime(void)
{
    const char* source="\xef\xbb\xbf" "import overlay_owner_dep\r\nfunc local(int value) as int\r\n return overlay_owner_dep.value(value)\r\nend\r\nfunc main(...) as int\r\n return local(7)\r\nend\r\n";
    const char* dependency="export func value(int value) as int\n return value\nend\n";
    char *path=xrtPathAbs("overlay-owner-main.xl"),*dep=xrtPathAbs("overlay_owner_dep.xl"),*unused=xrtPathAbs("overlay-owner-unused.xl");CHECK(path && dep && unused);
    xls_analysis_document docs[3]={{"entry",path,text(source),1,1},{"dep",dep,text(dependency),2,1},{"unused",unused,text("// unrelated 中文\n"),3,1}};
    for(size_t i=0;i<3;++i)CHECK(docs[i].text);
    xls_semantic_input inputs[3];for(size_t i=0;i<3;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    size_t cursor=(size_t)(strstr(source,"return local(7)")-source)+strlen("return loc")-3,call=cursor+strlen("al(");
    xls_check_session* cs=xlsCheckSessionCreate(NULL);xls_completion_session* ms=xlsCompletionSessionCreate(NULL);xls_signature_session* ss=xlsSignatureSessionCreate(NULL);xls_dependency_session* ds=xlsDependencySessionCreate(NULL);CHECK(cs && ms && ss && ds);
    xls_semantic_check* c=xlsCheckSessionCheck(cs,docs,3,"entry",1,NULL,NULL,NULL,0);CHECK(c && xlsSemanticCheckInfo(c).accepted);
    xls_semantic_completion* m=xlsCompletionSessionQuery(ms,docs,3,"entry","entry",cursor,NULL,NULL,NULL,0);CHECK(m && xlsSemanticCompletionInfo(m).available);
    xls_signature_help* s=xlsSignatureSessionQueryInContext(ss,NULL,docs,3,"entry","entry",call,NULL,NULL,NULL,0);CHECK(s && xlsSignatureInfo(s).available);
    xls_dependency_capture* d=xlsDependencySessionCaptureInContext(ds,NULL,docs,3,"entry",NULL,NULL,NULL,0);CHECK(d && xlsDependencyCaptureInfo(d).expanded);
    xls_semantic_check* cf=xlsSemanticCheckCreate(path,inputs,3,1,NULL,NULL,NULL,0);xls_semantic_completion* mf=xlsSemanticCompletionCreate(path,path,cursor,inputs,3,NULL,NULL,NULL,0);xls_signature_help* sf=xlsSignatureCreateInContext(NULL,path,path,call,inputs,3,NULL,NULL,NULL,0);CHECK(cf && mf && sf);
    char *cj=xlsSemanticCheckJson(cf,NULL),*mj=xlsSemanticCompletionJson(mf,NULL),*sj=xlsSignatureInputJson(sf,NULL);CHECK(cj && mj && sj);
    xlsSemanticCheckDestroy(cf);xlsSemanticCompletionDestroy(mf);
    xlsCheckSessionDestroy(cs);xlsCompletionSessionDestroy(ms);xlsSignatureSessionDestroy(ss);xlsDependencySessionDestroy(ds);
    for(size_t i=0;i<3;++i){xlsTextRelease(docs[i].text);}xrtFree(path);xrtFree(dep);xrtFree(unused);
    held readers[4];xthread* threads[4];
    for(size_t i=0;i<4;++i){readers[i]=(held){xlsSemanticCheckRetain(c),xlsSemanticCompletionRetain(m),xlsSignatureRetain(s),xlsDependencyCaptureRetain(d),cj,mj,sj,sf,0};threads[i]=xrtThreadCreate(readHeld,&readers[i],0);CHECK(threads[i]);}
    for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    xlsSemanticStringFree(cj);xlsSemanticStringFree(mj);xlsSemanticStringFree(sj);xlsSemanticCheckDestroy(c);xlsSemanticCompletionDestroy(m);xlsSignatureRelease(s);xlsSignatureRelease(sf);xlsDependencyCaptureRelease(d);return 1;
}
int main(void)
{if(!adapter() || !limits() || !lifetime())return 1;printf("analysis_overlay_owner_test passed: %u checks; pointer ownership/BOM/UTF8/duplicates/32MiB/partial cancel/path allocation faults and four retained models after session/text destruction with fresh JSON parity on4threads\n",checks);return 0;}
