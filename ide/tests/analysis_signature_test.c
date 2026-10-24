#include "analysis/signature.h"
#include "analysis/frontend_internal.h"
#include "analysis/signature_session.h"
#include "analysis/worker.h"
#include "analysis/signature_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_signature_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static const char* functions="func plus(int left,string right=\"\") as int\n return left\nend\n";
static int walkCaretSource(const char* source,size_t size,size_t start);
static int one(const char* definitions,const char* body,const char* label,size_t parameter,int available)
{
 const char* caret=strstr(body,"@caret@");CHECK(caret);xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,"%sfunc main(...) as int\n%s",definitions,body));size_t prefix=strlen(definitions)+strlen("func main(...) as int\n");size_t offset=prefix+(size_t)(caret-body);
 memmove(text.Data+offset,text.Data+offset+7,text.Size-offset-7+1);text.Size-=7;
 size_t bom=text.Size>=3 && !memcmp(text.Data,"\xef\xbb\xbf",3) ? 3 : 0;
 char* path=xrtPathAbs("signature-test.xl");CHECK(path);xls_semantic_input input={path,text.Data,text.Size};char error[256]={0};xls_signature_help* result=xlsSignatureCreateInContext(NULL,path,path,offset-bom,&input,1,NULL,NULL,error,sizeof(error));
 if(!result)fprintf(stderr,"signature creation error: %s\n",error);
 CHECK(result);xls_signature_info info=xlsSignatureInfo(result);
 if(info.available!=available || (available && !info.count))fprintf(stderr,"signature unavailable: %s projected=%d\n",info.error ? info.error : "",info.projected);
 CHECK(info.available==available && xlsSignatureValidate(result));
 if(available){CHECK(info.count && info.activeSignature<info.count && info.projected);xls_signature_item item;CHECK(xlsSignatureItem(result,info.activeSignature,&item));if(strcmp(item.label,label))fprintf(stderr,"signature actual=%s expected=%s\n",item.label,label);CHECK(!strcmp(item.label,label) && item.activeParameter==parameter);
  for(size_t i=0;i<item.parameters;++i){xls_signature_parameter p;CHECK(xlsSignatureParameter(result,info.activeSignature,i,&p) && p.labelStart<p.labelEnd && p.labelEnd<=strlen(item.label));}
  xls_inputs inputs;CHECK(xlsSignatureInputs(result,&inputs) && inputs.sources);int source=0;for(size_t i=0;i<inputs.sources;++i){xls_input_source captured;CHECK(xlsSignatureSource(result,i,&captured));if(captured.text && !strcmp(captured.path,path)){CHECK(captured.size==text.Size-bom && !memcmp(captured.text,text.Data+bom,text.Size-bom));source=1;}}CHECK(source);
 }
 if(available)CHECK(walkCaretSource(text.Data,text.Size,prefix-bom));
 xlsSignatureRelease(result);xrtFree(path);xrtStrBufFree(&text);return 1;
}
static const XL6_AstNode* findCall(const XL6_AstNode* n,size_t open,const char* path)
{
 if(!n)return NULL;
 if(n->kind==XL6_AST_CALL && n->tokenRange.known && n->tokenRange.startOffset==open && xl6AstSourcePath(n) && !strcmp(xl6AstSourcePath(n),path))return n;
 for(size_t i=0;i<n->childCount;++i){const XL6_AstNode* found=findCall(n->children[i],open,path);if(found)return found;}return NULL;
}
static int originalOracle(const char* marked)
{
 const char* caret=strstr(marked,"@caret@");CHECK(caret);char* source=xrtStrDup(marked);CHECK(source);size_t offset=(size_t)(caret-marked);memmove(source+offset,source+offset+7,strlen(source+offset+7)+1);char* path=xrtPathAbs("signature-oracle.xl");CHECK(path);xls_semantic_input input={path,source,strlen(source)};xls_signature_help* result=xlsSignatureCreateInContext(NULL,path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(result && xlsSignatureInfo(result).available);xls_signature_item item;CHECK(xlsSignatureItem(result,xlsSignatureInfo(result).activeSignature,&item));xls_semantic_check* full=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);
 if(full && !xlsSemanticCheckInfo(full).accepted){for(size_t i=0;i<xlsSemanticCheckInfo(full).diagnostics;++i){xls_check_diagnostic d;if(xlsSemanticCheckDiagnostic(full,i,&d))fprintf(stderr,"oracle diagnostic: %s\n",d.message);}}
 CHECK(full && xlsSemanticCheckInfo(full).accepted);const XL6_AstNode* call=findCall(xlsSemanticCheckAst(full),xlsSemanticCompletionInfo(xlsSignatureSemantic(result)).start,path);CHECK(call && call->semanticTargetSymbolId && item.symbol==call->semanticTargetSymbolId);xlsSemanticCheckDestroy(full);xlsSignatureRelease(result);xrtFree(path);xrtFree(source);return 1;
}
static int sameText(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int parity(xls_signature_help* a,xls_signature_help* b)
{
 xls_signature_info x=xlsSignatureInfo(a),y=xlsSignatureInfo(b);CHECK(x.offset==y.offset && sameText(x.path,y.path) && x.available==y.available && x.count==y.count && x.activeSignature==y.activeSignature && x.argument==y.argument && x.projected==y.projected);
 for(size_t i=0;i<x.count;++i){xls_signature_item left,right;CHECK(xlsSignatureItem(a,i,&left) && xlsSignatureItem(b,i,&right));CHECK(sameText(left.label,right.label) && sameText(left.documentation,right.documentation) && left.parameters==right.parameters && left.activeParameter==right.activeParameter && left.symbol==right.symbol);
  for(size_t j=0;j<left.parameters;++j){xls_signature_parameter p,q;CHECK(xlsSignatureParameter(a,i,j,&p) && xlsSignatureParameter(b,i,j,&q));CHECK(sameText(p.name,q.name) && p.labelStart==q.labelStart && p.labelEnd==q.labelEnd && p.optional==q.optional && p.variadic==q.variadic);}
 }
 return 1;
}
static uint64_t caretViews;
static int walkCaretSource(const char* source,size_t size,size_t start)
{
 char* path=xrtPathAbs("signature-caret-walk.xl");CHECK(path);
 xls_text* text=xlsTextCreate(source,size,8u*1024u*1024u,NULL);xls_signature_session* session=xlsSignatureSessionCreate(NULL);CHECK(text && session);
 xls_analysis_document doc={"entry",path,text,71,1};xls_semantic_input input={path,source,size};size_t bom=size>=3 && !memcmp(source,"\xef\xbb\xbf",3) ? 3 : 0;
 xls_signature_help *previous=NULL,*held=NULL,*heldView=NULL;size_t previousOffset=0,heldOffset=0,viewOffset=0;
 /* Walk both ways: fresh parser/semantic results at every scalar boundary are
    independent of the retained call-site/model reuse path. Includes nested
    calls, commas, named prefixes, literals, comments and outside-call exits. */
 for(unsigned reverse=0;reverse<2;++reverse)for(size_t n=start;n<=size-bom;++n){
  size_t at=reverse ? size-bom-(n-start) : n;size_t raw=at+bom;
  if(raw<size && (((unsigned char)source[raw]&0xc0)==0x80 || (raw && source[raw-1]=='\r' && source[raw]=='\n')))continue;
  xls_signature_help* result=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);
  xls_signature_help* fresh=xlsSignatureCreateInContext(NULL,path,path,at,&input,1,NULL,NULL,NULL,0);
  if(!result || !fresh || !parity(result,fresh)){fprintf(stderr,"caret parity failed at %zu reverse=%u source=[%s]\n",at,reverse,source);CHECK(0);}
  CHECK(xlsSignatureInfo(result).offset==at);
  if(previous){CHECK(xlsSignatureInfo(previous).offset==previousOffset);if(previous!=result && xlsSignatureInfo(result).available && xlsSignatureSemantic(previous)==xlsSignatureSemantic(result)){
   CHECK(previousOffset!=at);++caretViews;if(!heldView){heldView=xlsSignatureRetain(result);viewOffset=at;}
  }}
  if(!held && xlsSignatureInfo(result).available){held=xlsSignatureRetain(result);heldOffset=at;}
  xlsSignatureRelease(previous);previous=result;previousOffset=at;xlsSignatureRelease(fresh);
 }
 xlsSignatureRelease(previous);xlsSignatureSessionDestroy(session);xlsTextRelease(text);
 if(held){xls_signature_item item;CHECK(xlsSignatureValidate(held) && xlsSignatureInfo(held).offset==heldOffset && xlsSignatureItem(held,0,&item) && item.label && *item.label);}
 if(heldView){xls_signature_item item;CHECK(xlsSignatureValidate(heldView) && xlsSignatureInfo(heldView).offset==viewOffset && xlsSignatureItem(heldView,0,&item) && item.label && *item.label);}
 xlsSignatureRelease(held);xlsSignatureRelease(heldView);xrtFree(path);return 1;
}
static int session(void)
{
 char* path=xrtPathAbs("signature-session.xl");CHECK(path);
 /* The stdio/IDE host uses forward slashes; XRT normalizes its model path. */
 for(char* p=path;*p;++p)if(*p=='\\')*p='/';
 xls_signature_session* s=xlsSignatureSessionCreate(NULL);CHECK(s);xls_analysis_document doc={"entry",path,NULL,23,1};xls_signature_help *held=NULL,*heldWarm=NULL;xls_signature_storage_work warmStorage={0},lastStorage={0};
 for(unsigned i=0;i<16;++i){xstrbuf source={0};for(unsigned j=0;j<24;++j)CHECK(xrtStrBufAppendFormat(&source,"func stable_%u(int value) as int\n int local=value+%u\n if local>0 then\n return local\n end\n return 0\nend\n",j,j));CHECK(xrtStrBufAppendFormat(&source,"%sfunc main(...) as int\n return plus(%u,\"%u\")\nend\n",functions,i,i));xlsTextRelease(doc.text);doc.text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);CHECK(doc.text);++doc.revision;size_t offset=(size_t)(strrchr(source.Data,'"')-source.Data);xls_sem_storage_control control;xlsFrontendSemStorageBegin(&control,i==2 ? 4096 : 64u*1024u*1024u);xls_signature_help* r=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);xlsFrontendSemStorageEnd(&control,NULL);CHECK(r && xlsSignatureInfo(r).available);xls_semantic_input in={path,source.Data,source.Size};xls_signature_help* fresh=xlsSignatureCreateInContext(NULL,path,path,offset,&in,1,NULL,NULL,NULL,0);CHECK(fresh && parity(r,fresh));xlsSignatureRelease(fresh);
  xls_signature_storage_work storage=xlsSignatureStorageWork(r);lastStorage=storage;
  if(i==2)CHECK(!storage.nodes && !storage.pages && !storage.chargedBytes && storage.heapFallbacks>=24);
  else if(i)CHECK(storage.nodes>storage.pages && storage.pages>=24 && storage.chargedBytes>0 && storage.chargedBytes<=64u*1024u*1024u && !storage.heapFallbacks);
  else CHECK(!storage.nodes && !storage.pages && !storage.chargedBytes && !storage.heapFallbacks);
  if(i==1){heldWarm=xlsSignatureRetain(r);warmStorage=storage;}
  xls_signature_parse_work parse=xlsSignatureParseWork(r);
  if(i){CHECK(xlsSignatureWork(r).reused>=24);CHECK(!parse.full && parse.fragments==1 && parse.reused==25 && parse.bytes<source.Size/4);}
  else CHECK(parse.full==1 && !parse.fragments && !parse.reused);
  CHECK(parse.cacheBytes>0 && parse.cacheBytes<=32u*1024u*1024u);
  xls_signature_stats stats;xlsSignatureSessionStats(s,&stats);CHECK(stats.builds==i+1 && stats.indexBuilds==i+1 && stats.hits==i);
  ++doc.revision;xls_signature_help* hit=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(hit==r);xlsSignatureRelease(hit);
  if(!i)held=xlsSignatureRetain(r);
  xlsSignatureRelease(r);xrtStrBufFree(&source);
 }
 size_t offset=(size_t)(strrchr(xlsTextData(doc.text),'"')-xlsTextData(doc.text))-1;
 xls_signature_help* moved=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(moved && xlsSignatureInfo(moved).available);xls_signature_stats stats;xlsSignatureSessionStats(s,&stats);CHECK(stats.indexHits==1 && stats.caretHits==1 && stats.builds==16 && stats.indexBuilds==16);xls_signature_storage_work movedStorage=xlsSignatureStorageWork(moved);CHECK(movedStorage.nodes==lastStorage.nodes && movedStorage.pages==lastStorage.pages && movedStorage.chargedBytes==lastStorage.chargedBytes && movedStorage.heapFallbacks==lastStorage.heapFallbacks);xlsSignatureRelease(moved);
 ++doc.identity;doc.revision=1;moved=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(moved && !xlsSignatureWork(moved).reused);xlsSignatureRelease(moved);xlsSignatureSessionDestroy(s);xlsTextRelease(doc.text);CHECK(held && xlsSignatureValidate(held) && xlsSignatureInfo(held).available);xls_signature_item item;CHECK(xlsSignatureItem(held,0,&item) && strstr(item.label,"plus("));xlsSignatureRelease(held);
 xls_signature_storage_work retained=xlsSignatureStorageWork(heldWarm);CHECK(heldWarm && xlsSignatureValidate(heldWarm) && xlsSignatureItem(heldWarm,0,&item) && retained.nodes==warmStorage.nodes && retained.pages==warmStorage.pages && retained.chargedBytes==warmStorage.chargedBytes && retained.heapFallbacks==warmStorage.heapFallbacks);xlsSignatureRelease(heldWarm);xrtFree(path);return 1;
}
static int parseIncrementality(void)
{
 /* Every edited snapshot has a separate fresh-parser oracle. Exercise length,
    line/scalar offsets, nested calls, edits outside the query function and
    multiple-function edits. Header/class changes must use the original parser. */
 const char* stable="func stable(int value) as int\r\n return value\r\nend\r\n";
 const char* changed="func stable(int value) as int\r\n return value+1\r\nend\r\n";
 const char* tails[]={
  "func main(...) as int\r\n return plus(7,\"@a\")\r\nend\r\n",
  "func main(...) as int\r\n int 中文=8\r\n\r\n return plus(中文,\"@😀 long\")\r\nend\r\n",
  "func main(...) as int\r\n return plus(plus(1,\"inner\"),\"@outer\")\r\nend\r\n",
  "func main(...) as int\r\n return plus(7,\"@a\")\r\nend\r\n",
  "func main(...) as int\r\n return plus(7,@\r\nend\r\n",
  "func main(...) as int\r\n return plus(7,\"un@finished\r\nend\r\n",
  "func main(...) as int\r\n return plus(7,\"@repaired\")\r\nend\r\n",
 };
 char* path=xrtPathAbs("signature-parse-incremental.xl");char* otherPath=xrtPathAbs("signature-parse-other.xl");
 xls_signature_session* session=xlsSignatureSessionCreate(NULL);CHECK(path && otherPath && session);
 xls_analysis_document docs[2]={{"entry",path,NULL,53,1},{"other",otherPath,NULL,54,1}};
 docs[1].text=xlsTextCreate("// other0\n",10,1024,NULL);CHECK(docs[1].text);
 xls_signature_help* held=NULL;size_t oldCharge=0;
 for(unsigned i=0;i<14;++i){
  const char* header=i>=9 ? "func plus(int left,string right=\"new\") as int\n return left\nend\n" : functions;
  const char* tail=i>=12 ? (i==12 ? "class Box\r\n func run() as int\r\n return plus(7,\"@class\")\r\n end\r\nend\r\n" : "class Box\r\n func run() as int\r\n return plus(8,\"@changed class\")\r\n end\r\nend\r\n") : tails[i<7 ? i : i==11 ? 1 : 0];
  xstrbuf source={0};CHECK(xrtStrBufAppendFormat(&source,"\xef\xbb\xbf%s%s%sfunc trailing() as int\r\n return 0\r\nend\r\n",i>=8 ? changed : stable,header,tail));
  size_t raw=(size_t)(strchr(source.Data,'@')-source.Data),offset=raw-3;memmove(source.Data+raw,source.Data+raw+1,source.Size-raw);--source.Size;
  xlsTextRelease(docs[0].text);docs[0].text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);++docs[0].revision;CHECK(docs[0].text);
  xls_signature_help* result=xlsSignatureSessionQueryInContext(session,NULL,docs,2,"entry","entry",offset,NULL,NULL,NULL,0);
  xls_semantic_input inputs[2]={{path,source.Data,source.Size},{otherPath,xlsTextData(docs[1].text),xlsTextSize(docs[1].text)}};
  xls_signature_help* fresh=xlsSignatureCreateInContext(NULL,path,path,offset,inputs,2,NULL,NULL,NULL,0);CHECK(result && fresh && parity(result,fresh) && xlsSignatureInfo(result).available);
  xls_signature_parse_work work=xlsSignatureParseWork(result);
  if(i>=1 && i<=3)CHECK(!work.full && work.fragments==1 && work.reused==3 && work.bytes<source.Size);
  if(i==8 || i==9 || i>=12)CHECK(work.full==1 && work.fallbacks==1);
  if(i==11)CHECK(!work.full && work.fragments==1 && work.reused==3);
  CHECK(work.cacheBytes>0 && work.cacheBytes<=32u*1024u*1024u);
  xls_inputs report;CHECK(xlsSignatureInputs(result,&report));int captured=0;
  for(size_t j=0;j<report.sources;++j){xls_input_source item;CHECK(xlsSignatureSource(result,j,&item));if(!strcmp(item.path,path)){CHECK(item.size==source.Size-3 && !memcmp(item.text,source.Data+3,item.size));captured=1;}}
  CHECK(captured);
  if(!i){held=xlsSignatureRetain(result);oldCharge=work.cacheBytes;
   const xls_syntax* syntax=xlsSignatureData(result)->syntax;CHECK(syntax && !xlsSyntaxCursorMatches(syntax,path,offset));
   XL6_CompletionQuery q={.path=path,.offset=offset};XL6_ParseResult parsed;xl6ParseResultInit(&parsed);size_t charge=42;
   CHECK(!xlsSyntaxCopyCursorParse(syntax,&q,&parsed) && !parsed.root && !q.prefix && !xlsSyntaxCacheBytes(syntax,SIZE_MAX,&charge) && !charge);
   CHECK(!xlsSyntaxCopySignatureParse(syntax,&q,&parsed,0) && !parsed.root && !q.prefix);
   size_t open=xlsSemanticCompletionInfo(xlsSignatureSemantic(result)).start;
   CHECK(xlsSyntaxCopySignatureParse(syntax,&q,&parsed,open) && parsed.root && q.parsed==1 && q.prefix && !*q.prefix && q.replacement.startOffset==open);
   CHECK(!xlsSyntaxCaptureSignature(path,docs[0].text,parsed.root,&q,1));xl6ParseResultFree(&parsed);xl6CompletionFree(&q);
  }
  if(i==10){ /* An unobserved overlay edit reuses the immutable model. */
   xls_signature_stats before;xlsSignatureSessionStats(session,&before);
   xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate("// other1\n",10,1024,NULL);++docs[1].revision;CHECK(docs[1].text);
   xls_signature_help* exact=xlsSignatureSessionQueryInContext(session,NULL,docs,2,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(exact==result && parity(exact,fresh));
   xls_signature_parse_work hit=xlsSignatureParseWork(exact);CHECK(hit.full==work.full && hit.fragments==work.fragments && hit.bytes==work.bytes && hit.exactHits==work.exactHits && hit.cacheBytes==work.cacheBytes);
   xls_signature_stats after;xlsSignatureSessionStats(session,&after);CHECK(after.builds==before.builds && after.hits==before.hits+1 && after.inputScopeHits==before.inputScopeHits+1);xlsSignatureRelease(exact);
  }
  xlsSignatureRelease(fresh);xlsSignatureRelease(result);xrtStrBufFree(&source);
 }
 xlsSignatureSessionDestroy(session);xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);
 CHECK(held && xlsSignatureValidate(held) && xlsSignatureParseWork(held).cacheBytes==oldCharge && xlsSignatureParseWork(held).full==1);
 xls_signature_item item;CHECK(xlsSignatureItem(held,0,&item) && !strcmp(item.label,"plus(int left, string right = ...) as int"));xlsSignatureRelease(held);xrtFree(path);xrtFree(otherPath);return 1;
}
static int sameFullAst(const XL6_AstNode* a,const XL6_AstNode* b,int sources)
{
    if(!a || !b) return a==b;
    XL6_AstNode left=*a,right=*b;
    left.text=right.text=left.doc=right.doc=NULL; left.literalData=right.literalData=NULL;
    left.children=right.children=NULL; left.inferredType=right.inferredType=left.semanticDeclaredType=right.semanticDeclaredType=NULL;
    left.xlsStorage=right.xlsStorage=NULL;
    left.source=right.source=NULL; left.childCapacity=right.childCapacity=0;
    if(memcmp(&left,&right,sizeof(left)) || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize))) return 0;
    if(a->source) {
        if(!b->source || b->source==a->source || !sameText(a->source->path,b->source->path))return 0;
        if(sources && (a->source->completionQuery!=b->source->completionQuery || a->source->completionStart!=b->source->completionStart ||
           a->source->completionEnd!=b->source->completionEnd))return 0;
    } else if(b->source) return 0;
    if(!sameFullAst(a->inferredType,b->inferredType,sources) || !sameFullAst(a->semanticDeclaredType,b->semanticDeclaredType,sources)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!sameFullAst(a->children[i],b->children[i],sources)) return 0;
    return 1;
}


typedef struct signature_ast_reader {xls_signature_help* result;const XL6_AstNode* expected;int okay;} signature_ast_reader;
static int32 signatureAstRead(void* user)
{
 signature_ast_reader* r=user;r->okay=1;
 for(unsigned i=0;i<12;++i){const XL6_AstNode* ast=xlsSemanticCompletionAst(xlsSignatureSemantic(r->result));xls_sem_tail_work w=xlsSemanticCompletionTailWork(xlsSignatureSemantic(r->result));if(!ast || !sameFullAst(ast,r->expected,1) || !w.deferred || !w.materialized || w.retainedBytes>8u*1024u*1024u || !xlsSignatureValidate(r->result))r->okay=0;}
 xlsSignatureRelease(r->result);return 0;
}
static int retainedFullAst(void)
{
 char* path=xrtPathAbs("signature-retained-full-ast.xl");CHECK(path);
 const size_t budgets[]={0,1,8192,8u*1024u*1024u};xls_signature_help *held=NULL,*oracle=NULL;
 for(unsigned b=0;b<4;++b){
  xls_signature_session* session=xlsSignatureSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,211+b,1};
  xls_sem_tail_control outer;xlsFrontendSemTailBegin(&outer,0);
  for(unsigned i=0;i<5;++i){
   xstrbuf source={0};CHECK(xrtStrBufAppendFormat(&source,"\xef\xbb\xbf// 中文😀 generation %u%s\r\n%s",i*100,i%2?" with longer Unicode 中文😀\r\n// extra line":"",functions));
   for(unsigned j=0;j<32;++j)CHECK(xrtStrBufAppendFormat(&source,"func stable_%u(int value) as int\r\n int local=value+%u\r\n if local>0 then\r\n return local\r\n end\r\n return 0\r\nend\r\n",j,j));
   CHECK(xrtStrBufAppendFormat(&source,"class Box\r\n func get(int value) as int\r\n return stable_0(value)\r\n end\r\nend\r\nfunc main(...) as int\r\n auto callback=stable_1\r\n auto box=Box()\r\n int typed=callback(%u)+box.get(%u)\r\n return %s(typed,\"😀 %u\")\r\nend\r\n",i,i,i==4?"missing":"plus",i));
   size_t offset=(size_t)(strrchr(source.Data,'"')-source.Data)-3; ++doc.revision;xlsTextRelease(doc.text);doc.text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);CHECK(doc.text);
   xls_sem_tail_control inner;xlsFrontendSemTailBegin(&inner,budgets[b]);xls_signature_help* r=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);xlsFrontendSemTailEnd(&inner);
   xls_semantic_input in={path,source.Data,source.Size};xls_signature_help* fresh=xlsSignatureCreateInContext(NULL,path,path,offset,&in,1,NULL,NULL,NULL,0);CHECK(r && fresh && parity(r,fresh));
   xls_sem_tail_work before=xlsSemanticCompletionTailWork(xlsSignatureSemantic(r));CHECK(!before.materialized && before.retainedBytes<=budgets[b] && !xlsSemanticCompletionTailWork(xlsSignatureSemantic(fresh)).deferred);
   if(i && i<4){CHECK(xlsSignatureWork(r).reused>=32);if(b==3)CHECK(before.deferred && before.retainedBytes>0);if(b<2)CHECK(!before.deferred && !before.retainedBytes);}
   if(i==4)CHECK(!xlsSignatureInfo(r).available && !before.deferred && !before.retainedBytes);
   /* A projected signature tree is not a fully checked call. Independently
      check the actual complete source through the ordinary typed IR pipeline.
      Its already checked helpers must keep every annotation and coordinate;
      only the original completion-source stamp differs between these modes. */
   xls_semantic_check* full=xlsSemanticCheckCreate(path,&in,1,0,NULL,NULL,NULL,0);CHECK(full);
   if(i<4){
    xls_check_info checked=xlsSemanticCheckInfo(full);CHECK(checked.accepted && checked.irTypes && checked.irNodes && xlsSemanticCheckIr(full));
    const XL6_AstNode *projected=xlsSemanticCompletionAst(xlsSignatureSemantic(fresh)),*ordinary=xlsSemanticCheckAst(full);CHECK(projected && ordinary);
    size_t helpers=0;
    for(size_t n=0;n<projected->childCount;++n){const XL6_AstNode* fn=projected->children[n];
     if(fn->kind!=XL6_AST_FUNC || !fn->childCount || !fn->children[0]->text || strncmp(fn->children[0]->text,"stable_",7))continue;
     const XL6_AstNode* match=NULL;
     for(size_t j=0;j<ordinary->childCount;++j){const XL6_AstNode* other=ordinary->children[j];if(other->kind==XL6_AST_FUNC && other->childCount && sameText(other->children[0]->text,fn->children[0]->text)){match=other;break;}}
     CHECK(match && sameFullAst(fn,match,0));++helpers;
    }
    CHECK(helpers==32);
    xls_signature_item selected;CHECK(xlsSignatureItem(r,xlsSignatureInfo(r).activeSignature,&selected));
    const XL6_AstNode* call=findCall(ordinary,xlsSemanticCompletionInfo(xlsSignatureSemantic(r)).start,path);CHECK(call && call->semanticTargetSymbolId && call->semanticTargetSymbolId==selected.symbol);
   }else CHECK(!xlsSemanticCheckInfo(full).accepted && xlsSemanticCheckInfo(full).diagnostics);
   xlsSemanticCheckDestroy(full);
   if(b==3 && i==3){held=xlsSignatureRetain(r);oracle=xlsSignatureRetain(fresh);}
   else {
    const XL6_AstNode* actual=xlsSemanticCompletionAst(xlsSignatureSemantic(r));CHECK(actual && sameFullAst(actual,xlsSemanticCompletionAst(xlsSignatureSemantic(fresh)),1));
    xls_sem_tail_work after=xlsSemanticCompletionTailWork(xlsSignatureSemantic(r));CHECK(after.deferred==before.deferred && after.materialized==before.deferred && after.retainedBytes<=before.retainedBytes && actual==xlsSemanticCompletionAst(xlsSignatureSemantic(r)));
   }
   xlsSignatureRelease(r);xlsSignatureRelease(fresh);xrtStrBufFree(&source);
  }
  xlsFrontendSemTailEnd(&outer);xlsSignatureSessionDestroy(session);xlsTextRelease(doc.text);
 }
 CHECK(held && oracle && xlsSemanticCompletionTailWork(xlsSignatureSemantic(held)).deferred && !xlsSemanticCompletionTailWork(xlsSignatureSemantic(held)).materialized);
 const XL6_AstNode* expected=xlsSemanticCompletionAst(xlsSignatureSemantic(oracle));CHECK(expected);signature_ast_reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(signature_ast_reader){xlsSignatureRetain(held),expected,0};threads[i]=xrtThreadCreate(signatureAstRead,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 CHECK(xlsSemanticCompletionTailWork(xlsSignatureSemantic(held)).materialized && xlsSemanticCompletionTailWork(xlsSignatureSemantic(held)).retainedBytes<=8u*1024u*1024u);xlsSignatureRelease(held);xlsSignatureRelease(oracle);xrtFree(path);return 1;
}

typedef struct cancel_state {unsigned calls,at;} cancel_state;
static int cancelAt(void* user){cancel_state* c=user;return ++c->calls==c->at;}
static int caretCancellationAndLifetime(void)
{
 const char* source="func target(string value) as int\n return 0\nend\nfunc main(...) as int\n return target(\"abcdef\")\nend\n";
 char* path=xrtPathAbs("signature-caret-cancel.xl");xls_text* text=xlsTextCreate(source,strlen(source),1024,NULL);xls_signature_session* session=xlsSignatureSessionCreate(NULL);CHECK(path && text && session);
 xls_analysis_document doc={"entry",path,text,101,1};size_t at=(size_t)(strstr(source,"abcdef")-source)+1;
 xls_signature_help* initial=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);CHECK(initial && xlsSignatureInfo(initial).available);
 cancel_state c={0};xls_signature_help* moved=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",at+1,cancelAt,&c,NULL,0);CHECK(moved && moved!=initial && xlsSignatureSemantic(moved)==xlsSignatureSemantic(initial));unsigned calls=c.calls;CHECK(calls>=2);xlsSignatureRelease(moved);
 xls_signature_help* cached=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);CHECK(cached && cached!=initial && xlsSignatureInfo(initial).offset==at);
 for(unsigned i=1;i<=calls;++i){c=(cancel_state){0,i};char error[160]={0};moved=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",at+1,cancelAt,&c,error,sizeof(error));CHECK(!moved && strstr(error,"cancel"));xls_signature_help* same=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);CHECK(same==cached && xlsSignatureInfo(same).offset==at);xlsSignatureRelease(same);}
 xls_signature_stats stats;xlsSignatureSessionStats(session,&stats);CHECK(stats.builds==1 && stats.caretHits==2 && stats.indexHits==2);
 for(unsigned i=0;i<10000;++i){size_t next=at+1+(i%2);moved=xlsSignatureSessionQueryInContext(session,NULL,&doc,1,"entry","entry",next,NULL,NULL,NULL,0);CHECK(moved && moved!=cached && xlsSignatureInfo(moved).offset==next && xlsSignatureInfo(cached).offset==(i ? at+1+((i-1)%2) : at) && xlsSignatureSemantic(moved)==xlsSignatureSemantic(initial));xlsSignatureRelease(cached);cached=moved;}
 xlsSignatureSessionStats(session,&stats);CHECK(stats.builds==1 && stats.caretHits==10002 && stats.indexBuilds==1);
 xlsSignatureSessionDestroy(session);xlsTextRelease(text);CHECK(xlsSignatureValidate(cached) && xlsSignatureInfo(initial).offset==at && xlsSignatureInfo(cached).offset==at+2);xls_signature_item item;CHECK(xlsSignatureItem(cached,0,&item) && strstr(item.label,"target(string value)"));
 printf("Caret cancellation checkpoints=%u;10000flat-owner views preserve held offsets/lifetime\n",calls);xlsSignatureRelease(cached);xlsSignatureRelease(initial);xrtFree(path);return 1;
}
static int cancellation(void)
{
 CHECK(caretCancellationAndLifetime());
 char* path=xrtPathAbs("signature-cancel.xl");CHECK(path);const char* before="func target(int value) as int\n return value\nend\nfunc main(...) as int\n return target(7)\nend\n";xls_text* body=xlsTextCreate(before,strlen(before),1024,NULL);CHECK(body);xls_analysis_document doc={"entry",path,body,37,1};size_t offset=(size_t)(strstr(before,"target(7)")-before)+strlen("target(");xls_signature_session* s=xlsSignatureSessionCreate(NULL);CHECK(s);xls_signature_help* stable=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(stable && xlsSignatureInfo(stable).available);
 xls_text_edit edit={.text="8",.length=1,.hasRange=1};CHECK(xlsTextPosition(body,offset,&edit.start) && xlsTextPosition(body,offset+1,&edit.end));xls_text* next=xlsTextApply(body,&edit,1,1024,NULL);CHECK(next);xls_analysis_document changed=doc;changed.text=next;changed.revision=2;cancel_state c={0};xls_signature_session* calibrate=xlsSignatureSessionCreate(NULL);CHECK(calibrate);xls_signature_help* seed=xlsSignatureSessionQueryInContext(calibrate,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(seed);xlsSignatureRelease(seed);xls_signature_help* result=xlsSignatureSessionQueryInContext(calibrate,NULL,&changed,1,"entry","entry",offset,cancelAt,&c,NULL,0);CHECK(result && xlsSignatureInfo(result).available && c.calls>20);unsigned calls=c.calls;xlsSignatureRelease(result);xlsSignatureSessionDestroy(calibrate);
 for(unsigned at=1;at<=calls;++at){c=(cancel_state){0,at};char error[256]={0};result=xlsSignatureSessionQueryInContext(s,NULL,&changed,1,"entry","entry",offset,cancelAt,&c,error,sizeof(error));if(result)fprintf(stderr,"unexpected signature cancellation result at=%u,total=%u\n",at,calls);CHECK(!result && strstr(error,"cancel"));xls_signature_help* same=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(same==stable && xlsSignatureInfo(stable).available);xlsSignatureRelease(same);}
 printf("Signature cancellation checkpoints=%u,all preserve last batch/result\n",calls);xlsSignatureRelease(stable);xlsSignatureSessionDestroy(s);xlsTextRelease(next);xlsTextRelease(body);xrtFree(path);return 1;
}
static void wake(void* user){xrtAtomic32FetchAdd(user,1,XMEMORY_RELAXED);}
static enum xls_query_state waitSignature(xls_query* query,xls_signature_help** result,xls_signature_stats* stats,uint64_t* thread)
{uint64 until=xrtClock()+15000000;do{enum xls_query_state state=xlsQuerySignatureResult(query,result,stats,thread,NULL,0);if(state!=XLS_QUERY_PENDING)return state;xrtSleepUs(500);}while(xrtClock()<until);return XLS_QUERY_PENDING;}
static int actor(void)
{
 xls_worker* worker=xlsWorkerCreate(NULL,2);CHECK(worker);char* path=xrtPathAbs("signature-actor.xl");CHECK(path);const char* source="func apply(int value) as int\n return value\nend\nfunc main(...) as int\n return apply(7)\nend\n";xls_text* text=xlsTextCreate(source,strlen(source),1024,NULL);CHECK(text);xls_analysis_document doc={"entry",path,text,42,1};size_t offset=(size_t)(strstr(source,"apply(7)")-source)+strlen("apply(");xls_query* q=xlsWorkerSignatureInContext(worker,NULL,&doc,1,"entry","entry",offset);CHECK(q);xatomic32 wakes;xrtAtomic32Init(&wakes,0);CHECK(xlsQuerySetWake(q,wake,&wakes));xls_signature_help* first=NULL;xls_signature_stats stats;uint64_t thread=0;CHECK(waitSignature(q,&first,&stats,&thread)==XLS_QUERY_READY && first && xlsSignatureInfo(first).available && thread && thread!=xrtThreadCurrentId() && stats.builds==1);uint64_t owner=thread;xls_semantic_completion* wrong=NULL;CHECK(xlsQueryCompletionResult(q,&wrong,NULL,NULL,NULL,0)==XLS_QUERY_FAILED && !wrong);xlsQueryDestroy(q);CHECK(xrtAtomic32Load(&wakes,XMEMORY_RELAXED)==1);
 for(unsigned i=0;i<12;++i){++doc.revision;q=xlsWorkerSignatureInContext(worker,NULL,&doc,1,"entry","entry",offset);CHECK(q);xls_signature_help* result=NULL;CHECK(waitSignature(q,&result,&stats,&thread)==XLS_QUERY_READY && result==first && thread==owner && stats.builds==1 && stats.hits==i+1);xlsSignatureRelease(result);xlsQueryDestroy(q);}
 q=xlsWorkerCompletion(worker,&doc,1,"entry","entry",offset);CHECK(q);xls_semantic_completion* ordinary=NULL;uint64 waitUntil=xrtClock()+15000000;enum xls_query_state ordinaryState;do{ordinaryState=xlsQueryCompletionResult(q,&ordinary,NULL,NULL,NULL,0);if(ordinaryState!=XLS_QUERY_PENDING)break;xrtSleepUs(500);}while(xrtClock()<waitUntil);CHECK(ordinaryState==XLS_QUERY_READY && ordinary);xls_signature_help* wrongSignature=NULL;CHECK(xlsQuerySignatureResult(q,&wrongSignature,NULL,NULL,NULL,0)==XLS_QUERY_FAILED && !wrongSignature);xlsSemanticCompletionDestroy(ordinary);xlsQueryDestroy(q);
 q=xlsWorkerSignatureInContext(worker,NULL,&doc,1,"entry","entry",offset);CHECK(q);xls_signature_help* same=NULL;CHECK(waitSignature(q,&same,&stats,&thread)==XLS_QUERY_READY && same==first && stats.builds==1 && stats.hits==13);xlsSignatureRelease(same);xlsQueryDestroy(q);
 xstrbuf large={0};for(unsigned i=0;i<20000;++i)CHECK(xrtStrBufAppendFormat(&large,"func work_%u(int value) as int\n return value+1\nend\n",i));xls_text* largeText=xlsTextCreate(large.Data,large.Size,4u*1024u*1024u,NULL);char* largePath=xrtPathAbs("signature-actor-large.xl");CHECK(largeText && largePath);xls_analysis_document largeDoc={"large",largePath,largeText,44,1};xls_query* blocker=xlsWorkerSyntax(worker,&largeDoc,1,"large");CHECK(blocker);xls_worker_load load={0};uint64 until=xrtClock()+15000000;do{CHECK(xlsWorkerLoad(worker,&load));if(load.running)break;xrtSleepUs(500);}while(xrtClock()<until);CHECK(load.running);
 q=xlsWorkerSignatureInContext(worker,NULL,&doc,1,"entry","entry",offset);CHECK(q);xlsQueryCancel(q);xls_signature_help* cancelled=NULL;CHECK(waitSignature(q,&cancelled,NULL,NULL)==XLS_QUERY_CANCELLED && !cancelled);xlsQueryDestroy(q);xlsQueryCancel(blocker);xlsQueryDestroy(blocker);xlsTextRelease(largeText);xrtFree(largePath);xrtStrBufFree(&large);xlsTextRelease(text);xrtFree(path);xlsWorkerDestroy(worker);CHECK(xlsSignatureValidate(first) && xlsSignatureInfo(first).available);xlsSignatureRelease(first);return 1;
}
static int diskInputs(void)
{
 char relative[160];snprintf(relative,sizeof(relative),"build/signature-inputs-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());char* folder=xrtPathAbs(relative);char* entry=xrtPathJoin(folder,"main.xl");char* path=xrtPathJoin(folder,"helper.xl");char* project=xrtPathJoin(folder,"sample.xlp");CHECK(folder && entry && path && project && xrtDirCreateAll(folder));const char* entryBody="import helper\nfunc main(...) as int\n return helper.compute()\nend\n";CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)entryBody,strlen(entryBody)}));const char* body="func plus(int value) as int\n return value\nend\nexport func compute() as int\n return plus(7)\nend\n";xls_semantic_input input={path,body,strlen(body)};size_t offset=(size_t)(strstr(body,"plus(7)")-body)+strlen("plus(");xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.entry=entry,.directory=folder,.configuration="debug",.analysisProfile="fixture-linked-native",.targetKind="exe",.packagePath=""};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);CHECK(context);xls_signature_help* result=xlsSignatureCreateInContext(context,entry,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(result && xlsSignatureInfo(result).available && xlsSignatureValidate(result));xls_signature_item item;CHECK(xlsSignatureItem(result,0,&item) && !strcmp(item.label,"plus(int value) as int"));const char* held=item.label;xls_inputs inputs;CHECK(xlsSignatureInputs(result,&inputs) && inputs.sources>=2 && inputs.probes);xlsContextRelease(context);
 xls_signature_help* view=xlsSignatureReposition(result,offset+1);CHECK(view && view!=result && xlsSignatureSemantic(view)==xlsSignatureSemantic(result) && xlsSignatureInfo(view).offset==offset+1 && xlsSignatureInfo(result).offset==offset);
 const char* changed="// changed disk entry\nimport helper\nfunc main(...) as int\n return helper.compute()\nend\n";CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)changed,strlen(changed)}));CHECK(!xlsSignatureValidate(view) && !xlsSignatureInfo(view).available && !xlsSignatureItem(view,0,&item));CHECK(!xlsSignatureValidate(result) && !xlsSignatureInfo(result).available && !xlsSignatureInfo(result).count && !xlsSignatureItem(result,0,&item));CHECK(!strcmp(held,"plus(int value) as int"));CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)entryBody,strlen(entryBody)}) && !xlsSignatureValidate(result));xlsSignatureRelease(view);xlsSignatureRelease(result);xrtFree(project);xrtFree(path);xrtFree(entry);xrtFree(folder);return 1;
}
static int isolationAndBounds(void)
{
 const char* plain="func main(...) as int\n int value=7\n return val\nend\n";char* path=xrtPathAbs("signature-isolation.xl");CHECK(path);xls_semantic_input input={path,plain,strlen(plain)};size_t offset=(size_t)(strstr(plain,"return val")-plain)+strlen("return val");xls_signature_data data={.hasSite=1};xls_signature_scope outer;XL6_CompletionQuery bound={.path=path,.offset=offset};xlsSignatureScopeBegin(&outer,&data,NULL,NULL);outer.query=&bound;
 xls_semantic_completion* completion=xlsSemanticCompletionCreate(path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(completion && xlsSemanticCompletionInfo(completion).available && xlsSemanticCompletionInfo(completion).count==1);CHECK(xlsSignatureScopeCurrent(&bound)==&outer && !xlsSemanticSignatureData(completion));xlsSemanticCompletionDestroy(completion);
 const char* source="func apply(int value) as int\n return value\nend\nfunc main(...) as int\n return apply(7)\nend\n";input=(xls_semantic_input){path,source,strlen(source)};offset=(size_t)(strstr(source,"apply(7)")-source)+strlen("apply(");xls_signature_help* result=xlsSignatureCreateInContext(NULL,path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(result && xlsSignatureInfo(result).available && xlsSignatureScopeCurrent(&bound)==&outer);size_t bytes=77;CHECK(!xlsSemanticCompletionJson(xlsSignatureSemantic(result),&bytes) && !bytes && xlsSignatureInfo(result).available);xlsSignatureRelease(result);xlsSignatureScopeEnd(&outer);CHECK(!xlsSignatureScopeCurrent(&bound));
 CHECK(!xlsSignatureCreateInContext(NULL,path,path,strlen(source)+1,&input,1,NULL,NULL,NULL,0));xls_semantic_input duplicate[2]={input,input};CHECK(!xlsSignatureCreateInContext(NULL,path,path,offset,duplicate,2,NULL,NULL,NULL,0));
 xstrbuf big={0};CHECK(xrtStrBufAppend(&big,(xstrview){"func excessive(",15}));for(unsigned i=0;i<129;++i)CHECK(xrtStrBufAppendFormat(&big,"%sint p%u",i ? "," : "",i));CHECK(xrtStrBufAppendFormat(&big,") as int\n return p0\nend\nfunc main(...) as int\n return excessive(0)\nend\n"));input=(xls_semantic_input){path,big.Data,big.Size};offset=(size_t)(strstr(big.Data,"excessive(0)")-big.Data)+strlen("excessive(");result=xlsSignatureCreateInContext(NULL,path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(result && !xlsSignatureInfo(result).available && xlsSignatureInfo(result).error && strstr(xlsSignatureInfo(result).error,"parameter limit"));xlsSignatureRelease(result);xrtStrBufFree(&big);xrtFree(path);return 1;
}
static int run(void)
{
 CHECK(one(functions," return plus(@caret@)\nend\n","plus(int left, string right = ...) as int",0,1));
 CHECK(one(functions," return plus(7,@caret@)\nend\n","plus(int left, string right = ...) as int",1,1));
 CHECK(one(functions," return plus(7,@caret@\nend\n","plus(int left, string right = ...) as int",1,1));
 CHECK(one(functions," return plus(7,\"unfinished@caret@\nend\n","plus(int left, string right = ...) as int",1,1));
 CHECK(one(functions," return plus(right=@caret@)\nend\n","plus(int left, string right = ...) as int",1,1));
 CHECK(one(functions," return plus(plus(@caret@\nend\n","plus(int left, string right = ...) as int",0,1));
 CHECK(one(functions," int plus=7\n return plus(@caret@)\nend\n",NULL,0,0));
 CHECK(one(functions," return @caret@7\nend\n",NULL,0,0));
 CHECK(one("func identity<T>(T value) as T\n return value\nend\n"," return identity::<int>(@caret@)\nend\n","identity::<int>(int value) as int",0,1));
 CHECK(one("class Box\n func add(int value) as int\n return value\n end\nend\n"," auto box=Box()\n return box.add(@caret@)\nend\n","box.add(int value) as int",0,1));
 CHECK(one("class Box\n create(int value)\n end\nend\n"," auto box=Box(@caret@)\n return 0\nend\n","Box(int value)",0,1));
 CHECK(one("class Box\nend\n"," auto box=Box(@caret@)\n return 0\nend\n","Box()",SIZE_MAX,1));
 CHECK(one("func unary(int value) as int\n return value\nend\n"," auto callback=unary\n return callback(@caret@)\nend\n","callback(int) as int",0,1));
 CHECK(one(functions," return plus(right=\"x\",left=@caret@7)\nend\n","plus(int left, string right = ...) as int",0,1));
 CHECK(one("func choose(int value,int endValue) as int\n return value\nend\nfunc choose(string value,int endValue) as int\n return endValue\nend\n"," return choose(\"x\",@caret@0)\nend\n","choose(string value, int endValue) as int",1,1));
 CHECK(one("func choose(int value,int endValue) as int\n return value\nend\nfunc choose(string value,int endValue) as int\n return endValue\nend\n"," return choose(7,@caret@0)\nend\n","choose(int value, int endValue) as int",1,1));
 CHECK(one("class Box<T>\n func apply(T value) as T\n return value\n end\nend\n"," auto box=Box::<int>()\n return box.apply(@caret@)\nend\n","box.apply(int value) as int",0,1));
 CHECK(one("class Box\n func __call__(int value) as int\n return value\n end\nend\n"," auto box=Box()\n return box(@caret@)\nend\n","box(int value) as int",0,1));
 CHECK(one("class Base\n func method(int value) as int\n return value\n end\nend\nclass Derived extends Base\nend\n"," auto box=Derived()\n return box.method(@caret@)\nend\n","box.method(int value) as int",0,1));
 CHECK(one("func make() as func(int) as int\n return func(int value) as int\n return value\n end\nend\n"," return make()(@caret@)\nend\n","make()(int) as int",0,1));
 CHECK(one("func log(int tag,...) as int\n return tag\nend\n"," return log(7,1,2,@caret@)\nend\n","log(int tag, array<var> arg) as int",1,1));
 CHECK(one("func options(***) as int\n return 0\nend\n"," return options(first=1,second=@caret@2)\nend\n","options(dict<var> arg) as int",0,1));
 CHECK(one("func higher(func(int,string) as int callback,int count) as int\n return count\nend\n"," return higher(@caret@)\nend\n","higher(func(int,string) as int callback, int count) as int",0,1));
 CHECK(one("\xef\xbb\xbf""func 中文(int 数字,string 文本=\"😀\") as int\r\n return 数字\r\nend\r\n"," return 中文(7,文本=@caret@\"x\")\r\nend\r\n","中文(int 数字, string 文本 = ...) as int",1,1));
 CHECK(one("class Box\n private func hidden(int value) as int\n return value\n end\nend\n"," auto box=Box()\n return box.hidden(@caret@)\nend\n",NULL,0,0));
 CHECK(one(functions," return plus( // @caret@comment\n )\nend\n",NULL,0,0));
 CHECK(one(functions," return plus(7,f\"text {unknown(@caret@\nend\n",NULL,0,0));
 CHECK(originalOracle("func choose(int value,int other) as int\n return value\nend\nfunc choose(string value,int other) as int\n return other\nend\nfunc main(...) as int\n return choose(\"x\",@caret@0)\nend\n"));
 CHECK(originalOracle("class Box\n func method(int value) as int\n return value\n end\nend\nfunc main(...) as int\n auto box=Box()\n return box.method(@caret@7)\nend\n"));
 CHECK(originalOracle("class Box\n create(int value)\n end\nend\nfunc main(...) as int\n auto box=Box(@caret@7)\n return 0\nend\n"));
 CHECK(session());CHECK(parseIncrementality());CHECK(retainedFullAst());CHECK(cancellation());CHECK(actor());CHECK(diskInputs());CHECK(isolationAndBounds());
 return 1;
}
int main(void){if(!run())return 1;printf("Same-argument immutable caret views=%llu\n",(unsigned long long)caretViews);printf("analysis_signature_test passed: %u checks\n",checks);return 0;}
