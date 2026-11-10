#include <xrt.h>
#include "analysis/check.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_internal.h"
#include "analysis/input_topology.h"
#include "analysis/text.h"
#include "services/semantic_binding_check.h"
#include "services/language_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static _Thread_local size_t fault=SIZE_MAX,calls,compareFault=SIZE_MAX,compares;
static int refuse(void){return calls++==fault;}
static void* frozenAllocate(size_t n){return refuse()?NULL:xrtMalloc(n);}
static void* frozenCalloc(size_t a,size_t b){return refuse()?NULL:xrtCalloc(a,b);}
static char* frozenPath(const char* path){return refuse()?NULL:xrtPathAbs(path);}
static xls_input_path_key frozenKey(const char* path,int absolute){return refuse()?(xls_input_path_key){0}:xlsInputPathPrepare(path,absolute);}
static int frozenCompare(const xls_input_path_key* a,const xls_input_path_key* b,int* okay)
{if(compares++==compareFault){*okay=0;return 0;}return xlsInputPathCompare(a,b,okay);}
#define xrtMalloc frozenAllocate
#define xrtCalloc frozenCalloc
#define xrtPathAbs frozenPath
#define xlsInputPathPrepare frozenKey
#define xlsInputPathCompare frozenCompare
#include "../src/analysis/frozen_inputs.c"
#undef xrtMalloc
#undef xrtCalloc
#undef xrtPathAbs
#undef xlsInputPathPrepare
#undef xlsInputPathCompare
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_frozen_check_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t blocks(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int writeFrozen(const char* path,const char* text)
{FILE* f=fopen(path,"wb");if(!f)return 0;size_t n=strlen(text);int ok=fwrite(text,1,n,f)==n;return fclose(f)==0 && ok;}
typedef struct frozen_cancel {size_t calls,stop;} frozen_cancel;
static int cancelFrozen(void* p){frozen_cancel* c=p;return c->calls++==c->stop;}
static int inputOwner(const char* dir)
{
 char *a=xrtPathJoin(dir,"入口😀.xl"),*b=xrtPathJoin(dir,"库.xl");CHECK(a && b);
 const char* raw="\xef\xbb\xbf" "func main(...) as int\r\n return 3\r\nend\r\n";xls_semantic_input in[]={{a,raw,strlen(raw)},{b,"// second",9}};
 uint64_t live=blocks();calls=0;compares=0;xls_frozen_inputs* owner=xlsFrozenInputsCreate(in,2,NULL,NULL,NULL,0);CHECK(owner && xlsFrozenInputsBytes(owner)>strlen(raw));size_t allocationCount=calls,comparisonCount=compares;
 const XL6_SourceOverlayItem* item=NULL;XL6_SourceOverlay overlay={0};CHECK(xlsFrozenInputsLookup(owner,&overlay,a,&item)==1 && item->size==strlen(raw)-3 && !strcmp(item->text,raw+3));
 CHECK(xlsFrozenInputsRetain(owner)==owner);xlsFrozenInputsRelease(owner);xlsFrozenInputsRelease(owner);CHECK(blocks()==live);
 for(size_t i=0;i<allocationCount;++i){fault=i;calls=0;CHECK(!xlsFrozenInputsCreate(in,2,NULL,NULL,NULL,0) && calls==i+1 && blocks()==live);}fault=SIZE_MAX;
 for(size_t i=0;i<comparisonCount;++i){compareFault=i;compares=0;CHECK(!xlsFrozenInputsCreate(in,2,NULL,NULL,NULL,0) && blocks()==live);}compareFault=SIZE_MAX;
 frozen_cancel c={0,SIZE_MAX};owner=xlsFrozenInputsCreate(in,2,cancelFrozen,&c,NULL,0);CHECK(owner);xlsFrozenInputsRelease(owner);size_t polls=c.calls;
 for(size_t i=0;i<polls;++i){c=(frozen_cancel){0,i};CHECK(!xlsFrozenInputsCreate(in,2,cancelFrozen,&c,NULL,0) && c.calls==i+1 && blocks()==live);}
 CHECK(!xlsFrozenInputsCreate(in,0,NULL,NULL,NULL,0) && !xlsFrozenInputsCreate(in,513,NULL,NULL,NULL,0));
 xls_semantic_input bad[]={in[0],in[0]};CHECK(!xlsFrozenInputsCreate(bad,2,NULL,NULL,NULL,0));bad[0]=(xls_semantic_input){a,"a\0b",3};CHECK(!xlsFrozenInputsCreate(bad,1,NULL,NULL,NULL,0));bad[0]=(xls_semantic_input){a,"\xc0\x80",2};CHECK(!xlsFrozenInputsCreate(bad,1,NULL,NULL,NULL,0));
 bad[0]=(xls_semantic_input){"relative.xl","",0};CHECK(!xlsFrozenInputsCreate(bad,1,NULL,NULL,NULL,0));
 CHECK(!xlsInputPathCompare(NULL,NULL,NULL));xls_input_path_key key=xlsInputPathPrepare(a,0);CHECK(key.data);xrtFree(key.data);
 /* Distinguish an absent candidate from identity/resource failure. A failed
  * wide lookup must poison the original report, never permit disk fallback. */
 owner=xlsFrozenInputsCreate(in,2,NULL,NULL,NULL,0);CHECK(owner);XL6_SourceReport report;xl6SourceReportInit(&report);overlay.report=&report;
 char* missing=xrtPathJoin(dir,"missing.xl");CHECK(missing && xlsFrozenInputsLookup(owner,&overlay,missing,&item)==0 && report.complete && !item);
 calls=0;fault=0;CHECK(xlsFrozenInputsLookup(owner,&overlay,a,&item)==-1 && !item && !report.complete);fault=SIZE_MAX;xl6SourceReportFree(&report);xl6SourceReportInit(&report);
 compares=0;compareFault=0;CHECK(xlsFrozenInputsLookup(owner,&overlay,a,&item)==-1 && !item && !report.complete);compareFault=SIZE_MAX;xl6SourceReportFree(&report);overlay.report=NULL;xrtFree(missing);xlsFrozenInputsRelease(owner);
 /* Exact original text quotas, without asking the native parser to accept a
  *64MiB program. One extra byte must refuse before publishing any owner. */
 char* large=xrtMalloc(XL6_SOURCE_OVERLAY_BYTES+1);CHECK(large);memset(large,' ',XL6_SOURCE_OVERLAY_BYTES);large[XL6_SOURCE_OVERLAY_BYTES]=0;
 xls_semantic_input quota[9];char* quotaPaths[9];for(size_t i=0;i<9;++i){char name[32];snprintf(name,sizeof(name),"quota%zu.xl",i);quotaPaths[i]=xrtPathJoin(dir,name);CHECK(quotaPaths[i]);quota[i]=(xls_semantic_input){quotaPaths[i],large,i<8?XL6_SOURCE_OVERLAY_BYTES:1};}
 owner=xlsFrozenInputsCreate(quota,8,NULL,NULL,NULL,0);CHECK(owner);xlsFrozenInputsRelease(owner);CHECK(!xlsFrozenInputsCreate(quota,9,NULL,NULL,NULL,0));
 quota[0].size=XL6_SOURCE_OVERLAY_BYTES+1;CHECK(!xlsFrozenInputsCreate(quota,1,NULL,NULL,NULL,0));for(size_t i=0;i<9;++i)xrtFree(quotaPaths[i]);xrtFree(large);
 xls_frontend_context_scope outer,inner;owner=xlsFrozenInputsCreate(in,2,NULL,NULL,NULL,0);CHECK(owner);xlsFrontendContextBegin(&outer,NULL);
 CHECK(xlsFrontendFrozenBind(&overlay,owner) && !xlsFrontendFrozenBind(&overlay,owner) && xlsFrontendFrozenLookup(&overlay,a,&item)==1);
 xlsFrontendContextBegin(&inner,NULL);CHECK(xlsFrontendFrozenLookup(&overlay,a,&item)==2 && xlsFrontendContextEnd(&inner));CHECK(xlsFrontendFrozenLookup(&overlay,a,&item)==1 && xlsFrontendContextEnd(&outer));CHECK(xlsFrontendFrozenLookup(&overlay,a,&item)==2);xlsFrozenInputsRelease(owner);
 CHECK(blocks()==live);xrtFree(a);xrtFree(b);return 1;
}
typedef struct frozen_read {xls_semantic_check* check;const char* path;int okay;} frozen_read;
static int32 readFrozen(void* p)
{frozen_read* r=p;r->okay=1;for(size_t i=0;i<3;++i){xls_check_inputs inputs;if(!xlsSemanticCheckValidate(r->check) || !xlsSemanticCheckInputs(r->check,&inputs) || inputs.sources<65)r->okay=0;}xlsSemanticCheckDestroy(r->check);return 0;}
static int wideNative(const char* dir,size_t count,int explicitPaths)
{
 xls_semantic_input* in=xrtCalloc(count,sizeof(*in));CHECK(in);char* main=xrtMalloc(count*64+256);CHECK(main);size_t used=0;
 for(size_t i=1;i<count;++i){if(explicitPaths)used+=(size_t)sprintf(main+used,"import \"./dep%03zu.xl\" as dep%03zu\r\n",i,i);else used+=(size_t)sprintf(main+used,"import dep%03zu\r\n",i);}
 used+=(size_t)sprintf(main+used,"func main(...) as int\r\n return dep001.value()+dep%03zu.value()\r\nend\r\n",count-1);
 for(size_t i=0;i<count;++i){char name[64];snprintf(name,sizeof(name),i?"dep%03zu.xl":"main%03zu.xl",i);char* path=xrtPathJoin(dir,name);char* text=xrtStrDup(i?"\xef\xbb\xbf" "export func value() as int\r\n return 1\r\nend\r\n":main);CHECK(path && text);in[i]=(xls_semantic_input){path,text,strlen(text)};CHECK(writeFrozen(path,i?"invalid disk fallback(":"invalid disk main("));}
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=in[0].path,.directory=dir,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);CHECK(context);
 char error[512];CHECK(!xlsSemanticCheckCreateInContext(context,in[0].path,in,count,1,NULL,NULL,error,sizeof(error)));
 xls_semantic_check* check=xlsSemanticCheckCreateFrozenInContext(context,in[0].path,in,count,1,NULL,NULL,error,sizeof(error));if(!check)fprintf(stderr,"frozen native: %s\n",error);CHECK(check);
 xls_check_info ci=xlsSemanticCheckInfo(check);if(!ci.accepted)for(size_t i=0;i<ci.diagnostics;++i){xls_check_diagnostic d;xlsSemanticCheckDiagnostic(check,i,&d);fprintf(stderr,"%s:%d %s\n",d.path,d.line,d.message);}CHECK(ci.accepted && ci.hasIndex);
 xls_check_inputs inputs;CHECK(xlsSemanticCheckInputs(check,&inputs) && inputs.sources==count);uint64_t mask=UINT64_MAX;CHECK(!xlsSemanticCheckInputUsage(check,&mask) && !mask && !xlsSemanticCheckTopology(check));
 xide_semantic_source* sources=xrtCalloc(count,sizeof(*sources));CHECK(sources);
 for(size_t i=0;i<count;++i){xls_input_source s;CHECK(xlsSemanticCheckSource(check,i,&s) && !strcmp(s.origin,"overlay"));size_t j=0;while(j<count && strcmp(in[j].path,s.path))++j;CHECK(j<count);size_t skip=j?3:0;CHECK(s.size==in[j].size-skip && !memcmp(s.text,in[j].text+skip,s.size));sources[i]=(xide_semantic_source){s.path,s.text,s.kind,j?1:0};}
 /* A real native target and its IDENT rows produce the candidate; recheck the
  * entire >64-file hypothetical source bank through the production API. */
 xls_projection_store* cache=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(cache);int line,column;CHECK(xideLspOffsetToPosition(main,(int)(strstr(main,"value()+")-main),&line,&column));
 xide_reference_shard* before=xideSemanticReferenceShardCreate(cache,check,sources,count,XIDE_REFERENCE_BYTES,NULL);xide_reference_key* key=xideSemanticReferenceKeyCreate(cache,check,sources,count,in[0].path,line,column,XIDE_REFERENCE_BYTES,NULL);xide_reference_identifier_view view;
 CHECK(before && key && xideSemanticReferenceShardIdentifiers(before,key,1,&view)==XIDE_REFERENCE_MATCH && view.references.count==2);
 xide_rename_plan_input docs[2]={0};xide_rename_plan_change changes[2];size_t dc=0;
 for(size_t i=0;i<2;++i){xide_semantic_reference r;CHECK(xideSemanticReferenceShardIdentifierAt(&view,i,&r));size_t d=0;while(d<dc && strcmp(docs[d].source.path,r.path))++d;if(d==dc){size_t j=0;while(j<count && strcmp(sources[j].path,r.path))++j;CHECK(j<count);docs[dc].source=sources[j];docs[dc].document=xideLanguageReferenceDocument(sources+j,30,NULL);CHECK(docs[dc++].document);}changes[i]=(xide_rename_plan_change){d,r.start,r.end};}
 xide_rename_plan* plan=xideRenamePlanCreate(docs,dc,changes,2,xrtStrView("value"),xrtStrView("新值😀"),0,NULL,NULL,NULL);CHECK(plan);for(size_t i=0;i<dc;++i)xrtValueRelease((xvalue*)docs[i].document);
 xls_semantic_input* edited=xrtMalloc(count*sizeof(*edited));CHECK(edited);memcpy(edited,in,count*sizeof(*edited));
 for(size_t i=0;i<dc;++i){xide_rename_plan_document d;CHECK(xideRenamePlanDocumentAt(plan,i,&d));size_t j=0;while(j<count && strcmp(in[j].path,d.path))++j;CHECK(j<count);edited[j]=(xls_semantic_input){d.path,d.after.Data,d.after.Size};}
 xls_semantic_check* after=xlsSemanticCheckCreateFrozenInContext(context,in[0].path,edited,count,1,NULL,NULL,error,sizeof(error));CHECK(after && xlsSemanticCheckInfo(after).accepted);xls_check_inputs ai;CHECK(xlsSemanticCheckInputs(after,&ai) && ai.sources==count);
 for(size_t i=0;i<count;++i){xls_input_source s;CHECK(xlsSemanticCheckSource(after,i,&s));size_t j=0;while(j<count && strcmp(edited[j].path,s.path))++j;CHECK(j<count);sources[i]=(xide_semantic_source){s.path,s.text,s.kind,j?1:0};}
 xide_reference_shard* afterShard=xideSemanticReferenceShardCreate(cache,after,sources,count,XIDE_REFERENCE_BYTES,NULL);xide_binding_check_info binding;CHECK(afterShard && xideSemanticBindingsPreserved(before,afterShard,plan,0,NULL,NULL,&binding)==XIDE_BINDING_PRESERVED && binding.sources==count);
 xideSemanticReferenceShardRelease(afterShard);xlsSemanticCheckDestroy(after);xrtFree(edited);xideRenamePlanRelease(plan);xideSemanticReferenceKeyRelease(key);xideSemanticReferenceShardRelease(before);xlsProjectionStoreDestroy(cache);xrtFree(sources);
 /* Independently parse the same bytes from disk with the ORIGINAL0-overlay
  * constructor, then compare the complete native semantic index. */
 for(size_t i=0;i<count;++i)CHECK(writeFrozen(in[i].path,in[i].text));
 xls_semantic_check* oracle=xlsSemanticCheckCreateInContext(context,in[0].path,NULL,0,1,NULL,NULL,error,sizeof(error));CHECK(oracle && xlsSemanticCheckInfo(oracle).accepted);
 const char *a=xlsSemanticCheckIndex(check),*b=xlsSemanticCheckIndex(oracle);CHECK(a && b && !strcmp(a,b));xlsSemanticCheckDestroy(oracle);
 for(size_t i=0;i<count;++i){xrtFree((void*)in[i].path);xrtFree((void*)in[i].text);}xrtFree(in);xrtFree(main);
 frozen_read readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(frozen_read){xlsSemanticCheckRetain(check),NULL,0};threads[i]=xrtThreadCreate(readFrozen,readers+i,0);CHECK(threads[i]);}xlsSemanticCheckDestroy(check);
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xlsContextRelease(context);return 1;
}
int main(void)
{
 char* dir=xrtPathAbs("build/frozen-inputs");if(!dir || !xrtDirCreateAll(dir) || !inputOwner(dir) || !wideNative(dir,130,0) || !wideNative(dir,512,0) || !wideNative(dir,512,1))return 1;xrtFree(dir);
 printf("analysis_frozen_check_test passed: %u checks; actual130/512-file hypothetical native checks and rename binding comparison, original disk-index oracle, Unicode/BOM/CRLF, source ownership, original64-overlay refusal, no truncated usage/topology, faults/cancellation and detached readers\n",checks);return 0;
}
