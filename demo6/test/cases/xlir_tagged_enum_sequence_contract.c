#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>

static size_t mutations, recipes, nullableRecipes, erasures;
static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output out; size_t size = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out, ir, "BAD_ENUM_SEQUENCE", NULL) :
            xl6IrCgenEmitC(&out, ir, NULL)));
        char* text = xl6OutputTakeMemoryText(&out, &size);
        assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId id)
{
    for (size_t i = 0; i < ir->instructionCount; ++i)
        if (id && ir->instructions[i].result == id) return &ir->instructions[i];
    return NULL;
}
static void check(XL6_IrModule* ir)
{
    for (size_t i = 0; i < ir->types.count; ++i) {
        XL6_IrType* type = &ir->types.items[i];
        XL6_IrSequenceLayout saved = type->sequenceLayout;
        const XL6_IrType* base = xl6IrTypeGet(&ir->types, saved.baseElementType);
        if (!base || base->kind != XL6_IR_TYPE_TAGGED_ENUM) continue;
        XL6_IrNominalIdentitySetId id = 0;
        assert(saved.abi == XL6_IR_SEQUENCE_LAYOUT_NOMINAL_V2 &&
            saved.slotAbi == XL6_IR_NATIVE_SLOT_XVALUE_CARRIER &&
            saved.transferAbi == XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN &&
            saved.defaultAbi == (saved.nullable ? XL6_IR_SEQUENCE_DEFAULT_ABSENT :
                XL6_IR_SEQUENCE_DEFAULT_FORBIDDEN));
        assert(xl6IrSequenceConstructionRecipe(ir, (XL6_IrTypeId)(i+1), saved.role, &id));
        assert(id && id <= ir->nominalIdentitySetCount);
        XL6_IrNominalIdentitySet* set = &ir->nominalIdentitySets[id-1];
        XL6_IrNominalIdentitySet original = *set;
        assert(set->targetType == saved.baseElementType && set->identityCount == 1 &&
            set->kind == XL6_IR_NOMINAL_IDENTITY_EXACT_TYPE_ID && !set->nonObjectTypeId &&
            ir->nominalIdentities[set->firstIdentity] == base->typeValueId);
#define LAYOUT(FIELD, VALUE) do { type->sequenceLayout.FIELD = (VALUE); reject(ir); \
    assert(!xl6IrSequenceConstructionRecipe(ir, (XL6_IrTypeId)(i+1), saved.role, NULL)); \
    type->sequenceLayout = saved; } while (0)
        LAYOUT(abi, XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2);
        LAYOUT(nullable, !saved.nullable);
        LAYOUT(admissionAbi, XL6_VALUE_ADMIT_ANY);
        LAYOUT(defaultAbi, XL6_IR_SEQUENCE_DEFAULT_ZERO);
#undef LAYOUT
        set->identityCount = 0; reject(ir); *set = original;
        uint64_t identity = ir->nominalIdentities[set->firstIdentity];
        ir->nominalIdentities[set->firstIdentity] = 0; reject(ir);
        ir->nominalIdentities[set->firstIdentity] = identity;
        ++recipes; nullableRecipes += saved.nullable;
    }
    for (size_t i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir->instructions[i];
        if (instruction->runtimeOp != XL6_IR_RUNTIME_STACK_ERASE) continue;
        XL6_IrInstruction saved = *instruction;
        assert(xl6IrStackErasureInstructionCompatible(ir, instruction));
        const XL6_IrInstruction* owner = definition(ir, ir->operands[instruction->firstOperand]);
        assert(owner && owner->kind == XL6_IR_INSTR_RUNTIME_CALL &&
            owner->runtimeOp == XL6_IR_RUNTIME_XVALUE_RETAIN);
        /* The local source is retained for a distinct wrapper. No MOVE may
         * pretend the wrapper result was the source's physical LOAD. */
        for (size_t j = 0; j < ir->instructionCount; ++j) {
            const XL6_IrInstruction* move = &ir->instructions[j];
            if (move->kind == XL6_IR_INSTR_MOVE)
                for (size_t k = 0; k < move->operandCount; ++k)
                    assert(ir->operands[move->firstOperand+k] != instruction->result);
        }
        instruction->runtimeIdentityType = 0; reject(ir); *instruction = saved;
        instruction->payloadType = 0; reject(ir); *instruction = saved;
        instruction->sequenceResultArrayType = 0; reject(ir); *instruction = saved;
        ++erasures;
    }
    assert(xl6IrVerifyModule(ir, NULL));
}
int main(int argc, char** argv)
{
    assert(argc >= 3);
    for (int i = 1; i < argc; ++i) {
        XL6_ParseResult parse; XL6_IrModule ir;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        if (!xl6ParseFile(argv[i], &parse) || !xl6StdlibExpandModule(parse.root, &parse.diags) ||
            !xl6SemCheckModule(parse.root, &parse.diags, "enum_sequence") ||
            !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
            !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) {
            for (size_t j = 0; j < parse.diags.count; ++j)
                fprintf(stderr, "%s\n", parse.diags.items[j].message);
            return 1;
        }
        check(&ir); xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(recipes && nullableRecipes && erasures && mutations);
    printf("tagged enum sequences: %zu exact recipes (%zu nullable), %zu independent Stack erasures, %zu mutations refused before C/header output\n",
        recipes, nullableRecipes, erasures, mutations);
    return 0;
}
