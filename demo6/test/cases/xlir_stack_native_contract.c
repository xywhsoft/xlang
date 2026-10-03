#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

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
        assert(!xl6IrVerifyModule(&ir,NULL)); instruction->payloadType = payload;
        XL6_IrRuntimeResourcePayloadAbi transfer = instruction->runtimeResourcePayloadAbi;
        instruction->runtimeResourcePayloadAbi = XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
        assert(!xl6IrVerifyModule(&ir,NULL)); instruction->runtimeResourcePayloadAbi = transfer;
        XL6_IrType* type = &ir.types.items[payload-1]; uint64_t identity = type->typeValueId;
        type->typeValueId = 0; assert(!xl6IrVerifyModule(&ir,NULL)); type->typeValueId = identity;
        assert(xl6IrVerifyModule(&ir,NULL)); ++checked;
    }
    assert(checked);
    if (!xl6OutputOpenFile(&out,argv[2])) goto done;
    ok = xl6IrCgenEmitC(&out,&ir,&parse.diags); xl6OutputClose(&out);
    printf("stack contract: %u exact element/transfer/identity instructions\n",checked);
done:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
