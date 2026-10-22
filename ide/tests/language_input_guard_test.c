#include "services/language_check.h"
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
typedef struct consumer { xide_language_input_guard* guard; int valid,failures; } consumer;
static int consume(void* user)
{
 consumer* value=user;
 for(unsigned i=0;i<40;++i) {
  xide_language_input_guard* lease=xideLanguageInputGuardRetain(value->guard); xide_model_error error={0};
  if(xideLanguageInputGuardValidate(lease,NULL,NULL,&error)!=value->valid) ++value->failures;
  if(!xideLanguageInputGuardFiles(lease) || !xrtValueCount(xideLanguageInputGuardFiles(lease))) ++value->failures;
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
 char relative[160]; snprintf(relative,sizeof(relative),"build/input-guard-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
 CHECK(xrtDirCreateAll(relative)); char* root=xrtPathAbs(relative); CHECK(root);
 char *entry=xrtPathJoin(root,"entry.xl"),*dep=xrtPathJoin(root,"retained_dep.xl"),*unused=xrtPathJoin(root,"unused.xl"); CHECK(entry && dep && unused);
 CHECK(writeText(dep,dependency));
 xide_language_input_guard* guard=capture(root,entry,entryText,unused,1); CHECK(guard);
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
 char* created=xrtPathJoin(root,"created_dep.xl"); CHECK(created && writeText(created,dependency));
 CHECK(!xideLanguageInputGuardValidate(guard,NULL,NULL,&error)); xideLanguageInputGuardRelease(guard);

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
 printf("language_input_guard_test passed: %u checks plus 320 concurrent validations; detached ownership, cancellation, disk identity/bytes, negative probes, environment and source-error proofs.\n",checks);
 return 0;
}
