#include "services/project_references.h"
static size_t cloneCalls,cloneFailAt,setCalls,setFailAt;static uint64_t faultThread;
xvalue* __real_xrtValueDeepClone(const xvalue*);
bool __real_xrtValueObjectSetNew(xvalue*,xstrview,xvalue*);
xvalue* __wrap_xrtValueDeepClone(const xvalue* value)
{if(faultThread==xrtThreadCurrentId()&&++cloneCalls==cloneFailAt)return NULL;return __real_xrtValueDeepClone(value);}
bool __wrap_xrtValueObjectSetNew(xvalue* object,xstrview name,xvalue* value)
{if(faultThread==xrtThreadCurrentId()&&++setCalls==setFailAt){xrtValueRelease(value);return false;}return __real_xrtValueObjectSetNew(object,name,value);}
static int checkResultProof(xide_reference_query*);
#define XIDE_RENAME_PLAN_READY_TEST checkResultProof
#define XIDE_RENAME_PLAN_TEST_NO_MAIN
#include "project_rename_plan_test.c"
static size_t exportPolls,exportStop,proofPasses;static const char* exportMutationPath;static const char* exportMutationBody;static int exportMutationOkay;static xvalue* detachedProof;
static int exportCancelled(void* unused)
{(void)unused;++exportPolls;if(exportMutationPath&&exportPolls==20){exportMutationOkay=writeText(exportMutationPath,exportMutationBody);exportMutationPath=NULL;}return exportStop&&exportPolls>=exportStop;}
static int checkResultProof(xide_reference_query* q)
{
 ++proofPasses;xide_model_error error={0};reference_output* native=xrtFutureValue(q->future);CHECK(native&&native->guards.Count>=4);
 exportPolls=exportStop=0;xvalue* wire=xideProjectReferencesRenameResult(q,0,exportCancelled,NULL,&error);CHECK(wire);size_t totalPolls=exportPolls;
 int64 schema=0,scopes=0;const xvalue* guards=testGet(wire,"inputGuards");CHECK(xrtValueCount(wire)==4&&xrtValueGetInt(testGet(wire,"schema"),&schema)&&schema==1&&xrtValueGetInt(testGet(wire,"scopeCount"),&scopes)&&scopes==4&&xrtValueCount(guards)==native->guards.Count&&xrtValueCount(testGet(testGet(wire,"workspaceEdit"),"documentChanges"))==5);
 size_t absent=0,unrelated=0;const char* isolated=NULL;
 for(size_t i=0;i<native->guards.Count;++i){
  const xide_language_input_guard* guard=((xide_language_input_guard**)native->guards.Data)[i];const xvalue* row=xrtValueArrayGet(guards,i);CHECK(xrtValueCount(row)==4&&xrtValueEqual(testGet(row,"files"),xideLanguageInputGuardFiles(guard)));
  const xvalue* probes=testGet(testGet(row,"resolution"),"probes");CHECK(xrtValueEqual(probes,xideLanguageInputGuardProbes(guard)));
  xls_context* context=xideAnalysisContextRead(testGet(row,"context"),NULL,0);CHECK(context&&xlsContextEqual(context,xideLanguageInputGuardContext(guard)));xlsContextRelease(context);
  for(size_t j=0;j<xrtValueCount(probes);++j){bool exists=true;CHECK(xrtValueGetBool(testGet(xrtValueArrayGet(probes,j),"exists"),&exists));if(!exists)++absent;}
  const xvalue* files=testGet(row,"files");for(size_t j=0;j<xrtValueCount(files);++j){const char* path=string(testGet(xrtValueArrayGet(files,j),"path"));CHECK(path);if(strstr(path,"isolated.xl")){++unrelated;isolated=string(testGet(xrtValueArrayGet(xideLanguageInputGuardFiles(guard),j),"path"));}}
  char* encoded=xrtJsonStringify(row,false,NULL);CHECK(encoded);size_t n=strlen(encoded);xrtFree(encoded);xvalue* exact=xideLanguageInputGuardExport(guard,n,NULL,NULL,&error);CHECK(exact&&xrtValueEqual(exact,row)&&!xideLanguageInputGuardExport(guard,n-1,NULL,NULL,&error));xrtValueRelease(exact);
 }
 CHECK(absent&&unrelated&&isolated);
#ifdef XIDE_RENAME_INPUT_IMPORT_TEST
 CHECK(XIDE_RENAME_INPUT_IMPORT_TEST(q,wire));
#endif
 char* encoded=xrtJsonStringify(wire,false,NULL);CHECK(encoded);size_t n=strlen(encoded);xrtFree(encoded);xvalue* exact=xideProjectReferencesRenameResult(q,n,NULL,NULL,&error);CHECK(exact&&xrtValueEqual(exact,wire)&&!xideProjectReferencesRenameResult(q,n-1,NULL,NULL,&error)&&!xideProjectReferencesRenameResult(q,8u*1024u*1024u,NULL,NULL,&error));xrtValueRelease(exact);
 /* Mutating returned nested metadata never changes the retained guard. */
 xvalue* first=(xvalue*)xrtValueArrayGet(guards,0);CHECK(testPut(first,"files",xrtValueArray()));exact=xideProjectReferencesRenameResult(q,0,NULL,NULL,&error);CHECK(exact&&xrtValueCount(testGet(xrtValueArrayGet(testGet(exact,"inputGuards"),0),"files")));xrtValueRelease(exact);xrtValueRelease(wire);
 size_t boundaries[]={1,2,17,totalPolls/2,totalPolls-1,totalPolls};for(size_t i=0;i<sizeof(boundaries)/sizeof(*boundaries);++i){exportPolls=0;exportStop=boundaries[i];CHECK(!xideProjectReferencesRenameResult(q,0,exportCancelled,NULL,&error));}
 exportPolls=exportStop=0;exact=xideProjectReferencesRenameResult(q,0,NULL,NULL,&error);CHECK(exact);if(proofPasses==3)detachedProof=xrtValueRetain(exact);xrtValueRelease(exact);
 if(proofPasses==1){
  cloneCalls=setCalls=cloneFailAt=setFailAt=0;faultThread=xrtThreadCurrentId();exact=xideProjectReferencesRenameResult(q,0,NULL,NULL,&error);faultThread=0;CHECK(exact);xrtValueRelease(exact);size_t clones=cloneCalls,sets=setCalls;CHECK(clones>=native->guards.Count*2&&sets>clones);
  for(size_t i=1;i<=clones;++i){cloneCalls=0;cloneFailAt=i;faultThread=xrtThreadCurrentId();exact=xideProjectReferencesRenameResult(q,0,NULL,NULL,&error);faultThread=0;CHECK(!exact&&cloneCalls==i);}cloneFailAt=0;
  for(size_t i=1;i<=sets;++i){setCalls=0;setFailAt=i;faultThread=xrtThreadCurrentId();exact=xideProjectReferencesRenameResult(q,0,NULL,NULL,&error);faultThread=0;CHECK(!exact&&setCalls==i);}setFailAt=0;
  exact=xideProjectReferencesRenameResult(q,0,NULL,NULL,&error);CHECK(exact);xrtValueRelease(exact);printf("observed export fault boundaries: %zu clone calls, %zu consuming object insertions\n",clones,sets);
 }
 if(proofPasses==3){size_t size=0;unsigned char* body=xrtFileReadAll(isolated,&size);CHECK(body);exportMutationPath=isolated;exportMutationBody=(const char*)body;exportMutationOkay=0;exportPolls=0;CHECK(!xideProjectReferencesRenameResult(q,0,exportCancelled,NULL,&error)&&exportMutationOkay&&!exportMutationPath&&strstr(error.message,"changed"));xrtFree(body);exportMutationBody=NULL;}
 return 1;
}
#ifndef XIDE_RENAME_RESULT_TEST_NO_MAIN
int main(void)
{if(xideProjectReferencesRenameResult(NULL,0,NULL,NULL,NULL)||!runDraft()||!detachedProof)return 1;char* encoded=xrtJsonStringify(detachedProof,false,NULL);if(!encoded)return 1;xrtFree(encoded);xrtValueRelease(detachedProof);printf("project_rename_result_test passed: %u checks; complete actual four-scope candidate envelope, zero-target inputs, absent probes, full contexts, exact aggregate bounds, deep ownership after all originals exit, cancellation and post-encoding physical identity rejection\n",checks);return 0;}
#endif
