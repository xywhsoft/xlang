/* Actual generated checker, with test-only refusal of optional certificate
 * allocations. Production has no fault hook. This TU supplies the semantic
 * symbols, so the archive's ordinary semantic TU is not pulled in. */
#include "analysis/completion_session.h"
#include "analysis/signature_session.h"
#include "analysis/signature_internal.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static int proofTracking,denyProof,denyPush,denyInit;
static void* trackedProof;
static int proofAllocation(size_t count,size_t size);
static void* proofCalloc(size_t count,size_t size)
{
    int match=proofAllocation(count,size);
    if(match && denyProof){--denyProof;return NULL;}
    void* p=xrtCalloc(count,size);if(match)trackedProof=p;return p;
}
static void proofFree(void* p)
{if(p && p==trackedProof)trackedProof=NULL;xrtFree(p);}
static bool proofPush(xarray* a,const void* p)
{if(proofTracking && denyPush){--denyPush;return false;}return xrtArrayPush(a,p);}
static bool proofInit(xarray* a,size_t size)
{if(proofTracking && denyInit){--denyInit;return false;}return xrtArrayInit(a,size);}
#define xrtCalloc proofCalloc
#define xrtFree proofFree
#define xrtArrayPush proofPush
#define xrtArrayInit proofInit
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#include "xl6_sem.c"
#pragma GCC diagnostic pop
#undef xrtCalloc
#undef xrtFree
#undef xrtArrayPush
#undef xrtArrayInit
static int proofAllocation(size_t count,size_t size)
{return proofTracking && count==1 && size==sizeof(xls_sem_proof);}
/* Reuse the original complete logical/source/recursive AST comparator.
 * Its existing scenarios remain in their separately executed fixture. */
#define main xlsProofOriginalBodyMain
#include "analysis_completion_body_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_semantic_proof_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xls_sem_proof* trackedCreate(const xls_sem_unit* unit,size_t limit)
{proofTracking=1;xls_sem_proof* p=xlsSemProofCreate(unit,limit);proofTracking=0;return p;}
static int both(xls_sem_unit* unit,xarray* nodes,const xls_sem_moves* moves,const xmap* sources,
    XL6_AstNode* function,const xls_sem_cursor* cursor,int shifted,int wanted)
{
    unit->proof=trackedCreate(unit,4096);CHECK(unit->proof && trackedProof==unit->proof);
    xls_sem_entry old={.unit=unit,.lexicalRange=function->range};xarray a={0},b={0};uint64_t hit=0,slow=0;
    XL6_SymbolId end=0;CHECK(xlsSemSymbolsLimit(&unit->symbols,200,&end) && end==204);
    int fast=xlsSemBodyPrepare(&old,nodes,moves,sources,&a,function,cursor,200,shifted,&hit);
    xls_sem_proof_control zero;xlsFrontendSemProofBegin(&zero,0);
    int original=xlsSemBodyPrepare(&old,nodes,moves,sources,&b,function,cursor,200,shifted,&slow);
    xlsFrontendSemProofEnd(&zero);CHECK(fast==wanted && original==wanted && !slow);
    if(wanted)CHECK(a.Count==b.Count);
    for(size_t i=0;wanted && i<a.Count;++i){const xls_sem_position *x=xrtArrayConstGet(&a,i),*y=xrtArrayConstGet(&b,i);
        CHECK(sameRange(x->range,y->range) && sameRange(x->token,y->token) && x->line==y->line && x->column==y->column);}
    if(wanted && (!shifted || unit->proof->shiftSafe))CHECK(hit==unit->proof->nodes && hit>0);
    xrtArrayUnit(&a);xrtArrayUnit(&b);xlsSemProofDestroy(unit->proof);unit->proof=NULL;CHECK(!trackedProof);return 1;
}
static int admission(void)
{
    char* path=xrtPathAbs("semantic-proof.xl");CHECK(path);
    XL6_AstNode *type=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL),*function=xl6AstNew(XL6_AST_FUNC,NULL),*target=xl6AstNew(XL6_AST_MODULE,NULL);
    CHECK(type && function && target && xl6AstSetSourceTree(type,path) && xl6AstSetSourceTree(function,path) && xl6AstSetSourceTree(target,path));
    xls_sem_unit unit={.symbols={100,104,100,1}};CHECK(xrtArrayInit(&unit.facts,sizeof(xls_sem_fact)));
    type->semanticTargetSymbolId=103;xls_sem_fact fact={.symbol=101,.declared=50,.target=102,.inferred=type};CHECK(xrtArrayPush(&unit.facts,&fact));
    xarray nodes={0};CHECK(xrtArrayInit(&nodes,sizeof(xls_sem_raw)));xls_sem_raw raw={.node=function};CHECK(xrtArrayPush(&nodes,&raw));
    xls_sem_moves moves={0};xmap sources={0};CHECK(xrtMapInit(&moves.paths,sizeof(xls_sem_move)) && xrtMapInit(&sources,sizeof(XL6_AstNode*)));
    CHECK(xrtMapSet(&sources,(xbytesview){(const unsigned char*)path,strlen(path)},&target));xls_sem_cursor cursor={path,0,0};
    CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,1));
    xls_sem_fact* stored=xrtArrayGet(&unit.facts,0);
    XL6_SymbolId* fields[]={&type->semanticSymbolId,&type->semanticDeclaredSymbolId,&type->semanticTargetSymbolId,
        &type->semanticCompoundOperatorSymbolId,&stored->symbol,&stored->declared,&stored->target,&stored->compound};
    for(size_t i=0;i<8;++i)*fields[i]=0;
    unit.symbols=(xls_sem_symbol_span){120,124,100,1};
    XL6_SymbolId values[]={0,1,99,100,119,120,123,124,UINT32_MAX};
    for(size_t i=0;i<8;++i)for(size_t j=0;j<sizeof(values)/sizeof(*values);++j){
        XL6_SymbolId value=values[j];*fields[i]=value;
        CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,!value || value<100 || (value>=120 && value<124)));
        *fields[i]=0;
    }
    unit.symbols=(xls_sem_symbol_span){100,104,100,1};type->semanticTargetSymbolId=103;stored->symbol=101;stored->declared=50;stored->target=102;
    type->line=1;type->column=3;type->range=(XL6_AstRange){.known=1,.startOffset=2,.endOffset=5,.startLine=1,.startColumn=3,.endLine=1,.endColumn=6};
    CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,1));
    xls_sem_proof* p=trackedCreate(&unit,4096);CHECK(p && p->located.Count==1 && p->sources.Count==1 && p->nodes==1 && p->bytes<=4096);size_t exact=p->bytes;xlsSemProofDestroy(p);CHECK(!trackedProof);
    p=trackedCreate(&unit,exact);CHECK(p && p->bytes==exact);xlsSemProofDestroy(p);CHECK(!trackedProof && !trackedCreate(&unit,exact-1) && !trackedProof);
    xls_sem_proof_control outer,inner;xlsFrontendSemProofBegin(&outer,0);xlsFrontendSemProofBegin(&inner,4096);CHECK(!trackedCreate(&unit,4096));xlsFrontendSemProofEnd(&inner);xlsFrontendSemProofEnd(&outer);
    denyProof=1;CHECK(!trackedCreate(&unit,4096) && !denyProof && !trackedProof);
    denyPush=1;CHECK(!trackedCreate(&unit,4096) && !denyPush && !trackedProof);
    denyInit=1;CHECK(!trackedCreate(&unit,4096) && !denyInit && !trackedProof);
    type->semanticTargetSymbolId=150;CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,0));CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,0,1));type->semanticTargetSymbolId=103;
    ((xls_sem_fact*)xrtArrayGet(&unit.facts,0))->compound=150;CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,0));((xls_sem_fact*)xrtArrayGet(&unit.facts,0))->compound=0;
    XL6_SymbolId end=0;CHECK(!xlsSemSymbolsLimit(&unit.symbols,UINT32_MAX-2,&end));
    XL6_AstSource* saved=target->source;target->source=type->source;CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,0));target->source=saved;
    size_t references=target->source->references;target->source->references=0;CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,0));target->source->references=references;
    target->source->completionQuery=1;CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,0));target->source->completionQuery=0;
    target->source->completionStart=1;CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,0));target->source->completionStart=0;
    type->source->completionQuery=target->source->completionQuery=1;type->source->completionStart=10;type->source->completionEnd=20;
    target->source->completionStart=30;target->source->completionEnd=40;unit.cursor=(xls_sem_cursor){path,10,20};cursor=(xls_sem_cursor){path,30,40};
    CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,1));++unit.cursor.start;CHECK(both(&unit,&nodes,&moves,&sources,function,&cursor,1,0));
    unit.cursor=(xls_sem_cursor){0};type->source->completionQuery=0;type->source->completionStart=type->source->completionEnd=0;
    type->childCount=1;CHECK(!trackedCreate(&unit,4096) && !trackedProof);type->childCount=0;
    type->literalData=(unsigned char*)"";type->literalSize=SIZE_MAX;CHECK(!trackedCreate(&unit,4096) && !trackedProof);type->literalData=NULL;type->literalSize=0;
    xrtMapUnit(&sources);xrtMapUnit(&moves.paths);xrtArrayUnit(&nodes);xrtArrayUnit(&unit.facts);xl6AstFree(type);xl6AstFree(function);xl6AstFree(target);xrtFree(path);return 1;
}
static int boundaries(void)
{
    xls_sem_unit unit={0};CHECK(xrtArrayInit(&unit.facts,sizeof(xls_sem_fact)));
    XL6_AstNode* root=xl6AstNew(XL6_AST_TYPE,NULL);CHECK(root);xls_sem_fact fact={.inferred=root};CHECK(xrtArrayPush(&unit.facts,&fact));
    for(unsigned i=0;i<65;++i){
        char path[96];snprintf(path,sizeof(path),"proof-source-%u.xl",i);XL6_AstNode* child=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);CHECK(child && xl6AstSetSourceTree(child,path));
        xl6AstAddChild(root,child);CHECK(root->childCount==i+1);
        if(i==63){xls_sem_proof* p=trackedCreate(&unit,4096);CHECK(p && p->sources.Count==64 && p->nodes==65);xlsSemProofDestroy(p);CHECK(!trackedProof);}
    }
    CHECK(!trackedCreate(&unit,4096) && !trackedProof);xl6AstFree(root);
    XL6_AstNode* chain=xrtCalloc(513,sizeof(*chain));CHECK(chain);
    for(size_t i=0;i<513;++i){chain[i].kind=XL6_AST_TYPE;if(i<512)chain[i].inferredType=chain+i+1;}
    ((xls_sem_fact*)xrtArrayGet(&unit.facts,0))->inferred=chain;chain[511].inferredType=NULL;
    xls_sem_proof* p=trackedCreate(&unit,4096);CHECK(p && p->nodes==512);xlsSemProofDestroy(p);CHECK(!trackedProof);
    chain[511].inferredType=chain+512;CHECK(!trackedCreate(&unit,4096) && !trackedProof);
    xrtFree(chain);xrtArrayUnit(&unit.facts);return 1;
}
static int generations(void)
{
    char* path=xrtPathAbs("proof-generations.xl");CHECK(path);xls_analysis_document doc={.key="entry",.path=path,.identity=1};
    xls_completion_session *fast=xlsCompletionSessionCreate(NULL),*slow=xlsCompletionSessionCreate(NULL);CHECK(fast && slow);
    xls_semantic_completion *held[8]={0},*fresh[8]={0};
    for(unsigned generation=0;generation<8;++generation){xstrbuf text={0};CHECK(xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf")));
        if(generation%2)CHECK(xrtStrBufAppend(&text,xrtStrView("// moved 中文😀\r\n")));
        for(unsigned i=0;i<32;++i)CHECK(xrtStrBufAppendFormat(&text,"func stable_%u(int value) as int\r\n if value>0 then\r\n return value+%u\r\n end\r\n return 0\r\nend\r\n",i,i));
        CHECK(xrtStrBufAppendFormat(&text,"func main(...) as int\r\n int completionLocal=stable_0(%u)\r\n return completionL\r\nend\r\n",generation));CHECK(set(&doc,text.Data));
        size_t at=(size_t)(strstr(text.Data,"return completionL")-text.Data)+strlen("return completionL")-3;
        held[generation]=xlsCompletionSessionQuery(fast,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);CHECK(held[generation] && xlsSemanticCompletionInfo(held[generation]).available);
        xls_sem_proof_control zero;xlsFrontendSemProofBegin(&zero,0);xls_semantic_completion* reference=xlsCompletionSessionQuery(slow,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);xlsFrontendSemProofEnd(&zero);CHECK(reference);
        xls_semantic_input in={path,xlsTextData(doc.text),xlsTextSize(doc.text)};fresh[generation]=xlsSemanticCompletionCreate(path,path,at,&in,1,NULL,NULL,NULL,0);CHECK(fresh[generation]);
        char *a=xlsSemanticCompletionJson(held[generation],NULL),*b=xlsSemanticCompletionJson(reference,NULL),*c=xlsSemanticCompletionJson(fresh[generation],NULL);CHECK(a && b && c && !strcmp(a,b) && !strcmp(a,c));xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticStringFree(c);
        xls_sem_work work=xlsSemanticCompletionSemWork(held[generation]);CHECK(!xlsSemanticCompletionSemWork(reference).proofHits && !xlsSemanticCompletionTailWork(held[generation]).bodyMaterialized);
        if(generation)CHECK(work.proofHits>=24 && work.proofNodes>work.proofHits);
        CHECK(sameAst(xlsSemanticCompletionAst(reference),xlsSemanticCompletionAst(fresh[generation])));xlsSemanticCompletionDestroy(reference);xrtStrBufFree(&text);
    }
    xlsCompletionSessionDestroy(fast);xlsCompletionSessionDestroy(slow);xlsTextRelease(doc.text);xrtFree(path);
    for(unsigned i=0;i<8;++i){CHECK(sameAst(xlsSemanticCompletionAst(held[i]),xlsSemanticCompletionAst(fresh[i])));xlsSemanticCompletionDestroy(held[i]);xlsSemanticCompletionDestroy(fresh[i]);}
    return 1;
}
int main(void)
{
    if(!admission() || !boundaries() || !generations())return 1;
    printf("analysis_semantic_proof_test passed: %u checks; complete immutable restoration certificates,original source/cursor/symbol/coordinate parity,optional OOM,quota,nested controls and independent8generations.\n",checks);return 0;
}
