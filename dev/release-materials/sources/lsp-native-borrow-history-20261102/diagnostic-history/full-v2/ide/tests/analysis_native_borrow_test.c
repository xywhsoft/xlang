#define main nativeBorrowReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/typed_module_internal.h"
#include "analysis/function_unit_internal.h"
#include "analysis/ast_size.h"
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static int expectEager,expectNoPlacement,unexpectedPlacement;
int __real_xlsFrontendSemCheckMemoFinal(XL6_AstNode*,XL6_DiagList*,const char*,int,const char*,const XL6_SourceReport*,const XL6_SourceReport*,const xls_sem_memo*,xls_sem_memo**,xls_sem_work*,size_t);
int __wrap_xlsFrontendSemCheckMemoFinal(XL6_AstNode* root,XL6_DiagList* diags,const char* module,int index,const char* path,
    const XL6_SourceReport* sources,const XL6_SourceReport* oldSources,const xls_sem_memo* old,xls_sem_memo** memo,xls_sem_work* work,size_t bytes)
{
    int result=__real_xlsFrontendSemCheckMemoFinal(root,diags,module,index,path,sources,oldSources,old,memo,work,bytes);
    if(expectNoPlacement && memo && *memo && xlsFrontendSemMemoNativeWork(*memo).reused)++unexpectedPlacement;
    return result;
}
static int extras(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    if(!!a->source!=!!b->source || (a->source && (a->source->completionQuery!=b->source->completionQuery ||
       a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd)))return 0;
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
        expectNoPlacement=v>=4;unexpectedPlacement=0;
        char* text=body(v+1,v==4,v==5,v==6);CHECK(text);
        xls_semantic_check* result=update(session,&doc,1,text);CHECK(result && xlsSemanticCheckInfo(result).accepted && complete(result,&doc,1));
        xls_sem_native_work work=xlsSemanticCheckNativeWork(result);xls_ast_materialize_work copies=xlsSemanticCheckMaterializationWork(result);
        printf("native-v%u retained=%llu groups=%llu reused=%llu retries=%llu bytes=%zu copied=%llu\n",v,
            (unsigned long long)work.retained,(unsigned long long)work.owners,(unsigned long long)work.reused,(unsigned long long)work.retries,work.bytes,(unsigned long long)copies.nodes);
        CHECK(work.bytes<=32u*1024u*1024u);
        CHECK(!unexpectedPlacement);
        if(!v)CHECK((expectEager || work.retained>=60) && !work.reused && !work.retries);
        if(v>=1 && v<=3){CHECK(expectEager ? !work.reused : work.reused>=60 && !work.retries);CHECK(equalFunctions(xlsSemanticCheckAst(previous),xlsSemanticCheckAst(result))==work.reused);}
        if(v==4 || v==5)CHECK((expectEager || work.retries==1) && !work.reused);
        if(v==6)CHECK(!work.reused && !work.retries);
        xlsSemanticCheckDestroy(previous);previous=result;xrtFree(text);
    }
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(previous));
    size_t functions=0;CHECK(xlsSemanticCheckFunctionCount(previous,&functions) && functions>=60);
    char error[256];xls_function_set* set=xlsSemanticCheckFunctionsCapture(previous,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(set);
    xlsSemanticCheckDestroy(previous);CHECK(xlsFunctionSetInfo(set).functions==functions);xlsFunctionSetRelease(set);
    xlsTextRelease(doc.text);xrtFree(path);return 1;
}
static int backing(void)
{
    xls_ast_storage* storage=xlsAstStorageCreate(8u*1024u*1024u);CHECK(storage);
    XL6_AstNode* function=xlsAstStorageNode(storage,SIZE_MAX);CHECK(function);function->kind=XL6_AST_FUNC;
    XL6_AstNode* spare[512];for(size_t i=0;i<512;++i){spare[i]=xlsAstStorageNode(storage,SIZE_MAX);CHECK(spare[i]);spare[i]->kind=XL6_AST_TYPE;}
    XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);xl6AstAddChild(root,function);CHECK(root->childCount==1);
    xls_typed_module* module=xlsTypedModuleTake(root);CHECK(module);
    xls_typed_function* owner=xlsTypedModuleFunction(module,0);CHECK(owner && !xlsTypedFunctionKnownBytes(owner));
    CHECK(!xlsTypedModuleMeasureFunctions(module,1,NULL,NULL) && !xlsTypedFunctionKnownBytes(owner));
    CHECK(xlsTypedModuleMeasureFunctions(module,8u*1024u*1024u,NULL,NULL));
    size_t known=xlsTypedFunctionKnownBytes(owner);uint64_t pool=xlsAstStorageWork(storage).bytes;
    CHECK(known>=pool && pool>512*sizeof(XL6_AstNode));
    CHECK(xlsTypedModuleMeasureFunctions(module,known,NULL,NULL));CHECK(!xlsTypedModuleMeasureFunctions(module,known-1,NULL,NULL));
    for(size_t i=0;i<512;++i)xl6AstFree(spare[i]);
    xlsAstStorageUnpin(storage);
    xlsTypedModuleRelease(module);CHECK(xlsTypedFunctionKnownBytes(owner)==known && xlsTypedFunctionRoot(owner)==function);xlsTypedFunctionRelease(owner);
    printf("backing snapshot=%zu,one retained node,full original pool upper bound=%llu\n",known,(unsigned long long)pool);return 1;
}
static int budgets(void)
{
    for(unsigned mode=0;mode<3;++mode){
        char* path=xrtPathAbs("native-budget.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
        xls_analysis_document doc={"entry",path,NULL,1,1};char* text=body(1,0,0,0);CHECK(text);
        xls_semantic_check* first=update(session,&doc,1,text);xrtFree(text);CHECK(first && xlsSemanticCheckInfo(first).accepted);
        size_t required=xlsSemanticCheckNativeWork(first).bytes;CHECK(required>1);
        size_t limit=mode==0 ? 0 : mode==1 ? required-1 : required;
        xls_sem_native_control outer,inner;xlsFrontendSemNativeBegin(&outer,limit);xlsFrontendSemNativeBegin(&inner,SIZE_MAX);
        CHECK(xlsFrontendSemNativeBudget()==limit);text=body(2,0,0,0);CHECK(text);
        xls_semantic_check* current=update(session,&doc,1,text);xrtFree(text);CHECK(current && xlsSemanticCheckInfo(current).accepted);
        xls_sem_native_work work=xlsSemanticCheckNativeWork(current);CHECK(work.bytes<=limit);
        if(mode==2)CHECK(work.reused>=60 && !work.retries);else CHECK(!work.reused);
        if(mode==0)CHECK(!work.retained && !work.retries);
        if(mode==1)CHECK(work.retries==1);
        xlsFrontendSemNativeEnd(&inner);xlsFrontendSemNativeEnd(&outer);CHECK(complete(current,&doc,1));
        xlsSemanticCheckDestroy(first);xlsSemanticCheckDestroy(current);xlsCheckSessionDestroy(session);xlsTextRelease(doc.text);xrtFree(path);
    }
    return 1;
}
static const XL6_AstNode* named(const XL6_AstNode* root,const char* name)
{
    for(size_t i=0;root && i<root->childCount;++i){const XL6_AstNode* n=root->children[i];
        if(n && n->kind==XL6_AST_FUNC && n->childCount && sameText(n->children[0]->text,name))return n;
    }
    return NULL;
}
static int interop(int owned)
{
    char* path=xrtPathAbs(owned ? "native-owned.xl" : "native-record.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
    xls_analysis_document doc={"entry",path,NULL,1,1};xls_semantic_check* previous=NULL;
    for(unsigned v=0;v<2;++v){
        char* base=body(v+1,0,0,0);CHECK(base);xstrbuf text={0};CHECK(xrtStrBufAppend(&text,(xstrview){base,3}));
        if(owned){
            CHECK(xrtStrBufAppend(&text,xrtStrView("extern \"text\" @no_raise func nativeText() as point\nextern \"free\" @no_raise func nativeFree(point p)\nfunc make() as cstring\n int local=0\n")));
            for(unsigned i=0;i<12;++i)CHECK(xrtStrBufAppend(&text,xrtStrView(" local=local+1\n")));
            CHECK(xrtStrBufAppend(&text,xrtStrView(" return cstring.take(nativeText(),nativeFree)\nend\n")));
        }else CHECK(xrtStrBufAppend(&text,xrtStrView("extern \"record\" struct Record @header(\"record.h\")\n c_int value\nend\nfunc recordValue(Record value) as int\n int local=int(value.value)\n")));
        if(!owned){for(unsigned i=0;i<12;++i)CHECK(xrtStrBufAppend(&text,xrtStrView(" local=local+1\n")));CHECK(xrtStrBufAppend(&text,xrtStrView(" return local\nend\n")));}
        CHECK(xrtStrBufAppend(&text,xrtStrView(base+3)));expectNoPlacement=unexpectedPlacement=0;
        xls_semantic_check* result=update(session,&doc,1,text.Data);CHECK(result && xlsSemanticCheckInfo(result).accepted && complete(result,&doc,1));
        xls_sem_native_work work=xlsSemanticCheckNativeWork(result);
        printf("interop-owned%d-v%u native=%llu rejected-writes=%llu reused=%llu retries=%llu\n",owned,v,
            (unsigned long long)work.retained,(unsigned long long)work.readonlyDeclined,(unsigned long long)work.reused,(unsigned long long)work.retries);
        if(owned){CHECK(work.readonlyDeclined>=1);if(previous)CHECK(named(xlsSemanticCheckAst(previous),"make")!=named(xlsSemanticCheckAst(result),"make"));}
        if(v){CHECK(work.reused>=60 && !work.retries);if(!owned)CHECK(named(xlsSemanticCheckAst(previous),"recordValue")==named(xlsSemanticCheckAst(result),"recordValue"));}
        xlsSemanticCheckDestroy(previous);previous=result;xrtStrBufFree(&text);xrtFree(base);
    }
    xlsSemanticCheckDestroy(previous);xlsCheckSessionDestroy(session);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
int main(int argc,char** argv)
{
    if(argc>2 || (argc==2 && strcmp(argv[1],"--expect-eager")))return 2;
    expectEager=argc==2;
    xrtAtomic32Init(&checks,0);if(!editing())return 1;
    if(!expectEager && (!backing() || !budgets() || !interop(0) || !interop(1)))return 1;
    printf("analysis_native_borrow_test passed:%u checks;actual ordinary native root identity,complete fresh AST/index/IR parity,region/symbol eager retries,lifetime\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
#else
int main(void){puts("analysis_native_borrow_test skipped:old native representation");return 0;}
#endif
