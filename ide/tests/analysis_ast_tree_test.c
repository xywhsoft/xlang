#include "analysis/ast_clone.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ast_tree_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct allocator_state {xallocator original;xatomic64 live,calls;uint64 failAt;int recording;} allocator_state;
static allocator_state allocation;
static uint64 faultsHit;
static int fail(void){int hit=allocation.recording && xrtAtomic64FetchAdd(&allocation.calls,1,XMEMORY_RELAXED)+1==allocation.failAt;if(hit)++faultsHit;return hit;}
static void* trackedAlloc(void* user,size_t size){(void)user;if(fail())return NULL;void* p=allocation.original.Alloc(allocation.original.Context,size);if(p)xrtAtomic64FetchAdd(&allocation.live,1,XMEMORY_RELAXED);return p;}
static void trackedFree(void* user,void* p){(void)user;if(p){if(!xrtAtomic64FetchSub(&allocation.live,1,XMEMORY_RELAXED))abort();allocation.original.Free(allocation.original.Context,p);}}
static void* trackedRealloc(void* user,void* p,size_t size){(void)user;if(!size){trackedFree(NULL,p);return NULL;}if(fail())return NULL;void* value=allocation.original.Realloc(allocation.original.Context,p,size);if(value && !p)xrtAtomic64FetchAdd(&allocation.live,1,XMEMORY_RELAXED);return value;}
static uint64 live(void){xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static int equal(const XL6_AstNode* a,const XL6_AstNode* b)
{
 if(!a || !b)return a==b;
 XL6_AstNode x=*a,y=*b;
 x.text=y.text=x.doc=y.doc=NULL;x.literalData=y.literalData=NULL;x.children=y.children=NULL;x.inferredType=y.inferredType=x.semanticDeclaredType=y.semanticDeclaredType=NULL;x.source=y.source=NULL;x.childCapacity=y.childCapacity=0;x.xlsStorage=y.xlsStorage=NULL;x.xlsLeafReferences=y.xlsLeafReferences=0;
 if(memcmp(&x,&y,sizeof(x)) || (a->text && b->text ? strcmp(a->text,b->text)!=0 : a->text!=b->text) || (a->doc && b->doc ? strcmp(a->doc,b->doc)!=0 : a->doc!=b->doc))return 0;
 if(!!a->source!=!!b->source)return 0;
 if(a->source && (a->source->completionQuery!=b->source->completionQuery || a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd || strcmp(a->source->path,b->source->path)))return 0;
 if(!equal(a->inferredType,b->inferredType) || !equal(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
 for(size_t i=0;i<a->childCount;++i)if(!equal(a->children[i],b->children[i]))return 0;
 return 1;
}

static _Thread_local unsigned initializations,rootSets,failInit,failSet;
static _Thread_local int failAfterInsert;
static _Thread_local uintptr_t rootMap;
static bool gateInit(xmap* map,size_t size)
{++initializations;if(initializations==3)rootMap=(uintptr_t)map;return initializations==failInit ? false : xrtMapInit(map,size);}
static bool gateSet(xmap* map,xbytesview key,const void* value)
{
 if((uintptr_t)map==rootMap && ++rootSets==failSet){if(failAfterInsert && !xrtMapSet(map,key,value))return false;return false;}
 return xrtMapSet(map,key,value);
}
#define xrtMapInit gateInit
#define xrtMapSet gateSet
#ifndef XLS_AST_TREE_IMPLEMENTATION
#define XLS_AST_TREE_IMPLEMENTATION "../src/analysis/ast_clone.c"
#endif
#include XLS_AST_TREE_IMPLEMENTATION
#undef xrtMapInit
#undef xrtMapSet
static void reset(void){initializations=rootSets=failInit=failSet=0;failAfterInsert=0;rootMap=0;}
static size_t treeNodes(const XL6_AstNode* n)
{if(!n)return 0;size_t count=1+treeNodes(n->inferredType)+treeNodes(n->semanticDeclaredType);for(size_t i=0;i<n->childCount;++i)count+=treeNodes(n->children[i]);return count;}
static XL6_AstNode* makeType(void)
{
 XL6_AstNode* root=xl6AstNewText(XL6_AST_TYPE,"Outer",5,NULL);if(!root)return NULL;
 root->line=2;root->column=3;root->range=(XL6_AstRange){1,3,11,2,3,2,11};root->tokenRange=root->range;root->semanticTypeId=7;root->semanticTypeAbiId=99;
 root->doc=xrtStrDup("complex type");root->inferredType=xl6AstNewText(XL6_AST_TYPE,"nested inferred",15,NULL);root->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"nested declared",15,NULL);
 if(!root->doc || !root->inferredType || !root->semanticDeclaredType)goto failed;
 for(unsigned i=0;i<3;++i){XL6_AstNode* child=xl6AstNewText(XL6_AST_TYPE,"Inner",5,NULL);if(!child)goto failed;child->semanticTypeId=i+20;child->flags=i+1;
  if(!xl6AstSetLiteralData(child,"x\0y",3)){xl6AstFree(child);goto failed;}size_t before=root->childCount;xl6AstAddChild(root,child);if(root->childCount!=before+1){xl6AstFree(child);goto failed;}}
 if(!xl6AstSetSourceTree(root,"/folder/类型😀.xl"))goto failed;
 return root;
failed:xl6AstFree(root);return NULL;
}
typedef struct transform_state {size_t calls;int varying,rejectReplay;XL6_AstSource* target;unsigned targetCalls;} transform_state;
static int transformTree(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{transform_state* state=user;++state->calls;if(in->kind==XL6_AST_TYPE)out->line+=state->varying ? (int)state->calls : 7;return 1;}
static XL6_AstSource* sourceTarget(const XL6_AstSource* in,void* user)
{(void)in;transform_state* state=user;++state->targetCalls;return state->target;}
static size_t mark(void* user){return ((transform_state*)user)->calls;}
static int replay(void* user,size_t begin,size_t count)
{transform_state* s=user;if(s->rejectReplay || s->varying || begin>s->calls || count>s->calls-begin || count>SIZE_MAX-s->calls)return 0;s->calls+=count;return 1;}
static const xls_ast_tree_projection projection={mark,replay};
static int cases(void)
{
 XL6_AstNode* input=makeType();CHECK(input);XL6_AstNode* roots[8];for(unsigned i=0;i<8;++i)roots[i]=input;XL6_AstNode forest={.kind=XL6_AST_MODULE,.children=roots,.childCount=8};
 CHECK(!xrtPoolInitLayout(NULL,0,0,0));uint64 baseline=live();size_t count=treeNodes(input);
 for(unsigned pooled=0;pooled<2;++pooled)for(unsigned certified=0;certified<2;++certified){
  reset();transform_state a={0},b={0};xls_ast_clone_options opts={transformTree,&a,2u*1024u*1024u,0,NULL};XL6_AstNode* expected=xlsAstCloneFactTypes(&forest,&opts,NULL,NULL,0,NULL,NULL,NULL);CHECK(expected);
  opts.user=&b;xls_ast_tree_work work;xls_ast_pool_work pool;XL6_AstNode* out=xlsAstCloneFactTrees(&forest,&opts,certified ? &projection : NULL,pooled ? 2u*1024u*1024u : 0,&pool,&work,NULL);
  CHECK(out && equal(expected,out) && a.calls==b.calls && b.calls==1+8*count && work.entries==1 && work.hits==7 && work.allocatedNodes==1+count);
  CHECK(certified ? work.certifiedNodes==7*count && !work.validatedNodes : work.validatedNodes==7*count && !work.certifiedNodes);
  CHECK(out->children[0]==out->children[7] && out->children[0]!=input && out->children[0]->xlsLeafReferences==8);
  XL6_AstNode* independent=xlsAstClone(out,NULL,NULL);CHECK(independent && equal(out,independent) && independent->children[0]!=independent->children[1]);independent->children[0]->children[0]->semanticTypeId^=1;CHECK(independent->children[0]->children[0]->semanticTypeId!=out->children[0]->children[0]->semanticTypeId);xl6AstFree(independent);
  if(pooled)CHECK(pool.nodes==count+1 && pool.pages==1);
  xl6AstFree(expected);xl6AstFree(out);CHECK(live()==baseline);
 }
 /* Arbitrary scalar callbacks still run in exact order. Diverging aliases
  * reject the attempt;the caller can restore its state and use ordinary copy. */
 for(unsigned cert=0;cert<2;++cert){reset();transform_state state={.varying=!cert,.rejectReplay=(int)cert};xls_ast_clone_options opts={transformTree,&state,2u*1024u*1024u,0,NULL};CHECK(!xlsAstCloneFactTrees(&forest,&opts,cert ? &projection : NULL,0,NULL,NULL,NULL));CHECK(live()==baseline);state=(transform_state){.varying=!cert};XL6_AstNode* fallback=xlsAstCloneFactTypes(&forest,&opts,NULL,NULL,0,NULL,NULL,NULL);CHECK(fallback);xl6AstFree(fallback);CHECK(live()==baseline);}
 /* Same source projection memo as the original copier;shared edges own no
  * additional source reference and never change the immutable input source. */
 XL6_AstNode* anchor=xl6AstNew(XL6_AST_TYPE,NULL);CHECK(anchor && xl6AstSetSourceTree(anchor,"/folder/类型😀.xl"));uint64 withAnchor=live();size_t inputRefs=input->source->references,anchorRefs=anchor->source->references;
 transform_state a={.target=anchor->source},b=a;xls_ast_clone_options opts={transformTree,&a,2u*1024u*1024u,0,sourceTarget};XL6_AstNode* expected=xlsAstCloneFactTypes(&forest,&opts,NULL,NULL,0,NULL,NULL,NULL);opts.user=&b;XL6_AstNode* out=xlsAstCloneFactTrees(&forest,&opts,NULL,0,NULL,NULL,NULL);CHECK(expected && out && equal(expected,out) && a.targetCalls==b.targetCalls && a.calls==b.calls && input->source->references==inputRefs);xl6AstFree(out);xl6AstFree(expected);CHECK(anchor->source->references==anchorRefs && live()==withAnchor);xl6AstFree(anchor);CHECK(live()==baseline);
 for(unsigned pooled=0;pooled<2;++pooled){
  for(unsigned which=1;which<=3;++which){reset();failInit=which;out=xlsAstCloneFactTrees(&forest,NULL,NULL,pooled ? 2u*1024u*1024u : 0,NULL,NULL,NULL);if(which==1)CHECK(!out);else CHECK(out && equal(&forest,out));xl6AstFree(out);CHECK(live()==baseline);}
  for(unsigned after=0;after<2;++after){reset();failSet=1;failAfterInsert=(int)after;out=xlsAstCloneFactTrees(&forest,NULL,NULL,pooled ? 2u*1024u*1024u : 0,NULL,NULL,NULL);CHECK(out && rootSets==1 && equal(&forest,out));xl6AstFree(out);CHECK(live()==baseline);}
 }
 reset();CHECK(!xlsAstCloneFactTrees(&forest,&(xls_ast_clone_options){.bytes=1},NULL,0,NULL,NULL,NULL) && live()==baseline);
 /* An unresolved target nested in two distinct handle roots stays private;
  * only identical DIRECT roots can alias,never a shared nested target. */
 XL6_AstNode* other=xlsAstClone(input,NULL,NULL);CHECK(other);other->semanticTypeId=8;roots[1]=other;reset();out=xlsAstCloneFactTrees(&forest,NULL,NULL,0,NULL,NULL,NULL);CHECK(out && out->children[0]!=out->children[1] && out->children[0]->children[0]!=out->children[1]->children[0]);out->children[0]->children[0]->semanticTypeId=111;CHECK(out->children[1]->children[0]->semanticTypeId!=111);xl6AstFree(out);xl6AstFree(other);roots[1]=input;CHECK(live()==baseline);
 /* The retained pointer index stops at256 independent keys;remaining roots
  * still copy correctly and never manufacture aliases by type-label equality. */
 XL6_AstNode* limited[514];for(unsigned i=0;i<257;++i){XL6_AstNode* n=xl6AstNewText(XL6_AST_TYPE,"limited",7,NULL);CHECK(n);n->line=1;n->semanticTypeId=i+1;limited[2*i]=limited[2*i+1]=n;}
 XL6_AstNode large={.kind=XL6_AST_MODULE,.children=limited,.childCount=514};reset();xls_ast_tree_work limitedWork;out=xlsAstCloneFactTrees(&large,NULL,NULL,0,NULL,&limitedWork,NULL);CHECK(out && equal(&large,out) && limitedWork.entries==256 && limitedWork.hits==256 && limitedWork.allocatedNodes==259);
 for(unsigned i=0;i<257;++i)CHECK((out->children[2*i]==out->children[2*i+1])==(i<256));
 xl6AstFree(out);for(unsigned i=0;i<257;++i)xl6AstFree(limited[2*i]);CHECK(live()==baseline);
 /* Observe actual backing allocator failures,not invented per-malloc points. */
 for(unsigned pooled=0;pooled<2;++pooled){reset();allocation.recording=1;allocation.failAt=UINT64_MAX;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);out=xlsAstCloneFactTrees(&forest,NULL,NULL,pooled ? 2u*1024u*1024u : 0,NULL,NULL,NULL);allocation.recording=0;uint64 operations=xrtAtomic64Load(&allocation.calls,XMEMORY_RELAXED);CHECK(out);xl6AstFree(out);CHECK(live()==baseline);
  for(uint64 at=1;at<=operations;++at){reset();allocation.recording=1;allocation.failAt=at;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);out=xlsAstCloneFactTrees(&forest,NULL,NULL,pooled ? 2u*1024u*1024u : 0,NULL,NULL,NULL);allocation.recording=0;CHECK(!out || equal(&forest,out));xl6AstFree(out);CHECK(live()==baseline);}
 }
 allocation.failAt=0;xl6AstFree(input);return 1;
}
typedef struct reader {const XL6_AstNode* input;unsigned failures;} reader;
static int32 readClones(void* user)
{reader* r=user;for(unsigned i=0;i<80;++i){XL6_AstNode* items[4]={(XL6_AstNode*)r->input,(XL6_AstNode*)r->input,(XL6_AstNode*)r->input,(XL6_AstNode*)r->input};XL6_AstNode forest={.kind=XL6_AST_MODULE,.children=items,.childCount=4};reset();XL6_AstNode* out=xlsAstCloneFactTrees(&forest,NULL,NULL,0,NULL,NULL,NULL);if(!out || !equal(&forest,out) || out->children[0]==r->input || out->children[0]!=out->children[3])++r->failures;xl6AstFree(out);}return 0;}
static int readers(void)
{XL6_AstNode* input=makeType();CHECK(input);XL6_AstNode snapshot=*input;size_t references=input->source->references;reader data[4];xthread* threads[4];for(unsigned i=0;i<4;++i){data[i]=(reader){input,0};threads[i]=xrtThreadCreate(readClones,&data[i],0);CHECK(threads[i]);}for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !data[i].failures);xrtThreadDestroy(threads[i]);}CHECK(!memcmp(&snapshot,input,sizeof(snapshot)) && input->source->references==references);xl6AstFree(input);return 1;}
int main(void)
{xrtAtomic64Init(&allocation.live,0);xrtAtomic64Init(&allocation.calls,0);xrtGetAllocator(&allocation.original);xallocator custom={&allocation,trackedAlloc,trackedRealloc,trackedFree};if(!xrtSetAllocator(&custom))return 1;xrtMemStatsEnable(true);if(!cases() || !readers())return 1;printf("analysis_ast_tree_test passed: %u checks;complex direct-root aliases,full callback order/certificates,rejection/caller retry,source targets,heap/pool release,independent clones,optional index failures and backing faults=%llu;320 concurrent independent clones\n",checks,(unsigned long long)faultsHit);return 0;}
