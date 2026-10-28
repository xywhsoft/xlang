#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include "analysis/syntax_node_storage.h"
#include "analysis/cursor_word_view.h"
/* Keep every original generic source-map construction/failure seam explicit.
 * Production fused binding is independently exercised below via public APIs. */
static int legacySourceWithoutSize(XL6_AstNode* node,const char* path,size_t* bytes)
{*bytes=0;return xl6AstSetSourceTree(node,path);}
#define xlsAstSetSourceTreeSized legacySourceWithoutSize
static size_t operations,failAt,maps;
static int invalidLifetime;
static int allowed(void) {return ++operations!=failAt;}
static bool mapInit(xmap* map,size_t size)
{if(!allowed())return false;bool okay=xrtMapInit(map,size);if(okay)++maps;return okay;}
static bool mapSet(xmap* map,xbytesview key,const void* value)
{return allowed() && xrtMapSet(map,key,value);}
static void mapUnit(xmap* map)
{if(map->ValueSize){if(maps)--maps;else invalidLifetime=1;}xrtMapUnit(map);}
#define xrtMapInit mapInit
#define xrtMapSet mapSet
#define xrtMapUnit mapUnit
#define xlsSyntaxCacheBytes privateCacheBytes
#define xlsSyntaxRetain privateRetain
#define xlsSyntaxRelease privateRelease
#define xlsSyntaxBuild privateBuild
#define xlsSyntaxAdoptOrdinary privateAdopt
#define xlsSyntaxTryFunction privateFunction
#define xlsSyntaxBuildCursor privateCursor
#define xlsSyntaxAccepted privateAccepted
#define xlsSyntaxIsCursor privateIsCursor
#define xlsSyntaxCursorMatches privateCursorMatches
#define xlsSyntaxPath privatePath
#define xlsSyntaxText privateText
#define xlsSyntaxCopyParse privateCopy
#define xlsSyntaxCopyCursorParse privateCopyCursor
#define xlsSyntaxAst privateAst
#define xlsSyntaxVisit privateVisit
#define xlsSyntaxDiagnosticCount privateDiagnosticCount
#define xlsSyntaxDiagnostic privateDiagnostic
/* Isolate the complete current syntax TU,including optional raw producers. */
#define xlsCursorWordMapRange privateExtraxlsCursorWordMapRange
#define xlsRawBodyProjectionAst privateExtraxlsRawBodyProjectionAst
#define xlsRawBodyProjectionCreate privateExtraxlsRawBodyProjectionCreate
#define xlsRawBodyProjectionCreatePolicy privateExtraxlsRawBodyProjectionCreatePolicy
#define xlsRawBodyProjectionHeaders privateExtraxlsRawBodyProjectionHeaders
#define xlsRawBodyProjectionRelease privateExtraxlsRawBodyProjectionRelease
#define xlsRawBodyProjectionRetain privateExtraxlsRawBodyProjectionRetain
#define xlsRawBodyProjectionTakeHeaderParse privateExtraxlsRawBodyProjectionTakeHeaderParse
#define xlsRawBodyProjectionWork privateExtraxlsRawBodyProjectionWork
#define xlsRawBodySyntaxCopy privateExtraxlsRawBodySyntaxCopy
#define xlsRawBodySyntaxRegister privateExtraxlsRawBodySyntaxRegister
#define xlsRawUnitBytes privateExtraxlsRawUnitBytes
#define xlsRawUnitNode privateExtraxlsRawUnitNode
#define xlsRawUnitRelease privateExtraxlsRawUnitRelease
#define xlsRawUnitRetain privateExtraxlsRawUnitRetain
#define xlsSyntaxCaptureSignature privateExtraxlsSyntaxCaptureSignature
#define xlsSyntaxCopySignatureParse privateExtraxlsSyntaxCopySignatureParse
#define xlsSyntaxCursorRetainedBytes privateExtraxlsSyntaxCursorRetainedBytes
#define xlsSyntaxCursorWordProof privateExtraxlsSyntaxCursorWordProof
#define xlsSyntaxFunctionHeader privateExtraxlsSyntaxFunctionHeader
#define xlsSyntaxSignatureCacheBytes privateExtraxlsSyntaxSignatureCacheBytes
#define xlsSyntaxSignatureMatches privateExtraxlsSyntaxSignatureMatches
#define xlsSyntaxTrySignatureFunction privateExtraxlsSyntaxTrySignatureFunction
#include "analysis/syntax.c"
#undef xlsCursorWordMapRange
#undef xlsRawBodyProjectionAst
#undef xlsRawBodyProjectionCreate
#undef xlsRawBodyProjectionCreatePolicy
#undef xlsRawBodyProjectionHeaders
#undef xlsRawBodyProjectionRelease
#undef xlsRawBodyProjectionRetain
#undef xlsRawBodyProjectionTakeHeaderParse
#undef xlsRawBodyProjectionWork
#undef xlsRawBodySyntaxCopy
#undef xlsRawBodySyntaxRegister
#undef xlsRawUnitBytes
#undef xlsRawUnitNode
#undef xlsRawUnitRelease
#undef xlsRawUnitRetain
#undef xlsSyntaxCaptureSignature
#undef xlsSyntaxCopySignatureParse
#undef xlsSyntaxCursorRetainedBytes
#undef xlsSyntaxCursorWordProof
#undef xlsSyntaxFunctionHeader
#undef xlsSyntaxSignatureCacheBytes
#undef xlsSyntaxSignatureMatches
#undef xlsSyntaxTrySignatureFunction

#undef xrtMapInit
#undef xrtMapSet
#undef xrtMapUnit
#undef xlsSyntaxCacheBytes
#undef xlsSyntaxRetain
#undef xlsSyntaxRelease
#undef xlsSyntaxBuild
#undef xlsSyntaxAdoptOrdinary
#undef xlsSyntaxTryFunction
#undef xlsSyntaxBuildCursor
#undef xlsSyntaxAccepted
#undef xlsSyntaxIsCursor
#undef xlsSyntaxCursorMatches
#undef xlsSyntaxPath
#undef xlsSyntaxText
#undef xlsSyntaxCopyParse
#undef xlsSyntaxCopyCursorParse
#undef xlsSyntaxAst
#undef xlsSyntaxVisit
#undef xlsSyntaxDiagnosticCount
#undef xlsSyntaxDiagnostic
#undef xlsAstSetSourceTreeSized
#include "analysis_ast_same.h"
static unsigned checks;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"analysis_syntax_storage_test:%d: %s\n",__LINE__,#e);return 0;}}while(0)
static xls_syntax* adopt(const char* path,xls_text* text)
{
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    int okay=xl6ParseSource(xlsTextData(text),path,&parsed);
    xls_syntax* syntax=okay ? privateAdopt(path,text,&parsed.root) : NULL;
    xl6ParseResultFree(&parsed);return syntax;
}
/* Independent allocation formula,including alternating identities with the
 * same path bytes. Source deduplication must stay by identity,not path text. */
static int exactSourceBytes(void)
{
    XL6_AstNode nodes[7]={0};XL6_AstNode* children[8]={&nodes[1],&nodes[2],&nodes[3],&nodes[4],&nodes[5]};
    char label[]="/memory/shared-名字.xl",doc[]="é";unsigned char literal[]={0,0xfe,0};
    XL6_AstSource a={.path=label},b={.path=label},c={0};
    for(size_t i=0;i<7;++i)nodes[i].source=&a;
    nodes[0].children=children;nodes[0].childCount=5;nodes[0].childCapacity=8;nodes[0].text="module";nodes[0].doc="doc";
    nodes[1].text="A";nodes[2].doc=doc;nodes[3].source=&b;nodes[3].literalData=literal;nodes[3].literalSize=sizeof(literal);
    nodes[4].inferredType=&nodes[6];nodes[5].text="";nodes[6].source=&c;
    const size_t payload=sizeof(nodes)+sizeof(children)+sizeof("module")+sizeof("doc")+sizeof("A")+sizeof(doc)+sizeof("")+sizeof(literal)+1;
    size_t expected=payload+3*sizeof(a)+2*sizeof(label),bytes=0;
    CHECK(cacheTreeBytes(nodes,SIZE_MAX,&bytes) && bytes==expected);
    CHECK(cacheTreeBytes(nodes,expected,&bytes) && bytes==expected);
    CHECK(!cacheTreeBytes(nodes,expected-1,&bytes) && !bytes);
    for(size_t i=0;i<7;++i)nodes[i].source=&a;
    expected=payload+sizeof(a)+sizeof(label);
    CHECK(cacheTreeBytes(nodes,expected,&bytes) && bytes==expected);
    CHECK(!cacheTreeBytes(nodes,expected-1,&bytes) && !bytes);
    for(size_t i=0;i<7;++i)nodes[i].source=NULL;
    CHECK(cacheTreeBytes(nodes,payload,&bytes) && bytes==payload);
    CHECK(!cacheTreeBytes(nodes,payload-1,&bytes) && !bytes);
    nodes[0].xlsStorage=(void*)1;
    CHECK(!cacheTreeBytes(nodes,SIZE_MAX,&bytes) && !bytes);
    CHECK(cacheTreeBytes(NULL,0,&bytes) && !bytes);
    CHECK(!maps && !invalidLifetime);return 1;
}
/* These two actual native allocation seams have the same order as the
 * original source setter. Active only around the new setter under test. */
static size_t sourceAllocation,sourceFail;
void* __real_xrtCalloc(size_t,size_t);
char* __real_xrtStrDupN(const char*,size_t);
void* __wrap_xrtCalloc(size_t count,size_t size)
{if(sourceFail && ++sourceAllocation==sourceFail)return NULL;return __real_xrtCalloc(count,size);}
char* __wrap_xrtStrDupN(const char* text,size_t size)
{if(sourceFail && ++sourceAllocation==sourceFail)return NULL;return __real_xrtStrDupN(text,size);}
static int fusedSourceStorage(void)
{
    const char* source="/// 文档😀\nfunc first(int value) as int\n string text=\"a\\0b\"\n return value+1\nend\nfunc second(int value) as int\n return first(value)\nend\nfunc main(...) as int\n int completed=second(7)\n return completed\nend\n";
    const char* original="/memory/original-类型😀.xl",*target="/memory/rebound-名字😀.xl";
    XL6_ParseResult a,b;xl6ParseResultInit(&a);xl6ParseResultInit(&b);
    CHECK(xl6ParseSource(source,original,&a) && xl6ParseSource(source,original,&b) && sameAst(a.root,b.root));
    const XL6_AstSource* initial=a.root->source;size_t initialRefs=initial->references;
    for(size_t failure=1;failure<=2;++failure){
        size_t bytes=123;sourceAllocation=0;sourceFail=failure;int okay=xlsAstSetSourceTreeSized(a.root,target,&bytes);sourceFail=0;
        CHECK(!okay && !bytes && sourceAllocation==failure && a.root->source==initial && initial->references==initialRefs && sameAst(a.root,b.root));
    }
    size_t measured=0,reference=0;
    CHECK(xlsAstSetSourceTreeSized(a.root,target,&measured) && measured && cacheTreeBytes(a.root,SIZE_MAX,&reference) && measured==reference);
    CHECK(xl6AstSetSourceTree(b.root,target) && sameAst(a.root,b.root) && a.root->source->references==b.root->source->references);
    CHECK(cacheTreeBytes(a.root,measured,&reference) && reference==measured);
    CHECK(!cacheTreeBytes(a.root,measured-1,&reference) && !reference);
    CHECK(xlsAstSetSourceTreeSized(a.root,original,NULL) && xl6AstSetSourceTree(b.root,original) && sameAst(a.root,b.root));
    measured=123;CHECK(xlsAstSetSourceTreeSized(NULL,target,&measured) && !measured);
    measured=123;CHECK(xlsAstSetSourceTreeSized(a.root,NULL,&measured) && !measured && sameAst(a.root,b.root));
    /* The original traversal leaves semanticDeclaredType outside its edge
     * set. It must keep that behavior and decline the optional size proof. */
    a.root->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);
    b.root->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);
    CHECK(a.root->semanticDeclaredType && b.root->semanticDeclaredType &&
        xl6AstSetSourceTree(a.root->semanticDeclaredType,"/declared.xl") && xl6AstSetSourceTree(b.root->semanticDeclaredType,"/declared.xl"));
    measured=123;CHECK(xlsAstSetSourceTreeSized(a.root,target,&measured) && !measured && xl6AstSetSourceTree(b.root,target) && sameAst(a.root,b.root));
    CHECK(cacheTreeBytes(a.root,SIZE_MAX,&reference) && reference>0 && !strcmp(xl6AstSourcePath(a.root->semanticDeclaredType),"/declared.xl"));
    xl6ParseResultFree(&a);xl6ParseResultFree(&b);
    XL6_AstNode* deep=xl6AstNew(XL6_AST_TYPE,NULL);CHECK(deep);XL6_AstNode* last=deep;
    for(size_t i=0;i<514;++i){last->inferredType=xl6AstNew(XL6_AST_TYPE,NULL);CHECK(last->inferredType);last=last->inferredType;}
    measured=123;CHECK(xlsAstSetSourceTreeSized(deep,target,&measured) && !measured && !strcmp(xl6AstSourcePath(last),target));
    CHECK(!cacheTreeBytes(deep,SIZE_MAX,&reference) && !reference);xl6AstFree(deep);
    /* Actual default cursor construction publishes complete sizes,then the
     * public word-owner budget reuses them. Recompute every forest with the
     * separate complete generic map walk,including the untouched module root. */
    xls_text* text=xlsTextCreate(source,strlen(source),1024u*1024u,NULL);CHECK(text);
    char* canonical=xrtPathAbs(original);CHECK(canonical);
    XL6_CompletionQuery query={.path=canonical,.offset=(size_t)(strstr(source,"return completed")-source)+strlen("return compl")};xls_parse_work work={0};
    xls_syntax* syntax=xlsSyntaxBuildCursor(canonical,text,NULL,&query,&work);xl6CompletionFree(&query);CHECK(syntax && xlsSyntaxAccepted(syntax) && syntax->units.Count==3);
    size_t total=sizeof(*syntax)+xlsTextStorageBytes(text)+strlen(syntax->path)+1+sizeof(*syntax->view)+syntax->units.Capacity*sizeof(placement),tree=0;
    CHECK(cacheTreeBytes(syntax->parse.root,SIZE_MAX,&tree));total+=tree;
    for(size_t i=0;i<syntax->units.Count;++i){
        const syntax_unit* unit=((const placement*)xrtArrayConstGet(&syntax->units,i))->unit;
        CHECK(unit->storageKnown && cacheTreeBytes(unit->node,SIZE_MAX,&tree) && unit->storageBytes==tree);total+=sizeof(*unit)+tree;
    }
    CHECK(xlsSyntaxCursorRetainedBytes(syntax,SIZE_MAX,&measured) && measured==total);
    CHECK(xlsSyntaxCursorRetainedBytes(syntax,total,&measured) && measured==total);
    CHECK(!xlsSyntaxCursorRetainedBytes(syntax,total-1,&measured) && !measured);
    xlsSyntaxRelease(syntax);xlsTextRelease(text);xrtFree(canonical);CHECK(!maps && !invalidLifetime && !sourceFail);
    return 1;
}
static int run(void)
{
    const char* path="/memory/module-storage.xl";
    const char* source="func first() as int\r\n return 1\r\nend\r\nfunc changed() as int\r\n return 2\r\nend\r\nfunc last() as int\r\n return 3\r\nend\r\n";
    const char* changed="func first() as int\r\n return 1\r\nend\r\nfunc changed() as int\r\n // 中文😀\r\n return 12345\r\nend\r\nfunc last() as int\r\n return 3\r\nend\r\n";
    xls_text *text=xlsTextCreate(source,strlen(source),1024*1024,NULL),*next=xlsTextCreate(changed,strlen(changed),1024*1024,NULL);CHECK(text && next);
    size_t calls=0;
    for(size_t failure=0;failure<=calls;++failure) {
        operations=0;failAt=failure;xls_syntax* syntax=adopt(path,text);
        if(!failure){calls=operations;CHECK(syntax && calls>=6);}else CHECK(!syntax);
        CHECK(!maps && !invalidLifetime);privateRelease(syntax);
        operations=failAt=0;syntax=adopt(path,text);CHECK(syntax && !maps);privateRelease(syntax);
    }
    size_t adoptCalls=calls;operations=failAt=0;xls_syntax* base=adopt(path,text);CHECK(base && base->units.Count==3);
    calls=0;
    for(size_t failure=0;failure<=calls;++failure) {
        xls_parse_work work={0};operations=0;failAt=failure;xls_syntax* update=privateFunction(path,next,base,&work);
        if(!failure) {
            calls=operations;CHECK(update && calls>=2 && !work.full && work.fragments==1 && work.reused==2);
            for(size_t i=0;i<3;++i) {
                const syntax_unit* old=((const placement*)xrtArrayConstGet(&base->units,i))->unit;
                const syntax_unit* now=((const placement*)xrtArrayConstGet(&update->units,i))->unit;
                CHECK(i==1 ? old!=now : old==now);CHECK(now->storageKnown && now->storageBytes>sizeof(*now->node));
            }
        } else CHECK(!update && work.fragments==1 && !work.reused);
        CHECK(!maps && !invalidLifetime);privateRelease(update);
        operations=failAt=0;work=(xls_parse_work){0};update=privateFunction(path,next,base,&work);CHECK(update && work.reused==2 && !maps);privateRelease(update);
    }
    size_t updateCalls=calls;calls=0;
    for(size_t failure=0;failure<=calls;++failure) {
        size_t bytes=0;operations=0;failAt=failure;int okay=privateCacheBytes(base,SIZE_MAX,&bytes);
        if(!failure){calls=operations;CHECK(okay && bytes>xlsTextStorageBytes(text));size_t same=0;CHECK(privateCacheBytes(base,bytes,&same) && same==bytes);CHECK(!privateCacheBytes(base,bytes-1,&same) && !same);}
        else CHECK(!okay && !bytes);
        CHECK(!maps && !invalidLifetime);
        operations=failAt=0;CHECK(privateCacheBytes(base,SIZE_MAX,&bytes) && !maps);
    }
    size_t measureCalls=calls;operations=failAt=0;xls_parse_work work={0};xls_syntax* update=privateFunction(path,next,base,&work);CHECK(update);privateRelease(base);base=NULL;
    size_t bytes=0;CHECK(privateCacheBytes(update,SIZE_MAX,&bytes));XL6_ParseResult owned;xl6ParseResultInit(&owned);CHECK(privateCopy(update,&owned) && owned.root && !strcmp(xl6AstSourcePath(owned.root),path));xl6ParseResultFree(&owned);
    CHECK(!strcmp(xlsTextData(privateText(update)),changed));
    /* A lazy materialized view is deliberately not allowed in this bounded
     * private cache; ordinary clients can still use it outside that cache. */
    CHECK(privateAst(update) && !privateCacheBytes(update,SIZE_MAX,&bytes) && !bytes);
    privateRelease(update);CHECK(!privateCacheBytes(NULL,SIZE_MAX,&bytes) && !xlsTextStorageBytes(NULL));
    CHECK(!maps && !invalidLifetime && !strcmp(xlsTextData(text),source));xlsTextRelease(text);xlsTextRelease(next);
    printf("Syntax storage seams: adopt=%zu,update=%zu,measure=%zu; actual map calls independently failed/recovered\n",adoptCalls,updateCalls,measureCalls);return 1;
}
int main(void){if(!exactSourceBytes() || !fusedSourceStorage() || !run())return 1;printf("analysis_syntax_storage_test passed: %u checks; real adopted/shared chunks,exact/short budgets,storage-map faults and independent generations\n",checks);return 0;}
