#include <xrt.h>
#include "analysis/context.h"
#include <stdio.h>
#include <string.h>
static size_t calls,failAt,live;
static int fixtureOp(void) { return ++calls!=failAt; }
static void* fixtureCalloc(size_t count,size_t size) {if(!fixtureOp()) return NULL;void* p=xrtCalloc(count,size);if(p) ++live;return p;}
static char* fixtureDup(const char* text) {if(!fixtureOp()) return NULL;char* p=xrtStrDup(text);if(p) ++live;return p;}
static void fixtureFree(void* p) {if(p) --live;xrtFree(p);}
static bool fixtureMapInit(xmap* map,size_t size) {return fixtureOp() && xrtMapInit(map,size);}
static bool fixtureMapDrop(xmap* map,xmapdrop drop,void* user) {return fixtureOp() && xrtMapSetDrop(map,drop,user);}
static bool fixtureMapSet(xmap* map,xbytesview key,const void* value) {return fixtureOp() && xrtMapSet(map,key,value);}
static bool fixtureArrayInit(xarray* array,size_t size) {return fixtureOp() && xrtArrayInit(array,size);}
static bool fixtureArrayPush(xarray* array,const void* value) {return fixtureOp() && xrtArrayPush(array,value);}
#define xrtCalloc fixtureCalloc
#define xrtStrDup fixtureDup
#define xrtFree fixtureFree
#define xrtMapInit fixtureMapInit
#define xrtMapSetDrop fixtureMapDrop
#define xrtMapSet fixtureMapSet
#define xrtArrayInit fixtureArrayInit
#define xrtArrayPush fixtureArrayPush
#define xlsFrontendContextValid privateContextValid
#define xlsFrontendContextsEqual privateContextsEqual
#define xlsFrontendContextBegin privateContextBegin
#define xlsFrontendContextEnd privateContextEnd
#define xlsFrontendContextCurrent privateContextCurrent
#define xlsFrontendContextPackagePath privateContextPackagePath
#define xlsFrontendContextAbsolute privateContextAbsolute
#define xlsFrontendNamespaceReady privateNamespaceReady
#define xlsFrontendNamespaceStart privateNamespaceStart
#define xlsFrontendNamespaceAdd privateNamespaceAdd
#define xlsFrontendNamespaceActive privateNamespaceActive
#define xlsFrontendNamespaceCount privateNamespaceCount
#define xlsFrontendNamespaceName privateNamespaceName
#include "analysis/frontend_context.c"
static unsigned checks;
#define CHECK(call) do {++checks;if(!(call)){fprintf(stderr,"analysis_context_fault_test:%d: %s\n",__LINE__,#call);return 1;}}while(0)
static int entries(void)
{return privateNamespaceStart() && privateNamespaceAdd("internal-one","first") && privateNamespaceAdd("internal-one","alias") && privateNamespaceAdd("internal-two","other");}
int main(void)
{
    xls_frontend_context_scope scope;
    CHECK(!privateNamespaceReady() && !privateNamespaceActive());
    privateContextBegin(&scope,NULL);CHECK(entries());size_t operations=calls;
    CHECK(privateNamespaceCount("internal-one")==2 && !strcmp(privateNamespaceName("internal-one",1),"alias"));
    CHECK(!privateNamespaceName("internal-one",2) && !privateNamespaceName("absent",0));
    CHECK(privateNamespaceAdd("internal-one","first") && privateNamespaceCount("internal-one")==2);
    CHECK(privateContextEnd(&scope) && !live && !privateNamespaceReady());
    for(size_t i=1;i<=operations;++i) {
        calls=0;failAt=i;privateContextBegin(&scope,NULL);CHECK(!entries());
        CHECK(!privateContextEnd(&scope) && (scope.failed&2) && !live && !privateNamespaceReady());
    }
    calls=0;failAt=0;privateContextBegin(&scope,NULL);CHECK(entries());
    xls_frontend_context_scope nested;privateContextBegin(&nested,NULL);CHECK(privateNamespaceStart() && privateNamespaceAdd("inner","only"));
    CHECK(privateNamespaceCount("internal-one")==0 && privateContextEnd(&nested));
    CHECK(privateNamespaceCount("internal-one")==2 && privateContextEnd(&scope) && !live);
    privateContextBegin(&scope,NULL);CHECK(privateNamespaceStart());int okay=1;
    for(unsigned i=0;i<4096 && okay;++i) {char key[32],display[32];snprintf(key,sizeof(key),"private-%u",i);snprintf(display,sizeof(display),"visible_%u",i);okay=privateNamespaceAdd(key,display);}
    CHECK(okay && privateNamespaceAdd("private-0","visible_0"));
    CHECK(!privateNamespaceAdd("too-many","extra") && !privateContextEnd(&scope) && !live);
    xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry="C:/scope/main.xl",.directory="C:/scope",.analysisProfile="fixture",.targetKind="exe",.packagePath="packages"};
    xls_context* context=xlsContextCreate(&spec,4096,NULL);CHECK(context);
    calls=0;failAt=1;privateContextBegin(&scope,context);CHECK(!privateContextPackagePath());
    CHECK(!privateContextEnd(&scope) && (scope.failed&1) && !live);xlsContextRelease(context);
    printf("analysis_context_fault_test passed: %u checks; %zu allocation/container failure seams, alias ownership, nested scope and exact capacity\n",checks,operations);return 0;
}
