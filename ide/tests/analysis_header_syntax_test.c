#define main headerSyntaxBaseMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/module_store.h"

typedef struct header_node {const char* name;const XL6_AstNode* node;} header_node;
static int headerFind(const XL6_AstNode* n,const XL6_AstRange* range,void* user)
{
    (void)range;header_node* out=user;
    if((n->kind==XL6_AST_FUNC || n->kind==XL6_AST_EXPORT_FUNC || n->kind==XL6_AST_ASYNC_FUNC) && n->childCount && sameText(n->children[0]->text,out->name)){out->node=n;return 0;}
    return 1;
}
static const XL6_AstNode* headerNode(const xls_syntax* syntax,const char* name)
{header_node out={name,NULL};return xlsSyntaxVisit(syntax,headerFind,&out,NULL,NULL) ? out.node : NULL;}
static int headerFresh(const xls_syntax* syntax,const char* body)
{
    XL6_ParseResult p;xl6ParseResultInit(&p);int accepted=xl6ParseSource(body,xlsSyntaxPath(syntax),&p);
    CHECK(accepted==xlsSyntaxAccepted(syntax) && p.diags.count==xlsSyntaxDiagnosticCount(syntax) && sameAst(p.root,xlsSyntaxAst(syntax)));
    for(size_t i=0;i<p.diags.count;++i){xls_syntax_diagnostic d;CHECK(xlsSyntaxDiagnostic(syntax,i,&d) && d.line==p.diags.items[i].line && d.column==p.diags.items[i].column && sameText(d.message,p.diags.items[i].message));}
    xl6ParseResultFree(&p);return 1;
}
static int headerCase(const char* body,const char* needle,const char* replacement,int fragment)
{
    char* path=xrtPathAbs("header-中文😀.xl");CHECK(path);xls_text *a=xlsTextCreate(body,strlen(body),4u*1024u*1024u,NULL);CHECK(a);
    xls_parse_work work={0};xls_syntax* before=xlsSyntaxBuild(path,a,NULL,&work);CHECK(before && xlsSyntaxAccepted(before) && headerFresh(before,body));
    const XL6_AstNode* tail=headerNode(before,"tail");CHECK(tail);const char* at=strstr(body,needle);CHECK(at);xstrbuf changed={0};
    CHECK(xrtStrBufAppend(&changed,(xstrview){body,(size_t)(at-body)}) && xrtStrBufAppend(&changed,xrtStrView(replacement)) && xrtStrBufAppend(&changed,xrtStrView(at+strlen(needle))));
    xls_text* b=xlsTextCreate(changed.Data,changed.Size,4u*1024u*1024u,NULL);CHECK(b);work=(xls_parse_work){0};xls_syntax* after=xlsSyntaxBuild(path,b,before,&work);
    CHECK(after && headerFresh(after,changed.Data) && headerFresh(before,body));
    if(fragment)CHECK(!work.full && work.fragments==1 && work.reused>0 && work.bytes<changed.Size && headerNode(after,"tail")==tail);
    else CHECK(work.full==1);
    xlsSyntaxRelease(before);xlsSyntaxRelease(after);xlsTextRelease(a);xlsTextRelease(b);xrtStrBufFree(&changed);xrtFree(path);return 1;
}
static int headerCorpus(void)
{
    const char* ordinary="\xef\xbb\xbf// 😀\r\nfunc edited(int value,int other=7) as int\r\n return value+other\r\nend\r\nfunc tail() as int\r\n return 0\r\nend\r\n";
    CHECK(headerCase(ordinary,"edited(","renamed(",1));
    CHECK(headerCase(ordinary,"int value","float value",1));
    CHECK(headerCase(ordinary,"int other=7","string other=\"a\\0😀\"",1));
    CHECK(headerCase(ordinary,"as int\r\n return value","as float\r\n return value",1));
    CHECK(headerCase(ordinary,"int value,int other=7","int value",1));
    CHECK(headerCase(ordinary,"int value,int other=7","int value,int other=7,int extra=9",1));
    CHECK(headerCase(ordinary,"edited(int value,int other=7) as int\r\n return value+other","edited(int input,int other=7) as int\r\n return input+other",1));
    CHECK(headerCase(ordinary,"func edited","async func edited",0));
    CHECK(headerCase(ordinary,"func edited","func broken(\nend\nfunc edited",0));
    CHECK(headerCase(ordinary,"int other=7","int other=\"unterminated",0));
    CHECK(headerCase(ordinary,"int other=7","int other=/* unterminated",0));
    CHECK(headerCase("#doc\ndoc😀\n#end\nfunc edited(int value) as int\n return value\nend\nfunc tail()\nend\n","int value","string value",1));
    CHECK(headerCase("func edited<T>(T value) as T\n return value\nend\nfunc tail()\nend\n","<T>(T value) as T","<U>(U value) as U",1));
    CHECK(headerCase("export \"entry\" func edited(int value) as int\n return value\nend\nfunc tail()\nend\n","\"entry\"","\"entry_😀\"",1));
    CHECK(headerCase("async func edited(int value) as int\n return value\nend\nfunc tail()\nend\n","int value","string value",1));
    CHECK(headerCase("func edited(int value) as int\n return value\nend\nfunc tail()\nend\n","func edited","/*new*/func edited",0));
    return 1;
}
typedef struct header_reader {xls_syntax* syntax;const char* body;int passed;} header_reader;
static int32 headerThread(void* user)
{
    header_reader* reader=user;if(!headerFresh(reader->syntax,reader->body))return 1;
    reader->passed=1;xlsSyntaxRelease(reader->syntax);return 0;
}
static int headerSequence(void)
{
    char* path=xrtPathAbs("header-sequence.xl");CHECK(path);xstrbuf prefix={0};
    CHECK(xrtStrBufAppend(&prefix,xrtStrView("\xef\xbb\xbf// 😀\r\n")));
    for(unsigned i=0;i<64;++i)CHECK(xrtStrBufAppendFormat(&prefix,"func helper_%u(int value) as int\r\n return value\r\nend\r\n",i));
    xls_syntax* old=NULL;const XL6_AstNode* shared=NULL;
    for(unsigned i=0;i<40;++i){xstrbuf body={0};CHECK(xrtStrBufAppend(&body,xrtStrView(prefix.Data)) && xrtStrBufAppendFormat(&body,"#doc\r\neditable docs\r\n#end\r\nfunc edited(int argument_%u,int pad=%u) as int\r\n return argument_%u\r\nend\r\nfunc tail() as int\r\n return 7\r\nend\r\n",i,i,i));
        xls_text* text=xlsTextCreate(body.Data,body.Size,4u*1024u*1024u,NULL);CHECK(text);xls_parse_work w={0};xls_syntax* current=xlsSyntaxBuild(path,text,old,&w);CHECK(current && xlsSyntaxAccepted(current) && headerFresh(current,body.Data));
        if(i)CHECK(w.fragments==1 && !w.full && w.reused==65 && w.bytes<256 && headerNode(current,"helper_0")==shared);
        else {shared=headerNode(current,"helper_0");CHECK(shared && w.full==1);}
        xlsSyntaxRelease(old);old=current;xlsTextRelease(text);xrtStrBufFree(&body);
    }
    xstrbuf current={0};CHECK(xrtStrBufAppend(&current,xrtStrView(prefix.Data)) && xrtStrBufAppend(&current,xrtStrView("#doc\r\neditable docs\r\n#end\r\nfunc edited(int argument_reader,int pad=40) as int\r\n return argument_reader\r\nend\r\nfunc tail() as int\r\n return 7\r\nend\r\n")));
    xls_text* body=xlsTextCreate(current.Data,current.Size,4u*1024u*1024u,NULL);CHECK(body);xls_parse_work w={0};xls_syntax* next=xlsSyntaxBuild(path,body,old,&w);CHECK(next && w.fragments==1 && !w.full && w.reused==65 && headerNode(next,"helper_0")==shared);
    xlsSyntaxRelease(old);xlsTextRelease(body);header_reader readers[4];xthread* threads[4];
    for(unsigned i=0;i<4;++i){readers[i]=(header_reader){xlsSyntaxRetain(next),current.Data,0};threads[i]=xrtThreadCreate(headerThread,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
    xlsSyntaxRelease(next);xrtStrBufFree(&current);xrtStrBufFree(&prefix);xrtFree(path);return 1;
}
static int headerAnalysis(void)
{
    char *entry=xrtPathAbs("header-main.xl"),*dep=xrtPathAbs("dep.xl");CHECK(entry && dep);
    const char* main="import dep\r\nfunc main(...) as int\r\n return dep.compute(7)\r\nend\r\n";
    xls_analysis_document docs[2]={{"entry",entry,xlsTextCreate(main,strlen(main),4u*1024u*1024u,NULL),1,1},{"dep",dep,NULL,1,1}};CHECK(docs[0].text);
    xls_check_session* session=xlsCheckSessionCreate(NULL);xls_module_store* store=xlsModuleStoreCreate(NULL);CHECK(session && store);xls_module_scope bank;xlsModuleScopeBegin(&bank,store);
    for(unsigned v=0;v<5;++v){xstrbuf text={0};const char* arg=v>=3 ? "input" : "value";CHECK(xrtStrBufAppendFormat(&text,"\xef\xbb\xbf// 😀\r\nfunc identity(int %s) as int\r\n return %s\r\nend\r\n",arg,arg));
        for(unsigned i=0;i<32;++i)CHECK(xrtStrBufAppendFormat(&text,"func work_%u(int value) as int\r\n int local=identity(value)\r\n if local > 0 then\r\n return local\r\n end\r\n return value\r\nend\r\n",i));
        const char* name=v==2 ? "longer" : v ? "other" : "value";
        CHECK(xrtStrBufAppendFormat(&text,"export func compute(int value) as int\r\n return work_0(value)\r\nend\r\n#doc\r\ndesc:\r\n unused docs😀\r\n#end\r\nfunc unused(int %s%s) as int\r\n return %s\r\nend\r\n",name,v==4 ? "=2" : "",name));
        xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(text.Data,text.Size,4u*1024u*1024u,NULL);CHECK(docs[1].text);++docs[1].revision;
        xls_semantic_check* result=xlsCheckSessionCheck(session,docs,2,"entry",1,NULL,NULL,NULL,0);
        if(result && !xlsSemanticCheckInfo(result).accepted){char* json=xlsSemanticCheckJson(result,NULL);fprintf(stderr,"header import v%u:%s\n",v,json ? json : "no report");xlsSemanticStringFree(json);}
        CHECK(result && xlsSemanticCheckInfo(result).accepted && fresh(result,docs,2));
        xls_parse_work w=xlsSemanticCheckModuleParseWork(result);
        if(v && v!=3)CHECK(!w.full && w.fragments==1 && w.reused==34 && w.bytes<256 && xlsSemanticCheckReads(result).reused>=32);
        /* Two separate functions changed. Current ordinary chunks parse both
         * while used-header dependencies still force semantic rechecking. */
        if(v==3){
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
            CHECK(!w.full && w.fragments==2 && w.reused==33 && w.bytes<512 && xlsSemanticCheckWork(result).functionsChecked>=32);
#else
            CHECK(w.full==1 && xlsSemanticCheckWork(result).functionsChecked>=32);
#endif
        }
        printf("import v%u full=%llu fragments=%llu retained=%llu bytes=%llu\n",v,(unsigned long long)w.full,(unsigned long long)w.fragments,(unsigned long long)w.reused,(unsigned long long)w.bytes);
        xlsSemanticCheckDestroy(result);xrtStrBufFree(&text);
    }
    xlsModuleScopeEnd(&bank);xlsModuleStoreRelease(store);xlsCheckSessionDestroy(session);xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xrtFree(entry);xrtFree(dep);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!headerCorpus() || !headerSequence() || !headerAnalysis())return 1;
    printf("analysis_header_syntax_test passed: %u checks;actual retained declaration identities across signature/default/generic/export/doc/Unicode changes,strict boundaries and malformed fallback,complete fresh AST17 parity\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
