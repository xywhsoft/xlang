/* Private actual decoder gives deterministic URI gates and verifies that
   accepted DOMs are disposed on its worker,including cancellation/shutdown. */
#include "app_xrt.h"
#include "protocol/lsp_wire.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
static xatomic32 gated;
static HANDLE entered,resume;
static xvalue* tracked[5];static uint64_t released[5];static size_t trackedCount;
static char* fixtureUri(const char* uri,xide_model_error* error)
{if(xrtAtomic32Load(&gated,XMEMORY_ACQUIRE)==1){xrtAtomic32Store(&gated,2,XMEMORY_RELEASE);SetEvent(entered);WaitForSingleObject(resume,5000);}return xideLspUriToPath(uri,error);}
static void fixtureRelease(xvalue* value)
{for(size_t i=0;i<trackedCount;++i)if(value==tracked[i])released[i]=xrtThreadCurrentId();xrtValueRelease(value);}
#define xideLspUriToPath fixtureUri
#define xrtValueRelease fixtureRelease
#include "../src/services/reference_results.c"
#undef xideLspUriToPath
#undef xrtValueRelease
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"reference_results_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int prop(xvalue* o,const char* key,xvalue* v){return v && xrtValueObjectSetNew(o,xrtStrView(key),v);}
static xvalue* point(int line,int character)
{xvalue* v=xrtValueObject();if(!v || !prop(v,"line",xrtValueInt(line)) || !prop(v,"character",xrtValueInt(character))){xrtValueRelease(v);return NULL;}return v;}
static xvalue* location(const char* path,int line,int character)
{
 static const char zeros[]="0000000000000000000000000000000000000000000000000000000000000000";xvalue *row=xrtValueObject(),*range=xrtValueObject(),*proof=xrtValueObject(),*doc=xrtValueObject();char* uri=xideLspPathToUri(path,NULL);int okay=row && range && proof && doc && uri && prop(row,"uri",xrtValueString(xrtStrView(uri))) && prop(range,"start",point(line,character)) && prop(range,"end",point(line,character+7)) && xrtValueObjectSet(row,xrtStrView("range"),range) && prop(proof,"schema",xrtValueInt(1)) && prop(doc,"path",xrtValueString(xrtStrView(path))) && prop(doc,"origin",xrtValueString(xrtStrView("overlay"))) && prop(doc,"version",xrtValueInt(1)) && prop(doc,"normalizedSize",xrtValueInt(1024*1024)) && prop(doc,"normalizedSha256",xrtValueString(xrtStrView(zeros))) && xrtValueObjectSet(proof,xrtStrView("document"),doc) && xrtValueObjectSet(row,xrtStrView("xlang"),proof);
 free(uri);xrtValueRelease(doc);xrtValueRelease(proof);xrtValueRelease(range);if(!okay){xrtValueRelease(row);return NULL;}return row;
}
static xvalue* reply(const char* path,size_t count)
{xvalue *message=xrtValueObject(),*rows=xrtValueArray();int okay=message && rows && prop(message,"jsonrpc",xrtValueString(xrtStrView("2.0"))) && prop(message,"id",xrtValueInt(7));for(size_t i=0;okay && i<count;++i)okay=xrtValueArrayAppendNew(rows,location(path,(int)(count-i),2));if(okay){okay=prop(message,"result",rows);rows=NULL;}xrtValueRelease(rows);if(!okay){xrtValueRelease(message);return NULL;}return message;}
static int valid(xvalue* message,int expected,size_t count)
{xide_reference_result* result=xideReferenceDecode(message,NULL,NULL);CHECK(result && result->valid==expected && (!expected || result->count==count));xideReferenceResultRelease(result);return 1;}
static int cancel(void* user){return ++*(int*)user>=3;}
static int fixtureRun(void)
{
 char* path=xrtPathAbs("build/reference-results/未打开.xl");CHECK(path);CHECK(valid(reply(path,0),1,0));xvalue* message=reply(path,0);CHECK(prop(message,"result",xrtValueNull()) && valid(message,1,0));CHECK(valid(reply(path,32),1,32));
 message=reply(path,3);xvalue* rows=(xvalue*)field(message,"result");CHECK(xrtValueArrayAppend(rows,xrtValueArrayGet(rows,0)));xide_reference_result* result=xideReferenceDecode(message,NULL,NULL);CHECK(result && result->valid && result->count==3);CHECK(result->items[0].start.line==1 && result->items[2].start.line==3);CHECK(xideReferenceResultFind(result,result->items[1].key,result->items[1].start,result->items[1].end)==1);xide_lsp_position missing={.line=700,.character=0};CHECK(xideReferenceResultFind(result,result->items[0].key,missing,missing)==SIZE_MAX);CHECK(xidePathLexicalEqual(result->items[0].path,path) && strstr(result->items[0].label,"(2:3)"));xideReferenceResultRelease(result);
 for(unsigned test=0;test<16;++test){message=reply(path,1);xvalue* row=xrtValueArrayGet(field(message,"result"),0);xvalue* doc=(xvalue*)field(field(row,"xlang"),"document");xvalue* range=(xvalue*)field(row,"range");
  if(test==0)prop(row,"uri",xrtValueString(xrtStrView("https://example.invalid/a.xl")));
  if(test==1)prop(range,"end",point(1,2));
  if(test==2)prop(range,"start",point(-1,0));
  if(test==3)prop(row,"xlang",xrtValueNull());
  if(test==4)prop(doc,"path",xrtValueString(xrtStrView("C:/different.xl")));
  if(test==5)prop(doc,"normalizedSha256",xrtValueString(xrtStrView("abc")));
  if(test==6)prop(doc,"normalizedSize",xrtValueInt(8*1024*1024+1));
  if(test==7)prop(doc,"input",xrtValueObject());
  if(test==8)prop(doc,"version",xrtValueInt((int64)INT_MAX+1));
  if(test==9)prop(doc,"origin",xrtValueString(xrtStrView("unknown")));
  if(test==10)prop(message,"error",xrtValueObject());
  if(test==11)prop(message,"result",xrtValueObject());
  if(test==12)prop(message,"jsonrpc",xrtValueString(xrtStrView("1.0")));
  if(test==13)prop(message,"method",xrtValueNull());
  if(test==14)prop(message,"id",xrtValueString(xrtStrView("7")));
  if(test==15)prop(message,"id",xrtValueInt(0));
  CHECK(valid(message,0,0));
 }
 for(unsigned i=0;i<2;++i){message=reply(path,1);xvalue* row=xrtValueArrayGet(field(message,"result"),0);xvalue* doc=(xvalue*)field(field(row,"xlang"),"document");CHECK(prop(doc,"version",xrtValueInt(i?INT_MAX:INT_MIN)) && valid(message,1,1));}
 message=reply(path,1);rows=(xvalue*)field(message,"result");xvalue* repeated=xrtValueArrayGet(rows,0);for(size_t i=1;i<65537;++i)CHECK(xrtValueArrayAppend(rows,repeated));CHECK(valid(message,0,0));
 int cancelled=0;result=xideReferenceDecode(reply(path,100),cancel,&cancelled);CHECK(result && !result->valid);xideReferenceResultRelease(result);
 xvalue *a=reply(path,1024),*b=reply(path,3),*c=reply(path,3),*d=reply(path,2),*e=reply(path,5),*g=reply(path,512);CHECK(a && b && c && d && e && g);tracked[0]=a;tracked[1]=b;tracked[2]=c;tracked[3]=e;tracked[4]=g;trackedCount=5;result=xideReferenceDecode(c,NULL,NULL);CHECK(result && result->valid);
 entered=CreateEventW(NULL,FALSE,FALSE,NULL);resume=CreateEventW(NULL,FALSE,FALSE,NULL);CHECK(entered && resume);xide_reference_decoder* decoder=xideReferenceDecoderCreate(NULL,NULL);CHECK(decoder && !xideReferenceDecoderArm(decoder,0,7) && !xideReferenceDecoderArm(decoder,1,0));xrtAtomic32Store(&gated,1,XMEMORY_RELEASE);CHECK(xideReferenceDecoderArm(decoder,10,7) && xideReferenceDecoderReceive(a,1000,decoder));CHECK(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);CHECK(xideReferenceDecoderArm(decoder,11,8) && !xideReferenceDecoderReceive(b,1000,decoder));CHECK(prop(b,"id",xrtValueInt(8)) && xideReferenceDecoderReceive(b,1000,decoder));xvalue* duplicate=reply(path,1);CHECK(duplicate && prop(duplicate,"id",xrtValueInt(8)) && xideReferenceDecoderReceive(duplicate,1000,decoder));CHECK(!xideReferenceDecoderReceive(d,1000,decoder));CHECK(!xideReferenceDecoderSubmit(decoder,12,d) && xrtValueType(d)==XVALUE_OBJECT && xideReferenceDecoderReceived(decoder)==2);xrtValueRelease(d);xideReferenceDecoderCancel(decoder);CHECK(!xideReferenceDecoderTake(decoder) && !xideReferenceDecoderHasResult(decoder));CHECK(xideReferenceDecoderRetire(decoder,result));xide_reference_result* extra=xideReferenceDecode(reply(path,0),NULL,NULL);CHECK(extra && !xideReferenceDecoderRetire(decoder,extra));xideReferenceResultRelease(extra);SetEvent(resume);
 int submitted=0;for(unsigned i=0;i<3000 && !submitted;++i){submitted=xideReferenceDecoderSubmit(decoder,13,e);if(!submitted)xrtSleep(1);}CHECK(submitted);result=NULL;for(unsigned i=0;i<3000 && !result;++i){result=xideReferenceDecoderTake(decoder);if(!result)xrtSleep(1);}CHECK(result && result->valid && result->token==13 && result->count==5);CHECK(xideReferenceDecoderRetire(decoder,result));CHECK(xideReferenceDecoderSubmit(decoder,0,g));xideReferenceDecoderDestroy(decoder);
 for(size_t i=0;i<5;++i)CHECK(released[i] && released[i]!=xrtThreadCurrentId());
 trackedCount=0;CloseHandle(entered);CloseHandle(resume);xrtFree(path);return 1;
}
int main(void){if(!fixtureRun())return 1;printf("reference_results_test passed: %u checks;complete bounded proof decoding,Unicode/signed wire versions/sort/dedup/binary lookup,malformed/cancel,deterministic queue and worker-only disposal/shutdown\n",checks);return 0;}
