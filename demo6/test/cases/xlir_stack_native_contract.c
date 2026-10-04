#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/type/xl6_type_model.h"
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
    assert(argc == 3); int ok = 0; unsigned checked = 0, ownedResults = 0, nullableComparisons = 0;
    uint64_t magnitude = 73;
    assert(xl6TypeParseIntegerMagnitude("0xffff_ffff_ffff_ffff", &magnitude) && magnitude == UINT64_MAX);
    assert(xl6TypeParseIntegerMagnitude("0b1010", &magnitude) && magnitude == 10);
    assert(xl6TypeParseIntegerMagnitude("0o17", &magnitude) && magnitude == 15);
    assert(!xl6TypeParseIntegerMagnitude("18446744073709551616", &magnitude) && magnitude == 15);
    assert(!xl6TypeParseIntegerMagnitude("0x_", &magnitude) && magnitude == 15);
    assert(!xl6TypeParseIntegerMagnitude("-1", &magnitude) && magnitude == 15);
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"stack_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrType* type=&ir.types.items[i];
        XL6_IrNativeSlotAbi saved=type->nativeSlotAbi;
        assert(saved==xl6IrNativeSlotAbiForType(&ir.types,type));
        type->nativeSlotAbi=saved==XL6_IR_NATIVE_SLOT_NONE ?
            XL6_IR_NATIVE_SLOT_CHAR : XL6_IR_NATIVE_SLOT_NONE;
        rejected(&ir);
        type->nativeSlotAbi=(XL6_IrNativeSlotAbi)0x7fffffff; rejected(&ir);
        type->nativeSlotAbi=saved;
    }
    assert(xl6IrVerifyModule(&ir,NULL));
    uint64 charIdentity=ir.runtimeTypeMap.charTypeValueId;
    assert(ir.runtimeTypeMap.abiVersion==2 && charIdentity && charIdentity!=ir.runtimeTypeMap.u64TypeValueId);
    ir.runtimeTypeMap.charTypeValueId=ir.runtimeTypeMap.fallbackTypeValueId; rejected(&ir);
    ir.runtimeTypeMap.charTypeValueId=charIdentity;
    for (size_t i=0; i<ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL && instruction->runtimeOp==XL6_IR_RUNTIME_NULLABLE_SCALAR_EQUAL) {
            assert(xl6IrNullableScalarEqualSignatureValid(&ir,instruction));
            assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
            assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,1)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
            instruction->runtimeOp=XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL; rejected(&ir);
            instruction->runtimeOp=XL6_IR_RUNTIME_NULLABLE_SCALAR_EQUAL;
            instruction->payloadType=instruction->type; rejected(&ir); instruction->payloadType=XL6_IR_TYPE_INVALID;
            XL6_IrTypeId saved=instruction->type;
            const XL6_IrInstruction* operand=NULL;
            for(size_t definition=0;definition<ir.instructionCount;++definition)
                if(ir.instructions[definition].result==ir.operands[instruction->firstOperand]) operand=&ir.instructions[definition];
            assert(operand);
            instruction->type=operand->type; rejected(&ir); instruction->type=saved;
            instruction->opcode=XL6_IR_OPCODE_EQ; rejected(&ir); instruction->opcode=XL6_IR_OPCODE_INVALID;
            assert(xl6IrVerifyModule(&ir,NULL)); ++checked;
            continue;
        }
        if (instruction->kind == XL6_IR_INSTR_RUNTIME_CALL && instruction->runtimeOp == XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL) {
            assert(xl6IrStringCompareRuntimeSignatureValid(&ir,instruction));
            assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
            assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,1)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
            instruction->runtimeOp=XL6_IR_RUNTIME_STRING_EQUAL; rejected(&ir);
            instruction->runtimeOp=XL6_IR_RUNTIME_STRING_ORDER_LT; rejected(&ir);
            instruction->runtimeOp=XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL;
            instruction->payloadType=instruction->type; rejected(&ir); instruction->payloadType=XL6_IR_TYPE_INVALID;
            assert(xl6IrVerifyModule(&ir,NULL)); ++checked; ++nullableComparisons;
            continue;
        }
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
        if (instruction->runtimeOp == XL6_IR_RUNTIME_STACK_PUSH &&
            xl6IrNullableScalarElementAbi(&ir, payload) != XL6_IR_C_ABI_UNSUPPORTED) {
            XL6_IrIntrinsicCall* intrinsic = (XL6_IrIntrinsicCall*)xl6IrIntrinsicCallGet(&ir, instruction->origin);
            assert(intrinsic && intrinsic->operandCount == 2);
            XL6_IrNodeId sourceId = ir.intrinsicOperands[intrinsic->firstOperand+1].expression;
            if (ir.nodes[sourceId-1].storageType != type->params[0] &&
                xl6IrContextualIntegerLiteralValue(&ir, sourceId, type->params[0], NULL, NULL)) {
                XL6_IrNode* literal = &ir.nodes[sourceId-1];
                if (literal->kind == XL6_IR_NODE_UNARY) literal = &ir.nodes[ir.children[literal->firstChild]-1];
                char* spelling = literal->text;
                literal->text = "18446744073709551616";
                rejected(&ir); literal->text = spelling;
            }
        }
        if (instruction->runtimeOp == XL6_IR_RUNTIME_STACK_POP ||
            instruction->runtimeOp == XL6_IR_RUNTIME_STACK_PEEK ||
            instruction->runtimeOp == XL6_IR_RUNTIME_STACK_PEEK_AT ||
            instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_POP ||
            instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_PEEK ||
            instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_PEEK_AT) {
            XL6_IrTypeId resultType=instruction->type;
            instruction->type=XL6_IR_TYPE_INVALID;rejected(&ir);instruction->type=resultType;
            XL6_IrIntrinsicCall* intrinsic=(XL6_IrIntrinsicCall*)xl6IrIntrinsicCallGet(&ir,instruction->origin);
            assert(intrinsic);
            XL6_IrIntrinsicResultAbi resultAbi=intrinsic->resultAbi;
            intrinsic->resultAbi=resultAbi == XL6_IR_INTRINSIC_RESULT_OWNED
                ? XL6_IR_INTRINSIC_RESULT_VALUE : XL6_IR_INTRINSIC_RESULT_OWNED;
            rejected(&ir);intrinsic->resultAbi=resultAbi;
            XL6_IrErrorAbi errorAbi=intrinsic->errorAbi;
            intrinsic->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;rejected(&ir);intrinsic->errorAbi=errorAbi;
            ++ownedResults;
            if (instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_POP ||
                instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_PEEK ||
                instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_PEEK_AT) {
                XL6_IrType* product=&ir.types.items[resultType-1];
                XL6_IrTypeId status=product->params[0], optional=product->params[1];
                product->params[0]=optional;rejected(&ir);product->params[0]=status;
                product->params[1]=status;rejected(&ir);product->params[1]=optional;
                XL6_IrType* value=&ir.types.items[optional-1];
                if (value->kind==XL6_IR_TYPE_NULLABLE) {
                    XL6_IrTypeId inner=value->params[0];
                    value->params[0]=inner == status ? XL6_IR_TYPE_INVALID : status;
                    rejected(&ir);value->params[0]=inner;
                }
            }
        }
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
    printf("stack contract: %u exact element/transfer/identity/ownership/null instructions, %u parameter lifetimes, %u owned/value result contracts; mutations refused before C output\n",checked,lifetimes,ownedResults);
    printf("nullable string contract: %u explicit borrowed comparisons; plain text equality/ordering substitution refused before C output\n",nullableComparisons);
done:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
