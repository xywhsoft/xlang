#include "analysis/ir_native_pipeline.h"
#include "analysis/ir_native_call_cache.h"
#include "analysis/ir_native_name_cache.h"
#if defined(XLS_NATIVE_IR_PIPELINE_COMPONENT) && XLS_NATIVE_IR_PIPELINE_COMPONENT && \
    defined(XLS_NATIVE_IR_PIPELINE_SOURCE) && defined(XLS_NATIVE_IR_TYPE_RESOURCE_GUARD)
typedef struct xls_call_build xls_call_build;
typedef struct xls_name_build xls_name_build;
static const XL6_IrNode* xlsCallsNodeGet(const XL6_IrModule*,XL6_IrNodeId);
static const XL6_IrNode* xlsCallsEntryGet(const XL6_IrModule*,XL6_IrNodeId);
static const XL6_IrType* xlsCallsTypeGet(const XL6_IrTypePool*,XL6_IrTypeId);
static const XL6_IrNode* xlsNamesNodeGet(const XL6_IrModule*,XL6_IrNodeId);
static const XL6_IrNode* xlsNamesEntryGet(const XL6_IrModule*,XL6_IrNodeId);
static const XL6_IrType* xlsNamesTypeGet(const XL6_IrTypePool*,XL6_IrTypeId);
struct xls_ir_native_pipeline {
 XL6_IrModule module;const XL6_AstNode* root;
 size_t phase,lookupBytes,cursor;xls_ir_pipeline_state state;xls_call_build* calls;xls_name_build* names;
};
#include "ir_native_pipeline_generated.inc"
#include "analysis/frontend_ir_input_bytes.inc"
#include "analysis/frontend_ir_call_cache.inc"
#include "analysis/frontend_ir_name_cache.inc"
static void xlsIrPipelineStatus(xls_ir_pipeline_status* out,xls_ir_pipeline_status status)
{if(out)*out=status;}
int xlsIrNativePipelineAvailable(void){return 1;}
size_t xlsIrNativePipelinePhaseCount(void){return sizeof(xlsIrPipelineNames)/sizeof(*xlsIrPipelineNames);}
const char* xlsIrNativePipelinePhaseName(size_t at)
{return at<xlsIrNativePipelinePhaseCount()?xlsIrPipelineNames[at]:NULL;}
xls_ir_native_pipeline* xlsIrNativePipelineCreate(xls_ir_pipeline_status* status)
{
 xls_ir_native_pipeline* b=xrtCalloc(1,sizeof(*b));
 xlsIrPipelineStatus(status,b?XLS_PIPELINE_OK:XLS_PIPELINE_MEMORY);return b;
}
int xlsIrNativePipelinePhaseHasNodeRanges(size_t phase){return xlsIrPipelineNodeRanges(phase);}
size_t xlsIrNativePipelineNodeCursor(const xls_ir_native_pipeline* b){return b?b->cursor:0;}
static int xlsIrPipelineExecute(xls_ir_native_pipeline* b,XL6_DiagList* diags,size_t end)
{
 size_t count=xlsIrNativePipelinePhaseCount();int okay;
 if(b->phase==18&&!end&&!b->cursor&&b->names){
  okay=xlsNamePhase(&b->module,b->names);
  if(!okay)xl6IrDiag(diags,NULL,"cannot build XLIR implicit names");
 }
 else if(b->phase==26&&!end&&!b->cursor&&b->calls){
  okay=xlsCallPhase(&b->module,b->calls);
  if(!okay)xl6IrDiag(diags,NULL,"cannot build XLIR call sites");
 }
 else if(end||b->cursor){
  if(!end)end=b->module.nodeCount;
  okay=xlsIrPipelinePhaseNodesOriginal(&b->module,diags,b->phase,b->cursor,end);
  if(okay&&end<b->module.nodeCount){b->cursor=end;return 1;}
 }
 else if(b->phase+1<count)okay=xlsIrPipelinePhaseOriginal(&b->module,b->root,diags,b->phase);
 else if(b->phase+1==count)okay=xlsIrPipelineCompleteOriginal(&b->module,diags);
 else return 0;
 if(okay){b->cursor=0;++b->phase;if(b->phase==count)b->state=XLS_PIPELINE_READY;}
 return okay;
}
/* Scope is deliberately call-local:separate builders may interleave and no
 * suspended builder owns a thread's compiler indexes,memo or borrow owner.
 * Finish holds one scope for the entire remaining original pipeline. */
static int xlsIrPipelineRun(xls_ir_native_pipeline* b,XL6_DiagList* diags,
 int prepare,int all,size_t end,xls_ir_pipeline_status* status)
{
 xls_ir_memo_build* memo=xlsActiveIrMemo;xls_ir_result* owner=xlsActiveIrOwner;
 xlsActiveIrMemo=NULL;xlsActiveIrOwner=NULL;
 xls_ir_type_lookup scan;xlsIrTypeLookupBegin(&scan,b->root,b->lookupBytes);
 size_t charged=scan.ready?scan.filtered.childCount*sizeof(*scan.items):0;
 xls_type_alias_scope aliases;xlsFrontendTypeAliasBegin(&aliases,b->root,b->lookupBytes-charged);
 xls_ir_type_resource_scope resources={.previous=xlsActiveTypeResource};xlsActiveTypeResource=&resources;
#ifdef XLS_IR_PIPELINE_SCOPE_TEST
 XLS_IR_PIPELINE_SCOPE_TEST();
#endif
 int okay;size_t attempted=b->phase;
 if(prepare)okay=xlsIrPipelinePrepareOriginal(&b->module,b->root,diags);
 else{
  do{attempted=b->phase;okay=xlsIrPipelineExecute(b,diags,end);}while(okay&&all&&!resources.failed&&b->state!=XLS_PIPELINE_READY);
 }
 xlsActiveTypeResource=resources.previous;
 if(resources.failed){b->phase=attempted;xl6IrModuleFree(&b->module);xl6DiagAdd(diags,0,0,"resource failure constructing native IR");okay=0;}
 xlsFrontendTypeAliasEnd(&aliases);xlsActiveTypeLookup=scan.previous;xrtFree(scan.items);
 if(!okay){xl6IrModuleFree(&b->module);b->root=NULL;b->state=XLS_PIPELINE_FAILED;}
 else if(prepare)b->state=XLS_PIPELINE_PREPARED;
 else if(b->state==XLS_PIPELINE_READY)b->root=NULL;
 xlsActiveIrMemo=memo;xlsActiveIrOwner=owner;
 xlsIrPipelineStatus(status,okay?XLS_PIPELINE_OK:xlsFrontendCancelObserved()?XLS_PIPELINE_CANCELLED:resources.failed?XLS_PIPELINE_MEMORY:XLS_PIPELINE_NATIVE_ERROR);
 return okay;
}
int xlsIrNativePipelinePrepare(xls_ir_native_pipeline* b,const XL6_AstNode* root,
 XL6_DiagList* diags,size_t lookupBytes,xls_ir_pipeline_status* status)
{
 if(!b||!root||root->kind!=XL6_AST_MODULE||!diags||diags->count||
    (b->state!=XLS_PIPELINE_EMPTY&&b->state!=XLS_PIPELINE_MOVED&&b->state!=XLS_PIPELINE_FAILED)){
  xlsIrPipelineStatus(status,XLS_PIPELINE_INPUT);return 0;
 }
 b->root=root;b->phase=b->cursor=0;b->lookupBytes=lookupBytes<XLS_IR_TYPE_LOOKUP_BYTES?lookupBytes:XLS_IR_TYPE_LOOKUP_BYTES;
 return xlsIrPipelineRun(b,diags,1,0,0,status);
}
static int xlsIrPipelineAdvance(xls_ir_native_pipeline* b,XL6_DiagList* diags,int all,xls_ir_pipeline_status* status)
{
 if(!b||!diags||diags->count||(b->state!=XLS_PIPELINE_PREPARED&&b->state!=XLS_PIPELINE_READY)){
  xlsIrPipelineStatus(status,XLS_PIPELINE_INPUT);return 0;
 }
 if(b->state==XLS_PIPELINE_READY){xlsIrPipelineStatus(status,XLS_PIPELINE_OK);return 1;}
 return xlsIrPipelineRun(b,diags,0,all,0,status);
}
int xlsIrNativePipelineAdvanceNodes(xls_ir_native_pipeline* b,size_t end,XL6_DiagList* d,xls_ir_pipeline_status* s)
{
 if(!b||!d||d->count||b->state!=XLS_PIPELINE_PREPARED||
    !xlsIrPipelineNodeRanges(b->phase)||end<=b->cursor||end>b->module.nodeCount){
  xlsIrPipelineStatus(s,XLS_PIPELINE_INPUT);return 0;
 }
 return xlsIrPipelineRun(b,d,0,0,end,s);
}
int xlsIrNativePipelineAdvance(xls_ir_native_pipeline* b,XL6_DiagList* d,xls_ir_pipeline_status* s)
{return xlsIrPipelineAdvance(b,d,0,s);}
int xlsIrNativePipelineFinish(xls_ir_native_pipeline* b,XL6_DiagList* d,xls_ir_pipeline_status* s)
{return xlsIrPipelineAdvance(b,d,1,s);}
int xlsIrNativePipelineFinishCachedCalls(xls_ir_native_pipeline* b,XL6_DiagList* d,
 const xls_ir_call_cache* previous,size_t limit,xls_ir_call_cache** out,
 xls_ir_call_cache_work* work,xls_ir_pipeline_status* status)
{
 if(!out||*out||!b||!d||d->count||b->calls||b->names||
    (b->state!=XLS_PIPELINE_READY&&(b->state!=XLS_PIPELINE_PREPARED||b->phase>26||(b->phase==26&&b->cursor)))){
  xlsIrPipelineStatus(status,XLS_PIPELINE_INPUT);return 0;
 }
 xls_call_build build={.previous=previous,.limit=limit<16u*1024u*1024u?limit:16u*1024u*1024u};
 b->calls=build.limit?&build:NULL;int okay=xlsIrNativePipelineFinish(b,d,status);b->calls=NULL;
 if(okay){*out=build.next;build.work.bytes=xlsIrCallCacheBytes(build.next);}else xlsIrCallCacheDestroy(build.next);
 if(work)*work=build.work;
 return okay;
}
int xlsIrNativePipelineFinishCachedNames(xls_ir_native_pipeline* b,XL6_DiagList* d,
 const xls_ir_name_cache* previous,size_t limit,xls_ir_name_cache** out,
 xls_ir_name_cache_work* work,xls_ir_pipeline_status* status)
{
 if(!out||*out||!b||!d||d->count||b->calls||b->names||
    (b->state!=XLS_PIPELINE_READY&&(b->state!=XLS_PIPELINE_PREPARED||b->phase>18||(b->phase==18&&b->cursor)))){
  xlsIrPipelineStatus(status,XLS_PIPELINE_INPUT);return 0;
 }
 xls_name_build build={.previous=previous,.limit=limit<16u*1024u*1024u?limit:16u*1024u*1024u};
 b->names=build.limit?&build:NULL;int okay=xlsIrNativePipelineFinish(b,d,status);b->names=NULL;
 if(okay){*out=build.next;build.work.bytes=xlsIrNameCacheBytes(build.next);}else xlsIrNameCacheDestroy(build.next);
 if(work)*work=build.work;
 return okay;
}
const XL6_IrModule* xlsIrNativePipelineModule(const xls_ir_native_pipeline* b)
{return b&&(b->state==XLS_PIPELINE_PREPARED||b->state==XLS_PIPELINE_READY)?&b->module:NULL;}
xls_ir_pipeline_state xlsIrNativePipelineState(const xls_ir_native_pipeline* b)
{return b?b->state:XLS_PIPELINE_EMPTY;}
size_t xlsIrNativePipelinePhase(const xls_ir_native_pipeline* b){return b?b->phase:0;}
int xlsIrNativePipelineTake(xls_ir_native_pipeline* b,XL6_IrModule* module)
{
 if(!b||!module||module==&b->module||b->state!=XLS_PIPELINE_READY)return 0;
 xl6IrModuleFree(module);*module=b->module;xl6IrModuleInit(&b->module);b->state=XLS_PIPELINE_MOVED;return 1;
}
void xlsIrNativePipelineReset(xls_ir_native_pipeline* b)
{if(b){xl6IrModuleFree(&b->module);b->root=NULL;b->phase=b->lookupBytes=b->cursor=0;b->state=XLS_PIPELINE_EMPTY;}}
void xlsIrNativePipelineDestroy(xls_ir_native_pipeline* b)
{if(b){xlsIrNativePipelineReset(b);xrtFree(b);}}
#else
static void xlsIrPipelineUnavailable(xls_ir_pipeline_status* s){if(s)*s=XLS_PIPELINE_UNAVAILABLE;}
int xlsIrNativePipelineAvailable(void){return 0;}
void xlsIrNameCacheDestroy(xls_ir_name_cache* c){(void)c;}
size_t xlsIrNameCacheBytes(const xls_ir_name_cache* c){(void)c;return 0;}
int xlsIrNativePipelineFinishCachedNames(xls_ir_native_pipeline* b,XL6_DiagList* d,
 const xls_ir_name_cache* p,size_t n,xls_ir_name_cache** out,xls_ir_name_cache_work* w,xls_ir_pipeline_status* s)
{(void)b;(void)d;(void)p;(void)n;(void)out;(void)w;xlsIrPipelineUnavailable(s);return 0;}
void xlsIrCallCacheDestroy(xls_ir_call_cache* c){(void)c;}
size_t xlsIrCallCacheBytes(const xls_ir_call_cache* c){(void)c;return 0;}
int xlsIrNativePipelineFinishCachedCalls(xls_ir_native_pipeline* b,XL6_DiagList* d,
 const xls_ir_call_cache* p,size_t n,xls_ir_call_cache** out,xls_ir_call_cache_work* w,xls_ir_pipeline_status* s)
{(void)b;(void)d;(void)p;(void)n;(void)out;(void)w;xlsIrPipelineUnavailable(s);return 0;}
int xlsIrNativePipelinePhaseHasNodeRanges(size_t phase){(void)phase;return 0;}
size_t xlsIrNativePipelineNodeCursor(const xls_ir_native_pipeline* b){(void)b;return 0;}
int xlsIrNativePipelineAdvanceNodes(xls_ir_native_pipeline* b,size_t end,XL6_DiagList* d,xls_ir_pipeline_status* s)
{(void)b;(void)end;(void)d;xlsIrPipelineUnavailable(s);return 0;}
size_t xlsIrNativePipelinePhaseCount(void){return 0;}
const char* xlsIrNativePipelinePhaseName(size_t at){(void)at;return NULL;}
xls_ir_native_pipeline* xlsIrNativePipelineCreate(xls_ir_pipeline_status* s){xlsIrPipelineUnavailable(s);return NULL;}
int xlsIrNativePipelinePrepare(xls_ir_native_pipeline* b,const XL6_AstNode* a,XL6_DiagList* d,size_t n,xls_ir_pipeline_status* s)
{(void)b;(void)a;(void)d;(void)n;xlsIrPipelineUnavailable(s);return 0;}
int xlsIrNativePipelineAdvance(xls_ir_native_pipeline* b,XL6_DiagList* d,xls_ir_pipeline_status* s)
{(void)b;(void)d;xlsIrPipelineUnavailable(s);return 0;}
int xlsIrNativePipelineFinish(xls_ir_native_pipeline* b,XL6_DiagList* d,xls_ir_pipeline_status* s)
{(void)b;(void)d;xlsIrPipelineUnavailable(s);return 0;}
const XL6_IrModule* xlsIrNativePipelineModule(const xls_ir_native_pipeline* b){(void)b;return NULL;}
xls_ir_pipeline_state xlsIrNativePipelineState(const xls_ir_native_pipeline* b){(void)b;return XLS_PIPELINE_EMPTY;}
size_t xlsIrNativePipelinePhase(const xls_ir_native_pipeline* b){(void)b;return 0;}
int xlsIrNativePipelineTake(xls_ir_native_pipeline* b,XL6_IrModule* m){(void)b;(void)m;return 0;}
void xlsIrNativePipelineReset(xls_ir_native_pipeline* b){(void)b;}
void xlsIrNativePipelineDestroy(xls_ir_native_pipeline* b){(void)b;}
#endif
