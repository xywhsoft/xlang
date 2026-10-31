/* Real point-construction API failures. Source checking and fallback are
 * outside injection. Track exact CRT addresses plus XRT live allocations.
 * XRT's internal pooled backing storage is excluded from the CRT address
 * tracker;its logical allocations are covered by the XRT counter instead. */
#include "analysis/text.h"
static void* pointCrt[256];static size_t pointCrtCount;static int pointTrackAlloc;
static void* pointTrack(void* p)
{if(p && pointTrackAlloc && !pointCrtIndirect){if(pointCrtCount==256)abort();pointCrt[pointCrtCount++]=p;}return p;}
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void __real_free(void*);
void* __wrap_malloc(size_t n){return !pointCrtIndirect && captureRefuse(4) ? NULL : pointTrack(__real_malloc(n));}
void* __wrap_calloc(size_t n,size_t size){return !pointCrtIndirect && captureRefuse(5) ? NULL : pointTrack(__real_calloc(n,size));}
void __wrap_free(void* p)
{for(size_t i=0;i<pointCrtCount;++i)if(pointCrt[i]==p){pointCrt[i]=pointCrt[--pointCrtCount];break;}__real_free(p);}
void* __real_xrtMalloc(size_t);void* __real_xrtRealloc(void*,size_t);
void* __wrap_xrtMalloc(size_t n){if(captureRefuse(3))return NULL;++pointCrtIndirect;void* p=__real_xrtMalloc(n);--pointCrtIndirect;return p;}
void* __wrap_xrtRealloc(void* p,size_t n){if(captureRefuse(6))return NULL;++pointCrtIndirect;void* result=__real_xrtRealloc(p,n);--pointCrtIndirect;return result;}
bool __real_xrtArrayInit(xarray*,size_t);bool __real_xrtArrayReserve(xarray*,size_t);bool __real_xrtArrayResize(xarray*,size_t);
bool __wrap_xrtArrayInit(xarray* p,size_t n){if(captureRefuse(7))return false;++pointCrtIndirect;bool result=__real_xrtArrayInit(p,n);--pointCrtIndirect;return result;}
bool __wrap_xrtArrayReserve(xarray* p,size_t n){if(captureRefuse(8))return false;++pointCrtIndirect;bool result=__real_xrtArrayReserve(p,n);--pointCrtIndirect;return result;}
bool __wrap_xrtArrayResize(xarray* p,size_t n){if(captureRefuse(9))return false;++pointCrtIndirect;bool result=__real_xrtArrayResize(p,n);--pointCrtIndirect;return result;}
bool __real_xrtStrBufAppend(xstrbuf*,xstrview);
bool __wrap_xrtStrBufAppend(xstrbuf* p,xstrview v){if(captureRefuse(10))return false;++pointCrtIndirect;bool result=__real_xrtStrBufAppend(p,v);--pointCrtIndirect;return result;}
xls_text* __real_xlsTextCreate(const char*,size_t,size_t,enum xls_text_error*);
xls_text* __wrap_xlsTextCreate(const char* s,size_t n,size_t limit,enum xls_text_error* why)
{if(captureRefuse(11)){if(why)*why=XLS_TEXT_MEMORY;return NULL;}++pointCrtIndirect;xls_text* result=__real_xlsTextCreate(s,n,limit,why);--pointCrtIndirect;return result;}
static unsigned pointFactFault;static size_t pointFactAt;
int __real_xlsSemanticIndexPointFact(const xls_semantic_index*,size_t,xls_index_point_fact*);
int __wrap_xlsSemanticIndexPointFact(const xls_semantic_index* index,size_t at,xls_index_point_fact* out)
{
 int okay=__real_xlsSemanticIndexPointFact(index,at,out);
 if(okay && pointFactFault && at==pointFactAt){
  xls_index_fact* f=&out->fact;
  switch(pointFactFault){
  case 1:++f->id;break;case 2:f->parent=(unsigned)at+1;break;
  case 3:f->source=513;break;case 4:out->kind=XLS_INDEX_POINT_KIND_COUNT;break;
  case 5:f->typeOrigin=1;f->canonical=NULL;break;
  case 6:f->declaredType=f->typeId==UINT32_MAX ? 1 : UINT32_MAX;break;
  case 7:++f->range.startColumn;break;
  case 8:f->tokenRange.known=1;f->tokenRange.endOffset=SIZE_MAX;break;
  }
 }
 return okay;
}
static int pointOracle(xide_navigation_snapshot* snapshot,xide_semantic_definitions* reference,const xide_semantic_source* sources,size_t count)
{
 for(size_t source=0;source<count;++source){
  const char* text=sources[source].text;size_t size=strlen(text);int line=0,character=sources[source].utf16Prefix;
  for(size_t offset=0;;){
   CHECK(equalValue(xideSemanticNavigationPointDefinition(snapshot,sources[source].path,line,character),xideSemanticDefinition(reference,sources[source].path,line,character)));
   CHECK(equalValue(xideSemanticNavigationPointHover(snapshot,sources[source].path,line,character),xideSemanticHover(reference,sources[source].path,line,character)));
   if(offset==size)break;
   unsigned char c=(unsigned char)text[offset];size_t length=c<128 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
   if(c=='\r' && offset+1<size && text[offset+1]=='\n')length=2;
   if(c=='\r' || c=='\n'){++line;character=0;}else character+=length==4 ? 2 : 1;
   offset+=length;
  }
 }
 return 1;
}
typedef struct point_reader {const xide_navigation_snapshot* snapshot;const char* path;const xvalue *definition,*hover;int okay;} point_reader;
static int32 readPoints(void* user)
{
 point_reader* r=user;r->okay=1;
 for(unsigned i=0;i<32;++i){
  xvalue* definition=xideSemanticNavigationPointDefinition(r->snapshot,r->path,1,8);
  xvalue* hover=xideSemanticNavigationPointHover(r->snapshot,r->path,1,8);
  if(!definition || !hover || !xrtValueEqual(definition,r->definition) || !xrtValueEqual(hover,r->hover))r->okay=0;
  xrtValueRelease(definition);xrtValueRelease(hover);
 }
 return 0;
}
static int pointThreads(xide_navigation_snapshot* snapshot,xide_semantic_definitions* reference,const char* path)
{
 xvalue *definition=xideSemanticDefinition(reference,path,1,8),*hover=xideSemanticHover(reference,path,1,8);
 CHECK(definition && hover && xrtValueType(definition)!=XVALUE_NULL && xrtValueType(hover)!=XVALUE_NULL);
 point_reader readers[4];xthread* threads[4];
 for(size_t i=0;i<4;++i){readers[i]=(point_reader){snapshot,path,definition,hover,0};threads[i]=xrtThreadCreate(readPoints,readers+i,0);CHECK(threads[i]);}
 for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xrtValueRelease(definition);xrtValueRelease(hover);return 1;
}
static int pointLargeFallback(const char* path)
{
 size_t length=2u*1024u*1024u+32;const char* tail="\nfunc main(...) as int\n return 0\nend\n";
 char* text=malloc(length+strlen(tail)+1);CHECK(text);memset(text,'a',length);text[0]=text[1]='/';strcpy(text+length,tail);
 xls_semantic_input input={path,text,strlen(text)};xls_semantic_check* owner=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);CHECK(owner && xlsSemanticCheckInfo(owner).accepted);
 xls_input_source row;CHECK(xlsSemanticCheckSource(owner,0,&row));xide_semantic_source source={row.path,row.text,row.kind,0};xide_model_error error={0};
 xide_navigation_snapshot* p=xideSemanticNavigationCapture(owner,&source,1,&error);CHECK(p);
 captureFaultKind=11;captureFaultAt=SIZE_MAX;captureVisits=captureRefusals=0;captureFaultActive=1;
 int prepared=xideSemanticNavigationPreparePoints(p,&error);captureFaultActive=0;
 CHECK(!prepared && !error.code && !captureVisits && !xideSemanticNavigationPointsReady(p));
 xls_projection* lease=NULL;xide_navigation_work work;
 xide_semantic_definitions* fallback=xideSemanticNavigationResolve(p,NULL,&lease,&work,&error);CHECK(fallback && !error.code);
 xvalue* hover=xideSemanticHover(fallback,path,1,5);CHECK(hover && xrtValueType(hover)!=XVALUE_NULL);xrtValueRelease(hover);
 xideSemanticDefinitionsReleaseTyped(fallback,lease);xideSemanticNavigationDestroy(p);xlsSemanticCheckDestroy(owner);free(text);return 1;
}
static int pointFaults(xls_semantic_check* checked,const xide_semantic_source* sources,xide_semantic_definitions* reference)
{
 size_t positions=0;
 for(unsigned kind=1;kind<=11;++kind){
  if(kind==2)continue;
  xide_model_error error={0};xide_navigation_snapshot* p=xideSemanticNavigationCapture(checked,sources,2,&error);CHECK(p);
  captureFaultKind=kind;captureFaultAt=SIZE_MAX;captureVisits=captureRefusals=0;captureFaultActive=pointTrackAlloc=1;
  int prepared=xideSemanticNavigationPreparePoints(p,&error);captureFaultActive=pointTrackAlloc=0;size_t total=captureVisits;
  CHECK(prepared==1 && !error.code && !captureRefusals);xideSemanticNavigationDestroy(p);CHECK(!pointCrtCount);positions+=total;
  for(size_t at=0;at<total;++at){
   uint64_t before=liveBlocks();error=(xide_model_error){0};p=xideSemanticNavigationCapture(checked,sources,2,&error);CHECK(p);
   captureFaultAt=at;captureVisits=captureRefusals=0;captureFaultActive=pointTrackAlloc=1;
   prepared=xideSemanticNavigationPreparePoints(p,&error);captureFaultActive=pointTrackAlloc=0;
   CHECK(captureRefusals==1 && prepared>=0 && !error.code);
   if(prepared)CHECK(pointOracle(p,reference,sources,2));
   else{
    xls_projection* lease=NULL;xide_navigation_work work;
    xide_semantic_definitions* fallback=xideSemanticNavigationResolve(p,NULL,&lease,&work,&error);
    CHECK(fallback && !error.code && allQueries(fallback,reference,sources,2));xideSemanticDefinitionsReleaseTyped(fallback,lease);
   }
   xideSemanticNavigationDestroy(p);CHECK(!pointCrtCount && liveBlocks()==before);
  }
  printf("native point fault API %u: %zu actual positions\n",kind,total);
 }
 CHECK(positions>20);
 xide_model_error stale={0};xide_navigation_snapshot* staleSnapshot=xideSemanticNavigationCapture(checked,sources,2,&stale);CHECK(staleSnapshot);
 stale.code=XIDE_MODEL_INVALID;strcpy(stale.message,"previous request");captureFaultKind=11;captureFaultAt=0;captureVisits=captureRefusals=0;captureFaultActive=1;
 int declined=xideSemanticNavigationPreparePoints(staleSnapshot,&stale);captureFaultActive=0;
 CHECK(!declined && !stale.code && !*stale.message && captureRefusals==1);
 stale.code=XIDE_MODEL_INVALID;strcpy(stale.message,"previous request");
 CHECK(xideSemanticNavigationPreparePoints(staleSnapshot,&stale)==1 && !stale.code && !*stale.message);
 stale.code=XIDE_MODEL_INVALID;strcpy(stale.message,"previous request");
 CHECK(xideSemanticNavigationPreparePoints(staleSnapshot,&stale)==1 && !stale.code && !*stale.message);xideSemanticNavigationDestroy(staleSnapshot);
 const xls_semantic_index* index=xlsSemanticCheckTypedIndex(checked);xls_index_point_fact point;
 CHECK(!xlsSemanticIndexPointFact(NULL,0,&point) && !point.fact.id && point.kind==XLS_INDEX_POINT_OTHER);
 CHECK(!xlsSemanticIndexPointFact(index,xlsSemanticIndexCount(index),&point) && !point.fact.id);
 CHECK(!xlsSemanticIndexPointFact(index,0,NULL));
 pointFactAt=SIZE_MAX;
 for(size_t i=0;i<xlsSemanticIndexCount(index);++i){CHECK(xlsSemanticIndexPointFact(index,i,&point));if(point.fact.range.known && point.fact.source==1)pointFactAt=i;}
 CHECK(pointFactAt!=SIZE_MAX);
 for(unsigned fault=1;fault<=8;++fault){
  uint64_t before=liveBlocks();xide_model_error error={0};xide_navigation_snapshot* p=xideSemanticNavigationCapture(checked,sources,2,&error);CHECK(p);
  pointFactFault=fault;CHECK(xideSemanticNavigationPreparePoints(p,&error)==-1 && error.code==XIDE_MODEL_INVALID && !xideSemanticNavigationPointsReady(p));
  CHECK(xideSemanticNavigationPreparePoints(p,NULL)==-1 && !xideSemanticNavigationPointsReady(p));pointFactFault=0;
  xideSemanticNavigationDestroy(p);CHECK(liveBlocks()==before);
 }
 return 1;
}
