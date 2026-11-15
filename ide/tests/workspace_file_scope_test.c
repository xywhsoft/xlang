#include "services/analysis_configuration.h"
#include "services/rename_scope.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_file_scope_test:%d %s\n",__LINE__,#v);return 0;}}while(0)
static const xvalue* get(const xvalue* v,const char* key){return xrtValueObjectGet(v,xrtStrView(key));}
static int put(xvalue* v,const char* key,xvalue* item){return item && xrtValueObjectSetNew(v,xrtStrView(key),item);}
static xvalue* packet(xide_workspace* w){xide_model_error e={0};return xideAnalysisConfigurationPacket(w,NULL,NULL,NULL,1,&e);}
static int run(void)
{
 /* Nonexistent paths intentionally exercise metadata without filesystem I/O. */
 xide_file files[5]={{.path="D:/scope 空间😀/main.xl"},{.path="d:\\scope 空间😀\\MAIN.xl"},
  {.path="D:/scope 空间😀/closed.xl"},{.path="D:/scope 空间😀/ignored.c"},{.path="D:/scope 空间😀/owned.xl"}};
 xide_project project={.available=1,.id="p",.manifest="D:/scope 空间😀/p.xlp",.entry=files[4].path,.kind="exe"};
 xide_workspace w={.projects=&project,.projectCount=1,.files={.items=files,.count=5}};
 xvalue* p=packet(&w);CHECK(p);const xvalue* value=get(p,"catalog");int64 schema=0;
 CHECK(xrtValueGetInt(get(value,"schema"),&schema) && schema==2 && xrtValueCount(get(value,"standaloneRoots"))==4);
 xide_analysis_catalog* c=xideAnalysisCatalogCreate(value,NULL,0);CHECK(c && xideAnalysisCatalogComplete(c));
 CHECK(xideAnalysisCatalogCount(c)==1 && xideAnalysisCatalogFileCount(c)==2 && !xideAnalysisCatalogFileAt(c,2));
 xls_context* opened=xideAnalysisCatalogFile(c,files[1].path);CHECK(opened==xideAnalysisCatalogFileAt(c,0));
 const xls_context* found=NULL;CHECK(xideAnalysisCatalogFind(c,files[1].path,&found)==XIDE_ANALYSIS_UNOWNED && !found);
 enum xide_rename_scope_error e;uint64_t owners=0;
 xide_rename_scope* s=xideRenameScopeCreate(c,&opened,1,0,NULL,NULL,&e);CHECK(s && e==XIDE_RENAME_SCOPE_OK && xideRenameScopeCount(s)==3);
 CHECK(xideRenameScopeOwners(s,files[4].path,&owners) && owners==1);
 CHECK(xideRenameScopeOwners(s,files[1].path,&owners) && owners==2);
 CHECK(xideRenameScopeOwners(s,files[2].path,&owners) && owners==4);
 CHECK(xideRenameScopeOwners(s,"D:/scope 空间😀/unlisted.xl",&owners) && !owners);
 size_t budget=xideRenameScopeBytes(s);CHECK(budget && !xideRenameScopeCreate(c,&opened,1,budget-1,NULL,NULL,&e) && e==XIDE_RENAME_SCOPE_LIMIT);
 xls_context_spec different=*xlsContextSpec(opened);different.packagePath="different";
 xls_context* conflict=xlsContextCreate(&different,4096,NULL);CHECK(conflict && !xideRenameScopeCreate(c,&conflict,1,0,NULL,NULL,&e) && e==XIDE_RENAME_SCOPE_ARGUMENT);xlsContextRelease(conflict);
 xideAnalysisCatalogDestroy(c);xrtValueRelease(p);xlsContextRelease(opened);
 CHECK(xideRenameScopeOwners(s,files[2].path,&owners) && owners==4);xideRenameScopeRelease(s);
 /* A missing project must not silently narrow whole-workspace operations. */
 project.available=0;p=packet(&w);CHECK(p);c=xideAnalysisCatalogCreate(get(p,"catalog"),NULL,0);
 CHECK(c && !xideAnalysisCatalogComplete(c) && !xideRenameScopeCreate(c,NULL,0,0,NULL,NULL,&e));
 opened=xideAnalysisCatalogFile(c,files[0].path);CHECK(opened);xlsContextRelease(opened);xideAnalysisCatalogDestroy(c);
 CHECK(put((xvalue*)get(p,"catalog"),"complete",xrtValueInt(1)) && !xideAnalysisCatalogCreate(get(p,"catalog"),NULL,0));xrtValueRelease(p);
 xide_model_error error={0};CHECK(!xideAnalysisConfigurationPacket(&w,"p",NULL,NULL,1,&error) && error.code);
 /* No loose xlang references keeps schema1, including C-only workspaces. */
 w.projects=NULL;w.projectCount=0;w.files.items=&files[3];w.files.count=1;p=packet(&w);CHECK(p);
 CHECK(xrtValueGetInt(get(get(p,"catalog"),"schema"),&schema) && schema==1);xrtValueRelease(p);
 /* Capacity is applied after project ownership/native duplicate compaction. */
 xide_file many[65]={0};char names[65][64];
 for(size_t i=0;i<65;++i){snprintf(names[i],sizeof(names[i]),"D:/roots/file-%zu.xl",i);many[i].path=names[i];}
 w.files.items=many;w.files.count=64;p=packet(&w);CHECK(p);c=xideAnalysisCatalogCreate(get(p,"catalog"),NULL,0);CHECK(c && xideAnalysisCatalogFileCount(c)==64);
 opened=xideAnalysisCatalogFile(c,names[63]);s=xideRenameScopeCreate(c,&opened,1,0,NULL,NULL,&e);
 CHECK(s && xideRenameScopeCount(s)==64 && xideRenameScopeOwners(s,names[63],&owners) && owners==(UINT64_C(1)<<63));
 xideRenameScopeRelease(s);xlsContextRelease(opened);xideAnalysisCatalogDestroy(c);xrtValueRelease(p);
 w.files.count=65;p=packet(&w);CHECK(p && !xideAnalysisCatalogCreate(get(p,"catalog"),NULL,0));xrtValueRelease(p);
 many[64].path=names[0];p=packet(&w);CHECK(p);c=xideAnalysisCatalogCreate(get(p,"catalog"),NULL,0);CHECK(c && xideAnalysisCatalogFileCount(c)==64);xideAnalysisCatalogDestroy(c);xrtValueRelease(p);
 many[64].path="relative.xl";CHECK(!packet(&w));many[64].path=names[0];
 w.files.count=1;p=packet(&w);CHECK(p);value=get(p,"catalog");
 CHECK(put((xvalue*)value,"standaloneRoots",xrtValueArray()));
 const char* invalid[]={"relative.xl","D:/roots/not-c.c","D:/roots/bad\xff.xl"};
 for(size_t i=0;i<3;++i){xvalue* roots=xrtValueArray();CHECK(roots && xrtValueArrayAppendNew(roots,xrtValueString(xrtStrView(invalid[i]))) && put((xvalue*)value,"standaloneRoots",roots));CHECK(!xideAnalysisCatalogCreate(value,NULL,0));}
 xrtValueRelease(p);CHECK(!xideAnalysisCatalogComplete(NULL) && !xideAnalysisCatalogFileCount(NULL) && !xideAnalysisCatalogFileAt(NULL,0));
 return 1;
}
int main(void){if(!run())return 1;printf("workspace_file_scope_test passed: %u checks; closed roots, native aliases, exact contexts, complete coverage, retained lifetime and union quotas.\n",checks);return 0;}
