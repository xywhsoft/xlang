#define main canonicalReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/frontend_index.h"
#include "analysis/function_unit.h"
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static const XL6_AstNode* watched;
static unsigned freed;static int expectNoCanonical;
void __real_xl6AstFree(XL6_AstNode*);
void __wrap_xl6AstFree(XL6_AstNode* node)
{if(node && node==watched)++freed;__real_xl6AstFree(node);}
static int extended(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    if(!!a->source!=!!b->source || (a->source && (a->source->completionQuery!=b->source->completionQuery ||
       a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd)) ||
       a->semanticReceiverSymbolId!=b->semanticReceiverSymbolId || a->compiledPackageError!=b->compiledPackageError ||
       a->compiledPackageRoot!=b->compiledPackageRoot || !sameText(a->compiledPackageLink,b->compiledPackageLink))return 0;
    if(!extended(a->inferredType,b->inferredType) || !extended(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!extended(a->children[i],b->children[i]))return 0;
    return 1;
}
static int indexRange(xls_index_range a,xls_index_range b)
{return a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn && a.known==b.known;}
static int indexFact(xls_index_fact a,xls_index_fact b)
{
    return sameText(a.kind,b.kind) && sameText(a.canonical,b.canonical) && a.id==b.id && a.parent==b.parent && a.source==b.source && a.selectionSource==b.selectionSource &&
        a.declaredSymbol==b.declaredSymbol && a.symbol==b.symbol && a.targetSymbol==b.targetSymbol && a.compoundOperatorSymbol==b.compoundOperatorSymbol &&
        a.declaredType==b.declaredType && a.referencedType==b.referencedType && a.typeId==b.typeId && a.typeObjectId==b.typeObjectId &&
        a.typeAbi==b.typeAbi && a.typeObjectAbi==b.typeObjectAbi && a.typeOrigin==b.typeOrigin &&
        indexRange(a.range,b.range) && indexRange(a.tokenRange,b.tokenRange) && indexRange(a.selectionRange,b.selectionRange);
}
static int completeCanonical(xls_semantic_check* result,const xls_analysis_document* docs,size_t count)
{
    xls_semantic_input inputs[2];CHECK(count<=2);
    for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    xls_semantic_check* original=xlsSemanticCheckCreate(docs[0].path,inputs,count,1,NULL,NULL,NULL,0);CHECK(original);
    CHECK(xlsSemanticCheckInfo(original).accepted && sameAst(xlsSemanticCheckAst(result),xlsSemanticCheckAst(original)) && extended(xlsSemanticCheckAst(result),xlsSemanticCheckAst(original)));
    const xls_semantic_index *a=xlsSemanticCheckTypedIndex(result),*b=xlsSemanticCheckTypedIndex(original);
    CHECK(a && b && xlsSemanticIndexCount(a)==xlsSemanticIndexCount(b));
    for(size_t i=0;i<xlsSemanticIndexCount(a);++i){xls_index_point_fact left,right;
        CHECK(xlsSemanticIndexPointFact(a,i,&left) && xlsSemanticIndexPointFact(b,i,&right) && left.kind==right.kind && indexFact(left.fact,right.fact));}
    char *left=irJson(result),*right=irJson(original);CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);
    left=xlsSemanticCheckJson(result,NULL);right=xlsSemanticCheckJson(original,NULL);CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);
    xlsSemanticCheckDestroy(original);return 1;
}
static char* canonicalSource(unsigned functions,unsigned value,unsigned defaultValue,int imported,int alias)
{
    xstrbuf text={0};int okay=xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf"));const char* type=alias ? "Number" : "int";
    if(alias)okay=okay && xrtStrBufAppendFormat(&text,"typedef Number = %s\r\n",value&1 ? "float" : "int");
    okay=okay && xrtStrBufAppendFormat(&text,"func identity(%s v) as %s\r\n return v\r\nend\r\nfunc changedStart() as int\r\n return %u\r\nend\r\n",type,type,value);
    for(unsigned i=0;okay && i<functions;++i){
        okay=xrtStrBufAppendFormat(&text,"func helper_%u(%s value) as %s\r\n %s local=value+%u\r\n",i,type,type,type,i);
        if(functions<512)for(unsigned j=0;okay && j<12;++j)okay=xrtStrBufAppend(&text,xrtStrView(" local=local+1\r\n"));
        okay=okay && xrtStrBufAppend(&text,xrtStrView(" return identity(local)\r\nend\r\n"));
    }
    okay=okay && xrtStrBufAppendFormat(&text,"%sfunc compute(int value=%u) as int\r\n return value+%u\r\nend\r\n",imported ? "export " : "",defaultValue,value);
    if(!imported)okay=okay && xrtStrBufAppend(&text,xrtStrView("func main(...) as int\r\n return compute()\r\nend\r\n"));
    char* result=okay ? xrtStrDup(text.Data) : NULL;xrtStrBufFree(&text);return result;
}
static int editingCanonical(unsigned scenario,unsigned functions,int imported)
{
    char* path=xrtPathAbs(imported ? "canonical-entry-中文😀.xl" : "canonical-origin-中文😀.xl");
    char* library=imported ? xrtPathAbs("canonical-library.xl") : NULL;
    xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session && (!imported || library));
    xls_analysis_document docs[2]={{"entry",path,NULL,1,1},{"library",library,NULL,1,1}};
    if(imported)CHECK(setBody(&docs[0],"import \"canonical-library.xl\" as lib\r\nfunc main(...) as int\r\n return lib.compute()\r\nend\r\n"));
    xls_index_canonical_control outer,inner;
    if(scenario==1){xlsFrontendIndexCanonicalBegin(&outer,0);xlsFrontendIndexCanonicalBegin(&inner,SIZE_MAX);}
    xls_semantic_check* previous=NULL;uint64_t coldCalls=0;
    for(unsigned generation=0;generation<3;++generation){
        unsigned value=scenario==2 ? generation ? generation==1 ? 100 : 10000 : 0 : generation;
        char* text=canonicalSource(functions,value,scenario==3 ? generation : 0,imported,scenario==4);CHECK(text && setBody(&docs[imported ? 1 : 0],text));xrtFree(text);
        char error[256];xls_semantic_check* result=xlsCheckSessionCheck(session,docs,imported ? 2 : 1,"entry",1,NULL,NULL,error,sizeof(error));
        if(!result)fprintf(stderr,"canonical check:%s\n",error);
        else if(!xlsSemanticCheckInfo(result).accepted){char* diagnostic=xlsSemanticCheckJson(result,NULL);fprintf(stderr,"canonical rejected:%s\n",diagnostic ? diagnostic : "no report");xrtFree(diagnostic);}
        CHECK(result && xlsSemanticCheckInfo(result).accepted && completeCanonical(result,docs,imported ? 2 : 1));
        xls_index_canonical_work work=xlsSemanticCheckCanonicalWork(result);xls_check_semantic_work semantic=xlsSemanticCheckWork(result);
        CHECK(work.retainedBytes<=8u*1024u*1024u && !work.fallbacks);
        if(!generation){coldCalls=work.calls;CHECK(work.calls>0 && !work.reused && !work.valuesReused);}
        if(scenario==1 || expectNoCanonical)CHECK(!work.captured && !work.reused && !work.valuesReused && !work.retainedBytes && (scenario==4 ? work.calls>0 : work.calls==coldCalls));
        else if(generation && scenario<3){CHECK(work.reused>=functions && work.valuesReused>0 && work.calls<coldCalls/4 && semantic.functionsReused>=functions);}
        else if(generation)CHECK(!work.reused && !work.valuesReused && (scenario==4 ? work.calls>0 : work.calls==coldCalls));
        if(scenario!=1 && !expectNoCanonical)CHECK(work.captured+work.reused>=functions && work.retainedBytes>0);
        printf("canonical-case%u-functions%u-import%d-gen%u:captured=%llu reused=%llu values=%llu calls=%llu bytes=%zu semantic=%llu\n",scenario,functions,imported,generation,
            (unsigned long long)work.captured,(unsigned long long)work.reused,(unsigned long long)work.valuesReused,(unsigned long long)work.calls,work.retainedBytes,(unsigned long long)semantic.functionsReused);
        if(previous && scenario==2 && !imported){
            const XL6_AstNode* root=xlsSemanticCheckAst(previous);watched=root->children[2];freed=0;
            CHECK(xlsSemanticCheckAst(result)->children[2]!=watched);xlsSemanticCheckDestroy(previous);previous=NULL;
            CHECK(freed==1);watched=NULL;CHECK(completeCanonical(result,docs,1));
        }
        xlsSemanticCheckDestroy(previous);previous=result;
    }
    if(scenario==1){xlsFrontendIndexCanonicalEnd(&inner);xlsFrontendIndexCanonicalEnd(&outer);}
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(previous) && completeCanonical(previous,docs,imported ? 2 : 1));
    xls_function_unit* held=NULL;xls_index_fact* expected=NULL;size_t facts=0;
    if(scenario==2 && !imported && functions==60){
        char error[256];held=xlsSemanticCheckFunctionCapture(previous,2,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(held);
        facts=xlsFunctionUnitInfo(held).facts;expected=xrtCalloc(facts,sizeof(*expected));CHECK(facts && expected);
        for(size_t i=0;i<facts;++i){CHECK(xlsFunctionUnitFact(held,i,&expected[i]));
            expected[i].canonical=expected[i].canonical ? xrtStrDup(expected[i].canonical) : NULL;}
    }
    xlsSemanticCheckDestroy(previous);
    for(size_t i=0;i<facts;++i){xls_index_fact fact;CHECK(xlsFunctionUnitFact(held,i,&fact) && indexFact(fact,expected[i]));xrtFree((char*)expected[i].canonical);}
    xrtFree(expected);xlsFunctionUnitRelease(held);
    xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xrtFree(path);xrtFree(library);return 1;
}
int main(int argc,char** argv)
{
    if(argc>2 || (argc==2 && strcmp(argv[1],"--expect-no-canonical")))return 2;
    expectNoCanonical=argc==2;
    xrtAtomic32Init(&checks,0);
    for(unsigned scenario=0;scenario<5;++scenario)if(!editingCanonical(scenario,60,0))return 1;
    if(!editingCanonical(2,60,1) || !editingCanonical(2,512,0))return 1;
    printf("analysis_index_canonical_test passed:%u checks;current typed facts and full native AST/IR parity,real source displacement,disabled cache,header invalidation,old AST retirement\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
#else
int main(void){puts("analysis_index_canonical_test skipped:old native representation");return 0;}
#endif
