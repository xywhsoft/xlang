#include "analysis/frontend_index.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
/* Compile the real generated collector in a private namespace. Fault only
 * its object/arena allocations,not the compiler/runtime or borrowed AST. */
static void* allocations[64];
static size_t live,operations,failAt;
static int badLifetime;
static void remember(void* p)
{ if(p) { if(live==64) badLifetime=1; else allocations[live++]=p; } }
static size_t tracked(void* p)
{ for(size_t i=0;i<live;++i) if(allocations[i]==p) return i; return SIZE_MAX; }
static void* objectCalloc(size_t count,size_t bytes)
{ if(++operations==failAt) return NULL; void* p=xrtCalloc(count,bytes); remember(p); return p; }
static void* arenaRealloc(void* p,size_t bytes)
{
 size_t slot=p ? tracked(p) : SIZE_MAX;
 if(p && slot==SIZE_MAX) badLifetime=1;
 if(++operations==failAt) return NULL;
 void* next=xrtRealloc(p,bytes);
 if(next) { if(slot!=SIZE_MAX) allocations[slot]=next; else remember(next); }
 return next;
}
static void objectFree(void* p)
{ size_t slot=tracked(p); if(slot!=SIZE_MAX) allocations[slot]=allocations[--live]; xrtFree(p); }
#define xrtCalloc objectCalloc
#define xrtRealloc arenaRealloc
#define xrtFree objectFree
#define xlsFrontendIndexCreate storageCreate
#define xlsFrontendIndexDestroy storageDestroy
#define xlsFrontendIndexBudgetUsed storageBudget
#define xlsSemanticIndexCount storageCount
#define xlsSemanticIndexFact storageFact
#define xl6SemIndexJson storageJson
#define xl6SemCBridgeMapJson storageBridgeJson
#include "xl6_sem_index.c"
#undef xrtCalloc
#undef xrtRealloc
#undef xrtFree
#undef xlsFrontendIndexCreate
#undef xlsFrontendIndexDestroy
#undef xlsFrontendIndexBudgetUsed
#undef xlsSemanticIndexCount
#undef xlsSemanticIndexFact
#undef xl6SemIndexJson
#undef xl6SemCBridgeMapJson
static unsigned checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"analysis_index_storage_test:%d: %s\n",__LINE__,#v); return 0; } } while(0)
static int run(void)
{
 char* path=xrtPathAbs("index-storage.xl"); CHECK(path);
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL); CHECK(root && xl6AstSetSourceTree(root,path));
 for(unsigned i=0;i<6000;++i) {
  XL6_AstNode* child=xl6AstNewText(XL6_AST_NAME,"name",4,NULL);
  CHECK(child && xl6AstSetSourceTree(child,path)); xl6AstAddChild(root,child); CHECK(root->childCount==i+1);
 }
 XL6_SourceReport report; xl6SourceReportInit(&report); CHECK(xl6SourceReportText(&report,path,"",0,"memory"));
 char error[256]={0}; XL6_IndexBuild sentinel={0}; xlsTypedIndexBuild=&sentinel;
 xls_semantic_index* index=storageCreate(root,&report,error,sizeof(error));
 CHECK(index && storageCount(index)==6001 && storageBudget(index)<32u*1024u*1024u && xlsTypedIndexBuild==&sentinel);
 size_t used=operations; CHECK(used>20 && !badLifetime); storageDestroy(index); CHECK(!live);
 for(size_t fault=1;fault<=used;++fault) {
  operations=0; failAt=fault; index=storageCreate(root,&report,error,sizeof(error));
  CHECK(!index && *error && !live && !badLifetime && xlsTypedIndexBuild==&sentinel && root->childCount==6000);
  CHECK(fault==1 || strstr(error,"cannot grow semantic index"));
  operations=failAt=0; index=storageCreate(root,&report,error,sizeof(error));
  CHECK(index && storageCount(index)==6001 && xlsTypedIndexBuild==&sentinel); storageDestroy(index); CHECK(!live);
 }
 xlsTypedIndexBuild=NULL; operations=failAt=0;
 XL6_IndexBuild bounded={0};
 bounded.budget=XL6_INDEX_BYTES-sizeof(XL6_IndexFact);
 CHECK(xlsIndexCapacity(&bounded,32,sizeof(XL6_IndexFact),256)==33);
 ++bounded.budget; CHECK(!xlsIndexCapacity(&bounded,32,sizeof(XL6_IndexFact),256) && strstr(bounded.error,"byte limit"));
 bounded.budget=0; CHECK(!xlsIndexCapacity(&bounded,UINT_MAX,sizeof(XL6_IndexFact),256));
 CHECK(xlsIndexNodeLimit(&bounded)==65536); xlsTypedIndexBuild=&bounded; CHECK(xlsIndexNodeLimit(&bounded)==UINT_MAX); xlsTypedIndexBuild=NULL;
 char* json=storageJson(root,&report,NULL,0); CHECK(json && strstr(json,"\"complete\":true") && !live); xrtFree(json);
 XL6_AstNode* deep=xl6AstNew(XL6_AST_MODULE,NULL),*tail=deep; CHECK(deep);
 for(unsigned i=0;i<258;++i) { XL6_AstNode* next=xl6AstNew(XL6_AST_BLOCK,NULL); CHECK(next); xl6AstAddChild(tail,next); CHECK(tail->childCount==1); tail=next; }
 index=storageCreate(deep,&report,error,sizeof(error)); CHECK(!index && strstr(error,"tree limit") && !live && !xlsTypedIndexBuild);
 xl6AstFree(deep); xl6AstFree(root); xl6SourceReportFree(&report); xrtFree(path);
 printf("index arena allocation seams=%zu,all independently failed/recovered;no live owned blocks\n",used); return 1;
}
int main(void)
{ if(!run()) return 1; printf("analysis_index_storage_test passed: %u checks; real collector growth,object/fact/source faults,cleanup,TLS restoration,depth and legacy JSON.\n",checks); return 0; }
