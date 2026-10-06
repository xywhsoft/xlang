#include "xlir_continuation_owner_test.h"

/* Allocation outcomes are canonical IR facts, not emitter-local error member
 * tests. Every allocating projection must have its exact physical continuation
 * before any temporary is published or the expression keeps executing. */
int main(int argc,char** argv)
{
    const XL6_IrRuntimeOp operations[]={XL6_IR_RUNTIME_ERROR_MESSAGE_COPY,
        XL6_IR_RUNTIME_ERROR_KIND_COPY,XL6_IR_RUNTIME_ERROR_FILE_COPY,
        XL6_IR_RUNTIME_ERROR_DOMAIN_COPY,XL6_IR_RUNTIME_ERROR_OPERATION_COPY,
        XL6_IR_RUNTIME_ERROR_DATA_COPY};
    XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;unsigned mask=0;
    assert(argc==3);xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"error_projection_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) {
        for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d %s\n",
            parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
        return 2;
    }
    verify(&ir);
    for (size_t i=0;i<ir.intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* intrinsic=&ir.intrinsicCalls[i];
        if (intrinsic->stringConversionAbi!=XL6_IR_STRING_CONVERSION_BORROW_ERROR_MESSAGE) continue;
        XL6_IrIntrinsicCall saved=*intrinsic;
        intrinsic->stringConversionAbi=XL6_IR_STRING_CONVERSION_BORROW_XVALUE;
        reject(&ir,NULL);*intrinsic=saved;
        intrinsic->stringConversionAbi=XL6_IR_STRING_CONVERSION_BORROW_STRING_COPY;
        reject(&ir,NULL);*intrinsic=saved;
    }
    for(size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];size_t kind=0;
        if(instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL)continue;
        for(;kind<6 && instruction->runtimeOp!=operations[kind];++kind){}
        if(kind==6)continue;
        mask|=1u<<kind;
        assert(xl6IrCallInstructionErrorAbi(&ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(&ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        assert(i+1<ir.instructionCount);
        XL6_IrInstruction* test=&ir.instructions[i+1];XL6_IrInstruction saved=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST && test->operandCount==1 &&
            ir.operands[test->firstOperand]==instruction->result);
        test->falseTarget=test->trueTarget;reject(&ir,NULL);*test=saved;
        test->kind=XL6_IR_INSTR_BRANCH;reject(&ir,NULL);*test=saved;
        test->kind=XL6_IR_INSTR_DISCARD;reject(&ir,NULL);*test=saved;
        XL6_IrValueId operand=ir.operands[test->firstOperand];
        ir.operands[test->firstOperand]=ir.operands[instruction->firstOperand];
        reject(&ir,NULL);ir.operands[test->firstOperand]=operand;
    }
    assert(mask==63u);verify(&ir);
    assert(xl6OutputOpenFile(&out,argv[2]) && xl6IrCgenEmitC(&out,&ir,&parse.diags));xl6OutputClose(&out);
    printf("error projections: 6 allocating families, %llu rejected C/header-gated mutations\n",
        (unsigned long long)mutations);
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
