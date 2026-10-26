#include "analysis/ast_clone.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,initializations,optionalSets,sourceSets,failInit,failSet,failSource;
static int afterInsert;
/* Address token is compared only while the measured call is active;never
   dereference or retain its stack map outside that construction. */
static uintptr_t optional;
static bool gateInit(xmap* map,size_t size)
{++initializations;if(initializations==2)optional=(uintptr_t)map;if(initializations==failInit)return false;return xrtMapInit(map,size);}
static bool gateSet(xmap* map,xbytesview key,const void* value)
{
 if((uintptr_t)map==optional && ++optionalSets==failSet){if(afterInsert && !xrtMapSet(map,key,value))return false;return false;}
 if((uintptr_t)map!=optional && ++sourceSets==failSource)return false;
 return xrtMapSet(map,key,value);
}
/* Same current implementation,with deterministic optional-map failures at
 * the actual API boundary. No observation/fault hooks in production. */
#define xrtMapInit gateInit
#define xrtMapSet gateSet
#include "analysis/ast_clone.c"
#undef xrtMapInit
#undef xrtMapSet
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ast_leaf_failure_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64 live(void){xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static void reset(void){initializations=optionalSets=sourceSets=failInit=failSet=failSource=0;afterInsert=0;optional=0;}
static int run(void)
{
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
 for(unsigned i=0;i<48;++i){char text[16];snprintf(text,sizeof(text),"type%u",i/12);XL6_AstNode* type=xl6AstNewText(XL6_AST_TYPE,text,strlen(text),NULL);CHECK(type);size_t before=root->childCount;xl6AstAddChild(root,type);CHECK(root->childCount==before+1);}
 CHECK(!xrtPoolInitLayout(NULL,0,0,0));uint64 baseline=live();
 for(unsigned pool=0;pool<2;++pool)for(unsigned mode=0;mode<3;++mode)for(unsigned at=1;at<=4;++at){reset();if(mode==0)failInit=2;else{failSet=at;afterInsert=mode==2;}xls_ast_leaf_work work;XL6_AstNode* out=xlsAstCloneFactTypes(root,NULL,NULL,NULL,pool ? 1024*1024 : 0,NULL,&work,NULL);CHECK(out && out->childCount==48);
  for(unsigned i=0;i<48;++i)CHECK(out->children[i]->kind==root->children[i]->kind && !strcmp(out->children[i]->text,root->children[i]->text));
  if(mode==0)CHECK(!work.entries && !work.hits);else CHECK(optionalSets==at);
  xl6AstFree(out);CHECK(live()==baseline);
 }
 reset();failInit=1;CHECK(!xlsAstCloneFactTypes(root,NULL,NULL,NULL,0,NULL,NULL,NULL) && live()==baseline);
 CHECK(xl6AstSetSourceTree(root,"/memory/failure😀.xl"));baseline=live();
 for(unsigned pool=0;pool<2;++pool){reset();failSource=1;CHECK(!xlsAstCloneFactTypes(root,NULL,NULL,NULL,pool ? 1024*1024 : 0,NULL,NULL,NULL) && sourceSets==1 && live()==baseline);}
 reset();xl6AstFree(root);return 1;
}
int main(void){xrtMemStatsEnable(true);if(!run())return 1;printf("analysis_ast_leaf_failure_test passed: %u checks;optional map init/before/after publication failure fallback,prior shared edges,source-required rejection and balanced heap/pool lifetime\n",checks);return 0;}
