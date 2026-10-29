#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t mutations;
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i)
        if (ir->instructions[i].result == value) return &ir->instructions[i];
    return NULL;
}
static XL6_IrBlockId instructionBlock(const XL6_IrModule* ir, size_t index)
{
    size_t i;
    for (i = 0; i < ir->blockCount; ++i)
        if (index >= ir->blocks[i].firstInstruction &&
            index - ir->blocks[i].firstInstruction < ir->blocks[i].instructionCount)
            return (XL6_IrBlockId)(i + 1u);
    return 0;
}
static int reachable(const XL6_IrModule* ir, XL6_IrBlockId start, XL6_IrBlockId target)
{
    unsigned char* visited = (unsigned char*)xrtCalloc(ir->blockCount, 1);
    XL6_IrBlockId* stack = (XL6_IrBlockId*)xrtMalloc(ir->blockCount * sizeof(*stack));
    size_t count = 0; int found = 0;
    assert(visited && stack && start && start <= ir->blockCount);
    visited[start - 1u] = 1; stack[count++] = start;
    while (count && !found) {
        XL6_IrBlockId id = stack[--count];
        const XL6_IrBlock* block = &ir->blocks[id - 1u];
        const XL6_IrInstruction* last;
        XL6_IrBlockId edges[2]; size_t edge;
        if (id == target) { found = 1; break; }
        if (block->instructionCount == 0) continue;
        last = &ir->instructions[block->firstInstruction + block->instructionCount - 1u];
        edges[0] = last->trueTarget; edges[1] = last->falseTarget;
        for (edge = 0; edge < 2; ++edge) {
            XL6_IrBlockId next = edges[edge];
            if (next && next <= ir->blockCount && !visited[next - 1u]) {
                visited[next - 1u] = 1; stack[count++] = next;
            }
        }
    }
    xrtFree(stack); xrtFree(visited); return found;
}
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_STORAGE", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}

static void checkSemanticBoundaries(void)
{
    const char* sources[] = {
        "test/cases/sem_dynamic_member_assign_temporary.xl",
        "test/cases/sem_dynamic_member_assign_compound.xl",
        "test/cases/sem_dynamic_member_assign_length.xl",
        "test/cases/sem_member_assign_type.xl"
    };
    const char* messages[] = {"receiver must be mutable", "only supports =", "length is read-only",
        "assignment expression type does not match member/index target type"};
    size_t i, j;
    for (i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        XL6_ParseResult parse; int found = 0;
        xl6ParseResultInit(&parse);
        assert(xl6ParseFile(sources[i], &parse));
        assert(xl6StdlibExpandModule(parse.root, &parse.diags));
        assert(!xl6SemCheckModule(parse.root, &parse.diags, "member_rejection"));
        for (j = 0; j < parse.diags.count; ++j)
            if (strstr(parse.diags.items[j].message, messages[i])) found = 1;
        assert(found); xl6ParseResultFree(&parse);
    }
    puts("member assignment: 4 existing language-level rejection boundaries preserved");
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t i, j, fields = 0, statics = 0, defaults = 0;
    XL6_SymbolId pending = 0;
    checkSemanticBoundaries();
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_member_assignment_transaction.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "member_assignment"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* test = &ir.instructions[i];
        const XL6_IrNode* origin = xl6IrNodeGet(&ir, test->origin);
        XL6_IrInstruction *branch = NULL, *write = NULL, *release = NULL;
        const XL6_IrInstruction *receiver, *acquisition = NULL, *read = NULL;
        size_t writeIndex = 0;
        if (test->kind != XL6_IR_INSTR_NULL_TEST || !origin ||
            origin->kind != XL6_IR_NODE_ASSIGN || origin->opcode != XL6_IR_OPCODE_NULL_COALESCE) continue;
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* x = &ir.instructions[j];
            if (x->origin != test->origin) continue;
            if (x->kind == XL6_IR_INSTR_BRANCH && x->operandCount == 1 &&
                ir.operands[x->firstOperand] == test->result) { assert(!branch); branch = x; }
            if (x->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                (x->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW ||
                 x->runtimeOp == XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW)) {
                assert(!write); write = x; writeIndex = j;
            }
        }
        assert(branch);
        {
            XL6_IrTypeId saved = test->type;
            test->type = definition(&ir, ir.operands[test->firstOperand])->type;
            assert(test->type != saved); reject(&ir); test->type = saved;
        }
        if (!write) {
            const XL6_IrInstruction* load = definition(&ir, ir.operands[test->firstOperand]);
            const XL6_IrStorage* storage = xl6IrStorageGet(&ir, load->targetSymbol);
            assert(load->kind == XL6_IR_INSTR_LOAD && storage && storage->kind == XL6_IR_STORAGE_GLOBAL);
            ++statics; continue;
        }
        receiver = definition(&ir, ir.operands[write->firstOperand]);
        assert(receiver && receiver->kind == XL6_IR_INSTR_LOAD);
        assert(xl6IrStorageGet(&ir, receiver->targetSymbol)->kind == XL6_IR_STORAGE_TEMPORARY);
        for (j = 0; j < ir.instructionCount; ++j) {
            XL6_IrInstruction* x = &ir.instructions[j];
            if (x->kind == XL6_IR_INSTR_STORE && x->targetSymbol == receiver->targetSymbol) acquisition = x;
            if (j < i && x->kind == XL6_IR_INSTR_RUNTIME_CALL && x->operandCount == 1 &&
                ir.operands[x->firstOperand] == receiver->result && x->immediateText &&
                strcmp(x->immediateText, write->immediateText) == 0) read = x;
            if (x->kind == XL6_IR_INSTR_RELEASE && x->targetSymbol == receiver->targetSymbol &&
                instructionBlock(&ir, j) == branch->falseTarget) { assert(!release); release = x; }
        }
        assert(acquisition && read && release);
        assert(reachable(&ir, branch->trueTarget, instructionBlock(&ir, writeIndex)));
        assert(!reachable(&ir, branch->falseTarget, instructionBlock(&ir, writeIndex)));
        {
            XL6_DiagList diags = {0};
            XL6_IrValueId saved = ir.operands[write->firstOperand];
            ir.operands[write->firstOperand] = ir.operands[acquisition->firstOperand];
            assert(!xl6IrVerifyValueLifetimes(&ir, &diags)); xl6DiagFree(&diags);
            reject(&ir); ir.operands[write->firstOperand] = saved;
        }
        {
            XL6_DiagList diags = {0}; XL6_IrInstruction saved = *release;
            release->kind = XL6_IR_INSTR_DISCARD; release->targetSymbol = 0;
            release->type = test->type; release->firstOperand = branch->firstOperand; release->operandCount = 1;
            assert(!xl6IrVerifyValueLifetimes(&ir, &diags)); xl6DiagFree(&diags);
            reject(&ir); *release = saved;
        }
        ++fields;
    }
    for (i = 0; i < ir.valueInitializerCount; ++i) {
        XL6_IrValueInitializer* init = &ir.valueInitializers[i];
        const XL6_IrNode* declaration = xl6IrNodeGet(&ir, init->declaration);
        if (declaration->kind == XL6_IR_NODE_FIELD_DECL && (declaration->flags & XL6_AST_FLAG_STATIC)) {
            XL6_IrValueInitializer saved = *init;
            assert(init->abi == XL6_IR_INITIALIZER_TAGGED_NULL || init->abi == XL6_IR_INITIALIZER_NULL_SENTINEL);
            init->abi = XL6_IR_INITIALIZER_SCALAR_ZERO; reject(&ir); *init = saved; ++defaults;
        }
        if (declaration->text && strcmp(declaration->text, "localWord") == 0)
            assert(init->abi == XL6_IR_INITIALIZER_STRING_EMPTY);
    }
    for (i = 0; i < ir.classFieldCount; ++i) {
        XL6_IrClassField* field = &ir.classFields[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, field->type);
        if (type->cAbi == XL6_IR_C_ABI_MANAGED_STRING && type->kind == XL6_IR_TYPE_NULLABLE &&
            field->initializer == XL6_IR_NODE_INVALID) {
            XL6_IrInitializerAbi saved = field->initializerAbi;
            assert(saved == XL6_IR_INITIALIZER_NULL_SENTINEL);
            field->initializerAbi = XL6_IR_INITIALIZER_STRING_EMPTY; reject(&ir); field->initializerAbi = saved;
        }
    }
    assert(fields == 12 && statics == 4 && defaults == 2 && mutations == 43);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    assert(argc == 4 && xl6OutputOpenFile(&out, argv[1]));
    assert(xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]));
    assert(xl6IrCgenEmitHeader(&out, &ir, "XLIR_MEMBER_ASSIGNMENT", &parse.diags)); xl6OutputClose(&out);
    for (i = 0; i < ir.storageCount; ++i) {
        const XL6_IrStorage* storage = &ir.storages[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir.types, storage->type);
        if (storage->kind == XL6_IR_STORAGE_GLOBAL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR) {
            assert(!pending); pending = storage->symbol;
        }
    }
    assert(pending && xl6OutputOpenFile(&out, argv[3]));
    xl6OutputPrintf(&out, "#define MEMBER_PENDING s%u\n", (unsigned int)pending); xl6OutputClose(&out);
    printf("member assignment: %llu field and %llu static lazy transactions, %llu default initializers, %llu rejected mutations\n",
        (unsigned long long)fields, (unsigned long long)statics, (unsigned long long)defaults, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    return 0;
}
