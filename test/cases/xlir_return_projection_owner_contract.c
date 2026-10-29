#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations, products, moves, duplicates, wrappedDuplicates, borrowedStores, narrowed, cells, classificationChecks;
static size_t joinedOwners, joinBypasses;
static const XL6_IrInstruction* definition(const XL6_IrModule* ir, XL6_IrValueId value)
{
    size_t i;
    for (i = 0; i < ir->instructionCount; ++i)
        if (ir->instructions[i].result == value && value != 0) return &ir->instructions[i];
    return NULL;
}
static void reject(XL6_IrModule* ir, const char* field)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    if (xl6IrVerifyModule(ir, &diags)) {
        fprintf(stderr, "return projection mutation escaped: %s\n", field); assert(0);
    }
    assert(diags.count != 0 && xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitHeader(&out, ir, "REJECTED_RETURN_PROJECTION", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(text == NULL || *text == '\0'); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
#define MUTATE(Type, item, field, value) do { \
    Type saved = *(item); (item)->field = (value); \
    reject(ir, #item "." #field "=" #value); *(item) = saved; \
} while (0)
static void classify(XL6_IrModule* ir)
{
    static const XL6_IrRuntimeOp borrowed[] = {
        XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64, XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64,
        XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL, XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL,
        XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL, XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING,
        XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY, XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BORROW,
        XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL, XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING,
        XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL
    };
    size_t i, j; int exercised = 0;
    assert(xl6IrValueOwnershipGet(NULL, 0) == XL6_IR_OWNERSHIP_NONE);
    assert(xl6IrValueOwnershipGet(ir, 0) == XL6_IR_OWNERSHIP_NONE);
    for (i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir->instructions[i];
        const XL6_IrType* type = xl6IrTypeGet(&ir->types, instruction->type);
        if (instruction->result == 0 || type == NULL) continue;
        if (type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL) {
            assert(xl6IrValueOwnershipGet(ir, instruction->result) == XL6_IR_OWNERSHIP_COPY); ++classificationChecks;
        } else if (instruction->kind == XL6_IR_INSTR_LOAD || instruction->kind == XL6_IR_INSTR_CELL_LOAD) {
            assert(xl6IrValueOwnershipGet(ir, instruction->result) == XL6_IR_OWNERSHIP_BORROW);
            ++classificationChecks; cells += instruction->kind == XL6_IR_INSTR_CELL_LOAD;
        }
        if (instruction->kind == XL6_IR_INSTR_RUNTIME_CALL && instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_RETAIN) {
            XL6_IrInstruction saved = *instruction;
            assert(xl6IrValueOwnershipGet(ir, instruction->result) == XL6_IR_OWNERSHIP_MOVE);
            if (!exercised) {
                /* This is a producer-ABI table unit test, not a claim that
                 * changing only the opcode makes the surrounding IR valid. */
                for (j = 0; j < sizeof(borrowed) / sizeof(borrowed[0]); ++j) {
                    instruction->runtimeOp = borrowed[j];
                    assert(xl6IrValueOwnershipGet(ir, instruction->result) == XL6_IR_OWNERSHIP_BORROW);
                    ++classificationChecks;
                }
                *instruction = saved; exercised = 1;
            }
        }
    }
    assert(exercised && cells != 0);
}
static void verify_join_acquisitions(XL6_IrModule* ir, XL6_IrValueId value, size_t depth)
{
    const XL6_IrInstruction* root = definition(ir, value);
    const XL6_IrStorage* slot;
    size_t i, stores = 0, firstStore = 0;
    assert(depth <= ir->instructionCount);
    while (root != NULL && (root->kind == XL6_IR_INSTR_VALUE || root->kind == XL6_IR_INSTR_NULLABLE_WRAP ||
        root->kind == XL6_IR_INSTR_NULLABLE_UNWRAP)) root = definition(ir, ir->operands[root->firstOperand]);
    if (root == NULL || root->kind != XL6_IR_INSTR_LOAD ||
        (slot = xl6IrStorageGet(ir, root->targetSymbol)) == NULL || slot->kind != XL6_IR_STORAGE_TEMPORARY) return;
    for (i = 0; i < ir->instructionCount; ++i) {
        const XL6_IrInstruction* instruction = &ir->instructions[i];
        if (instruction->kind != XL6_IR_INSTR_STORE || instruction->targetSymbol != slot->symbol) continue;
        if (stores++ == 0) firstStore = i;
        verify_join_acquisitions(ir, ir->operands[instruction->firstOperand], depth + 1);
    }
    if (stores > 1) {
        XL6_IrBlockId storeBlock = 0, loadBlock = 0;
        size_t moveCount = 0;
        ++joinedOwners;
        for (i = 0; i < ir->instructionCount; ++i) {
            XL6_IrInstruction* instruction = &ir->instructions[i];
            if (instruction->kind == XL6_IR_INSTR_MOVE && instruction->targetSymbol == slot->symbol) {
                XL6_IrInstruction saved = *instruction;
                instruction->kind = XL6_IR_INSTR_RELEASE; instruction->operandCount = 0;
                reject(ir, "branch owner must detach explicitly before its next owning STORE");
                *instruction = saved; ++moveCount;
            }
        }
        assert(moveCount == 1);
        for (i = 0; i < ir->blockCount; ++i) {
            const XL6_IrBlock* block = &ir->blocks[i];
            if (firstStore >= block->firstInstruction && firstStore < block->firstInstruction + block->instructionCount)
                storeBlock = (XL6_IrBlockId)i + 1;
            if ((size_t)(root - ir->instructions) >= block->firstInstruction &&
                (size_t)(root - ir->instructions) < block->firstInstruction + block->instructionCount)
                loadBlock = (XL6_IrBlockId)i + 1;
        }
        assert(storeBlock != 0 && loadBlock != 0 && storeBlock != loadBlock);
        for (i = 0; i < ir->instructionCount; ++i) {
            XL6_IrInstruction* branch = &ir->instructions[i];
            if (branch->kind != XL6_IR_INSTR_BRANCH) continue;
            if (branch->trueTarget == storeBlock) { MUTATE(XL6_IrInstruction, branch, trueTarget, loadBlock); ++joinBypasses; }
            if (branch->falseTarget == storeBlock) { MUTATE(XL6_IrInstruction, branch, falseTarget, loadBlock); ++joinBypasses; }
        }
    }
}
static void verify(XL6_IrModule* ir)
{
    size_t i, j, k;
    for (i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir->instructions[i];
        if (instruction->kind == XL6_IR_INSTR_MOVE) {
            const XL6_IrInstruction* load = instruction->operandCount == 1
                ? definition(ir, ir->operands[instruction->firstOperand]) : NULL;
            const XL6_IrNode* source = load != NULL ? xl6IrNodeGet(ir, load->origin) : NULL;
            if (load != NULL && load->kind == XL6_IR_INSTR_LOAD && source != NULL &&
                source->kind == XL6_IR_NODE_NAME && source->type != source->storageType) {
                XL6_IrInstruction* editable = &ir->instructions[load - ir->instructions];
                assert(load->type == source->storageType && instruction->type == load->type);
                MUTATE(XL6_IrInstruction, editable, type, source->type); ++narrowed;
            }
        }
        if (instruction->kind != XL6_IR_INSTR_PRODUCT_MAKE) continue;
        ++products;
        for (j = 0; j < instruction->operandCount; ++j) {
            XL6_IrValueId original = ir->operands[instruction->firstOperand + j];
            const XL6_IrInstruction* root = definition(ir, original);
            const XL6_IrType* type = xl6IrTypeGet(&ir->types, root->type);
            if (type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED) continue;
            verify_join_acquisitions(ir, original, 0);
            for (k = 0; k < j; ++k) {
                XL6_IrValueId other = ir->operands[instruction->firstOperand + k];
                const XL6_IrInstruction* prior = definition(ir, other);
                char label[160];
                if (prior->type != root->type || prior->kind == XL6_IR_INSTR_NULL || other == original) continue;
                ir->operands[instruction->firstOperand + j] = other;
                snprintf(label, sizeof(label), "two product fields consume one acquired owner (instruction %lu, field %lu, value %lu)",
                    (unsigned long)i, (unsigned long)j, (unsigned long)other);
                reject(ir, label);
                ir->operands[instruction->firstOperand + j] = original; ++duplicates; break;
            }
            while (root != NULL && (root->kind == XL6_IR_INSTR_VALUE || root->kind == XL6_IR_INSTR_NULLABLE_WRAP ||
                root->kind == XL6_IR_INSTR_NULLABLE_UNWRAP)) root = definition(ir, ir->operands[root->firstOperand]);
            assert(root != NULL);
            if (root->kind == XL6_IR_INSTR_NULL) continue;
            if (root->kind == XL6_IR_INSTR_LOAD) {
                XL6_IrInstruction* move = NULL;
                const XL6_IrStorage* slot = xl6IrStorageGet(ir, root->targetSymbol);
                if (slot != NULL && slot->kind == XL6_IR_STORAGE_TEMPORARY) {
                    for (k = 0; k < (size_t)(root - ir->instructions); ++k) {
                        const XL6_IrInstruction* store = &ir->instructions[k];
                        const XL6_IrInstruction* input;
                        size_t earlier;
                        if (store->kind != XL6_IR_INSTR_STORE || store->targetSymbol != slot->symbol || store->operandCount != 1) continue;
                        /* Distinct temporary symbols must not hide reuse of
                         * the same independently acquired SSA owner. */
                        for (earlier = 0; earlier < j; ++earlier) {
                            const XL6_IrInstruction* prior = definition(ir, ir->operands[instruction->firstOperand + earlier]);
                            const XL6_IrStorage* priorSlot;
                            size_t priorStore;
                            if (prior == NULL || prior->kind != XL6_IR_INSTR_LOAD || prior->type != root->type ||
                                prior->targetSymbol == root->targetSymbol) continue;
                            priorSlot = xl6IrStorageGet(ir, prior->targetSymbol);
                            if (priorSlot == NULL || priorSlot->kind != XL6_IR_STORAGE_TEMPORARY) continue;
                            for (priorStore = 0; priorStore < k; ++priorStore) {
                                const XL6_IrInstruction* acquisition = &ir->instructions[priorStore];
                                XL6_IrValueId saved;
                                if (acquisition->kind != XL6_IR_INSTR_STORE || acquisition->targetSymbol != prior->targetSymbol ||
                                    acquisition->operandCount != 1) continue;
                                input = definition(ir, ir->operands[acquisition->firstOperand]);
                                if (input == NULL || input->kind == XL6_IR_INSTR_NULL || input->type != store->type ||
                                    xl6IrValueOwnershipGet(ir, input->result) != XL6_IR_OWNERSHIP_MOVE) continue;
                                saved = ir->operands[store->firstOperand];
                                ir->operands[store->firstOperand] = input->result;
                                reject(ir, "distinct temporary fields reuse one acquired SSA owner");
                                ir->operands[store->firstOperand] = saved; ++wrappedDuplicates;
                            }
                        }
                        input = definition(ir, ir->operands[store->firstOperand]);
                        while (input != NULL && input->kind == XL6_IR_INSTR_VALUE)
                            input = definition(ir, ir->operands[input->firstOperand]);
                        if (input != NULL && input->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                            (input->runtimeOp == XL6_IR_RUNTIME_XVALUE_RETAIN ||
                             input->runtimeOp == XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN ||
                             input->runtimeOp == XL6_IR_RUNTIME_STRING_NULLABLE_COPY) &&
                            xl6IrTypeGet(&ir->types, definition(ir, ir->operands[input->firstOperand])->type)->cAbi ==
                                xl6IrTypeGet(&ir->types, input->type)->cAbi) {
                            XL6_IrInstruction* copy = &ir->instructions[input - ir->instructions];
                            XL6_IrInstruction saved = *copy;
                            copy->kind = XL6_IR_INSTR_VALUE; copy->runtimeOp = XL6_IR_RUNTIME_NONE;
                            reject(ir, "temporary MOVE cannot manufacture an owner from a borrowed STORE input");
                            *copy = saved; ++borrowedStores;
                        }
                    }
                }
                for (k = (size_t)(root - ir->instructions) + 1; k < i; ++k) {
                    XL6_IrInstruction* candidate = &ir->instructions[k];
                    if (candidate->kind == XL6_IR_INSTR_MOVE && candidate->targetSymbol == root->targetSymbol &&
                        candidate->operandCount == 1 && ir->operands[candidate->firstOperand] == root->result) move = candidate;
                }
                assert(move != NULL);
                {
                    XL6_IrInstruction saved = *move;
                    move->kind = XL6_IR_INSTR_RELEASE; move->operandCount = 0;
                    reject(ir, "product input storage released instead of detached"); *move = saved; ++moves;
                }
                MUTATE(XL6_IrInstruction, move, targetSymbol, 0);
                MUTATE(XL6_IrInstruction, move, type, 0);
            } else assert(xl6IrValueOwnershipGet(ir, root->result) == XL6_IR_OWNERSHIP_MOVE);
        }
    }
    assert(products >= 9 && moves >= 10 && duplicates >= 4 && wrappedDuplicates >= 2 && borrowedStores >= 6 && narrowed >= 5);
    assert(joinedOwners >= 4 && joinBypasses >= 2);
    assert(xl6IrVerifyModule(ir, NULL));
}
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i;
    assert(argc == 1 || argc == 3);
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile("test/cases/xlir_return_projection_owner.xl", &parse) ||
        !xl6StdlibExpandModule(parse.root, &parse.diags) ||
        !xl6SemCheckModule(parse.root, &parse.diags, "return_projection_contract") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) ||
        !xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    classify(&ir); verify(&ir);
    printf("return projection contract: %lu products, %lu moved fields, %lu duplicate-owner mutations, %lu wrapped-owner mutations, %lu borrowed-store mutations, %lu narrowed storage reads, %lu borrowed cell reads, %lu producer classifications, %lu mutations rejected before C/header output\n",
        (unsigned long)products, (unsigned long)moves, (unsigned long)duplicates, (unsigned long)wrappedDuplicates,
        (unsigned long)borrowedStores, (unsigned long)narrowed,
        (unsigned long)cells, (unsigned long)classificationChecks, (unsigned long)mutations);
    printf("return projection joins: %lu path-dependent owners, %lu skipped-acquisition CFG mutations rejected\n",
        (unsigned long)joinedOwners, (unsigned long)joinBypasses);
    if (argc == 3) {
        XL6_Output out;
        if (!xl6OutputOpenFile(&out, argv[1])) goto failed;
        int ok = xl6IrCgenEmitC(&out, &ir, &parse.diags); xl6OutputClose(&out);
        if (!ok || !xl6OutputOpenFile(&out, argv[2])) goto failed;
        ok = xl6IrCgenEmitHeader(&out, &ir, "RETURN_PROJECTION_OWNERS", &parse.diags);
        xl6OutputClose(&out); if (!ok) goto failed;
    }
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s\n", parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
