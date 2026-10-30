#include "src/stdlib/xl6_stdlib.h"
#include "analysis/frontend_raw_body.h"
int xlsReferenceStdlibExpandModule(XL6_AstNode*,XL6_DiagList*);
#define XLS_STDLIB_ORIGINAL_ORACLE xlsReferenceStdlibExpandModule
#define main xlsStdlibScanMain
#include "analysis_stdlib_scan_test.c"
#undef main

static int currentStdlib(void)
{
    const char* cases[]={
        "func query(X509Name value)\nend\n",
        "func query(X509TrustAnchor value)\nend\n",
        "func query(HttpHeaders value)\nend\n",
        "func query(WsMaskPolicy value)\nend\n",
        "func query(TlsRole value)\nend\n",
        "func query(NetSocket value)\nend\n",
        "func query(TcpListener value)\nend\n",
        "func query(TemplateArgument value)\nend\n",
        "func query(AesGcm value)\nend\n",
        "import concurrent\nfunc query(Executor value)\nend\n",
        "import memory\nfunc query(ObjectPool value)\nend\n",
        "func query()\n memory.stats()\nend\n",
        "import concurrent\nfunc query(var input)\n input.recvAsyncUntilCancel()\nend\n",
        "func query(var input)\n input.webSocketAsync()\nend\n",
        "func query(var input)\n input.readFromNetBuffer()\n input.recvOwnedAsyncUntilCancel()\nend\n",
        "func query() as uint32\n return crc32.checksum(\"123456789\")\nend\n"
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i){printf("current stdlib corpus %zu\n",i);CHECK(corpus(cases[i],0));}
    const char* features[]={"HttpHeaders","WsMaskPolicy","X509Name","X509TrustAnchor","TlsRole","NetSocket","recvAsyncUntilCancel","webSocketAsync","memory","\"concurrent\""};
    for(size_t i=0;i<sizeof(features)/sizeof(*features);++i)CHECK(xlsRawBodyFeatureSpelling(features[i]));
    CHECK(!xlsRawBodyFeatureSpelling("ordinary_local_value") && !xlsRawBodyFeatureSpelling(NULL));
    return 1;
}
static int rawObservations(void)
{
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    CHECK(xl6ParseSource("func ordinary() as int\n return 7\nend\nfunc active()\n print(7)\nend\n","current-raw-stdlib.xl",&parsed) && parsed.root->childCount==2);
    xls_frontend_context_scope call;xlsFrontendContextBegin(&call,NULL);
    xls_raw_scope scope;xlsRawBodyScopeBegin(&scope,NULL,1024*1024);CHECK(scope.ready);
    for(size_t i=0;i<2;++i){XL6_AstNode* function=parsed.root->children[i];xls_raw_binding binding={.function=function};
        CHECK(xrtArrayPush(&scope.bindings,&binding) && xrtMapSet(&scope.functions,(xbytesview){(const unsigned char*)&function,sizeof(function)},&i));}
    CHECK(xl6StdlibExpandModule(parsed.root,&parsed.diags));
    const xls_raw_binding* ordinary=xrtArrayConstGet(&scope.bindings,0);const xls_raw_binding* active=xrtArrayConstGet(&scope.bindings,1);
    CHECK(ordinary->stdlibVisited && !ordinary->stdlibMarks && active->stdlibVisited && active->stdlibMarks && !scope.stdlibFrame);
    xlsRawBodyScopeEnd(&scope);CHECK(!xlsRawBodyScopeCurrent() && xlsFrontendContextEnd(&call));xl6ParseResultFree(&parsed);return 1;
}
int main(void)
{
    if(xlsStdlibScanMain() || !currentStdlib() || !rawObservations())return 1;
    printf("analysis_stdlib_current_test passed: %u checks; original current expansion,full AST/diagnostics,current query families and old concurrent scope corpus\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));
    return 0;
}
