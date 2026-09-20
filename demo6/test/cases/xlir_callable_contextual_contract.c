#define main original_owner_contract_main
#include "xlir_owner_consumption_contract.c"
#undef main

int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t phase, i, casts = 0, initializers = 0;
    size_t boundaries[4] = {0};
    if (argc == 5) return generate(argc, argv);
    assert(argc == 1);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_callable_contextual_conversion.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "contextual_callable"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    for (phase = 0; phase < 2; ++phase) {
        if (phase) assert(xl6IrLowerModule(&ir, &parse.diags));
        assert(xl6IrVerifyModule(&ir, &parse.diags));
        for (i = 0; i < ir.valueInitializerCount; ++i) {
            XL6_IrValueInitializer* initializer = &ir.valueInitializers[i];
            if (initializer->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED) continue;
            ++initializers;
            assert(initializer->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
            initializer->runtimeOp = XL6_IR_RUNTIME_NONE;
            reject(&ir, "erase contextual initializer conversion", 0);
            initializer->runtimeOp = XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED;
            initializer->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
            reject(&ir, "erase contextual initializer failure", 0);
            initializer->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        }
        for (i = 0; i < ir.nodeCount; ++i) {
            XL6_IrNode* node = &ir.nodes[i];
            if (node->implicitConversionRuntimeOp != XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED) continue;
            assert(node->implicitConversionErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE);
            node->implicitConversionRuntimeOp = XL6_IR_RUNTIME_NONE;
            reject(&ir, "erase assignment conversion", 0);
            node->implicitConversionRuntimeOp = XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED;
            node->implicitConversionErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
            reject(&ir, "erase assignment conversion failure", 0);
            node->implicitConversionErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        }
    }
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrNodeId source, origin;
        XL6_IrTypeId signature;
        const XL6_IrNode* boundary;
        if (instruction->contextualCastSource == XL6_IR_NODE_INVALID) continue;
        ++casts; source = instruction->contextualCastSource; origin = instruction->origin;
        signature = instruction->calleeType; boundary = xl6IrNodeGet(&ir, origin); assert(boundary);
        if (boundary->kind == XL6_IR_NODE_VALUE_DECL) ++boundaries[0];
        else if (boundary->kind == XL6_IR_NODE_ASSIGN) ++boundaries[1];
        else if (boundary->kind == XL6_IR_NODE_CALL) ++boundaries[2];
        else { assert(boundary->kind == XL6_IR_NODE_RETURN); ++boundaries[3]; }
        instruction->contextualCastSource = XL6_IR_NODE_INVALID;
        reject(&ir, "missing contextual source identity", 0);
        instruction->contextualCastSource = origin;
        reject(&ir, "contextual source replaced by boundary", 0);
        instruction->contextualCastSource = source;
        instruction->origin = source;
        reject(&ir, "cast detached from owning boundary", 0);
        instruction->origin = origin;
        instruction->calleeType = XL6_IR_TYPE_INVALID;
        reject(&ir, "missing concrete callable signature", 0);
        instruction->calleeType = signature;
        assert(i + 1 < ir.instructionCount && ir.instructions[i + 1].kind == XL6_IR_INSTR_ERROR_TEST);
        { XL6_IrBlockId success = ir.instructions[i + 1].trueTarget;
          ir.instructions[i + 1].trueTarget = XL6_IR_BLOCK_INVALID;
          reject(&ir, "missing checked cast continuation", 0);
          ir.instructions[i + 1].trueTarget = success; }
    }
    assert(casts && initializers && boundaries[0] && boundaries[1] && boundaries[2] && boundaries[3]);
    assert(ir.instructions[0].contextualCastSource == XL6_IR_NODE_INVALID);
    ir.instructions[0].contextualCastSource = 1;
    reject(&ir, "contextual cast metadata on unrelated instruction", 0);
    ir.instructions[0].contextualCastSource = XL6_IR_NODE_INVALID;
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    { XL6_Output out; char* text;
      assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out, &ir);
      text = xl6OutputTakeMemoryText(&out, NULL);
      assert(text && strstr(text, "\"contextualCastSource\":") != NULL); xrtFree(text); }
    printf("contextual callable: %zu casts, boundaries=%zu/%zu/%zu/%zu, %zu rejected mutations\n",
        casts, boundaries[0], boundaries[1], boundaries[2], boundaries[3], rejected);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
