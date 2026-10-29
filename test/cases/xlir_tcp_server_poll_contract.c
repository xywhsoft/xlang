/* Data-only verification: no engine or socket is created by this test. */
#define main existingTcpContractMain
#include "xlir_tcp_managed_contract.c"
#undef main
int main(int argc,char** argv)
{
    assert(argc==2);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    XL6_IrCallTarget* poll=NULL;
    for(size_t i=0;i<module->callTargetCount;++i)
        if(module->callTargets[i].cName&&!strcmp(module->callTargets[i].cName,"xl6TcpServerTryAcceptV1"))
            poll=&module->callTargets[i];
    assert(poll&&poll->kind==XL6_IR_CALL_TARGET_NATIVE_FACTORY);
    XL6_IrCallTarget saved=*poll;
    XL6_IrType* optional=(XL6_IrType*)xl6IrTypeGet(&module->types,poll->returnType);
    assert(optional&&optional->kind==XL6_IR_TYPE_NULLABLE&&optional->paramCount==1);
    XL6_IrType original=*optional;
    assert(xl6IrCallTargetNativeClassResultField(module,poll));
    size_t emptyGuards=0;
    for(size_t i=0;i<module->instructionCount;++i){
        const XL6_IrInstruction* call=&module->instructions[i];
        if(call->kind!=XL6_IR_INSTR_CALL||call->targetSymbol!=poll->symbol)continue;
        size_t n=i+1;
        for(;n+1<module->instructionCount;++n){
            const XL6_IrInstruction* test=&module->instructions[n];
            if(test->kind==XL6_IR_INSTR_NULL_TEST&&test->operandCount==1&&
                module->operands[test->firstOperand]==call->result)break;
        }
        assert(n+1<module->instructionCount);
        const XL6_IrInstruction* branch=&module->instructions[n+1];
        assert(branch->kind==XL6_IR_INSTR_BRANCH&&branch->trueTarget&&branch->falseTarget);
        const XL6_IrBlock* empty=&module->blocks[branch->trueTarget-1];
        /* The exact empty successor contains no boxing, constructor, call,
         * await, allocation or runtime operation: only null/store/jump. */
        assert(empty->instructionCount==3);
        assert(module->instructions[empty->firstInstruction].kind==XL6_IR_INSTR_NULL);
        assert(module->instructions[empty->firstInstruction+1].kind==XL6_IR_INSTR_STORE);
        assert(module->instructions[empty->firstInstruction+2].kind==XL6_IR_INSTR_JUMP);
        if(emptyGuards==0){
            XL6_IrInstruction* gate=(XL6_IrInstruction*)branch;
            XL6_IrInstruction savedGate=*gate;
            gate->trueTarget=savedGate.falseTarget;gate->falseTarget=savedGate.trueTarget;
            refused(module,NULL,18);*gate=savedGate;
            XL6_IrInstruction* jump=&module->instructions[empty->firstInstruction+2];
            XL6_IrInstruction savedJump=*jump;
            jump->trueTarget=gate->falseTarget;refused(module,NULL,19);*jump=savedJump;
        }
        ++emptyGuards;
    }
    assert(emptyGuards>=3);
    poll->returnType=optional->params[0];refused(module,poll,11);*poll=saved;
    poll->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT;refused(module,poll,12);*poll=saved;
    poll->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;refused(module,poll,13);*poll=saved;
    poll->resultPhysicalDestroySymbol=0;refused(module,poll,14);*poll=saved;
    optional->nullAbi=XL6_IR_NULL_ABI_POINTER_SENTINEL;refused(module,poll,15);*optional=original;
    optional->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;refused(module,poll,16);*optional=original;
    optional->paramCount=0;refused(module,poll,17);*optional=original;
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    check(argv[1]);
    printf("TCP optional owner: %zu allocation-free empty CFG edges, nullable identity, independent error scope, exact destructor/field, %zu descriptor refusals, %zu zero-output emission guards\n",
        emptyGuards,refusals,emissionGuards);
    return 0;
}
