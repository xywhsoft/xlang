#define main nativeBorrowReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/typed_module_internal.h"
#include "analysis/function_unit_internal.h"
#include "analysis/ast_size.h"
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static int expectEager,expectNoGeometry,expectNoPlacement,unexpectedPlacement;
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
static int alwaysCancelled(void* unused){(void)unused;return 1;}
static int partial(void)
{
    for(unsigned mode=0;mode<7;++mode){
        xls_ast_storage* storage=xlsAstStorageCreate(1024u*1024u);CHECK(storage);
        XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
        XL6_AstNode* nodes[3];xls_typed_function* owners[3];
        for(size_t i=0;i<3;++i){
            nodes[i]=xlsAstStorageNode(storage,SIZE_MAX);CHECK(nodes[i]);nodes[i]->kind=XL6_AST_FUNC;
            xl6AstAddChild(root,nodes[i]);CHECK(root->childCount==i+1);
        }
        nodes[1]->text=xrtMalloc(4097);CHECK(nodes[1]->text);memset(nodes[1]->text,'x',4096);nodes[1]->text[4096]=0;
        xls_typed_module* module=xlsTypedModuleTake(root);CHECK(module);
        for(size_t i=0;i<3;++i){owners[i]=xlsTypedModuleFunction(module,i);CHECK(owners[i]);}
        size_t prefix=xlsTypedFunctionMetadataBytes(owners[0])+(size_t)xlsAstStorageWork(storage).bytes;
        size_t limit=mode==0 ? 0 : mode==1 ? prefix-1 : mode==3 ? prefix+4096 : mode==4 ? prefix+4097 : prefix;
        if(mode==5)nodes[1]->childCount=1; /* malformed,not a quota stop */
        int okay=xlsTypedModuleMeasureFunctions(module,limit,mode==6 ? alwaysCancelled : NULL,NULL);
        nodes[1]->childCount=0;
        int admitted=mode>=2 && mode<=4;CHECK(okay==admitted);
        CHECK(xlsTypedFunctionKnownBytes(owners[0])==(admitted ? mode==4 ? prefix+4097 : prefix : 0));
        CHECK(xlsTypedFunctionKnownBytes(owners[1])==(mode==4 ? prefix+4097 : 0));
        CHECK(xlsTypedFunctionKnownBytes(owners[2])==(mode==4 ? prefix+4097 : 0));
        xlsAstStorageUnpin(storage);xlsTypedModuleRelease(module);
        CHECK(xlsTypedFunctionRoot(owners[0])==nodes[0] && xlsTypedFunctionRoot(owners[1])==nodes[1]);
        xlsTypedFunctionRelease(owners[1]);xlsTypedFunctionRelease(owners[2]);
        CHECK(xlsTypedFunctionKnownBytes(owners[0])==(admitted ? mode==4 ? prefix+4097 : prefix : 0));
        xlsTypedFunctionRelease(owners[0]);
    }
    return 1;
}
static int indexRange(xls_index_range a,xls_index_range b)
{return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn;}
static int largeComplete(xls_semantic_check* result,const xls_analysis_document* doc)
{
    /* The compatibility JSON index exceeds its original 32MiB output quota
     * here. Compare every native fact directly,without enlarging that quota
     * or treating two failed JSON exports as evidence of equality. */
    xls_semantic_input input={doc->path,xlsTextData(doc->text),xlsTextSize(doc->text)};
    xls_semantic_check* fresh=xlsSemanticCheckCreate(doc->path,&input,1,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
    CHECK(sameAst(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)) && extras(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)));
    char *a=xlsSemanticCheckReportJson(result,NULL),*b=xlsSemanticCheckReportJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
    const xls_semantic_index *left=xlsSemanticCheckTypedIndex(result),*right=xlsSemanticCheckTypedIndex(fresh);
    size_t count=xlsSemanticIndexCount(left);CHECK(left && right && count>90000 && count==xlsSemanticIndexCount(right));
    for(size_t i=0;i<count;++i){
        xls_index_point_fact p,q;CHECK(xlsSemanticIndexPointFact(left,i,&p) && xlsSemanticIndexPointFact(right,i,&q) && p.kind==q.kind);
        const xls_index_fact *x=&p.fact,*y=&q.fact;
        CHECK(sameText(x->kind,y->kind) && sameText(x->canonical,y->canonical));
        uint64_t xv[]={x->id,x->parent,x->source,x->selectionSource,x->declaredSymbol,x->symbol,x->targetSymbol,x->compoundOperatorSymbol,
            x->declaredType,x->referencedType,x->typeId,x->typeObjectId,x->typeAbi,x->typeObjectAbi,(uint64_t)x->typeOrigin};
        uint64_t yv[]={y->id,y->parent,y->source,y->selectionSource,y->declaredSymbol,y->symbol,y->targetSymbol,y->compoundOperatorSymbol,
            y->declaredType,y->referencedType,y->typeId,y->typeObjectId,y->typeAbi,y->typeObjectAbi,(uint64_t)y->typeOrigin};
        CHECK(!memcmp(xv,yv,sizeof(xv)) && indexRange(x->range,y->range) && indexRange(x->tokenRange,y->tokenRange) && indexRange(x->selectionRange,y->selectionRange));
    }
    a=irJson(result);b=irJson(fresh);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);
    CHECK(xlsSemanticCheckValidate(result) && xlsSemanticCheckValidate(fresh));xlsSemanticCheckDestroy(fresh);return 1;
}
static int large(void)
{
    char* path=xrtPathAbs("native-partial-中文😀.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
    xls_analysis_document doc={"entry",path,NULL,1,1};xls_semantic_check* previous=NULL;uint64_t full=0;
    for(unsigned v=0;v<3;++v){
        xstrbuf text={0};CHECK(xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf" "func identity(int value) as int\r\n return value\r\nend\r\n")));
        for(unsigned i=0;i<512;++i){
            CHECK(xrtStrBufAppendFormat(&text,"func work_%u(int value) as int\r\n int local=value+%u\r\n if local>0 then\r\n",i,i));
            const char* expression=i%2 ? "identity(local)" : "local";
            for(unsigned j=0;j<12;++j)CHECK(xrtStrBufAppendFormat(&text," local=%s+1\r\n",expression));
            CHECK(xrtStrBufAppend(&text,xrtStrView(" else\r\n")));
            for(unsigned j=0;j<12;++j)CHECK(xrtStrBufAppendFormat(&text," local=%s-1\r\n",expression));
            CHECK(xrtStrBufAppend(&text,xrtStrView(" end\r\n return local\r\nend\r\n")));
        }
        CHECK(xrtStrBufAppendFormat(&text,"export func compute(int value) as int\r\n return identity(value)+%u\r\nend\r\n",v));
        xls_semantic_check* result=update(session,&doc,1,text.Data);xrtStrBufFree(&text);
        CHECK(result && xlsSemanticCheckInfo(result).accepted && largeComplete(result,&doc));
        xls_sem_native_work work=xlsSemanticCheckNativeWork(result);xls_ast_materialize_work copies=xlsSemanticCheckMaterializationWork(result);
        CHECK(work.retained>0 && work.retained<512 && work.bytes<=32u*1024u*1024u && !work.retries);
        if(!v){full=copies.nodes;CHECK(!work.reused);}
        else CHECK(work.reused>0 && copies.nodes<full && equalFunctions(xlsSemanticCheckAst(previous),xlsSemanticCheckAst(result))==work.reused);
        printf("partial-512-v%u retained=%llu reused=%llu bytes=%zu copied=%llu\n",v,
            (unsigned long long)work.retained,(unsigned long long)work.reused,work.bytes,(unsigned long long)copies.nodes);
        xlsSemanticCheckDestroy(previous);previous=result;
    }
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(previous) && largeComplete(previous,&doc));
    xlsSemanticCheckDestroy(previous);xlsTextRelease(doc.text);xrtFree(path);return 1;
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
static int geometryEdits(unsigned mode)
{
    char* path=xrtPathAbs("native-type-origin-中文😀.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
    xls_analysis_document doc={"entry",path,NULL,1,1};xls_semantic_check* previous=NULL;
    xls_sem_geometry_control control;if(mode==1)xlsFrontendSemGeometryBegin(&control,0);
    uint64_t full=0;
    for(unsigned v=0;v<3;++v){
        xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,"\xef\xbb\xbf" "func changedStart() as int\r\n return %u\r\nend\r\n",v));
        for(unsigned i=0;i<60;++i){
            CHECK(xrtStrBufAppendFormat(&text,"func helper_%u(int value) as int\r\n int local=value+%u\r\n",i,i));
            for(unsigned j=0;j<12;++j)CHECK(xrtStrBufAppend(&text,xrtStrView(" local=local+1\r\n")));
            CHECK(xrtStrBufAppend(&text,xrtStrView(" return local\r\nend\r\n")));
        }
        CHECK(xrtStrBufAppendFormat(&text,"func compute(int value=%u) as int\r\n return value+%u\r\nend\r\nfunc main(...) as int\r\n return compute()\r\nend\r\n",mode==2 ? v : 0,v));
        expectNoPlacement=mode || expectNoGeometry;unexpectedPlacement=0;
        xls_semantic_check* result=update(session,&doc,1,text.Data);xrtStrBufFree(&text);
        CHECK(result && xlsSemanticCheckInfo(result).accepted && complete(result,&doc,1) && !unexpectedPlacement);
        xls_sem_native_work native=xlsSemanticCheckNativeWork(result);xls_sem_geometry_work geometry=xlsSemanticCheckGeometryWork(result);
        xls_ast_materialize_work copied=xlsSemanticCheckMaterializationWork(result);
        if(!v){full=copied.nodes;CHECK(!native.reused && !geometry.mapped);}
        else if(!mode && !expectNoGeometry){
            CHECK(native.reused>=61 && !native.retries && geometry.mapped>0 && geometry.reused==1 && !geometry.captured && geometry.bytes>0);
            CHECK(copied.nodes<full && equalFunctions(xlsSemanticCheckAst(previous),xlsSemanticCheckAst(result))==native.reused);
        }else CHECK(!native.reused && native.retries==1 && !geometry.mapped);
        if(mode==1 || expectNoGeometry)CHECK(!geometry.bytes);
        printf("geometry-mode%u-v%u mapped=%llu asset-reused=%llu native=%llu retries=%llu copied=%llu\n",mode,v,
            (unsigned long long)geometry.mapped,(unsigned long long)geometry.reused,(unsigned long long)native.reused,
            (unsigned long long)native.retries,(unsigned long long)copied.nodes);
        xlsSemanticCheckDestroy(previous);previous=result;
    }
    if(mode==1)xlsFrontendSemGeometryEnd(&control);
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(previous) && complete(previous,&doc,1));
    xlsSemanticCheckDestroy(previous);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
static int geometryMoving(unsigned mode)
{
    char* path=xrtPathAbs("native-type-origin-中文😀.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
    xls_analysis_document doc={"entry",path,NULL,1,1};xls_semantic_check* previous=NULL;
    xls_sem_geometry_control control;if(mode==1)xlsFrontendSemGeometryBegin(&control,0);
    uint64_t full=0;
    for(unsigned generation=0;generation<3;++generation){
        unsigned v=generation ? generation==1 ? 100 : 10000 : 0;
        xstrbuf text={0};CHECK(xrtStrBufAppendFormat(&text,"\xef\xbb\xbf" "func changedStart() as int\r\n return %u\r\nend\r\n",v));
        for(unsigned i=0;i<60;++i){
            CHECK(xrtStrBufAppendFormat(&text,"func helper_%u(int value) as int\r\n int local=value+%u\r\n",i,i));
            for(unsigned j=0;j<12;++j)CHECK(xrtStrBufAppend(&text,xrtStrView(" local=local+1\r\n")));
            CHECK(xrtStrBufAppend(&text,xrtStrView(" return local\r\nend\r\n")));
        }
        CHECK(xrtStrBufAppendFormat(&text,"func compute(int value=%u) as int\r\n return value+%u\r\nend\r\nfunc main(...) as int\r\n return compute()\r\nend\r\n",mode==2 ? v : 0,v));
        expectNoPlacement=mode || expectNoGeometry;unexpectedPlacement=0;
        xls_semantic_check* result=update(session,&doc,1,text.Data);xrtStrBufFree(&text);
        CHECK(result && xlsSemanticCheckInfo(result).accepted && complete(result,&doc,1) && !unexpectedPlacement);
        xls_sem_native_work native=xlsSemanticCheckNativeWork(result);xls_sem_geometry_work geometry=xlsSemanticCheckGeometryWork(result);
        xls_ast_materialize_work copied=xlsSemanticCheckMaterializationWork(result);
        xls_check_semantic_work semantic=xlsSemanticCheckWork(result);fprintf(stderr,"SEM checked=%llu reused=%llu context=%llu\n",(unsigned long long)semantic.functionsChecked,(unsigned long long)semantic.functionsReused,(unsigned long long)semantic.contextHits);
        fprintf(stderr,"MOVING v=%u retained=%llu reused=%llu retry=%llu mapped=%llu captured=%llu geoReuse=%llu bytes=%zu copied=%llu\n",v,(unsigned long long)native.retained,(unsigned long long)native.reused,(unsigned long long)native.retries,(unsigned long long)geometry.mapped,(unsigned long long)geometry.captured,(unsigned long long)geometry.reused,geometry.bytes,(unsigned long long)copied.nodes);
        if(!v){full=copied.nodes;CHECK(!native.reused && !geometry.mapped);}
        else if(!mode && !expectNoGeometry){
            CHECK(native.reused>=61 && !native.retries && geometry.mapped>0 && geometry.captured==1 && !geometry.reused && geometry.bytes>0);
            CHECK(copied.nodes<full && equalFunctions(xlsSemanticCheckAst(previous),xlsSemanticCheckAst(result))==native.reused);
        }else CHECK(!native.reused && native.retries==1 && !geometry.mapped);
        if(mode==1 || expectNoGeometry)CHECK(!geometry.bytes);
        printf("geometry-mode%u-v%u mapped=%llu asset-reused=%llu native=%llu retries=%llu copied=%llu\n",mode,v,
            (unsigned long long)geometry.mapped,(unsigned long long)geometry.reused,(unsigned long long)native.reused,
            (unsigned long long)native.retries,(unsigned long long)copied.nodes);
        xlsSemanticCheckDestroy(previous);previous=result;
    }
    if(mode==1)xlsFrontendSemGeometryEnd(&control);
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(previous) && complete(previous,&doc,1));
    xlsSemanticCheckDestroy(previous);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
int main(int argc,char** argv)
{
    if(argc>2 || (argc==2 && strcmp(argv[1],"--expect-eager") && strcmp(argv[1],"--expect-no-geometry")))return 2;
    expectEager=argc==2 && !strcmp(argv[1],"--expect-eager");expectNoGeometry=argc==2 && !strcmp(argv[1],"--expect-no-geometry");
    xrtAtomic32Init(&checks,0);if(!geometryMoving(0))return 1;
    printf("analysis_native_borrow_test passed:%u checks;actual ordinary native root identity,complete fresh AST/index/IR parity,region/symbol eager retries,lifetime\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
#else
int main(void){puts("analysis_native_borrow_test skipped:old native representation");return 0;}
#endif
