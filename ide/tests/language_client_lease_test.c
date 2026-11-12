#include "../src/services/language_client.c"
#include <shellapi.h>
static volatile LONG checks,blockWrites,failWorkerAllocation,holdExit,workerId,trackOwner;
static volatile LONG receiverCalls;
static int receiverProbe(xvalue* v,size_t n,void* u){(void)v;(void)n;(void)u;InterlockedIncrement(&receiverCalls);return 0;}
static DWORD owner;static size_t ownerAllocations,ownerFrees;static HANDLE finished,releaseThread;
static xvalue* watchedRoot;static volatile LONG rootReleased;static DWORD rootReleaseThread;
#define CHECK(c) do{InterlockedIncrement(&checks);if(!(c)){fprintf(stderr,"language-client-lease:%d:%s\n",__LINE__,#c);exit(1);}}while(0)
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void __real_free(void*);
void* __wrap_malloc(size_t n){if(GetCurrentThreadId()==owner && trackOwner)++ownerAllocations;return __real_malloc(n);}
void* __wrap_calloc(size_t n,size_t z){if(GetCurrentThreadId()==owner && trackOwner)++ownerAllocations;if(GetCurrentThreadId()==(DWORD)workerId && InterlockedCompareExchange(&failWorkerAllocation,0,1)==1)return NULL;return __real_calloc(n,z);}
void __wrap_free(void* p){if(GetCurrentThreadId()==owner && trackOwner)++ownerFrees;__real_free(p);}
int __real_xideProcessWrite(xide_process*,const void*,size_t);
int __wrap_xideProcessWrite(xide_process* p,const void* data,size_t n){return blockWrites?0:__real_xideProcessWrite(p,data,n);}
extern HANDLE (WINAPI* __real___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
typedef struct invocation {LPTHREAD_START_ROUTINE proc;void* data;} invocation;
static DWORD WINAPI invoke(void* p){invocation* in=p;LPTHREAD_START_ROUTINE proc=in->proc;void* data=in->data;__real_free(in);InterlockedExchange(&workerId,(LONG)GetCurrentThreadId());DWORD code=proc(data);if(InterlockedCompareExchange(&holdExit,0,1)==1)CHECK(SetEvent(finished)&&WaitForSingleObject(releaseThread,15000)==WAIT_OBJECT_0);return code;}
static HANDLE WINAPI createProbe(LPSECURITY_ATTRIBUTES a,SIZE_T n,LPTHREAD_START_ROUTINE fn,LPVOID p,DWORD flags,LPDWORD id){if(fn!=worker)return __real___imp_CreateThread(a,n,fn,p,flags,id);invocation* in=__real_malloc(sizeof(*in));if(!in)return NULL;*in=(invocation){fn,p};HANDLE h=__real___imp_CreateThread(a,n,invoke,in,flags,id);if(!h)__real_free(in);return h;}
HANDLE (WINAPI* __wrap___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)=createProbe;
extern void (*__real___imp_xrtValueRelease)(xvalue*);
static void releaseProbe(xvalue* v){if(v && v==watchedRoot){rootReleaseThread=GetCurrentThreadId();InterlockedExchange(&rootReleased,1);}__real___imp_xrtValueRelease(v);}
void (*__wrap___imp_xrtValueRelease)(xvalue*)=releaseProbe;
static xvalue* tf(const xvalue* v,const char* k){return v && xrtValueType(v)==XVALUE_OBJECT?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static int eq(const xvalue* v,const char* text){return leaseText(v,text);}
static void put(xvalue* v,const char* k,xvalue* p){CHECK(p && xrtValueObjectSetNew(v,xrtStrView(k),p));}
static xvalue* params(int64 token){xvalue* p=xrtValueObject();CHECK(p);put(p,"token",xrtValueInt(token));return p;}
static void emit(xvalue* value){size_t n=0;char* bytes=xideLspFrame(value,&n,NULL);CHECK(bytes);xrtValueRelease(value);size_t at=0;while(at<n){DWORD sent=0;CHECK(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),bytes+at,(DWORD)(n-at),&sent,NULL)&&sent);at+=sent;}free(bytes);}
static void reply(const xvalue* id,xvalue* result,int error){xvalue* v=xrtValueObject();put(v,"jsonrpc",xrtValueString(xrtStrView("2.0")));put(v,"id",xrtValueDeepClone(id));if(error){xvalue* e=xrtValueObject();put(e,"code",xrtValueInt(error));put(e,"message",xrtValueString(xrtStrView("controlled lease response")));put(v,"error",e);xrtValueRelease(result);}else put(v,"result",result?result:xrtValueNull());emit(v);}
typedef struct helper {const char* mode;int count;int64 token;xvalue* pending;} helper;
static xvalue* ack(helper* h){xvalue* v=params(!strcmp(h->mode,"wrong-token")?h->token+1:h->token);put(v,"held",xrtValueBool(strcmp(h->mode,"not-held")!=0));put(v,"invalidated",xrtValueBool(false));put(v,"deferred",xrtValueInt(!strcmp(h->mode,"unproved-deferred")?1:0));put(v,"epoch",xrtValueInt(0));put(v,"inputSerial",xrtValueString(xrtStrView(!strcmp(h->mode,"serial-overflow")?"18446744073709551616":"0")));return v;}
static void noise(const char* method,int count){xvalue *v=xrtValueObject(),*p=xrtValueObject();put(v,"jsonrpc",xrtValueString(xrtStrView("2.0")));put(v,"method",xrtValueString(xrtStrView(method)));put(p,"count",xrtValueInt(count));put(v,"params",p);emit(v);}
static int serve(const xvalue* v,void* user)
{
 helper* h=user;const xvalue* id=tf(v,"id");
 if(eq(tf(v,"method"),"initialize")){xvalue *r=xrtValueObject(),*cap=xrtValueObject(),*e=xrtValueObject();put(e,"analysisLease",xrtValueInt(strcmp(h->mode,"unsupported")?1:0));put(cap,"experimental",e);put(r,"capabilities",cap);reply(id,r,0);}
 else if(eq(tf(v,"method"),"xlang/suspendAnalysis")){
  CHECK(xrtValueGetInt(tf(tf(v,"params"),"token"),&h->token));
  if(!strcmp(h->mode,"pending")){h->pending=xrtValueDeepClone(id);return 1;}
  if(!strcmp(h->mode,"wrong-id")){xvalue* wrong=xrtValueString(xrtStrView("not-the-current-lease"));reply(wrong,ack(h),0);xrtValueRelease(wrong);return 1;}
  noise("test/during-hold",h->count);reply(id,ack(h),0);
  if(!strcmp(h->mode,"invalidated")){xvalue *m=xrtValueObject(),*p=params(h->token);put(m,"jsonrpc",xrtValueString(xrtStrView("2.0")));put(m,"method",xrtValueString(xrtStrView("xlang/analysisLeaseInvalidated")));put(p,"invalidated",xrtValueBool(true));put(m,"params",p);emit(m);}
 }
 else if(eq(tf(v,"method"),"xlang/resumeAnalysis")){
  if(h->pending){reply(h->pending,NULL,-32800);xrtValueRelease(h->pending);h->pending=NULL;}
  reply(id,NULL,!strcmp(h->mode,"resume-error")?-32603:0);noise("test/after-resume",h->count);
 }else if(eq(tf(v,"method"),"test/noop"))++h->count;
 return 1;
}
static int server(const char* mode){helper h={.mode=mode};xide_lsp_stream stream={0};xide_model_error e={0};char bytes[16384];DWORD n=0;while(ReadFile(GetStdHandle(STD_INPUT_HANDLE),bytes,sizeof(bytes),&n,NULL)&&n)CHECK(xideLspStreamFeed(&stream,bytes,n,serve,&h,&e));xideLspStreamClear(&stream);xrtValueRelease(h.pending);return 0;}
static void clientSend(xide_language_client* c,const char* method,int id,xvalue* p){xvalue* v=xrtValueObject();put(v,"jsonrpc",xrtValueString(xrtStrView("2.0")));put(v,"method",xrtValueString(xrtStrView(method)));if(id)put(v,"id",xrtValueInt(id));if(p)put(v,"params",p);CHECK(xideLanguageClientSend(c,v,NULL));xrtValueRelease(v);}
static xvalue* next(xide_language_client* c,int id){ULONGLONG until=GetTickCount64()+10000;while(GetTickCount64()<until){xvalue* v=xideLanguageClientNext(c);if(v){int64 n=0;if(xrtValueGetInt(tf(v,"id"),&n)&&n==id)return v;xrtValueRelease(v);}Sleep(1);}CHECK(0);return NULL;}
static void initialize(xide_language_client* c){xvalue *p=xrtValueObject(),*o=xrtValueObject();put(o,"automaticDiagnostics",xrtValueBool(false));put(o,"checkOnSave",xrtValueBool(false));put(p,"initializationOptions",o);clientSend(c,"initialize",1,p);xvalue* r=next(c,1);int64 schema=0;CHECK(xrtValueGetInt(tf(tf(tf(tf(r,"result"),"capabilities"),"experimental"),"analysisLease"),&schema));CHECK(xideLanguageClientSetAnalysisLease(c,schema==1));CHECK(!xideLanguageClientSetAnalysisLease(c,0));xrtValueRelease(r);clientSend(c,"initialized",0,xrtValueObject());}
static xide_language_client* start(const char* self,const char* directory,const char* mode){const char* args[]={"--lease-server",mode};xide_language_client_spec s={self,directory,args,2};xide_language_client* c=xideLanguageClientStartProcess(&s,NULL);CHECK(c);initialize(c);return c;}
static void waitState(xide_language_client* c,int64 token,enum xide_language_lease_state state){ULONGLONG until=GetTickCount64()+10000;while(xideLanguageClientAnalysisState(c,token,NULL)!=state){CHECK(GetTickCount64()<until);Sleep(1);}}
static void waitStopped(xide_language_client* c){ULONGLONG until=GetTickCount64()+10000;while(!xideLanguageClientStopped(c)){CHECK(GetTickCount64()<until);Sleep(1);}}
static void settle(xide_language_client* c,int64 token){CHECK(xideLanguageClientResumeAnalysis(c,token));waitState(c,token,XIDE_LS_LEASE_RESUMED);CHECK(xideLanguageClientAnalysisCanRelease(c,token)&&xideLanguageClientReleaseAnalysis(c,token));}
int main(int argc,char** argv)
{
 if(argc==3&&!strcmp(argv[1],"--lease-server"))return server(argv[2]);
 CHECK(argc==4);owner=GetCurrentThreadId();finished=CreateEventW(NULL,TRUE,FALSE,NULL);releaseThread=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(finished&&releaseThread);
 char self[131072];wchar_t module[32768];CHECK(GetModuleFileNameW(NULL,module,32768)&&WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,module,-1,self,sizeof(self),NULL,NULL));
 xide_language_client* c=start(self,argv[1],"valid");CHECK(xideLanguageClientCanSuspendAnalysis(c)&&xideLanguageClientSetReceiver(c,receiverProbe,NULL));
 watchedRoot=xrtValueObject();put(watchedRoot,"payload",xrtValueString(xrtStrView("old parsed body")));CHECK(receive(watchedRoot,128,c));
 xvalue* ordinary=xrtValueObject();put(ordinary,"jsonrpc",xrtValueString(xrtStrView("2.0")));put(ordinary,"method",xrtValueString(xrtStrView("test/noop")));
 LONG beforeCallbacks=receiverCalls;ownerAllocations=ownerFrees=0;trackOwner=1;int64 token=xideLanguageClientSuspendAnalysis(c,NULL);CHECK(token==1);CHECK(!xideLanguageClientSend(c,ordinary,NULL));trackOwner=0;CHECK(!ownerAllocations&&!ownerFrees);
 CHECK(!xideLanguageClientReleaseAnalysis(c,token)&&!xideLanguageClientResumeAnalysis(c,token+1));waitState(c,token,XIDE_LS_LEASE_HELD);CHECK(rootReleased&&rootReleaseThread!=owner);watchedRoot=NULL;CHECK(!xideLanguageClientNext(c)&&receiverCalls==beforeCallbacks);CHECK(xideLanguageClientResumeAnalysis(c,token));waitState(c,token,XIDE_LS_LEASE_RESUMED);ULONGLONG until=GetTickCount64()+10000;while(!xideLanguageClientHasPending(c)){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(receiverCalls==beforeCallbacks&&!xideLanguageClientNext(c)&&xideLanguageClientReleaseAnalysis(c,token));xvalue* post=NULL;while(!(post=xideLanguageClientNext(c))){CHECK(GetTickCount64()<until);Sleep(1);}CHECK(eq(tf(post,"method"),"test/after-resume"));xrtValueRelease(post);CHECK(xideLanguageClientCanSuspendAnalysis(c));xideLanguageClientStop(c);
 c=start(self,argv[1],"valid");until=GetTickCount64()+10000;for(;;){EnterCriticalSection(&c->lock);size_t pending=c->sendCount;LeaveCriticalSection(&c->lock);if(!pending)break;CHECK(GetTickCount64()<until);Sleep(1);}blockWrites=1;for(size_t i=0;i<QUEUE_COUNT;++i)CHECK(xideLanguageClientSend(c,ordinary,NULL));token=xideLanguageClientSuspendAnalysis(c,NULL);CHECK(token==1&&xideLanguageClientResumeAnalysis(c,token));until=GetTickCount64()+10000;for(;;){EnterCriticalSection(&c->lock);int queued=c->lease.resumeQueued;size_t count=c->sendCount;LeaveCriticalSection(&c->lock);if(queued){CHECK(count==QUEUE_COUNT+2u);break;}CHECK(GetTickCount64()<until);Sleep(1);}blockWrites=0;signalTransport(c);waitState(c,token,XIDE_LS_LEASE_RESUMED);CHECK(!xideLanguageClientNext(c)&&xideLanguageClientReleaseAnalysis(c,token));until=GetTickCount64()+10000;while(!(post=xideLanguageClientNext(c))){CHECK(GetTickCount64()<until);Sleep(1);}int64 received=0;CHECK(eq(tf(post,"method"),"test/after-resume")&&xrtValueGetInt(tf(tf(post,"params"),"count"),&received)&&received==QUEUE_COUNT);xrtValueRelease(post);xideLanguageClientStop(c);
 const char* invalid[] = {"wrong-token","not-held","unproved-deferred","serial-overflow"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i){c=start(self,argv[1],invalid[i]);token=xideLanguageClientSuspendAnalysis(c,NULL);CHECK(token);waitState(c,token,XIDE_LS_LEASE_FAILED);CHECK(!xideLanguageClientReleaseAnalysis(c,token)&&!xideLanguageClientSend(c,ordinary,NULL));CHECK(xideLanguageClientResumeAnalysis(c,token));waitStopped(c);CHECK(xideLanguageClientReleaseAnalysis(c,token));xideLanguageClientStop(c);}
 c=start(self,argv[1],"pending");token=xideLanguageClientSuspendAnalysis(c,NULL);CHECK(token&&xideLanguageClientResumeAnalysis(c,token));waitState(c,token,XIDE_LS_LEASE_RESUMED);CHECK(xideLanguageClientReleaseAnalysis(c,token));xideLanguageClientStop(c);
 c=start(self,argv[1],"wrong-id");token=xideLanguageClientSuspendAnalysis(c,NULL);Sleep(50);CHECK(xideLanguageClientAnalysisState(c,token,NULL)==XIDE_LS_LEASE_PENDING);settle(c,token);xideLanguageClientStop(c);
 c=start(self,argv[1],"invalidated");token=xideLanguageClientSuspendAnalysis(c,NULL);waitState(c,token,XIDE_LS_LEASE_HELD);until=GetTickCount64()+10000;int invalidated=0;while(!invalidated){xideLanguageClientAnalysisState(c,token,&invalidated);CHECK(GetTickCount64()<until);Sleep(1);}settle(c,token);xideLanguageClientStop(c);
 c=start(self,argv[1],"resume-error");token=xideLanguageClientSuspendAnalysis(c,NULL);waitState(c,token,XIDE_LS_LEASE_HELD);CHECK(xideLanguageClientResumeAnalysis(c,token));waitState(c,token,XIDE_LS_LEASE_FAILED);holdExit=1;CHECK(xideLanguageClientResumeAnalysis(c,token));CHECK(WaitForSingleObject(finished,10000)==WAIT_OBJECT_0);CHECK(xideLanguageClientState(c,NULL,0)>=XIDE_LS_STOPPED&&!xideLanguageClientStopped(c)&&!xideLanguageClientAnalysisCanRelease(c,token)&&!xideLanguageClientReleaseAnalysis(c,token));CHECK(SetEvent(releaseThread));waitStopped(c);CHECK(xideLanguageClientReleaseAnalysis(c,token));xideLanguageClientStop(c);
 c=start(self,argv[1],"valid");failWorkerAllocation=1;token=xideLanguageClientSuspendAnalysis(c,NULL);waitState(c,token,XIDE_LS_LEASE_FAILED);CHECK(xideLanguageClientResumeAnalysis(c,token));waitStopped(c);CHECK(xideLanguageClientReleaseAnalysis(c,token));xideLanguageClientStop(c);
 c=start(self,argv[1],"unsupported");CHECK(!xideLanguageClientCanSuspendAnalysis(c)&&!xideLanguageClientSuspendAnalysis(c,NULL));xideLanguageClientStop(c);
 c=start(self,argv[1],"valid");EnterCriticalSection(&c->lock);c->lease.last=INT64_MAX;LeaveCriticalSection(&c->lock);CHECK(!xideLanguageClientSuspendAnalysis(c,NULL));xideLanguageClientStop(c);
 c=xideLanguageClientStart(argv[2],argv[3],argv[1],NULL);CHECK(c);initialize(c);for(int i=0;i<8;++i){token=xideLanguageClientSuspendAnalysis(c,NULL);CHECK(token==i+1);if(i%2)CHECK(xideLanguageClientResumeAnalysis(c,token));else waitState(c,token,XIDE_LS_LEASE_HELD);if(i%2){waitState(c,token,XIDE_LS_LEASE_RESUMED);CHECK(xideLanguageClientReleaseAnalysis(c,token));}else settle(c,token);}xideLanguageClientStop(c);
 xrtValueRelease(ordinary);CloseHandle(finished);CloseHandle(releaseThread);printf("PASS language client analysis lease: %ld checks; real transport/native server, full FIFO control reserve, worker-only disposal, strict replies, pending abandon, invalidation, failed-resume recovery and actual thread exit\n",(long)checks);return 0;
}
