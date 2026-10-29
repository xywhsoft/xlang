#define main legacyMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/typed_module_internal.h"
#include "analysis/ast_clone.h"
static XL6_AstNode* watchedRoot;
static xatomic32 rootDrops;
static int captureInput;
static xls_typed_module* heldInput;
xls_typed_module* __real_xlsTypedModuleTake(XL6_AstNode*);
xls_typed_module* __wrap_xlsTypedModuleTake(XL6_AstNode* root)
{
    xls_typed_module* input=__real_xlsTypedModuleTake(root);
    if(captureInput && input && !heldInput)heldInput=xlsTypedModuleRetain(input);
    return input;
}
void __real_xl6AstFree(XL6_AstNode*);
void __wrap_xl6AstFree(XL6_AstNode* root)
{
    if(root && root==watchedRoot)xrtAtomic32FetchAdd(&rootDrops,1,XMEMORY_RELAXED);
    __real_xl6AstFree(root);
}
static int shifted(const XL6_AstNode* before,XL6_AstNode* after,void* user)
{(void)user;if(before->line>0)after->line=before->line+4;return 1;}
static int stopNow(void* user){(void)user;return 1;}
typedef struct prepare_reader {xls_ir_result* result;xatomic32* start;int code;} prepare_reader;
static int32 prepareRead(void* user)
{
    prepare_reader* r=user;
    while(!xrtAtomic32Load(r->start,XMEMORY_ACQUIRE))xrtThreadYield();
    r->code=xlsFrontendIrResultPreparePersistent(r->result,32u*1024u*1024u);return 0;
}
static int ownership(const char* path)
{
    char* text=body(0);CHECK(text);
    xls_semantic_check* source=checked(path,text);CHECK(source && xlsSemanticCheckInfo(source).accepted);
    xls_ast_clone_options options={.bytes=64u*1024u*1024u};
    XL6_AstNode* original=xlsAstClone(xlsSemanticCheckAst(source),&options,NULL);CHECK(original);
    options.transform=shifted;XL6_AstNode* moved=xlsAstClone(original,&options,NULL);CHECK(moved);
    watchedRoot=original;xrtAtomic32Init(&rootDrops,0);
    xls_typed_module* input=xlsTypedModuleTake(original);CHECK(input && xlsTypedModuleRoot(input)==original);
    CHECK(xlsTypedModuleRetain(input)==input);xlsTypedModuleRelease(input);
    XL6_DiagList diags;xl6DiagInit(&diags);
    xls_ir_result* before=xlsFrontendIrResultBuildTyped(input,&diags,NULL,32u*1024u*1024u,NULL,0);
    CHECK(before && !diags.count);xlsTypedModuleRelease(input);
    CHECK(!xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED));
    char* expected=dump(xlsSemanticCheckIr(source));char* actual=dump(xlsFrontendIrResultModule(before));
    CHECK(expected && actual && !strcmp(expected,actual));xrtFree(actual);
    xls_ir_persistent_work cold=xlsFrontendIrResultPersistentWork(before);
    CHECK(!cold.reused && !cold.captured && !cold.keyBytes && !cold.metadataBytes);
    CHECK(!xlsFrontendIrResultPreparePersistent(before,0));
    CHECK(!xlsFrontendIrResultPreparePersistent(before,1));
    CHECK(!xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED));
    xls_frontend_cancel_scope cancel;xls_frontend_cancel_work work;
    xlsFrontendCancelBegin(&cancel,stopNow,NULL);
    CHECK(!xlsFrontendIrResultPreparePersistent(before,32u*1024u*1024u));
    xlsFrontendCancelEnd(&cancel,&work);CHECK(work.cancelled && !xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED));
    xatomic32 start;xrtAtomic32Init(&start,0);prepare_reader readers[4];xthread* threads[4];
    for(size_t i=0;i<4;++i){readers[i]=(prepare_reader){before,&start,-1};threads[i]=xrtThreadCreate(prepareRead,&readers[i],0);CHECK(threads[i]);}
    xrtAtomic32Store(&start,1,XMEMORY_RELEASE);unsigned prepared=0;
    for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK);xrtThreadDestroy(threads[i]);CHECK(readers[i].code>=0 && readers[i].code<=2);prepared+=readers[i].code==2;}
    CHECK(prepared==1 && xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED)==1);
    watchedRoot=NULL;
    CHECK(xlsFrontendIrResultPreparePersistent(before,32u*1024u*1024u)==1);
    xls_ir_persistent_work after=xlsFrontendIrResultPersistentWork(before);CHECK(!memcmp(&cold,&after,sizeof(cold)));
    actual=dump(xlsFrontendIrResultModule(before));CHECK(actual && !strcmp(expected,actual));xrtFree(actual);xrtFree(expected);
    XL6_IrModule ordinary;xl6IrModuleInit(&ordinary);CHECK(xl6IrBuildModuleFromAst(&ordinary,moved,&diags) && !diags.count);
    expected=dump(&ordinary);CHECK(expected);xl6IrModuleFree(&ordinary);
    input=xlsTypedModuleTake(moved);CHECK(input);
    xls_ir_result* current=xlsFrontendIrResultBuildTyped(input,&diags,before,32u*1024u*1024u,NULL,32u*1024u*1024u);
    CHECK(current && !diags.count);xls_ir_persistent_work w=xlsFrontendIrResultPersistentWork(current);
    CHECK(w.reused==1 && w.verified==1 && w.tablesShared==1 && w.nodesCopied>16 && !w.built);
    const XL6_IrModule *a=xlsFrontendIrResultModule(before),*b=xlsFrontendIrResultModule(current);
    CHECK(a->nodes!=b->nodes && a->types.items==b->types.items && a->nodeCount==b->nodeCount);
    watchedRoot=moved;xrtAtomic32Store(&rootDrops,0,XMEMORY_RELAXED);xlsTypedModuleRelease(input);
    CHECK(xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED)==1);watchedRoot=NULL;
    xlsFrontendIrResultDestroy(before);xlsSemanticCheckDestroy(source);
    actual=dump(xlsFrontendIrResultModule(current));CHECK(actual && !strcmp(expected,actual));
    xrtFree(actual);xrtFree(expected);xlsFrontendIrResultDestroy(current);xl6DiagFree(&diags);xrtFree(text);return 1;
}
static int adaptiveSession(const char* path)
{
    char* text=body(0);CHECK(text);xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);
    const unsigned edits[]={0,0,0,1,1,1,7,7,7,8,8,8};xls_semantic_check* held=NULL;char error[256];
    for(size_t i=0;i<sizeof(edits)/sizeof(*edits);++i){
        char* bodyText=body(edits[i]==8 ? 7 : edits[i]);CHECK(bodyText);xstrbuf payload={0};
        if(edits[i]==8){char* at=strstr(bodyText," return value\r\n");CHECK(at);memcpy(at+8,"2    ",5);}
        CHECK(xrtStrBufAppend(&payload,xrtStrView(bodyText)) && xrtStrBufAppendFormat(&payload,"\r\n// current revision %zu 中文😀\r\n",i));
        xls_text* documentText=xlsTextCreate(payload.Data,payload.Size,8u*1024u*1024u,NULL);CHECK(documentText);
        xls_analysis_document document={"entry",path,documentText,1,i+1};
        xls_semantic_check* result=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));
        if(!result)fprintf(stderr,"adaptive check:%s\n",error);
        CHECK(result && xlsSemanticCheckInfo(result).accepted);
        xls_check_ir_persistent_work p=xlsSemanticCheckIrPersistentWork(result);xls_check_semantic_work sem=xlsSemanticCheckWork(result);
        printf("adaptive step=%zu checked=%llu reused=%llu context=%llu prepared=%llu persistent=%llu key=%llu\n",i,(unsigned long long)sem.functionsChecked,(unsigned long long)sem.functionsReused,(unsigned long long)sem.contextHits,(unsigned long long)p.prepared,(unsigned long long)p.reused,(unsigned long long)p.keyBytes);
        if(i%3==0)CHECK(!p.prepared && !p.reused && !p.captured && !p.keyBytes && !p.metadataBytes);
        else CHECK(p.reused==1 && p.verified==1 && p.tablesShared==1 && p.prepared==(i%3==1) && p.nodesCopied>16);
        if(i==9)CHECK(sem.functionsChecked>=1 && sem.functionsReused>=16);
        xls_semantic_check* fresh=checked(path,payload.Data);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
        char* expected=dump(xlsSemanticCheckIr(fresh));char* actual=dump(xlsSemanticCheckIr(result));CHECK(expected && actual && !strcmp(expected,actual));
        xrtFree(expected);xrtFree(actual);xlsSemanticCheckDestroy(fresh);
        xls_semantic_check* exact=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(exact==result);
        xls_check_ir_persistent_work repeat=xlsSemanticCheckIrPersistentWork(exact);CHECK(!memcmp(&p,&repeat,sizeof(p)));xlsSemanticCheckDestroy(exact);
        if(!held)held=xlsSemanticCheckRetain(result);
        xlsSemanticCheckDestroy(result);xlsTextRelease(documentText);xrtStrBufFree(&payload);xrtFree(bodyText);
    }
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(held));char* actual=dump(xlsSemanticCheckIr(held));CHECK(actual);xrtFree(actual);xlsSemanticCheckDestroy(held);xrtFree(text);return 1;
}
static int actualInputLifetime(const char* path)
{
    char* text=body(5);CHECK(text);xls_text* documentText=xlsTextCreate(text,strlen(text),8u*1024u*1024u,NULL);CHECK(documentText);
    xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_analysis_document document={"entry",path,documentText,1,1};char error[256];
    captureInput=1;xls_semantic_check* check=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));captureInput=0;
    CHECK(check && xlsSemanticCheckInfo(check).accepted && heldInput);
    watchedRoot=(XL6_AstNode*)xlsTypedModuleRoot(heldInput);xrtAtomic32Store(&rootDrops,0,XMEMORY_RELAXED);
    CHECK(watchedRoot==xlsSemanticCheckAst(check));char* expected=dump(xlsSemanticCheckIr(check));CHECK(expected);
    /* Retain the ACTUAL checked tree,not a clone. All report,source-batch,
     * syntax,dependency and check owners retire before its next IR build. */
    xlsCheckSessionDestroy(session);xlsSemanticCheckDestroy(check);xlsTextRelease(documentText);xrtFree(text);
    CHECK(!xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED));
    XL6_DiagList diags;xl6DiagInit(&diags);
    xls_ir_result* result=xlsFrontendIrResultBuildTyped(heldInput,&diags,NULL,32u*1024u*1024u,NULL,0);CHECK(result && !diags.count);
    xlsTypedModuleRelease(heldInput);heldInput=NULL;CHECK(!xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED));
    CHECK(xlsFrontendIrResultPreparePersistent(result,32u*1024u*1024u)==2);
    CHECK(xrtAtomic32Load(&rootDrops,XMEMORY_RELAXED)==1);watchedRoot=NULL;
    char* actual=dump(xlsFrontendIrResultModule(result));CHECK(actual && !strcmp(actual,expected));
    xrtFree(actual);xrtFree(expected);xlsFrontendIrResultDestroy(result);xl6DiagFree(&diags);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);char* path=xrtPathAbs("typed-owner-中文.xl");if(!path || !ownership(path) || !adaptiveSession(path) || !actualInputLifetime(path))return 1;
    xrtFree(path);printf("analysis_typed_module_test passed:%u checks;one AST retirement;4 preparers;adaptive sessions.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
