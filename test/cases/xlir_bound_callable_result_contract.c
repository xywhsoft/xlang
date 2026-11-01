#define main original_owner_main
#include "xlir_owner_consumption_contract.c"
#undef main
int main(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/extern_owned_nested_result.xl",&parse));
    assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"nested_result"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags));assert(xl6IrVerifyModule(&ir,&parse.diags));
    size_t checked=0;
    for(size_t i=0;i<ir.instructionCount;++i)
        assert(ir.instructions[i].runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED);
    for(size_t c=0;c<ir.closureTargetCount;++c) {
        XL6_IrClosureTarget* closure=&ir.closureTargets[c];
        if(closure->kind!=XL6_IR_CLOSURE_TARGET_RESULT_ADAPTER)continue;
        XL6_IrClosureCapture* capture=&ir.closureCaptures[closure->firstCapture];
        assert(closure->captureCount==1 && capture->abi==XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE);
        XL6_IrFunction* body=NULL;
        for(size_t f=0;f<ir.functionCount;++f)if(ir.functions[f].symbol==closure->targetSymbol)body=&ir.functions[f];
        assert(body);
        closure->forwardCallMetadata=0;reject(&ir,"missing callable metadata forwarding",0);closure->forwardCallMetadata=1;
        XL6_IrClosureCaptureAbi abi=capture->abi;
        capture->abi=XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE;reject(&ir,"wrong capture lifetime",0);capture->abi=abi;
        XL6_SymbolId symbol=capture->storageSymbol;
        capture->storageSymbol=0;reject(&ir,"missing captured source",0);capture->storageSymbol=symbol;
        XL6_IrTypeId output=closure->signatureType;
        closure->signatureType=capture->type;reject(&ir,"erased result conversion",0);closure->signatureType=output;
        for(size_t i=body->firstInstruction;i<body->firstInstruction+body->instructionCount;++i) {
            XL6_IrInstruction* instruction=&ir.instructions[i];
            if(instruction->kind!=XL6_IR_INSTR_CLOSURE_CALL)continue;
            XL6_IrTypeId input=instruction->calleeType;
            instruction->calleeType=output;reject(&ir,"source call used target ABI",0);instruction->calleeType=input;
            XL6_IrValueId value=ir.operands[instruction->firstOperand];
            ir.operands[instruction->firstOperand]=instruction->result;
            reject(&ir,"source was replaced",0);ir.operands[instruction->firstOperand]=value;
        }
        assert(xl6IrVerifyModule(&ir,&parse.diags));++checked;
    }
    assert(checked>=6 && rejected==checked*6);
    puts("returned callables: retained source, invariant inputs, exact conversion and error edges authenticated");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
