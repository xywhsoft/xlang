#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>

int main(int argc,char** argv)
{
    assert(argc==4); unsigned bindings=0;
    for (unsigned lane=1;lane<3;++lane) {
        assert(fixture_contract(argv[lane])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_VALUE_ITERATOR)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_NATIVE)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_COLLECTION_VALUE_ITERATOR)==(lane==1));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_GRAPH));
        if (lane==1) {
            XL6_Output out; assert(xl6OutputOpenFile(&out,argv[3]));
            for (size_t i=0;i<unit.ir.functionCount;++i) {
                const XL6_IrFunction* f=&unit.ir.functions[i]; if (!f->debugName) continue;
                const char* name=!strcmp(f->debugName,"visitNativeSet") ? "TYPED" :
                    !strcmp(f->debugName,"visitNativeErasedSet") ? "ERASED" : NULL;
                if (!name) continue;
                const XL6_IrType* t=xl6IrTypeGet(&unit.ir.types,f->returnType); assert(t && t->errorResultCName);
                xl6OutputPrintf(&out,"#define XL6_SET_ITERATE_%s xlir_fn_%u\n#define XL6_SET_ITERATE_%s_RESULT %s\n",
                    name,(unsigned)f->symbol,name,t->errorResultCName); ++bindings;
            }
            for (size_t i=0;i<unit.ir.types.count;++i) {
                const XL6_IrType* t=&unit.ir.types.items[i];
                if (t->collectionAbi!=XL6_IR_COLLECTION_SET || t->paramCount!=1 || !t->params) continue;
                const XL6_IrType* e=xl6IrTypeGet(&unit.ir.types,t->params[0]);
                if (!e || e->cAbi!=XL6_IR_C_ABI_MANAGED_STRING || e->kind==XL6_IR_TYPE_NULLABLE) continue;
                const XL6_IrType* w=xl6IrTypeGet(&unit.ir.types,t->collectionValueSchemaType); assert(w);
                xl6OutputPrintf(&out,"#define XL6_SET_ITERATE_OWNER 0x%016llXULL\n#define XL6_SET_ITERATE_WITNESS 0x%016llXULL\n#define XL6_SET_ITERATE_ELEMENT 0x%016llXULL\n",
                    (unsigned long long)t->typeValueId,(unsigned long long)w->typeValueId,
                    (unsigned long long)e->typeValueId); ++bindings;
            }
            for (size_t i=0;i<unit.ir.storageCount;++i) {
                const XL6_IrStorage* s=&unit.ir.storages[i]; if (s->kind!=XL6_IR_STORAGE_GLOBAL) continue;
                for (size_t j=0;j<unit.ir.nodeCount;++j) {
                    const XL6_IrNode* n=&unit.ir.nodes[j];
                    if (n->symbol==s->symbol && n->text && !strcmp(n->text,"setIterationCleanups")) {
                        xl6OutputPrintf(&out,"#define XL6_SET_ITERATE_CLEANUPS s%u\n",(unsigned)s->symbol); ++bindings; break;
                    }
                }
            }
            assert(bindings==4); xl6OutputClose(&out);
        }
        xl6CompileUnitFree(&unit);
    }
    /* Test host input construction is a separately selected real provider
     * closure, not an unrecorded addition to generated code's dependencies. */
    XL6_ArtifactRuntimeSet roots={{0}}; XL6_ArtifactRuntimePlan input={0};
    const char* symbols[]={"xl6SequencePrimitiveCreateV1","xl6StringTypeV1","xl6StringConverterV1"};
    for (size_t i=0;i<sizeof(symbols)/sizeof(symbols[0]);++i) {
        XL6_ArtifactRuntimeId owner=xl6ArtifactRuntimeSymbolOwner(symbols[i]);
        assert(owner!=XL6_ARTIFACT_RUNTIME_COUNT && xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,owner));
    }
    assert(xl6ArtifactRuntimeClosePlan(&roots,&input));
    printf("{\"HostInputFactory\":true,\"Sources\":[");
    for (size_t i=0;i<input.sourceCount;++i) printf("%s\"%s\"",i ? "," : "",input.sources[i]);
    printf("],\"NativeModules\":[");
    for (size_t i=0;i<input.nativeModuleCount;++i) printf("%s\"%s\"",i ? "," : "",input.nativeModules[i]);
    puts("]}");
    puts("Set iteration contract: Array excludes Set and unused graph providers"); return 0;
}
