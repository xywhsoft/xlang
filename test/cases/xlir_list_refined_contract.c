#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t length=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"REFUSED_REFINED_KEY",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(int argc,char** argv)
{
    assert(argc==4 && fixture_contract(argv[1]) && fixture_contract(argv[3]));
    XL6_CompileUnit array; xl6CompileUnitInit(&array);
    assert(xl6CompileUnitLoadFile(&array,argv[3],1) && xl6IrLowerModule(&array.ir,&array.parse.diags));
    XL6_ArtifactRuntimePlan arrayPlan={0};
    assert(xl6ArtifactRuntimePlanBuild(&array.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&arrayPlan,&array.parse.diags));
    assert(!xl6ArtifactRuntimeSetHas(&arrayPlan.components,XL6_ARTIFACT_LIST_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&arrayPlan.components,XL6_ARTIFACT_LIST_METHODS));
    assert(!xl6ArtifactRuntimeSetHas(&arrayPlan.components,XL6_ARTIFACT_LIST_VALUE_ITERATOR));
    xl6CompileUnitFree(&array);
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_METHODS));
    unsigned projections=0,mutations=0;
    for (size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* item=&unit.ir.instructions[i];
        if (item->kind!=XL6_IR_INSTR_RUNTIME_CALL || !item->payloadType ||
            (item->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64 &&
             item->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64 &&
             item->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY)) continue;
        XL6_IrInstruction saved=*item;
#define MUTATE(Field,Value) do { item->Field=(Value); refused(&unit.ir); *item=saved; ++mutations; } while(0)
        MUTATE(payloadType,XL6_IR_TYPE_INVALID); MUTATE(payloadType,UINT32_MAX);
        MUTATE(type,XL6_IR_TYPE_INVALID); MUTATE(operandCount,0); MUTATE(firstOperand,SIZE_MAX);
        MUTATE(runtimeOp,XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE);
        MUTATE(runtimeOp,saved.runtimeOp==XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64
            ? XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64 : XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64);
#undef MUTATE
        if (saved.runtimeOp!=XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY) {
            XL6_IrCollectionAbi other=saved.runtimeOp==XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64
                ? XL6_IR_COLLECTION_ARRAY : XL6_IR_COLLECTION_LIST;
            bool tested=false;
            for (size_t t=0;t<unit.ir.types.count;++t) {
                const XL6_IrType* logical=&unit.ir.types.items[t];
                if (logical->kind!=XL6_IR_TYPE_BUILTIN || logical->collectionAbi!=other) continue;
                item->payloadType=(XL6_IrTypeId)(t+1);
                item->runtimeOp=other==XL6_IR_COLLECTION_ARRAY
                    ? XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64 : XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64;
                refused(&unit.ir); *item=saved; ++mutations; tested=true; break;
            }
            assert(tested);
        }
        const XL6_IrInstruction* load=NULL;
        for (size_t j=0;j<unit.ir.instructionCount;++j)
            if (unit.ir.instructions[j].result==unit.ir.operands[saved.firstOperand]) load=&unit.ir.instructions[j];
        assert(load && load->kind==XL6_IR_INSTR_LOAD);
        for (size_t j=0;j<unit.ir.instructionCount;++j) {
            XL6_IrInstruction* store=&unit.ir.instructions[j];
            if (store->kind!=XL6_IR_INSTR_STORE || store->targetSymbol!=load->targetSymbol) continue;
            XL6_IrValueId acquired=unit.ir.operands[store->firstOperand];
            for (size_t k=0;k<unit.ir.instructionCount;++k) {
                XL6_IrInstruction* create=&unit.ir.instructions[k];
                if (create->result!=acquired) continue;
                assert(create->runtimeOp==XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED);
                XL6_IrTypeId payload=create->payloadType;
                create->payloadType=create->type; refused(&unit.ir); create->payloadType=payload; ++mutations;
            }
        }
        ++projections;
    }
    assert(projections==4 && mutations==34);
    XL6_Output out; assert(xl6OutputOpenFile(&out,argv[2])); unsigned bindings=0;
    for (size_t i=0;i<unit.ir.functionCount;++i) {
        const XL6_IrFunction* f=&unit.ir.functions[i]; if (!f->debugName) continue;
        const char* token=!strcmp(f->debugName,"refinedListPair") ? "REFINED" :
            !strcmp(f->debugName,"dynamicListMethods") ? "DYNAMIC" : NULL;
        if (!token) continue;
        const XL6_IrType* type=xl6IrTypeGet(&unit.ir.types,f->returnType); assert(type && type->errorResultCName);
        xl6OutputPrintf(&out,"#define XL6_REFINED_%s xlir_fn_%u\n#define XL6_REFINED_%s_RESULT %s\n",
            token,(unsigned)f->symbol,token,type->errorResultCName); ++bindings;
    }
    for (size_t i=0;i<unit.ir.types.count;++i) {
        const XL6_IrType* type=&unit.ir.types.items[i];
        if (type->kind!=XL6_IR_TYPE_BUILTIN || type->collectionAbi!=XL6_IR_COLLECTION_LIST) continue;
        const XL6_IrType* schema=xl6IrTypeGet(&unit.ir.types,type->collectionValueSchemaType); assert(schema);
        const XL6_IrType* element=xl6IrTypeGet(&unit.ir.types,schema->sequenceLayout.elementType); assert(element);
        xl6OutputPrintf(&out,"#define XL6_REFINED_OWNER 0x%016llXULL\n#define XL6_REFINED_WITNESS 0x%016llXULL\n#define XL6_REFINED_ELEMENT 0x%016llXULL\n",
            (unsigned long long)type->typeValueId,(unsigned long long)schema->typeValueId,
            (unsigned long long)element->typeValueId); ++bindings;
    }
    for (size_t i=0;i<unit.ir.storageCount;++i) {
        const XL6_IrStorage* storage=&unit.ir.storages[i]; if (storage->kind!=XL6_IR_STORAGE_GLOBAL) continue;
        for (size_t j=0;j<unit.ir.nodeCount;++j) {
            const XL6_IrNode* node=&unit.ir.nodes[j];
            if (node->symbol==storage->symbol && node->text && !strcmp(node->text,"refinedCleanups")) {
                xl6OutputPrintf(&out,"#define XL6_REFINED_CLEANUPS s%u\n",(unsigned)storage->symbol); ++bindings; break;
            }
        }
    }
    assert(bindings==4); xl6OutputClose(&out); xl6CompileUnitFree(&unit);
    printf("Refined keys: %u projections, %u malformed IR mutations refused before C/header output\n",projections,mutations);
    return 0;
}
