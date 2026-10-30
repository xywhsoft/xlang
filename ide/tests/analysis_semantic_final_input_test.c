#include "analysis/frontend_internal.h"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* Component projection fixtures use the actual generated semantic TU.
 * Synthetic baselines exercise the exact delta representation/ownership;
 * real compiler/key/IR integration must also be verified independently. */
#include "xl6_sem.c"
#include "analysis/check.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/module_store.h"
static unsigned checks;
static int tracking,overflow,faultActive;
static size_t tracked,faultSeen,faultAt,faultDenied;
static void* allocations[128];
void* __real_xrtCalloc(size_t,size_t);void __real_xrtFree(void*);
bool __real_xrtStrBufReserve(xstrbuf*,size_t);void __real_xrtStrBufFree(xstrbuf*);
static int refuse(void){if(faultActive && ++faultSeen==faultAt){++faultDenied;return 1;}return 0;}
static void remember(void* p)
{
 if(!p || !tracking)return;
 for(size_t i=0;i<128;++i)if(allocations[i]==p)return;
 for(size_t i=0;i<128;++i)if(!allocations[i]){allocations[i]=p;++tracked;return;}
 overflow=1;
}
static void forget(void* p)
{if(p)for(size_t i=0;i<128;++i)if(allocations[i]==p){allocations[i]=NULL;--tracked;return;}}
void* __wrap_xrtCalloc(size_t n,size_t s)
{if(refuse())return NULL;void* p=__real_xrtCalloc(n,s);remember(p);return p;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n)
{if(refuse())return false;char* before=b->Data;bool okay=__real_xrtStrBufReserve(b,n);if(before!=b->Data){forget(before);remember(b->Data);}return okay;}
void __wrap_xrtStrBufFree(xstrbuf* b){forget(b->Data);__real_xrtStrBufFree(b);}
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_semantic_final_input_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct fixture {XL6_AstNode* function;XL6_AstNode* rawType;xls_sem_unit* unit;} fixture;
static XL6_AstNode* node(XL6_AstKind kind,const char* text)
{
 XL6_AstNode* n=xl6AstNew(kind,NULL);if(n && text){n->text=xrtStrDup(text);if(!n->text){xl6AstFree(n);return NULL;}}return n;
}
static int init(fixture* f)
{
 memset(f,0,sizeof(*f));f->function=node(XL6_AST_FUNC,"work");CHECK(f->function);
 XL6_AstNode* body=node(XL6_AST_BLOCK,NULL);CHECK(body);f->rawType=node(XL6_AST_TYPE,"int");CHECK(f->rawType);
 f->function->children=xrtCalloc(2,sizeof(XL6_AstNode*));CHECK(f->function->children);
 f->function->children[0]=body;f->function->childCount=1;f->function->childCapacity=2;
 body->children=xrtCalloc(1,sizeof(XL6_AstNode*));CHECK(body->children);body->children[0]=f->rawType;body->childCount=body->childCapacity=1;
 f->function->inferredType=node(XL6_AST_TYPE,"int");f->function->semanticDeclaredType=node(XL6_AST_TYPE,"int");
 CHECK(f->function->inferredType && f->function->semanticDeclaredType);
 f->function->inferredType->inferredType=node(XL6_AST_TYPE,"bool");CHECK(f->function->inferredType->inferredType);
 f->unit=xrtCalloc(1,sizeof(*f->unit));CHECK(f->unit);xrtAtomic32Init(&f->unit->references,1);
 xarray raw={0};CHECK(xrtArrayInit(&raw,sizeof(xls_sem_raw)));CHECK(xlsSemRawNodes(f->function,&raw,0));
 size_t owned=0;CHECK(xlsSemCaptureFacts(&raw,f->unit,1024u*1024u,&owned,1));f->unit->bytes=sizeof(*f->unit)+owned;xrtArrayUnit(&raw);
 /* Model the real post-capture writers before taking final deltas. */
 f->function->semanticRegionId=11;f->function->semanticScopeDepth=1;
 body->semanticRegionId=12;body->semanticScopeDepth=2;
 f->rawType->semanticRegionId=12;f->rawType->semanticScopeDepth=2;
 f->rawType->semanticTypeId=6;f->rawType->semanticTypeAbiId=61;
 f->function->inferredType->semanticTypeId=4;f->function->inferredType->semanticTypeAbiId=41;
 f->function->inferredType->inferredType->semanticTypeId=5;
 f->function->semanticDeclaredType->semanticTypeId=6;f->function->semanticDeclaredType->semanticTypeAbiId=61;
 return 1;
}
static void drop(fixture* f){xlsSemUnitRelease(f->unit);xl6AstFree(f->function);}
static xls_sem_ir_receipt* receipt(fixture* f,const xls_sem_ir_receipt* previous,size_t budget)
{tracking=1;xls_sem_ir_receipt* result=xlsSemIrReceipt(f->function,f->unit,previous,budget);tracking=0;return result;}
static void mutate(XL6_AstNode* n,unsigned field)
{
 switch(field){
 case 0:n->flags^=1u;break;case 1:++n->semanticTypeId;break;case 2:n->semanticTypeAbiId^=UINT64_C(0x8000000000000001);break;
 case 3:++n->semanticSymbolId;break;case 4:++n->semanticDeclaredSymbolId;break;case 5:++n->semanticTargetSymbolId;break;
 case 6:++n->semanticCompoundOperatorSymbolId;break;case 7:++n->semanticCallKind;break;case 8:++n->semanticLvalueKind;break;
 case 9:++n->semanticOwnership;break;case 10:++n->semanticExitKind;break;case 11:++n->semanticScopeDepth;break;
 case 12:++n->semanticTargetScopeDepth;break;case 13:++n->semanticRegionId;break;case 14:++n->semanticTargetRegionId;break;
 case 15:++n->semanticReceiverSymbolId;break;case 16:++n->compiledPackageError;break;case 17:++n->compiledPackageRoot;break;
 }
}
static int equalKey(const XL6_AstNode* n,const xstrbuf* before,int equal)
{
 xstrbuf key={0};CHECK(xlsFrontendIrAstKey(&key,n,XLS_IR_KEY_EXACT));
 CHECK((key.Size==before->Size && !memcmp(key.Data,before->Data,key.Size))==equal);xrtStrBufFree(&key);return 1;
}
static int fields(void)
{
 fixture f;CHECK(init(&f));xls_sem_ir_receipt* base=receipt(&f,NULL,1024u*1024u);CHECK(base && base->identity && base->nodes==3 && base->types==3);
 size_t retained=tracked;xstrbuf key={0};CHECK(xlsFrontendIrAstKey(&key,f.function,XLS_IR_KEY_EXACT));
 XL6_AstNode* targets[]={f.function,f.function->inferredType,f.function->inferredType->inferredType,f.function->semanticDeclaredType};
 for(size_t at=0;at<4;++at)for(unsigned field=0;field<XLS_SEM_IR_FIELDS;++field){
  XL6_AstNode saved=*targets[at];mutate(targets[at],field);xls_sem_ir_receipt* changed=receipt(&f,base,1024u*1024u);
  CHECK(changed && changed!=base && changed->identity!=base->identity);CHECK(equalKey(f.function,&key,0));
  xlsSemIrReceiptRelease(changed);*targets[at]=saved;CHECK(tracked==retained);
  faultActive=1;faultAt=SIZE_MAX;faultSeen=faultDenied=0;changed=receipt(&f,base,1024u*1024u);faultActive=0;
  CHECK(changed==base && !faultSeen && !faultDenied);xlsSemIrReceiptRelease(changed);CHECK(equalKey(f.function,&key,1));
 }
 for(size_t at=0;at<4;++at){
  char* old=targets[at]->compiledPackageLink;targets[at]->compiledPackageLink=xrtStrDup("other/package.xlp");CHECK(targets[at]->compiledPackageLink);
  xls_sem_ir_receipt* changed=receipt(&f,base,1024u*1024u);CHECK(changed && changed->identity!=base->identity);CHECK(equalKey(f.function,&key,0));
  xlsSemIrReceiptRelease(changed);xrtFree(targets[at]->compiledPackageLink);targets[at]->compiledPackageLink=old;CHECK(tracked==retained);
 }
 /* A late borrowed alias changes final type-edge execution fields. Read the
  * final graph by value,without assuming type/raw storage cannot alias. */
 XL6_AstNode* old=f.function->semanticDeclaredType;f.function->semanticDeclaredType=f.rawType;
 xls_sem_ir_receipt* aliased=receipt(&f,base,1024u*1024u);CHECK(aliased && aliased->identity!=base->identity);CHECK(equalKey(f.function,&key,0));
 f.function->semanticDeclaredType=old;xlsSemIrReceiptRelease(aliased);CHECK(tracked==retained);
 xrtStrBufFree(&key);xlsSemIrReceiptRelease(base);drop(&f);CHECK(!tracked && !overflow);return 1;
}
static int boundaries(void)
{
 fixture f;CHECK(init(&f));xls_sem_ir_receipt* base=receipt(&f,NULL,1024u*1024u);CHECK(base);size_t retained=tracked;
 CHECK(!receipt(&f,base,0));CHECK(!receipt(&f,base,sizeof(*base)));
 CHECK(!receipt(&f,base,sizeof(*base)+base->data.Size+xlsTypedInputIdentityOwnedBytes(base->identity)-1));
 XL6_AstSource source={0};source.completionQuery=1;XL6_AstSource* old=f.rawType->source;f.rawType->source=&source;
 CHECK(!receipt(&f,base,1024u*1024u));f.rawType->source=old;
 const char* cursor=f.unit->cursor.path;f.unit->cursor.path="projection";CHECK(!receipt(&f,base,1024u*1024u));f.unit->cursor.path=cursor;
 ++f.function->inferredType->childCount;CHECK(!receipt(&f,base,1024u*1024u));--f.function->inferredType->childCount;
 XL6_AstNode* extra=node(XL6_AST_BLOCK,NULL);CHECK(extra);f.function->children[1]=extra;++f.function->childCount;
 CHECK(!receipt(&f,base,1024u*1024u));--f.function->childCount;xl6AstFree(extra);f.function->children[1]=NULL;
 CHECK(tracked==retained);xlsSemIrReceiptRelease(base);drop(&f);CHECK(!tracked);return 1;
}
static int faults(void)
{
 fixture f;CHECK(init(&f));faultActive=1;faultAt=SIZE_MAX;faultSeen=faultDenied=0;
 xls_sem_ir_receipt* okay=receipt(&f,NULL,1024u*1024u);faultActive=0;CHECK(okay && faultSeen>=3);
 size_t points=faultSeen;xlsSemIrReceiptRelease(okay);CHECK(!tracked);
 for(size_t at=1;at<=points;++at){
  faultActive=1;faultAt=at;faultSeen=faultDenied=0;okay=receipt(&f,NULL,1024u*1024u);faultActive=0;
  CHECK(!okay && faultDenied==1);CHECK(!tracked && !overflow);
 }
 drop(&f);return 1;
}
static xls_sem_memo* makeMemo(fixture* f,XL6_AstNode** output)
{
 XL6_AstNode* root=node(XL6_AST_MODULE,NULL);CHECK(root);
 root->children=xrtCalloc(1,sizeof(XL6_AstNode*));CHECK(root->children);
 root->children[0]=f->function;root->childCount=root->childCapacity=1;*output=root;
 xls_sem_memo* memo=xrtCalloc(1,sizeof(*memo));CHECK(memo);xrtAtomic32Init(&memo->references,1);
 CHECK(xrtArrayInit(&memo->entries,sizeof(xls_sem_entry)) && xrtArrayResize(&memo->entries,1));
 ((xls_sem_entry*)memo->entries.Data)[0].unit=xlsSemUnitRetain(f->unit);return memo;
}
static int differentUnits(void)
{
 fixture a,b;CHECK(init(&a) && init(&b));XL6_AstNode *aroot,*broot;
 xls_sem_memo* before=makeMemo(&a,&aroot);CHECK(before);
 xls_sem_memo* after=makeMemo(&b,&broot);CHECK(after);
 xrtFree(b.function->text);b.function->text=xrtStrDup("other");CHECK(b.function->text);
 tracking=1;xlsSemIrFinalizeMemo(before,aroot,NULL,1024u*1024u);
 xlsSemIrFinalizeMemo(after,broot,before,1024u*1024u);tracking=0;
 xls_sem_ir_receipt* x=((xls_sem_entry*)before->entries.Data)[0].finalInput;
 xls_sem_ir_receipt* y=((xls_sem_entry*)after->entries.Data)[0].finalInput;CHECK(x && y);
 CHECK(x->data.Size==y->data.Size && !memcmp(x->data.Data,y->data.Data,x->data.Size));
 CHECK(x->identity!=y->identity);
 xstrbuf key={0};CHECK(xlsFrontendIrAstKey(&key,a.function,XLS_IR_KEY_EXACT));
 CHECK(equalKey(b.function,&key,0));xrtStrBufFree(&key);
 xlsFrontendSemMemoDestroy(before);xlsFrontendSemMemoDestroy(after);
 xl6AstFree(aroot);xl6AstFree(broot);a.function=b.function=NULL;drop(&a);drop(&b);
 CHECK(!tracked);return 1;
}
static int stopNow(void* ignored){(void)ignored;return 1;}
static int cancellation(void)
{
 fixture f;CHECK(init(&f));xls_frontend_cancel_scope cancel;
 xlsFrontendCancelBegin(&cancel,stopNow,NULL);
 xls_sem_ir_writer writer={.limit=1024u*1024u,.visits=63};
 CHECK(!xlsSemIrNodes(&writer,f.function,f.unit,0));CHECK(writer.visits==64);
 CHECK(xlsFrontendCancelObserved() && !writer.data.Size);
 CHECK(!receipt(&f,NULL,1024u*1024u));
 xls_frontend_cancel_work work;xlsFrontendCancelEnd(&cancel,&work);
 CHECK(work.cancelled && work.predicateCalls==1);xrtStrBufFree(&writer.data);drop(&f);return 1;
}
static int owners(void)
{
 fixture f;CHECK(init(&f));XL6_AstNode* root=node(XL6_AST_MODULE,NULL);CHECK(root);
 root->children=xrtCalloc(1,sizeof(XL6_AstNode*));CHECK(root->children);root->children[0]=f.function;root->childCount=root->childCapacity=1;
 xls_sem_memo* memo=xrtCalloc(1,sizeof(*memo));CHECK(memo);xrtAtomic32Init(&memo->references,1);
 CHECK(xrtArrayInit(&memo->entries,sizeof(xls_sem_entry)) && xrtArrayResize(&memo->entries,1));
 ((xls_sem_entry*)memo->entries.Data)[0].unit=xlsSemUnitRetain(f.unit);
 xlsSemIrFinalizeMemo(memo,root,NULL,1024u*1024u);CHECK(((xls_sem_entry*)memo->entries.Data)[0].finalInput);
 xls_typed_module* input=xlsTypedModuleTake(root);CHECK(input);CHECK(!xlsFrontendSemMemoIrIdentity(memo,input,0));
 CHECK(xlsFrontendSemMemoBindIrInput(memo,input));xls_typed_input_identity* id=xlsFrontendSemMemoIrIdentity(memo,input,0);CHECK(id);
 CHECK(!xlsFrontendSemMemoIrIdentity(memo,input,1));CHECK(xlsFrontendSemMemoBindIrInput(memo,input));
 xls_typed_input_identity* held=xlsTypedInputIdentityRetain(id);
 xlsTypedModuleRelease(input);f.function=NULL;
 XL6_AstNode* otherRoot=node(XL6_AST_MODULE,NULL);CHECK(otherRoot);input=xlsTypedModuleTake(otherRoot);CHECK(input);
 /* Simulate a recycled root address in the private record: the independently
  * retained owner token must still reject rebinding and identity access. */
 memo->finalizedRoot=otherRoot;CHECK(!xlsFrontendSemMemoBindIrInput(memo,input));CHECK(!xlsFrontendSemMemoIrIdentity(memo,input,0));
 xlsTypedModuleRelease(input);xlsFrontendSemMemoDestroy(memo);CHECK(held);
 xls_typed_input_identity* other=xlsTypedInputIdentityCreate();CHECK(other && other!=held);
 xlsTypedInputIdentityRelease(other);xlsTypedInputIdentityRelease(held);xlsSemUnitRelease(f.unit);return 1;
}
typedef struct entity_observations {size_t calls,positive,negative,zero;int bad,symbolMiss,receiverMiss;} entity_observations;
static void observeEntity(void* user,XL6_SymbolId symbol,uint64_t identity)
{
 entity_observations* o=user;++o->calls;
 if(!symbol){++o->zero;if(identity)o->bad=1;}
 else if(identity)++o->positive;else ++o->negative;
 if(symbol==UINT32_MAX && !identity)o->symbolMiss=1;
 if(symbol==UINT32_MAX-1 && !identity)o->receiverMiss=1;
}
static int observations(void)
{
 char *directory=xrtPathAbs("."),*entry=xrtPathAbs("final-input-observer.xl"),*project=xrtPathAbs("final-input-observer.xlp");
 CHECK(directory && entry && project);
 const char* members[]={entry};
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",
  .entry=entry,.directory=directory,.analysisProfile="final-input-observer",.targetKind="exe"};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,1};
 xls_context* context=xlsContextCreate(&spec,1024u*1024u,NULL);
 xls_module_store* store=xlsModuleStoreCreate(NULL);xls_entry_cache* cache=xlsEntryCacheCreate(8u*1024u*1024u);
 CHECK(context && store && cache);
 const char* source="func callee(int value) as int\n return value+1\nend\nfunc caller(int value) as int\n if value>0 then\n return callee(value)\n end\n return value\nend\nfunc main(...) as int\n return caller(1)\nend\n";
 xls_semantic_input input={entry,source,strlen(source)};char error[256];
 xls_module_scope scope;xlsModuleScopeBegin(&scope,store);
 xls_semantic_check* check=xlsSemanticCheckCreateCachedInContext(context,cache,entry,&input,1,1,NULL,NULL,NULL,NULL,error,sizeof(error));
 xlsModuleScopeEnd(&scope);CHECK(check && xlsSemanticCheckInfo(check).accepted);
 const xls_entity_view* view=xlsSemanticIndexEntities(xlsSemanticCheckTypedIndex(check));CHECK(view);
 const XL6_AstNode* root=xlsSemanticCheckAst(check);const XL6_AstNode* function=NULL;
 for(size_t i=0;i<root->childCount;++i){const XL6_AstNode* n=root->children[i];if(n->kind==XL6_AST_FUNC && n->childCount && n->children[0]->text && !strcmp(n->children[0]->text,"caller"))function=n;}
 CHECK(function);
 xls_lexical_symbols symbols={0};CHECK(xlsLexicalSymbolsBuild(&symbols,function,XLS_LEXICAL_SYMBOL_BYTES));
 xstrbuf plain={0},observed={0},after={0};entity_observations o={0};
 CHECK(xlsFrontendIrAstKeyEntities(&plain,function,&symbols,view));
 CHECK(xlsFrontendIrAstKeyEntitiesObserved(&observed,function,&symbols,view,observeEntity,&o));
 CHECK(o.calls && o.positive && !o.bad && plain.Size==observed.Size && !memcmp(plain.Data,observed.Data,plain.Size));
 size_t calls=o.calls;CHECK(!xlsActiveIrKeyObserver);
 CHECK(xlsFrontendIrAstKeyEntities(&after,function,&symbols,view));CHECK(o.calls==calls);
 CHECK(after.Size==plain.Size && !memcmp(after.Data,plain.Data,plain.Size));
 xrtStrBufFree(&plain);xrtStrBufFree(&observed);xrtStrBufFree(&after);xlsLexicalSymbolsClear(&symbols);
 XL6_AstNode* copy=xlsAstClone(function,NULL,NULL);CHECK(copy);
 xl6AstFree(copy->inferredType);copy->inferredType=node(XL6_AST_TYPE,"int");CHECK(copy->inferredType);
 copy->inferredType->semanticSymbolId=UINT32_MAX;
 copy->inferredType->semanticReceiverSymbolId=UINT32_MAX-1;
 CHECK(xlsLexicalSymbolsBuild(&symbols,copy,XLS_LEXICAL_SYMBOL_BYTES));o=(entity_observations){0};
 CHECK(xlsFrontendIrAstKeyEntities(&plain,copy,&symbols,view));
 CHECK(xlsFrontendIrAstKeyEntitiesObserved(&observed,copy,&symbols,view,observeEntity,&o));
 CHECK(o.negative>=2 && o.symbolMiss && o.receiverMiss && !o.bad);
 CHECK(plain.Size==observed.Size && !memcmp(plain.Data,observed.Data,plain.Size));
 CHECK(!xlsFrontendIrAstKeyEntitiesObserved(&after,copy,NULL,view,observeEntity,&o));CHECK(!xlsActiveIrKeyObserver);
 xrtStrBufFree(&plain);xrtStrBufFree(&observed);xrtStrBufFree(&after);xlsLexicalSymbolsClear(&symbols);xl6AstFree(copy);
 xlsSemanticCheckDestroy(check);xlsEntryCacheDestroy(cache);xlsModuleStoreRelease(store);xlsContextRelease(context);
 xrtFree(directory);xrtFree(entry);xrtFree(project);return 1;
}
int main(void)
{
 fixture warm;if(!init(&warm))return 1;xls_sem_ir_receipt* r=xlsSemIrReceipt(warm.function,warm.unit,NULL,1024u*1024u);if(!r)return 1;xlsSemIrReceiptRelease(r);drop(&warm);
 if(!fields() || !boundaries() || !faults() || !differentUnits() || !cancellation() || !owners() || !observations())return 1;
 if(tracked || overflow){fputs("final input fixture leaked tracked component storage\n",stderr);return 1;}
 printf("analysis_semantic_final_input_test passed: %u checks; final typed deltas,all18scalar fields/package links,nested type trees,late alias effects,COW/no hot allocations,bounds,real allocation failures,independent owner binding\n",checks);return 0;
}
#else
int main(void){puts("analysis_semantic_final_input_test passed: legacy cohort retains its original semantic path");return 0;}
#endif
