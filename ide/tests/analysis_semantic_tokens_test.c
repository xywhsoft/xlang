#include "analysis/semantic_tokens.h"
#include "analysis/check_session.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_semantic_tokens_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static xls_semantic_check* create(const char* path,const char* source,const char* dependency)
{
 char* dep=dependency?xrtPathAbs("dep.xl"):NULL;xls_semantic_input inputs[]={{path,source,strlen(source)},{dep,dependency,dependency?strlen(dependency):0}};
 xls_semantic_check* c=xlsSemanticCheckCreate(path,inputs,dependency?2:1,1,NULL,NULL,NULL,0);xrtFree(dep);
 if(!c || !xlsSemanticCheckInfo(c).accepted){char* raw=c?xlsSemanticCheckJson(c,NULL):NULL;fprintf(stderr,"rejected:%s\n",raw?raw:"");xrtFree(raw);}return c;
}
static int rangeEqual(xls_index_range a,xls_index_range b)
{return a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn && a.known==b.known;}
static int parity(xls_semantic_tokens* a,xls_semantic_tokens* b)
{
 CHECK(a && b && xlsSemanticTokensCount(a)==xlsSemanticTokensCount(b));const xls_semantic_token* x=xlsSemanticTokensData(a),*y=xlsSemanticTokensData(b);
 for(size_t i=0;i<xlsSemanticTokensCount(a);++i)CHECK(x[i].kind==y[i].kind && x[i].modifiers==y[i].modifiers && rangeEqual(x[i].range,y[i].range));
 return 1;
}
static int expect(const xls_semantic_tokens* tokens,const char* source,const char* marker,const char* spelling,unsigned kind,unsigned modifiers)
{
 const char* at=strstr(source,marker);CHECK(at);at=strstr(at,spelling);CHECK(at);size_t offset=at-source,size=strlen(spelling);const xls_semantic_token* rows=xlsSemanticTokensData(tokens);
 for(size_t i=0;i<xlsSemanticTokensCount(tokens);++i)if(rows[i].range.startOffset==offset){CHECK(rows[i].range.endOffset==offset+size && rows[i].kind==kind && rows[i].modifiers==modifiers);return 1;}
 fprintf(stderr,"missing expected token:%s/%s kind%u at%zu\n",marker,spelling,kind,offset);return 0;
}
static int validate(const xls_semantic_tokens* t,const char* text)
{
 CHECK(t);size_t size=strlen(text),last=0;const xls_semantic_token* data=xlsSemanticTokensData(t);
 for(size_t i=0;i<xlsSemanticTokensCount(t);++i){const xls_semantic_token* row=data+i;CHECK(row->range.known && row->range.startOffset>=last && row->range.startOffset<row->range.endOffset && row->range.endOffset<=size && row->range.startLine==row->range.endLine && row->range.startColumn<row->range.endColumn && row->kind>0 && row->kind<XLS_TOKEN_KIND_COUNT && !(row->modifiers&~7u));
  CHECK(!(row->range.endOffset-row->range.startOffset==3 && !memcmp(text+row->range.startOffset,"...",3)));last=row->range.endOffset;
 }
 xls_semantic_tokens_work work=xlsSemanticTokensWork(t);CHECK(work.atomBytes<=size && work.bytes<=work.peakBytes && work.peakBytes<=16u*1024u*1024u);return 1;
}
static int examples(void)
{
 const char* text="typedef Count = int\r\nclass Box<T>\r\n T field\r\nend\r\nfunc choose(int value) as int\r\n return value\r\nend\r\nfunc choose(string value) as int\r\n return 1\r\nend\r\nfunc helper(int 值) as int\r\n Count local=choose(值)\r\n string text=\"😀\"\r\n if true\r\n int local=2\r\n return local\r\n end\r\n return local+choose(text)\r\nend\r\nfunc main(...) as int\r\n return helper(2)\r\nend\r\n";
 char* path=xrtPathAbs("semantic-中文😀.xl");CHECK(path);xls_semantic_check* c=create(path,text,NULL);CHECK(c && xlsSemanticCheckInfo(c).accepted);enum xls_semantic_tokens_error error;
 xls_semantic_tokens* t=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(t && !error && validate(t,text));CHECK(expect(t,text,"Count =","Count",XLS_TOKEN_TYPE,1) && expect(t,text,"Box<T>","Box",XLS_TOKEN_CLASS,1) && expect(t,text,"<T>","T",XLS_TOKEN_TYPE_PARAMETER,1) && expect(t,text," T field","T",XLS_TOKEN_TYPE_PARAMETER,0) && expect(t,text," T field","field",XLS_TOKEN_PROPERTY,1));
 CHECK(expect(t,text,"helper(int","值",XLS_TOKEN_PARAMETER,1) && expect(t,text,"choose(值)","choose",XLS_TOKEN_FUNCTION,0) && expect(t,text,"choose(值)","值",XLS_TOKEN_PARAMETER,0) && expect(t,text,"local=2","local",XLS_TOKEN_VARIABLE,1) && expect(t,text,"return local","local",XLS_TOKEN_VARIABLE,0));
 size_t bytes=xlsSemanticTokensCount(t)*sizeof(xls_semantic_token);void* copy=xrtMalloc(bytes);CHECK(copy);memcpy(copy,xlsSemanticTokensData(t),bytes);xlsSemanticCheckDestroy(c);CHECK(xlsSemanticTokensRetain(t)==t);xlsSemanticTokensRelease(t);CHECK(!memcmp(copy,xlsSemanticTokensData(t),bytes));xrtFree(copy);xlsSemanticTokensRelease(t);
 const char* methods="class Box\r\n int value\r\n func answer() as int\r\n return self.value\r\n end\r\nend\r\nfunc main(...) as int\r\n string 结果 = \"😀\"\r\n auto box = Box()\r\n return box.answer()\r\nend\r\n";
 c=create(path,methods,NULL);CHECK(c && xlsSemanticCheckInfo(c).accepted);t=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(t && validate(t,methods));CHECK(expect(t,methods,"func answer","answer",XLS_TOKEN_METHOD,1) && expect(t,methods,"self.value","value",XLS_TOKEN_PROPERTY,0) && expect(t,methods,"box.answer","answer",XLS_TOKEN_METHOD,0));xlsSemanticTokensRelease(t);xlsSemanticCheckDestroy(c);
 const char* constants="const int LIMIT = 7\nasync func work() as int\n return LIMIT\nend\nfunc main(...) as int\n return LIMIT\nend\n";
 c=create(path,constants,NULL);CHECK(c && xlsSemanticCheckInfo(c).accepted);t=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(t && validate(t,constants));CHECK(expect(t,constants,"const int LIMIT","LIMIT",XLS_TOKEN_VARIABLE,3) && expect(t,constants,"return LIMIT","LIMIT",XLS_TOKEN_VARIABLE,2) && expect(t,constants,"func work","work",XLS_TOKEN_FUNCTION,5));xlsSemanticTokensRelease(t);xlsSemanticCheckDestroy(c);xrtFree(path);return 1;
}
static int imports(void)
{
 char* path=xrtPathAbs("token-main.xl");const char* code="import dep\nfunc main(...) as int\n return dep.compute(2)\nend\n";const char* dep="export func compute(int value) as int\n return value+1\nend\n";xls_semantic_check* c=create(path,code,dep);CHECK(c && xlsSemanticCheckInfo(c).accepted);xls_inputs inputs;CHECK(xlsSemanticCheckInputs(c,&inputs));size_t covered=0;
 for(size_t i=0;i<inputs.sources;++i){xls_input_source row;CHECK(xlsSemanticCheckSource(c,i,&row));if(!(row.kind&1))continue;enum xls_semantic_tokens_error e;xls_semantic_tokens* t=xlsSemanticTokensCreate(c,i,0,NULL,NULL,&e);CHECK(t && !e && validate(t,row.text));if(!strcmp(row.path,path)){CHECK(expect(t,row.text,"dep.compute","compute",XLS_TOKEN_FUNCTION,0));++covered;}else if(strstr(row.path,"dep.xl")){CHECK(expect(t,row.text,"func compute","compute",XLS_TOKEN_FUNCTION,1));++covered;}xlsSemanticTokensRelease(t);}
 CHECK(covered==2);xlsSemanticCheckDestroy(c);xrtFree(path);return 1;
}
static int incremental(void)
{
 char* path=xrtPathAbs("token-incremental.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
 for(unsigned version=1;version<=24;++version){xstrbuf code={0};CHECK(xrtStrBufAppend(&code,xrtStrView(version%3==0?"\xef\xbb\xbf\n":"")));CHECK(xrtStrBufAppend(&code,xrtStrView("func helper(int 值) as int\r\n int total=值\r\n")));for(unsigned j=0;j<16;++j)CHECK(xrtStrBufAppend(&code,xrtStrView(" total=total+1\r\n")));CHECK(xrtStrBufAppendFormat(&code," return total\r\nend\r\nfunc main(...) as int\r\n%s return helper(%u)\r\nend\r\n",version%2 ? " int extra=1\r\n" : "",version));xls_text* text=xlsTextCreate(code.Data,code.Size,8u*1024u*1024u,NULL);CHECK(text);xls_analysis_document document={"entry",path,text,9171,version};xls_semantic_check* c=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,NULL,0),*fresh=create(path,code.Data,NULL);xlsTextRelease(text);CHECK(c && fresh && xlsSemanticCheckInfo(c).accepted && xlsSemanticCheckInfo(fresh).accepted);enum xls_semantic_tokens_error error;xls_semantic_tokens *a=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error),*b=xlsSemanticTokensCreate(fresh,0,0,NULL,NULL,&error);CHECK(parity(a,b));xlsSemanticTokensRelease(a);xlsSemanticTokensRelease(b);xlsSemanticCheckDestroy(c);xlsSemanticCheckDestroy(fresh);xrtStrBufFree(&code);}
 xlsCheckSessionDestroy(session);xrtFree(path);return 1;
}
static int stop(void* user){size_t* n=user;return ++*n>=3;}
static int failures(void)
{
 char* path=xrtPathAbs("token-failures.xl");const char* code="func main(...) as int\n int value=7\n return value\nend\n";xls_semantic_check* c=create(path,code,NULL);CHECK(c && xlsSemanticCheckInfo(c).accepted);enum xls_semantic_tokens_error error;size_t calls=0;CHECK(!xlsSemanticTokensCreate(c,0,0,stop,&calls,&error) && error==XLS_TOKENS_CANCELLED && calls==3);CHECK(!xlsSemanticTokensCreate(c,0,1,NULL,NULL,&error) && error==XLS_TOKENS_ARGUMENT);CHECK(!xlsSemanticTokensCreate(c,0,128,NULL,NULL,&error) && error==XLS_TOKENS_LIMIT);CHECK(!xlsSemanticTokensCreate(c,SIZE_MAX,0,NULL,NULL,&error) && error==XLS_TOKENS_ARGUMENT);xls_semantic_tokens* t=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(t && !error);xlsSemanticTokensRelease(t);xlsSemanticCheckDestroy(c);
 CHECK(!xlsSemanticTokensCreate(NULL,0,0,NULL,NULL,&error));CHECK(!xlsSemanticTokensCount(NULL) && !xlsSemanticTokensData(NULL));xlsSemanticTokensRelease(NULL);xrtFree(path);return 1;
}

typedef struct token_reader {xls_semantic_check* check;xls_semantic_tokens* expected;int okay;} token_reader;
static int32 readTokens(void* user)
{
 token_reader* r=user;r->okay=1;
 for(unsigned i=0;i<32;++i){enum xls_semantic_tokens_error error;xls_semantic_tokens* t=xlsSemanticTokensCreate(r->check,0,0,NULL,NULL,&error);
  if(!t || error || xlsSemanticTokensCount(t)!=xlsSemanticTokensCount(r->expected))r->okay=0;
  const xls_semantic_token *a=xlsSemanticTokensData(t),*b=xlsSemanticTokensData(r->expected);
  if(t && r->okay)for(size_t j=0;j<xlsSemanticTokensCount(t);++j)if(a[j].kind!=b[j].kind || a[j].modifiers!=b[j].modifiers || !rangeEqual(a[j].range,b[j].range))r->okay=0;
  xlsSemanticTokensRelease(t);
 }return 0;
}
static int longUnicode(void)
{
 char* path=xrtPathAbs("token-long-unicode.xl");xstrbuf code={0};CHECK(path && xrtStrBufAppend(&code,xrtStrView("func main(...) as int\r\n string text=\"")));
 for(unsigned i=0;i<4096;++i)CHECK(xrtStrBufAppend(&code,xrtStrView("😀")));
 CHECK(xrtStrBufAppend(&code,xrtStrView("\"; int 变量=1;")));
 for(unsigned i=0;i<800;++i)CHECK(xrtStrBufAppend(&code,xrtStrView("变量=变量+1;")));
 CHECK(xrtStrBufAppend(&code,xrtStrView("return 变量\r\nend\r\n")));
 xls_semantic_check* c=create(path,code.Data,NULL);CHECK(c && xlsSemanticCheckInfo(c).accepted);enum xls_semantic_tokens_error error;xls_semantic_tokens* t=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(t && !error && validate(t,code.Data) && xlsSemanticTokensCount(t)>1600);
 xls_text* text=xlsTextCreate(code.Data,code.Size,8u*1024u*1024u,NULL);CHECK(text);
 const xls_semantic_token* rows=xlsSemanticTokensData(t);size_t bytes=0;
 for(size_t i=0;i<xlsSemanticTokensCount(t);++i){xls_position a,b;CHECK(xlsTextPosition(text,rows[i].range.startOffset,&a) && xlsTextPosition(text,rows[i].range.endOffset,&b));CHECK(a.line==b.line && a.line+1==(size_t)rows[i].range.startLine && b.character>a.character);bytes+=rows[i].range.endOffset-rows[i].range.startOffset;}
 CHECK(bytes<=xlsSemanticTokensWork(t).atomBytes && xlsSemanticTokensWork(t).atomBytes<=code.Size);
 token_reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(token_reader){c,t,0};threads[i]=xrtThreadCreate(readTokens,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 xlsTextRelease(text);xlsSemanticTokensRelease(t);xlsSemanticCheckDestroy(c);xrtStrBufFree(&code);xrtFree(path);return 1;
}
static int rejectedAndInvalidated(void)
{
 char* path=xrtPathAbs("token-invalidated.xl");char* dep=xrtPathAbs("token_live_dep.xl");CHECK(path && dep);
 const char* code="import token_live_dep\nfunc main(...) as int\n return token_live_dep.value()\nend\n";
 const char* original="export func value() as int\n return 1\nend\n";
 CHECK(!xrtFileExists(dep) && xrtFileWriteText(dep,xrtStrView(original),XENCODING_UTF8,XUTF_STRICT,false));
 xls_semantic_check* c=create(path,code,NULL);CHECK(c && xlsSemanticCheckInfo(c).accepted);
 enum xls_semantic_tokens_error error;xls_semantic_tokens* t=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error);CHECK(t && !error);
 size_t count=xlsSemanticTokensCount(t),size=count*sizeof(xls_semantic_token);void* copy=xrtMalloc(size);CHECK(copy);memcpy(copy,xlsSemanticTokensData(t),size);
 CHECK(xrtFileWriteText(dep,xrtStrView("export func value() as int\n return 22\nend\n"),XENCODING_UTF8,XUTF_STRICT,false) && !xlsSemanticCheckValidate(c));
 CHECK(!xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error) && error==XLS_TOKENS_NO_INDEX);
 xlsSemanticCheckDestroy(c);CHECK(xlsSemanticTokensCount(t)==count && !memcmp(copy,xlsSemanticTokensData(t),size));xrtFree(copy);xlsSemanticTokensRelease(t);CHECK(xrtFileDelete(dep));
 const char* bad="func main(...) as int\n return missing\nend\n";xls_semantic_input input={path,bad,strlen(bad)};
 c=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);CHECK(c && !xlsSemanticCheckInfo(c).accepted);
 CHECK(!xlsSemanticTokensCreate(c,0,0,NULL,NULL,&error) && error==XLS_TOKENS_NO_INDEX);xlsSemanticCheckDestroy(c);
 CHECK(!xlsSemanticTokensRetain(NULL));xrtFree(dep);xrtFree(path);return 1;
}
int main(void){if(!examples() || !imports() || !incremental() || !failures() || !longUnicode() || !rejectedAndInvalidated())return 1;printf("analysis_semantic_tokens_test passed: %u checks; checked roles,bindings,methods,fields,imports,Unicode,rebasing,fresh parity,lifetime,cancellation,invalidation and four concurrent readers\n",checks);return 0;}
