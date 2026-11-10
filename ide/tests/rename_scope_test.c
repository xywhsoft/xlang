#include "services/rename_scope.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int denyAllocation;
static unsigned allocations;
static void* allocateScope(size_t bytes){++allocations;return denyAllocation ? NULL : xrtCalloc(1,bytes);}
#define XIDE_RENAME_SCOPE_ALLOC allocateScope
#include "services/rename_scope.c"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"rename_scope_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static int put(xvalue* v,const char* k,xvalue* x){return x && xrtValueObjectSetNew(v,sv(k),x);}
static xide_analysis_catalog* catalog(xls_context* const* contexts,size_t count,const char* selected)
{
 xvalue *root=xrtValueObject(),*array=xrtValueArray();int okay=root && array;
 for(size_t i=0;okay && i<count;++i){xvalue* v=xideAnalysisContextWrite(contexts[i]);okay=v && xrtValueArrayAppendNew(array,v);}
 okay=okay && put(root,"schema",xrtValueInt(1)) && put(root,"selectedProject",xrtValueString(sv(selected))) && put(root,"packagePath",xrtValueNull());
 if(okay){okay=put(root,"projects",array);array=NULL;}xrtValueRelease(array);
 xide_analysis_catalog* out=okay ? xideAnalysisCatalogCreate(root,NULL,0) : NULL;xrtValueRelease(root);return out;
}
static xls_context* context(int kind,const char* project,const char* entry,const char* const* members,size_t count)
{
 xls_context_spec spec={.kind=kind,.project=project,.configuration=project ? "debug" : "",.entry=entry,.directory="D:/scope",.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe"};
 spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,count};return xlsContextCreate(&spec,16u*1024u*1024u,NULL);
}
typedef struct cancellation {int calls,stop;} cancellation;
static int cancel(void* data){cancellation* c=data;return ++c->calls==c->stop;}
static int targets(xide_rename_scope* scope,const char* entry,const char* shared)
{
 const char* main="import shared\nfunc main(...) as int\n return shared.compute(1)\nend\n";
 const char* dep="export func compute(int v) as int\n return v\nend\n";
 xls_semantic_input inputs[]={{entry,main,strlen(main)},{shared,dep,strlen(dep)}};
 xls_semantic_check* check=xlsSemanticCheckCreate(entry,inputs,2,1,NULL,NULL,NULL,0);CHECK(check && xlsSemanticCheckInfo(check).accepted);
 xls_check_inputs info;CHECK(xlsSemanticCheckInputs(check,&info) && info.sources<=8);xide_semantic_source sources[8];
 for(size_t i=0;i<info.sources;++i){xls_check_source row;CHECK(xlsSemanticCheckSource(check,i,&row));sources[i]=(xide_semantic_source){row.path,row.text,row.kind,0};}
 enum xide_rename_target_error error;const char* query=strstr(main,"compute(1)");int line,column;
 CHECK(xideLspOffsetToPosition(main,(int)(query-main),&line,&column));
 xide_rename_target* target=xideRenameTargetCreate(NULL,check,sources,info.sources,entry,line,column,0,NULL,NULL,&error);CHECK(target && !error);
 xlsSemanticCheckDestroy(check);uint64_t owners=99;CHECK(xideRenameScopeTarget(scope,target,&owners) && owners==11);
 xide_analysis_catalog* empty=catalog(NULL,0,"");xide_rename_scope* unowned=xideRenameScopeCreate(empty,NULL,0,0,NULL,NULL,NULL);CHECK(unowned && xideRenameScopeTarget(unowned,target,&owners) && !owners);
 xideRenameScopeRelease(unowned);xideAnalysisCatalogDestroy(empty);xideRenameTargetRelease(target);return 1;
}
static int run(void)
{
 const char *entry="D:/scope/main.xl",*shared="D:/scope/shared.xl",*alias="d:\\scope\\SHARED.xl";
 const char* members[]={shared,alias,"D:/scope/native.c"};
 xls_context* contexts[]={context(XLS_CONTEXT_PROJECT,"D:/scope/A.xlp",entry,members,3),context(XLS_CONTEXT_PROJECT,"D:/scope/B.xlp","D:/scope/second.xl",members,3)};
 CHECK(contexts[0] && contexts[1]);xide_analysis_catalog *selected=catalog(contexts,2,"D:/scope/A.xlp"),*other=catalog(contexts,2,"D:/scope/B.xlp");CHECK(selected && other);
 const char *standalone="D:/file/😀文件.xl",*standaloneAlias="d:\\file\\😀文件.xl";const char* fileMembers[]={standaloneAlias,standalone};
 xls_context* files[]={context(XLS_CONTEXT_XLANG,NULL,standalone,fileMembers,2),context(XLS_CONTEXT_XLANG,NULL,"D:/scope/shared.xl",NULL,0)};CHECK(files[0] && files[1]);
 enum xide_rename_scope_error error;xide_rename_scope* scope=xideRenameScopeCreate(selected,files,2,0,NULL,NULL,&error);CHECK(scope && error==XIDE_RENAME_SCOPE_OK && xideRenameScopeCount(scope)==4);
 uint64_t owners=99;wchar_t* key=xidePathNativeWide(alias);CHECK(key && xideAnalysisCatalogOwnersNative(selected,key,&owners) && owners==3);
 CHECK(xideAnalysisCatalogOwnersNative(other,key,&owners) && owners==3);CHECK(xideRenameScopeOwnersNative(scope,key,&owners) && owners==11);
 const xls_context* chosen=NULL;CHECK(xideAnalysisCatalogFindNative(selected,key,&chosen)==XIDE_ANALYSIS_PROJECT && xlsContextEqual(chosen,contexts[0]));free(key);
 CHECK(xideRenameScopeOwners(scope,standaloneAlias,&owners) && owners==4);
 CHECK(xideRenameScopeOwners(scope,"D:/scope/native.c",&owners) && owners==3);
 const char* absent[]={"D:/scope/unlisted.xl","D:/scope2/shared.xl","D:/file/import.xl","D:/packages/shared.xl","D:/scope/A.xlp"};
 for(size_t i=0;i<sizeof(absent)/sizeof(*absent);++i)CHECK(xideRenameScopeOwners(scope,absent[i],&owners) && !owners);
 CHECK(!xideRenameScopeOwners(scope,"relative.xl",&owners) && !owners);
 CHECK(!xideRenameScopeOwnersNative(NULL,L"x",&owners) && !owners);CHECK(!xideRenameScopeOwnersNative(scope,NULL,&owners) && !owners);CHECK(!xideRenameScopeOwners(scope,entry,NULL));
 CHECK(!xideAnalysisCatalogOwnersNative(NULL,L"x",&owners) && !owners);CHECK(!xideAnalysisCatalogOwnersNative(selected,NULL,&owners) && !owners);
 CHECK(!xideRenameScopeTarget(scope,NULL,&owners) && !owners);CHECK(!xideRenameScopeAt(scope,4) && !xideRenameScopeAt(NULL,0));
 size_t exact=xideRenameScopeBytes(scope);CHECK(exact>sizeof(*scope)+xideAnalysisCatalogBytes(selected)+xlsContextBytes(files[0])+xlsContextBytes(files[1]) && exact<XIDE_RENAME_SCOPE_BYTES);
 xide_rename_scope* tight=xideRenameScopeCreate(selected,files,2,exact,NULL,NULL,&error);CHECK(tight && !error && xideRenameScopeBytes(tight)==exact);xideRenameScopeRelease(tight);
 CHECK(!xideRenameScopeCreate(selected,files,2,exact-1,NULL,NULL,&error) && error==XIDE_RENAME_SCOPE_LIMIT);
 CHECK(!xideRenameScopeCreate(selected,files,2,XIDE_RENAME_SCOPE_BYTES+1u,NULL,NULL,&error) && error==XIDE_RENAME_SCOPE_LIMIT);
 CHECK(!xideRenameScopeCreate(selected,NULL,1,0,NULL,NULL,&error) && error==XIDE_RENAME_SCOPE_ARGUMENT);
 CHECK(!xideRenameScopeCreate(NULL,NULL,0,0,NULL,NULL,&error) && error==XIDE_RENAME_SCOPE_ARGUMENT);
 denyAllocation=1;CHECK(!xideRenameScopeCreate(selected,files,2,0,NULL,NULL,&error) && error==XIDE_RENAME_SCOPE_MEMORY);denyAllocation=0;
 for(int stop=1;stop<=6;++stop){cancellation c={0,stop};CHECK(!xideRenameScopeCreate(selected,files,2,0,cancel,&c,&error) && error==XIDE_RENAME_SCOPE_CANCELLED && c.calls==stop);}
 xls_context* invalid=context(XLS_CONTEXT_XLANG,NULL,standalone,members,3);CHECK(invalid && !xideRenameScopeCreate(selected,&invalid,1,0,NULL,NULL,&error) && error==XIDE_RENAME_SCOPE_ARGUMENT);xlsContextRelease(invalid);
 invalid=context(XLS_CONTEXT_C,NULL,"D:/scope/a.c",NULL,0);CHECK(invalid && !xideRenameScopeCreate(selected,&invalid,1,0,NULL,NULL,&error));xlsContextRelease(invalid);
 CHECK(targets(scope,entry,shared));
 key=xidePathNativeWide(alias);unsigned before=allocations;
 for(size_t i=0;i<1000;++i)CHECK(xideRenameScopeOwnersNative(scope,key,&owners) && owners==11 && allocations==before);
 free(key);xlsContextRelease(contexts[0]);xlsContextRelease(contexts[1]);xlsContextRelease(files[0]);xlsContextRelease(files[1]);xideAnalysisCatalogDestroy(selected);xideAnalysisCatalogDestroy(other);
 CHECK(xideRenameScopeOwners(scope,alias,&owners) && owners==11 && !strcmp(xlsContextSpec(xideRenameScopeAt(scope,0))->project,"D:/scope/A.xlp"));
 CHECK(xideRenameScopeRetain(scope)==scope);xideRenameScopeRelease(scope);xideRenameScopeRelease(scope);
 CHECK(!xideRenameScopeRetain(NULL) && !xideRenameScopeBytes(NULL) && !xideRenameScopeCount(NULL) && !xideAnalysisCatalogBytes(NULL));xideRenameScopeRelease(NULL);
 /* Both the top project bit and the top standalone bit use the unsigned64
    domain. An overfull workspace must fail before allocating/retaining. */
 xls_context* many[64];char manifests[64][32];
 for(size_t i=0;i<64;++i){snprintf(manifests[i],sizeof(manifests[i]),"D:/scope/p%zu.xlp",i);many[i]=context(XLS_CONTEXT_PROJECT,manifests[i],entry,NULL,0);CHECK(many[i]);}
 selected=catalog(many,64,manifests[0]);CHECK(selected);key=xidePathNativeWide(entry);CHECK(xideAnalysisCatalogOwnersNative(selected,key,&owners) && owners==UINT64_MAX);free(key);
 scope=xideRenameScopeCreate(selected,NULL,0,0,NULL,NULL,&error);CHECK(scope && xideRenameScopeOwners(scope,entry,&owners) && owners==UINT64_MAX);xideRenameScopeRelease(scope);
 CHECK(!xideRenameScopeCreate(selected,many,1,0,NULL,NULL,&error) && error==XIDE_RENAME_SCOPE_ARGUMENT);xideAnalysisCatalogDestroy(selected);
 for(size_t i=0;i<64;++i)xlsContextRelease(many[i]);
 selected=catalog(NULL,0,"");invalid=context(XLS_CONTEXT_XLANG,NULL,entry,NULL,0);CHECK(selected && invalid);
 for(size_t i=0;i<64;++i)many[i]=invalid;
 scope=xideRenameScopeCreate(selected,many,64,0,NULL,NULL,&error);CHECK(scope && xideRenameScopeOwners(scope,entry,&owners) && owners==UINT64_MAX);
 CHECK(xlsContextEqual(xideRenameScopeAt(scope,63),invalid));xideRenameScopeRelease(scope);xideAnalysisCatalogDestroy(selected);xlsContextRelease(invalid);return 1;
}
int main(void){if(!run())return 1;printf("rename_scope_test passed: %u checks; all configured owners, selected-project independence, standalone boundaries, actual native target, exact quotas, cancellation, allocation and retained lifetime\n",checks);return 0;}
