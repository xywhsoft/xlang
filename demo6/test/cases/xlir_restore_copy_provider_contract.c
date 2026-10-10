#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>

static void boundedClosure(void)
{
    XL6_IrModule module={0}; XL6_IrType types[33]={0}; XL6_IrTypeId edges[32][2];
    module.types.items=types; module.types.count=33;
    types[0].kind=XL6_IR_TYPE_BUILTIN; types[0].cAbi=XL6_IR_C_ABI_I64;
    for (unsigned i=1;i<33;++i) {
        edges[i-1][0]=i; edges[i-1][1]=i;
        types[i].kind=XL6_IR_TYPE_UNION; types[i].cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
        types[i].params=edges[i-1]; types[i].paramCount=2;
    }
    assert(xl6IrValueCopyProviderAbiForType(&module,2,1)==XL6_IR_VALUE_COPY_ARRAY_WIRE_V1);
    /* A canonical DAG can revisit the same nodes exponentially. Exhaustion
     * must conservatively keep providers rather than recurse 2^32 times. */
    assert(xl6IrValueCopyProviderAbiForType(&module,33,1)==XL6_IR_VALUE_COPY_ARRAY_COLLECTIONS_WIRE_V1);
    types[1].kind=XL6_IR_TYPE_GENERIC_INSTANCE; types[1].nominalSymbol=1;
    assert(xl6IrValueCopyProviderAbiForType(&module,2,1)==XL6_IR_VALUE_COPY_ARRAY_COLLECTIONS_WIRE_V1);
    XL6_IrClassLayout layout={0}; layout.type=2;
    module.classLayouts=&layout; module.classLayoutCount=1; types[1].paramCount=0;
    assert(xl6IrValueCopyProviderAbiForType(&module,2,1)==XL6_IR_VALUE_COPY_ARRAY_WIRE_V1);
    layout.persistenceObjectAbi=XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_SCHEMA;
    assert(xl6IrValueCopyProviderAbiForType(&module,2,1)==XL6_IR_VALUE_COPY_ARRAY_COLLECTIONS_WIRE_V1);
    layout.persistenceObjectAbi=XL6_IR_PERSISTENCE_OBJECT_CLOSED_SCHEMA;
    XL6_IrClassField field={0}; field.type=1; layout.fieldCount=1;
    module.classFields=&field; module.classFieldCount=1;
    assert(xl6IrValueCopyProviderAbiForType(&module,2,1)==XL6_IR_VALUE_COPY_ARRAY_WIRE_V1);
    XL6_IrTypeId listElement=1;
    types[2].collectionAbi=XL6_IR_COLLECTION_LIST; types[2].kind=XL6_IR_TYPE_GENERIC_INSTANCE;
    types[2].nominalSymbol=0; types[2].params=&listElement; types[2].paramCount=1; field.type=3;
    assert(xl6IrValueCopyProviderAbiForType(&module,2,1)==XL6_IR_VALUE_COPY_ARRAY_LIST_WIRE_V1);
    assert(xl6IrValueCopyProviderAbiForType(&module,2,0)==XL6_IR_VALUE_COPY_ARRAY_WIRE_V1);
    layout.firstField=SIZE_MAX;
    assert(xl6IrValueCopyProviderAbiForType(&module,2,1)==XL6_IR_VALUE_COPY_ARRAY_COLLECTIONS_WIRE_V1);
    assert(xl6IrValueCopyProviderAbiForType(&module,0,1)==XL6_IR_VALUE_COPY_INVALID);
}
int main(int argc,char** argv)
{
    assert(argc==3 || argc==4); boundedClosure();
    for (unsigned i=1;i<3;++i) {
        assert(fixture_contract(argv[i]));
        XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[i],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        bool deep=false;
        for (size_t j=0;j<unit.ir.persistenceRestoreNodeCount;++j) {
            const XL6_IrPersistenceRestoreNode* node=&unit.ir.persistenceRestoreNodes[j];
            if (node->isolationAbi!=XL6_IR_PERSIST_ISOLATION_RUNTIME_DEEP) continue;
            deep=true; assert(xl6IrValueCopyProviderAbiForType(&unit.ir,node->payloadType,1)==
                (i==1 ? XL6_IR_VALUE_COPY_ARRAY_WIRE_V1 : XL6_IR_VALUE_COPY_ARRAY_COLLECTIONS_WIRE_V1));
        }
        assert(deep);
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY_COLLECTIONS)==(i==2));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_GRAPH)==(i==2));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_GRAPH)==(i==2));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_RESTORE_CLASS_COLLECTIONS)==(i==2));
        if (i==2 && argc==4) {
            XL6_Output out; assert(xl6OutputOpenFile(&out,argv[3])); unsigned bindings=0;
            for (size_t j=0;j<unit.ir.functionCount;++j) {
                const XL6_IrFunction* f=&unit.ir.functions[j];
                if (!f->debugName || strcmp(f->debugName,"restoreCopy")) continue;
                const XL6_IrType* type=xl6IrTypeGet(&unit.ir.types,f->returnType); assert(type);
                xl6OutputPrintf(&out,"#define XL6_RESTORE_COPY_FN xlir_fn_%u\n#define XL6_RESTORE_COPY_RESULT %s\n",
                    (unsigned)f->symbol,type->errorResultCName); ++bindings;
            }
            for (size_t j=0;j<unit.ir.storageCount;++j) {
                const XL6_IrStorage* s=&unit.ir.storages[j]; if (s->kind!=XL6_IR_STORAGE_GLOBAL) continue;
                for (size_t k=0;k<unit.ir.nodeCount;++k) {
                    const XL6_IrNode* n=&unit.ir.nodes[k];
                    if (n->symbol==s->symbol && n->text && !strcmp(n->text,"copyShared")) {
                        xl6OutputPrintf(&out,"#define XL6_RESTORE_COPY_SHARED s%u\n",(unsigned)s->symbol); ++bindings; break;
                    }
                }
            }
            assert(bindings==2); xl6OutputClose(&out);
        }
        xl6CompileUnitFree(&unit);
    }
    puts("Restore isolation: bounded typed closure, opaque generic, leaf pruning and open provider plans verified");
    return 0;
}
