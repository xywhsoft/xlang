#define main nativeBorrowReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/typed_module_internal.h"
#include "analysis/function_unit_internal.h"
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static int extras(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    if(a->semanticReceiverSymbolId!=b->semanticReceiverSymbolId || a->compiledPackageError!=b->compiledPackageError ||
       a->compiledPackageRoot!=b->compiledPackageRoot || !sameText(a->compiledPackageLink,b->compiledPackageLink))return 0;
    if(!extras(a->inferredType,b->inferredType) || !extras(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!extras(a->children[i],b->children[i]))return 0;
    return 1;
}
static int complete(xls_semantic_check* result,const xls_analysis_document* docs,size_t count)
{
    CHECK(freshInContext(result,NULL,docs[0].path,docs,count));
    xls_semantic_input inputs[64];for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    xls_semantic_check* fresh=xlsSemanticCheckCreate(docs[0].path,inputs,count,1,NULL,NULL,NULL,0);CHECK(fresh);
    CHECK(extras(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)));xlsSemanticCheckDestroy(fresh);return 1;
}
static char* body(unsigned value,int region,int local,int move)
{
    char* base=source(value,0,1);if(!base)return NULL;const char* at=strstr(base," return changed\r\n");xstrbuf out={0};
    int okay=at && xrtStrBufAppend(&out,(xstrview){base,3}) &&
        (!move || xrtStrBufAppend(&out,xrtStrView("// moved 中文😀\r\n"))) &&
        xrtStrBufAppend(&out,(xstrview){base+3,(size_t)(at-base)-3}) &&
        xrtStrBufAppendFormat(&out,"%-16s\r\n%-16s\r\n",region ? " if true then" : local ? " int extra=1" : "",region ? " end" : "") &&
        xrtStrBufAppend(&out,xrtStrView(at));
    char* result=okay ? xrtStrDup(out.Data) : NULL;xrtStrBufFree(&out);xrtFree(base);return result;
}
static size_t equalFunctions(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b || a->childCount!=b->childCount)return 0;
    size_t count=0;for(size_t i=0;i<a->childCount;++i)if(a->children[i] && a->children[i]==b->children[i] && a->children[i]->kind==XL6_AST_FUNC)++count;
    return count;
}
static int editing(void)
{
    char* path=xrtPathAbs("native-borrow-中文😀.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
    xls_analysis_document doc={"entry",path,NULL,1,1};xls_semantic_check* previous=NULL;
    for(unsigned v=0;v<7;++v){
        char* text=body(v+1,v==4,v==5,v==6);CHECK(text);
        xls_semantic_check* result=update(session,&doc,1,text);CHECK(result && xlsSemanticCheckInfo(result).accepted && complete(result,&doc,1));
        xls_sem_native_work work=xlsSemanticCheckNativeWork(result);xls_ast_materialize_work copies=xlsSemanticCheckMaterializationWork(result);
        printf("native-v%u retained=%llu groups=%llu reused=%llu retries=%llu bytes=%zu copied=%llu\n",v,
            (unsigned long long)work.retained,(unsigned long long)work.owners,(unsigned long long)work.reused,(unsigned long long)work.retries,work.bytes,(unsigned long long)copies.nodes);
        CHECK(work.bytes<=32u*1024u*1024u);
        if(!v)CHECK(work.retained>=60 && !work.reused && !work.retries);
        if(v>=1 && v<=3){CHECK(work.reused>=60 && !work.retries);CHECK(equalFunctions(xlsSemanticCheckAst(previous),xlsSemanticCheckAst(result))==work.reused);}
        if(v==4 || v==5)CHECK(work.retries==1 && !work.reused);
        if(v==6)CHECK(!work.reused && !work.retries);
        xlsSemanticCheckDestroy(previous);previous=result;xrtFree(text);
    }
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(previous));
    size_t functions=0;CHECK(xlsSemanticCheckFunctionCount(previous,&functions) && functions>=60);
    char error[256];xls_function_set* set=xlsSemanticCheckFunctionsCapture(previous,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(set);
    xlsSemanticCheckDestroy(previous);CHECK(xlsFunctionSetInfo(set).functions==functions);xlsFunctionSetRelease(set);
    xlsTextRelease(doc.text);xrtFree(path);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!editing())return 1;
    printf("analysis_native_borrow_test passed:%u checks;actual ordinary native root identity,complete fresh AST/index/IR parity,region/symbol eager retries,lifetime\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
#else
int main(void){puts("analysis_native_borrow_test skipped:old native representation");return 0;}
#endif
