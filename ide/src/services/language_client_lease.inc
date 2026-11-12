/* Included once in the transport implementation. All protocol framing and
 * stale DOM/body disposal stay on that worker; owner controls change metadata. */
static const xvalue* leaseField(const xvalue* v,const char* key)
{return v && xrtValueType(v)==XVALUE_OBJECT?xrtValueObjectGet(v,xrtStrView(key)):NULL;}
static int leaseText(const xvalue* v,const char* text)
{xstrview s={0};return v && xrtValueGetString(v,&s) && s.Size==strlen(text) && !memcmp(s.Data,text,s.Size);}
static void leaseId(char out[64],int64_t token,int resume)
{snprintf(out,64,"xide-analysis-%s-%lld",resume?"resume":"suspend",(long long)token);}
static int leaseReceipt(const xvalue* p,int64_t token)
{
 int64 n=0,epoch=0;xstrview serial={0};bool held=false,invalid=false;
 if(!xrtValueGetInt(leaseField(p,"token"),&n) || n!=token ||
    !xrtValueGetBool(leaseField(p,"held"),&held) || !held ||
    !xrtValueGetBool(leaseField(p,"invalidated"),&invalid) ||
    !xrtValueGetInt(leaseField(p,"deferred"),&n) || n<0 || n>256 ||
    !xrtValueGetInt(leaseField(p,"epoch"),&epoch) || epoch<0 ||
    !xrtValueGetString(leaseField(p,"inputSerial"),&serial) || !serial.Size || serial.Size>20)return 0;
 uint64_t value=0;for(size_t i=0;i<serial.Size;++i){unsigned char ch=(unsigned char)serial.Data[i];if(ch<'0'||ch>'9'||value>(UINT64_MAX-(ch-'0'))/10u)return 0;value=value*10u+(ch-'0');}
 return (!n || invalid)?1:0;
}
static int leaseReceive(xide_language_client* c,xvalue* message)
{
 int take=0,changed=0;EnterCriticalSection(&c->lock);
 int64_t token=c->lease.token;
 if(token){
  const xvalue* id=leaseField(message,"id");char suspend[64],resume[64];leaseId(suspend,token,0);leaseId(resume,token,1);
  int pauseReply=leaseText(id,suspend),resumeReply=leaseText(id,resume);
  if(pauseReply || resumeReply){
   take=changed=1;const xvalue* result=leaseField(message,"result");
   int okay=leaseText(leaseField(message,"jsonrpc"),"2.0") && !leaseField(message,"method") && !leaseField(message,"error");
   if(pauseReply){
    int64 code=0;int abandoned=c->lease.wantResume && c->lease.resumeQueued && !result &&
     leaseText(leaseField(message,"jsonrpc"),"2.0") && !leaseField(message,"method") &&
     xrtValueGetInt(leaseField(leaseField(message,"error"),"code"),&code) && code==-32800;
    okay=c->lease.suspendQueued && (abandoned || (okay && leaseReceipt(result,token)));
    if(okay){bool invalid=false;xrtValueGetBool(leaseField(result,"invalidated"),&invalid);c->lease.invalidated|=invalid;if(!c->lease.failed && !c->lease.wantResume)c->lease.phase=XIDE_LS_LEASE_HELD;}
   }else{
    okay=okay && c->lease.resumeQueued && c->lease.wantResume && result && xrtValueType(result)==XVALUE_NULL;
    if(okay){c->lease.resumeAck=1;if(!c->lease.failed)c->lease.phase=XIDE_LS_LEASE_RESUMED;}
   }
   if(!okay){c->lease.failed=1;c->lease.invalidated=1;c->lease.phase=XIDE_LS_LEASE_FAILED;}
  }else if(leaseText(leaseField(message,"method"),"xlang/analysisLeaseInvalidated")){
   take=1;int64 n=0;bool invalid=false;const xvalue* p=leaseField(message,"params");
   if(!xrtValueGetInt(leaseField(p,"token"),&n)){c->lease.failed=1;c->lease.phase=XIDE_LS_LEASE_FAILED;c->lease.invalidated=1;changed=1;}
   else if(n==token){c->lease.invalidated=1;changed=1;if(!leaseText(leaseField(message,"jsonrpc"),"2.0") || leaseField(message,"id") || !xrtValueGetBool(leaseField(p,"invalidated"),&invalid) || !invalid){c->lease.failed=1;c->lease.phase=XIDE_LS_LEASE_FAILED;}}
  }
  /* Old ordinary replies cannot launch source-reading decoder callbacks after
     the remote acknowledgement. JSON parser values have no user finalizers. */
  if(!take)take=!c->lease.resumeAck;
 }
 LeaveCriticalSection(&c->lock);
 if(take)xrtValueRelease(message);
 if(changed)wakeOwner(c);
 return take;
}
static void leasePrepare(xide_language_client* c)
{
 node* old=NULL;int action=0;int64_t token=0;
 EnterCriticalSection(&c->lock);
 if(c->lease.token){if(!c->lease.resumeAck){old=c->receiveHead;c->receiveHead=c->receiveTail=NULL;c->receiveBytes=c->receiveCount=0;}
  if(!c->lease.failed && !InterlockedCompareExchange(&c->stop,0,0)){
   token=c->lease.token;
   if(!c->lease.suspendQueued)action=1;
   else if(c->lease.wantResume && !c->lease.resumeQueued)action=2;
  }
 }
 LeaveCriticalSection(&c->lock);clear(old);if(!action)return;
 char id[64];leaseId(id,token,action==2);xvalue *message=xrtValueObject(),*params=xrtValueObject();node* n=calloc(1,sizeof(*n));
 int okay=n && message && params && xrtValueObjectSetNew(message,xrtStrView("jsonrpc"),xrtValueString(xrtStrView("2.0"))) &&
  xrtValueObjectSetNew(message,xrtStrView("id"),xrtValueString(xrtStrView(id))) &&
  xrtValueObjectSetNew(message,xrtStrView("method"),xrtValueString(xrtStrView(action==1?"xlang/suspendAnalysis":"xlang/resumeAnalysis"))) &&
  xrtValueObjectSetNew(params,xrtStrView("token"),xrtValueInt(token)) && xrtValueObjectSet(message,xrtStrView("params"),params);
 if(okay)n->bytes=xideLspFrame(message,&n->length,NULL);
 okay=okay && n->bytes && n->length<=512;xrtValueRelease(message);xrtValueRelease(params);
 EnterCriticalSection(&c->lock);
 if(c->lease.token==token && !InterlockedCompareExchange(&c->stop,0,0)){
  /* Two bounded control frames have dedicated reserve beyond the ordinary
     queue; a full pre-existing document batch cannot deadlock suspension. */
  if(okay && c->sendCount<QUEUE_COUNT+2u && n->length<=QUEUE_BYTES+1024u-c->sendBytes){
   if(c->sendTail)c->sendTail->next=n;else c->sendHead=n;c->sendTail=n;++c->sendCount;c->sendBytes+=n->length;n=NULL;
   if(action==1)c->lease.suspendQueued=1;else c->lease.resumeQueued=1;
  }else{c->lease.failed=1;c->lease.invalidated=1;c->lease.phase=XIDE_LS_LEASE_FAILED;}
 }
 LeaveCriticalSection(&c->lock);clear(n);wakeOwner(c);
}
int xideLanguageClientSetAnalysisLease(xide_language_client* c,int supported)
{
 if(!c)return 0;
 EnterCriticalSection(&c->lock);int okay=!c->lease.known && !c->lease.token;
 if(okay){c->lease.known=1;c->lease.supported=supported!=0;}LeaveCriticalSection(&c->lock);return okay;
}
int xideLanguageClientCanSuspendAnalysis(xide_language_client* c)
{
 if(!c)return 0;
 EnterCriticalSection(&c->lock);int okay=c->lease.known && c->lease.supported && !c->lease.token && c->lease.last<INT64_MAX && c->state==XIDE_LS_RUNNING && !InterlockedCompareExchange(&c->stop,0,0);LeaveCriticalSection(&c->lock);return okay;
}
int64_t xideLanguageClientSuspendAnalysis(xide_language_client* c,xide_model_error* e)
{
 if(!c)return fail(e,"Language connection is absent."),0;
 EnterCriticalSection(&c->lock);int64_t token=0;
 if(c->lease.known && c->lease.supported && !c->lease.token && c->lease.last<INT64_MAX && c->state==XIDE_LS_RUNNING && !InterlockedCompareExchange(&c->stop,0,0)){
  token=++c->lease.last;c->lease.token=token;c->lease.invalidated=c->lease.suspendQueued=c->lease.resumeQueued=c->lease.wantResume=c->lease.resumeAck=c->lease.failed=0;c->lease.phase=XIDE_LS_LEASE_PENDING;
 }
 LeaveCriticalSection(&c->lock);if(token)signalTransport(c);else fail(e,"Language connection cannot acquire a native analysis lease.");return token;
}
enum xide_language_lease_state xideLanguageClientAnalysisState(xide_language_client* c,int64_t token,int* invalidated)
{
 if(invalidated)*invalidated=1;
 if(!c || token<=0)return XIDE_LS_LEASE_STALE;
 EnterCriticalSection(&c->lock);enum xide_language_lease_state state=XIDE_LS_LEASE_STALE;
 if(c->lease.token==token){if(c->state>=XIDE_LS_STOPPED || InterlockedCompareExchange(&c->stop,0,0)){c->lease.failed=1;c->lease.invalidated=1;c->lease.phase=XIDE_LS_LEASE_FAILED;}state=c->lease.phase;if(invalidated)*invalidated=c->lease.invalidated;}
 LeaveCriticalSection(&c->lock);return state;
}
void xideLanguageClientInvalidateAnalysis(xide_language_client* c,int64_t token)
{if(c){EnterCriticalSection(&c->lock);if(token>0 && c->lease.token==token)c->lease.invalidated=1;LeaveCriticalSection(&c->lock);}}
void xideLanguageClientRequestStop(xide_language_client* c)
{if(c){InterlockedExchange(&c->stop,1);signalTransport(c);}}
int xideLanguageClientStopped(xide_language_client* c)
{return !c || !c->worker || WaitForSingleObject(c->worker,0)==WAIT_OBJECT_0;}
int xideLanguageClientResumeAnalysis(xide_language_client* c,int64_t token)
{
 if(!c)return 0;
 EnterCriticalSection(&c->lock);int okay=token>0 && c->lease.token==token,stop=0;
 if(okay){c->lease.wantResume=1;stop=c->lease.failed || c->state>=XIDE_LS_STOPPED;if(!stop && !c->lease.resumeAck)c->lease.phase=XIDE_LS_LEASE_RESUMING;}
 LeaveCriticalSection(&c->lock);if(okay){if(stop)xideLanguageClientRequestStop(c);else signalTransport(c);}return okay;
}
int xideLanguageClientReleaseAnalysis(xide_language_client* c,int64_t token)
{
 if(!c)return 0;
 EnterCriticalSection(&c->lock);int okay=token>0 && c->lease.token==token && c->lease.wantResume && (c->lease.resumeAck || (c->state>=XIDE_LS_STOPPED && xideLanguageClientStopped(c)));
 if(okay){c->lease.token=0;c->lease.phase=XIDE_LS_LEASE_STALE;}LeaveCriticalSection(&c->lock);if(okay)signalTransport(c);return okay;
}
int xideLanguageClientAnalysisCanRelease(xide_language_client* c,int64_t token)
{
 if(!c)return 0;
 EnterCriticalSection(&c->lock);int ready=token>0 && c->lease.token==token && c->lease.wantResume && (c->lease.resumeAck || (c->state>=XIDE_LS_STOPPED && xideLanguageClientStopped(c)));LeaveCriticalSection(&c->lock);return ready;
}
