#define main functionIndexReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/function_unit_internal.h"
#include "analysis/frontend_function_index.h"
#include <stdlib.h>

#define WATCHED_ALLOCATIONS 16384u
static xatomicptr allocations[WATCHED_ALLOCATIONS];static xatomic32 allocationCount,denied;
static _Thread_local int recordingIndex,inBind;
static int denySourceCatalog;
static void remember(void* p)
{
    if(!p || !recordingIndex)return;
    for(unsigned i=0;i<WATCHED_ALLOCATIONS;++i)if(xrtAtomicPtrLoad(&allocations[i],XMEMORY_ACQUIRE)==p)return;
    for(unsigned i=0;i<WATCHED_ALLOCATIONS;++i){ptr expected=NULL;
        if(xrtAtomicPtrCompareExchange(&allocations[i],&expected,p,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
            xrtAtomic32FetchAdd(&allocationCount,1,XMEMORY_RELAXED);return;
        }
    }
    abort();
}
static void forget(void* p)
{
    if(!p)return;
    for(unsigned i=0;i<WATCHED_ALLOCATIONS;++i){ptr expected=p;
        if(xrtAtomicPtrCompareExchange(&allocations[i],&expected,NULL,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){
            xrtAtomic32FetchSub(&allocationCount,1,XMEMORY_RELAXED);return;
        }
    }
}
void* __real_xrtRealloc(void*,size_t);void __real_xrtFree(void*);void* __real_xrtCalloc(size_t,size_t);
void* __wrap_xrtRealloc(void* p,size_t n)
{void* q=__real_xrtRealloc(p,n);if(q){forget(p);remember(q);}else if(!n)forget(p);return q;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
void* __wrap_xrtCalloc(size_t n,size_t size)
{if(inBind && denySourceCatalog){xrtAtomic32FetchAdd(&denied,1,XMEMORY_RELAXED);return NULL;}return __real_xrtCalloc(n,size);}
xls_semantic_index* __real_xlsFrontendIndexCreate(const XL6_AstNode*,const XL6_SourceReport*,char*,size_t);
xls_semantic_index* __wrap_xlsFrontendIndexCreate(const XL6_AstNode* root,const XL6_SourceReport* report,char* error,size_t size)
{int previous=recordingIndex;recordingIndex=1;xls_semantic_index* result=__real_xlsFrontendIndexCreate(root,report,error,size);recordingIndex=previous;return result;}
int __real_xlsFrontendIndexBindFunctions(xls_semantic_index*,const xls_typed_module*);
int __wrap_xlsFrontendIndexBindFunctions(xls_semantic_index* index,const xls_typed_module* module)
{inBind=1;int result=__real_xlsFrontendIndexBindFunctions(index,module);inBind=0;return result;}
static int rangeEqual(xls_index_range a,xls_index_range b)
{return a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn && a.known==b.known;}
static int factEqual(xls_index_fact a,xls_index_fact b)
{
    return sameText(a.kind,b.kind) && sameText(a.canonical,b.canonical) && a.id==b.id && a.parent==b.parent && a.source==b.source && a.selectionSource==b.selectionSource &&
        a.declaredSymbol==b.declaredSymbol && a.symbol==b.symbol && a.targetSymbol==b.targetSymbol && a.compoundOperatorSymbol==b.compoundOperatorSymbol &&
        a.declaredType==b.declaredType && a.referencedType==b.referencedType && a.typeId==b.typeId && a.typeObjectId==b.typeObjectId &&
        a.typeAbi==b.typeAbi && a.typeObjectAbi==b.typeObjectAbi && a.typeOrigin==b.typeOrigin &&
        rangeEqual(a.range,b.range) && rangeEqual(a.tokenRange,b.tokenRange) && rangeEqual(a.selectionRange,b.selectionRange);
}
static xls_semantic_check* create(const char* path,const char* source,const char* dependency,int flat)
{
    xls_text* text=xlsTextCreate(source,strlen(source),8u*1024u*1024u,NULL);if(!text)return NULL;
    xls_text* dep=dependency ? xlsTextCreate(dependency,strlen(dependency),8u*1024u*1024u,NULL) : NULL;
    char* depPath=dependency ? xrtPathAbs("dep.xl") : NULL;
    if(dependency && (!dep || !depPath)){xlsTextRelease(text);xlsTextRelease(dep);xrtFree(depPath);return NULL;}
    xls_analysis_document documents[]={{"entry",path,text,71,1},{"dep",depPath,dep,72,1}};
    xls_check_session* session=xlsCheckSessionCreate(NULL);char error[256];
    if(!session){xlsTextRelease(text);xlsTextRelease(dep);xrtFree(depPath);return NULL;}
    xls_index_storage_control control;if(flat)xlsFrontendIndexStorageBegin(&control,0);
    xls_semantic_check* result=xlsCheckSessionCheck(session,documents,dependency ? 2 : 1,"entry",1,NULL,NULL,error,sizeof(error));
    if(flat)xlsFrontendIndexStorageEnd(&control);
    xlsCheckSessionDestroy(session);xlsTextRelease(text);xlsTextRelease(dep);xrtFree(depPath);
    if(!result)fprintf(stderr,"function index create:%s\n",error);
    else if(!xlsSemanticCheckInfo(result).accepted){char* report=xlsSemanticCheckJson(result,NULL);fprintf(stderr,"function index rejected:%s\n",report ? report : "no report");xrtFree(report);}
    return result;
}
static xls_typed_function* find(const xls_semantic_check* check,const char* name)
{
    const xls_typed_module* module=xlsFrontendCheckFunctionModule(check);const XL6_AstNode* root=xlsSemanticCheckAst(check);
    for(size_t i=0;root && i<root->childCount;++i){const XL6_AstNode* n=root->children[i];
        if(n && n->kind==XL6_AST_FUNC && n->childCount && sameText(n->children[0]->text,name))return xlsTypedModuleFunction(module,i);
    }
    return NULL;
}
typedef struct asset_reader {xls_function_index_asset* asset;const xls_index_fact* expected;size_t count;int okay;} asset_reader;
static int32 readAsset(void* user)
{
    asset_reader* reader=user;reader->okay=1;
    for(unsigned round=0;round<64;++round)for(size_t i=0;i<reader->count;++i){xls_index_fact value;
        if(!xlsFunctionIndexAssetFact(reader->asset,i,&value) || !factEqual(value,reader->expected[i]))reader->okay=0;
    }
    xlsFunctionIndexAssetRelease(reader->asset);return 0;
}
static int lifetime(const char* source,const char* dependency)
{
    char* path=xrtPathAbs("function-index-中文😀.xl");CHECK(path);
    CHECK(!xrtAtomic32Load(&allocationCount,XMEMORY_RELAXED));
    xls_semantic_check* check=create(path,source,dependency,0);CHECK(check && xlsSemanticCheckInfo(check).accepted);
    const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);xls_typed_function* function=find(check,"helper");CHECK(function);
    xls_function_index_asset* asset=xlsFrontendIndexFunctionAsset(index,function);CHECK(asset);
    xls_function_index_info info=xlsFunctionIndexAssetInfo(asset);CHECK(info.facts && info.recordBytes && info.sharedBytes);
    xls_semantic_check* flat=create(path,source,dependency,1);CHECK(flat && xlsSemanticCheckInfo(flat).accepted);
    const xls_semantic_index* reference=xlsSemanticCheckTypedIndex(flat);CHECK(xlsSemanticIndexCount(reference)==xlsSemanticIndexCount(index));
    for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_point_fact a,b;
        CHECK(xlsSemanticIndexPointFact(index,i,&a) && xlsSemanticIndexPointFact(reference,i,&b) && a.kind==b.kind && factEqual(a.fact,b.fact));
    }
    xls_typed_function* other=find(flat,"helper");CHECK(other && !xlsFrontendIndexFunctionAsset(index,other) && !xlsFrontendIndexFunctionAsset(reference,other));xlsTypedFunctionRelease(other);xlsSemanticCheckDestroy(flat);
    xls_index_fact* expected=xrtCalloc(info.facts,sizeof(*expected));const char** canonical=xrtCalloc(info.facts,sizeof(*canonical));CHECK(expected && canonical);
    for(size_t i=0;i<info.facts;++i){xls_index_fact a,b;CHECK(xlsFunctionIndexAssetFact(asset,i,&a) && xlsSemanticIndexFact(index,info.first+i,&b) && factEqual(a,b));
        CHECK(a.canonical==b.canonical);canonical[i]=a.canonical;expected[i]=a;expected[i].canonical=a.canonical ? xrtStrDup(a.canonical) : NULL;CHECK(!a.canonical || expected[i].canonical);
        xls_index_point_fact point;CHECK(xlsFunctionIndexAssetPointFact(asset,i,&point) && factEqual(point.fact,a));
    }
    size_t sources=xlsFunctionIndexAssetSourceCount(asset);xls_inputs inputs;CHECK(xlsSemanticCheckInputs(check,&inputs) && sources==inputs.sources);
    xls_function_index_source* sourceSnapshot=xrtCalloc(sources,sizeof(*sourceSnapshot));CHECK(sourceSnapshot);
    for(size_t i=0;i<sources;++i){xls_function_index_source own;xls_check_source original;
        CHECK(xlsFunctionIndexAssetSource(asset,i,&own) && xlsSemanticCheckSource(check,i,&original) && own.path!=original.path && sameText(own.path,original.path) &&
            own.size==original.size && own.rawSize==original.rawSize && own.kind==original.kind && !memcmp(own.textSha256,original.textSha256,32) && !memcmp(own.rawSha256,original.rawSha256,32));
        sourceSnapshot[i]=own;sourceSnapshot[i].path=xrtStrDup(own.path);CHECK(sourceSnapshot[i].path);
    }
    xlsTypedFunctionRelease(function);xlsSemanticCheckDestroy(check);
    size_t remainingBuffers=0;
    for(unsigned i=0;i<WATCHED_ALLOCATIONS;++i){ptr p=xrtAtomicPtrLoad(&allocations[i],XMEMORY_ACQUIRE);if(!p)continue;int text=0;
        for(size_t j=0;j<info.facts;++j)if(p==canonical[j])text=1;
        if(!text)++remainingBuffers;
    }
    /* Only THIS function's fact buffer and canonical text remain. In
     * particular,the complete index directory/source lookup and every
     * unrelated declaration buffer have already retired. */
    CHECK(remainingBuffers==1);
    for(size_t i=0;i<sources;++i){xls_function_index_source actual;const xls_function_index_source* expectedSource=&sourceSnapshot[i];
        CHECK(xlsFunctionIndexAssetSource(asset,i,&actual) && sameText(actual.path,expectedSource->path) && actual.kind==expectedSource->kind &&
            actual.size==expectedSource->size && actual.rawSize==expectedSource->rawSize && !memcmp(actual.textSha256,expectedSource->textSha256,32) && !memcmp(actual.rawSha256,expectedSource->rawSha256,32));
    }
    for(size_t i=0;i<info.facts;++i){xls_index_fact actual;CHECK(xlsFunctionIndexAssetFact(asset,i,&actual) && factEqual(actual,expected[i]));}
    xls_index_fact invalid;CHECK(!xlsFunctionIndexAssetFact(asset,info.facts,&invalid));
    xls_function_index_source meta;CHECK(!xlsFunctionIndexAssetSource(asset,sources,&meta) && !meta.path);
    asset_reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(asset_reader){xlsFunctionIndexAssetRetain(asset),expected,info.facts,0};threads[i]=xrtThreadCreate(readAsset,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    xlsFunctionIndexAssetRelease(asset);CHECK(!xrtAtomic32Load(&allocationCount,XMEMORY_RELAXED));
    for(size_t i=0;i<info.facts;++i)xrtFree((char*)expected[i].canonical);
    for(size_t i=0;i<sources;++i)xrtFree((char*)sourceSnapshot[i].path);
    xrtFree(sourceSnapshot);
    xrtFree(expected);xrtFree(canonical);xrtFree(path);
    printf("function index lifetime:%zu facts,%zu source rows;only one fact buffer retained;256 concurrent reader rounds\n",info.facts,sources);return 1;
}
static int sourceFailure(void)
{
    char* path=xrtPathAbs("function-index-source-fallback.xl");CHECK(path);const char* source="func helper(int value) as int\n return value+1\nend\nfunc main(...) as int\n return helper(7)\nend\n";
    denySourceCatalog=1;xrtAtomic32Store(&denied,0,XMEMORY_RELAXED);xls_semantic_check* check=create(path,source,NULL,0);denySourceCatalog=0;
    CHECK(check && xlsSemanticCheckInfo(check).accepted && xrtAtomic32Load(&denied,XMEMORY_RELAXED)>0);
    xls_typed_function* function=find(check,"helper");CHECK(function && !xlsFrontendIndexFunctionAsset(xlsSemanticCheckTypedIndex(check),function));xlsTypedFunctionRelease(function);
    char error[256];xls_function_unit* unit=xlsSemanticCheckFunctionCapture(check,0,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(unit && xlsFunctionUnitInfo(unit).facts);
    xlsSemanticCheckDestroy(check);xls_index_fact fact;CHECK(xlsFunctionUnitFact(unit,0,&fact));xlsFunctionUnitRelease(unit);xrtFree(path);
    CHECK(!xrtAtomic32Load(&allocationCount,XMEMORY_RELAXED));return 1;
}
static int runPrograms(void)
{
    xstrbuf body={0};CHECK(xrtStrBufAppend(&body,xrtStrView("\xef\xbb\xbf// 中文😀\nfunc helper(int value) as int\n int total=value+1\n return total\nend\n")));
    for(unsigned i=0;i<48;++i)CHECK(xrtStrBufAppendFormat(&body,"func spare_%u(int value) as int\n return value+%u\nend\n",i,i));
    CHECK(xrtStrBufAppend(&body,xrtStrView("func main(...) as int\n return helper(7)\nend\n")));
    if(!lifetime(body.Data,NULL)){xrtStrBufFree(&body);return 0;}xrtStrBufFree(&body);
#ifdef XLS_NATIVE_IR_FOREIGN_FIELDS
    if(!lifetime("extern \"record\" struct Record @header(\"record.h\")\n c_int value\nend\nfunc helper(Record value) as int\n return int(value.value)\nend\nfunc main(...) as int\n Record item=Record()\n return helper(item)\nend\n",NULL))return 0;
#endif
    if(!lifetime("import dep\nfunc helper(int value) as int\n return dep.value(value)+1\nend\nfunc main(...) as int\n return helper(7)\nend\n","export func value(int item) as int\n return item+7\nend\n"))return 0;
    if(!sourceFailure())return 0;
    CHECK(!xlsFunctionIndexAssetSourceCount(NULL) && !xlsFunctionIndexAssetInfo(NULL).facts);xlsFunctionIndexAssetRelease(NULL);
    return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);xrtAtomic32Init(&allocationCount,0);xrtAtomic32Init(&denied,0);
    for(unsigned i=0;i<WATCHED_ALLOCATIONS;++i)xrtAtomicPtrInit(&allocations[i],NULL);
    if(!runPrograms())return 1;
    printf("analysis_function_index_asset_test passed:%u checks\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
