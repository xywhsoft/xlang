#include "analysis/frontend_internal.h"
#include "analysis/ast_package_fields.h"
#include "src/sem/xl6_completion.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
#define CHECK(value) do { xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED); if(!(value)) { fprintf(stderr,"analysis_cursor_syntax_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static int sameText(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static int sameRange(XL6_AstRange a,XL6_AstRange b)
{ return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn; }
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b) return a==b;
    if(!xlsAstPackageScalarsEqual(a,b) || !xlsAstPackageLinkEqual(a,b))return 0;
    if(!!a->source!=!!b->source) return 0;
    if(a->source && (a->source->completionQuery!=b->source->completionQuery ||
       a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd)) return 0;
    if(a->kind!=b->kind || a->flags!=b->flags || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) ||
       a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar || a->literalDecoded!=b->literalDecoded || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || a->line!=b->line || a->column!=b->column ||
       !sameRange(a->range,b->range) || !sameRange(a->tokenRange,b->tokenRange) || !sameText(xl6AstSourcePath(a),xl6AstSourcePath(b)) ||
       a->semanticTypeId!=b->semanticTypeId || a->semanticTypeAbiId!=b->semanticTypeAbiId || a->semanticSymbolId!=b->semanticSymbolId ||
       a->semanticDeclaredSymbolId!=b->semanticDeclaredSymbolId || a->semanticTargetSymbolId!=b->semanticTargetSymbolId ||
       a->semanticCompoundOperatorSymbolId!=b->semanticCompoundOperatorSymbolId || a->semanticCallKind!=b->semanticCallKind ||
       a->semanticLvalueKind!=b->semanticLvalueKind || a->semanticOwnership!=b->semanticOwnership || a->semanticExitKind!=b->semanticExitKind ||
       a->semanticScopeDepth!=b->semanticScopeDepth || a->semanticTargetScopeDepth!=b->semanticTargetScopeDepth ||
       a->semanticRegionId!=b->semanticRegionId || a->semanticTargetRegionId!=b->semanticTargetRegionId || a->childCount!=b->childCount) return 0;
    if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!sameAst(a->children[i],b->children[i])) return 0;
    return 1;
}


static size_t skipBom(const char* body)
{ return strlen(body)>=3 && !memcmp(body,"\xef\xbb\xbf",3) ? 3 : 0; }
static size_t cursorAt(const char* body,const char* marker)
{ const char* at=strstr(body,marker);return at ? (size_t)(at-body)+strlen(marker)-skipBom(body) : SIZE_MAX; }
static int verify(xls_syntax* syntax,const XL6_CompletionQuery* query,size_t offset)
{
    const char* body=xlsTextData(xlsSyntaxText(syntax));const char* path=xlsSyntaxPath(syntax);
    XL6_ParseResult fresh,copy;xl6ParseResultInit(&fresh);xl6ParseResultInit(&copy);
    XL6_CompletionQuery reference={.path=path,.offset=offset};
    int accepted=xl6ParseSourceWithCompletion(body,path,&fresh,&reference);
    CHECK(accepted==xlsSyntaxAccepted(syntax)&&fresh.diags.count==xlsSyntaxDiagnosticCount(syntax));
    CHECK(query->parsed==reference.parsed&&query->reached==reference.reached&&query->available==reference.available);
    CHECK(query->offset==offset&&sameText(query->prefix,reference.prefix)&&sameText(query->error,reference.error)&&sameRange(query->replacement,reference.replacement));
    CHECK(sameAst(xlsSyntaxAst(syntax),fresh.root));
    CHECK(xlsSyntaxIsCursor(syntax));
    XL6_CompletionQuery projection={.path=path,.offset=offset};
    XL6_ParseResult projected;xl6ParseResultInit(&projected);
    CHECK(xlsSyntaxCopyCursorParse(syntax,&projection,&projected));
    CHECK(projection.parsed==reference.parsed&&sameText(projection.prefix,reference.prefix)&&sameText(projection.error,reference.error)&&sameRange(projection.replacement,reference.replacement));
    CHECK(sameAst(projected.root,fresh.root));
    if(projection.prefix&&query->prefix) CHECK(projection.prefix!=query->prefix);
    xl6CompletionFree(&projection);xl6ParseResultFree(&projected);
    CHECK(xlsSyntaxCopyParse(syntax,&copy)&&sameAst(copy.root,fresh.root));
    if(copy.root && copy.root->source) {
        CHECK(copy.root->source!=xlsSyntaxAst(syntax)->source);
        copy.root->source->completionStart=SIZE_MAX;
        CHECK(sameAst(xlsSyntaxAst(syntax),fresh.root));
    }
    for(size_t i=0;i<fresh.diags.count;++i) {
        xls_syntax_diagnostic d;const XL6_Diag* f=&fresh.diags.items[i];
        CHECK(xlsSyntaxDiagnostic(syntax,i,&d)&&d.line==f->line&&d.column==f->column&&sameText(d.path,f->file)&&sameText(d.message,f->message));
    }
    xl6CompletionFree(&reference);xl6ParseResultFree(&copy);xl6ParseResultFree(&fresh);return 1;
}
static xls_syntax* build(const char* path,const char* body,size_t offset,const xls_syntax* previous,xls_parse_work* work)
{
    xls_text* text=xlsTextCreate(body,strlen(body),8u*1024u*1024u,NULL);if(!text)return NULL;
    XL6_CompletionQuery query={.path=path,.offset=offset};
    xls_syntax* syntax=xlsSyntaxBuildCursor(path,text,previous,&query,work);xlsTextRelease(text);
    if(syntax&&!verify(syntax,&query,offset)) {xlsSyntaxRelease(syntax);syntax=NULL;}
    xl6CompletionFree(&query);return syntax;
}
static char* replace(const char* body,const char* before,const char* after)
{
    const char* at=strstr(body,before);if(!at)return NULL;
    xstrbuf out;xrtStrBufInit(&out);int okay=xrtStrBufAppend(&out,(xstrview){body,(size_t)(at-body)})&&
        xrtStrBufAppend(&out,xrtStrView(after))&&xrtStrBufAppend(&out,xrtStrView(at+strlen(before)));
    char* result=okay ? xrtStrDup(out.Data) : NULL;xrtStrBufFree(&out);return result;
}
static int corpus(const char* body,const char* marker,const char* needle,const char* replacement,const char* nextMarker,int fragment)
{
    char* path=xrtPathAbs("cursor-syntax-入口.xl");char* next=replace(body,needle,replacement);CHECK(path&&next);
    xls_parse_work initial={0},work={0};size_t offset=cursorAt(body,marker),nextOffset=cursorAt(next,nextMarker);
    CHECK(offset!=SIZE_MAX&&nextOffset!=SIZE_MAX);
    xls_syntax* first=build(path,body,offset,NULL,&initial);CHECK(first&&xlsSyntaxAccepted(first)&&initial.full==1);
    xls_syntax* changed=build(path,next,nextOffset,first,&work);CHECK(changed);
    /* Call again after the builder's caller-owned prefix has been freed. */
    XL6_CompletionQuery q={.path=path,.offset=offset};XL6_ParseResult parse;xl6ParseResultInit(&parse);
    xl6ParseSourceWithCompletion(body,path,&parse,&q);CHECK(verify(first,&q,offset));xl6CompletionFree(&q);xl6ParseResultFree(&parse);
    if(fragment && (work.full || work.fragments!=1 || !work.reused || work.bytes>=strlen(next))) fprintf(stderr,"cursor work full=%llu fragments=%llu reused=%llu bytes=%llu updated=%zu\n",(unsigned long long)work.full,(unsigned long long)work.fragments,(unsigned long long)work.reused,(unsigned long long)work.bytes,strlen(next));
    if(fragment) CHECK(work.full==0&&work.fragments==1&&work.reused&&work.bytes<strlen(next));
    else CHECK(work.full==1&&!work.reused);
    xlsSyntaxRelease(first);xlsSyntaxRelease(changed);xrtFree(path);xrtFree(next);return 1;
}
typedef struct find_node {const char* name;const XL6_AstNode* found;} find_node;
static int find(const XL6_AstNode* node,const XL6_AstRange* range,void* user)
{
    (void)range;find_node* f=user;
    if(node->kind==XL6_AST_FUNC&&node->childCount&&sameText(node->children[0]->text,f->name)) {f->found=node;return 0;}return 1;
}
typedef struct reader {xls_syntax* syntax;size_t offset;int passed;} reader;
static int32 parallel(void* user)
{
    reader* r=user;const char* path=xlsSyntaxPath(r->syntax);const char* text=xlsTextData(xlsSyntaxText(r->syntax));
    XL6_ParseResult fresh;xl6ParseResultInit(&fresh);XL6_CompletionQuery q={.path=path,.offset=r->offset};
    r->passed=xl6ParseSourceWithCompletion(text,path,&fresh,&q)&&verify(r->syntax,&q,r->offset);
    xl6ParseResultFree(&fresh);xl6CompletionFree(&q);xlsSyntaxRelease(r->syntax);return 0;
}
static int run(void)
{
    const char* body="func edited() as int\n int result = 7\n return res\nend\nfunc tail() as int\n return 9\nend\n";
    CHECK(corpus(body,"return res","result = 7","result = 777777","return res",1));
    CHECK(corpus(body,"return res","int result = 7","string 中文 = \"😀\"\n int result = 88","return res",1));
    CHECK(corpus(body,"return res","return res","return result","return result",1));
    CHECK(corpus(body,"return res","return res","if true\n return result\n end\n return res","return result",1));
    CHECK(corpus(body,"return res","as int","as float","return res",0));
    CHECK(corpus(body,"return res","return 9","return 99","return res",0));
    CHECK(corpus(body,"return res","result = 7","result = 8","return 9",0));
    CHECK(corpus(body,"return res","result = 7","result = 8","edited",0));
    CHECK(corpus(body,"return res","return res","return \"unterminated","return \"unterminated",0));
    CHECK(corpus(body,"return res","return res","/* unterminated","/* unterminated",0));
    CHECK(corpus(body,"return res","return res","return res\nend\nfunc inserted() as int\n return 1","return res",0));
    CHECK(corpus(body,"return res","result = 7","result = 7","return res",0));
    CHECK(corpus(body,"return res","result = 7","result = 7","result =",0));
    CHECK(corpus(body,"return res","result = 7","result = 7","return re",0));
    CHECK(corpus("/*😀*/ func edited() as int\n int result = 7\n return res\nend\nfunc tail()\nend\n","return res","result = 7","result = 88","return res",1));
    CHECK(corpus("import unknown_module\nfunc edited() as int\n int result = 7\n return res\nend\nfunc tail()\nend\n","return res","result = 7","result = 88","return res",1));
    const char* unicode="\xef\xbb\xbf" "// 😀\r\n#doc\r\ndesc:\r\n 查询文档\r\n#end\r\nfunc edited<T>(T 值) as T\r\n string 文本 = f\"😀={值}\"\r\n return 值\r\nend\r\nfunc tail() as int\r\n return 9\r\nend\r\n";
    CHECK(corpus(unicode,"return 值","string 文本 =","string 其它 =","return 值",1));
    CHECK(corpus(unicode,"return 值","return 值","if true\r\n return 值\r\n end\r\n return 值","return 值",1));
    CHECK(corpus(unicode,"return 值","\xef\xbb\xbf","","return 值",0));
    const char* member="func edited() as int\n auto box = Box()\n return box.\nend\nfunc tail() as int\n return 9\nend\n";
    CHECK(corpus(member,"box.","Box()","Box(7)","box.",1));
    CHECK(corpus(member,"box.","box.","box.field","box.fi",1));
    const char* safe="func edited() as int\n auto box = Box()\n return box?.\nend\nfunc tail() as int\n return 9\nend\n";
    CHECK(corpus(safe,"box?.","Box()","Box(7)","box?.",1));
    CHECK(corpus("async func edited() as int\n int result = 7\n return res\nend\nfunc tail()\nend\n","return res","result = 7","result = 8","return res",1));
    CHECK(corpus("export \"selected\" func edited() as int\n int result = 7\n return res\nend\nfunc tail()\nend\n","return res","result = 7","result = 8","return res",1));
    CHECK(corpus("class Box\n func edited() as int\n int result = 7\n return res\n end\nend\nfunc tail()\nend\n","return res","result = 7","result = 8","return res",0));

    char* path=xrtPathAbs("cursor-syntax-large.xl");CHECK(path);xstrbuf source;xrtStrBufInit(&source);
    for(unsigned i=0;i<500;++i) CHECK(xrtStrBufAppendFormat(&source,"func helper_%u(int value) as int\n return value + 1\nend\n",i));
    CHECK(xrtStrBufAppend(&source,xrtStrView(body)));size_t firstOffset=cursorAt(source.Data,"return res");xls_parse_work initial={0};
    xls_syntax* first=build(path,source.Data,firstOffset,NULL,&initial);CHECK(first);
    find_node saved={"tail",NULL};CHECK(xlsSyntaxVisit(first,find,&saved,NULL,NULL)&&saved.found);
    xls_syntax* previous=xlsSyntaxRetain(first);xls_parse_work total={0};
    for(unsigned i=0;i<40;++i) {
        char changed[200];snprintf(changed,sizeof(changed),"string 中文 = \"😀-%u\"\n%s int result = %u",i,i&1 ? "\n" : "",i+8);
        char* next=replace(source.Data,"int result = 7",changed);CHECK(next);size_t offset=cursorAt(next,"return res");xls_parse_work work={0};
        xls_syntax* syntax=build(path,next,offset,previous,&work);CHECK(syntax&&work.full==0&&work.fragments==1&&work.reused==501);
        find_node tail={"tail",NULL};CHECK(xlsSyntaxVisit(syntax,find,&tail,NULL,NULL)&&tail.found==saved.found);
        total.fragments+=work.fragments;total.reused+=work.reused;total.bytes+=work.bytes;
        xlsSyntaxRelease(previous);previous=syntax;xrtFree(next);
    }
    CHECK(total.fragments==40&&total.reused==20040&&total.bytes<40u*256u);
    printf("cursor fragments=%llu retained declarations=%llu parsed bytes=%llu\n",(unsigned long long)total.fragments,(unsigned long long)total.reused,(unsigned long long)total.bytes);

    /* Ordinary parsing must remove the cursor marker; mode changes never
     * reuse a marker-bearing chunk. Invalid source then valid source recovers. */
    xls_text* text=xlsTextCreate(source.Data,source.Size,8u*1024u*1024u,NULL);CHECK(text);xls_parse_work work={0};
    xls_syntax* plain=xlsSyntaxBuild(path,text,previous,&work);CHECK(plain&&work.full==1&&!work.reused);
    CHECK(!xlsSyntaxIsCursor(plain));
    XL6_CompletionQuery wrongMode={.path=path,.offset=firstOffset};XL6_ParseResult untouched;xl6ParseResultInit(&untouched);
    CHECK(!xlsSyntaxCopyCursorParse(plain,&wrongMode,&untouched)&&!untouched.root&&!wrongMode.prefix&&!wrongMode.parsed);
    XL6_ParseResult reference;xl6ParseResultInit(&reference);CHECK(xl6ParseSource(source.Data,path,&reference)&&sameAst(xlsSyntaxAst(plain),reference.root));xl6ParseResultFree(&reference);
    XL6_CompletionQuery q={.path=path,.offset=firstOffset};work=(xls_parse_work){0};
    xls_syntax* cursor=xlsSyntaxBuildCursor(path,text,plain,&q,&work);CHECK(cursor&&work.full==1&&!work.reused&&verify(cursor,&q,firstOffset));xl6CompletionFree(&q);xlsTextRelease(text);
    XL6_CompletionQuery wrongOffset={.path=path,.offset=firstOffset-1};
    CHECK(!xlsSyntaxCopyCursorParse(cursor,&wrongOffset,&untouched)&&!untouched.root&&!wrongOffset.prefix&&!wrongOffset.parsed);
    const char* invalid="func edited() as int\n return \"unterminated\nend\n";work=(xls_parse_work){0};
    xls_syntax* broken=build(path,invalid,cursorAt(invalid,"unterminated"),cursor,&work);CHECK(broken&&!xlsSyntaxAccepted(broken));
    work=(xls_parse_work){0};xls_syntax* restored=build(path,source.Data,firstOffset,broken,&work);CHECK(restored&&work.full==1&&!work.reused);
    const char* unicodeCursor="// 😀\nfunc edited() as int\n return 0\nend\n";
    size_t interior=(size_t)(strstr(unicodeCursor,"😀")-unicodeCursor)+1;work=(xls_parse_work){0};
    xls_syntax* boundary=build(path,unicodeCursor,interior,restored,&work);CHECK(boundary&&!xlsSyntaxAccepted(boundary)&&work.full==1);xlsSyntaxRelease(boundary);
    xlsSyntaxRelease(restored);xlsSyntaxRelease(broken);xlsSyntaxRelease(cursor);xlsSyntaxRelease(plain);xlsSyntaxRelease(previous);

    /* All readers own leases; simultaneous first materialization and private
     * clone mutation may not alter shared chunks or source marker metadata. */
    text=xlsTextCreate(source.Data,source.Size,8u*1024u*1024u,NULL);CHECK(text);q=(XL6_CompletionQuery){.path=path,.offset=firstOffset};work=(xls_parse_work){0};
    xls_syntax* concurrent=xlsSyntaxBuildCursor(path,text,NULL,&q,&work);CHECK(concurrent);xl6CompletionFree(&q);xlsTextRelease(text);
    reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i)readers[i]=(reader){xlsSyntaxRetain(concurrent),firstOffset,0};xlsSyntaxRelease(concurrent);
    for(size_t i=0;i<4;++i){threads[i]=xrtThreadCreate(parallel,&readers[i],0);CHECK(threads[i]);}
    for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].passed);xrtThreadDestroy(threads[i]);}
    /* The oldest view still has its original global cursor range. */
    q=(XL6_CompletionQuery){.path=path,.offset=firstOffset};XL6_ParseResult oracle;xl6ParseResultInit(&oracle);CHECK(xl6ParseSourceWithCompletion(source.Data,path,&oracle,&q)&&verify(first,&q,firstOffset));
    xl6CompletionFree(&q);xl6ParseResultFree(&oracle);xlsSyntaxRelease(first);xrtStrBufFree(&source);xrtFree(path);return 1;
}
int main(void)
{ xrtAtomic32Init(&checks,0);if(!run())return 1;printf("analysis_cursor_syntax_test passed: %u checks\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0; }
