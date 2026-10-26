#include "analysis/ast_clone.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ast_leaf_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
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
static XL6_AstNode* forest(size_t count,int distinct)
{
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);if(!root)return NULL;
 for(size_t i=0;i<count;++i){char name[48];snprintf(name,sizeof(name),"type_%zu",distinct ? i%80 : 0);XL6_AstNode* type=xl6AstNewText(XL6_AST_TYPE,name,strlen(name),NULL);size_t before=root->childCount;if(type)xl6AstAddChild(root,type);if(!type || root->childCount!=before+1){xl6AstFree(type);xl6AstFree(root);return NULL;}}
 return root;
}
typedef struct transform_state {unsigned calls,failAt;int alternate;} transform_state;
static int transform(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{transform_state* state=user;++state->calls;if(state->alternate && in->kind==XL6_AST_TYPE && state->calls%3==0){if(state->alternate==2)out->semanticTypeAbiId^=UINT64_C(1)<<61;else out->flags=1;}return state->calls!=state->failAt;}
static int inheritPrivate(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{(void)user;out->xlsLeafReferences=in->xlsLeafReferences+42;out->xlsStorage=in->xlsStorage;return 1;}
static int cases(void)
{
 XL6_AstNode* input=forest(1000,0);CHECK(input);uint64 baseline=live();
 for(unsigned pool=0;pool<2;++pool){xls_ast_leaf_work leaves;xls_ast_pool_work work;size_t bytes=0;XL6_AstNode* out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,pool ? 1024u*1024u : 0,&work,&leaves,&bytes);CHECK(out && equal(input,out) && leaves.entries==1 && leaves.hits==999 && leaves.keyBytes<512 && out->children[0]==out->children[999] && out->children[0]->xlsLeafReferences==1000);if(pool)CHECK(work.nodes==2 && work.pages==1);else CHECK(!work.nodes);
  XL6_AstNode* ordinary=xlsAstClone(out,NULL,NULL);CHECK(ordinary && equal(out,ordinary) && ordinary->children[0]!=ordinary->children[1] && !ordinary->children[0]->xlsLeafReferences);ordinary->children[0]->flags=1;CHECK(!out->children[0]->flags);xl6AstFree(ordinary);
  ordinary=xlsAstClone(out,&(xls_ast_clone_options){.transform=inheritPrivate},NULL);CHECK(ordinary && equal(out,ordinary) && !ordinary->xlsLeafReferences && !ordinary->children[0]->xlsLeafReferences);xl6AstFree(ordinary);
  xl6AstFree(out->children[0]);out->children[0]=NULL;CHECK(out->children[1]->xlsLeafReferences==999 && !strcmp(out->children[1]->text,"type_0"));xl6AstFree(out);CHECK(live()==baseline);
 }
 xl6AstFree(input);
 /* Nested equal leaves may receive different parent identities during the
    compiler's class-handle finalization. Only independent fact roots share. */
 input=forest(4,0);CHECK(input);
 for(unsigned i=2;i<4;++i){XL6_AstNode* parent=input->children[i];XL6_AstNode* child=xl6AstNewText(XL6_AST_TYPE,"type_0",6,NULL);CHECK(child);xl6AstAddChild(parent,child);CHECK(parent->childCount==1);parent->flags=XL6_AST_FLAG_CLASS_HANDLE;parent->semanticTypeId=100+i;}
 baseline=live();xls_ast_leaf_work nested;XL6_AstNode* nestedOut=xlsAstCloneFactTypes(input,NULL,NULL,NULL,1024*1024,NULL,&nested,NULL);CHECK(nestedOut && equal(input,nestedOut) && nested.entries==1 && nested.hits==1);
 CHECK(nestedOut->children[0]==nestedOut->children[1] && nestedOut->children[2]->children[0]!=nestedOut->children[0] && nestedOut->children[3]->children[0]!=nestedOut->children[2]->children[0]);
 nestedOut->children[2]->children[0]->semanticTypeId=102;CHECK(!nestedOut->children[3]->children[0]->semanticTypeId && !nestedOut->children[0]->semanticTypeId);xl6AstFree(nestedOut);CHECK(live()==baseline);xl6AstFree(input);
 input=forest(160,1);CHECK(input);baseline=live();xls_ast_leaf_work leaves;XL6_AstNode* out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,1024u*1024u,NULL,&leaves,NULL);CHECK(out && equal(input,out) && leaves.entries==64 && leaves.hits==64);CHECK(out->children[0]==out->children[80] && out->children[64]!=out->children[144]);xl6AstFree(out);CHECK(live()==baseline);xl6AstFree(input);
 input=forest(16,0);CHECK(input);
 for(unsigned i=0;i<16;++i){input->children[i]->semanticTypeId=1+i%2;input->children[i]->semanticTypeAbiId=(UINT64_C(1)<<61)*(i%3);}
 baseline=live();out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,0,NULL,&leaves,NULL);CHECK(out && equal(input,out) && leaves.entries==6 && leaves.hits==10);
 CHECK(out->children[0]==out->children[6] && out->children[0]!=out->children[1] && out->children[0]!=out->children[2]);xl6AstFree(out);CHECK(live()==baseline);
 for(unsigned mode=0;mode<3;++mode){transform_state a={.alternate=(int)mode},b=a;xls_ast_clone_options options={transform,&a,1024*1024,0,NULL};XL6_AstNode* expected=xlsAstClone(input,&options,NULL);options.user=&b;out=xlsAstCloneFactTypes(input,&options,NULL,NULL,1024*1024,NULL,&leaves,NULL);CHECK(expected && out && equal(expected,out) && a.calls==17 && b.calls==17);xl6AstFree(expected);xl6AstFree(out);CHECK(live()==baseline);}
 xl6AstFree(input);input=forest(16,0);CHECK(input);baseline=live();
 for(unsigned i=0;i<16;++i){input->children[i]->source=xrtCalloc(1,sizeof(XL6_AstSource));CHECK(input->children[i]->source);input->children[i]->source->references=1;input->children[i]->source->path=xrtStrDup("/memory/叶子😀.xl");CHECK(input->children[i]->source->path);}
 baseline=live();out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,0,NULL,&leaves,NULL);CHECK(out && equal(input,out) && !leaves.hits && leaves.entries==16);xl6AstFree(out);CHECK(live()==baseline);
 for(unsigned i=0;i<16;++i)input->children[i]->source->completionQuery=1;
 out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,0,NULL,&leaves,NULL);CHECK(out && equal(input,out) && !leaves.hits && !leaves.entries);xl6AstFree(out);CHECK(live()==baseline);xl6AstFree(input);
 input=forest(16,0);CHECK(input);baseline=live();
 for(unsigned mode=0;mode<2;++mode){transform_state a={.alternate=(int)mode},b=a;xls_ast_clone_options options={transform,&a,1024*1024,0,NULL};XL6_AstNode* expected=xlsAstClone(input,&options,NULL);options.user=&b;out=xlsAstCloneFactTypes(input,&options,NULL,NULL,1024*1024,NULL,&leaves,NULL);CHECK(expected && out && equal(expected,out) && a.calls==17 && b.calls==17);xl6AstFree(expected);xl6AstFree(out);CHECK(live()==baseline);}
 for(unsigned at=1;at<=17;++at){transform_state state={.failAt=at};xls_ast_clone_options options={transform,&state,1024*1024,0,NULL};out=xlsAstCloneFactTypes(input,&options,NULL,NULL,1024*1024,NULL,&leaves,NULL);CHECK(!out && state.calls==at && live()==baseline);}
 /* Warm XRT's thread error state. Small heap spans may already be retained,
    so count actual backing allocator operations rather than inventing fault
    points for cached logical allocations. Pool pages still exercise failures. */
 CHECK(!xrtPoolInitLayout(NULL,0,0,0));baseline=live();
 for(unsigned pool=0;pool<2;++pool){allocation.recording=1;allocation.failAt=UINT64_MAX;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,pool ? 1024*1024 : 0,NULL,&leaves,NULL);allocation.recording=0;uint64 calls=xrtAtomic64Load(&allocation.calls,XMEMORY_RELAXED);CHECK(out && equal(input,out));if(pool)CHECK(calls>0);xl6AstFree(out);CHECK(live()==baseline);
  for(uint64 at=1;at<=calls;++at){allocation.recording=1;allocation.failAt=at;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,pool ? 1024*1024 : 0,NULL,&leaves,NULL);allocation.recording=0;CHECK(!out || equal(input,out));xl6AstFree(out);CHECK(live()==baseline);}
 }
 CHECK(faultsHit>0);
 for(unsigned i=0;i<16;++i)input->children[i]->range.startOffset=1;
 out=xlsAstCloneFactTypes(input,NULL,NULL,NULL,0,NULL,&leaves,NULL);CHECK(out && equal(input,out) && !leaves.entries && !leaves.hits);xl6AstFree(out);CHECK(live()==baseline);xl6AstFree(input);return 1;
}
int main(void)
{
 xrtAtomic64Init(&allocation.live,0);xrtAtomic64Init(&allocation.calls,0);xrtGetAllocator(&allocation.original);xallocator custom={&allocation,trackedAlloc,trackedRealloc,trackedFree};if(!xrtSetAllocator(&custom))return 1;xrtMemStatsEnable(true);if(!cases())return 1;printf("analysis_ast_leaf_test passed: %u checks;heap/pool exact typed leaves,independent ordinary clone,edge release,source/marker/coordinate separation,64-key limit,callback order/rejection and observed backing allocator faults=%llu with logical live-block balance\n",checks,(unsigned long long)faultsHit);return 0;
}
