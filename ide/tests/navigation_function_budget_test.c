#include "analysis/text.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t positions;
static int countedPosition(const xls_text* text,size_t offset,xls_position* out){++positions;return xlsTextPosition(text,offset,out);}
#define xlsTextPosition countedPosition
#include "services/semantic_definition.c"
#undef xlsTextPosition
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"navigation_function_budget_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int guard(int mismatch)
{
 const size_t size=256u*1024u;char* bytes=malloc(size+1);CHECK(bytes);
 for(size_t i=0;i<size;i+=4){memcpy(bytes+i,"\xf0\x9f\x98\x80",4);}
 bytes[size]=0;
 xls_text* current=xlsTextCreate(bytes,size,size,NULL);bytes[3]='\x81';xls_text* previous=xlsTextCreate(bytes,size,size,NULL);free(bytes);CHECK(current && previous);
 unsigned short shapes[3]={0};int identity=0;xls_text* texts[]={current};xls_text* oldTexts[]={previous};
 native_function_points old={0};old.info=(xls_index_segment){.first=1,.count=1,.identity=&identity,.source=1,.proofFirst=0,.proofEnd=size,.reusable=1};old.ready=1;
 CHECK(xlsTextPosition(current,size,&old.proofEndPosition));if(mismatch)++old.proofEndPosition.character;
 native_points prior={.texts=oldTexts,.shapes=shapes,.sourcesCount=1,.nodesCount=2,.functions=&old,.functionCount=1};
 native_points now={.texts=texts,.shapes=shapes,.sourcesCount=1,.nodesCount=2};xide_semantic_definitions definitions={.native=&now};native_reuse reuse={.previous=&prior};
 CHECK(xrtArrayInit(&reuse.keys,sizeof(native_function_key)));native_function_key key={(uintptr_t)&identity,0};CHECK(xrtArrayPush(&reuse.keys,&key));positions=0;
 for(unsigned i=0;i<100;++i){native_function_points part={.info=old.info};size_t charge=0;CHECK(!nativeFunctionReuseFacts(&definitions,&part,&reuse,&charge) && !charge);}
 size_t admitted=NATIVE_POINT_PROOF_BYTES/size;CHECK(reuse.proofBytes==admitted*size && positions==2*admitted);
 CHECK(!now.reuse.functions && !now.reuse.facts && !now.reuse.cursors && now.reuse.proofBytes==(mismatch?0:admitted*size));
 xrtArrayUnit(&reuse.keys);xlsTextRelease(current);xlsTextRelease(previous);return 1;
}
#include "analysis/check_session.h"
static int noOverlap(void)
{
 char* path=xrtPathAbs("point-cache-no-overlap.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);xls_projection_store* store=xlsProjectionStoreCreate(XLS_PROJECTION_STORE_BYTES);CHECK(path && session && store);
 for(unsigned revision=1;revision<=2;++revision){xstrbuf source={0};CHECK(xrtStrBufAppend(&source,xrtStrView(revision==1 ? "func main(...) as int\n return 0\nend\n" : "func main(...) as int\n int changed=0; return changed\nend\n")) && xrtStrBufAppend(&source,xrtStrView("func helper(int value) as int\n int local=value+1\n")));
  for(unsigned i=0;i<20;++i)CHECK(xrtStrBufAppend(&source,xrtStrView(" local=local+1\n")));
  CHECK(xrtStrBufAppend(&source,xrtStrView(" return local\nend\n")));xls_text* text=xlsTextCreate(source.Data,source.Size,8u*1024u*1024u,NULL);CHECK(text);xls_analysis_document document={"entry",path,text,99000,revision};xls_semantic_check* check=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,NULL,0);xlsTextRelease(text);xrtStrBufFree(&source);CHECK(check && xlsSemanticCheckInfo(check).accepted);
  xls_input_source row;CHECK(xlsSemanticCheckSource(check,0,&row));xide_semantic_source input={row.path,row.text,row.kind,0};xide_model_error error={0};xide_navigation_snapshot* snapshot=xideSemanticNavigationCapture(check,&input,1,&error);CHECK(snapshot && xideSemanticNavigationBindAndPreparePoints(snapshot,store,&error)==1 && !error.code && xideSemanticNavigationPointsReady(snapshot));native_points* data=snapshot->points->native;
  if(revision==1)CHECK(data->functions && data->cachedCursors && data->cachedCursorCount);
  else CHECK(!data->functions && !data->cachedCursors && !data->cachedDeclarations && !data->cachedTypes && !data->reuse.functions);
  xideSemanticNavigationDestroy(snapshot);xlsSemanticCheckDestroy(check);
 }
 xlsProjectionStoreDestroy(store);xlsCheckSessionDestroy(session);xrtFree(path);return 1;
}
typedef struct prior_value {unsigned value;xls_projection_store* store;} prior_value;
static unsigned priorDestructions;
static void priorDestroy(void* pointer){prior_value* p=pointer;xls_projection_stats observed;xlsProjectionStoreStats(p->store,&observed);++priorDestructions;xrtFree(p);}
static int priorLease(void)
{
 const char* source="func main(...) as int\n return 0\nend\n";CHECK(xrtDirCreateAll("build"));char* path=xrtPathAbs("build/previous-projection.xl");CHECK(path && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)source,strlen(source)}));
 xls_semantic_check* old=xlsSemanticCheckCreate(path,NULL,0,1,NULL,NULL,NULL,0);xls_semantic_input input={path,source,strlen(source)};xls_semantic_check* current=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);CHECK(old && current && xlsSemanticCheckInfo(old).hasIndex && xlsSemanticCheckInfo(current).hasIndex);
 xls_projection_store* store=xlsProjectionStoreCreate(4096);CHECK(store);prior_value* value=xrtMalloc(sizeof(*value));CHECK(value);*value=(prior_value){42,store};const char key[]="point\0domain",wrong[]="other\0domain";xls_projection* p=xlsProjectionCreate(old,key,sizeof(key),value,sizeof(*value),priorDestroy);CHECK(p);p=xlsProjectionPublish(store,p);xlsProjectionRelease(p);
 CHECK(!xlsProjectionAcquirePrevious(store,old,key,sizeof(key)) && !xlsProjectionAcquirePrevious(store,current,wrong,sizeof(wrong)) && !xlsProjectionAcquirePrevious(store,current,key,0));
 CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)source,strlen(source)}) && !xlsSemanticCheckValidate(old) && xlsSemanticCheckInfo(current).hasIndex);
 xls_projection_stats before,after;xlsProjectionStoreStats(store,&before);p=xlsProjectionAcquirePrevious(store,current,key,sizeof(key));xlsProjectionStoreStats(store,&after);CHECK(p && !memcmp(&before,&after,sizeof(before)) && ((prior_value*)xlsProjectionValue(p))->value==42);
 CHECK(xlsProjectionRetireOther(store,current));xlsSemanticCheckDestroy(old);CHECK(!priorDestructions && ((prior_value*)xlsProjectionValue(p))->value==42);xlsProjectionRelease(p);CHECK(priorDestructions==1);xlsProjectionStoreDestroy(store);xlsSemanticCheckDestroy(current);xrtFree(path);return 1;
}
int main(void){if(!guard(0) || !guard(1) || !noOverlap() || !priorLease())return 1;printf("navigation_function_budget_test passed: %u checks; failed Unicode/byte proofs charged before scans,8MiB total work bound\n",checks);return 0;}
