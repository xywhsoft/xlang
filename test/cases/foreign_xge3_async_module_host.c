#include "include/xlc.h"
#include <xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include "foreign_xge3_async_module.h"
#if defined(_WIN32)
#include <windows.h>
static int loaded(const char* path){return GetModuleHandleA(path)!=NULL;}
static void* address(const char* path,const char* name)
{HMODULE h=GetModuleHandleA(path);return h ? (void*)GetProcAddress(h,name):NULL;}
static void sleepOne(void){Sleep(1);}
#else
#include <dlfcn.h>
#include <time.h>
static int loaded(const char* path)
{void* h=dlopen(path,RTLD_NOLOAD|RTLD_NOW);if(!h)return 0;dlclose(h);return 1;}
static void* address(const char* path,const char* name)
{void* h=dlopen(path,RTLD_NOLOAD|RTLD_NOW);if(!h)return NULL;void* p=dlsym(h,name);dlclose(h);return p;}
static void sleepOne(void){const struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
#endif
static int ticks,callbacks;
static const char* engine;
static char imagePath[4096];
static xge_async_owned retained;
int fixtureAsyncModuleAudioBegin(void)
{typedef int (*Begin)(void);Begin begin=(Begin)address(engine,"fixtureXgeAudioBegin");assert(begin);return begin();}
int fixtureAsyncModuleTick(void){return ++ticks;}
int fixtureAsyncModuleCallbacks(void){return callbacks;}
const char* fixtureAsyncModulePath(void){return imagePath;}
int fixtureAsyncModulePublish(xge_async_owned request)
{
 typedef int (*Share)(xge_async_owned,xge_async_owned*);
 Share share=(Share)address(engine,"xgeAsyncLoadShareOwned");
 assert(share && !retained);return share(request,&retained);
}
int main(int argc,char** argv)
{
 #if defined(XGE_TEST_FONT_LOAD) || defined(XGE_TEST_SOUND_LOAD)
 assert(argc==9);
 #else
 assert(argc==8);
 #endif
 callbacks=argv[6][0]=='1';engine=argv[5];assert(!loaded(engine));
 #if defined(XGE_TEST_FONT_LOAD) || defined(XGE_TEST_SOUND_LOAD)
 int length=snprintf(imagePath,sizeof(imagePath),"%s",argv[8]);
 assert(length>0 && (size_t)length<sizeof(imagePath));
 #else
 int length=snprintf(imagePath,sizeof(imagePath),"%s/input.tga",argv[4]);
 assert(length>0 && (size_t)length<sizeof(imagePath));
 static const unsigned char data[]={0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,24,32,3,2,1};
 FILE* file=fopen(imagePath,"wb");assert(file);
 assert(fwrite(data,1,sizeof(data),file)==sizeof(data));assert(!fclose(file));
 #endif
 XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=argv[2];options.tempDir=argv[4];
 XLC_Context* compiler=xlcCreateContext(&options);assert(compiler);
 assert(xlcAddIncludePath(compiler,argv[2]) && xlcAddIncludePath(compiler,argv[3]) && xlcAddIncludePath(compiler,argv[7]));
 assert(xlcRegisterCFunction(compiler,"fixtureAsyncModuleTick",(void*)fixtureAsyncModuleTick));
 assert(xlcRegisterCFunction(compiler,"fixtureAsyncModuleCallbacks",(void*)fixtureAsyncModuleCallbacks));
 assert(xlcRegisterCFunction(compiler,"fixtureAsyncModulePath",(void*)fixtureAsyncModulePath));
 assert(xlcRegisterCFunction(compiler,"fixtureAsyncModulePublish",(void*)fixtureAsyncModulePublish));
 assert(xlcRegisterCFunction(compiler,"fixtureAsyncModuleAudioBegin",(void*)fixtureAsyncModuleAudioBegin));
 if(!xlcAddSourceFile(compiler,argv[1]) || !xlcCompile(compiler)){
  fprintf(stderr,"Async module compile failed: %s\n",xlcGetLastError(compiler));return 3;
 }
 XL_Module* module=xlcRelocateModule(compiler);
 if(!module){fprintf(stderr,"Async module relocate failed: %s\n",xlcGetLastError(compiler));return 4;}
 typedef int (*CheckedMain)(xvalue*,int*);
 CheckedMain entry=(CheckedMain)xlModuleGetSymbol(module,"xl_script_main_checked");assert(entry);
 xlcDestroyContext(compiler);int code=1;assert(entry(NULL,&code) && !code && retained && loaded(engine));
 typedef int (*Poll)(xge_async_owned,int*);
 typedef int (*Info)(xge_async_owned,xge_async_load_info_t*);
 typedef int (*Callback)(xge_async_owned,xge_scene_proc,void*,const xge_callback_owner_t*);
 typedef int (*Release)(xge_async_owned,bool*);
 typedef int (*Operation)(void);
 Poll poll=(Poll)address(engine,"xgeAsyncLoadPollOwned");
 Info info=(Info)address(engine,"xgeAsyncLoadInfoGet");
 Callback clear=(Callback)address(engine,"xgeAsyncLoadCallbackSetOwned");
 Release release=(Release)address(engine,"xgeAsyncLoadReleaseOwnerChecked");
 Operation unit=(Operation)address(engine,"xgeUnitChecked");
 assert(poll && info && clear && release && unit);
 #if defined(XGE_TEST_FONT_LOAD) || defined(XGE_TEST_SOUND_LOAD)
 assert(ticks==callbacks);
 #else
 assert(ticks==0);
 #endif
 xlDestroyModule(module);
 assert(loaded(engine));
 if(callbacks){
  int status=XGE_ASYNC_LOADING,count=0;
  while(status==XGE_ASYNC_LOADING && count++<5000){assert(poll(retained,&status)==XGE_OK);sleepOne();}
  assert(status==XGE_ASYNC_READY && ticks==1 && !xrtGetError());
 #if defined(XGE_TEST_SOUND_LOAD)
  /* SOUND completes synchronously, so Poll alone cannot prove its closure
   * still executes after Close. Reuse the retained native request and execute
   * completion again while only its existing code/ownership graph keeps it live. */
  typedef int (*ResetSound)(xge_async_owned);
  typedef int (*StartSound)(xge_async_owned,const char*);
  ResetSound resetSound=(ResetSound)address(engine,"xgeAsyncLoadResetOwned");
  StartSound startSound=(StartSound)address(engine,"xgeAsyncSoundStartOwned");
  assert(resetSound && startSound && unit()==XGE_ERROR_BUSY);xrtClearError();
  assert(resetSound(retained)==XGE_OK && startSound(retained,imagePath)==XGE_OK);
  assert(ticks==2 && !xrtGetError());
  puts("Sound completion executed again after real module Close");
 #endif
 }else{
  xge_async_load_info_t state={0};state.size=sizeof(state);
  assert(info(retained,&state)==XGE_OK && state.status==XGE_ASYNC_PENDING && ticks==0);
 }
 #if defined(XGE_TEST_FONT_LOAD) || defined(XGE_TEST_SOUND_LOAD)
 if(callbacks){
  /* An unclaimed Font/Sound is a live engine owner, even after module Close.
   * Explicitly discard it while the C publisher still owns the request.
   * This is not the separately pending automatic module-global shutdown gate. */
  assert(unit()==XGE_ERROR_BUSY);xrtClearError();
  typedef int (*Reset)(xge_async_owned);
  Reset reset=(Reset)address(engine,"xgeAsyncLoadResetOwned");assert(reset && reset(retained)==XGE_OK);
 }
 #endif
 assert(clear(retained,NULL,NULL,NULL)==XGE_OK);
 bool consumed=false;assert(release(retained,&consumed)==XGE_OK && consumed);retained=NULL;
 assert(unit()==XGE_OK);
 XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(5000);
 if(!drain.ready || !drain.ok || loaded(engine) || xrtGetError()){
  fprintf(stderr,"Async final drain: ready=%d ok=%d loaded=%d kind=%d\n",drain.ready,drain.ok,loaded(engine),(int)xrtErrorKind(xrtGetError()));return 5;
 }
 #if !defined(XGE_TEST_FONT_LOAD) && !defined(XGE_TEST_SOUND_LOAD)
 assert(!remove(imagePath));
 #endif
 puts("Async request kept real module/worker/callback code through Close; final drain unloaded the native image");return 0;
}
