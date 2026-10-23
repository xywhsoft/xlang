#include "services/analysis_catalog.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do {++checks;if(!(v)) {fprintf(stderr,"analysis_catalog_test:%d %s\n",__LINE__,#v);return 0;}}while(0)
static xstrview sv(const char* s) {return (xstrview){s,strlen(s)};}
static xvalue* get(const xvalue* value,const char* key) {return xrtValueObjectGet(value,sv(key));}
static int put(xvalue* value,const char* key,xvalue* owned) {return owned && xrtValueObjectSetNew(value,sv(key),owned);}
static xls_context* project(const char* manifest,const char* entry,const char* package,const char* const* members,size_t count)
{
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=manifest,.configuration="debug",.entry=entry,.directory="D:/目录😀/project",
  .analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=package};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,count};
 return xlsContextCreate(&spec,16u*1024u*1024u,NULL);
}
static xvalue* catalog(xls_context* const* contexts,size_t count,const char* selected,const char* package)
{
 xvalue *root=xrtValueObject(),*array=xrtValueArray();int okay=root && array;
 for(size_t i=0;okay && i<count;++i) {xvalue* wire=xideAnalysisContextWrite(contexts[i]);okay=wire && xrtValueArrayAppend(array,wire);xrtValueRelease(wire);}
 okay=okay && put(root,"schema",xrtValueInt(1)) && put(root,"selectedProject",xrtValueString(sv(selected))) &&
  put(root,"packagePath",package ? xrtValueString(sv(package)) : xrtValueNull());
 if(okay) {okay=put(root,"projects",array);array=NULL;}xrtValueRelease(array);if(!okay) {xrtValueRelease(root);root=NULL;}return root;
}
static int run(void)
{
 const char* shared="D:/目录😀/project/shared.xl";const char* members[]={shared,"d:\\目录😀\\project\\SHARED.xl",shared};
 xls_context *a=project("D:/目录😀/A.xlp","D:/目录😀/project/main.xl",NULL,members,3),
  *b=project("D:/目录😀/B.xlp","D:/目录😀/project/main.xl","",members,3),
  *c=project("D:/目录😀/C.xlp","D:/目录😀/other/main.xl","packages",NULL,0);CHECK(a && b && c);
 char error[256];xvalue* wire=xideAnalysisContextWrite(a);CHECK(wire);
 xls_context* copy=xideAnalysisContextRead(wire,error,sizeof(error));CHECK(copy && xlsContextEqual(a,copy));xlsContextRelease(copy);
 CHECK(put(wire,"unknown",xrtValueInt(1)) && !xideAnalysisContextRead(wire,error,sizeof(error)));xrtValueRelease(wire);
 wire=xideAnalysisContextWrite(a);CHECK(put(get(wire,"lists"),"members",xrtValueString(sv("wrong"))) && !xideAnalysisContextRead(wire,NULL,0));xrtValueRelease(wire);
 wire=xideAnalysisContextWrite(a);CHECK(put(wire,"entry",xrtValueString((xstrview){"x\0y",3})) && !xideAnalysisContextRead(wire,error,sizeof(error)));xrtValueRelease(wire);
 wire=xideAnalysisContextWrite(b);copy=xideAnalysisContextRead(wire,error,sizeof(error));CHECK(copy && xlsContextSpec(copy)->packagePath && !*xlsContextSpec(copy)->packagePath);xlsContextRelease(copy);xrtValueRelease(wire);
 CHECK(!xideAnalysisContextRead(NULL,error,sizeof(error)) && !xideAnalysisContextWrite(NULL));
 xls_context* contexts[]={a,b,c};wire=catalog(contexts,3,"",NULL);CHECK(wire);
 xide_analysis_catalog* prepared=xideAnalysisCatalogCreate(wire,error,sizeof(error));CHECK(prepared && xideAnalysisCatalogCount(prepared)==3 && !xideAnalysisCatalogPackagePath(prepared));
 const xls_context* found=(const xls_context*)1;
 CHECK(xideAnalysisCatalogFind(prepared,shared,&found)==XIDE_ANALYSIS_AMBIGUOUS && !found);
 CHECK(xideAnalysisCatalogFind(prepared,"D:/目录😀/other/main.xl",&found)==XIDE_ANALYSIS_PROJECT && xlsContextEqual(found,c));
 CHECK(xideAnalysisCatalogFind(prepared,"relative.xl",&found)==XIDE_ANALYSIS_ERROR && !found);
 CHECK(!xideAnalysisCatalogFile(prepared,"relative.xl") && !xideAnalysisCatalogFile(prepared,"D:/folder/test.c"));
 xls_context* file=xideAnalysisCatalogFile(prepared,"D:/独立😀/main.xl");CHECK(file && !xlsContextSpec(file)->packagePath && !strcmp(xlsContextSpec(file)->directory,"D:/独立😀"));xlsContextRelease(file);
 xideAnalysisCatalogDestroy(prepared);xrtValueRelease(wire);
 wire=catalog(contexts,3,xlsContextSpec(c)->project,"");prepared=xideAnalysisCatalogCreate(wire,error,sizeof(error));CHECK(prepared && xideAnalysisCatalogFind(prepared,shared,&found)==XIDE_ANALYSIS_AMBIGUOUS);xideAnalysisCatalogDestroy(prepared);xrtValueRelease(wire);
 wire=catalog(contexts,3,xlsContextSpec(a)->project,"");prepared=xideAnalysisCatalogCreate(wire,error,sizeof(error));CHECK(prepared && xideAnalysisCatalogFind(prepared,members[1],&found)==XIDE_ANALYSIS_PROJECT && xlsContextEqual(found,a));
 CHECK(xideAnalysisCatalogPrefers(prepared,found) && !xideAnalysisCatalogPrefers(prepared,b));
 wchar_t* key=xidePathNativeWide(shared);CHECK(key && xideAnalysisCatalogFindNative(prepared,key,&found)==XIDE_ANALYSIS_PROJECT && xlsContextEqual(found,a));free(key);
 file=xideAnalysisCatalogFile(prepared,"D:/独立😀/main.xl");CHECK(file && xlsContextSpec(file)->packagePath && !*xlsContextSpec(file)->packagePath);xlsContextRelease(file);
 xide_analysis_catalog* lease=xideAnalysisCatalogRetain(prepared);xideAnalysisCatalogDestroy(prepared);xrtValueRelease(wire);xlsContextRelease(a);xlsContextRelease(b);xlsContextRelease(c);
 CHECK(xideAnalysisCatalogFind(lease,shared,&found)==XIDE_ANALYSIS_PROJECT && !strcmp(xlsContextSpec(found)->configuration,"debug"));
 wire=xideAnalysisContextWrite(found);CHECK(wire && xrtValueCount(get(get(wire,"lists"),"members"))==3);xrtValueRelease(wire);xideAnalysisCatalogDestroy(lease);
 xideAnalysisCatalogDestroy(NULL);CHECK(!xideAnalysisCatalogRetain(NULL));
 a=project("D:/a.xlp","D:/a/main.xl",NULL,NULL,0);contexts[0]=a;contexts[1]=a;
 wire=catalog(contexts,2,"",NULL);CHECK(!xideAnalysisCatalogCreate(wire,error,sizeof(error)));xrtValueRelease(wire);
 wire=catalog(contexts,1,"D:/missing.xlp",NULL);CHECK(!xideAnalysisCatalogCreate(wire,error,sizeof(error)));xrtValueRelease(wire);
 wire=catalog(contexts,1,"",NULL);CHECK(put(wire,"unknown",xrtValueInt(1)) && !xideAnalysisCatalogCreate(wire,error,sizeof(error)));xrtValueRelease(wire);
 xls_context_spec options=*xlsContextSpec(a);const char* include="D:/actual/headers";options.lists[XLS_CONTEXT_INCLUDES]=(xls_context_strings){&include,1};
 b=xlsContextCreate(&options,4096,NULL);CHECK(b);wire=xideAnalysisContextWrite(b);copy=xideAnalysisContextRead(wire,error,sizeof(error));CHECK(copy && xlsContextEqual(copy,b) && xlsContextSpec(copy)->lists[XLS_CONTEXT_INCLUDES].count==1);xlsContextRelease(copy);xlsContextRelease(b);xrtValueRelease(wire);xlsContextRelease(a);
#ifdef XLS_COMPILER_XRT
 /* Exact binding boundary and duplicate compaction on actual prepared paths. */
 const char** many=xrtCalloc(65536,sizeof(char*));CHECK(many);for(size_t i=0;i<65536;++i) many[i]="D:/bound/shared.xl";
 a=project("D:/bound.xlp","D:/bound/main.xl",NULL,many,65535);CHECK(a);contexts[0]=a;wire=catalog(contexts,1,"",NULL);prepared=xideAnalysisCatalogCreate(wire,error,sizeof(error));CHECK(prepared);
 for(size_t i=0;i<1000;++i) CHECK(xideAnalysisCatalogFind(prepared,many[0],&found)==XIDE_ANALYSIS_PROJECT && xlsContextEqual(found,a));
 xideAnalysisCatalogDestroy(prepared);xrtValueRelease(wire);xlsContextRelease(a);
 a=project("D:/bound.xlp","D:/bound/main.xl",NULL,many,65536);CHECK(a);contexts[0]=a;wire=catalog(contexts,1,"",NULL);CHECK(!xideAnalysisCatalogCreate(wire,error,sizeof(error)));xrtValueRelease(wire);xlsContextRelease(a);xrtFree(many);
#endif
 return 1;
}
int main(void) {if(!run()) return 1;printf("analysis_catalog_test passed: %u checks; exact wire identity, schema rejection, prepared native membership, ambiguity, selection, bounded lists and independent lifetimes.\n",checks);return 0;}
