#include "analysis_test_clock.h"
#include "analysis/function_unit_internal.h"
#include "analysis/check.h"
#include "analysis/frontend_internal.h"
#include "analysis/module_store.h"
#include <stdio.h>
#include <string.h>
/* Keep the independent full fresh semantic/AST/index/IR reference tests. */
#define main functionReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"analysis_function_unit_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)

static int sameSource(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    CHECK(!!a->source==!!b->source);
    if(a->source)CHECK(sameText(a->source->path,b->source->path) && a->source->completionQuery==b->source->completionQuery && a->source->completionStart==b->source->completionStart && a->source->completionEnd==b->source->completionEnd && a->source!=b->source);
    for(size_t i=0;i<a->childCount;++i)CHECK(sameSource(a->children[i],b->children[i]));
    CHECK(sameSource(a->inferredType,b->inferredType) && sameSource(a->semanticDeclaredType,b->semanticDeclaredType));return 1;
}
static int sameIndexRange(xls_index_range a,xls_index_range b)
{return a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn && a.known==b.known;}
static int sameFact(xls_index_fact a,xls_index_fact b)
{
    return sameText(a.kind,b.kind) && sameText(a.canonical,b.canonical) && a.id==b.id && a.parent==b.parent && a.source==b.source && a.selectionSource==b.selectionSource &&
        a.declaredSymbol==b.declaredSymbol && a.symbol==b.symbol && a.targetSymbol==b.targetSymbol && a.compoundOperatorSymbol==b.compoundOperatorSymbol &&
        a.declaredType==b.declaredType && a.referencedType==b.referencedType && a.typeId==b.typeId && a.typeObjectId==b.typeObjectId &&
        a.typeAbi==b.typeAbi && a.typeObjectAbi==b.typeObjectAbi && a.typeOrigin==b.typeOrigin && sameIndexRange(a.range,b.range) && sameIndexRange(a.tokenRange,b.tokenRange) && sameIndexRange(a.selectionRange,b.selectionRange);
}
static xls_index_range plainRange(XL6_AstRange r)
{return(xls_index_range){r.startOffset,r.endOffset,r.startLine,r.startColumn,r.endLine,r.endColumn,r.known};}
static int nodeValues(const xls_function_unit* unit,uint32_t id,uint32_t parent,const XL6_AstNode* expected,size_t* nodes)
{
    if(!expected){CHECK(!id);return 1;}
    xls_function_node n;CHECK(xlsFunctionUnitNode(unit,id,&n));++*nodes;
    CHECK(n.id==id && n.parent==parent && n.kind==(uint32_t)expected->kind && sameText(n.kindName,xl6AstKindName(expected->kind)) && n.flags==expected->flags);
    CHECK(sameText(n.text,expected->text) && sameText(n.doc,expected->doc) && !!n.literalData==!!expected->literalData && n.literalSize==expected->literalSize && (!n.literalData || !memcmp(n.literalData,expected->literalData,n.literalSize)) && n.literalScalar==expected->literalScalar && n.literalDecoded==expected->literalDecoded);
    CHECK(n.type==expected->semanticTypeId && n.typeAbi==expected->semanticTypeAbiId && n.symbol==expected->semanticSymbolId && n.declaredSymbol==expected->semanticDeclaredSymbolId && n.targetSymbol==expected->semanticTargetSymbolId && n.compoundOperatorSymbol==expected->semanticCompoundOperatorSymbolId);
    CHECK(n.call==(int)expected->semanticCallKind && n.lvalue==(int)expected->semanticLvalueKind && n.ownership==(int)expected->semanticOwnership && n.exit==(int)expected->semanticExitKind && n.scope==expected->semanticScopeDepth && n.targetScope==expected->semanticTargetScopeDepth && n.region==expected->semanticRegionId && n.targetRegion==expected->semanticTargetRegionId);
    CHECK(n.line==expected->line && n.column==expected->column && sameIndexRange(n.range,plainRange(expected->range)) && sameIndexRange(n.tokenRange,plainRange(expected->tokenRange)));
    CHECK(n.hasSource==!!expected->source && sameText(n.sourcePath,xl6AstSourcePath(expected)));
    if(expected->source)CHECK(n.completionQuery==expected->source->completionQuery && n.completionStart==expected->source->completionStart && n.completionEnd==expected->source->completionEnd);
    CHECK(n.children==expected->childCount);
    for(size_t i=0;i<n.children;++i){uint32_t child;CHECK(xlsFunctionUnitChild(unit,id,i,&child) && nodeValues(unit,child,id,expected->children[i],nodes));}
    CHECK(nodeValues(unit,n.inferredType,id,expected->inferredType,nodes) && nodeValues(unit,n.declaredType,id,expected->semanticDeclaredType,nodes));
    uint32_t bad=99;CHECK(!xlsFunctionUnitChild(unit,id,n.children,&bad) && !bad);return 1;
}
static const XL6_AstNode* factNode(const XL6_AstNode* node,unsigned* remaining)
{
    if(!node)return NULL;
    if(--*remaining==0)return node;
    for(size_t i=0;i<node->childCount;++i){const XL6_AstNode* found=factNode(node->children[i],remaining);if(found)return found;}return NULL;
}
typedef struct unitReader {xls_function_unit* unit;int passed;} unitReader;
static int32 readUnit(void* user)
{
    unitReader* reader=user;reader->passed=0;xls_function_unit* unit=xlsFunctionUnitRetain(reader->unit);
    XL6_AstNode* clone=xlsFrontendFunctionUnitMaterialize(unit,64u*1024u*1024u,NULL,NULL);size_t nodes=0;
    if(clone && sameAst(clone,xlsFrontendFunctionUnitRoot(unit)) && nodeValues(unit,1,0,clone,&nodes) && nodes==xlsFunctionUnitInfo(unit).nodes)reader->passed=1;
    xl6AstFree(clone);xlsFunctionUnitRelease(unit);return 0;
}
typedef struct cancelCopy {size_t calls,fail;} cancelCopy;
static int stopCopy(void* user){cancelCopy* c=user;return ++c->calls==c->fail;}
static int inputValues(xls_function_unit* unit,const xls_semantic_check* check)
{
    xls_inputs a,b;CHECK(xlsFunctionUnitInputs(unit,&a) && xlsSemanticCheckInputs(check,&b));CHECK(a.sources==b.sources && a.probes==b.probes && sameText(a.packagePath,b.packagePath) && a.packagePathObserved==b.packagePathObserved);
    for(size_t i=0;i<a.sources;++i){xls_function_source own;xls_check_source original;CHECK(xlsFunctionUnitSource(unit,i,&own) && xlsSemanticCheckSource(check,i,&original));CHECK(sameText(own.path,original.path) && own.path!=original.path && sameText(own.origin,original.origin) && (!own.origin || own.origin!=original.origin) && own.size==original.size && own.rawSize==original.rawSize && own.kind==original.kind && !memcmp(own.textSha256,original.textSha256,32) && !memcmp(own.rawSha256,original.rawSha256,32));}
    for(size_t i=0;i<a.probes;++i){xls_input_probe own,original;CHECK(xlsFunctionUnitProbe(unit,i,&own) && xlsSemanticCheckProbe(check,i,&original));CHECK(sameText(own.path,original.path) && own.path!=original.path && own.exists==original.exists);}
    return 1;
}
static int unitCase(const char* text,int captureIndex,int cancellation)
{
    char* path=xrtPathAbs("function-unit-中文😀.xl");CHECK(path);xls_semantic_input input={path,text,strlen(text)};char error[256];
    xls_semantic_check* check=xlsSemanticCheckCreate(path,&input,1,captureIndex,NULL,NULL,error,sizeof(error));
    xls_semantic_check* reference=xlsSemanticCheckCreate(path,&input,1,captureIndex,NULL,NULL,error,sizeof(error));
    xls_semantic_check* selector=captureIndex ? reference : xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    if(check && !xlsSemanticCheckInfo(check).accepted){char* report=xlsSemanticCheckJson(check,NULL);fprintf(stderr,"unit corpus rejected:%s\n",report?report:"no report");xrtFree(report);}
    CHECK(check && reference && selector && xlsSemanticCheckInfo(check).accepted && xlsSemanticCheckInfo(reference).accepted && xlsSemanticCheckInfo(selector).accepted);
    size_t declared=0,ordinal=0;CHECK(xlsSemanticCheckFunctionCount(check,&declared) && declared);
    xls_function_set* set=xlsSemanticCheckFunctionsCapture(check,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(set);
    xls_function_set_info setInfo=xlsFunctionSetInfo(set);CHECK(setInfo.functions==declared && setInfo.ownedBytes<=64u*1024u*1024u && !xlsFunctionSetUnit(set,declared));
    xls_function_unit* firstBatch=xlsFunctionSetUnit(set,0);xls_function_source sharedOrigin;CHECK(firstBatch && xlsFunctionUnitSource(firstBatch,0,&sharedOrigin));xlsFunctionUnitRelease(firstBatch);
    const xls_semantic_index* index=xlsSemanticCheckTypedIndex(selector);xls_function_unit* held=NULL;
    for(size_t i=0;i<xlsSemanticIndexCount(index);++i){xls_index_fact fact;CHECK(xlsSemanticIndexFact(index,i,&fact));
        int function=!strcmp(fact.kind,xl6AstKindName(XL6_AST_FUNC)) || !strcmp(fact.kind,xl6AstKindName(XL6_AST_ASYNC_FUNC)) || !strcmp(fact.kind,xl6AstKindName(XL6_AST_EXPORT_FUNC)) || !strcmp(fact.kind,xl6AstKindName(XL6_AST_ANON_FUNC));if(!function)continue;
        unsigned remaining=fact.id;const XL6_AstNode* expected=factNode(xlsSemanticCheckAst(reference),&remaining);CHECK(expected);
        xls_function_unit* unit=xlsSemanticCheckFunctionCapture(check,ordinal++,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(unit && !*error);
        xls_function_unit* batched=xlsFunctionSetUnit(set,ordinal-1);xls_function_source shared;CHECK(batched && xlsFunctionUnitSource(batched,0,&shared) && shared.path==sharedOrigin.path && sameAst(xlsFrontendFunctionUnitRoot(batched),expected));xlsFunctionUnitRelease(batched);
        const char* expectedName=expected->childCount && expected->children[0] ? expected->children[0]->text : NULL;
        xls_function_info info=xlsFunctionUnitInfo(unit);CHECK(info.nodes && info.treeBytes && info.ownedBytes<=64u*1024u*1024u && info.originBytes<=16u*1024u*1024u && sameText(info.name,expectedName) && sameText(info.path,xl6AstSourcePath(expected)));
        size_t nodes=0;CHECK(nodeValues(unit,1,0,expected,&nodes) && nodes==info.nodes && sameAst(xlsFrontendFunctionUnitRoot(unit),expected) && sameSource(xlsFrontendFunctionUnitRoot(unit),expected));
        CHECK(inputValues(unit,check));
        if(captureIndex){CHECK(info.facts>0);for(size_t j=0;j<info.facts;++j){xls_index_fact a,b;CHECK(xlsFunctionUnitFact(unit,j,&a) && xlsSemanticIndexFact(index,a.id-1,&b) && sameFact(a,b) && (!a.canonical || a.canonical!=b.canonical));}}
        else CHECK(!info.facts && !info.entityDomain && !xlsFunctionUnitEntities(unit));
        XL6_AstNode* copy=xlsFrontendFunctionUnitMaterialize(unit,64u*1024u*1024u,NULL,NULL);CHECK(copy && copy!=expected && sameAst(copy,expected) && sameSource(copy,expected));copy->flags^=1;CHECK(xlsFrontendFunctionUnitRoot(unit)->flags==expected->flags);xl6AstFree(copy);
        CHECK(!xlsSemanticCheckFunctionCapture(check,ordinal-1,info.ownedBytes-1,NULL,NULL,error,sizeof(error)) && *error && xlsSemanticCheckInfo(check).accepted);
        xls_function_node invalid={.id=999};CHECK(!xlsFunctionUnitNode(unit,0,&invalid) && !invalid.id && !xlsFunctionUnitNode(unit,UINT32_MAX,&invalid));
        if(!held)held=xlsFunctionUnitRetain(unit);
        xlsFunctionUnitRelease(unit);
    }
    CHECK(ordinal==declared && !xlsSemanticCheckFunctionCapture(check,declared,64u*1024u*1024u,NULL,NULL,error,sizeof(error)));
    CHECK(!xlsSemanticCheckFunctionsCapture(check,setInfo.ownedBytes-1,NULL,NULL,error,sizeof(error)) && *error && xlsSemanticCheckInfo(check).accepted);
    if(cancellation){cancelCopy c={0};xls_function_unit* normal=xlsSemanticCheckFunctionCapture(check,0,64u*1024u*1024u,stopCopy,&c,error,sizeof(error));CHECK(normal && c.calls>4);xlsFunctionUnitRelease(normal);size_t calls=c.calls;
        for(size_t at=1;at<=calls;++at){c=(cancelCopy){0,at};CHECK(!xlsSemanticCheckFunctionCapture(check,0,64u*1024u*1024u,stopCopy,&c,error,sizeof(error)) && *error && xlsSemanticCheckInfo(check).accepted);}printf("function capture cancellation points=%zu\n",calls);
        c=(cancelCopy){0};xls_function_set* normalSet=xlsSemanticCheckFunctionsCapture(check,64u*1024u*1024u,stopCopy,&c,error,sizeof(error));CHECK(normalSet && c.calls>4);xlsFunctionSetRelease(normalSet);calls=c.calls;
        for(size_t at=1;at<=calls;++at){c=(cancelCopy){0,at};CHECK(!xlsSemanticCheckFunctionsCapture(check,64u*1024u*1024u,stopCopy,&c,error,sizeof(error)) && *error && xlsSemanticCheckInfo(check).accepted);}printf("function set cancellation points=%zu\n",calls);
    }
    xlsSemanticCheckDestroy(check);if(selector!=reference)xlsSemanticCheckDestroy(selector);xlsSemanticCheckDestroy(reference);xrtFree(path);
    CHECK(held && xlsFunctionUnitInfo(held).nodes);xls_inputs inputs;CHECK(xlsFunctionUnitInputs(held,&inputs) && inputs.sources);xls_function_source source;CHECK(xlsFunctionUnitSource(held,0,&source) && source.path && source.size);
    firstBatch=xlsFunctionSetUnit(set,declared-1);CHECK(firstBatch && xlsFunctionUnitInfo(firstBatch).nodes);xlsFunctionSetRelease(set);CHECK(xlsFunctionUnitInfo(firstBatch).nodes);xlsFunctionUnitRelease(firstBatch);
    unitReader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(unitReader){held,0};threads[i]=xrtThreadCreate(readUnit,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}xlsFunctionUnitRelease(held);return 1;
}
static int runUnits(void)
{
    const char* corpus[]={
        "\xef\xbb\xbf// 中文😀\r\nfunc main(...) as int\r\n string text=\"a\\0😀\"\r\n int local=7\r\n return local\r\nend\r\n",
        "func loop(int count) as int\n while count > 0\n count = count - 1\n if count > 5\n continue\n end\n while count > 1\n break 2\n end\n break\n end\n return count\nend\n",
        "func guarded() as int\n string outer = \"outer\"\n try\n string inner = \"inner\"\n return 7\n catch e\n string message = e.message\n return 8\n finally\n string final = \"final\"\n end\nend\n",
        "func closure() as func() as int\n array values = [41]\n return func() as int\n return int(values[0])\n end\nend\n",
        "func values(int count) as generator<string>\n string suffix = \"owned\"\n for i in range(count)\n string item = suffix + string(i)\n yield item\n end\nend\n",
        "class Box\n string item = \"owned\"\n func value() as string\n string local = self.item\n return local\n end\nend\nfunc read(Box item) as string\n return item.value()\nend\n",
        "protocol Named\n func name() as string\nend\ntypedef Count = int\nfunc named(Named item) as Count\n return 0\nend\n",
        "func identity<T>(T value) as T\n return value\nend\nfunc run() as int\n return identity::<int>(7)\nend\n",
        "enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n"
    };
    for(size_t i=0;i<sizeof(corpus)/sizeof(*corpus);++i)CHECK(unitCase(corpus[i],1,i==0));
    CHECK(unitCase(corpus[0],0,0));return 1;
}
static int originLifetime(void)
{
    char relative[160];snprintf(relative,sizeof(relative),"build/function-unit-origin-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xlsTestClockMicros());
    char* directory=xrtPathAbs(relative);char* entry=directory?xrtPathJoin(directory,"main.xl"):NULL;char* dep=directory?xrtPathJoin(directory,"dep.xl"):NULL;
    CHECK(directory && entry && dep && xrtDirCreateAll(directory));
    const char* mainText="import dep\nfunc main(...) as int\n return dep.value(7)\nend\n";
    const char* depText="export func value(int item) as int\n return item+1\nend\n";
    const char* changed="export func value(int item) as int\n return item+2\nend\n";
    CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)mainText,strlen(mainText)}) && xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)depText,strlen(depText)}));
    const char* members[]={entry,dep};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=entry,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="function-unit-origin",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};
    xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_module_store* store=xlsModuleStoreCreate(NULL);xls_entry_cache* cache=xlsEntryCacheCreate(1024*1024);CHECK(context && store && cache);xls_module_scope scope;xlsModuleScopeBegin(&scope,store);
    char error[256];xls_semantic_check* check=xlsSemanticCheckCreateCachedInContext(context,cache,entry,NULL,0,1,NULL,NULL,NULL,NULL,error,sizeof(error));CHECK(check && xlsSemanticCheckInfo(check).accepted);
    xls_function_unit* held=xlsSemanticCheckFunctionCapture(check,0,1024*1024,NULL,NULL,error,sizeof(error));CHECK(held && inputValues(held,check) && xlsFunctionUnitContext(held)==context && xlsFunctionUnitModules(held) && xlsFunctionUnitEntities(held) && xlsFunctionUnitInfo(held).entityDomain);
    xls_inputs inputs;CHECK(xlsFunctionUnitInputs(held,&inputs) && inputs.sources==2 && inputs.probes);
    unsigned char digest[32]={0};int found=0;
    for(size_t i=0;i<inputs.sources;++i){xls_function_source source;CHECK(xlsFunctionUnitSource(held,i,&source));if(!strcmp(source.path,dep)){memcpy(digest,source.textSha256,32);found=1;}}CHECK(found);
    CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)changed,strlen(changed)}));
    CHECK(!xlsSemanticCheckFunctionCapture(check,0,1024*1024,NULL,NULL,error,sizeof(error)) && *error);
    size_t count=999;CHECK(!xlsSemanticCheckFunctionCount(check,&count) && !count && !xlsSemanticCheckValidate(check));
    xls_semantic_check* next=xlsSemanticCheckCreateCachedInContext(context,cache,entry,NULL,0,1,NULL,NULL,NULL,NULL,error,sizeof(error));CHECK(next && xlsSemanticCheckInfo(next).accepted);
    xls_function_unit* freshUnit=xlsSemanticCheckFunctionCapture(next,0,1024*1024,NULL,NULL,error,sizeof(error));CHECK(freshUnit && xlsFunctionUnitInfo(freshUnit).entityDomain==xlsFunctionUnitInfo(held).entityDomain);
    found=0;CHECK(xlsFunctionUnitInputs(freshUnit,&inputs));for(size_t i=0;i<inputs.sources;++i){xls_function_source source;CHECK(xlsFunctionUnitSource(freshUnit,i,&source));if(!strcmp(source.path,dep)){CHECK(memcmp(digest,source.textSha256,32));found=1;}}CHECK(found);
    xlsFunctionUnitRelease(freshUnit);xlsSemanticCheckDestroy(next);xlsSemanticCheckDestroy(check);xlsModuleScopeEnd(&scope);xlsEntryCacheDestroy(cache);xlsModuleStoreRelease(store);xlsContextRelease(context);
    CHECK(xlsFunctionUnitContext(held) && xlsFunctionUnitModules(held) && xlsEntityViewInfo(xlsFunctionUnitEntities(held)).domain==xlsFunctionUnitInfo(held).entityDomain);
    CHECK(xlsFunctionUnitInputs(held,&inputs));found=0;for(size_t i=0;i<inputs.sources;++i){xls_function_source source;CHECK(xlsFunctionUnitSource(held,i,&source));if(!strcmp(source.path,dep)){CHECK(!memcmp(digest,source.textSha256,32));found=1;}}CHECK(found);
    XL6_AstNode* copy=xlsFrontendFunctionUnitMaterialize(held,1024*1024,NULL,NULL);CHECK(copy && sameAst(copy,xlsFrontendFunctionUnitRoot(held)));xl6AstFree(copy);xlsFunctionUnitRelease(held);xrtFree(directory);xrtFree(entry);xrtFree(dep);return 1;
}
typedef struct reentrantCopy {xls_semantic_check* check;size_t calls;int released,releaseOwner,failed;} reentrantCopy;
static int reenterCapture(void* user)
{
    reentrantCopy* state=user;++state->calls;
    if(state->releaseOwner){if(!state->released){xlsSemanticCheckDestroy(state->check);state->released=1;}return 0;}
    size_t count=0;if(!xlsSemanticCheckValidate(state->check) || !xlsSemanticCheckFunctionCount(state->check,&count) || !count){state->failed=1;return 1;}return 0;
}
static int captureLifetime(void)
{
    const char* body="func first(int value) as int\n return value+1\nend\nfunc main(...) as int\n return first(7)\nend\n";char* path=xrtPathAbs("function-unit-reentrant.xl");CHECK(path);xls_semantic_input input={path,body,strlen(body)};char error[256];
    for(unsigned releaseOwner=0;releaseOwner<2;++releaseOwner)for(unsigned bulk=0;bulk<2;++bulk){
        xls_semantic_check* check=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));CHECK(check && xlsSemanticCheckInfo(check).accepted);
        reentrantCopy state={.check=check,.releaseOwner=(int)releaseOwner};xls_function_unit* unit=NULL;xls_function_set* set=NULL;
        if(bulk){set=xlsSemanticCheckFunctionsCapture(check,1024*1024,reenterCapture,&state,error,sizeof(error));CHECK(set && xlsFunctionSetInfo(set).functions==2);unit=xlsFunctionSetUnit(set,1);xlsFunctionSetRelease(set);}
        else unit=xlsSemanticCheckFunctionCapture(check,0,1024*1024,reenterCapture,&state,error,sizeof(error));
        CHECK(unit && state.calls>4 && !state.failed && state.released==(int)releaseOwner);
        if(!releaseOwner)xlsSemanticCheckDestroy(check);
        XL6_AstNode* copy=xlsFrontendFunctionUnitMaterialize(unit,1024*1024,NULL,NULL);CHECK(copy && sameAst(copy,xlsFrontendFunctionUnitRoot(unit)));xl6AstFree(copy);xlsFunctionUnitRelease(unit);
    }
    xrtFree(path);return 1;
}
int main(void)
{
    if(functionReferenceMain() || !runUnits() || !originLifetime() || !captureLifetime())return 1;
    printf("analysis_function_unit_test passed:%u checks;complete fresh typed node/type/source/index parity,independent lifetime,exact quotas,cancellation and4readers\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
