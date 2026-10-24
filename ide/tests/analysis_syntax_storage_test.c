#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
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
#define xlsSyntaxPath privatePath
#define xlsSyntaxText privateText
#define xlsSyntaxCopyParse privateCopy
#define xlsSyntaxCopyCursorParse privateCopyCursor
#define xlsSyntaxAst privateAst
#define xlsSyntaxVisit privateVisit
#define xlsSyntaxDiagnosticCount privateDiagnosticCount
#define xlsSyntaxDiagnostic privateDiagnostic
#include "analysis/syntax.c"
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
#undef xlsSyntaxPath
#undef xlsSyntaxText
#undef xlsSyntaxCopyParse
#undef xlsSyntaxCopyCursorParse
#undef xlsSyntaxAst
#undef xlsSyntaxVisit
#undef xlsSyntaxDiagnosticCount
#undef xlsSyntaxDiagnostic
static unsigned checks;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"analysis_syntax_storage_test:%d: %s\n",__LINE__,#e);return 0;}}while(0)
static xls_syntax* adopt(const char* path,xls_text* text)
{
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    int okay=xl6ParseSource(xlsTextData(text),path,&parsed);
    xls_syntax* syntax=okay ? privateAdopt(path,text,&parsed.root) : NULL;
    xl6ParseResultFree(&parsed);return syntax;
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
int main(void){if(!run())return 1;printf("analysis_syntax_storage_test passed: %u checks; real adopted/shared chunks,exact/short budgets,storage-map faults and independent generations\n",checks);return 0;}
