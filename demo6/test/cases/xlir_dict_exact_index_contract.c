#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned dictIndexMutations;
static void dictIndexRejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_DICT_INDEX",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++dictIndexMutations;
}
int main(int argc,char** argv)
{
    assert(argc==3 && fixture_contract(argv[1])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL)); unsigned reads=0,writes=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir->instructions[i];
        bool read=instruction->runtimeOp==XL6_IR_RUNTIME_DICT_READ_VALUE_COPY ||
            instruction->runtimeOp==XL6_IR_RUNTIME_DICT_READ_VALUE_OR_NULL_COPY ||
            instruction->runtimeOp==XL6_IR_RUNTIME_DICT_READ_VALUE_REQUIRED_COPY;
        bool write=instruction->runtimeOp==XL6_IR_RUNTIME_DICT_SET_VALUE_TAKE;
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL || (!read && !write)) continue;
        assert(instruction->operandCount==(read ? 2u : 3u));
        assert(xl6IrCallInstructionErrorAbi(ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,instruction)==(read ?
            XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE));
        for (unsigned operand=0;operand<instruction->operandCount;++operand)
            assert(xl6IrInstructionOperandOwnership(ir,instruction,operand)==(write && operand==2 ?
                XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE));
        if (read) assert(xl6IrOwnedIndexReadRuntimeInstructionCompatible(ir,instruction) &&
            xl6IrValueOwnershipGet(ir,instruction->result)==XL6_IR_OWNERSHIP_MOVE);
        XL6_IrInstruction saved=*instruction;
#define MUTATE(Field,Value) do { instruction->Field=(Value); dictIndexRejected(ir); *instruction=saved; } while (0)
        MUTATE(type,XL6_IR_TYPE_INVALID); MUTATE(result,XL6_IR_VALUE_INVALID); MUTATE(firstOperand,SIZE_MAX);
        MUTATE(operandCount,0); MUTATE(operandCount,1); MUTATE(operandCount,4);
        MUTATE(targetValue,saved.result); MUTATE(targetSymbol,UINT32_MAX); MUTATE(calleeType,saved.type);
        MUTATE(payloadType,saved.type); MUTATE(runtimeIdentityType,saved.type); MUTATE(opcode,XL6_IR_OPCODE_ADD);
        MUTATE(immediateText,(char*)"not-a-key"); MUTATE(typedCollectionOp,XL6_IR_TYPED_COLLECTION_CLEAR);
        MUTATE(runtimeOp,XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_I64);
        MUTATE(runtimeOp,XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_STRING_COPY);
        MUTATE(dynamicCollectionOp,XL6_IR_DYNAMIC_COLLECTION_CLEAR); MUTATE(sequenceAdmissionIdentitySet,1);
#undef MUTATE
        for (unsigned operand=0;operand<saved.operandCount;++operand) {
            XL6_IrValueId* cell=&ir->operands[saved.firstOperand+operand],old=*cell;
            *cell=XL6_IR_VALUE_INVALID; dictIndexRejected(ir); *cell=old;
        }
        XL6_IrInstruction* test=&ir->instructions[i+1]; XL6_IrInstruction outcome=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST);
        test->kind=XL6_IR_INSTR_INVALID; dictIndexRejected(ir); *test=outcome;
        if (read) ++reads; else ++writes;
    }
    assert(reads>=8 && writes>=4 && xl6IrVerifyModule(ir,NULL));
    XL6_ArtifactRuntimePlan plan={0}; assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    const XL6_ArtifactRuntimeId required[]={XL6_ARTIFACT_DICT_VALUE_INDEX,XL6_ARTIFACT_DICT_NATIVE};
    for (size_t i=0;i<sizeof(required)/sizeof(required[0]);++i) assert(xl6ArtifactRuntimeSetHas(&plan.components,required[i]));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_SCHEMA));
    XL6_Output out; unsigned bindings=0; assert(xl6OutputOpenFile(&out,argv[2]));
    for (size_t i=0;i<ir->functionCount;++i) {
        const XL6_IrFunction* f=&ir->functions[i]; if (!f->debugName) continue;
        const char* name=!strcmp(f->debugName,"dictIndexRead") ? "READ" : !strcmp(f->debugName,"dictIndexWrite") ? "WRITE" : NULL;
        if (!name) continue;
        const XL6_IrType* t=xl6IrTypeGet(&ir->types,f->returnType); assert(t && t->errorResultCName);
        xl6OutputPrintf(&out,"#define XL6_DICT_INDEX_%s xlir_fn_%u\n#define XL6_DICT_INDEX_%s_RESULT %s\n",
            name,(unsigned)f->symbol,name,t->errorResultCName); ++bindings;
    }
    for (size_t i=0;i<ir->storageCount;++i) {
        const XL6_IrStorage* s=&ir->storages[i]; if (s->kind!=XL6_IR_STORAGE_GLOBAL) continue;
        for (size_t j=0;j<ir->nodeCount;++j) {
            const XL6_IrNode* n=&ir->nodes[j]; if (n->symbol==s->symbol && n->text && !strcmp(n->text,"dictIndexCleanups")) {
                xl6OutputPrintf(&out,"#define XL6_DICT_INDEX_CLEANUPS s%u\n",(unsigned)s->symbol); ++bindings; break;
            }
        }
    }
    assert(bindings==3); xl6OutputClose(&out);
    puts("Dict dynamic index plan and owned/consuming XLIR contracts verified");
    printf("Dict index contract reads=%u writes=%u mutations=%u\n",reads,writes,dictIndexMutations);
    xl6CompileUnitFree(&unit); return 0;
}
