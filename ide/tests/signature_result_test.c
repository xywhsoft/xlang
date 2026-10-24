#include "services/signature_result.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
static xatomic32 failAllocation;
static void* faultCalloc(size_t count,size_t size)
{int n=xrtAtomic32Load(&failAllocation,XMEMORY_RELAXED);if(n>0 && xrtAtomic32FetchSub(&failAllocation,1,XMEMORY_RELAXED)==1)return NULL;return xrtCalloc(count,size);}
#define xrtCalloc faultCalloc
#include "../src/services/signature_result.c"
#undef xrtCalloc
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"signature_result_test:%d %s\n",__LINE__,#v);return 0;}}while(0)
static xvalue* parse(const char* s){return xrtJsonParse(xrtStrView(s));}
static xvalue* get(xvalue* v,const char* k){return (xvalue*)field(v,k);}
static int put(xvalue* v,const char* k,xvalue* item){return item && xrtValueObjectSetNew(v,xrtStrView(k),item);}
static xvalue* message(int strings)
{
 return parse(strings ? "{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{\"version\":3,\"queryId\":7,\"activeSignature\":0,\"activeParameter\":1,\"signatures\":[{\"label\":\"make(\\\"😀\\\")(int 数字, string 文本)\",\"documentation\":{\"kind\":\"plaintext\",\"value\":\"说明😀\"},\"parameters\":[{\"label\":\"int 数字\"},{\"label\":\"string 文本\",\"documentation\":\"参数文档\"}]}]}}" :
 "{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{\"version\":3,\"queryId\":7,\"activeSignature\":0,\"activeParameter\":1,\"signatures\":[{\"label\":\"make(\\\"😀\\\")(int 数字, string 文本)\",\"documentation\":\"说明😀\",\"parameters\":[{\"label\":[11,17]},{\"label\":[19,28],\"documentation\":\"参数文档\"}]}]}}");
}
static int invalid(xls_text* text,xvalue* m,xide_signature_expected expected)
{xide_signature_result* r=xideSignatureDecode(text,m,expected,NULL,NULL);CHECK(r && !r->valid && !r->available && !r->count && !r->items && !r->message);xideSignatureResultRelease(r);return 1;}
static int cancelAt(void* user){return --*(int*)user==0;}
typedef struct dropped {xatomic32 count,thread;HANDLE entered,release;} dropped;
static void drop(ptr value,ptr user)
{dropped* d=user;if(value!=d)return;if(d->entered){SetEvent(d->entered);WaitForSingleObject(d->release,10000);}xrtAtomic32Store(&d->thread,(int32)GetCurrentThreadId(),XMEMORY_RELAXED);xrtAtomic32FetchAdd(&d->count,1,XMEMORY_RELEASE);}
static xvalue* handle(dropped* d){static const xvaluehandleops ops={.Drop=drop};ptr p=d;return xrtValueHandleTake(&p,&ops,d);}
static int waitDrop(dropped* d){uint64_t until=GetTickCount64()+10000;while(!xrtAtomic32Load(&d->count,XMEMORY_ACQUIRE) && GetTickCount64()<until)Sleep(1);return xrtAtomic32Load(&d->count,XMEMORY_ACQUIRE)==1;}
static xide_signature_result* take(xide_signature_decoder* d){uint64_t until=GetTickCount64()+10000;xide_signature_result* r;do{r=xideSignatureDecoderTake(d);if(r)return r;Sleep(1);}while(GetTickCount64()<until);return NULL;}
static int runTests(void)
{
 const char* source="\xef\xbb\xbf""func main(...) as int\r\n return make(\"😀\")(7)\r\nend\r\n";xls_text* text=xlsTextCreate(source,strlen(source),4096,NULL);CHECK(text);xide_signature_expected expected={7,3,3};
 for(int strings=0;strings<2;++strings){xvalue* m=message(strings);CHECK(m);xide_signature_result* r=xideSignatureDecode(text,m,expected,NULL,NULL);CHECK(r && r->valid && r->available && r->count==1 && r->activeSignature==0 && r->items[0].activeParameter==1);const xide_signature_item* item=r->items;CHECK(item->parameters[0].start==13 && item->parameters[0].end==23 && item->parameters[1].start==25 && item->parameters[1].end==38);CHECK(!strcmp(item->documentation,"说明😀") && !strcmp(item->parameters[1].documentation,"参数文档"));xrtValueRelease(m);CHECK(strstr(item->label,"数字"));xideSignatureResultRelease(r);}
 /* Mixed label representations retain one forward Unicode cursor. */
 for(int firstString=0;firstString<2;++firstString){xvalue* m=message(0);CHECK(m);xvalue* sig=xrtValueArrayGet(get(get(m,"result"),"signatures"),0);xvalue* p=xrtValueArrayGet(get(sig,"parameters"),firstString ? 0 : 1);CHECK(put(p,"label",parse(firstString ? "\"int 数字\"" : "\"string 文本\"")));xide_signature_result* r=xideSignatureDecode(text,m,expected,NULL,NULL);CHECK(r && r->valid && r->items[0].parameters[1].start==25 && r->items[0].parameters[1].end==38);xideSignatureResultRelease(r);xrtValueRelease(m);}
 /* Exact capacity succeeds; excess is rejected without a partial result. */
 {xvalue* m=message(0);CHECK(m);xvalue* list=get(get(m,"result"),"signatures");xvalue* sig=xrtValueArrayGet(list,0);for(size_t i=1;i<SIGNATURES;++i)CHECK(xrtValueArrayAppend(list,sig));xide_signature_result* r=xideSignatureDecode(text,m,expected,NULL,NULL);CHECK(r && r->valid && r->count==SIGNATURES);xideSignatureResultRelease(r);CHECK(xrtValueArrayAppend(list,sig) && invalid(text,m,expected));xrtValueRelease(m);}
 {xvalue* m=message(0);CHECK(m);xvalue* sig=xrtValueArrayGet(get(get(m,"result"),"signatures"),0);xvalue* list=xrtValueArray();CHECK(list && put(sig,"parameters",list));for(size_t i=0;i<PARAMETERS;++i)CHECK(xrtValueArrayAppendNew(list,parse("{\"label\":[0,0]}")));xide_signature_result* r=xideSignatureDecode(text,m,expected,NULL,NULL);CHECK(r && r->valid && r->items[0].count==PARAMETERS);xideSignatureResultRelease(r);CHECK(xrtValueArrayAppendNew(list,parse("{\"label\":[0,0]}")) && invalid(text,m,expected));char label[LABEL_BYTES+1];memset(label,'a',sizeof(label));CHECK(put(sig,"parameters",xrtValueArray()) && put(sig,"label",xrtValueString((xstrview){label,LABEL_BYTES})));r=xideSignatureDecode(text,m,expected,NULL,NULL);CHECK(r && r->valid);xideSignatureResultRelease(r);CHECK(put(sig,"label",xrtValueString((xstrview){label,sizeof(label)})) && invalid(text,m,expected));xrtValueRelease(m);}
 for(int mode=0;mode<22;++mode){xvalue* m=message(0);CHECK(m);xvalue* body=get(m,"result");xvalue* sig=xrtValueArrayGet(get(body,"signatures"),0);xvalue* param=xrtValueArrayGet(get(sig,"parameters"),1);
  if(mode==0)put(m,"jsonrpc",parse("\"wrong\""));
  if(mode==1)put(m,"id",xrtValueInt(8));
  if(mode==2)put(m,"error",xrtValueNull());
  if(mode==3)put(body,"version",xrtValueInt(4));
  if(mode==4)put(body,"queryId",xrtValueInt(9));
  if(mode==5)put(body,"signatures",xrtValueObject());
  if(mode==6)put(sig,"label",xrtValueString((xstrview){"a\0b",3}));
  if(mode==7)put(sig,"label",xrtValueString((xstrview){"\xc0\x80",2}));
  if(mode==8)put(sig,"parameters",xrtValueObject());
  if(mode==9)put(param,"label",parse("[28,19]"));
  if(mode==10)put(param,"label",parse("[19,999]"));
  if(mode==11)put(param,"label",parse("[-1,28]"));
  if(mode==12)put(param,"label",parse("[19,28,29]"));
  if(mode==13)put(param,"label",parse("\"missing\""));
  if(mode==14)put(sig,"documentation",xrtValueInt(7));
  if(mode==15)put(param,"documentation",parse("{\"kind\":\"html\",\"value\":\"unsafe\"}"));
  if(mode==16)put(body,"activeParameter",xrtValueInt(-1));
  if(mode==17)put(body,"activeSignature",xrtValueFloat(.5));
  if(mode==18)put(sig,"activeParameter",xrtValueInt(-1));
  if(mode==19)put(param,"label",parse("\"\""));
  if(mode==20)put(xrtValueArrayGet(get(sig,"parameters"),0),"label",parse("[7,17]"));
  if(mode==21)put(param,"label",parse("[12,28]"));
  CHECK(invalid(text,m,expected));xrtValueRelease(m);
 }
 xvalue* m=message(0);CHECK(m);for(int i=0;i<5;++i){xide_signature_expected e=expected;if(i==0)e.request=1;if(i==1)e.version=0;if(i==2)e.offset=-1;if(i==3)e.offset=1;if(i==4)e.offset=999;CHECK(invalid(text,m,e));}
 for(int fail=1;fail<=3;++fail){xrtAtomic32Store(&failAllocation,fail,XMEMORY_RELAXED);xide_signature_result* r=xideSignatureDecode(text,m,expected,NULL,NULL);xrtAtomic32Store(&failAllocation,0,XMEMORY_RELAXED);CHECK(fail==1 ? !r : r && !r->valid && !r->message && !r->items);xideSignatureResultRelease(r);}
 int at=1;xide_signature_result* r=xideSignatureDecode(text,m,expected,cancelAt,&at);CHECK(r && !r->valid && !r->message);xideSignatureResultRelease(r);xrtValueRelease(m);
 m=parse("{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":null}");CHECK(m);r=xideSignatureDecode(text,m,expected,NULL,NULL);CHECK(r && r->valid && !r->available && !r->message);xideSignatureResultRelease(r);xrtValueRelease(m);
 dropped old={0},oversize={0};xrtAtomic32Init(&old.count,0);xrtAtomic32Init(&old.thread,0);xrtAtomic32Init(&oversize.count,0);xrtAtomic32Init(&oversize.thread,0);xide_signature_decoder* d=xideSignatureDecoderCreate(NULL,NULL);CHECK(d);
 CHECK(xideSignatureDecoderExpect(d,1,xlsTextRetain(text),expected));m=message(0);CHECK(m && put(m,"owned",handle(&old)));CHECK(xideSignatureDecoderReceiveOwned(m,1024,d));m=NULL;r=take(d);CHECK(r && r->valid && r->token==1);
 CHECK(xideSignatureDecoderRetire(d,r));r=NULL;CHECK(waitDrop(&old) && (DWORD)xrtAtomic32Load(&old.thread,XMEMORY_ACQUIRE)!=GetCurrentThreadId());
 CHECK(xideSignatureDecoderExpect(d,2,xlsTextRetain(text),expected));m=message(0);CHECK(m && put(m,"owned",handle(&oversize)) && xideSignatureDecoderReceiveOwned(m,RESPONSE_BYTES+1,d));m=NULL;r=take(d);CHECK(r && !r->valid && r->token==2 && waitDrop(&oversize) && (DWORD)xrtAtomic32Load(&oversize.thread,XMEMORY_ACQUIRE)!=GetCurrentThreadId());xideSignatureResultRelease(r);
 CHECK(xideSignatureDecoderExpect(d,3,xlsTextRetain(text),expected));xideSignatureDecoderCancel(d);m=message(0);CHECK(m && !xideSignatureDecoderReceiveOwned(m,1024,d));xrtValueRelease(m);xideSignatureDecoderDestroy(d);
 xlsTextRelease(text);return 1;
}
int main(void){if(!runTests())return 1;printf("signature_result_test passed: %u checks; Unicode tuples/stringlabels,strict schemas,source boundaries,allocation faults,worker handoff/retirement/oversize/cancel\n",checks);return 0;}
