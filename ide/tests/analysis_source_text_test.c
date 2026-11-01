#include "analysis/check_session.h"
#include "analysis/input_batch.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_source_text_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static unsigned fault,faultVisits;static const char* faultPath;static xls_text* faultText;
const xls_analysis_document* __real_xlsInputBatchAt(const xls_input_batch*,size_t);
const xls_analysis_document* __wrap_xlsInputBatchAt(const xls_input_batch* batch,size_t ordinal)
{
 const xls_analysis_document* original=__real_xlsInputBatchAt(batch,ordinal);
 if(!original || !fault || strcmp(original->path,faultPath))return original;
 static xls_analysis_document forged;forged=*original;++faultVisits;
 if(fault==1)forged.path="unrelated-source-text-path";else forged.text=faultText;
 return &forged;
}
typedef struct reader {xls_semantic_check* check;xls_text* expected;size_t ordinal,bom;int okay;} reader;
static int32 readLease(void* user)
{
 reader* r=user;r->okay=1;
 for(unsigned i=0;i<64;++i){size_t bom=99;xls_text* text=xlsSemanticCheckRetainSourceText(r->check,r->ordinal,&bom);
  if(text!=r->expected || bom!=r->bom)r->okay=0;
  xlsTextRelease(text);
 }return 0;
}
static int run(void)
{
 char* directory=xrtPathAbs("build/source-text-lease");CHECK(directory && xrtDirCreateAll(directory));
 char *entry=xrtPathJoin(directory,"entry.xl"),*dependency=xrtPathJoin(directory,"lease_dep.xl"),*unused=xrtPathJoin(directory,"unused.xl");CHECK(entry && dependency && unused);
 const char* mainText="\xef\xbb\xbf" "import lease_dep\r\nfunc main(...) as int\r\n string 文本=\"😀\"\r\n return lease_dep.value()\r\nend\r\n";
 const char* depText="export func value() as int\r\n return 7\r\nend\r\n";
 xls_text *a=xlsTextCreate(mainText,strlen(mainText),8u*1024u*1024u,NULL),*b=xlsTextCreate(depText,strlen(depText),8u*1024u*1024u,NULL),*c=xlsTextCreate("",0,1024,NULL);CHECK(a && b && c);
 /* Input ordinal and report ordinal deliberately differ; unused inputs must
  * not be selected merely because their storage is retained by the batch. */
 xls_analysis_document docs[3]={{"unused",unused,c,3,1},{"dep",dependency,b,2,1},{"entry",entry,a,1,1}};
 xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);
 char error[256];xls_semantic_check* r=xlsCheckSessionCheck(session,docs,3,"entry",1,NULL,NULL,error,sizeof(error));
 if(!r)fprintf(stderr,"check:%s\n",error);
 CHECK(r && xlsSemanticCheckInfo(r).accepted);xls_check_inputs inputs;CHECK(xlsSemanticCheckInputs(r,&inputs) && inputs.sources==2);
 xls_text* leases[2]={0};size_t boms[2],entryOrdinal=SIZE_MAX;
 for(size_t i=0;i<2;++i){xls_check_source row;CHECK(xlsSemanticCheckSource(r,i,&row));int isEntry=!strcmp(row.path,entry);CHECK(isEntry || !strcmp(row.path,dependency));
  leases[i]=xlsSemanticCheckRetainSourceText(r,i,&boms[i]);CHECK(leases[i]==(isEntry ? a : b) && boms[i]==(isEntry ? 3u : 0u));
  CHECK(xlsTextSize(leases[i])-boms[i]==row.size && !memcmp(xlsTextData(leases[i])+boms[i],row.text,row.size));if(isEntry)entryOrdinal=i;
 }CHECK(entryOrdinal!=SIZE_MAX);
 /* Exercise real path/size/byte guards with an altered captured-row view.
  * Same-length text differs inside the normalized body,not just its BOM. */
 char* changed=xrtStrDup(mainText);CHECK(changed);changed[3]='x';xls_text* different=xlsTextCreate(changed,strlen(changed),8u*1024u*1024u,NULL);xrtFree(changed);CHECK(different);
 faultPath=entry;
 for(unsigned kind=1;kind<=3;++kind){fault=kind;faultVisits=0;faultText=kind==2 ? different : c;size_t bom=99;
  xls_text* rejected=xlsSemanticCheckRetainSourceText(r,entryOrdinal,&bom);fault=0;CHECK(!rejected && !bom && faultVisits==1);
 }xlsTextRelease(different);
 size_t bad=99;CHECK(!xlsSemanticCheckRetainSourceText(NULL,0,&bad) && !bad);bad=99;
 CHECK(!xlsSemanticCheckRetainSourceText(r,SIZE_MAX,&bad) && !bad && !xlsSemanticCheckRetainSourceText(r,0,NULL));
 xmemstats before,after;xrtMemStatsGet(&before);
 for(unsigned j=0;j<256;++j){size_t bom;xls_text* text=xlsSemanticCheckRetainSourceText(r,j%2,&bom);CHECK(text==leases[j%2] && bom==boms[j%2]);xlsTextRelease(text);}
 xrtMemStatsGet(&after);CHECK(before.BlockAllocCalls==after.BlockAllocCalls && before.BlockFreeCalls==after.BlockFreeCalls);
 reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(reader){r,leases[i%2],i%2,boms[i%2],0};threads[i]=xrtThreadCreate(readLease,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 /* Raw-input checks and disk rows have no retained editor text. Optional
  * refusal must leave the original source/copy path available. */
 xls_semantic_input raw[2]={{entry,mainText,strlen(mainText)},{dependency,depText,strlen(depText)}};
 xls_semantic_check* fresh=xlsSemanticCheckCreate(entry,raw,2,1,NULL,NULL,error,sizeof(error));CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
 bad=99;CHECK(!xlsSemanticCheckRetainSourceText(fresh,0,&bad) && !bad);xlsSemanticCheckDestroy(fresh);
 xlsCheckSessionDestroy(session);xlsTextRelease(a);xlsTextRelease(b);xlsTextRelease(c);xlsSemanticCheckDestroy(r);
 xls_position p;CHECK(xlsTextPosition(leases[entryOrdinal],3,&p) && !p.line && p.character==1);
 for(size_t i=0;i<2;++i){CHECK(strstr(xlsTextData(leases[i]),"return "));xlsTextRelease(leases[i]);}
 CHECK(xrtFileWriteAtomic(dependency,(xbytesview){(const unsigned char*)depText,strlen(depText)}));
 a=xlsTextCreate(mainText,strlen(mainText),8u*1024u*1024u,NULL);CHECK(a);docs[0]=(xls_analysis_document){"entry",entry,a,1,1};session=xlsCheckSessionCreate(NULL);CHECK(session);
 r=xlsCheckSessionCheck(session,docs,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(r && xlsSemanticCheckInfo(r).accepted);
 xls_text* held=NULL;
 for(size_t i=0;i<2;++i){xls_check_source row;CHECK(xlsSemanticCheckSource(r,i,&row));bad=99;xls_text* text=xlsSemanticCheckRetainSourceText(r,i,&bad);
  if(!strcmp(row.path,entry)){CHECK(text==a && bad==3);entryOrdinal=i;held=text;}else CHECK(!text && !bad);
 }CHECK(held);
 CHECK(xrtFileWriteAtomic(dependency,(xbytesview){(const unsigned char*)depText,strlen(depText)}) && !xlsSemanticCheckValidate(r));
 bad=99;CHECK(!xlsSemanticCheckRetainSourceText(r,entryOrdinal,&bad) && !bad);
 xlsCheckSessionDestroy(session);xlsSemanticCheckDestroy(r);xlsTextRelease(a);CHECK(xlsTextPosition(held,3,&p) && p.character==1);xlsTextRelease(held);
 xrtFree(entry);xrtFree(dependency);xrtFree(unused);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("analysis_source_text_test passed: %u checks\n",checks);return 0;}
