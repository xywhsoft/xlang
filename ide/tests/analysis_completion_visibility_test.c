#include "analysis/semantic.h"
#include "analysis/completion_session.h"
#include "analysis/check.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
static unsigned mode;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_completion_visibility_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static const char* prefix=
 "extern \"native_text\" @no_raise func nativeText() as point\n"
 "extern \"native_free\" @no_raise func nativeFree(point value)\n"
 "func text() as cstring\n return cstring.take(nativeText(),nativeFree)\nend\n";
static size_t cursor(const char* source,const char* marker)
{
 const char* at=NULL;const char* next=source;
 while((next=strstr(next,marker))!=NULL){at=next;++next;}
 return at ? (size_t)(at-source)+strlen(marker) : SIZE_MAX;
}
static int facts(xls_semantic_completion* result,const char* source,const char* const* labels,const char* const* details,size_t count)
{
 CHECK(result);xls_completion_info info=xlsSemanticCompletionInfo(result);
 if(!info.available)fprintf(stderr,"completion unavailable: %s\n",info.error?info.error:"");
 CHECK(info.available && xlsSemanticCompletionValidate(result));
 for(size_t i=0;i<info.count;++i){xls_completion_item item;CHECK(xlsSemanticCompletionItem(result,i,&item));printf("%s: %s\n",item.label,item.detail);}
 CHECK(info.count==count);
 for(size_t i=0;i<count;++i){int found=0;for(size_t j=0;j<info.count;++j){xls_completion_item item;CHECK(xlsSemanticCompletionItem(result,j,&item));
   if(!strcmp(item.label,labels[i])){CHECK(!details || !strcmp(item.detail,details[i]));CHECK(item.hasSelection && item.symbol);
    CHECK(item.selectionEnd>=item.selectionStart && item.selectionEnd<=strlen(source) && item.selectionEnd-item.selectionStart==strlen(labels[i]));
    CHECK(!memcmp(source+item.selectionStart,labels[i],strlen(labels[i])));++found;}}
  CHECK(found==1);
 }
 return 1;
}
static int probe(const char* source,const char* marker,const char* const* labels,const char* const* details,size_t count)
{
 char* path=xrtPathAbs("completion-visibility.xl");CHECK(path);char error[256];
 xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);
 xls_analysis_document document={"entry",path,NULL,11,0};
 xls_semantic_completion* retained=NULL;char* retainedBody=NULL;
 for(unsigned generation=0;generation<4;++generation){
  xstrbuf body={0};
  if(generation)CHECK(xrtStrBufAppend(&body,xrtStrView("// shifted 中文 😀\n")));
  CHECK(xrtStrBufAppend(&body,xrtStrView(source)));
  if(generation>=2)CHECK(xrtStrBufAppendFormat(&body,"\nfunc xls_added() as int\n return %u\nend\n",generation==2 ? 17u : 18u));
  size_t at=cursor(body.Data,marker);CHECK(at!=SIZE_MAX);
  xlsTextRelease(document.text);document.text=xlsTextCreate(body.Data,body.Size,1024u*1024u,NULL);CHECK(document.text);++document.revision;
  xls_semantic_completion* actual=xlsCompletionSessionQuery(session,&document,1,"entry","entry",at,NULL,NULL,error,sizeof(error));
  if(!actual)fprintf(stderr,"completion session: %s\n",error);
  xls_semantic_input input={path,body.Data,body.Size};
  xls_semantic_completion* fresh=xlsSemanticCompletionCreate(path,path,at,&input,1,NULL,NULL,error,sizeof(error));
  CHECK(facts(actual,body.Data,labels,details,count) && facts(fresh,body.Data,labels,details,count));
  char *a=xlsSemanticCompletionJson(actual,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));
  xls_semantic_completion* hit=xlsCompletionSessionQuery(session,&document,1,"entry","entry",at,NULL,NULL,error,sizeof(error));CHECK(hit);
  char* c=xlsSemanticCompletionJson(hit,NULL);CHECK(c && !strcmp(a,c));
  xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticStringFree(c);xlsSemanticCompletionDestroy(hit);
  if(generation==3){retained=xlsSemanticCompletionRetain(actual);retainedBody=xrtStrDup(body.Data);CHECK(retained && retainedBody);}
  xlsSemanticCompletionDestroy(fresh);xlsSemanticCompletionDestroy(actual);xrtStrBufFree(&body);
 }
 xls_completion_stats stats;xlsCompletionSessionStats(session,&stats);CHECK(stats.hits>=4);
 xlsCompletionSessionDestroy(session);xlsTextRelease(document.text);
 CHECK(facts(retained,retainedBody,labels,details,count));
 xlsSemanticCompletionDestroy(retained);xrtFree(retainedBody);xrtFree(path);return 1;
}
static int rejectedInheritance(void)
{
 const char* invalid="class Base\nend\nclass Box<T> extends Base\n async func Echo(T value) as T\n return value\n end\nend\n";
 char* path=xrtPathAbs("completion-invalid-inheritance.xl");CHECK(path);
 xls_semantic_input input={path,invalid,strlen(invalid)};
 xls_semantic_check* check=xlsSemanticCheckCreate(path,&input,1,0,NULL,NULL,NULL,0);CHECK(check && !xlsSemanticCheckInfo(check).accepted);
 int found=0;for(size_t i=0;i<xlsSemanticCheckInfo(check).diagnostics;++i){xls_check_diagnostic diagnostic;CHECK(xlsSemanticCheckDiagnostic(check,i,&diagnostic));if(strstr(diagnostic.message,"泛型 class 当前不支持继承"))found=1;}
 CHECK(found);xlsSemanticCheckDestroy(check);
 xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,51,0};
 const char* valid="class Base\nend\nclass Box<T>\n async func Echo(T value) as T\n return value\n end\nend\n";
 const char* labels[]={"Echo"};const char* details[]={"func(string) as future<string>"};
 for(unsigned generation=0;generation<3;++generation){
  xstrbuf body={0};if(generation)CHECK(xrtStrBufAppend(&body,xrtStrView("// repaired 中文😀\n")));
  CHECK(xrtStrBufAppend(&body,xrtStrView(generation==1 ? invalid : valid)) && xrtStrBufAppend(&body,xrtStrView("func query(Box<string> box) as int\n return box.Ec\nend\n")));
  xlsTextRelease(doc.text);doc.text=xlsTextCreate(body.Data,body.Size,1024u*1024u,NULL);CHECK(doc.text);++doc.revision;size_t at=cursor(body.Data,"return box.Ec");CHECK(at!=SIZE_MAX);char error[256];
  xls_semantic_completion* actual=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",at,NULL,NULL,error,sizeof(error));
  input=(xls_semantic_input){path,body.Data,body.Size};xls_semantic_completion* fresh=xlsSemanticCompletionCreate(path,path,at,&input,1,NULL,NULL,error,sizeof(error));CHECK(actual && fresh);
  if(generation==1){CHECK(!xlsSemanticCompletionInfo(actual).available && !xlsSemanticCompletionInfo(actual).count && !xlsSemanticCompletionInfo(fresh).available);xls_completion_item item;CHECK(!xlsSemanticCompletionItem(actual,0,&item));}
  else CHECK(facts(actual,body.Data,labels,details,1) && facts(fresh,body.Data,labels,details,1));
  char *a=xlsSemanticCompletionJson(actual,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
  xlsSemanticCompletionDestroy(actual);xlsSemanticCompletionDestroy(fresh);xrtStrBufFree(&body);
 }
 xlsCompletionSessionDestroy(session);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
static int run(void)
{
 xstrbuf body={0};CHECK(xrtStrBufAppend(&body,xrtStrView(prefix)));
 CHECK(xrtStrBufAppend(&body,xrtStrView(
 "class Reader\n"
 " func Read<T>(T value) as cstring\n return text()\n end\n"
 " async func Factory<T>(T value) as func(T) as cstring\n return self.Read\n end\n"
 " func __xl_c_owned_async_user() as int\n return 7\n end\nend\n"
 "func bind(Reader reader) as func(c_int) as future<func(c_int) as string>\n return reader.Factory\nend\n"
 "func query(Reader reader) as int\n return reader.\nend\n")));
 const char* labels[]={"Read","Factory","__xl_c_owned_async_user"};
 const char* details[]={"func(T) as cstring","func(T) as future<func(T) as cstring>","func() as int"};
 if(!mode || mode==1)CHECK(probe(body.Data,"return reader.",labels,details,3));
 xrtStrBufFree(&body);
 const char* global=
 "async func factory<T>(T witness) as func() as cstring\n return text\nend\n"
 "func bind<U>(U witness) as func(U) as future<func() as string>\n return factory::<U>\nend\n"
 "func __xl_c_owned_async_user() as int\n return 7\nend\n";
 CHECK(xrtStrBufAppend(&body,xrtStrView(prefix)) && xrtStrBufAppend(&body,xrtStrView(global)) &&
  xrtStrBufAppend(&body,xrtStrView("func query() as int\n return __xl_c_owned_async_\nend\n")));
 const char* one[]={"__xl_c_owned_async_user"};const char* oneDetails[]={"func() as int"};
 if(!mode || mode==2)CHECK(probe(body.Data,"return __xl_c_owned_async_",one,oneDetails,1));
 xrtStrBufFree(&body);
 CHECK(xrtStrBufAppend(&body,xrtStrView(prefix)) && xrtStrBufAppend(&body,xrtStrView(global)) &&
  xrtStrBufAppend(&body,xrtStrView("func query() as int\n return fact\nend\n")));
 const char* factory[]={"factory"};const char* factoryDetails[]={"func(T) as future<func() as cstring>"};
 if(!mode || mode==3)CHECK(probe(body.Data,"return fact",factory,factoryDetails,1));
 xrtStrBufFree(&body);
 const struct extra {const char* source;const char* marker;const char* label;const char* detail;} extra[]={
  {"async func empty()\nend\nfunc query()\n return emp\nend\n","return emp","empty","func() as future<null>"},
  {"class Base\n static async func Static<T>(T value) as T\n return value\n end\nend\nclass Child extends Base\nend\nfunc query()\n return Child.Sta\nend\n","return Child.Sta","Static","func(T) as future<T>"},
  {"class Base\n async func Read() as int\n return 7\n end\nend\nclass Child extends Base\nend\nfunc query(Child child)\n return child.Rea\nend\n","return child.Rea","Read","func() as future<int>"},
  {"class Box<T>\n async func Echo(T value) as T\n return value\n end\nend\nfunc query(Box<string> box)\n return box.Ec\nend\n","return box.Ec","Echo","func(string) as future<string>"},
  {"func values() as generator<int>\n yield 7\nend\nfunc query()\n return val\nend\n","return val","values","func() as generator<int>"},
  {"async func stream() as generator<int>\n yield 7\nend\nfunc query()\n return stre\nend\n","return stre","stream","func() as generator<int>"},
 };
 if(!mode || mode==4)for(size_t i=0;i<sizeof(extra)/sizeof(*extra);++i){
  const char* wanted[]={extra[i].label};const char* detail[]={extra[i].detail};
  CHECK(probe(extra[i].source,extra[i].marker,wanted,detail,1));
 }
 if(!mode || mode==4)CHECK(rejectedInheritance());
 return 1;
}
int main(int argc,char** argv)
{
 if(argc>1){if(!strcmp(argv[1],"--member"))mode=1;else if(!strcmp(argv[1],"--global"))mode=2;else if(!strcmp(argv[1],"--signature"))mode=3;else if(!strcmp(argv[1],"--extra"))mode=4;else return 2;}
 if(!run())return 1;
 printf("analysis_completion_visibility_test passed: %u checks\n",checks);return 0;
}
