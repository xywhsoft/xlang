#define main semanticReadsBaseMain
#include "analysis_semantic_memo_test.c"
#undef main
static char* readsProgram(unsigned version)
{
 xstrbuf s={0};const char* argument=version>=2 ? "input" : "value";
 int okay=xrtStrBufAppendFormat(&s,"\xef\xbb\xbf// 中文😀\r\nfunc identity(%s %s) as int\r\n return %s\r\nend\r\n",version==7 ? "string" : "int",argument,version==7 ? "0" : argument);
 if(version>=3)okay=okay && xrtStrBufAppendFormat(&s,"func identity(string value,int pad=%u) as int\r\n return pad\r\nend\r\n",version>=4 ? 2u : 1u);
 for(unsigned i=0;okay && i<16;++i)okay=xrtStrBufAppendFormat(&s,"func work_%u(int value) as int\r\n int local=identity(value)\r\n%s if local > 0 then\r\n return local\r\n end\r\n return value\r\nend\r\n",i,version>=6 && i==5 ? " int extra=value+1\r\n" : "");
 const char* unused=version>=9 ? "longer_input" : version ? "input" : "value";
 okay=okay && xrtStrBufAppendFormat(&s,"func %s(int %s) as int\r\n return %s\r\nend\r\nfunc main(...) as int\r\n return work_0(7)\r\nend\r\n",version>=5 ? "spare" : "unused",unused,unused);
 char* out=okay ? xrtStrDup(s.Data) : NULL;xrtStrBufFree(&s);return out;
}
static int readsRun(void)
{
 char* path=xrtPathAbs("semantic-reads.xl");CHECK(path);xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,1,0};
 for(unsigned v=0;v<10;++v){char* text=readsProgram(v);CHECK(text);xls_semantic_check* r=update(session,&doc,1,text);CHECK(r && xlsSemanticCheckInfo(r).accepted==(v!=7) && fresh(r,&doc,1));xls_check_semantic_work w=xlsSemanticCheckWork(r);xls_sem_read_work reads=xlsSemanticCheckReads(r);
  printf("v%u checked=%llu reused=%llu contexts=%llu hits=%llu observed=%llu negative=%llu matched=%llu rejected=%llu readReuse=%llu bytes=%llu\n",v,(unsigned long long)w.functionsChecked,(unsigned long long)w.functionsReused,(unsigned long long)reads.contexts,(unsigned long long)reads.contextHits,(unsigned long long)reads.recorded,(unsigned long long)reads.negative,(unsigned long long)reads.matched,(unsigned long long)reads.rejected,(unsigned long long)reads.reused,(unsigned long long)reads.bytes);
  if(v==1 || v==5 || v==9)CHECK(w.functionsReused>=16 && reads.reused>=16 && reads.contextHits && reads.matched && !w.contextHits);
  if(v==2 || v==4)CHECK(w.functionsChecked>=18 && reads.rejected>=16 && reads.contextHits && !w.contextHits);
  if(v==3)CHECK(!reads.contextHits && !reads.reused && !w.functionsReused);
  if(v==6)CHECK(w.contextHits && !reads.reused && w.functionsReused>=14);
  xlsSemanticCheckDestroy(r);xrtFree(text);
 }
 xlsCheckSessionDestroy(session);xlsTextRelease(doc.text);doc.text=NULL;
 for(size_t budget=0;budget<2;++budget){session=xlsCheckSessionCreate(NULL);CHECK(session);doc.revision=0;xls_sem_read_control outer,inner;xlsFrontendSemReadsBegin(&outer,budget);xlsFrontendSemReadsBegin(&inner,8u*1024u*1024u);
  for(unsigned v=0;v<2;++v){char* text=readsProgram(v);CHECK(text);xls_semantic_check* r=update(session,&doc,1,text);CHECK(r && xlsSemanticCheckInfo(r).accepted && fresh(r,&doc,1));if(v)CHECK(!xlsSemanticCheckWork(r).functionsReused && !xlsSemanticCheckReads(r).reused && !xlsSemanticCheckReads(r).contextHits);xlsSemanticCheckDestroy(r);xrtFree(text);}
  xlsFrontendSemReadsEnd(&inner);xlsFrontendSemReadsEnd(&outer);xlsCheckSessionDestroy(session);xlsTextRelease(doc.text);doc.text=NULL;
 }
 xrtFree(path);return 1;
}
static int readsNegative(void)
{
 char* path=xrtPathAbs("semantic-negative-read.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);xls_analysis_document doc={"entry",path,NULL,0,0};
 for(unsigned v=0;v<4;++v){xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,
  "func %s(string value) as int\n return 0\nend\nfunc use() as int\n return copy(7)\nend\nfunc main(...) as int\n return use()\nend\n",v==2 ? "copy" : v ? "spare" : "unused"));
  xls_semantic_check* r=update(session,&doc,1,text.Data);CHECK(r && fresh(r,&doc,1));
  CHECK(xlsSemanticCheckInfo(r).accepted==(v!=2));
  if(v==0)CHECK(xlsSemanticCheckReads(r).negative>0);
  if(v==1)CHECK(xlsSemanticCheckReads(r).reused>=1 && xlsSemanticCheckReads(r).contextHits);
  xlsSemanticCheckDestroy(r);xrtStrBufFree(&text);
 }
 xlsTextRelease(doc.text);xlsCheckSessionDestroy(session);xrtFree(path);return 1;
}
static int readsTypes(void)
{
 char* path=xrtPathAbs("semantic-live-type-read.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);xls_analysis_document doc={"entry",path,NULL,0,0};
 for(unsigned v=0;v<4;++v){xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,
  "func source() as %s\n return %s\nend\nclass Box\n var item=source()\nend\nfunc use(Box value) as var\n if true then\n return value.item\n end\n return value.item\nend\nfunc unused(int %s) as int\n return %s\nend\nfunc main(...) as int\n return 0\nend\n",v>=2 ? "bool" : "int",v>=2 ? "true" : "7",v%2 ? "input" : "value",v%2 ? "input" : "value"));
  xls_semantic_check* r=update(session,&doc,1,text.Data);CHECK(r && xlsSemanticCheckInfo(r).accepted && fresh(r,&doc,1));xls_sem_read_work reads=xlsSemanticCheckReads(r);
  printf("type v%u checked=%llu reused=%llu hit=%llu rejected=%llu\n",v,(unsigned long long)xlsSemanticCheckWork(r).functionsChecked,(unsigned long long)reads.reused,(unsigned long long)reads.contextHits,(unsigned long long)reads.rejected);
  if(v==1 || v==3)CHECK(reads.contextHits && reads.reused>=1);
  if(v==2)CHECK(reads.contextHits && reads.rejected>=1);
  xlsSemanticCheckDestroy(r);xrtStrBufFree(&text);
 }
 xlsTextRelease(doc.text);xlsCheckSessionDestroy(session);xrtFree(path);return 1;
}
typedef struct reads_reader {const char* path;xls_semantic_check* previous;unsigned passed;} reads_reader;
static int32 readsThread(void* user)
{
 reads_reader* reader=user;char* source=readsProgram(1);CHECK(source);xls_analysis_document doc={"entry",reader->path,NULL,700,1};doc.text=xlsTextCreate(source,strlen(source),8u*1024u*1024u,NULL);CHECK(doc.text);
 xls_entry_cache* cache=xlsEntryCacheCreate(32u*1024u*1024u);CHECK(cache);xls_semantic_input input={reader->path,source,strlen(source)};
 xls_semantic_check* r=xlsSemanticCheckCreateCachedInContext(NULL,cache,reader->path,&input,1,1,NULL,reader->previous,NULL,NULL,NULL,0);
 CHECK(r && fresh(r,&doc,1) && xlsSemanticCheckReads(r).reused>=16);xlsSemanticCheckDestroy(r);xlsEntryCacheDestroy(cache);xlsTextRelease(doc.text);xrtFree(source);reader->passed=1;return 0;
}
static int readsOwners(void)
{
 char* path=xrtPathAbs("semantic-read-owners.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);xls_analysis_document doc={"entry",path,NULL,0,0};char* source=readsProgram(0);CHECK(source);xls_semantic_check* held=update(session,&doc,1,source);CHECK(held && fresh(held,&doc,1));xrtFree(source);
 xlsCheckSessionDestroy(session);reads_reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(reads_reader){path,held,0};threads[i]=xrtThreadCreate(readsThread,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}CHECK(fresh(held,&doc,1));xlsSemanticCheckDestroy(held);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
int main(void){xrtAtomic32Init(&checks,0);if(!readsRun() || !readsNegative() || !readsTypes() || !readsOwners())return 1;printf("analysis_semantic_reads_test passed: %u checks;actual semantic reuse across unobserved headers,negative shadowing/live type mutations,full fresh AST/index/report/IR parity and four readers after session close\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
