#include "services/diagnostic_dependencies.h"
#include "services/diagnostic_notice.h"
#include "platform/path.h"
#include "protocol/lsp_wire.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(v) do {++checks;if(!(v)){fprintf(stderr,"diagnostic_dependencies_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static const char* pairId="1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee";
static const char* emptyId="e3d55c9066bce93a2936028873706e9f77d091b1c2bc26898cd070bfb0a55408";
static const char* first="D:/dep.xl",*second="D:/工程😀/main.xl";
static int put(xvalue* value,const char* name,xvalue* owned)
{return owned && xrtValueObjectSetNew(value,xrtStrView(name),owned);}
static const xvalue* get(const xvalue* value,const char* name)
{return value && xrtValueType(value)==XVALUE_OBJECT ? xrtValueObjectGet(value,xrtStrView(name)) : NULL;}
static const char* text(const xvalue* value)
{xstrview s={0};return xrtValueGetString(value,&s) ? s.Data : NULL;}
static xvalue* part(const char* id,int index,int total,const char* const* paths,size_t count)
{
 xvalue* out=xrtValueObject(),*array=xrtValueArray();int okay=out && array;
 for(size_t i=0;okay && i<count;++i) {xvalue* path=xrtValueString(xrtStrView(paths[i]));okay=path && xrtValueArrayAppend(array,path);xrtValueRelease(path);}
 okay=okay && put(out,"schema",xrtValueInt(1)) && put(out,"id",xrtValueString(xrtStrView(id))) &&
  put(out,"removed",xrtValueBool(false)) && put(out,"part",xrtValueInt(index)) && put(out,"parts",xrtValueInt(total)) && xrtValueObjectSet(out,xrtStrView("paths"),array);
 xrtValueRelease(array);if(!okay) {xrtValueRelease(out);out=NULL;}return out;
}
static xvalue* removed(const char* id)
{
 xvalue* out=xrtValueObject();if(out && put(out,"schema",xrtValueInt(1)) && put(out,"id",xrtValueString(xrtStrView(id))) && put(out,"removed",xrtValueBool(true))) return out;
 xrtValueRelease(out);return NULL;
}
static int unchanged(xide_diagnostic_dependencies* d,xvalue* p,enum xide_dependency_message expected)
{
 xide_dependency_stats a,b;xideDiagnosticDependenciesStats(d,&a);
 CHECK(xideDiagnosticDependenciesMessage(d,p,NULL,NULL)==expected);xideDiagnosticDependenciesStats(d,&b);
 CHECK(!memcmp(&a,&b,sizeof(a)));if(expected==XIDE_DEPENDENCY_ERROR) CHECK(*xideDiagnosticDependenciesError(d));return 1;
}
typedef struct consumer {const xide_diagnostic_dependency* p;const wchar_t* key;int failed;} consumer;
static int consume(void* user)
{
 consumer* c=user;for(unsigned i=0;i<1000;++i) {
  xide_diagnostic_dependency* p=xideDiagnosticDependencyRetain(c->p);
  if(!p || strcmp(xideDiagnosticDependencyId(p),pairId) || xideDiagnosticDependencyCount(p)!=2 ||
    strcmp(xideDiagnosticDependencyPath(p,0),first) || strcmp(xideDiagnosticDependencyPath(p,1),second) || !xideDiagnosticDependencyContainsKey(p,c->key)) c->failed=1;
  xideDiagnosticDependencyRelease(p);
 }return 0;
}
static int lifecycle(void)
{
 const char* values[]={first,second};xvalue *a=part(pairId,0,2,values,1),*b=part(pairId,1,2,values+1,1),*del=removed(pairId);CHECK(a && b && del);
 xide_diagnostic_dependencies* d=xideDiagnosticDependenciesCreate(NULL);CHECK(d);
 CHECK(!xideDiagnosticDependenciesFind(d,pairId) && unchanged(d,b,XIDE_DEPENDENCY_ERROR));
 CHECK(xideDiagnosticDependenciesMessage(d,a,NULL,NULL)==XIDE_DEPENDENCY_PART && !xideDiagnosticDependenciesFind(d,pairId));
 CHECK(!xideDiagnosticDependenciesReference(d,first,pairId) && unchanged(d,a,XIDE_DEPENDENCY_ERROR) && unchanged(d,del,XIDE_DEPENDENCY_ERROR));
 xvalue* corrupt=xrtValueDeepClone(b);CHECK(corrupt);
 CHECK(put(corrupt,"paths",xrtValueArray()));CHECK(unchanged(d,corrupt,XIDE_DEPENDENCY_ERROR));xrtValueRelease(corrupt);
 CHECK(xideDiagnosticDependenciesMessage(d,b,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE);
 xide_diagnostic_dependency* lease=xideDiagnosticDependencyRetain(xideDiagnosticDependenciesFind(d,pairId));CHECK(lease && !strcmp(xideDiagnosticDependencyId(lease),pairId) && xideDiagnosticDependencyCount(lease)==2 && !xideDiagnosticDependencyPath(lease,2));
 CHECK(xideDiagnosticDependenciesReference(d,first,pairId));CHECK(xideDiagnosticDependenciesReference(d,"d:/DEP.xl",pairId));
 xide_dependency_stats stats;xideDiagnosticDependenciesStats(d,&stats);CHECK(stats.dictionaries==1 && !stats.pending && stats.bindings==1);
 CHECK(!xideDiagnosticDependenciesReference(d,"D:/unrelated.xl",pairId) && !xideDiagnosticDependenciesReference(d,"relative.xl",pairId) && !xideDiagnosticDependenciesReference(d,first,emptyId));
 CHECK(unchanged(d,del,XIDE_DEPENDENCY_ERROR) && unchanged(d,b,XIDE_DEPENDENCY_ERROR));
 CHECK(xideDiagnosticDependenciesReference(d,second,pairId));CHECK(xideDiagnosticDependenciesReference(d,"D:/unknown.xl",NULL));
 CHECK(xideDiagnosticDependenciesReference(d,"D:/./dep.xl",NULL));CHECK(unchanged(d,del,XIDE_DEPENDENCY_ERROR));
 CHECK(xideDiagnosticDependenciesReference(d,second,NULL));CHECK(xideDiagnosticDependenciesMessage(d,del,NULL,NULL)==XIDE_DEPENDENCY_REMOVED && !xideDiagnosticDependenciesFind(d,pairId));
 CHECK(xideDiagnosticDependenciesMessage(d,a,NULL,NULL)==XIDE_DEPENDENCY_PART && xideDiagnosticDependenciesMessage(d,b,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE);
 CHECK(xideDiagnosticDependenciesFind(d,pairId)!=lease);xideDiagnosticDependenciesDestroy(d);
 wchar_t* key=xidePathNativeWide(second);CHECK(key && xideDiagnosticDependencyContainsKey(lease,key));
 consumer consumers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i) {consumers[i]=(consumer){lease,key,0};threads[i]=xrtThreadCreate(consume,consumers+i,0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i) {CHECK(xrtThreadWait(threads[i])==XWAIT_OK && !consumers[i].failed);xrtThreadDestroy(threads[i]);}
 free(key);xideDiagnosticDependencyRelease(lease);xrtValueRelease(a);xrtValueRelease(b);xrtValueRelease(del);return 1;
}
static int cancelAt(void* user) {size_t* at=user;++at[0];return at[0]>=at[1];}
static int cancellation(void)
{
 const char* values[]={first,second};xvalue *a=part(pairId,0,2,values,1),*b=part(pairId,1,2,values+1,1);CHECK(a && b);size_t cancelled=0,completed=0;
 for(size_t stop=1;stop<=96;++stop) {
  xide_diagnostic_dependencies* d=xideDiagnosticDependenciesCreate(NULL);CHECK(d && xideDiagnosticDependenciesMessage(d,a,NULL,NULL)==XIDE_DEPENDENCY_PART);
  xide_dependency_stats before,after;xideDiagnosticDependenciesStats(d,&before);size_t calls[]={0,stop};
  enum xide_dependency_message result=xideDiagnosticDependenciesMessage(d,b,cancelAt,calls);
  if(result==XIDE_DEPENDENCY_CANCELLED) {++cancelled;xideDiagnosticDependenciesStats(d,&after);CHECK(!memcmp(&before,&after,sizeof(before)) && !xideDiagnosticDependenciesFind(d,pairId));CHECK(xideDiagnosticDependenciesMessage(d,b,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE);}
  else {++completed;CHECK(result==XIDE_DEPENDENCY_COMPLETE && calls[0]<stop);}
  CHECK(xideDiagnosticDependencyCount(xideDiagnosticDependenciesFind(d,pairId))==2);xideDiagnosticDependenciesDestroy(d);
 }
 CHECK(cancelled && completed);xrtValueRelease(a);xrtValueRelease(b);return 1;
}
static int malformed(void)
{
 const char* values[]={first,second};xvalue* valid=part(pairId,0,1,values,2);CHECK(valid);xide_diagnostic_dependencies* d=xideDiagnosticDependenciesCreate(NULL);CHECK(d);
 const char* fields[]={"schema","id","removed","part","parts","paths"};
 for(size_t i=0;i<6;++i) for(size_t bad=0;bad<5;++bad) {
  xvalue* p=xrtValueDeepClone(valid);CHECK(p);xvalue* wrong=bad==0 ? xrtValueNull() : bad==1 ? xrtValueArray() : bad==2 ? xrtValueBool(true) : bad==3 ? xrtValueFloat(1.0) : xrtValueString(xrtStrView("bad"));
  CHECK(put(p,fields[i],wrong));
  CHECK(unchanged(d,p,XIDE_DEPENDENCY_ERROR));
  xrtValueRelease(p);
 }
 xvalue* p=xrtValueDeepClone(valid);CHECK(p && put(p,"extra",xrtValueInt(0)) && unchanged(d,p,XIDE_DEPENDENCY_ERROR));xrtValueRelease(p);
 for(size_t i=0;i<5;++i) {
  p=xrtValueDeepClone(valid);CHECK(p);const char* field=i<3 ? "parts" : "part";int value=i==0 ? 0 : i==1 ? -1 : i==2 ? 65537 : i==3 ? -1 : 1;
  CHECK(put(p,field,xrtValueInt(value)) && unchanged(d,p,XIDE_DEPENDENCY_ERROR));xrtValueRelease(p);
 }
 const char* invalid[]={"relative.xl","D:drive-relative.xl","D:/bad\xff","D:/dep.xl"};
 for(size_t i=0;i<4;++i) {const char* pair[]={i==3 ? second : invalid[i],first};p=part(pairId,0,1,pair,2);CHECK(p && unchanged(d,p,XIDE_DEPENDENCY_ERROR));xrtValueRelease(p);}
 const char* aliases[]={first,"d:/DEP.xl"};p=part(pairId,0,1,aliases,2);CHECK(p && unchanged(d,p,XIDE_DEPENDENCY_ERROR));xrtValueRelease(p);
 p=xrtValueDeepClone(valid);xvalue* list=xrtValueArray(),*nul=xrtValueString((xstrview){"D:/a\0b",6});CHECK(p && list && nul && xrtValueArrayAppend(list,nul) && xrtValueObjectSet(p,xrtStrView("paths"),list));CHECK(unchanged(d,p,XIDE_DEPENDENCY_ERROR));xrtValueRelease(p);xrtValueRelease(list);xrtValueRelease(nul);
 CHECK(unchanged(d,NULL,XIDE_DEPENDENCY_ERROR));p=xrtValueArray();CHECK(p && unchanged(d,p,XIDE_DEPENDENCY_ERROR));xrtValueRelease(p);
 CHECK(xideDiagnosticDependenciesMessage(d,valid,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE);xideDiagnosticDependenciesDestroy(d);xrtValueRelease(valid);return 1;
}
static int limits(void)
{
 const char* values[]={first,second};xvalue* valid=part(pairId,0,1,values,2);xvalue* empty=part(emptyId,0,1,NULL,0);xvalue* remove=removed(emptyId);CHECK(valid && empty && remove);
 xide_dependency_limits l={8192,64u*1024u*1024u,163840,8192,16u*1024u*1024u};
 size_t* fields[]={&l.dictionaries,&l.encodedBytes,&l.pathsPerDictionary,&l.bindings,&l.bindingKeyBytes};
 for(size_t i=0;i<5;++i) {size_t original=*fields[i];*fields[i]=0;CHECK(!xideDiagnosticDependenciesCreate(&l));*fields[i]=original+1;CHECK(!xideDiagnosticDependenciesCreate(&l));*fields[i]=original;}
 l.dictionaries=1;xide_diagnostic_dependencies* d=xideDiagnosticDependenciesCreate(&l);CHECK(d && xideDiagnosticDependenciesMessage(d,empty,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE && unchanged(d,valid,XIDE_DEPENDENCY_ERROR));
 CHECK(xideDiagnosticDependenciesMessage(d,remove,NULL,NULL)==XIDE_DEPENDENCY_REMOVED && xideDiagnosticDependenciesMessage(d,valid,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE);xideDiagnosticDependenciesDestroy(d);l.dictionaries=8192;
 l.pathsPerDictionary=1;d=xideDiagnosticDependenciesCreate(&l);CHECK(d && unchanged(d,valid,XIDE_DEPENDENCY_ERROR));xideDiagnosticDependenciesDestroy(d);l.pathsPerDictionary=163840;
 size_t n=0;char* json=xrtJsonStringify(valid,false,&n);CHECK(json && n);xrtFree(json);
 l.encodedBytes=n-1;d=xideDiagnosticDependenciesCreate(&l);CHECK(d && unchanged(d,valid,XIDE_DEPENDENCY_ERROR));xideDiagnosticDependenciesDestroy(d);
 l.encodedBytes=n;d=xideDiagnosticDependenciesCreate(&l);CHECK(d && xideDiagnosticDependenciesMessage(d,valid,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE);xide_dependency_stats stats;xideDiagnosticDependenciesStats(d,&stats);CHECK(stats.encodedBytes==n);xideDiagnosticDependenciesDestroy(d);l.encodedBytes=64u*1024u*1024u;
 l.bindings=1;d=xideDiagnosticDependenciesCreate(&l);CHECK(d && xideDiagnosticDependenciesMessage(d,valid,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE && xideDiagnosticDependenciesReference(d,first,pairId) && !xideDiagnosticDependenciesReference(d,second,pairId));xideDiagnosticDependenciesStats(d,&stats);CHECK(stats.bindings==1);xideDiagnosticDependenciesDestroy(d);l.bindings=8192;
 wchar_t* key=xidePathNativeWide(first);CHECK(key);n=(wcslen(key)+1)*sizeof(wchar_t);free(key);
 l.bindingKeyBytes=n-1;d=xideDiagnosticDependenciesCreate(&l);CHECK(d && xideDiagnosticDependenciesMessage(d,valid,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE && !xideDiagnosticDependenciesReference(d,first,pairId));xideDiagnosticDependenciesDestroy(d);
 l.bindingKeyBytes=n;d=xideDiagnosticDependenciesCreate(&l);CHECK(d && xideDiagnosticDependenciesMessage(d,valid,NULL,NULL)==XIDE_DEPENDENCY_COMPLETE && xideDiagnosticDependenciesReference(d,first,pairId));xideDiagnosticDependenciesStats(d,&stats);CHECK(stats.bindingKeyBytes==n);xideDiagnosticDependenciesDestroy(d);
 char* huge=xrtMalloc(65536);CHECK(huge);memset(huge,'"',65535);memcpy(huge,"D:/",3);huge[65535]=0;const char* large[]={huge,huge};xvalue* oversized=part(pairId,0,1,large,2);CHECK(oversized);d=xideDiagnosticDependenciesCreate(NULL);CHECK(d && unchanged(d,oversized,XIDE_DEPENDENCY_ERROR));xideDiagnosticDependenciesDestroy(d);xrtValueRelease(oversized);xrtFree(huge);
 xrtValueRelease(valid);xrtValueRelease(empty);xrtValueRelease(remove);return 1;
}
typedef struct source_binding {char* uri;xls_text* text;int version;} source_binding;
static int sentSources(source_binding* sources,const xvalue* message)
{
 const char* method=text(get(message,"method"));const xvalue* params=get(message,"params"),*document=get(params,"textDocument");
 if(!method || (strcmp(method,"textDocument/didOpen") && strcmp(method,"textDocument/didChange") && strcmp(method,"textDocument/didClose"))) return 1;
 const char* uri=text(get(document,"uri"));CHECK(uri);size_t slot=0;
 while(slot<64 && (!sources[slot].uri || strcmp(sources[slot].uri,uri))) ++slot;
 if(!strcmp(method,"textDocument/didClose")) {if(slot<64) {xrtFree(sources[slot].uri);xlsTextRelease(sources[slot].text);sources[slot]=(source_binding){0};}return 1;}
 int64 version=0;CHECK(xrtValueGetInt(get(document,"version"),&version) && version>=INT32_MIN && version<=INT32_MAX);
 if(!strcmp(method,"textDocument/didOpen")) {
  CHECK(slot==64);for(slot=0;slot<64 && sources[slot].uri;++slot) {}CHECK(slot<64);
  xstrview value={0};CHECK(xrtValueGetString(get(document,"text"),&value));
  sources[slot].uri=xrtStrDup(uri);sources[slot].text=xlsTextCreate(value.Data ? value.Data : "",value.Size,8u*1024u*1024u,NULL);
  CHECK(sources[slot].uri && sources[slot].text);
 } else {
  CHECK(slot<64);const xvalue* changes=get(params,"contentChanges");CHECK(changes && xrtValueType(changes)==XVALUE_ARRAY);
  for(size_t i=0;i<xrtValueCount(changes);++i) {
   const xvalue* change=xrtValueArrayGet(changes,i);xstrview value={0};CHECK(!get(change,"range") && xrtValueGetString(get(change,"text"),&value));
   xls_text* next=xlsTextCreate(value.Data ? value.Data : "",value.Size,8u*1024u*1024u,NULL);CHECK(next);xlsTextRelease(sources[slot].text);sources[slot].text=next;
  }
 }
 sources[slot].version=(int)version;return 1;
}
static int transcript(const char* path)
{
 size_t bytes=0;unsigned char* raw=xrtFileReadAllLimit(path,64u*1024u*1024u,&bytes);CHECK(raw);
 xjsonreadconfig config;xrtJsonReadConfigInit(&config);config.MaxInputBytes=64u*1024u*1024u;config.MaxStringBytes=8u*1024u*1024u;config.MaxValues=2097152;config.MaxContainerItems=262144;config.MaxDepth=32;
 xvalue* events=xrtJsonRead((xstrview){(const char*)raw,bytes},&config);xrtFree(raw);CHECK(events && xrtValueType(events)==XVALUE_ARRAY);
 xide_diagnostic_dependencies* d=xideDiagnosticDependenciesCreate(NULL);CHECK(d);size_t parts=0,references=0,clears=0,mapped=0;source_binding sources[64]={0};
 for(size_t i=0;i<xrtValueCount(events);++i) {
  const xvalue* sent=get(xrtValueArrayGet(events,i),"send");if(sent) CHECK(sentSources(sources,sent));
  const xvalue* message=get(xrtValueArrayGet(events,i),"receive");const char* method=text(get(message,"method"));const xvalue* p=get(message,"params");if(!method) continue;
  if(!strcmp(method,"xlang/diagnosticDependencies")) {int result=xideDiagnosticDependenciesMessage(d,p,NULL,NULL);if(!result) fprintf(stderr,"event%zu: %s\n",i,xideDiagnosticDependenciesError(d));CHECK(result>=1 && result<=3);++parts;}
  else if(!strcmp(method,"textDocument/publishDiagnostics")) {
   const xvalue* proof=get(p,"xlang");const char* id=text(get(proof,"dependencies"));bool cleared=false;
   if(!xrtValueGetBool(get(proof,"cleared"),&cleared) || (!cleared && !id)) continue;
   char error[256];xide_diagnostic_notice* notice=xideDiagnosticNoticeDecode(d,p,NULL,NULL,error,sizeof(error));
   if(!notice) fprintf(stderr,"event%zu: %s\n",i,error);
   CHECK(notice && notice->cleared==cleared);
   if(!notice->cleared && notice->overlay) for(size_t j=0;j<64;++j) if(sources[j].uri && !strcmp(sources[j].uri,notice->uri) && sources[j].version==notice->version) {CHECK(xideDiagnosticNoticeMap(notice,sources[j].text,NULL,NULL));++mapped;break;}
   xideDiagnosticNoticeRelease(notice);if(cleared) ++clears;else ++references;
  }
 }
 xide_dependency_stats stats;xideDiagnosticDependenciesStats(d,&stats);CHECK(!stats.pending);xideDiagnosticDependenciesDestroy(d);xrtValueRelease(events);
 for(size_t i=0;i<64;++i) {xrtFree(sources[i].uri);xlsTextRelease(sources[i].text);}
 printf("real transcript accepted: parts=%zu references=%zu withdrawals=%zu mapped-current-open-bodies=%zu\n",parts,references,clears,mapped);return 1;
}
int main(int argc,char** argv)
{
 if(argc==3 && !strcmp(argv[1],"--transcript")) return transcript(argv[2]) ? 0 : 1;
 xide_dependency_stats stats={1,1,1,1,1};xideDiagnosticDependenciesStats(NULL,&stats);
 if(stats.dictionaries || stats.pending || stats.bindings || stats.encodedBytes || stats.bindingKeyBytes ||
   xideDiagnosticDependenciesFind(NULL,pairId) || xideDiagnosticDependencyRetain(NULL) || xideDiagnosticDependencyCount(NULL) || xideDiagnosticDependencyId(NULL) || xideDiagnosticDependencyPath(NULL,0) || xideDiagnosticDependencyContainsKey(NULL,NULL) || xideDiagnosticDependenciesReference(NULL,first,pairId) || xideDiagnosticDependenciesMessage(NULL,NULL,NULL,NULL)) return 1;
 xideDiagnosticDependenciesDestroy(NULL);xideDiagnosticDependencyRelease(NULL);
 if(!lifecycle() || !cancellation() || !malformed() || !limits()) return 1;
 printf("diagnostic_dependencies_test passed: %u checks plus4000 concurrent immutable reads; independent digest vectors, strict multipart/ordering/schema, atomic cancellation/rejection, native aliases, reference/release lifecycle, detached leases and logical byte/count limits.\n",checks);return 0;
}
