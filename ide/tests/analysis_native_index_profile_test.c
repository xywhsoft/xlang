/* Original current checker -> real checked AST -> typed and original JSON
 * collectors. No mocked semantic facts, second resolver or cached checker. */
#include "analysis/frontend_index.h"
#include "analysis/frontend_types.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_sem_index.h"
#include "src/sem/xl6_type_registry.h"
#include "src/type/xl6_type_model.h"
#include <stdio.h>

char* xlsReferenceSemIndexJson(const XL6_AstNode*,const XL6_SourceReport*,char*,size_t);
static unsigned checks;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static unsigned receivers;
#endif
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_native_index_profile_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static const xvalue* get(const xvalue* v,const char* key){return xrtValueObjectGet(v,xrtStrView(key));}
static int number(const xvalue* v,uint64_t expected){int64 n;return xrtValueGetInt(v,&n) && n>=0 && (uint64_t)n==expected;}
static int equal(const xvalue* v,const char* text){xstrview s;return text && xrtValueGetString(v,&s) && s.Size==strlen(text) && !memcmp(s.Data,text,s.Size);}
static int abi(const xvalue* v,uint64_t id){char s[17];snprintf(s,sizeof(s),"%016llx",(unsigned long long)id);return equal(v,s);}
static int range(const xvalue* v,xls_index_range r)
{
    return !r.known ? xrtValueType(v)==XVALUE_NULL :
        number(get(v,"startOffset"),r.startOffset) && number(get(v,"endOffset"),r.endOffset) &&
        number(get(v,"startLine"),(unsigned)r.startLine) && number(get(v,"endLine"),(unsigned)r.endLine) &&
        number(get(v,"startColumn"),(unsigned)r.startColumn) && number(get(v,"endColumn"),(unsigned)r.endColumn);
}
static int astFacts(const XL6_AstNode* n,const xls_semantic_index* index,size_t* at,unsigned parent)
{
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    receivers+=n->semanticReceiverSymbolId!=0;
#endif
    xls_index_fact f;CHECK(xlsSemanticIndexFact(index,(*at)++,&f));
    CHECK(f.id==*at && f.parent==parent && !strcmp(f.kind,xl6AstKindName(n->kind)));
    CHECK(f.symbol==n->semanticSymbolId && f.declaredSymbol==n->semanticDeclaredSymbolId &&
        f.targetSymbol==n->semanticTargetSymbolId && f.compoundOperatorSymbol==n->semanticCompoundOperatorSymbolId &&
        f.typeId==n->semanticTypeId && f.typeAbi==n->semanticTypeAbiId);
    const XL6_AstNode* t=n->semanticDeclaredType ? n->semanticDeclaredType : n->inferredType ? n->inferredType : n->kind==XL6_AST_TYPE ? n : NULL;
    CHECK(f.typeOrigin==(t ? n->semanticDeclaredType ? 1 : n->inferredType ? 2 : 3 : 0));
    CHECK(f.typeObjectId==(t ? t->semanticTypeId : 0) && f.typeObjectAbi==(t ? t->semanticTypeAbiId : 0));
    for(size_t i=0;i<n->childCount;++i)CHECK(astFacts(n->children[i],index,at,f.id));
    return 1;
}
static int jsonFacts(const xls_semantic_index* index,const char* json)
{
    xjsonreadconfig config;xrtJsonReadConfigInit(&config);config.MaxInputBytes=32u*1024u*1024u;config.MaxValues=4u*1024u*1024u;config.MaxContainerItems=65536;
    xvalue* raw=xrtJsonRead(xrtStrView(json),&config);CHECK(raw);const xvalue* nodes=get(raw,"nodes");CHECK(xrtValueCount(nodes)==xlsSemanticIndexCount(index));
    for(size_t i=0;i<xlsSemanticIndexCount(index);++i){
        xls_index_fact f;CHECK(xlsSemanticIndexFact(index,i,&f));const xvalue* row=xrtValueArrayGet(nodes,i),*type=get(row,"type");
        CHECK(equal(get(row,"kind"),f.kind) && number(get(row,"id"),f.id) && number(get(row,"parent"),f.parent));
        CHECK(number(get(row,"sourceId"),f.source) && number(get(row,"selectionSourceId"),f.selectionSource));
        CHECK(number(get(row,"declaredSymbolId"),f.declaredSymbol) && number(get(row,"symbolId"),f.symbol) &&
            number(get(row,"targetSymbolId"),f.targetSymbol) && number(get(row,"compoundOperatorSymbolId"),f.compoundOperatorSymbol));
        CHECK(number(get(row,"declaredTypeId"),f.declaredType) && number(get(row,"referencedTypeId"),f.referencedType) &&
            number(get(row,"typeId"),f.typeId) && abi(get(row,"typeAbiId"),f.typeAbi));
        CHECK(range(get(row,"range"),f.range) && range(get(row,"tokenRange"),f.tokenRange) && range(get(row,"selectionRange"),f.selectionRange));
        CHECK(f.typeOrigin ? equal(get(type,"canonical"),f.canonical) && number(get(type,"id"),f.typeObjectId) &&
            abi(get(type,"abiId"),f.typeObjectAbi) && equal(get(type,"origin"),f.typeOrigin==1 ? "declaration" : f.typeOrigin==2 ? "inferred" : "syntax") :
            xrtValueType(type)==XVALUE_NULL && !f.canonical);
    }
    xrtValueRelease(raw);return 1;
}
static void diagnostics(const XL6_DiagList* d)
{for(size_t i=0;i<d->count;++i)fprintf(stderr,"%d:%d %s\n",d->items[i].line,d->items[i].column,d->items[i].message);}
static int checked(const char* source,const char* path,int want,int jsonParity)
{
    XL6_ParseResult p;xl6ParseResultInit(&p);CHECK(xl6ParseSource(source,path,&p));
    int accepted=xl6SemCheckModuleWithIndex(p.root,&p.diags,"index-profile",1);
    if(accepted!=want)diagnostics(&p.diags);
    CHECK(accepted==want);
    if(!want){CHECK(p.diags.count);xl6ParseResultFree(&p);return 1;}
    CHECK(!p.diags.count);
    XL6_SourceReportItem item={.path=(char*)path,.text=(char*)source,.size=strlen(source),.kind=XL6_SOURCE_REPORT_SOURCE};
    XL6_SourceReport report={.items=&item,.count=1,.capacity=1,.complete=1};char error[192];
    xls_semantic_index* index=xlsFrontendIndexCreate(p.root,&report,error,sizeof(error));CHECK(index && !*error);
    CHECK(xlsFrontendIndexBudgetUsed(index)>0 && xlsFrontendIndexBudgetUsed(index)<=32u*1024u*1024u);
    size_t at=0;CHECK(astFacts(p.root,index,&at,0) && at==xlsSemanticIndexCount(index));
    if(jsonParity){
        char* reference=xlsReferenceSemIndexJson(p.root,&report,error,sizeof(error));CHECK(reference && !*error);
        char* adapted=xl6SemIndexJson(p.root,&report,error,sizeof(error));CHECK(adapted && !strcmp(reference,adapted));
        CHECK(jsonFacts(index,reference));xrtFree(reference);xrtFree(adapted);
    }
    report.complete=0;CHECK(!xlsFrontendIndexCreate(p.root,&report,error,sizeof(error)) && *error);
    xlsFrontendIndexDestroy(index);xl6ParseResultFree(&p);return 1;
}
static int registry(void)
{
    XL6_TypeRegistry r;xl6TypeRegistryInit(&r);CHECK(xl6TypeRegistryAddBuiltins(&r));
    CHECK(xl6TypeRegistryFind(&r,"int") && xl6TypeRegistryFind(&r,"string"));
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    const char* names[]={"cstring","cbytes","SlotMapKey","SlotMap","OrderedMap","Queue","MpmcQueue"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)CHECK(xl6TypeRegistryFind(&r,names[i]));
    CHECK(xl6TypeRegistryIsNativeValueAbi(xl6TypeAbiIdText("xlang.runtime.slot-map-key.v1")));
    CHECK(!xl6TypeRegistryIsNativeValueAbi(xl6TypeAbiIdText("not-a-native-profile")));
    CHECK(!strcmp(xl6TypeCanonicalBuiltinName("Queue"),"MpmcQueue"));
#endif
    xl6TypeRegistryFree(&r);return 1;
}
static int run(void)
{
    char* path=xrtPathAbs("index-profile-中文😀.xl");CHECK(path);CHECK(registry());
    CHECK(checked("typedef Count = int\nfunc add(Count value) as Count\n return value+1\nend\nfunc use() as int\n return add(7)\nend\n",path,1,1));
    CHECK(checked("class Box\n int value = 7\n create()\n self.value=9\n end\n func read() as int\n return self.value\n end\nend\n",path,1,1));
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    CHECK(receivers>0);
#endif
    CHECK(checked("\xef\xbb\xbf""// 😀\r\nfunc identity(string 值) as string\r\n return 值\r\nend\r\n",path,1,1));
    CHECK(checked("func wrong() as int\n return unknown_name\nend\n",path,0,0));
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    CHECK(checked("typedef Callback = func(c_int,c_vararg) as c_int\nextern \"xls_probe_owned\" @owned func text() as string\n",path,1,1));
    CHECK(checked("func queue(Queue<int> value) as MpmcQueue<int>\n return value\nend\n",path,1,1));
    CHECK(checked("extern \"C\" @owned func wrong() as int\n",path,0,0));
    CHECK(checked("extern \"C\" @owned @no_raise func wrong() as string\n",path,0,0));
    CHECK(checked("extern \"xls_probe_bytes\" func wrong() as cbytes\n",path,0,0));
#endif
    for(unsigned count=32;count<=512;count*=16){
        xstrbuf body={0};for(unsigned i=0;i<count;++i)CHECK(xrtStrBufAppendFormat(&body,"func f_%u(int value) as int\n return value+1\nend\n",i));
        CHECK(checked(body.Data,path,1,count==32));xrtStrBufFree(&body);
    }
    xrtFree(path);return 1;
}
int main(void)
{if(!run())return 1;printf("analysis_native_index_profile_test passed: %u checks; original semantic checker/JSON parity,typed growth,registry and rejection contracts\n",checks);return 0;}
