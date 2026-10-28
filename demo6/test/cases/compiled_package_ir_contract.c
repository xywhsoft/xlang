#include "src/compile/xl6_compile_unit.h"
#include "src/package/xl6_compiled_package.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "include/xruntime_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void checked(int ok,const XL6_DiagList* diagnostics)
{
    if(!ok)for(size_t i=0;i<diagnostics->count;++i)
        fprintf(stderr,"%s\n",diagnostics->items[i].message);
    assert(ok);
}

static void reject(const XL6_IrModule* module)
{
    XL6_DiagList diagnostics={0};
    assert(!xl6IrVerifyModule(module,&diagnostics));
    int found=0;
    for(size_t i=0;i<diagnostics.count;++i)
        found|=strstr(diagnostics.items[i].message,"package")!=NULL ||
            strstr(diagnostics.items[i].message,"parameter lifetime")!=NULL;
    assert(found);xl6DiagFree(&diagnostics);
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
}

static void fingerprintBytes(uint64_t* hash,const void* data,size_t size)
{
    for(size_t i=0;i<size;++i)*hash=(*hash^((const unsigned char*)data)[i])*UINT64_C(1099511628211);
}

static void fingerprintNode(uint64_t* hash,const XL6_AstNode* node)
{
    uint64_t facts[]={node->kind,node->flags,node->compiledPackageError,node->literalScalar,
        (unsigned)node->literalDecoded,node->literalSize,node->childCount};
    fingerprintBytes(hash,facts,sizeof(facts));
    if(node->text)fingerprintBytes(hash,node->text,strlen(node->text)+1);
    if(node->compiledPackageLink)fingerprintBytes(hash,node->compiledPackageLink,strlen(node->compiledPackageLink)+1);
    fingerprintBytes(hash,node->literalData,node->literalSize);
    for(size_t i=0;i<node->childCount;++i)fingerprintNode(hash,node->children[i]);
}

static void importFaults(const XL6_CompiledPackageSet* set,const char* source,const char* label)
{
    uint64_t expected=0;
    for(size_t run=0;run<4097;++run) {
        int baseline=run==0;size_t point=run?run-1:0;
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
        assert(xl6ParseSource(source,"bound_import.xl",&parsed));
        XL6_ImportDeps deps;xl6ImportDepsInit(&deps);deps.compiledPackages=set;
        if(!baseline)assert(xrtMemDebugFailAfter(point));
        int ok=xl6ImportsExpandModule(parsed.root,NULL,&parsed.diags,&deps);
        int hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(ok) {
            uint64_t actual=UINT64_C(14695981039346656037);fingerprintNode(&actual,parsed.root);
            if(baseline)expected=actual;
            else if(expected!=actual) {
                fprintf(stderr,"compiled import %s changed after allocation failure at %zu\n",label,point);assert(0);
            }
        } else assert(hit);
        xl6ImportDepsFree(&deps);xl6ParseResultFree(&parsed);balanced(&before);
        if(!baseline && !hit) {
            printf("compiled package import %s: complete allocation-failure prefixes=%zu\n",label,point+1);return;
        }
    }
    assert(0);
}

static void freePlans(XL6_IrModule* module)
{
    XL6_IrModule plans={0};plans.compiledPackages=module->compiledPackages;
    plans.compiledPackageCount=module->compiledPackageCount;
    module->compiledPackages=NULL;module->compiledPackageCount=0;
    xl6IrModuleFree(&plans);
}

int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    XL6_CompileUnit producer;xl6CompileUnitInit(&producer);
    checked(xl6CompileUnitLoadFile(&producer,"test/cases/compiled_package_host.xl",1),&producer.parse.diags);
    checked(xl6IrLowerModule(&producer.ir,&producer.parse.diags),&producer.parse.diags);
    checked(xl6CompileUnitPrepareCompiledPackages(&producer),&producer.parse.diags);
    assert(producer.ir.compiledPackageCount==4);
    uint64_t hashes[4];size_t entries[4];
    for(size_t p=0;p<4;++p){hashes[p]=producer.ir.compiledPackages[p].interfaceHash;entries[p]=producer.ir.compiledPackages[p].entryCount;}
    freePlans(&producer.ir);
    size_t point=0;
    for(;point<8192;++point) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));
        int ok=xl6CompileUnitPrepareCompiledPackages(&producer);
        int hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(ok) {
            assert(producer.ir.compiledPackageCount==4);
            for(size_t p=0;p<4;++p) {
                if(hashes[p]!=producer.ir.compiledPackages[p].interfaceHash || entries[p]!=producer.ir.compiledPackages[p].entryCount)
                    fprintf(stderr,"plan changed after allocation failure at %zu: %s\n",point,producer.ir.compiledPackages[p].name);
                assert(hashes[p]==producer.ir.compiledPackages[p].interfaceHash && entries[p]==producer.ir.compiledPackages[p].entryCount);
            }
            if(!hit)break;
            freePlans(&producer.ir);xl6DiagFree(&producer.parse.diags);balanced(&before);continue;
        }
        assert(!producer.ir.compiledPackages && !producer.ir.compiledPackageCount);
        xl6DiagFree(&producer.parse.diags);balanced(&before);
    }
    assert(point<8192 && producer.ir.compiledPackageCount==4);
    checked(xl6IrVerifyModule(&producer.ir,&producer.parse.diags),&producer.parse.diags);
    printf("compiled package plan: complete allocation-failure prefixes=%zu\n",point+1);
    XL6_IrCompiledPackage* package=&producer.ir.compiledPackages[0];
    package->interfaceHash^=1;reject(&producer.ir);package->interfaceHash^=1;
    unsigned int entry=package->entries[0];package->entries[0]=UINT32_MAX;
    reject(&producer.ir);package->entries[0]=entry;
    size_t count=producer.ir.compiledPackageCount;
    producer.ir.compiledPackageCount=65;reject(&producer.ir);producer.ir.compiledPackageCount=count;
    for(size_t p=0;p<count;++p)if(producer.ir.compiledPackages[p].dependencyCount) {
        size_t old=producer.ir.compiledPackages[p].dependencies[0];
        producer.ir.compiledPackages[p].dependencies[0]=p;reject(&producer.ir);
        producer.ir.compiledPackages[p].dependencies[0]=old;break;
    }
    XL_CompiledPackage records[64]={0};const XL_CompiledPackage* dependencies[64][64]={{0}};
    for(size_t p=0;p<count;++p) {
        XL6_IrCompiledPackage* source=&producer.ir.compiledPackages[p];
        records[p].name=source->name;records[p].interfaceData=source->interfaceData;
        records[p].interfaceSize=source->interfaceSize;records[p].interfaceHash=source->interfaceHash;
        for(size_t d=0;d<source->dependencyCount;++d)dependencies[p][d]=&records[source->dependencies[d]];
        records[p].dependencies=dependencies[p];records[p].dependencyCount=source->dependencyCount;
    }
    XL6_CompiledPackageSet set={records,count};
    importFaults(&set,"import compiled_leaf\nassert compiled_leaf.value()==21\n","simple");
    importFaults(&set,"import compiled_leaf as leaf\nfrom compiled_leaf import value as getValue\n"
        "assert leaf.value()==21 and getValue()==21\n","aliases");
    importFaults(&set,"import compiled_demo as lib\nfrom compiled_demo import Box as ImportedBox, add as sum\n"
        "assert sum(40)==42 and lib.answer==42\n","diamond");
    XL6_CompiledPackageSet incomplete={records,1};
    XL6_CompileUnit missing;xl6CompileUnitInit(&missing);missing.importDeps.compiledPackages=&incomplete;
    assert(!xl6CompileUnitLoadSource(&missing,"import compiled_demo\n","missing_bound_dependency.xl",1));
    int noFallback=0;
    for(size_t d=0;d<missing.parse.diags.count;++d)
        noFallback|=strstr(missing.parse.diags.items[d].message,"filesystem fallback is forbidden")!=NULL;
    assert(noFallback);xl6CompileUnitFree(&missing);xrtClearError();
    XL6_CompileUnit consumer;xl6CompileUnitInit(&consumer);consumer.importDeps.compiledPackages=&set;
    checked(xl6CompileUnitLoadFile(&consumer,"test/cases/compiled_package_script.xl",1),&consumer.parse.diags);
    checked(xl6IrLowerModule(&consumer.ir,&consumer.parse.diags),&consumer.parse.diags);
    checked(xl6IrVerifyModule(&consumer.ir,&consumer.parse.diags),&consumer.parse.diags);
    XL6_Output output;assert(xl6OutputOpenMemory(&output));
    XL6_IrCgenOptions options={0};options.emitScriptEntry=1;
    int emitted=xl6IrCgenEmitCEx(&output,&consumer.ir,&consumer.parse.diags,&options);
    size_t generatedSize=0;char* generated=xl6OutputTakeMemoryText(&output,&generatedSize);
    if(!emitted && generated)fprintf(stderr,"C emitter stopped after: %.4000s\n",generated+(generatedSize>4000?generatedSize-4000:0));
    if(emitted && argc>1) {
        FILE* file=fopen(argv[1],"wb");assert(file);
        assert(fwrite(generated,1,generatedSize,file)==generatedSize);assert(!fclose(file));
    }
    xrtFree(generated);xl6OutputDiscard(&output);checked(emitted,&consumer.parse.diags);
    size_t mutations=0;
    for(size_t n=0;n<consumer.ir.nodeCount;++n) {
        XL6_IrNode* node=&consumer.ir.nodes[n];if(!node->compiledPackageLink)continue;
        unsigned error=node->compiledPackageError;node->compiledPackageError=0;
        reject(&consumer.ir);node->compiledPackageError=error;++mutations;
        char first=node->compiledPackageLink[0];node->compiledPackageLink[0]='!';
        reject(&consumer.ir);node->compiledPackageLink[0]=first;++mutations;
    }
    for(size_t n=0;n<consumer.ir.nodeCount;++n) {
        XL6_IrNode* node=&consumer.ir.nodes[n];
        if(node->sourceKind!=XL6_AST_PARAM || !XL6_COMPILED_PARAM_LIFETIME_VALID(node->compiledPackageError))continue;
        unsigned saved=node->compiledPackageError;node->compiledPackageError=0;
        reject(&consumer.ir);node->compiledPackageError=saved;++mutations;
    }
    assert(mutations);
    for(size_t p=0;p<count;++p) {
        XL6_IrCompiledPackage* source=&producer.ir.compiledPackages[p];
        assert(xl6CompiledPackageInterfaceValid(source->interfaceData,source->interfaceSize));
        for(size_t n=0;n<source->interfaceSize;++n)
            assert(!xl6CompiledPackageInterfaceValid(source->interfaceData,n));
        size_t failure=0;
        for(;failure<4096;++failure) {
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(failure));
            XL6_AstNode* root=xl6CompiledPackageDecode(source->interfaceData,source->interfaceSize);
            int hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
            if(root)assert(!hit);else assert(hit);
            xl6AstFree(root);balanced(&before);
            if(!hit)break;
        }
        assert(failure<4096);
        printf("compiled interface %s: bytes=%zu, decode allocation-failure prefixes=%zu\n",
            source->name,source->interfaceSize,failure+1);
    }
    printf("compiled package IR: %zu rejected host binding mutations\n",mutations);
    if(argc>2) {
        FILE* file=fopen(argv[2],"wb");assert(file);fputs("{\"Packages\":[",file);
        for(size_t p=0;p<count;++p) {
            const XL6_IrCompiledPackage* value=&producer.ir.compiledPackages[p];
            fprintf(file,"%s{\"InterfaceBytes\":%zu,\"EntryRecords\":%zu,\"EntryBytes\":%zu,\"DescriptorBytes\":%zu,\"DependencyBytes\":%zu}",
                p?",":"",value->interfaceSize,value->entryCount,value->entryCount*sizeof(XL_PackageEntry),
                sizeof(XL_CompiledPackage),value->dependencyCount*sizeof(void*));
        }
        fputs("]}\n",file);assert(!fclose(file));
    }
    xl6CompileUnitFree(&consumer);xl6CompileUnitFree(&producer);
    return 0;
}
