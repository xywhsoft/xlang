#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_LIST_ALGEBRA",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0);
        xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(int argc,char** argv)
{
    assert((argc==3 || argc==4) && fixture_contract(argv[1]));
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    if (argc==4) {
        assert(!strcmp(argv[3],"absent"));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_ALGEBRA));
        /* An open-var index may legally depend on the native List reader.
         * It must not directly root the List owner or the new algebra. */
        assert(!xl6ArtifactRuntimeSetHas(&plan.roots,XL6_ARTIFACT_LIST_NATIVE));
        xl6CompileUnitFree(&unit); puts("Unrelated program excludes List algebra: verified"); return 0;
    }
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_ALGEBRA));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE));
    /* Native List factories authenticate their projected Array schema through
     * this provider. They must not pull an import transaction into algebra. */
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_ERASED));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_IMPORT_CHAIN));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_IMPORT_CHAIN));
    const char* symbols[]={"xl6ListValueCloneV1","xl6ListValueMergeRightV1","xl6ListValueWithoutKeyV1"};
    for (size_t i=0;i<3;++i) assert(xl6ArtifactRuntimeSymbolOwner(symbols[i])==XL6_ARTIFACT_LIST_ALGEBRA);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ListErasedCloneV1")==XL6_ARTIFACT_LIST_NATIVE);
    unsigned operations=0,mutations=0;
    for (size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* ins=&unit.ir.instructions[i];
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            (ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_LIST_MERGE_RIGHT &&
             ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_LIST_WITHOUT_I64_KEY)) continue;
        assert(xl6IrCallInstructionErrorAbi(&unit.ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(&unit.ir,ins)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(i+1<unit.ir.instructionCount); XL6_IrInstruction* test=&unit.ir.instructions[i+1];
        XL6_IrInstruction saved=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST && test->operandCount==1 &&
            unit.ir.operands[test->firstOperand]==ins->result);
        test->falseTarget=test->trueTarget; reject(&unit.ir); *test=saved; ++mutations;
        test->kind=XL6_IR_INSTR_DISCARD; reject(&unit.ir); *test=saved; ++mutations;
        XL6_IrValueId operand=unit.ir.operands[test->firstOperand];
        unit.ir.operands[test->firstOperand]=unit.ir.operands[ins->firstOperand];
        reject(&unit.ir); unit.ir.operands[test->firstOperand]=operand; ++mutations;
        XL6_IrNode* origin=&unit.ir.nodes[ins->origin-1]; XL6_IrErrorAbi effect=origin->operatorErrorAbi;
        assert(effect==XL6_IR_ERROR_ABI_MAY_RAISE); origin->operatorErrorAbi=XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&unit.ir); origin->operatorErrorAbi=effect; ++mutations; ++operations;
    }
    assert(operations==4 && mutations==16 && xl6IrVerifyModule(&unit.ir,NULL));
    XL6_Output out; assert(xl6OutputOpenFile(&out,argv[2])); unsigned count=0;
    for (size_t i=0;i<unit.ir.functionCount;++i) {
        const XL6_IrFunction* f=&unit.ir.functions[i]; if (!f->debugName || !f->parameterCount) continue;
        const XL6_IrType* result=xl6IrTypeGet(&unit.ir.types,f->returnType);
        const XL6_IrStorage* parameter=xl6IrStorageGet(&unit.ir,unit.ir.functionParameters[f->firstParameter]);
        const XL6_IrType* owner=parameter ? xl6IrTypeGet(&unit.ir.types,parameter->type) : NULL;
        if (!owner || owner->collectionAbi!=XL6_IR_COLLECTION_LIST) continue;
        const XL6_IrType* schema=xl6IrTypeGet(&unit.ir.types,owner->listValueSchemaType);
        const XL6_IrType* element=xl6IrTypeGet(&unit.ir.types,owner->params[0]);
        assert(result && result->errorResultCName && schema && element);
        xl6OutputPrintf(&out,"#define XL6_ALG_%s xlir_fn_%u\n#define XL6_ALG_%s_RESULT %s\n"
            "#define XL6_ALG_%s_OWNER UINT64_C(0x%016llX)\n"
            "#define XL6_ALG_%s_SCHEMA UINT64_C(0x%016llX)\n"
            "#define XL6_ALG_%s_ELEMENT UINT64_C(0x%016llX)\n",f->debugName,(unsigned)f->symbol,
            f->debugName,result->errorResultCName,f->debugName,(unsigned long long)owner->typeValueId,
            f->debugName,(unsigned long long)schema->typeValueId,f->debugName,(unsigned long long)element->typeValueId);
        ++count;
    }
    for (size_t i=0;i<unit.ir.storageCount;++i) {
        const XL6_IrStorage* storage=&unit.ir.storages[i]; if (storage->kind!=XL6_IR_STORAGE_GLOBAL) continue;
        for (size_t j=0;j<unit.ir.nodeCount;++j) {
            const XL6_IrNode* node=&unit.ir.nodes[j];
            if (node->symbol==storage->symbol && node->text && !strcmp(node->text,"listAlgebraCleanups")) {
                xl6OutputPrintf(&out,"#define XL6_ALG_CLEANUPS s%u\n",(unsigned)storage->symbol); ++count; break;
            }
        }
    }
    assert(count==4); xl6OutputClose(&out); xl6CompileUnitFree(&unit);
    printf("List algebra %u checked operations, %u pre-output rejections, resident closure and bindings verified\n",operations,mutations); return 0;
}
