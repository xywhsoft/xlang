#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t checked, mutations, joins, projections, physicalConversions;
static void semantic(const char* body, int expected)
{
    static const char prefix[] =
        "class Item\n int id = 3\nend\n"
        "func fill() as int\n return 1\nend\n"
        "func fillBool() as bool\n return true\nend\n"
        "func pair(int other) as (int, int)\n return other, other\nend\n";
    char source[4096]; XL6_ParseResult parse; int result; size_t i;
    size_t prefixSize = strlen(prefix), bodySize = strlen(body);
    assert(prefixSize + bodySize < sizeof(source));
    memcpy(source, prefix, prefixSize);
    memcpy(source + prefixSize, body, bodySize + 1);
    xl6ParseResultInit(&parse);
    assert(xl6ParseSource(source, "nullable-join-semantic.xl", &parse));
    result = xl6SemCheckModule(parse.root, &parse.diags, "nullable_join");
    if (result != expected) {
        fprintf(stderr, "semantic expectation=%d actual=%d:\n%s\n", expected, result, body);
        for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
        assert(0);
    }
    assert(expected || parse.diags.count != 0); xl6ParseResultFree(&parse); ++checked;
}
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* output;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitC(&out, ir, &diags));
    output = xl6OutputTakeMemoryText(&out, NULL); assert(output == NULL || *output == '\0'); xrtFree(output);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out, ir, "BAD_JOIN", &diags));
    output = xl6OutputTakeMemoryText(&out, NULL); assert(output == NULL || *output == '\0'); xrtFree(output);
    xl6DiagFree(&diags); ++mutations;
}
static void lowered(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_conditional_nullable_join.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "nullable_join"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    for (i = 0; i < ir.nodeCount; ++i) {
        const XL6_IrNode* node = &ir.nodes[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, node->type);
        if (node->kind == XL6_IR_NODE_CONDITIONAL && type != NULL && type->kind == XL6_IR_TYPE_NULLABLE) {
            const XL6_IrType* payload;
            assert(type->paramCount == 1 && type->params != NULL);
            payload = xl6IrTypeGet(&ir.types, type->params[0]);
            assert(payload != NULL && payload->kind != XL6_IR_TYPE_NULLABLE); ++joins;
        }
    }
    assert(joins >= 12 && xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrInstruction saved = *instruction;
        if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN) {
            const XL6_IrType* type = xl6IrTypeGet(&ir.types, instruction->type);
            assert(type != NULL && type->kind == XL6_IR_TYPE_NULLABLE && type->params[0] == instruction->payloadType);
            assert(xl6IrValueOwnershipGet(&ir, instruction->result) == XL6_IR_OWNERSHIP_MOVE);
            instruction->type = instruction->payloadType; reject(&ir); *instruction = saved;
            instruction->payloadType = XL6_IR_TYPE_INVALID; reject(&ir); *instruction = saved;
            instruction->runtimeIdentityType = XL6_IR_TYPE_INVALID; reject(&ir); *instruction = saved;
            ++projections;
        }
        if (instruction->runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED && instruction->operandCount == 1) {
            XL6_IrValueId operand = ir.operands[instruction->firstOperand]; size_t j;
            for (j = 0; j < ir.instructionCount; ++j) {
                XL6_IrInstruction* input = &ir.instructions[j];
                const XL6_IrNode* source = xl6IrNodeGet(&ir, input->origin);
                if (input->result == operand && input->kind == XL6_IR_INSTR_LOAD && source != NULL &&
                    source->kind == XL6_IR_NODE_NAME && source->type != source->storageType) {
                    XL6_IrInstruction inputSaved = *input;
                    assert(input->type == source->storageType);
                    input->type = source->type; reject(&ir); *input = inputSaved; ++physicalConversions;
                }
            }
        }
    }
    assert(projections != 0 && physicalConversions != 0 && xl6IrVerifyModule(&ir, &parse.diags));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}
int main(void)
{
    semantic("func bad(bool c) as int\n return c ? 1 : null\nend\n", 0);
    semantic("func bad(bool c) as int\n auto item = c ? Item() : null\n return item.id\nend\n", 0);
    semantic("func bad(bool c) as int\n var item = c ? Item() : null\n return item.id\nend\n", 0);
    semantic("func good(Item? item) as int\n return item != null ? item.id : 0\nend\n", 1);
    semantic("func good(Item? item) as int\n return item == null ? 0 : item.id\nend\n", 1);
    semantic("func bad(Item? item) as int\n auto seen = item != null ? item.id : 0\n return item.id\nend\n", 0);
    semantic("func bad(Item? item, bool c) as int\n if item != null\n item = c ? null : item\n return item.id\n end\n return 0\nend\n", 0);
    semantic("func bad(bool c)\n if c ? true : null\n end\nend\n", 0);
    semantic("func bad(bool c) as bool\n return (c ? true : null) and true\nend\n", 0);
    semantic("func bad(bool c) as int\n int value\n if c then value = fill() end\n return value\nend\n", 0);
    semantic("func good(bool c) as int\n int value\n if c then value = fill() else value = 0 end\n return value\nend\n", 1);
    semantic("func good(bool c) as int\n return c ? fill() : 0\nend\n", 1);
    semantic("func good(bool c) as bool\n return c and fillBool()\nend\n", 1);
    semantic("func good(int? first) as int\n return first ?? fill()\nend\n", 1);
    semantic("func good(int other) as int\n auto left, right = pair(other)\n return left + right\nend\n", 1);
    semantic("func good(bool c) as int?\n int? prior = 2\n return c ? prior : null\nend\n", 1);
    semantic("func good(bool c) as var\n return c ? 1 : \"different domain\"\nend\n", 1);
    lowered();
    printf("nullable joins: %llu semantic cases, %llu nullable joins, %llu exact projections, %llu physical conversions, %llu rejected IR mutations\n",
        (unsigned long long)checked, (unsigned long long)joins, (unsigned long long)projections,
        (unsigned long long)physicalConversions, (unsigned long long)mutations);
    return 0;
}
