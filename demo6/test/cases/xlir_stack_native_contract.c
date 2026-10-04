#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static void rejected(const XL6_IrModule* ir)
{
    XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
    xl6DiagInit(&diagnostics);assert(!xl6IrVerifyModule(ir,NULL));
    assert(xl6OutputOpenMemory(&output));
    assert(!xl6IrCgenEmitC(&output,ir,&diagnostics));
    char* text=xl6OutputTakeMemoryText(&output,&size);
    assert(size==0);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
}
int main(int argc, char** argv)
{
    assert(argc == 3); int ok = 0; unsigned checked = 0;
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"stack_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0; i<ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL || instruction->runtimeOp < XL6_IR_RUNTIME_STACK_CREATE ||
            instruction->runtimeOp > XL6_IR_RUNTIME_STACK_CAPACITY) continue;
        XL6_IrTypeId payload = instruction->payloadType;
        instruction->payloadType = XL6_IR_TYPE_INVALID;
        rejected(&ir); instruction->payloadType = payload;
        XL6_IrRuntimeResourcePayloadAbi transfer = instruction->runtimeResourcePayloadAbi;
        instruction->runtimeResourcePayloadAbi = XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
        rejected(&ir);
        instruction->runtimeResourcePayloadAbi = transfer == XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY
            ? XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY : XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY;
        rejected(&ir); instruction->runtimeResourcePayloadAbi = transfer;
        XL6_IrType* type = &ir.types.items[payload-1]; uint64_t identity = type->typeValueId;
        type->typeValueId = 0; rejected(&ir); type->typeValueId = identity;
        XL6_IrOwnershipAbi ownership=type->ownershipAbi;
        type->ownershipAbi=ownership == XL6_IR_OWNERSHIP_ABI_MANAGED
            ? XL6_IR_OWNERSHIP_ABI_TRIVIAL : XL6_IR_OWNERSHIP_ABI_MANAGED;
        rejected(&ir);type->ownershipAbi=ownership;
        XL6_IrNullAbi nullAbi=type->nullAbi;
        type->nullAbi=nullAbi == XL6_IR_NULL_ABI_NONE
            ? XL6_IR_NULL_ABI_POINTER_SENTINEL : XL6_IR_NULL_ABI_NONE;
        rejected(&ir);type->nullAbi=nullAbi;
        assert(xl6IrVerifyModule(&ir,NULL)); ++checked;
    }
    assert(checked);
    unsigned lifetimes=0;
    for(size_t i=0;i<ir.callTargetCount;++i){
        XL6_IrCallTarget* target=&ir.callTargets[i];
        for(size_t p=0;target->parameterLifetimes && p<target->parameterCount;++p){
            XL6_IrParameterLifetime* lifetime=&target->parameterLifetimes[p];
            int rebindable=lifetime->rebindable;
            lifetime->rebindable=!rebindable;rejected(&ir);lifetime->rebindable=rebindable;
            int readOnly=lifetime->borrowReadOnly;
            lifetime->borrowReadOnly=!readOnly;rejected(&ir);lifetime->borrowReadOnly=readOnly;
            assert(xl6IrVerifyModule(&ir,NULL));++lifetimes;
        }
    }
    if (!xl6OutputOpenFile(&out,argv[2])) goto done;
    ok = xl6IrCgenEmitC(&out,&ir,&parse.diags); xl6OutputClose(&out);
    printf("stack contract: %u exact element/transfer/identity/ownership/null instructions, %u parameter lifetimes; mutations refused before C output\n",checked,lifetimes);
done:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
