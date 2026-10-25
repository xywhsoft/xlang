/* Diagnostic-only replacement of the new optional vector helper. Original
 * compiler andproduction objects contain no fault controls. */
#include <xrt.h>
#include <stdio.h>
static size_t symbolCalls,symbolFailAt,symbolHits;
static int symbolFail(void){++symbolCalls;if(symbolFailAt && symbolCalls==symbolFailAt){++symbolHits;return 1;}return 0;}
static bool symbolArrayInit(xarray* a,size_t size){return !symbolFail() && xrtArrayInit(a,size);}
static bool symbolArrayPush(xarray* a,const void* value){return !symbolFail() && xrtArrayPush(a,value);}
static bool symbolArrayResize(xarray* a,size_t size){return !symbolFail() && xrtArrayResize(a,size);}
static bool symbolArraySort(xarray* a,int (*compare)(const void*,const void*)){return !symbolFail() && xrtArraySort(a,compare);}
#define xrtArrayInit symbolArrayInit
#define xrtArrayPush symbolArrayPush
#define xrtArrayResize symbolArrayResize
#define xrtArraySort symbolArraySort
#include XLS_SYMBOL_FIXTURE
#undef xrtArrayInit
#undef xrtArrayPush
#undef xrtArrayResize
#undef xrtArraySort
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main unusedNormalFixtureMain
#include "analysis_ir_memo_test.c"
#undef main
#pragma GCC diagnostic pop
static int failures(unsigned edit)
{
    char* path=xrtPathAbs("symbol-fault-中文.xl");char* first=body(0),*changed=body(edit);CHECK(path && first && changed);
    xls_semantic_check *source=checked(path,first),*current=checked(path,changed);CHECK(source && current && xlsSemanticCheckInfo(source).accepted && xlsSemanticCheckInfo(current).accepted);
    XL6_DiagList diags;xl6DiagInit(&diags);xls_ir_result* base=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,NULL,32u*1024u*1024u);CHECK(base && !diags.count);
    char* expected=dump(xlsSemanticCheckIr(current));CHECK(expected);
    symbolCalls=symbolFailAt=symbolHits=0;
    xls_ir_result* normal=xlsFrontendIrResultBuild(xlsSemanticCheckAst(current),&diags,base,32u*1024u*1024u);CHECK(normal && !diags.count);
    size_t gates=symbolCalls;CHECK(gates>32);xlsFrontendIrResultDestroy(normal);
    for(size_t fail=1;fail<=gates;++fail){
        symbolCalls=symbolHits=0;symbolFailAt=fail;
        xls_ir_result* result=xlsFrontendIrResultBuild(xlsSemanticCheckAst(current),&diags,base,32u*1024u*1024u);symbolFailAt=0;
        CHECK(result && !diags.count && symbolHits==1);char* actual=dump(xlsFrontendIrResultModule(result));CHECK(actual && !strcmp(actual,expected));xrtFree(actual);xlsFrontendIrResultDestroy(result);
        CHECK(xlsSemanticCheckValidate(source) && xlsSemanticCheckValidate(current));
    }
    printf("symbol vectors edit=%u fault gates=%zu;original full IR on every optional failure\n",edit,gates);
    xrtFree(expected);xlsFrontendIrResultDestroy(base);xlsSemanticCheckDestroy(source);xlsSemanticCheckDestroy(current);xl6DiagFree(&diags);xrtFree(first);xrtFree(changed);xrtFree(path);return 1;
}
int main(void){xrtAtomic32Init(&checks,0);if(!failures(0) || !failures(4))return 1;printf("analysis_symbol_fault_test passed: %u checks\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
