#include "analysis_test_clock.h"
#include "analysis/check_session.h"
#include "analysis/completion_session.h"
#include "analysis/signature_session.h"
#include "analysis/signature_internal.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_input_usage.h"
#include "analysis/input_batch.h"
#include "src/compile/xl6_source_overlay.h"
#include "analysis/frontend_overlay_lookup.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_input_usage_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xls_text* text(const char* value){return xlsTextCreate(value,strlen(value),1024*1024,NULL);}
static int sameText(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
 if(!a || !b)return a==b;
 XL6_AstNode left=*a,right=*b;
 left.text=right.text=left.doc=right.doc=NULL;left.literalData=right.literalData=NULL;
 left.children=right.children=NULL;left.inferredType=right.inferredType=left.semanticDeclaredType=right.semanticDeclaredType=NULL;
 left.xlsStorage=right.xlsStorage=NULL;left.source=right.source=NULL;left.childCapacity=right.childCapacity=0;
 if(memcmp(&left,&right,sizeof(left)) || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) || !!a->literalData!=!!b->literalData ||
    (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || !!a->source!=!!b->source)return 0;
 if(a->source && (a->source==b->source || !sameText(a->source->path,b->source->path) || a->source->completionQuery!=b->source->completionQuery ||
    a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd))return 0;
 if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
 for(size_t i=0;i<a->childCount;++i)if(!sameAst(a->children[i],b->children[i]))return 0;
 return 1;
}
static int observation(void)
{
 XL6_SourceOverlay first={0},second={0};xls_input_usage_scope outer,inner;uint64_t used=99;
 CHECK(!xlsFrontendInputUsageEnd(NULL,&used) && !used);
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageRead(&first,1);CHECK(!xlsFrontendInputUsageEnd(&outer,&used) && !used);
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageBind(&first,64);xlsFrontendInputUsageRead(&first,63);
 xlsFrontendInputUsageBegin(&inner);xlsFrontendInputUsageBind(&second,2);xlsFrontendInputUsageRead(&first,0);xlsFrontendInputUsageRead(&second,1);
 CHECK(!xlsFrontendInputUsageEnd(&outer,&used) && !used);CHECK(xlsFrontendInputUsageEnd(&inner,&used) && used==2);
 xlsFrontendInputUsageRead(&first,0);CHECK(xlsFrontendInputUsageEnd(&outer,&used) && used==(UINT64_C(1)<<63)+1);
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageBind(&first,1);CHECK(!xlsFrontendInputUsageFailure(&second));xlsFrontendInputUsageRead(&first,0);CHECK(xlsFrontendInputUsageEnd(&outer,&used) && used==1);
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageBind(&first,1);CHECK(!xlsFrontendInputUsageFailure(NULL));CHECK(!xlsFrontendInputUsageEnd(&outer,&used) && !used);
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageBind(&first,1);xlsFrontendInputUsageRead(&first,1);CHECK(!xlsFrontendInputUsageEnd(&outer,&used) && !used);
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageBind(&first,65);CHECK(!xlsFrontendInputUsageEnd(&outer,&used) && !used);
 char* path=xrtPathAbs("usage-Ä.xl");CHECK(path && xl6SourceOverlaySet(&first,path,"",0));char* query=xrtStrDup(path);CHECK(query);
 const XL6_SourceOverlayItem* item=NULL;
 for(char* p=query;*p;++p)if(*p=='\\')*p='/';
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageBind(&first,1);CHECK(xlsFrontendOverlayLookup(&first,query,&item)==1 && item);CHECK(xlsFrontendInputUsageEnd(&outer,&used) && used==1);
#ifdef _WIN32
 char* unicode=strstr(query,"Ä");CHECK(unicode);memcpy(unicode,"ä",2);
 xlsFrontendInputUsageBegin(&outer);xlsFrontendInputUsageBind(&first,1);CHECK(xlsFrontendOverlayLookup(&first,query,&item)==1 && item);CHECK(xlsFrontendInputUsageEnd(&outer,&used) && used==1);
#endif
 xl6SourceOverlayClear(&first);xrtFree(query);xrtFree(path);return 1;
}
typedef struct trio {xls_check_session* check;xls_completion_session* completion;xls_signature_session* signature;} trio;
typedef struct results {xls_semantic_check* check;xls_semantic_completion* completion;xls_signature_help* signature;} results;
static void release(results* r){xlsSemanticCheckDestroy(r->check);xlsSemanticCompletionDestroy(r->completion);xlsSignatureRelease(r->signature);*r=(results){0};}
static results query(trio* s,const xls_analysis_document* docs,size_t count)
{
 const xls_analysis_document* entry=NULL;for(size_t i=0;i<count;++i)if(!strcmp(docs[i].key,"entry"))entry=&docs[i];
 if(!entry)return (results){0};
 const char* body=xlsTextData(entry->text);size_t bom=!memcmp(body,"\xef\xbb\xbf",3)?3:0;
 const char *completion=strstr(body,"return done"),*signature=strstr(body,"\"hello\"");if(!completion || !signature)return (results){0};
 return (results){xlsCheckSessionCheck(s->check,docs,count,"entry",1,NULL,NULL,NULL,0),
  xlsCompletionSessionQuery(s->completion,docs,count,"entry","entry",(size_t)(completion-body)+strlen("return done")-bom,NULL,NULL,NULL,0),
  xlsSignatureSessionQueryInContext(s->signature,NULL,docs,count,"entry","entry",(size_t)(signature-body)+2-bom,NULL,NULL,NULL,0)};
}
static int parity(const results* r,const xls_analysis_document* docs,size_t count)
{
 xls_semantic_input in[64];const xls_analysis_document* entry=NULL;for(size_t i=0;i<count;++i){in[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};if(!strcmp(docs[i].key,"entry"))entry=&docs[i];}CHECK(entry);
 const char* body=xlsTextData(entry->text);size_t bom=!memcmp(body,"\xef\xbb\xbf",3)?3:0;
 xls_semantic_check* a=xlsSemanticCheckCreate(entry->path,in,count,1,NULL,NULL,NULL,0);
 xls_semantic_completion* b=xlsSemanticCompletionCreate(entry->path,entry->path,(size_t)(strstr(body,"return done")-body)+strlen("return done")-bom,in,count,NULL,NULL,NULL,0);
 xls_signature_help* c=xlsSignatureCreateInContext(NULL,entry->path,entry->path,(size_t)(strstr(body,"\"hello\"")-body)+2-bom,in,count,NULL,NULL,NULL,0);
 CHECK(a && b && c && r->check && r->completion && r->signature);
 char *x=xlsSemanticCheckJson(a,NULL),*y=xlsSemanticCheckJson(r->check,NULL);CHECK(x && y && !strcmp(x,y));xlsSemanticStringFree(x);xlsSemanticStringFree(y);
 CHECK(sameAst(xlsSemanticCheckAst(a),xlsSemanticCheckAst(r->check)));
 x=xlsSemanticCompletionJson(b,NULL);y=xlsSemanticCompletionJson(r->completion,NULL);CHECK(x && y && !strcmp(x,y));xlsSemanticStringFree(x);xlsSemanticStringFree(y);
 CHECK(sameAst(xlsSemanticCompletionAst(b),xlsSemanticCompletionAst(r->completion)));
 xls_signature_info ci=xlsSignatureInfo(c),ri=xlsSignatureInfo(r->signature);CHECK(ci.available==ri.available && ci.count==ri.count && ci.activeSignature==ri.activeSignature);
 for(size_t i=0;i<ci.count;++i){xls_signature_item p,q;CHECK(xlsSignatureItem(c,i,&p) && xlsSignatureItem(r->signature,i,&q));CHECK(sameText(p.label,q.label) && p.symbol==q.symbol && p.activeParameter==q.activeParameter);}
 CHECK(sameAst(xlsSemanticCompletionAst(xlsSignatureSemantic(c)),xlsSemanticCompletionAst(xlsSignatureSemantic(r->signature))));
 xlsSemanticCheckDestroy(a);xlsSemanticCompletionDestroy(b);xlsSignatureRelease(c);return 1;
}
static int counts(trio* s,uint64_t builds,uint64_t hits)
{
 xls_check_stats a;xls_completion_stats b;xls_signature_stats c;xlsCheckSessionStats(s->check,&a);xlsCompletionSessionStats(s->completion,&b);xlsSignatureSessionStats(s->signature,&c);
 CHECK(a.builds==builds && b.builds==builds && c.builds==builds);CHECK(a.inputScopeHits==hits && b.inputScopeHits==hits && c.inputScopeHits==hits);return 1;
}
typedef struct cancel_state {unsigned calls,at;} cancel_state;
static int cancelAt(void* user){cancel_state* s=user;return ++s->calls==s->at;}
static int oneCancelled(trio* s,const xls_analysis_document* docs,unsigned kind,cancel_state* cancel,int expected)
{
 const char* body=xlsTextData(docs[0].text);size_t bom=!memcmp(body,"\xef\xbb\xbf",3)?3:0;char error[160]={0};
 if(kind==0){xls_semantic_check* r=xlsCheckSessionCheck(s->check,docs,3,"entry",1,cancelAt,cancel,error,sizeof(error));int present=r!=NULL;xlsSemanticCheckDestroy(r);CHECK(present==expected);}
 else if(kind==1){size_t at=(size_t)(strstr(body,"return done")-body)+strlen("return done")-bom;xls_semantic_completion* r=xlsCompletionSessionQuery(s->completion,docs,3,"entry","entry",at,cancelAt,cancel,error,sizeof(error));int present=r!=NULL;xlsSemanticCompletionDestroy(r);CHECK(present==expected);}
 else {size_t at=(size_t)(strstr(body,"\"hello\"")-body)+2-bom;xls_signature_help* r=xlsSignatureSessionQueryInContext(s->signature,NULL,docs,3,"entry","entry",at,cancelAt,cancel,error,sizeof(error));int present=r!=NULL;xlsSignatureRelease(r);CHECK(present==expected);}
 if(!expected)CHECK(strstr(error,"cancel"));
 return 1;
}
static int cancellation(trio* s,xls_analysis_document* docs,const results* held)
{
 unsigned calls[3];for(unsigned kind=0;kind<3;++kind){cancel_state c={0};CHECK(oneCancelled(s,docs,kind,&c,1) && c.calls>=2);calls[kind]=c.calls;}
 xlsTextRelease(docs[2].text);docs[2].text=text("// another unobserved edit 😀");++docs[2].revision;CHECK(docs[2].text);
 for(unsigned kind=0;kind<3;++kind)for(unsigned at=1;at<=calls[kind];++at){cancel_state c={0,at};CHECK(oneCancelled(s,docs,kind,&c,0));CHECK(counts(s,1,12));}
 results r=query(s,docs,3);CHECK(r.check==held->check && r.completion==held->completion && r.signature==held->signature && parity(&r,docs,3));CHECK(counts(s,1,13));release(&r);return 1;
}
typedef struct usage_reader {xls_analysis_document docs[3];const char* entry;size_t completion,signature;uint64_t expected;int okay;} usage_reader;
static int32 usageRead(void* user)
{
 usage_reader* r=user;r->okay=1;
 for(unsigned n=0;n<8;++n){xls_semantic_input in[3];for(size_t i=0;i<3;++i)in[i]=(xls_semantic_input){r->docs[i].path,xlsTextData(r->docs[i].text),xlsTextSize(r->docs[i].text)};
  xls_semantic_check* a=xlsSemanticCheckCreate(r->entry,in,3,1,NULL,NULL,NULL,0);xls_semantic_completion* b=xlsSemanticCompletionCreate(r->entry,r->entry,r->completion,in,3,NULL,NULL,NULL,0);
  xls_signature_help* c=xlsSignatureCreateInContext(NULL,r->entry,r->entry,r->signature,in,3,NULL,NULL,NULL,0);uint64_t used=0;
  if(!a || !b || !c || !xlsSemanticCheckInfo(a).accepted || !xlsSemanticCompletionInfo(b).available || !xlsSignatureInfo(c).available ||
     !xlsSemanticCheckInputUsage(a,&used) || used!=r->expected || !xlsSemanticCompletionInputUsage(b,&used) || used!=r->expected ||
     !xlsSemanticCompletionInputUsage(xlsSignatureSemantic(c),&used) || used!=r->expected)r->okay=0;
  xlsSemanticCheckDestroy(a);xlsSemanticCompletionDestroy(b);xlsSignatureRelease(c);
 }return 0;
}
static int concurrentUsage(const xls_analysis_document* docs)
{
 const char* body=xlsTextData(docs[0].text);usage_reader readers[4];xthread* threads[4];
 for(unsigned n=0;n<4;++n){usage_reader* r=&readers[n];*r=(usage_reader){.entry=docs[0].path,.completion=(size_t)(strstr(body,"return done")-body)+strlen("return done")-3,.signature=(size_t)(strstr(body,"\"hello\"")-body)+2-3};
  for(size_t i=0;i<3;++i){r->docs[i]=docs[(i+n)%3];if(strcmp(r->docs[i].key,"extra"))r->expected|=UINT64_C(1)<<i;}
  threads[n]=xrtThreadCreate(usageRead,r,0);CHECK(threads[n]);
 }
 for(unsigned n=0;n<4;++n){CHECK(xrtThreadWait(threads[n])==XWAIT_OK && readers[n].okay);xrtThreadDestroy(threads[n]);}return 1;
}
static int scopedSessions(void)
{
 char folder[128];snprintf(folder,sizeof(folder),"build/input-usage-%llu",(unsigned long long)xlsTestClockMicros());char* root=xrtPathAbs(folder);CHECK(root && xrtDirCreateAll(root));
 char *entry=xrtPathJoin(root,"main.xl"),*dep=xrtPathJoin(root,"dep.xl"),*extra=xrtPathJoin(root,"unrelated.xl"),*other=xrtPathJoin(root,"other.xl");CHECK(entry && dep && extra && other);
 const char* source="\xef\xbb\xbfimport dep\r\nfunc plus(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int computed=dep.value()\r\n int doneLocal=plus(computed,\"hello\")\r\n return doneLocal\r\nend\r\n";
 const char* api="export func value() as int\n return 7\nend\n";
 xls_analysis_document docs[4]={{"entry",entry,text(source),11,1},{"dep",dep,text(api),12,1},{"extra",extra,text("// first"),13,1},{"other",other,text("// other"),14,1}};
 for(size_t i=0;i<4;++i)CHECK(docs[i].text);
 trio s={xlsCheckSessionCreate(NULL),xlsCompletionSessionCreate(NULL),xlsSignatureSessionCreate(NULL)};CHECK(s.check && s.completion && s.signature);
 results held=query(&s,docs,3);CHECK(held.check && held.completion && held.signature && xlsSemanticCheckInfo(held.check).accepted && parity(&held,docs,3));
 CHECK(concurrentUsage(docs));
 uint64_t used=0;CHECK(xlsSemanticCheckInputUsage(held.check,&used) && used==3);CHECK(xlsSemanticCompletionInputUsage(held.completion,&used) && used==3);CHECK(xlsSemanticCompletionInputUsage(xlsSignatureSemantic(held.signature),&used) && used==3);CHECK(counts(&s,1,0));
 for(unsigned i=0;i<12;++i){
  char unused[160];snprintf(unused,sizeof(unused),"\xef\xbb\xbf// 中文😀 %u\r\nthis is deliberately unfinished (%u",i,i*10);xlsTextRelease(docs[2].text);docs[2].text=text(unused);++docs[2].revision;CHECK(docs[2].text);
  xls_analysis_document ordered[3]={docs[2],docs[0],docs[1]};const xls_analysis_document* batch=i%2?docs:ordered;
  results r=query(&s,batch,3);CHECK(r.check==held.check && r.completion==held.completion && r.signature==held.signature && parity(&r,batch,3));CHECK(counts(&s,1,i+1));release(&r);
 }
 /* Irrelevant bytes still obey the original revision admission contract. */
 xls_text* saved=docs[2].text;docs[2].text=text("// changed bytes at the old revision");CHECK(docs[2].text);results rejected=query(&s,docs,3);
 CHECK(!rejected.check && !rejected.completion && !rejected.signature && counts(&s,1,12));release(&rejected);xlsTextRelease(docs[2].text);docs[2].text=saved;
 CHECK(cancellation(&s,docs,&held));
 /* Unobserved identity/membership changes retain the complete model. */
 ++docs[2].identity;docs[2].revision=1;results r=query(&s,docs,3);CHECK(r.check==held.check && r.completion==held.completion && r.signature==held.signature && parity(&r,docs,3));CHECK(counts(&s,1,14));release(&r);
 const char* bad="export func value() as string\n return \"wrong type\"\nend\n";xlsTextRelease(docs[1].text);docs[1].text=text(bad);++docs[1].revision;CHECK(docs[1].text);
 r=query(&s,docs,3);CHECK(r.check && !xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,3));CHECK(counts(&s,2,14));release(&r);
 xlsTextRelease(docs[1].text);docs[1].text=text(api);++docs[1].revision;CHECK(docs[1].text);r=query(&s,docs,3);CHECK(r.check && xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,3));CHECK(counts(&s,3,14));release(&r);
 r=query(&s,docs,4);CHECK(r.check && parity(&r,docs,4));CHECK(counts(&s,3,15));release(&r);
 r=query(&s,docs,3);CHECK(r.check && parity(&r,docs,3));CHECK(counts(&s,3,16));release(&r);
 xlsCheckSessionDestroy(s.check);xlsCompletionSessionDestroy(s.completion);xlsSignatureSessionDestroy(s.signature);
 CHECK(parity(&held,docs,3));release(&held);for(size_t i=0;i<4;++i)xlsTextRelease(docs[i].text);xrtFree(entry);xrtFree(dep);xrtFree(extra);xrtFree(other);xrtFree(root);return 1;
}
static int setText(xls_analysis_document* doc,const char* source)
{xls_text* value=text(source);CHECK(value);xlsTextRelease(doc->text);doc->text=value;++doc->revision;return 1;}
static int sourceFor(xstrbuf* out,const char* module)
{
 return xrtStrBufAppendFormat(out,"\xef\xbb\xbfimport %s\r\nfunc plus(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int computed=%s.value()\r\n int doneLocal=plus(computed,\"hello\")\r\n return doneLocal\r\nend\r\n",module,module);
}
static int invalidations(trio* s,uint64_t expected)
{
 xls_check_stats a;xls_completion_stats b;xls_signature_stats c;xlsCheckSessionStats(s->check,&a);xlsCompletionSessionStats(s->completion,&b);xlsSignatureSessionStats(s->signature,&c);
 CHECK(a.invalidations==expected && b.invalidations==expected && c.invalidations==expected);return 1;
}
static int diskProofs(void)
{
 char folder[160],name[96];snprintf(folder,sizeof(folder),"build/input-usage-disk-%llu",(unsigned long long)xlsTestClockMicros());snprintf(name,sizeof(name),"usage_missing_%llu",(unsigned long long)xlsTestClockMicros());
 char* root=xrtPathAbs(folder);CHECK(root && xrtDirCreateAll(root));char* entry=xrtPathJoin(root,"entry.xl");char* extra=xrtPathJoin(root,"extra.xl");xstrbuf file={0},source={0};CHECK(xrtStrBufAppendFormat(&file,"%s.xl",name) && sourceFor(&source,name));char* dep=xrtPathJoin(root,file.Data);CHECK(entry && extra && dep && !xrtFileExists(dep));
 const char* api="export func value() as int\n return 7\nend\n";
 xls_analysis_document docs[3]={{"entry",entry,text(source.Data),31,1},{"extra",extra,text("// unused"),32,1},{"dep",dep,text(api),33,1}};for(size_t i=0;i<3;++i)CHECK(docs[i].text);
 trio s={xlsCheckSessionCreate(NULL),xlsCompletionSessionCreate(NULL),xlsSignatureSessionCreate(NULL)};CHECK(s.check && s.completion && s.signature);
 results held=query(&s,docs,2);CHECK(held.check && !xlsSemanticCheckInfo(held.check).accepted && parity(&held,docs,2));CHECK(counts(&s,1,0));
 CHECK(setText(&docs[1],"// unrelated edit while import is missing"));results r=query(&s,docs,2);CHECK(r.check==held.check && r.completion==held.completion && r.signature==held.signature && parity(&r,docs,2));CHECK(counts(&s,1,1));release(&r);
 r=query(&s,docs,3);CHECK(r.check && xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,3));CHECK(counts(&s,2,1));release(&r);
 r=query(&s,docs,2);CHECK(r.check && !xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2));CHECK(counts(&s,3,1));release(&r);
 CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)api,strlen(api)}) && setText(&docs[1],"// disk candidate appeared"));
 r=query(&s,docs,2);CHECK(r.check && xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2));CHECK(counts(&s,4,1) && invalidations(&s,1));release(&r);
 CHECK(!xlsSemanticCheckValidate(held.check) && !xlsSemanticCompletionValidate(held.completion) && !xlsSignatureValidate(held.signature));uint64_t used=99;
 CHECK(!xlsSemanticCheckInputUsage(held.check,&used) && !used);used=99;CHECK(!xlsSemanticCompletionInputUsage(held.completion,&used) && !used);release(&held);
 CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)api,strlen(api)}) && setText(&docs[1],"// same disk bytes replaced atomically"));
 r=query(&s,docs,2);CHECK(r.check && xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2));CHECK(counts(&s,5,1) && invalidations(&s,2));release(&r);
 xrtStrBufFree(&source);xrtStrBufFree(&file);CHECK(xrtStrBufAppendFormat(&file,"%s_env",name) && sourceFor(&source,file.Data) && setText(&docs[0],source.Data));
 r=query(&s,docs,2);xls_inputs inputs;CHECK(r.check && !xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2) && xlsSemanticCheckInputs(r.check,&inputs) && inputs.packagePathObserved);CHECK(counts(&s,6,1));release(&r);
 char* old=xrtEnvGet("XLANG_PACKAGE_PATH");CHECK(xrtEnvSet("XLANG_PACKAGE_PATH",root) && setText(&docs[1],"// changed observed environment too"));
 r=query(&s,docs,2);CHECK(r.check && !xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2));int restored=old ? xrtEnvSet("XLANG_PACKAGE_PATH",old) : xrtEnvRemove("XLANG_PACKAGE_PATH");CHECK(restored && counts(&s,7,1) && invalidations(&s,3));release(&r);xrtFree(old);
 xlsCheckSessionDestroy(s.check);xlsCompletionSessionDestroy(s.completion);xlsSignatureSessionDestroy(s.signature);for(size_t i=0;i<3;++i)xlsTextRelease(docs[i].text);
 xrtStrBufFree(&file);xrtStrBufFree(&source);xrtFree(root);xrtFree(entry);xrtFree(extra);xrtFree(dep);return 1;
}
static int packageProofs(void)
{
 char folder[160];snprintf(folder,sizeof(folder),"build/input-usage-package-%llu",(unsigned long long)xlsTestClockMicros());char* root=xrtPathAbs(folder);CHECK(root && xrtDirCreateAll(root));
 char *entry=xrtPathJoin(root,"entry.xl"),*extra=xrtPathJoin(root,"extra.xl"),*package=xrtPathJoin(root,"usage_pkg"),*apiDir=package?xrtPathJoin(package,"src"):NULL;
 char *manifest=package?xrtPathJoin(package,"xlang.package.json"):NULL,*api=apiDir?xrtPathJoin(apiDir,"api.xl"):NULL;CHECK(entry && extra && manifest && api && xrtDirCreateAll(apiDir));
 const char* json="{\"format\":\"xlang-package-1\",\"name\":\"usage_pkg\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"src/api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";
 const char* implementation="export func value() as int\n return 7\nend\n";CHECK(xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)json,strlen(json)}) && xrtFileWriteAtomic(api,(xbytesview){(const unsigned char*)implementation,strlen(implementation)}));
 xstrbuf source={0};CHECK(sourceFor(&source,"usage_pkg"));xls_analysis_document docs[2]={{"entry",entry,text(source.Data),41,1},{"extra",extra,text("// unused"),42,1}};CHECK(docs[0].text && docs[1].text);
 trio s={xlsCheckSessionCreate(NULL),xlsCompletionSessionCreate(NULL),xlsSignatureSessionCreate(NULL)};CHECK(s.check && s.completion && s.signature);
 results r=query(&s,docs,2);CHECK(r.check && xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2));CHECK(counts(&s,1,0));release(&r);
 CHECK(setText(&docs[1],"// source package unchanged"));r=query(&s,docs,2);CHECK(r.check && parity(&r,docs,2));CHECK(counts(&s,1,1));release(&r);
 CHECK(xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)json,strlen(json)}) && setText(&docs[1],"// same manifest bytes replaced"));
 r=query(&s,docs,2);CHECK(r.check && xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2));CHECK(counts(&s,2,1) && invalidations(&s,1));release(&r);
 char* changed=xrtStrDup(json);CHECK(changed);char* version=strstr(changed,"1.0.0");CHECK(version);version[4]='1';
 CHECK(xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)changed,strlen(changed)}) && setText(&docs[1],"// changed package manifest"));
 r=query(&s,docs,2);CHECK(r.check && xlsSemanticCheckInfo(r.check).accepted && parity(&r,docs,2));CHECK(counts(&s,3,1) && invalidations(&s,2));release(&r);
 xlsCheckSessionDestroy(s.check);xlsCompletionSessionDestroy(s.completion);xlsSignatureSessionDestroy(s.signature);for(size_t i=0;i<2;++i)xlsTextRelease(docs[i].text);
 xrtFree(changed);xrtStrBufFree(&source);xrtFree(root);xrtFree(entry);xrtFree(extra);xrtFree(package);xrtFree(apiDir);xrtFree(manifest);xrtFree(api);return 1;
}
static int batchScope(void)
{
 char *path=xrtPathAbs("input-usage-batch.xl"),*other=xrtPathAbs("input-usage-other.xl");xls_text *a=text("// a"),*b=text("// b");CHECK(path && other && a && b);
 xls_analysis_document docs[2]={{"entry",path,a,51,1},{"other",other,a,52,1}};xls_input_batch* first=xlsInputBatchCreate(NULL,NULL,docs,2,"entry",NULL,NULL,NULL,0);CHECK(first);
 docs[1].text=b;++docs[1].revision;xls_analysis_document reverse[2]={docs[1],docs[0]};xls_input_batch* second=xlsInputBatchCreate(NULL,first,reverse,2,"entry",NULL,NULL,NULL,0);CHECK(second);uint64_t remapped=99;
 CHECK(!xlsInputBatchEqual(first,second) && xlsInputBatchEqualUsed(first,second,1,NULL,&remapped) && remapped==2);
 CHECK(!xlsInputBatchEqualUsed(first,second,3,NULL,&remapped) && !remapped);CHECK(!xlsInputBatchEqualUsed(first,second,0,"other",&remapped) && !remapped);
 CHECK(!xlsInputBatchEqualUsed(first,second,4,NULL,&remapped) && !remapped);CHECK(!xlsInputBatchEqualUsed(first,second,0,"absent",&remapped) && !remapped);
 xlsInputBatchDestroy(first);first=second;docs[0].text=b;++docs[0].revision;second=xlsInputBatchCreate(NULL,first,docs,2,"entry",NULL,NULL,NULL,0);CHECK(second);
 CHECK(!xlsInputBatchEqualUsed(first,second,0,NULL,&remapped) && !remapped);xlsInputBatchDestroy(first);xlsInputBatchDestroy(second);
 char* directory=xrtPathAbs(".");CHECK(directory);xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=path,.directory=directory,.configuration="debug",.analysisProfile="fixture-linked-native",.targetKind="exe"};
 xls_context* ca=xlsContextCreate(&spec,1024*1024,NULL);spec.configuration="release";xls_context* cb=xlsContextCreate(&spec,1024*1024,NULL);CHECK(ca && cb);
 first=xlsInputBatchCreateInContext(ca,NULL,NULL,docs,2,"entry",NULL,NULL,NULL,0);second=xlsInputBatchCreateInContext(cb,NULL,first,docs,2,"entry",NULL,NULL,NULL,0);CHECK(first && second);
 CHECK(!xlsInputBatchEqualUsed(first,second,0,NULL,&remapped) && !remapped);xlsInputBatchDestroy(first);xlsInputBatchDestroy(second);xlsContextRelease(ca);xlsContextRelease(cb);xrtFree(directory);
 /* A result with no observed report must keep the conservative full set. */
 const char* plain="func main(...) as int\n return 0\nend\n";xlsTextRelease(a);a=text(plain);CHECK(a);docs[0]=(xls_analysis_document){"entry",path,a,61,1};docs[1]=(xls_analysis_document){"other",other,b,62,1};
 xls_signature_session* s=xlsSignatureSessionCreate(NULL);CHECK(s);xls_signature_help* r=xlsSignatureSessionQueryInContext(s,NULL,docs,2,"entry","entry",0,NULL,NULL,NULL,0);CHECK(r && !xlsSignatureInfo(r).available && !xlsSemanticCompletionInputUsage(xlsSignatureSemantic(r),&remapped) && !remapped);xlsSignatureRelease(r);
 docs[1].text=a;++docs[1].revision;r=xlsSignatureSessionQueryInContext(s,NULL,docs,2,"entry","entry",0,NULL,NULL,NULL,0);CHECK(r);xls_signature_stats stats;xlsSignatureSessionStats(s,&stats);CHECK(stats.builds==2 && !stats.inputScopeHits);xlsSignatureRelease(r);xlsSignatureSessionDestroy(s);
 xlsTextRelease(a);xlsTextRelease(b);xrtFree(path);xrtFree(other);return 1;
}
int main(void){if(!observation() || !batchScope() || !scopedSessions() || !diskProofs() || !packageProofs())return 1;printf("analysis_input_usage_test passed: %u checks\n",checks);return 0;}
