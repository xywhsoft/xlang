/* The reference API/Lemon objects are the unmodified original source, built
 * with renamed public exports. Only this fixture links them into the process. */
#include "analysis_ast_same.h"
#include "analysis/signature_internal.h"
#include "analysis/frontend_completion_path.h"
#include <stdio.h>

int xlsReferenceParseSource(const char*,const char*,XL6_ParseResult*);
int xlsReferenceParseSourceWithCompletion(const char*,const char*,XL6_ParseResult*,XL6_CompletionQuery*);
static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_parser_profile_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)

static int equivalent(const char* source,const char* path,const char* marker)
{
    XL6_ParseResult original,adapted;xl6ParseResultInit(&original);xl6ParseResultInit(&adapted);
    XL6_CompletionQuery a={.path=path},b={.path=path};int left,right;
    if(marker){
        const char* at=strstr(source,marker);CHECK(at);
        size_t bom=!memcmp(source,"\xef\xbb\xbf",3) ? 3 : 0;
        a.offset=b.offset=(size_t)(at-source)+strlen(marker)-bom;
        left=xlsReferenceParseSourceWithCompletion(source,path,&original,&a);
        right=xl6ParseSourceWithCompletion(source,path,&adapted,&b);
        CHECK(a.parsed==1 && b.parsed==1 && sameText(a.prefix,b.prefix) && sameText(a.error,b.error));
        CHECK(sameRange(a.replacement,b.replacement));
    }else{
        left=xlsReferenceParseSource(source,path,&original);
        right=xl6ParseSource(source,path,&adapted);
    }
    if(!left || !right || original.diags.count || adapted.diags.count){
        fprintf(stderr,"source rejected: %s",source);
        for(size_t i=0;i<adapted.diags.count;++i)fprintf(stderr,"%d:%d %s\n",adapted.diags.items[i].line,adapted.diags.items[i].column,adapted.diags.items[i].message);
    }
    CHECK(left && right && !original.diags.count && !adapted.diags.count);
    CHECK(sameAst(original.root,adapted.root));
    xl6CompletionFree(&a);xl6CompletionFree(&b);xl6ParseResultFree(&original);xl6ParseResultFree(&adapted);return 1;
}

static xls_syntax* cursor(const char* source,const char* path,const char* marker,
                          xls_syntax* previous,xls_parse_work* work)
{
    xls_text* text=xlsTextCreate(source,strlen(source),1024*1024,NULL);if(!text)return NULL;
    const char* at=strstr(source,marker);if(!at){xlsTextRelease(text);return NULL;}
    XL6_CompletionQuery q={.path=path,.offset=(size_t)(at-source)+strlen(marker)};
    xls_syntax* syntax=xlsSyntaxBuildCursor(path,text,previous,&q,work);
    xl6CompletionFree(&q);xlsTextRelease(text);return syntax;
}

static int chunks(const char* header,const char* path)
{
    xstrbuf before={0},after={0};
    CHECK(xrtStrBufAppendFormat(&before,"%s\nfunc edited() as int\n int result = 7\n return res\nend\nfunc tail()\nend\n",header));
    CHECK(xrtStrBufAppendFormat(&after,"%s\nfunc edited() as int\n int result = 777\n return result\nend\nfunc tail()\nend\n",header));
    xls_parse_work initial={0},work={0};xls_syntax* a=cursor(before.Data,path,"return res",NULL,&initial);CHECK(a && xlsSyntaxAccepted(a));
    xls_syntax* b=cursor(after.Data,path,"return result",a,&work);CHECK(b && xlsSyntaxAccepted(b));
    CHECK(work.full==0 && work.fragments==1 && work.reused>=1 && work.bytes<after.Size);
    XL6_ParseResult original,copy;xl6ParseResultInit(&original);xl6ParseResultInit(&copy);
    XL6_CompletionQuery q={.path=path,.offset=(size_t)(strstr(after.Data,"return result")-after.Data)+strlen("return result")};
    CHECK(xlsReferenceParseSourceWithCompletion(after.Data,path,&original,&q));
    CHECK(xlsSyntaxCopyParse(b,&copy) && sameAst(original.root,copy.root));
    xl6CompletionFree(&q);xl6ParseResultFree(&copy);xl6ParseResultFree(&original);
    xlsSyntaxRelease(a);xlsSyntaxRelease(b);xrtStrBufFree(&before);xrtStrBufFree(&after);return 1;
}

static int signature(const char* marked,const char* path,int closed,int invalidPath)
{
    const char* at=strstr(marked,"@caret@");CHECK(at);size_t offset=(size_t)(at-marked);
    xstrbuf source={0};CHECK(xrtStrBufAppend(&source,(xstrview){marked,offset}) && xrtStrBufAppend(&source,xrtStrView(at+7)));
    xls_text* text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);CHECK(text);
    xls_call_context* index=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);CHECK(index);
    xls_call_site site;CHECK(xlsCallContextLookup(index,offset,&site)==1 && site.closed==closed && site.argumentKnown);
    XL6_CompletionQuery q={.path=path,.offset=offset};
    xls_signature_data data={.index=index,.source=source.Data,.size=source.Size,.offset=offset,
        .path=(char*)(invalidPath ? "invalid-\xff.xl" : path),.site=site,.hasSite=1};
    xls_signature_scope scope;xlsSignatureScopeBegin(&scope,&data,NULL,NULL);scope.query=&q;
    XL6_ParseResult result;xl6ParseResultInit(&result);
    int accepted=xl6ParseSourceWithCompletion(source.Data,path,&result,&q);xlsSignatureScopeEnd(&scope);
    if(invalidPath)CHECK(!accepted && q.error && !data.projected);
    else CHECK(accepted && !result.diags.count && data.projected && q.prefix && !*q.prefix && q.replacement.known);
    xl6ParseResultFree(&result);xl6CompletionFree(&q);xlsCallContextRelease(index);xlsTextRelease(text);xrtStrBufFree(&source);return 1;
}

static int contexts(void)
{
    const char* examples[]={"outer(inner::<func(int?) as int,int>(1),2)",
        "outer(inner::<func(c_vararg) as int,int>(1),2)",
        "outer(inner::<func(dict<string,list<int>>?) as int,int>(1),2)"};
    for(size_t i=0;i<sizeof(examples)/sizeof(*examples);++i){
        xls_text* text=xlsTextCreate(examples[i],strlen(examples[i]),1024,NULL);CHECK(text);
        xls_call_context* c=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);CHECK(c);
        xls_call_site site;size_t offset=strlen(examples[i])-2;
        CHECK(xlsCallContextLookup(c,offset,&site)==1 && site.argumentKnown && site.argument==1);
        CHECK(xlsCallContextWork(c).genericProofs==1);xlsCallContextRelease(c);xlsTextRelease(text);
    }
    return 1;
}

static int run(void)
{
    char* path=xrtPathAbs("native-parser-中文😀.xl");CHECK(path);
    XL6_CompletionQuery probe={.path=path},unchanged=probe;
    CHECK(xlsCompletionTargetConst(&probe,path) && !memcmp(&probe,&unchanged,sizeof(probe)));
    CHECK(!xlsCompletionTargetConst(&probe,"invalid-\xff.xl") && !memcmp(&probe,&unchanged,sizeof(probe)));
    const char* common[]={"func empty()\nend\n", "func typed<T>(T value) as T\n return value\nend\n",
        "class Box\n int value\nend\n", "func lambda()\n auto f = func(int value) as int\n return value\n end\nend\n"};
    for(size_t i=0;i<sizeof(common)/sizeof(*common);++i)CHECK(equivalent(common[i],path,NULL));
    CHECK(equivalent("\xef\xbb\xbf""// 😀\r\nfunc edited()\r\n int result = 7\r\n return res\r\nend\r\n",path,"return res"));
    CHECK(equivalent("func edited()\n auto box = Box()\n return box.\nend\n",path,"box."));
    CHECK(signature("func edited()\n return outer(1,@caret@\nend\n",path,0,0));
    CHECK(signature("func edited()\n return outer(1,@caret@unresolved)\nend\n",path,1,0));
    CHECK(signature("func edited()\n return outer(@caret@unresolved)\nend\n",path,1,1));
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    const char* current[]={"typedef Callback = func(c_vararg) as int\n", "typedef Callback = func(int?) as int\n",
        "typedef Callback = func(dict<string,list<int>>?) as int\n", "extern \"C\" @owned func text() as string\n",
        "func factory()\n auto f = func() as array<array<int>>\n return nested()\n end\nend\n"};
    for(size_t i=0;i<sizeof(current)/sizeof(*current);++i){
        CHECK(equivalent(current[i],path,NULL));
        CHECK(chunks(current[i],path));
    }
    CHECK(contexts());
#else
    /* Keep the helper used in legacy builds, without expecting new grammar. */
    (void)contexts;CHECK(chunks("typedef Number = int",path));
#endif
    xrtFree(path);return 1;
}
int main(void)
{if(!run())return 1;printf("analysis_parser_profile_test passed: %u checks; original API/Lemon parity and live editing projections\n",checks);return 0;}
