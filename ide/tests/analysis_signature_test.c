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
 CHECK(full && xlsSemanticCheckInfo(full).accepted);const XL6_AstNode* call=findCall(xlsSemanticCheckAst(full),xlsSemanticCompletionInfo(result).start,path);CHECK(call && call->semanticTargetSymbolId && item.symbol==call->semanticTargetSymbolId);xlsSemanticCheckDestroy(full);xlsSignatureRelease(result);xrtFree(path);xrtFree(source);return 1;
}
static int sameText(const char* a,const char* b){return a && b ? !strcmp(a,b) : a==b;}
static int parity(xls_signature_help* a,xls_signature_help* b)
{
 xls_signature_info x=xlsSignatureInfo(a),y=xlsSignatureInfo(b);CHECK(x.available==y.available && x.count==y.count && x.activeSignature==y.activeSignature && x.argument==y.argument && x.projected==y.projected);
 for(size_t i=0;i<x.count;++i){xls_signature_item left,right;CHECK(xlsSignatureItem(a,i,&left) && xlsSignatureItem(b,i,&right));CHECK(sameText(left.label,right.label) && sameText(left.documentation,right.documentation) && left.parameters==right.parameters && left.activeParameter==right.activeParameter && left.symbol==right.symbol);
  for(size_t j=0;j<left.parameters;++j){xls_signature_parameter p,q;CHECK(xlsSignatureParameter(a,i,j,&p) && xlsSignatureParameter(b,i,j,&q));CHECK(sameText(p.name,q.name) && p.labelStart==q.labelStart && p.labelEnd==q.labelEnd && p.optional==q.optional && p.variadic==q.variadic);}
 }
 return 1;
}
static int session(void)
{
 char* path=xrtPathAbs("signature-session.xl");CHECK(path);xls_signature_session* s=xlsSignatureSessionCreate(NULL);CHECK(s);xls_analysis_document doc={"entry",path,NULL,23,1};xls_signature_help* held=NULL;
 for(unsigned i=0;i<16;++i){xstrbuf source={0};for(unsigned j=0;j<24;++j)CHECK(xrtStrBufAppendFormat(&source,"func stable_%u(int value) as int\n int local=value+%u\n if local>0 then\n return local\n end\n return 0\nend\n",j,j));CHECK(xrtStrBufAppendFormat(&source,"%sfunc main(...) as int\n return plus(%u,\"%u\")\nend\n",functions,i,i));xlsTextRelease(doc.text);doc.text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);CHECK(doc.text);++doc.revision;size_t offset=(size_t)(strrchr(source.Data,'"')-source.Data);xls_signature_help* r=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(r && xlsSignatureInfo(r).available);xls_semantic_input in={path,source.Data,source.Size};xls_signature_help* fresh=xlsSignatureCreateInContext(NULL,path,path,offset,&in,1,NULL,NULL,NULL,0);CHECK(fresh && parity(r,fresh));xlsSignatureRelease(fresh);
  if(i)CHECK(xlsSemanticCompletionSemWork(r).reused>=24);
  xls_signature_stats stats;xlsSignatureSessionStats(s,&stats);CHECK(stats.builds==i+1 && stats.indexBuilds==i+1 && stats.hits==i);
  ++doc.revision;xls_signature_help* hit=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(hit==r);xlsSignatureRelease(hit);
  if(!i)held=xlsSignatureRetain(r);
  xlsSignatureRelease(r);xrtStrBufFree(&source);
 }
 size_t offset=(size_t)(strrchr(xlsTextData(doc.text),'"')-xlsTextData(doc.text))-1;
 xls_signature_help* moved=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(moved && xlsSignatureInfo(moved).available);xls_signature_stats stats;xlsSignatureSessionStats(s,&stats);CHECK(stats.indexHits==1 && stats.builds==17 && stats.indexBuilds==16);xlsSignatureRelease(moved);
 ++doc.identity;doc.revision=1;moved=xlsSignatureSessionQueryInContext(s,NULL,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(moved && !xlsSemanticCompletionSemWork(moved).reused);xlsSignatureRelease(moved);xlsSignatureSessionDestroy(s);xlsTextRelease(doc.text);CHECK(held && xlsSignatureValidate(held) && xlsSignatureInfo(held).available);xls_signature_item item;CHECK(xlsSignatureItem(held,0,&item) && strstr(item.label,"plus("));xlsSignatureRelease(held);xrtFree(path);return 1;
}
typedef struct cancel_state {unsigned calls,at;} cancel_state;
static int cancelAt(void* user){cancel_state* c=user;return ++c->calls==c->at;}
static int cancellation(void)
{
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
 const char* changed="// changed disk entry\nimport helper\nfunc main(...) as int\n return helper.compute()\nend\n";CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)changed,strlen(changed)}));CHECK(!xlsSignatureValidate(result) && !xlsSignatureInfo(result).available && !xlsSignatureInfo(result).count && !xlsSignatureItem(result,0,&item));CHECK(!strcmp(held,"plus(int value) as int"));CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)entryBody,strlen(entryBody)}) && !xlsSignatureValidate(result));xlsSignatureRelease(result);xrtFree(project);xrtFree(path);xrtFree(entry);xrtFree(folder);return 1;
}
static int isolationAndBounds(void)
{
 const char* plain="func main(...) as int\n int value=7\n return val\nend\n";char* path=xrtPathAbs("signature-isolation.xl");CHECK(path);xls_semantic_input input={path,plain,strlen(plain)};size_t offset=(size_t)(strstr(plain,"return val")-plain)+strlen("return val");xls_signature_data data={.hasSite=1};xls_signature_scope outer;XL6_CompletionQuery bound={.path=path,.offset=offset};xlsSignatureScopeBegin(&outer,&data,NULL,NULL);outer.query=&bound;
 xls_semantic_completion* completion=xlsSemanticCompletionCreate(path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(completion && xlsSemanticCompletionInfo(completion).available && xlsSemanticCompletionInfo(completion).count==1);CHECK(xlsSignatureScopeCurrent(&bound)==&outer && !xlsSignatureInfo(completion).available && !xlsSignatureValidate(completion));xlsSemanticCompletionDestroy(completion);
 const char* source="func apply(int value) as int\n return value\nend\nfunc main(...) as int\n return apply(7)\nend\n";input=(xls_semantic_input){path,source,strlen(source)};offset=(size_t)(strstr(source,"apply(7)")-source)+strlen("apply(");xls_signature_help* result=xlsSignatureCreateInContext(NULL,path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(result && xlsSignatureInfo(result).available && xlsSignatureScopeCurrent(&bound)==&outer);size_t bytes=77;CHECK(!xlsSemanticCompletionJson(result,&bytes) && !bytes && xlsSignatureInfo(result).available);xlsSignatureRelease(result);xlsSignatureScopeEnd(&outer);CHECK(!xlsSignatureScopeCurrent(&bound));
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
 CHECK(session());CHECK(cancellation());CHECK(actor());CHECK(diskInputs());CHECK(isolationAndBounds());
 return 1;
}
int main(void){if(!run())return 1;printf("analysis_signature_test passed: %u checks\n",checks);return 0;}
