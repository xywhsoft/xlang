#include "services/rename_scope.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t providerCalls,providerCreated,providerReleased,providerFail;
static size_t cancelAfterProvider;static int pendingCancel;
static xls_context* observedProvider(const xls_context_spec* s,size_t limit,enum xls_context_error* error)
{++providerCalls;if(providerCalls==providerFail){if(error)*error=XLS_CONTEXT_MEMORY;return NULL;}xls_context* c=xlsContextCreate(s,limit,error);if(c)++providerCreated;if(cancelAfterProvider&&providerCalls==cancelAfterProvider)pendingCancel=1;return c;}
static void observedProviderRelease(xls_context* c){if(c)++providerReleased;xlsContextRelease(c);}
#define XIDE_RENAME_PROVIDER_CREATE observedProvider
#define XIDE_RENAME_PROVIDER_RELEASE observedProviderRelease
#include "services/rename_scope.c"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"rename-provider:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int put(xvalue* o,const char* k,xvalue* v){return v&&xrtValueObjectSetNew(o,xrtStrView(k),v);}
static xide_analysis_catalog* makeCatalog(xls_context* const* list,size_t count,const char* selected)
{
 xvalue *v=xrtValueObject(),*projects=xrtValueArray();int okay=v&&projects;
 for(size_t i=0;okay&&i<count;++i){xvalue* p=xideAnalysisContextWrite(list[i]);okay=p&&xrtValueArrayAppendNew(projects,p);}
 okay=okay&&put(v,"schema",xrtValueInt(1))&&put(v,"selectedProject",xrtValueString(xrtStrView(selected)))&&put(v,"packagePath",xrtValueNull());if(okay){okay=put(v,"projects",projects);projects=NULL;}xrtValueRelease(projects);xide_analysis_catalog* c=okay?xideAnalysisCatalogCreate(v,NULL,0):NULL;xrtValueRelease(v);return c;
}
typedef struct stop_state {size_t calls,at;} stop_state;
static int cancel(void* p){stop_state* s=p;return ++s->calls==s->at;}
static int externalCancel(void* unused){(void)unused;return pendingCancel;}
static int reject(xide_rename_scope* scope,xls_context_spec spec)
{xls_context* c=xlsContextCreate(&spec,16u*1024u*1024u,NULL);CHECK(c);uint64_t bits=UINT64_MAX;enum xide_rename_scope_error error;CHECK(!xideRenameScopeProvider(scope,c,&bits,NULL,NULL,&error)&&!bits&&error==XIDE_RENAME_SCOPE_ARGUMENT);xlsContextRelease(c);return 1;}
typedef struct context_reader {xls_context* value;int okay;} context_reader;
static int32 readContext(ptr user)
{context_reader* r=user;r->okay=1;for(size_t i=0;i<1000;++i){const xls_context_spec* s=xlsContextSpec(r->value);if(strcmp(s->project,"D:/provider/A.xlp")||strcmp(s->packagePath,"")||s->lists[XLS_CONTEXT_MEMBERS].count||s->lists[XLS_CONTEXT_C_UNDEFINES].count!=2||strcmp(s->lists[XLS_CONTEXT_C_UNDEFINES].items[1],"two"))r->okay=0;}xlsContextRelease(r->value);return 0;}
static int runProvider(void)
{
 const char* members[]={"D:/provider/shared.xl","d:\\provider\\SHARED.xl","D:/provider/native.c","E:/outside/member.xl"};const char* options[]={"one","two"};xls_context_spec a={.kind=XLS_CONTEXT_PROJECT,.project="D:/provider/A.xlp",.configuration="debug",.entry="D:/provider/main.xl",.directory="D:/provider",.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=""};
 a.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,4};for(size_t i=1;i<XLS_CONTEXT_LIST_COUNT;++i)a.lists[i]=(xls_context_strings){options,2};xls_context_spec b=a;b.project="D:/provider/B.xlp";b.configuration="release";
 xls_context* projects[]={xlsContextCreate(&a,16u*1024u*1024u,NULL),xlsContextCreate(&b,16u*1024u*1024u,NULL)};CHECK(projects[0]&&projects[1]);xide_analysis_catalog *catalogA=makeCatalog(projects,2,a.project),*catalogB=makeCatalog(projects,2,b.project);CHECK(catalogA&&catalogB);
 xide_rename_scope *scope=xideRenameScopeCreate(catalogA,NULL,0,0,NULL,NULL,NULL),*selectedB=xideRenameScopeCreate(catalogB,NULL,0,0,NULL,NULL,NULL);CHECK(scope&&selectedB);
 xls_context_spec claim=a;claim.entry="d:\\provider\\SHARED.xl";claim.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};xls_context* c=xlsContextCreate(&claim,16u*1024u*1024u,NULL);CHECK(c);uint64_t bits=99;enum xide_rename_scope_error error;xls_context* expected=xideRenameScopeProvider(scope,c,&bits,NULL,NULL,&error);CHECK(expected&&bits==1&&!error&&xlsContextEqual(expected,c));xls_context* other=xideRenameScopeProvider(selectedB,c,&bits,NULL,NULL,&error);CHECK(other&&bits==1&&xlsContextEqual(expected,other));xlsContextRelease(other);
 const char* accepted[]={a.entry,"E:/outside/member.xl"};for(size_t i=0;i<2;++i){xls_context_spec s=claim;s.entry=accepted[i];xls_context* p=xlsContextCreate(&s,16u*1024u*1024u,NULL);CHECK(p);other=xideRenameScopeProvider(scope,p,&bits,NULL,NULL,&error);CHECK(other&&bits==1&&xlsContextEqual(p,other));xlsContextRelease(other);xlsContextRelease(p);}
 for(int i=0;i<11;++i){xls_context_spec s=claim;switch(i){case 0:s.project=b.project;break;case 1:s.configuration="release";break;case 2:s.directory="D:/provider/other";break;case 3:s.analysisProfile="different";break;case 4:s.targetKind="dll";break;case 5:s.nativeSelection=XLS_NATIVE_STATIC;break;case 6:s.packagePath=NULL;break;case 7:s.entry="D:/provider/unlisted.xl";break;case 8:s.entry="D:/provider2/shared.xl";break;case 9:s.entry="D:/provider/native.c";break;case 10:s.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,1};break;}CHECK(reject(scope,s));}
 for(size_t i=1;i<XLS_CONTEXT_LIST_COUNT;++i){xls_context_spec s=claim;s.lists[i]=(xls_context_strings){0};CHECK(reject(scope,s));const char* reversed[]={"two","one"};s=claim;s.lists[i]=(xls_context_strings){reversed,2};CHECK(reject(scope,s));}
 bits=99;CHECK(!xideRenameScopeProvider(NULL,c,&bits,NULL,NULL,&error)&&!bits);bits=99;CHECK(!xideRenameScopeProvider(scope,NULL,&bits,NULL,NULL,&error)&&!bits);CHECK(!xideRenameScopeProvider(scope,c,NULL,NULL,NULL,&error));stop_state measure={0,0};other=xideRenameScopeProvider(scope,c,&bits,cancel,&measure,&error);CHECK(other&&bits==1);xlsContextRelease(other);
 for(size_t i=1;i<=measure.calls;++i){stop_state s={0,i};bits=99;CHECK(!xideRenameScopeProvider(scope,c,&bits,cancel,&s,&error)&&!bits&&error==XIDE_RENAME_SCOPE_CANCELLED&&s.calls==i);}
 providerCalls=providerCreated=providerReleased=0;providerFail=1;bits=99;CHECK(!xideRenameScopeProvider(scope,c,&bits,NULL,NULL,&error)&&!bits&&error==XIDE_RENAME_SCOPE_MEMORY&&providerCreated==providerReleased);providerFail=0;
 xlsContextRelease(c);xlsContextRelease(projects[0]);xlsContextRelease(projects[1]);xideAnalysisCatalogDestroy(catalogA);xideAnalysisCatalogDestroy(catalogB);xideRenameScopeRelease(scope);xideRenameScopeRelease(selectedB);context_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(context_reader){xlsContextRetain(expected),0};threads[i]=xrtThreadCreate(readContext,readers+i,0);CHECK(threads[i]);}xlsContextRelease(expected);for(size_t i=0;i<4;++i){CHECK(xrtThreadWaitFor(threads[i],XIDE_XRT_TIMEOUT_MS(10000))==XWAIT_OK&&readers[i].okay);xrtThreadDestroy(threads[i]);}
 /* Unsigned bit63, then all64 identical standalone roles; failure after a
  * successful prefix must not return a partial owner mask or retained result. */
 xls_context* many[64];char manifests[64][40];for(size_t i=0;i<64;++i){snprintf(manifests[i],sizeof(manifests[i]),"D:/provider/p%zu.xlp",i);xls_context_spec s=a;s.project=manifests[i];s.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){0};many[i]=xlsContextCreate(&s,16u*1024u*1024u,NULL);CHECK(many[i]);}
 catalogA=makeCatalog(many,64,manifests[0]);scope=xideRenameScopeCreate(catalogA,NULL,0,0,NULL,NULL,NULL);CHECK(catalogA&&scope);other=xideRenameScopeProvider(scope,many[63],&bits,NULL,NULL,&error);CHECK(other&&bits==(UINT64_C(1)<<63));xlsContextRelease(other);xideRenameScopeRelease(scope);xideAnalysisCatalogDestroy(catalogA);for(size_t i=0;i<64;++i)xlsContextRelease(many[i]);
 xls_context_spec single={.kind=XLS_CONTEXT_XLANG,.entry="D:/provider/standalone.xl",.directory="D:/provider",.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe"};c=xlsContextCreate(&single,16u*1024u*1024u,NULL);CHECK(c);for(size_t i=0;i<64;++i)many[i]=c;catalogA=makeCatalog(NULL,0,"");scope=xideRenameScopeCreate(catalogA,many,64,0,NULL,NULL,NULL);CHECK(catalogA&&scope);size_t before=providerCalls;other=xideRenameScopeProvider(scope,c,&bits,NULL,NULL,&error);CHECK(other&&bits==UINT64_MAX&&providerCalls==before);xlsContextRelease(other);
 single.entry="d:\\provider\\STANDALONE.xl";xls_context* alias=xlsContextCreate(&single,16u*1024u*1024u,NULL);CHECK(alias);
 for(size_t i=1;i<=64;++i){providerCalls=providerCreated=providerReleased=0;providerFail=i;bits=99;CHECK(!xideRenameScopeProvider(scope,alias,&bits,NULL,NULL,&error)&&!bits&&error==XIDE_RENAME_SCOPE_MEMORY&&providerCreated==providerReleased&&providerCreated==i-1);}providerFail=0;measure=(stop_state){0,0};other=xideRenameScopeProvider(scope,alias,&bits,cancel,&measure,&error);CHECK(other&&bits==UINT64_MAX);xlsContextRelease(other);for(size_t i=1;i<=measure.calls;++i){stop_state s={0,i};providerCreated=providerReleased=0;bits=99;CHECK(!xideRenameScopeProvider(scope,alias,&bits,cancel,&s,&error)&&!bits&&error==XIDE_RENAME_SCOPE_CANCELLED&&providerCreated==providerReleased);}
 providerCalls=providerCreated=providerReleased=0;cancelAfterProvider=64;pendingCancel=0;bits=99;CHECK(!xideRenameScopeProvider(scope,alias,&bits,externalCancel,NULL,&error)&&!bits&&error==XIDE_RENAME_SCOPE_CANCELLED&&pendingCancel&&providerCreated==providerReleased);cancelAfterProvider=0;pendingCancel=0;
 xideRenameScopeRelease(scope);xls_context* mixed[]={c,alias};scope=xideRenameScopeCreate(catalogA,mixed,2,0,NULL,NULL,NULL);CHECK(scope);providerCalls=providerCreated=providerReleased=0;providerFail=1;bits=99;CHECK(!xideRenameScopeProvider(scope,c,&bits,NULL,NULL,&error)&&!bits&&error==XIDE_RENAME_SCOPE_MEMORY&&!providerCreated&&providerReleased==1);providerFail=0;
 xideRenameScopeRelease(scope);xideAnalysisCatalogDestroy(catalogA);xlsContextRelease(c);xlsContextRelease(alias);printf("PASS independent provider scope binding: %u checks; all context options, explicit membership/aliases, selection independence, bit63/all64, zero-construction exact reuse, every constructor prefix failure/cancellation, mixed reuse prefix and four readers after original owners exit\n",checks);return 1;
}
int main(void){return runProvider()?0:1;}
