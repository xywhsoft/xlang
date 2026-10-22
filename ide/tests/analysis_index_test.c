#include "analysis/check.h"
#include "analysis/frontend_index.h"
#include "analysis/frontend_types.h"
#include "src/type/xl6_type_model.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_index_test:%d: %s\n",__LINE__,#value); return 0; } } while(0)
static const xvalue* get(const xvalue* v,const char* key) { return xrtValueObjectGet(v,xrtStrView(key)); }
static int number(const xvalue* v,uint64_t expected)
{ int64 n; return xrtValueGetInt(v,&n) && n>=0 && (uint64_t)n==expected; }
static int equal(const xvalue* v,const char* text)
{ xstrview view; return text && xrtValueGetString(v,&view) && view.Size==strlen(text) && !memcmp(view.Data,text,view.Size); }
static int hash(const xvalue* v,uint64_t expected)
{ char text[17]; snprintf(text,sizeof(text),"%016llx",(unsigned long long)expected); return equal(v,text); }
static int range(const xvalue* v,xls_index_range r)
{
    if(!r.known) return xrtValueType(v)==XVALUE_NULL;
    return number(get(v,"startOffset"),r.startOffset) && number(get(v,"endOffset"),r.endOffset) &&
        number(get(v,"startLine"),(unsigned)r.startLine) && number(get(v,"startColumn"),(unsigned)r.startColumn) &&
        number(get(v,"endLine"),(unsigned)r.endLine) && number(get(v,"endColumn"),(unsigned)r.endColumn);
}
static int verify(xls_semantic_check* check)
{
    CHECK(check && xlsSemanticCheckInfo(check).accepted);
    const xls_semantic_index* index=xlsSemanticCheckTypedIndex(check);
    CHECK(index && xlsSemanticIndexCount(index));
    size_t size; char* report=xlsSemanticCheckReportJson(check,&size);
    CHECK(report && size==strlen(report) && !strstr(report,"\"semanticIndex\""));
    xlsSemanticStringFree(report);
    /* Compare every typed field and nullable range with the unchanged shared
     * JSON serializer; neither side is a synthetic semantic response. */
    xlsFrontendTypeAliasReset();
    const char* json=xlsSemanticCheckIndex(check);
    CHECK(!xlsFrontendTypeAliasWork().indexes && !xlsFrontendTypeAliasWork().queries);
    xjsonreadconfig config; xrtJsonReadConfigInit(&config);
    config.MaxInputBytes=XLS_CHECK_REPORT_BYTES; config.MaxValues=4u*1024u*1024u;
    config.MaxContainerItems=65536; config.MaxDepth=64;
    xvalue* raw=json ? xrtJsonRead(xrtStrView(json),&config) : NULL;
    const xvalue* rows=raw ? get(raw,"nodes") : NULL;
    CHECK(rows && xrtValueCount(rows)==xlsSemanticIndexCount(index));
    unsigned declared=0,target=0,typed=0;
    for(size_t i=0;i<xlsSemanticIndexCount(index);++i) {
        xls_index_fact f; CHECK(xlsSemanticIndexFact(index,i,&f));
        const xvalue* row=xrtValueArrayGet(rows,i); const xvalue* type=get(row,"type");
        CHECK(equal(get(row,"kind"),f.kind) && number(get(row,"id"),f.id) && number(get(row,"parent"),f.parent));
        CHECK(number(get(row,"sourceId"),f.source) && number(get(row,"selectionSourceId"),f.selectionSource));
        CHECK(number(get(row,"declaredSymbolId"),f.declaredSymbol) && number(get(row,"symbolId"),f.symbol) &&
              number(get(row,"targetSymbolId"),f.targetSymbol) && number(get(row,"compoundOperatorSymbolId"),f.compoundOperatorSymbol));
        CHECK(number(get(row,"declaredTypeId"),f.declaredType) && number(get(row,"referencedTypeId"),f.referencedType) &&
              number(get(row,"typeId"),f.typeId) && hash(get(row,"typeAbiId"),f.typeAbi));
        CHECK(range(get(row,"range"),f.range) && range(get(row,"tokenRange"),f.tokenRange) && range(get(row,"selectionRange"),f.selectionRange));
        if(!f.typeOrigin) CHECK(xrtValueType(type)==XVALUE_NULL && !f.canonical);
        else CHECK(equal(get(type,"canonical"),f.canonical) && number(get(type,"id"),f.typeObjectId) &&
                   hash(get(type,"abiId"),f.typeObjectAbi) && equal(get(type,"origin"),f.typeOrigin==1 ? "declaration" : f.typeOrigin==2 ? "inferred" : "syntax"));
        declared+=f.declaredSymbol!=0; target+=f.targetSymbol!=0; typed+=f.typeOrigin!=0;
    }
    CHECK(declared && target && typed);
    xls_index_fact untouched={.id=123};
    CHECK(!xlsSemanticIndexFact(index,xlsSemanticIndexCount(index),&untouched) && untouched.id==123);
    CHECK(!xlsSemanticIndexFact(NULL,0,&untouched) && !xlsSemanticIndexFact(index,0,NULL) && !xlsSemanticIndexCount(NULL));
    xrtValueRelease(raw); return 1;
}
static int capacity(void)
{
    xstrbuf source; xrtStrBufInit(&source);
    for(unsigned i=0;i<3000;++i) CHECK(xrtStrBufAppendFormat(&source,
        "func helper_%u(int value) as int\n return value + 1\nend\n",i));
    CHECK(xrtStrBufAppendFormat(&source,"func main(...) as int\n return helper_2999(7)\nend\n"));
    char *path=xrtPathAbs("typed-capacity.xl"),error[256]; CHECK(path);
    xls_semantic_input input={path,source.Data,source.Size};
    xlsFrontendTypeAliasReset();
    xls_semantic_check* result=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    if(!result) fprintf(stderr,"capacity check: %s\n",error);
    CHECK(result && xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckInfo(result).hasIndex);
    xls_type_alias_work lookup=xlsFrontendTypeAliasWork();
    CHECK(lookup.indexes>=2 && lookup.indexedQueries && lookup.queries>=lookup.indexedQueries && lookup.peakBytes<=8u*1024u*1024u);
    const xls_semantic_index* index=xlsSemanticCheckTypedIndex(result); size_t count=xlsSemanticIndexCount(index);
    size_t budget=xlsFrontendIndexBudgetUsed(index);
    CHECK(count*1536>32u*1024u*1024u && count<65536 && budget<32u*1024u*1024u && !xlsFrontendIndexBudgetUsed(NULL));
    uint64_t last=0; unsigned references=0,typed=0;
    for(size_t i=0;i<count;++i) {
        xls_index_fact f; CHECK(xlsSemanticIndexFact(index,i,&f));
        CHECK(f.id==i+1 && f.parent<f.id && f.source<=1 && f.selectionSource<=1);
        if(f.selectionRange.known) {
            CHECK(f.selectionRange.startOffset<=f.selectionRange.endOffset && f.selectionRange.endOffset<=source.Size);
            size_t length=f.selectionRange.endOffset-f.selectionRange.startOffset;
            if(f.declaredSymbol && length==strlen("helper_2999") &&
               !memcmp(source.Data+f.selectionRange.startOffset,"helper_2999",length)) last=f.declaredSymbol;
        }
        typed+=f.typeOrigin!=0;
    }
    CHECK(last && typed>3000);
    size_t use=(size_t)(strstr(source.Data,"return helper_2999")-source.Data)+strlen("return ");
    for(size_t i=0;i<count;++i) {
        xls_index_fact f; CHECK(xlsSemanticIndexFact(index,i,&f));
        references+=f.tokenRange.known && f.tokenRange.startOffset==use && (f.symbol==last || f.targetSymbol==last);
    }
    CHECK(references);
    /* Native typed capture has a different allocation representation. The
     * unchanged shared JSON serializer must still enforce its old policy. */
    CHECK(!xlsSemanticCheckIndex(result) && xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckTypedIndex(result)==index);
    printf("typed capacity facts=%zu, charged data=%zu bytes; legacy JSON allowance still rejects\n",count,budget);
    xlsSemanticCheckDestroy(result);
    /* Fail during collection after a nonempty alias scope was entered, then
     * prove that an unrelated canonical query is outside that failed scope. */
    for(unsigned i=3000;i<6000;++i) CHECK(xrtStrBufAppendFormat(&source,
        "func helper_%u(int value) as int\n return value + 1\nend\n",i));
    CHECK(xrtStrBufAppendFormat(&source,"typedef Count = int\ntypedef Other = Count\n"));
    input=(xls_semantic_input){path,source.Data,source.Size};
    result=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    CHECK(!result && strstr(error,"tree limit"));
    XL6_AstNode probe={0}; probe.kind=XL6_AST_TYPE; probe.text="int"; xlsFrontendTypeAliasReset();
    CHECK(xl6TypeResolveAlias(NULL,&probe)==&probe && !xlsFrontendTypeAliasWork().queries);
    xrtStrBufFree(&source); xrtFree(path); return 1;
}
static int run(void)
{
    char* path=xrtPathAbs("typed-index-memory.xl"); char* dep=xrtPathAbs("typed_index_dep.xl"); CHECK(path && dep);
    const char* source="\xef\xbb\xbf" "class Box\r\n int value\r\n func answer() as int\r\n return self.value\r\n end\r\nend\r\nfunc main(...) as int\r\n string 结果 = \"😀\"\r\n auto box = Box()\r\n return box.answer()\r\nend\r\n";
    xls_semantic_input inputs[2]={{path,source,strlen(source)},{0}}; char error[256];
    xls_semantic_check* first=xlsSemanticCheckCreate(path,inputs,1,1,NULL,NULL,error,sizeof(error));
    if(!first) fprintf(stderr,"typed check: %s\n",error);
    CHECK(verify(first));
    const char* generics="protocol Named\n func name() as string\nend\nclass Base\n int value\nend\nclass Child extends Base\nend\nfunc keep<T>(T value) as T\n return value\nend\nfunc bounded<T is Named>(T value) as int\n return 0\nend\nfunc main(...) as int\n return keep::<int>(7)\nend\n";
    inputs[0]=(xls_semantic_input){path,generics,strlen(generics)};
    xls_semantic_check* second=xlsSemanticCheckCreate(path,inputs,1,1,NULL,NULL,error,sizeof(error));
    CHECK(verify(second)); CHECK(verify(first)); /* Later AST/type context cannot alter first facts. */
    xlsSemanticCheckDestroy(second);
    const char* aliases="typedef Count = int\ntypedef Other = Count\nfunc keep(Other value) as Count\n return value\nend\nfunc main(...) as int\n return keep(7)\nend\n";
    inputs[0]=(xls_semantic_input){path,aliases,strlen(aliases)};
    second=xlsSemanticCheckCreate(path,inputs,1,1,NULL,NULL,error,sizeof(error)); CHECK(verify(second)); xlsSemanticCheckDestroy(second);
    const char* entry="import typed_index_dep\nfunc main(...) as int\n return typed_index_dep.value()\nend\n";
    const char* dependency="export func value() as int\n return 7\nend\n";
    inputs[0]=(xls_semantic_input){path,entry,strlen(entry)}; inputs[1]=(xls_semantic_input){dep,dependency,strlen(dependency)};
    second=xlsSemanticCheckCreate(path,inputs,2,1,NULL,NULL,error,sizeof(error)); CHECK(verify(second)); xlsSemanticCheckDestroy(second);
    const char* bad="func main(...) as int\n return missing\nend\n";
    inputs[0]=(xls_semantic_input){path,bad,strlen(bad)};
    second=xlsSemanticCheckCreate(path,inputs,1,1,NULL,NULL,error,sizeof(error));
    CHECK(second && !xlsSemanticCheckInfo(second).accepted && !xlsSemanticCheckTypedIndex(second));
    xlsSemanticCheckDestroy(second); xlsSemanticCheckDestroy(first); xrtFree(path); xrtFree(dep); CHECK(capacity()); return 1;
}
int main(void)
{
    if(!run()) return 1;
    printf("analysis_index_test passed: %u field/range/identity/ownership checks against shared compiler serialization.\n",checks);
    return 0;
}
