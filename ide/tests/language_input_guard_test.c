#include "services/language_check.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"language_input_guard_test:%d: %s\n",__LINE__,#v); return 0; } } while(0)
static const char* dependency="export func value() as int\n return 7\nend\n";
static const char* entryText="import retained_dep\nfunc main(...) as int\n return retained_dep.value()\nend\n";
static int writeText(const char* path,const char* text)
{ return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int stopped(void* user) { return *(int*)user; }
static int stopDuring(void* user) { return ++*(int*)user>=2; }
static int containsPath(const xvalue* paths,const char* expected)
{
 for(size_t i=0;i<xrtValueCount(paths);++i) {xstrview s={0};if(!xrtValueGetString(xrtValueArrayGet(paths,i),&s)) return 0;
  if(s.Data && xrtPathIsAbs(s.Data) && xidePathLexicalEqual(s.Data,expected)) return 1;
 }return 0;
}
static int pathScope(xide_language_input_guard* guard,const char* entry,const char* dependency,const char* unused)
{
 xvalue* paths=xideLanguageInputGuardPaths(guard,NULL,NULL);CHECK(paths && containsPath(paths,entry) && containsPath(paths,dependency) && !containsPath(paths,unused));
 for(size_t i=0;i<xrtValueCount(paths);++i) {xstrview p={0};CHECK(xrtValueGetString(xrtValueArrayGet(paths,i),&p) && xideLanguageInputGuardTouches(guard,p.Data));
  for(size_t j=0;j<i;++j) {xstrview q={0};CHECK(xrtValueGetString(xrtValueArrayGet(paths,j),&q) && !xidePathLexicalEqual(p.Data,q.Data));}
 }
 int cancelled=1;CHECK(!xideLanguageInputGuardPaths(guard,stopped,&cancelled) && xideLanguageInputGuardCurrent(guard));
 cancelled=0;CHECK(!xideLanguageInputGuardPaths(guard,stopDuring,&cancelled) && xideLanguageInputGuardCurrent(guard));
 xvalue* again=xideLanguageInputGuardPaths(guard,NULL,NULL);CHECK(again && xrtValueEqual(paths,again));xrtValueRelease(again);
 const xide_language_input_guard* repeated[]={guard,guard};again=xideLanguageInputGuardsPaths(repeated,2,NULL,NULL);
 CHECK(again && xrtValueEqual(paths,again));xrtValueRelease(again);
 CHECK(!xideLanguageInputGuardsPaths(NULL,1,NULL,NULL) && !xideLanguageInputGuardsPaths(repeated,65,NULL,NULL));
 repeated[1]=NULL;CHECK(!xideLanguageInputGuardsPaths(repeated,2,NULL,NULL));
 again=xideLanguageInputGuardsPaths(NULL,0,NULL,NULL);CHECK(again && xrtValueType(again)==XVALUE_ARRAY && !xrtValueCount(again));xrtValueRelease(again);
 xrtValueRelease(paths);return 1;
}
typedef struct consumer { xide_language_input_guard* guard; int valid,failures; } consumer;
static int consume(void* user)
{
 consumer* value=user;
 for(unsigned i=0;i<40;++i) {
  xide_language_input_guard* lease=xideLanguageInputGuardRetain(value->guard); xide_model_error error={0};
  if(xideLanguageInputGuardValidate(lease,NULL,NULL,&error)!=value->valid) ++value->failures;
  if(!xideLanguageInputGuardFiles(lease) || !xrtValueCount(xideLanguageInputGuardFiles(lease))) ++value->failures;
  if(i%10==0) {xvalue* paths=xideLanguageInputGuardPaths(lease,NULL,NULL);if(!paths || !xrtValueCount(paths)) ++value->failures;xrtValueRelease(paths);}
  xideLanguageInputGuardRelease(lease);
 }
 xideLanguageInputGuardRelease(value->guard); return 0;
}
static int consumers(xide_language_input_guard* guard,int valid)
{
 consumer values[4]; xthread* threads[4];
 for(unsigned i=0;i<4;++i) values[i]=(consumer){xideLanguageInputGuardRetain(guard),valid,0};
 xideLanguageInputGuardRelease(guard);
 for(unsigned i=0;i<4;++i) { threads[i]=xrtThreadCreate(consume,&values[i],0); CHECK(threads[i]); }
 for(unsigned i=0;i<4;++i) { CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !values[i].failures); xrtThreadDestroy(threads[i]); }
 return 1;
}
static xide_language_input_guard* capture(const char* root,const char* entry,const char* text,const char* unused,int accepted)
{
 xide_model_error error={0}; xide_toolchains* cache=xideToolchainsCreate();
 char* compiler=xrtPathJoin(root,"no-executor/xl.exe");
 xide_language_snapshot inputs[2]={{entry,text,9},{unused,"export func unused() as int\n return 1\nend\n",17}};
 xide_language_analysis_options options={.nativeBackend=1};
 xide_language_check* check=xideLanguageCheckStartSnapshotsEx(cache,compiler,root,entry,inputs,2,&options,&error);
 if(!check) fprintf(stderr,"capture start: %s\n",error.message);
 uint64_t began=xrtClock(); enum xide_check_state state;
 while(check && (state=xideLanguageCheckState(check,error.message,sizeof(error.message)))==XIDE_CHECK_RUNNING && xrtClock()-began<15000000) xrtSleep(1);
 xvalue* result=xideLanguageCheckResult(check); bool okay=false;
 xide_language_input_guard* guard=NULL;
 if(result && xrtValueGetBool(xrtValueObjectGet(result,xrtStrView("ok")),&okay) && okay==accepted)
  guard=xideLanguageCheckCaptureInputGuard(check,&error);
 if(!guard) fprintf(stderr,"capture result: %s\n",error.message);
 xrtValueRelease(result); xideLanguageCheckDestroy(check); xideToolchainsDestroy(cache); xrtFree(compiler);
 return guard;
}
static int run(void)
{
 xide_model_error error={0}; CHECK(!xideLanguageCheckCaptureInputGuard(NULL,&error));
 CHECK(!xideLanguageInputGuardRetain(NULL)); xideLanguageInputGuardRelease(NULL);
 CHECK(!xideLanguageInputGuardFiles(NULL) && !xideLanguageInputGuardValidate(NULL,NULL,NULL,&error));
 CHECK(!xideLanguageInputGuardPaths(NULL,NULL,NULL));
 char relative[160]; snprintf(relative,sizeof(relative),"build/input-guard-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
 CHECK(xrtDirCreateAll(relative)); char* root=xrtPathAbs(relative); CHECK(root);
 char *entry=xrtPathJoin(root,"entry.xl"),*dep=xrtPathJoin(root,"retained_dep.xl"),*unused=xrtPathJoin(root,"unused.xl"); CHECK(entry && dep && unused);
 CHECK(writeText(dep,dependency));
 xide_language_input_guard* guard=capture(root,entry,entryText,unused,1); CHECK(guard);
 CHECK(pathScope(guard,entry,dep,unused));
 /* No producing check, toolchain cache, AST/IR, navigation table, overlay text
    or worker survives capture(). Only actual loaded files appear in reports. */
 CHECK(xrtValueCount(xideLanguageInputGuardFiles(guard))==2);
 int cancel=1; CHECK(!xideLanguageInputGuardValidate(guard,stopped,&cancel,&error));
 cancel=0; CHECK(xideLanguageInputGuardValidate(guard,stopped,&cancel,&error));
 cancel=0; CHECK(!xideLanguageInputGuardValidate(guard,stopDuring,&cancel,&error));
 CHECK(xideLanguageInputGuardValidate(guard,NULL,NULL,&error));
 CHECK(consumers(xideLanguageInputGuardRetain(guard),1));
 CHECK(writeText(dep,dependency)); /* same bytes, new physical identity */
 CHECK(consumers(xideLanguageInputGuardRetain(guard),0));
 CHECK(!xideLanguageInputGuardValidate(guard,NULL,NULL,&error)); xideLanguageInputGuardRelease(guard);
 guard=capture(root,entry,entryText,unused,1); CHECK(guard && xideLanguageInputGuardValidate(guard,NULL,NULL,&error));
 CHECK(writeText(dep,"export func value() as int\n return 8\nend\n"));
 CHECK(!xideLanguageInputGuardValidate(guard,NULL,NULL,&error));
 CHECK(writeText(dep,dependency)); CHECK(!xideLanguageInputGuardValidate(guard,NULL,NULL,&error)); xideLanguageInputGuardRelease(guard);

 guard=capture(root,entry,"import created_dep\nfunc main(...) as int\n return created_dep.value()\nend\n",unused,0);
 CHECK(guard && xideLanguageInputGuardValidate(guard,NULL,NULL,&error));
 char* candidate=xrtPathJoin(root,"created_dep.xl");xvalue* paths=xideLanguageInputGuardPaths(guard,NULL,NULL);CHECK(candidate && paths && containsPath(paths,candidate));
 CHECK(!xrtFileExists(candidate));xrtFree(candidate);
 char* created=xrtPathJoin(root,"created_dep.xl"); CHECK(created && writeText(created,dependency));
 CHECK(!xideLanguageInputGuardValidate(guard,NULL,NULL,&error)); xideLanguageInputGuardRelease(guard);
 CHECK(containsPath(paths,created));xrtValueRelease(paths);

 guard=capture(root,entry,entryText,unused,1); CHECK(guard);
 char* previous=NULL; CHECK(xideLanguageCheckPackagePath(&previous,&error));
 CHECK(xrtEnvSet("XLANG_PACKAGE_PATH",root));
 int changed=!xideLanguageInputGuardValidate(guard,NULL,NULL,&error);
 int restored=previous ? xrtEnvSet("XLANG_PACKAGE_PATH",previous) : xrtEnvRemove("XLANG_PACKAGE_PATH"); free(previous);
 CHECK(changed && restored && !xideLanguageInputGuardValidate(guard,NULL,NULL,&error)); xideLanguageInputGuardRelease(guard);

 guard=capture(root,entry,"func main(...) as int\n return \"bad\"\nend\n",unused,0);
 CHECK(guard && xrtValueCount(xideLanguageInputGuardFiles(guard))==1 && xideLanguageInputGuardValidate(guard,NULL,NULL,&error));
 xideLanguageInputGuardRelease(guard);
 xrtFree(created); xrtFree(entry); xrtFree(dep); xrtFree(unused); xrtFree(root); return 1;
}
int main(void)
{
 if(!run()) return 1;
 printf("language_input_guard_test passed: %u checks plus 320 concurrent validations; detached ownership, cancellation, disk identity/bytes, negative probes, environment, source-error proofs and sorted exact dependency paths.\n",checks);
 return 0;
}
