#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned rejected;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir,&diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,ir,&diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,ir,"BAD_LIST_INDEX",&diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++rejected;
}
int main(int argc,char** argv)
{
    assert(argc==3 && fixture_contract(argv[1]));
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    const XL6_ArtifactRuntimeId required[]={XL6_ARTIFACT_LIST_SCALAR_READ,XL6_ARTIFACT_LIST_STRING_READ,
        XL6_ARTIFACT_LIST_VALUE_READ,XL6_ARTIFACT_LIST_VALUE_WRITE,XL6_ARTIFACT_LIST_NATIVE};
    for (size_t i=0;i<sizeof(required)/sizeof(required[0]);++i) assert(xl6ArtifactRuntimeSetHas(&plan.components,required[i]));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_INDEX_READ));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_INDEX_WRITE));
    unsigned mask=0;
    for (size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* ins=&unit.ir.instructions[i];
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL) continue;
        if (ins->runtimeOp>=XL6_IR_RUNTIME_LIST_READ_VALUE_COPY && ins->runtimeOp<=XL6_IR_RUNTIME_LIST_READ_VALUE_REQUIRED_COPY) {
            unsigned bit=1u<<(ins->runtimeOp-XL6_IR_RUNTIME_LIST_READ_VALUE_COPY);
            if (!(mask&bit)) {
                XL6_IrInstruction saved=*ins;
                ins->operandCount=0; reject(&unit.ir); *ins=saved;
                ins->immediateText="unresolved"; reject(&unit.ir); *ins=saved;
                ins->type=XL6_IR_TYPE_INVALID; reject(&unit.ir); *ins=saved;
                ins->targetSymbol=1; reject(&unit.ir); *ins=saved;
                XL6_IrValueId receiver=unit.ir.operands[ins->firstOperand];
                XL6_IrValueId key=unit.ir.operands[ins->firstOperand+1];
                unit.ir.operands[ins->firstOperand]=key;
                unit.ir.operands[ins->firstOperand+1]=receiver; reject(&unit.ir);
                unit.ir.operands[ins->firstOperand]=receiver;
                unit.ir.operands[ins->firstOperand+1]=key;
                if (xl6IrListTypedReadRuntimeOpIs(ins->runtimeOp)) {
                    ins->runtimeOp=saved.runtimeOp==XL6_IR_RUNTIME_LIST_READ_I64 ?
                        XL6_IR_RUNTIME_LIST_READ_U64 : XL6_IR_RUNTIME_LIST_READ_I64;
                    reject(&unit.ir); *ins=saved;
                }
            }
            mask|=bit;
        }
        assert(ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64 &&
            ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL &&
            ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER &&
            ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64 &&
            ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL &&
            ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64 &&
            ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY);
    }
    assert(mask==511 && rejected==50);
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    XL6_Output out; assert(xl6OutputOpenFile(&out,argv[2])); unsigned count=0;
    for (size_t i=0;i<unit.ir.functionCount;++i) {
        const XL6_IrFunction* f=&unit.ir.functions[i]; if (!f->debugName || !f->parameterCount) continue;
        const XL6_IrType* result=xl6IrTypeGet(&unit.ir.types,f->returnType);
        const XL6_IrStorage* parameter=xl6IrStorageGet(&unit.ir,
            unit.ir.functionParameters[f->firstParameter]);
        const XL6_IrType* owner=parameter ? xl6IrTypeGet(&unit.ir.types,parameter->type) : NULL;
        if (owner && owner->kind==XL6_IR_TYPE_NULLABLE) owner=xl6IrTypeGet(&unit.ir.types,owner->params[0]);
        if (!owner || owner->collectionAbi!=XL6_IR_COLLECTION_LIST) continue;
        const XL6_IrType* schema=xl6IrTypeGet(&unit.ir.types,owner->collectionValueSchemaType);
        const XL6_IrType* element=xl6IrTypeGet(&unit.ir.types,owner->params[0]);
        assert(result && result->errorResultCName && schema && element);
        xl6OutputPrintf(&out,"#define XL6_STATIC_%s xlir_fn_%u\n#define XL6_STATIC_%s_RESULT %s\n"
            "#define XL6_STATIC_%s_OWNER UINT64_C(0x%016llX)\n"
            "#define XL6_STATIC_%s_SCHEMA UINT64_C(0x%016llX)\n"
            "#define XL6_STATIC_%s_ELEMENT UINT64_C(0x%016llX)\n",f->debugName,(unsigned)f->symbol,
            f->debugName,result->errorResultCName,f->debugName,(unsigned long long)owner->typeValueId,
            f->debugName,(unsigned long long)schema->typeValueId,f->debugName,(unsigned long long)element->typeValueId);
        ++count;
    }
    for (size_t i=0;i<unit.ir.storageCount;++i) {
        const XL6_IrStorage* storage=&unit.ir.storages[i]; if (storage->kind!=XL6_IR_STORAGE_GLOBAL) continue;
        for (size_t j=0;j<unit.ir.nodeCount;++j) {
            const XL6_IrNode* node=&unit.ir.nodes[j];
            if (node->symbol==storage->symbol && node->text && !strcmp(node->text,"staticListCleanups")) {
                xl6OutputPrintf(&out,"#define XL6_STATIC_CLEANUPS s%u\n",(unsigned)storage->symbol); ++count; break;
            }
        }
    }
    assert(count==13); xl6OutputClose(&out); xl6CompileUnitFree(&unit);
    printf("All nine List index contracts, %u pre-output rejections, exact native plans and bindings verified\n",rejected); return 0;
}
