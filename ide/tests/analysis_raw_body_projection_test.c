#define XLS_RAW_BODY_VERIFY_OWNERS 1
#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_cursor_memo_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int sameText(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static int sameRange(XL6_AstRange a,XL6_AstRange b)
{ return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn; }
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b) return a==b;
    if(a->kind!=b->kind || a->flags!=b->flags || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) ||
       a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar || a->literalDecoded!=b->literalDecoded || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || a->line!=b->line || a->column!=b->column ||
       !sameRange(a->range,b->range) || !sameRange(a->tokenRange,b->tokenRange) || !sameText(xl6AstSourcePath(a),xl6AstSourcePath(b)) ||
       !!a->source!=!!b->source || (a->source && (a->source->completionQuery!=b->source->completionQuery || a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd)) || a->semanticTypeId!=b->semanticTypeId || a->semanticTypeAbiId!=b->semanticTypeAbiId || a->semanticSymbolId!=b->semanticSymbolId ||
       a->semanticDeclaredSymbolId!=b->semanticDeclaredSymbolId || a->semanticTargetSymbolId!=b->semanticTargetSymbolId ||
       a->semanticCompoundOperatorSymbolId!=b->semanticCompoundOperatorSymbolId || a->semanticCallKind!=b->semanticCallKind ||
       a->semanticLvalueKind!=b->semanticLvalueKind || a->semanticOwnership!=b->semanticOwnership || a->semanticExitKind!=b->semanticExitKind ||
       a->semanticScopeDepth!=b->semanticScopeDepth || a->semanticTargetScopeDepth!=b->semanticTargetScopeDepth ||
       a->semanticRegionId!=b->semanticRegionId || a->semanticTargetRegionId!=b->semanticTargetRegionId || a->childCount!=b->childCount) return 0;
    if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!sameAst(a->children[i],b->children[i])) return 0;
    return 1;
}


#include "analysis/raw_body_projection.h"
#include "src/sem/xl6_completion.h"
#include <stdlib.h>
static size_t cloneCalls,denyClone,rootsLive;static int faultActive,trackRoots;static XL6_AstNode* tracked[8192];
XL6_AstNode* __real_xlsAstMaterialize(const XL6_AstNode*,const xls_ast_clone_options*,size_t*);
void __real_xl6AstFree(XL6_AstNode*);
static void rememberRoot(XL6_AstNode* node){if(!node || !trackRoots)return;for(size_t i=0;i<8192;++i)if(tracked[i]==node)return;for(size_t i=0;i<8192;++i)if(!tracked[i]){tracked[i]=node;++rootsLive;return;}}
static void forgetTree(const XL6_AstNode* node){if(!node)return;for(size_t i=0;i<8192;++i)if(tracked[i]==node){tracked[i]=NULL;--rootsLive;break;}for(size_t i=0;i<node->childCount;++i)forgetTree(node->children[i]);forgetTree(node->inferredType);forgetTree(node->semanticDeclaredType);}
XL6_AstNode* __wrap_xlsAstMaterialize(const XL6_AstNode* in,const xls_ast_clone_options* options,size_t* bytes){if(faultActive && ++cloneCalls==denyClone)return NULL;XL6_AstNode* out=__real_xlsAstMaterialize(in,options,bytes);rememberRoot(out);return out;}
void __wrap_xl6AstFree(XL6_AstNode* node){forgetTree(node);__real_xl6AstFree(node);}
static char* readSource(const char* name){FILE* f=fopen(name,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);if(n<0 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}char* s=xrtMalloc((size_t)n+1);if(!s){fclose(f);return NULL;}if(fread(s,1,(size_t)n,f)!=(size_t)n){xrtFree(s);fclose(f);return NULL;}s[n]=0;fclose(f);return s;}
static size_t queryAt(const char* text){const char* at=strstr(text,"return pref");size_t skip=!memcmp(text,"\xef\xbb\xbf",3)?3:0;return at ? (size_t)(at-text)+strlen("return pref")-skip:SIZE_MAX;}
static xls_syntax* prepare(const char* path,xls_text* text,xls_syntax* old,int mode,xls_parse_work* work){
 if(mode){XL6_CompletionQuery q={.path=path,.offset=queryAt(xlsTextData(text))};xls_syntax* s=xlsSyntaxBuildCursor(path,text,old,&q,work);xl6CompletionFree(&q);return s;}
 if(old)return xlsSyntaxTryFunction(path,text,old,work);
 XL6_ParseResult parsed;xl6ParseResultInit(&parsed);if(!xl6ParseSource(xlsTextData(text),path,&parsed)){xl6ParseResultFree(&parsed);return NULL;}
 xls_syntax* s=xlsSyntaxAdoptOrdinary(path,text,&parsed.root);xl6ParseResultFree(&parsed);return s;
}
typedef struct rawReader {xls_raw_body_projection* projection;const XL6_AstNode* expected;const XL6_AstNode* root;int okay;} rawReader;
static int32 readRaw(void* user){rawReader* r=user;r->root=xlsRawBodyProjectionAst(r->projection);r->okay=r->root && sameAst(r->root,r->expected);xlsRawBodyProjectionRelease(r->projection);return 0;}
typedef struct rawWitness {xls_raw_unit* unit;size_t bytes;} rawWitness;
typedef struct rawPolicy {rawWitness units[256];size_t count,records,deferred,admitted;int capture,valid;} rawPolicy;
static void recordRaw(xls_raw_unit* unit,const char* path,XL6_AstNode* copy,int deferred,void* user){
 rawPolicy* policy=user;++policy->records;policy->deferred+=!!deferred;
 const XL6_AstNode* raw=xlsRawUnitNode(unit);
 if(!raw || !copy || !path || !sameText(xl6AstSourcePath(copy),path) || raw->kind!=copy->kind)policy->valid=0;
 if(policy->capture){size_t bytes=0,again=0;
  if(policy->count>=256 || !xlsRawUnitBytes(unit,SIZE_MAX,&bytes) || !bytes || !xlsRawUnitBytes(unit,bytes,&again) || again!=bytes || xlsRawUnitBytes(unit,bytes-1,&again)){policy->valid=0;return;}
  policy->units[policy->count++]=(rawWitness){xlsRawUnitRetain(unit),bytes};
 }
}
static int admitRaw(xls_raw_unit* unit,const char* path,const XL6_AstRange* range,size_t* bytes,void* user){
 rawPolicy* policy=user;if(!path || !range || !range->known || !bytes){policy->valid=0;return 0;}
 for(size_t i=0;i<policy->count;++i)if(policy->units[i].unit==unit){*bytes=policy->units[i].bytes;++policy->admitted;return 1;}
 return 0;
}
static void releasePolicy(rawPolicy* policy){for(size_t i=0;i<policy->count;++i)xlsRawUnitRelease(policy->units[i].unit);policy->count=0;}
static int scenario(const char* beforePath,const char* afterPath,int mode){
 char* before=readSource(beforePath);char* after=readSource(afterPath);char* path=xrtPathAbs("raw-owned.xl");CHECK(before && after && path);
 xls_text* t0=xlsTextCreate(before,strlen(before),8*1024*1024,NULL);xls_text* t1=xlsTextCreate(after,strlen(after),8*1024*1024,NULL);CHECK(t0 && t1);
 xls_parse_work w0={0},w1={0};xls_syntax* old=prepare(path,t0,NULL,mode,&w0);xls_syntax* current=prepare(path,t1,old,mode,&w1);CHECK(old && current && xlsSyntaxAccepted(current) && w1.fragments==1 && w1.reused>=32);
 XL6_ParseResult fresh;xl6ParseResultInit(&fresh);XL6_CompletionQuery query={.path=path,.offset=queryAt(after)};CHECK(mode ? xl6ParseSourceWithCompletion(after,path,&fresh,&query) : xl6ParseSource(after,path,&fresh));
 trackRoots=1;faultActive=1;denyClone=0;cloneCalls=0;xls_raw_body_projection* probe=xlsRawBodyProjectionCreate(current,old,SIZE_MAX);faultActive=0;CHECK(probe);size_t operations=cloneCalls;CHECK(operations>=34);CHECK(sameAst(xlsRawBodyProjectionAst(probe),fresh.root));xlsRawBodyProjectionRelease(probe);CHECK(!rootsLive && !xlsRawBodyProjectionOwnedLeases());
 for(size_t failure=1;failure<=operations;++failure){cloneCalls=0;denyClone=failure;faultActive=1;xls_raw_body_projection* p=xlsRawBodyProjectionCreate(current,old,SIZE_MAX);faultActive=0;if(p){CHECK(sameAst(xlsRawBodyProjectionAst(p),fresh.root));xlsRawBodyProjectionRelease(p);}CHECK(!rootsLive && !xlsRawBodyProjectionOwnedLeases());}
 xls_ast_materialize_scope eagerScope;xlsAstMaterializeBegin(&eagerScope,64*1024*1024);xls_raw_body_projection* eager=xlsRawBodyProjectionCreate(current,old,0);xls_ast_materialize_work eagerWork;xlsAstMaterializeEnd(&eagerScope,&eagerWork);CHECK(eager && !xlsRawBodyProjectionWork(eager).deferred && sameAst(xlsRawBodyProjectionAst(eager),fresh.root));
 xls_ast_materialize_scope sparseScope;xlsAstMaterializeBegin(&sparseScope,64*1024*1024);xls_raw_body_projection* sparse=xlsRawBodyProjectionCreate(current,old,SIZE_MAX);xls_ast_materialize_work sparseWork;xlsAstMaterializeEnd(&sparseScope,&sparseWork);CHECK(sparse);xls_raw_body_work work=xlsRawBodyProjectionWork(sparse);CHECK(work.deferred>=32 && !work.complete && !work.materialized && work.retainedBytes>0);CHECK(sparseWork.nodes>0 && sparseWork.nodes<eagerWork.nodes/4 && !sparseWork.heapCopies && !sparseWork.poolFallbacks);
 const XL6_AstNode* headers=xlsRawBodyProjectionHeaders(sparse);CHECK(headers && !sameAst(headers,fresh.root));
 xls_raw_body_projection* exact=xlsRawBodyProjectionCreate(current,old,work.retainedBytes);xls_raw_body_projection* shortOne=xlsRawBodyProjectionCreate(current,old,work.retainedBytes-1);xls_raw_body_projection* tiny=xlsRawBodyProjectionCreate(current,old,1);CHECK(exact && shortOne && tiny);CHECK(xlsRawBodyProjectionWork(exact).deferred==work.deferred && xlsRawBodyProjectionWork(shortOne).deferred<work.deferred && !xlsRawBodyProjectionWork(tiny).deferred);CHECK(sameAst(xlsRawBodyProjectionAst(exact),fresh.root) && sameAst(xlsRawBodyProjectionAst(shortOne),fresh.root) && sameAst(xlsRawBodyProjectionAst(tiny),fresh.root));xlsRawBodyProjectionRelease(exact);xlsRawBodyProjectionRelease(shortOne);xlsRawBodyProjectionRelease(tiny);
 /* Every deferred job independently rolls back a failed body clone. Earlier
  * complete bodies stay private until the whole RAW tree is available. */
 for(size_t failure=1;failure<=work.deferred;++failure){xls_raw_body_projection* p=xlsRawBodyProjectionCreate(current,old,SIZE_MAX);CHECK(p);
  cloneCalls=0;denyClone=failure;faultActive=1;const XL6_AstNode* denied=xlsRawBodyProjectionAst(p);faultActive=0;
  CHECK(!denied && !xlsRawBodyProjectionWork(p).complete && xlsRawBodyProjectionWork(p).materialized==failure-1);
  CHECK(sameAst(xlsRawBodyProjectionAst(p),fresh.root));xlsRawBodyProjectionRelease(p);
 }
 rawPolicy policy={.capture=1,.valid=1};xls_raw_body_projection* capture=xlsRawBodyProjectionCreatePolicy(old,0,NULL,recordRaw,&policy);CHECK(capture && policy.valid && policy.count>=34 && policy.records==policy.count && !policy.deferred);xlsRawBodyProjectionRelease(capture);policy.capture=0;policy.records=policy.admitted=policy.deferred=0;
 xls_raw_body_projection* fromPolicy=xlsRawBodyProjectionCreatePolicy(current,SIZE_MAX,admitRaw,recordRaw,&policy);CHECK(fromPolicy && policy.valid && policy.records==headers->childCount && policy.deferred==work.deferred && policy.admitted==work.deferred && !xlsRawBodyProjectionWork(fromPolicy).sizingPasses);CHECK(sameAst(xlsRawBodyProjectionAst(fromPolicy),fresh.root));xlsRawBodyProjectionRelease(fromPolicy);
 /* A failed constructor publishes no provenance. Successful fallback still
  * publishes each final copy exactly once after every fallible operation. */
 for(size_t failure=1;failure<=operations;++failure){policy.records=policy.admitted=policy.deferred=0;cloneCalls=0;denyClone=failure;faultActive=1;xls_raw_body_projection* p=xlsRawBodyProjectionCreatePolicy(current,SIZE_MAX,admitRaw,recordRaw,&policy);faultActive=0;
  CHECK(policy.valid && policy.records==(p ? headers->childCount : 0));if(p){CHECK(sameAst(xlsRawBodyProjectionAst(p),fresh.root));xlsRawBodyProjectionRelease(p);}
 }
 policy.records=policy.admitted=policy.deferred=0;xls_raw_body_projection* transfer=xlsRawBodyProjectionCreatePolicy(current,SIZE_MAX,admitRaw,recordRaw,&policy);CHECK(transfer && policy.deferred==work.deferred);
 XL6_ParseResult moved;xl6ParseResultInit(&moved);CHECK(xlsRawBodyProjectionTakeHeaderParse(transfer,&moved) && moved.root && xlsRawBodyProjectionWork(transfer).detached && !xlsRawBodyProjectionHeaders(transfer) && !xlsRawBodyProjectionAst(transfer));CHECK(!xlsRawBodyProjectionTakeHeaderParse(transfer,&moved));xlsRawBodyProjectionRelease(transfer);CHECK(moved.root->childCount==headers->childCount && !sameAst(moved.root,fresh.root));xl6ParseResultFree(&moved);
 /* Only independent declaration leases survive;both syntax/text owners die. */
 xlsSyntaxRelease(old);xlsSyntaxRelease(current);xlsTextRelease(t0);xlsTextRelease(t1);xrtFree(before);xrtFree(after);xrtFree(path);
 for(size_t i=0;i<policy.count;++i){size_t bytes=0;CHECK(xlsRawUnitNode(policy.units[i].unit) && xlsRawUnitBytes(policy.units[i].unit,SIZE_MAX,&bytes) && bytes==policy.units[i].bytes);}releasePolicy(&policy);
 cloneCalls=0;denyClone=1;faultActive=1;CHECK(!xlsRawBodyProjectionAst(sparse));faultActive=0;CHECK(!xlsRawBodyProjectionWork(sparse).complete && !xlsRawBodyProjectionWork(sparse).materialized);
 rawReader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(rawReader){.projection=xlsRawBodyProjectionRetain(sparse),.expected=fresh.root};threads[i]=xrtThreadCreate(readRaw,&readers[i],0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay && readers[i].root==readers[0].root);xrtThreadDestroy(threads[i]);}
 CHECK(xlsRawBodyProjectionWork(sparse).complete && xlsRawBodyProjectionWork(sparse).materialized==work.deferred);
 printf("{\"mode\":%d,\"fullPhysicalNodes\":%llu,\"headerPhysicalNodes\":%llu,\"deferredFunctions\":%llu,\"retainedBytes\":%zu,\"sizingPasses\":%llu,\"constructorCloneRefusals\":%zu,\"bodyCloneRefusals\":%llu,\"policyAndTransfer\":true,\"fullRawAstParity\":true}\n",mode,(unsigned long long)eagerWork.nodes,(unsigned long long)sparseWork.nodes,(unsigned long long)work.deferred,work.retainedBytes,(unsigned long long)work.sizingPasses,operations,(unsigned long long)work.deferred);
 xlsRawBodyProjectionRelease(eager);xlsRawBodyProjectionRelease(sparse);xl6ParseResultFree(&fresh);xl6CompletionFree(&query);CHECK(!rootsLive && !xlsRawBodyProjectionOwnedLeases());trackRoots=0;return 1;
}
#include "analysis_raw_test_sources.h"
int main(int argc,char** argv){
 if(argc==1){for(int mode=0;mode<2;++mode){char *before=NULL,*after=NULL;if(!rawTestSources("raw-projection",mode,&before,&after))return 1;int okay=scenario(before,after,mode);xrtFree(before);xrtFree(after);if(!okay)return 1;}printf("raw projection default passed: %u checks\n",checks);return 0;}
 if(argc!=4 || !scenario(argv[1],argv[2],atoi(argv[3])))return 1;
 printf("raw body projection passed: %u checks;raw source/grammar/cloner retained,actual AST nodes reduced,independent chunk lifetime,clone refusal rollback,exact budgets,four concurrent raw AST demands;NOT semantic reuse\n",checks);return 0;}
