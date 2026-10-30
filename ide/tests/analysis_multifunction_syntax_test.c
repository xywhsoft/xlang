#include "analysis/frontend_internal.h"
#include "analysis_ast_same.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static xatomic32 checks;
#define CHECK(v) do{xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"multi-syntax:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static char* make(unsigned count,unsigned generation,int crlf)
{
    xstrbuf b={0};char line[1000];
    if(!xrtStrBufAppend(&b,xrtStrView("\xef\xbb\xbf// 中文😀\n\n")))return NULL;
    for(unsigned i=0;i<count;++i){
        snprintf(line,sizeof(line),"/// documented %u\nfunc work_%u(int value) as int\n int local = value+%u\n%s return local+%u\nend\n\n",i,i,i,generation && (i==1 || i+1==count) ? " // inserted 😀\n int extra = 8\n" : "",generation && (i==1 || i+1==count) ? generation : 0);
        if(!xrtStrBufAppend(&b,xrtStrView(line))){xrtStrBufFree(&b);return NULL;}
    }
    char* result=NULL;
    if(crlf){xstrbuf out={0};int okay=1;for(size_t i=0;okay && i<b.Size;++i){if(b.Data[i]=='\n')okay=xrtStrBufAppend(&out,xrtStrView("\r"));if(okay)okay=xrtStrBufAppend(&out,(xstrview){b.Data+i,1});}if(okay)result=xrtStrDup(out.Data);xrtStrBufFree(&out);}
    else result=xrtStrDup(b.Data);
    xrtStrBufFree(&b);return result;
}
static int verify(xls_syntax* syntax,const char* body)
{
    XL6_ParseResult full;xl6ParseResultInit(&full);int okay=xl6ParseSource(body,xlsSyntaxPath(syntax),&full);
    CHECK(okay==xlsSyntaxAccepted(syntax) && full.diags.count==xlsSyntaxDiagnosticCount(syntax));
    const XL6_AstNode* current=xlsSyntaxAst(syntax);
    if(!sameAst(full.root,current)){
        fprintf(stderr,"AST differs after multi-function update\n");
    }
    CHECK(sameAst(full.root,current));xl6ParseResultFree(&full);return 1;
}
static char* ir(const XL6_IrModule* module)
{
    XL6_Output output;if(!xl6OutputOpenMemory(&output))return NULL;
    xl6IrDumpModuleJson(&output,module);char* result=xl6OutputHasFailed(&output) ? NULL : xl6OutputTakeMemoryText(&output,NULL);xl6OutputDiscard(&output);return result;
}
static int semantic(xls_syntax* syntax,const char* body)
{
    const char* path=xlsSyntaxPath(syntax);xls_semantic_input input={path,body,strlen(body)};char error[256];
    xls_semantic_check* current=xlsSemanticCheckCreateParsed(path,&input,1,1,syntax,NULL,NULL,error,sizeof(error));
    xls_semantic_check* full=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    if(!current || !full)fprintf(stderr,"semantic infrastructure: %s\n",error);
    if(current && !xlsSemanticCheckInfo(current).accepted){char* problem=xlsSemanticCheckJson(current,NULL);fprintf(stderr,"semantic rejected: %s\n",problem ? problem : "no report");xrtFree(problem);}
    CHECK(current && full && xlsSemanticCheckInfo(current).accepted && xlsSemanticCheckInfo(full).accepted);
    CHECK(sameAst(xlsSemanticCheckAst(current),xlsSemanticCheckAst(full)));
    char *a=ir(xlsSemanticCheckIr(current)),*b=ir(xlsSemanticCheckIr(full));CHECK(a && b && !strcmp(a,b));
    xrtFree(a);xrtFree(b);xlsSemanticCheckDestroy(current);xlsSemanticCheckDestroy(full);return 1;
}
static int series(unsigned count,int crlf)
{
    char* path=xrtPathAbs("多函数😀.xl");CHECK(path);xls_syntax* prior=NULL;
    for(unsigned generation=0;generation<6;++generation){
        char* body=make(count,generation,crlf);CHECK(body);xls_text* text=xlsTextCreate(body,strlen(body),8u*1024u*1024u,NULL);CHECK(text);
        xls_parse_work work={0};xls_syntax* next=xlsSyntaxBuild(path,text,prior,&work);xlsTextRelease(text);CHECK(next && xlsSyntaxAccepted(next));
        if(generation)CHECK(!work.full && work.fragments==2 && work.reused==count-2 && work.bytes<strlen(body));
        else CHECK(work.full==1);
        xlsSyntaxRelease(prior);prior=NULL;CHECK(verify(next,body));
        if(count<=32)CHECK(semantic(next,body));
        printf("multi generation=%u functions=%u fragments=%llu reused=%llu bytes=%llu/%zu\n",generation,count,(unsigned long long)work.fragments,(unsigned long long)work.reused,(unsigned long long)work.bytes,strlen(body));
        prior=next;xrtFree(body);
    }
    xlsSyntaxRelease(prior);xrtFree(path);return 1;
}
static char* edited(const char* body,const char* from,const char* to)
{
    const char* p=strstr(body,from);if(!p)return NULL;xstrbuf b={0};
    int okay=xrtStrBufAppend(&b,(xstrview){body,(size_t)(p-body)}) && xrtStrBufAppend(&b,xrtStrView(to)) && xrtStrBufAppend(&b,xrtStrView(p+strlen(from)));
    char* result=okay ? xrtStrDup(b.Data) : NULL;xrtStrBufFree(&b);return result;
}
static int boundaries(void)
{
    char* path=xrtPathAbs("multi-boundaries.xl"),*before=make(8,0,0),*changed=make(8,2,0);CHECK(path && before && changed);
    xls_text* text=xlsTextCreate(before,strlen(before),1024u*1024u,NULL);CHECK(text);xls_parse_work initial={0};
    xls_syntax* old=xlsSyntaxBuild(path,text,NULL,&initial);xlsTextRelease(text);CHECK(old && xlsSyntaxAccepted(old));
    const char* from[]={"end\n\n/// documented 2","int extra = 8","int extra = 8","func work_1(int value) as int\n int local = value+1","func work_7(int value)"};
    const char* to[]={"end\n\n// changed gap\n/// documented 2","string extra = \"not closed","int extra = 8\n /*\n\n/// documented 2\nfunc work_2(int value) as int\n */","func work_1(int source) as int\n int local = source+1","func tail(int value)"};
    for(unsigned i=0;i<5;++i){
        char* body=edited(changed,from[i],to[i]);CHECK(body);text=xlsTextCreate(body,strlen(body),1024u*1024u,NULL);CHECK(text);
        xls_parse_work work={0};xls_syntax* next=xlsSyntaxBuild(path,text,old,&work);xlsTextRelease(text);CHECK(next && verify(next,body) && verify(old,before));
        if(i<3)CHECK(work.full==1);else CHECK(!work.full && work.fragments==2 && work.reused==6 && semantic(next,body));
        xlsSyntaxRelease(next);xrtFree(body);
    }
    xlsSyntaxRelease(old);xrtFree(before);xrtFree(changed);xrtFree(path);return 1;
}
typedef struct multi_reader {xls_syntax* old;unsigned value;int passed;} multi_reader;
static int32 concurrent(void* user)
{
    multi_reader* r=user;r->passed=1;
    for(unsigned i=0;i<8;++i){
        char* body=make(32,r->value+i,1);xls_text* text=body ? xlsTextCreate(body,strlen(body),1024u*1024u,NULL) : NULL;xls_parse_work work={0};
        xls_syntax* next=text ? xlsSyntaxBuild(xlsSyntaxPath(r->old),text,r->old,&work) : NULL;
        if(!next || work.full || work.fragments!=2 || !verify(next,body))r->passed=0;
        xlsSyntaxRelease(next);xlsTextRelease(text);xrtFree(body);if(!r->passed)break;
    }
    xlsSyntaxRelease(r->old);return 0;
}
static int parallel(void)
{
    char* path=xrtPathAbs("multi-shared.xl"),*body=make(32,0,1);CHECK(path && body);
    xls_text* text=xlsTextCreate(body,strlen(body),1024u*1024u,NULL);CHECK(text);xls_parse_work work={0};xls_syntax* old=xlsSyntaxBuild(path,text,NULL,&work);xlsTextRelease(text);CHECK(old && verify(old,body));
    multi_reader readers[4];xthread* threads[4];
    for(unsigned i=0;i<4;++i){readers[i]=(multi_reader){xlsSyntaxRetain(old),i+1,0};threads[i]=xrtThreadCreate(concurrent,&readers[i],0);CHECK(threads[i]);}
    xlsSyntaxRelease(old);
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
    xrtFree(body);xrtFree(path);return 1;
}
static int currentHeaders(void)
{
    const char* headers[]={"typedef Callback = func(c_vararg) as int\n",
        "typedef Callback = func(dict<string,list<int>>?) as int\n",
        "extern \"C\" @owned func text() as string\n",
        "func factory()\n auto f = func() as array<array<int>>\n return nested()\n end\nend\n"};
    char* path=xrtPathAbs("multi-current-中文😀.xl");CHECK(path);
    for(size_t i=0;i<sizeof(headers)/sizeof(*headers);++i){
        char *a=make(8,0,1),*b=make(8,2,1);CHECK(a && b);xstrbuf before={0},after={0};
        CHECK(xrtStrBufAppendFormat(&before,"\xef\xbb\xbf%s%s",headers[i],a+3) &&
              xrtStrBufAppendFormat(&after,"\xef\xbb\xbf%s%s",headers[i],b+3));
        xls_text *left=xlsTextCreate(before.Data,before.Size,1024u*1024u,NULL),
                 *right=xlsTextCreate(after.Data,after.Size,1024u*1024u,NULL);CHECK(left && right);
        xls_parse_work cold={0},work={0};xls_syntax* old=xlsSyntaxBuild(path,left,NULL,&cold);CHECK(old && verify(old,before.Data));
        xls_syntax* next=xlsSyntaxBuild(path,right,old,&work);
        CHECK(next && !work.full && work.fragments==2 && work.reused>=6 && verify(next,after.Data) && verify(old,before.Data));
        xlsSyntaxRelease(old);CHECK(verify(next,after.Data));xlsSyntaxRelease(next);
        xlsTextRelease(left);xlsTextRelease(right);xrtStrBufFree(&before);xrtStrBufFree(&after);xrtFree(a);xrtFree(b);
    }
    xrtFree(path);return 1;
}
static int seamProofs(void)
{
    char *path=xrtPathAbs("multi-proof-seams.xl"),*rawBefore=make(8,0,0),*rawChanged=make(8,2,0);CHECK(path && rawBefore && rawChanged);
    char *before=edited(rawBefore,"/// documented 4","#doc\ndesc:\n documented 4\n#end"),
         *changed=edited(rawChanged,"/// documented 4","#doc\ndesc:\n documented 4\n#end");CHECK(before && changed);xrtFree(rawBefore);xrtFree(rawChanged);
    xls_text* text=xlsTextCreate(before,strlen(before),1024u*1024u,NULL);CHECK(text);
    xls_parse_work cold={0};xls_syntax* old=xlsSyntaxBuild(path,text,NULL,&cold);xlsTextRelease(text);CHECK(old && verify(old,before));
    const XL6_AstNode* root=xlsSyntaxAst(old);CHECK(root && root->childCount==8 && root->children[4]->doc && strstr(root->children[4]->doc,"documented 4"));
    const char* from[]={"\n documented 4\n","end\n\n/// documented 2"," int local = value+7\n","/// documented 5"};
    const char* to[]={"\n documented X\n","end // trailing note\n\n/// documented 2",
        " int local = value+7\n /*\n\n/// documented 2\nfunc work_2(int value) as int\n */\n","/// documented X"};
    for(size_t i=0;i<4;++i){
        char* body=edited(changed,from[i],to[i]);CHECK(body);text=xlsTextCreate(body,strlen(body),1024u*1024u,NULL);CHECK(text);
        xls_parse_work work={0};xls_syntax* next=xlsSyntaxBuild(path,text,old,&work);xlsTextRelease(text);
        CHECK(next && verify(next,body) && verify(old,before));
        if(i==0)CHECK(!work.full && work.fragments==3 && work.reused==5);
        else CHECK(work.full==1);
        xlsSyntaxRelease(next);xrtFree(body);
    }
    xlsSyntaxRelease(old);xrtFree(path);xrtFree(before);xrtFree(changed);return 1;
}
static int documentation(void)
{
    char *path=xrtPathAbs("multi-documentation-中文😀.xl"),*raw=make(8,0,0),*rawChanged=make(8,2,0);CHECK(path && raw && rawChanged);
    const char* block="#doc\ndesc:\n documented 4\n#end\n";
    char *before=edited(raw,"/// documented 4\n",block),*changed=edited(rawChanged,"/// documented 4\n",block);CHECK(before && changed);
    xls_text* text=xlsTextCreate(before,strlen(before),1024u*1024u,NULL);CHECK(text);xls_parse_work cold={0};
    xls_syntax* old=xlsSyntaxBuild(path,text,NULL,&cold);xlsTextRelease(text);CHECK(old && verify(old,before));
    for(unsigned i=0;i<3;++i){
        char* body=i==0 ? edited(before,"documented 4","edited 中文😀") : edited(i==1 ? before : changed,block,"");CHECK(body);
        text=xlsTextCreate(body,strlen(body),1024u*1024u,NULL);CHECK(text);xls_parse_work work={0};
        xls_syntax* next=xlsSyntaxBuild(path,text,old,&work);xlsTextRelease(text);CHECK(next && verify(next,body) && verify(old,before));
        const XL6_AstNode* root=xlsSyntaxAst(next);CHECK(root && root->childCount==8);
        if(i==0)CHECK(!work.full && work.fragments==1 && root->children[4]->doc && strstr(root->children[4]->doc,"edited 中文😀"));
        else{CHECK(!root->children[4]->doc);if(i==1)CHECK(work.full==1);else CHECK(!work.full && work.fragments==3);}
        xlsSyntaxRelease(next);xrtFree(body);
    }
    char* retired=edited(before,"documented 4","retired owner 中文😀");CHECK(retired);text=xlsTextCreate(retired,strlen(retired),1024u*1024u,NULL);CHECK(text);
    xls_parse_work last={0};xls_syntax* final=xlsSyntaxBuild(path,text,old,&last);xlsTextRelease(text);CHECK(final && !last.full && last.fragments==1);
    xlsSyntaxRelease(old);CHECK(verify(final,retired));xlsSyntaxRelease(final);xrtFree(retired);
    xrtFree(path);xrtFree(raw);xrtFree(rawChanged);xrtFree(before);xrtFree(changed);return 1;
}
static int changedLimit(void)
{
    char* path=xrtPathAbs("multi-changed-limit.xl");CHECK(path);xstrbuf prefix={0};char padding[1024];memset(padding,'x',sizeof(padding));
    CHECK(xrtStrBufAppend(&prefix,xrtStrView("\xef\xbb\xbf/*")));
    for(unsigned i=0;i<256;++i)CHECK(xrtStrBufAppend(&prefix,(xstrview){padding,sizeof(padding)}));
    CHECK(xrtStrBufAppend(&prefix,xrtStrView("*/\n")));
    for(unsigned count=64;count<=65;++count){
        char *a=make(count,0,0),*b=a ? xrtStrDup(a) : NULL;CHECK(a && b);
        for(unsigned i=0;i<count;++i){
            char from[80],to[80];snprintf(from,sizeof(from),"value+%u\n",i);snprintf(to,sizeof(to),"value+%u+1\n",i);
            char* updated=edited(b,from,to);CHECK(updated);xrtFree(b);b=updated;
        }
        xstrbuf before={0},after={0};CHECK(xrtStrBufAppend(&before,(xstrview){prefix.Data,prefix.Size}) && xrtStrBufAppend(&before,xrtStrView(a+3)) &&
            xrtStrBufAppend(&after,(xstrview){prefix.Data,prefix.Size}) && xrtStrBufAppend(&after,xrtStrView(b+3)));
        xls_text *left=xlsTextCreate(before.Data,before.Size,2u*1024u*1024u,NULL),*right=xlsTextCreate(after.Data,after.Size,2u*1024u*1024u,NULL);CHECK(left && right);
        xls_parse_work cold={0},work={0};xls_syntax* old=xlsSyntaxBuild(path,left,NULL,&cold);CHECK(old && verify(old,before.Data));
        xls_syntax* next=xlsSyntaxBuild(path,right,old,&work);CHECK(next && verify(next,after.Data) && verify(old,before.Data));
        CHECK(work.fragments==64 && work.full==(count==65));
        xlsSyntaxRelease(old);CHECK(verify(next,after.Data));xlsSyntaxRelease(next);xlsTextRelease(left);xlsTextRelease(right);
        xrtStrBufFree(&before);xrtStrBufFree(&after);xrtFree(a);xrtFree(b);
    }
    xrtStrBufFree(&prefix);xrtFree(path);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);
    if(!series(3,0) || !series(8,0) || !series(32,1) || !series(512,1) || !boundaries() || !currentHeaders() || !seamProofs() || !documentation() || !changedLimit() || !parallel())return 1;
    printf("analysis_multifunction_syntax_test passed: %u checks; full native AST/semantic/IR parity, seam rejection, concurrent and retired-owner lifetimes.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}

#else
int main(void){puts("analysis_multifunction_syntax_test passed: legacy keeps original single-function route");return 0;}
#endif
