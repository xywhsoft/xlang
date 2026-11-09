#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xl6_sem.c"
#define main modelSourceReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/check_ir.h"
#include "analysis/function_unit_internal.h"
#include "analysis/typed_module_internal.h"
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static xls_sem_memo* capturedMemo;
int __real_xlsFrontendSemCheckMemoFinal(XL6_AstNode*,XL6_DiagList*,const char*,int,const char*,const XL6_SourceReport*,const XL6_SourceReport*,const xls_sem_memo*,xls_sem_memo**,xls_sem_work*,size_t);
int __wrap_xlsFrontendSemCheckMemoFinal(XL6_AstNode* root,XL6_DiagList* diags,const char* module,int index,const char* path,
 const XL6_SourceReport* sources,const XL6_SourceReport* oldSources,const xls_sem_memo* old,xls_sem_memo** memo,xls_sem_work* work,size_t bytes)
{
 int result=__real_xlsFrontendSemCheckMemoFinal(root,diags,module,index,path,sources,oldSources,old,memo,work,bytes);
 if(memo && *memo){xlsFrontendSemMemoDestroy(capturedMemo);capturedMemo=xlsFrontendSemMemoRetain(*memo);}return result;
}
static int denyComposition,cancelComposition,modelCancelledFlag;
static size_t deniedModelLeases,cancelledModelLeases;
xls_typed_module* __real_xlsTypedModuleTakeBorrowed(XL6_AstNode*,xls_typed_function* const*,size_t);
xls_typed_module* __wrap_xlsTypedModuleTakeBorrowed(XL6_AstNode* root,xls_typed_function* const* owners,size_t count)
{
 size_t leases=0;if(capturedMemo)for(size_t i=0;i<capturedMemo->entries.Count;++i)leases+=!!((const xls_sem_entry*)capturedMemo->entries.Data)[i].nativeModelLease;
 if(denyComposition){denyComposition=0;deniedModelLeases=leases;return NULL;}
 xls_typed_module* result=__real_xlsTypedModuleTakeBorrowed(root,owners,count);
 if(result && cancelComposition){cancelComposition=0;cancelledModelLeases=leases;modelCancelledFlag=1;}
 return result;
}
static int modelCancelled(void* unused){(void)unused;return modelCancelledFlag;}
static int checkPublicationSource(xls_sem_memo* memo,const xls_typed_module* owner,const xls_typed_module* wrong,size_t slot,xls_typed_function* function)
{
 for(unsigned mode=0;mode<3;++mode){
  xarray entries={0};CHECK(xrtArrayInit(&entries,sizeof(xls_sem_entry)) && xrtArrayResize(&entries,memo->entries.Count));
  memcpy(entries.Data,memo->entries.Data,memo->entries.Count*sizeof(xls_sem_entry));
  for(size_t i=0;i<entries.Count;++i){xls_sem_entry* e=xrtArrayGet(&entries,i);e->native=NULL;e->nativePlaced=e->nativeModelLease=0;}
  xls_sem_entry* entry=xrtArrayGet(&entries,slot);const xls_sem_entry* old=xrtArrayConstGet(&memo->entries,slot);
  entry->native=function;entry->nativeModelLease=entry->finalCandidate=1;entry->finalPredictedStart=old->finalRegionStart;entry->finalPredictedEnd=old->finalRegionEnd;
  xls_sem_memo candidate={.entries=entries,.nativeFinalPrepared=1,.finalPrepared=1};candidate.finalWork.contextHits=1;
  xls_sem_build build={.previous=xlsActiveSemBuild,.baseline=memo,.memo=&candidate,.enabled=1};xlsActiveSemBuild=&build;
  XL6_AstNode root=*xlsTypedModuleRoot(owner);root.children=xrtMalloc(root.childCount*sizeof(*root.children));CHECK(root.children);
  memcpy(root.children,xlsTypedModuleRoot(owner)->children,root.childCount*sizeof(*root.children));XL6_AstNode* stub=xl6AstNew(XL6_AST_FUNC,NULL);CHECK(stub);root.children[slot]=stub;
  xls_raw_scope raw;xlsRawBodyScopeBegin(&raw,memo,32u*1024u*1024u);raw.ordinary=1;raw.deferred=raw.consumed=1;raw.preprocessValid=1;
  xls_model_source_control control={.memo=mode==2 ? &candidate : memo};raw.modelSource=&control;raw.nativeSource=mode==1 ? wrong : owner;
  int result=xlsSemPrepareNative(NULL,&root);
  if(!mode)CHECK(result==2 && root.children[slot]==xlsTypedFunctionRoot(function));
  else CHECK(!result && build.bodyFailure && raw.failed && root.children[slot]==stub);
  xlsActiveSemBuild=build.previous;xlsRawBodyScopeEnd(&raw);xl6AstFree(stub);xrtFree(root.children);xrtArrayUnit(&entries);
 }
 return 1;
}
static char* modelBody(unsigned value)
{
 xstrbuf text={0};CHECK(xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf// 中文😀\r\n\r\nfunc identity(int value) as int\r\n return value\r\nend\r\n")));
 for(unsigned i=0;i<512;++i){
  const char* expression=i%2 ? "identity(local)" : "local";
  CHECK(xrtStrBufAppendFormat(&text,"func work_%u(int value) as int\r\n int local = value+%u\r\n if local > 0 then\r\n",i,i));
  for(unsigned j=0;j<12;++j)CHECK(xrtStrBufAppendFormat(&text," local = %s+1\r\n",expression));
  CHECK(xrtStrBufAppend(&text,xrtStrView(" else\r\n")));
  for(unsigned j=0;j<12;++j)CHECK(xrtStrBufAppendFormat(&text," local = %s-1\r\n",expression));
  CHECK(xrtStrBufAppend(&text,xrtStrView(" end\r\n return local\r\nend\r\n")));
 }
 CHECK(xrtStrBufAppendFormat(&text,"export func compute(int value) as int\r\n return identity(value)+%u\r\nend\r\n\nfunc main(...) as int\n return compute(3)\nend\n",value));
 char* result=xrtStrDup(text.Data);xrtStrBufFree(&text);return result;
}
static int extras(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    if(!!a->source!=!!b->source || (a->source && (a->source->completionQuery!=b->source->completionQuery ||
       a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd)))return 0;
    if(a->semanticReceiverSymbolId!=b->semanticReceiverSymbolId || a->compiledPackageError!=b->compiledPackageError ||
       a->compiledPackageRoot!=b->compiledPackageRoot || !sameText(a->compiledPackageLink,b->compiledPackageLink))return 0;
    if(!extras(a->inferredType,b->inferredType) || !extras(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!extras(a->children[i],b->children[i]))return 0;
    return 1;
}
static int indexRange(xls_index_range a,xls_index_range b)
{return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn;}
static int modelComplete(xls_semantic_check* result,const xls_analysis_document* doc)
{
    /* The compatibility JSON index exceeds its original 32MiB output quota
     * here. Compare every native fact directly,without enlarging that quota
     * or treating two failed JSON exports as evidence of equality. */
    xls_semantic_input input={doc->path,xlsTextData(doc->text),xlsTextSize(doc->text)};
    xls_semantic_check* fresh=xlsSemanticCheckCreate(doc->path,&input,1,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
    CHECK(sameAst(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)) && extras(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)));
    char *a=xlsSemanticCheckReportJson(result,NULL),*b=xlsSemanticCheckReportJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
    const xls_semantic_index *left=xlsSemanticCheckTypedIndex(result),*right=xlsSemanticCheckTypedIndex(fresh);
    size_t count=xlsSemanticIndexCount(left);CHECK(left && right && count>90000 && count==xlsSemanticIndexCount(right));
    for(size_t i=0;i<count;++i){
        xls_index_point_fact p,q;CHECK(xlsSemanticIndexPointFact(left,i,&p) && xlsSemanticIndexPointFact(right,i,&q) && p.kind==q.kind);
        const xls_index_fact *x=&p.fact,*y=&q.fact;
        CHECK(sameText(x->kind,y->kind) && sameText(x->canonical,y->canonical));
        uint64_t xv[]={x->id,x->parent,x->source,x->selectionSource,x->declaredSymbol,x->symbol,x->targetSymbol,x->compoundOperatorSymbol,
            x->declaredType,x->referencedType,x->typeId,x->typeObjectId,x->typeAbi,x->typeObjectAbi,(uint64_t)x->typeOrigin};
        uint64_t yv[]={y->id,y->parent,y->source,y->selectionSource,y->declaredSymbol,y->symbol,y->targetSymbol,y->compoundOperatorSymbol,
            y->declaredType,y->referencedType,y->typeId,y->typeObjectId,y->typeAbi,y->typeObjectAbi,(uint64_t)y->typeOrigin};
        CHECK(!memcmp(xv,yv,sizeof(xv)) && indexRange(x->range,y->range) && indexRange(x->tokenRange,y->tokenRange) && indexRange(x->selectionRange,y->selectionRange));
    }
    a=irJson(result);b=irJson(fresh);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);
    CHECK(xlsSemanticCheckValidate(result) && xlsSemanticCheckValidate(fresh));xlsSemanticCheckDestroy(fresh);return 1;
}
static int acquire(const xls_sem_memo* memo,size_t slot,int expected,int fromModel)
{
 int model=9;xls_typed_function* function=xlsSemNativeAcquire(memo,slot,&model);
 CHECK(!!function==expected);CHECK(model==(expected ? fromModel : 0));xlsTypedFunctionRelease(function);return 1;
}
static int rejectedOwners(xls_sem_memo* memo,const xls_typed_module* owner,const xls_typed_module* wrong,size_t slot)
{
 xarray entries={0};CHECK(xrtArrayInit(&entries,sizeof(xls_sem_entry)) && xrtArrayResize(&entries,memo->entries.Count));
 memcpy(entries.Data,memo->entries.Data,memo->entries.Count*sizeof(xls_sem_entry));xls_sem_memo fake=*memo;fake.entries=entries;
 xls_raw_scope raw;xlsRawBodyScopeBegin(&raw,&fake,32u*1024u*1024u);raw.ordinary=1;xls_model_source_control control={.memo=&fake};raw.modelSource=&control;raw.nativeSource=owner;
 CHECK(acquire(&fake,slot,1,1));CHECK(acquire(&fake,SIZE_MAX,0,0));
 control.memo=memo;CHECK(acquire(&fake,slot,0,0));control.memo=&fake;
 raw.nativeSource=NULL;CHECK(acquire(&fake,slot,0,0));raw.nativeSource=wrong;CHECK(acquire(&fake,slot,0,0));raw.nativeSource=owner;
 raw.ordinary=0;CHECK(acquire(&fake,slot,0,0));raw.ordinary=1;
 fake.finalizedRoot=xlsTypedModuleRoot(wrong);CHECK(acquire(&fake,slot,0,0));fake.finalizedRoot=memo->finalizedRoot;
 fake.inputOwner=xlsTypedModuleIdentity(wrong);CHECK(acquire(&fake,slot,0,0));fake.inputOwner=memo->inputOwner;
 fake.finalContext=NULL;CHECK(acquire(&fake,slot,0,0));fake.finalContext=memo->finalContext;
 xls_sem_entry* selected=xrtArrayGet(&fake.entries,slot);xls_sem_ir_receipt* receipt=selected->finalInput;
 selected->finalInput=NULL;CHECK(acquire(&fake,slot,0,0));selected->finalInput=receipt;
 unsigned plan=selected->finalPlanValid;selected->finalPlanValid=0;CHECK(acquire(&fake,slot,0,0));selected->finalPlanValid=plan;
 --fake.entries.Count;CHECK(acquire(&fake,slot,0,0));++fake.entries.Count;
 /* Unchanged optional cache lookup remains independent of model-source mode. */
 size_t cached=SIZE_MAX;for(size_t i=0;i<memo->entries.Count;++i)if(((xls_sem_entry*)memo->entries.Data)[i].native){cached=i;break;}
 CHECK(cached!=SIZE_MAX);raw.nativeSource=NULL;CHECK(acquire(memo,cached,1,0));
 xlsRawBodyScopeEnd(&raw);xrtArrayUnit(&entries);return 1;
}
static int rejectWritable(const xls_sem_memo* memo,const xls_sem_entry* reference)
{
 XL6_AstNode *root=xl6AstNew(XL6_AST_MODULE,NULL),*function=xl6AstNew(XL6_AST_FUNC,NULL),*marker=xl6AstNewText(XL6_AST_TYPE,"cstring",7,NULL);
 CHECK(root && function && marker);xl6AstAddChild(function,marker);CHECK(function->childCount==1);xl6AstAddChild(root,function);CHECK(root->childCount==1);
 xls_typed_module* owner=xlsTypedModuleTake(root);CHECK(owner);xls_sem_entry entry=*reference;entry.native=NULL;entry.nativeModelLease=entry.nativePlaced=0;
 xls_sem_memo fake={.finalizedRoot=root,.inputOwner=xlsTypedModuleIdentity(owner),.finalContext=memo->finalContext};
 CHECK(fake.inputOwner && xrtArrayInit(&fake.entries,sizeof(entry)) && xrtArrayPush(&fake.entries,&entry));
 xls_raw_scope raw;xlsRawBodyScopeBegin(&raw,&fake,32u*1024u*1024u);raw.ordinary=1;xls_model_source_control control={.memo=&fake};raw.modelSource=&control;raw.nativeSource=owner;
 CHECK(!xlsSemNativeCOwnedReadOnly(function,0));CHECK(acquire(&fake,0,0,0));
 xlsRawBodyScopeEnd(&raw);xrtArrayUnit(&fake.entries);xlsTypedModuleRelease(owner);return 1;
}
static int prefixAssets(const char* path,const char* source,const char* changed)
{
 xls_text *text=xlsTextCreate(source,strlen(source),2u*1024u*1024u,NULL),*next=xlsTextCreate(changed,strlen(changed),2u*1024u*1024u,NULL);CHECK(text && next);
 xls_parse_work parsed={0};xls_syntax* before=xlsSyntaxBuild(path,text,NULL,&parsed);CHECK(before && xlsSyntaxAccepted(before));
 xls_semantic_input input={path,source,strlen(source)};char error[256];
 xls_semantic_check* check=xlsSemanticCheckCreateCachedInContext(NULL,NULL,path,&input,1,1,before,NULL,NULL,NULL,error,sizeof(error));CHECK(check && xlsSemanticCheckInfo(check).accepted);
 xls_sem_memo* memo=xlsFrontendSemMemoRetain(capturedMemo);const xls_typed_module* owner=xlsFrontendCheckFunctionModule(check);CHECK(memo && owner && memo->raw && memo->raw->bytes<=32u*1024u*1024u);
 CHECK(memo->raw->prefixUnits.Count==memo->entries.Count && *(xls_raw_unit**)xrtArrayGet(&memo->raw->prefixUnits,0));
 size_t slot=SIZE_MAX;for(size_t i=0;i<memo->entries.Count;++i){const xls_sem_entry* e=xrtArrayConstGet(&memo->entries,i);if(!e->native && e->finalInput && e->finalPlanValid && *(xls_raw_fact* const*)xrtArrayConstGet(&memo->raw->entries,i)){slot=i;break;}}
 CHECK(slot!=SIZE_MAX);const xls_raw_fact* fact=*(xls_raw_fact* const*)xrtArrayConstGet(&memo->raw->entries,slot);const xls_sem_entry* entry=xrtArrayConstGet(&memo->entries,slot);
 xls_raw_scope raw;xlsRawBodyScopeBegin(&raw,memo,32u*1024u*1024u);raw.ordinary=1;xls_model_source_control control={.memo=memo};raw.modelSource=&control;raw.nativeSource=owner;
 size_t prefix=xlsRawBodyModelPrefix(&raw,before),bytes=0;CHECK(prefix>slot && prefix<=memo->entries.Count);
 CHECK(!xlsRawBodyMemoAdmit(memo,fact->unit,path,&entry->lexicalRange,&bytes));control.prefixActive=1;control.prefixEnd=prefix;
 CHECK(xlsRawBodyMemoAdmit(memo,fact->unit,path,&entry->lexicalRange,&bytes));control.prefixEnd=slot;CHECK(!xlsRawBodyMemoAdmit(memo,fact->unit,path,&entry->lexicalRange,&bytes));
 xls_syntax* literal=xlsSyntaxTryFunction(path,next,before,&parsed);CHECK(literal && xlsSyntaxAccepted(literal));CHECK(xlsRawBodyModelPrefix(&raw,literal)>slot);
 char* early=xrtStrDup(source);CHECK(early);char* token=strstr(early," int local = value+0");CHECK(token);token[strlen(" int local = value+")]='2';
 xls_text* edit=xlsTextCreate(early,strlen(early),2u*1024u*1024u,NULL);CHECK(edit);xls_syntax* different=xlsSyntaxTryFunction(path,edit,before,&parsed);CHECK(different && xlsSyntaxAccepted(different));
 size_t changedSlot=SIZE_MAX;for(size_t i=0;i<xlsRawBodySyntaxUnitCount(before);++i)if(xlsRawBodySyntaxUnitAt(before,i)!=xlsRawBodySyntaxUnitAt(different,i)){
  CHECK(xlsRawMemoFind(memo->raw,xlsRawBodySyntaxUnitAt(before,i),&changedSlot));break;}
 CHECK(changedSlot!=SIZE_MAX && changedSlot<slot);CHECK(xlsRawBodyModelPrefix(&raw,different)>slot);
 /* Same-input changes are still validated by the original semantic guards.
  * A different input cannot use this exemption,even with identical bytes. */
 char foreignPath[4096];CHECK(snprintf(foreignPath,sizeof(foreignPath),"%s.foreign.xl",path)>0);
 xls_syntax* foreign=xlsSyntaxBuild(foreignPath,text,NULL,&parsed);CHECK(foreign && xlsSyntaxAccepted(foreign));
 CHECK(!xlsRawBodyModelPrefix(&raw,foreign));control.prefixEnd=0;CHECK(!xlsRawBodyMemoAdmit(memo,fact->unit,path,&entry->lexicalRange,&bytes));xlsSyntaxRelease(foreign);
 xlsRawBodyScopeEnd(&raw);
 /* Previously parsed inputs can establish the prefix without another syntax
  * body. Register real copied nodes/units through the existing producer. */
 XL6_ParseResult copied;xl6ParseResultInit(&copied);CHECK(xlsSyntaxCopyParse(before,&copied));
 xlsRawBodyScopeBegin(&raw,memo,32u*1024u*1024u);raw.ordinary=1;raw.modelSource=&control;raw.nativeSource=owner;
 xlsRawBodySyntaxRegister(before,copied.root,&raw);CHECK(raw.ready && raw.bindings.Count);CHECK(xlsRawBodyModelPrefix(&raw,NULL)>slot);
 xlsRawBodyScopeEnd(&raw);xl6ParseResultFree(&copied);xlsSyntaxRelease(before);xlsSyntaxRelease(literal);xlsSyntaxRelease(different);xlsTextRelease(text);xlsTextRelease(next);xlsTextRelease(edit);xrtFree(early);xlsSemanticCheckDestroy(check);xlsFrontendSemMemoDestroy(memo);return 1;
}
static int runModel(void)
{
 xls_frontend_context_scope context;xlsFrontendContextBegin(&context,NULL);
 char* first=modelBody(0),*second=modelBody(1),*third=modelBody(2),*fourth=modelBody(3),*path=xrtPathAbs("model-source-中文😀.xl");CHECK(first && second && third && fourth && path);
 CHECK(prefixAssets(path,first,second));
 xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,1,1};
 xls_semantic_check* a=update(session,&doc,1,first);CHECK(a && xlsSemanticCheckInfo(a).accepted && modelComplete(a,&doc));
 xls_sem_memo* memo=xlsFrontendSemMemoRetain(capturedMemo);const xls_typed_module* owner=xlsFrontendCheckFunctionModule(a);CHECK(memo && owner && memo->finalizedRoot==xlsTypedModuleRoot(owner));
 size_t slot=SIZE_MAX;for(size_t i=0;i<memo->entries.Count;++i){const xls_sem_entry* entry=xrtArrayConstGet(&memo->entries,i);
  if(!entry->native && entry->unit && entry->unit->proof && entry->finalInput && entry->finalPlanValid){xls_typed_function* f=xlsTypedModuleFunction(owner,i);if(f){xlsTypedFunctionRelease(f);slot=i;break;}}}
 CHECK(slot!=SIZE_MAX);xls_raw_scope raw;xlsRawBodyScopeBegin(&raw,memo,32u*1024u*1024u);raw.ordinary=1;xls_model_source_control sourceControl={.memo=memo};raw.modelSource=&sourceControl;raw.nativeSource=owner;
  int leased=0;xls_typed_function* held=xlsSemNativeAcquire(memo,slot,&leased);CHECK(held && leased==1 && xlsTypedFunctionRoot(held)==xlsTypedModuleRoot(owner)->children[slot]);xlsRawBodyScopeEnd(&raw);
 xls_check_session* otherSession=xlsCheckSessionCreate(NULL);CHECK(otherSession);xls_analysis_document otherDoc={"entry",path,NULL,1,1};
 xls_semantic_check* other=update(otherSession,&otherDoc,1,first);CHECK(other && xlsSemanticCheckInfo(other).accepted);
 CHECK(rejectedOwners(memo,owner,xlsFrontendCheckFunctionModule(other),slot));CHECK(rejectWritable(memo,xrtArrayConstGet(&memo->entries,slot)));CHECK(checkPublicationSource(memo,owner,xlsFrontendCheckFunctionModule(other),slot,held));
 xls_semantic_check* b=update(session,&doc,1,second);CHECK(b && xlsSemanticCheckInfo(b).accepted && modelComplete(b,&doc));
 xls_sem_native_work native=xlsSemanticCheckNativeWork(b);xls_ast_materialize_work copied=xlsSemanticCheckMaterializationWork(b);
 CHECK(native.bytes<=32u*1024u*1024u && native.reused>native.retained && !native.retries);
 for(size_t i=0;i<capturedMemo->entries.Count;++i)CHECK(!((const xls_sem_entry*)capturedMemo->entries.Data)[i].nativeModelLease);
 CHECK(xlsSemanticCheckAst(a)->children[slot]==xlsSemanticCheckAst(b)->children[slot]);
 printf("model-source retained=%llu reused=%llu cache-bytes=%zu copied=%llu\n",(unsigned long long)native.retained,(unsigned long long)native.reused,native.bytes,(unsigned long long)copied.nodes);
 denyComposition=1;xls_semantic_check* c=update(session,&doc,1,third);
 CHECK(c && !denyComposition && deniedModelLeases>0 && xlsSemanticCheckNativeWork(c).retries==1 && modelComplete(c,&doc));
 CHECK(xlsSemanticCheckValidate(b));
 xls_text* pending=xlsTextCreate(fourth,strlen(fourth),8u*1024u*1024u,NULL);CHECK(pending);xlsTextRelease(doc.text);doc.text=pending;++doc.revision;
 cancelComposition=1;modelCancelledFlag=0;char error[256];xls_semantic_check* cancelled=xlsCheckSessionCheck(session,&doc,1,"entry",1,modelCancelled,NULL,error,sizeof(error));
 CHECK(!cancelled && !cancelComposition && cancelledModelLeases>0 && strstr(error,"cancel") && xlsSemanticCheckValidate(c));
 modelCancelledFlag=0;xls_semantic_check* d=xlsCheckSessionCheck(session,&doc,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(d && xlsSemanticCheckInfo(d).accepted && modelComplete(d,&doc));
 for(size_t i=0;i<capturedMemo->entries.Count;++i)CHECK(!((const xls_sem_entry*)capturedMemo->entries.Data)[i].nativeModelLease);
 xlsSemanticCheckDestroy(c);xlsSemanticCheckDestroy(d);
 xlsSemanticCheckDestroy(a);xlsSemanticCheckDestroy(b);xlsSemanticCheckDestroy(other);xlsCheckSessionDestroy(session);xlsCheckSessionDestroy(otherSession);
 xlsFrontendSemMemoDestroy(memo);xlsFrontendSemMemoDestroy(capturedMemo);capturedMemo=NULL;xlsTextRelease(doc.text);xlsTextRelease(otherDoc.text);
 const XL6_AstNode* live=xlsTypedFunctionRoot(held);CHECK(live && live->kind==XL6_AST_FUNC && live->childCount>3 && live->children[3] && live->children[3]->childCount);
 xlsTypedFunctionRelease(held);xrtFree(first);xrtFree(second);xrtFree(third);xrtFree(fourth);xrtFree(path);CHECK(xlsFrontendContextEnd(&context));return 1;
}
int main(void)
{
 xrtAtomic32Init(&checks,0);if(!runModel())return 1;
 printf("analysis_model_source_test passed:%u checks;complete fresh AST/index/IR,uncached model function ownership,source/ID/readonly guards,construction publication,OOM/cancel retry,32MiB cache and independent lifetime\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
#else
int main(void){puts("analysis_model_source_test skipped:old native representation");return 0;}
#endif
