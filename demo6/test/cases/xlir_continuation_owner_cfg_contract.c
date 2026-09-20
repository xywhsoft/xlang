#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    XL6_SymbolId targets[4] = {0}; size_t f, i, changed = 0, transfers = 0, spills = 0;
    assert(argc == 4); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_continuation_owner_cfg.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "owner_cfg"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags)); verify(&ir);
    for (f = 0; f < ir.functionCount; ++f) {
        XL6_IrFunction* fn = &ir.functions[f];
        const XL6_IrNode *decl, *name; size_t yields[4], count = 0, n, kind;
        XL6_IrValueId root, alias;
        if (fn->kind != XL6_IR_FUNCTION_GENERATOR_WORKER) continue;
        decl = xl6IrNodeGet(&ir, fn->declaration);
        name = decl && decl->childCount ? xl6IrNodeGet(&ir, ir.children[decl->firstChild]) : NULL;
        assert(name && name->text);
        kind = !strcmp(name->text, "ownerCfgBranch") ? 0 : !strcmp(name->text, "ownerCfgSkip") ? 1
            : !strcmp(name->text, "ownerCfgLoop") ? 2 : 3;
        assert(kind != 3 || !strcmp(name->text, "ownerCfgStore"));
        for (i = fn->firstInstruction; i < fn->firstInstruction + fn->instructionCount; ++i)
            if (ir.instructions[i].kind == XL6_IR_INSTR_YIELD) { assert(count < 4); yields[count++] = i; }
        assert(count == (kind >= 2 ? 3u : 4u));
        i = syntheticDefinition(&ir, ir.operands[ir.instructions[yields[0]].firstOperand]);
        root = ir.instructions[i].result; alias = ++ir.valueCount;
        assert(ir.instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_COPY);
        asAlias(&ir, &ir.instructions[yields[0]], alias, root);
        for (n = 1; n < count; ++n) {
            size_t copyIndex = syntheticDefinition(&ir, ir.operands[ir.instructions[yields[n]].firstOperand]);
            XL6_IrInstruction* copy = &ir.instructions[copyIndex];
            XL6_IrInstruction* load = &ir.instructions[syntheticDefinition(&ir, ir.operands[copy->firstOperand])];
            assert(load->kind == XL6_IR_INSTR_LOAD && copy->runtimeOp == XL6_IR_RUNTIME_STRING_COPY);
            if (kind == 3 && n > 1) continue;
            asAlias(&ir, load, load->result, root);
            if ((kind == 0 && n >= 2) || (kind != 0 && kind != 3 && n == count - 1)) {
                asAlias(&ir, copy, copy->result, n == 2 && kind == 0 ? root : alias);
                syntheticRetireOutcome(&ir, copyIndex);
            }
        }
        if (kind == 3) {
            size_t stores = 0;
            for (i = fn->firstInstruction; i < fn->firstInstruction + fn->instructionCount; ++i) {
                XL6_IrInstruction* store = &ir.instructions[i];
                const XL6_IrStorage* storage = xl6IrStorageGet(&ir, store->targetSymbol);
                if (store->kind != XL6_IR_INSTR_STORE || storage == NULL || storage->kind != XL6_IR_STORAGE_LOCAL) continue;
                size_t copy = syntheticDefinition(&ir, ir.operands[store->firstOperand]);
                assert(ir.instructions[copy].runtimeOp == XL6_IR_RUNTIME_STRING_COPY);
                asAlias(&ir, &ir.instructions[copy], ir.instructions[copy].result, alias);
                syntheticRetireOutcome(&ir, copy); ++stores;
            }
            assert(stores == 1);
        }
        syntheticCloseOwnerExits(&ir, fn, root);
        relocateOwnerViews(&ir, fn, alias);
        targets[kind] = fn->symbol; ++changed;
    }
    assert(changed == 4 && xl6IrBuildGeneratorMachines(&ir, &parse.diags)); verify(&ir);
    for (i = 0; i < ir.generatorMachineCount; ++i) {
        XL6_IrGeneratorMachine* machine = &ir.generatorMachines[i];
        size_t owner = SIZE_MAX, owners = 0, state, action;
        for (f = machine->firstFrameSlot; f < machine->firstFrameSlot + machine->frameSlotCount; ++f)
            if (ir.generatorFrameSlots[f].transferAbi == XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED) { owner = f; ++owners; }
        assert(owners == 1);
        size_t normalTransfers = 0;
        for (action = machine->firstTransferAction; action < machine->firstTransferAction + machine->transferActionCount; ++action) {
            XL6_IrContinuationFrameAction* command = &ir.generatorFrameActions[action], saved = *command;
            if (syntheticOwnerDisposal(&ir.instructions[command->instruction])) {
                assert(command->frameSlot == owner && command->kind == XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION);
                command->instruction = SIZE_MAX; reject(&ir, NULL); *command = saved;
                continue;
            }
            int store = machine->targetSymbol == targets[3];
            assert(command->frameSlot == owner && command->kind == (store
                ? XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION : XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_ON_YIELD_COMMIT));
            assert(ir.instructions[command->instruction].kind == (store ? XL6_IR_INSTR_STORE : XL6_IR_INSTR_YIELD));
            ++transfers; ++normalTransfers;
            command->instruction = SIZE_MAX; reject(&ir, NULL); *command = saved;
            command->kind = store ? XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_ON_YIELD_COMMIT
                : XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION; reject(&ir, NULL); *command = saved;
        }
        assert(normalTransfers == (machine->targetSymbol == targets[0] ? 2u : 1u));
        for (state = machine->firstSuspension; state < machine->firstSuspension + machine->suspensionCount; ++state) {
            XL6_IrGeneratorSuspension* suspension = &ir.generatorSuspensions[state];
            int liveOwner = 0; size_t live;
            for (live = suspension->firstLiveSlot; suspension->liveSlotCount && live < suspension->firstLiveSlot + suspension->liveSlotCount; ++live)
                if (ir.generatorLiveSlots[live].frameSlot == owner) liveOwner = 1;
            assert(suspension->spillActionCount == (size_t)liveOwner);
            if (liveOwner) {
                XL6_IrGeneratorSuspension saved = *suspension;
                assert(ir.generatorFrameActions[suspension->firstSpillAction].frameSlot == owner);
                ++spills;
                suspension->firstSpillAction = SIZE_MAX; suspension->spillActionCount = 0; reject(&ir, NULL); *suspension = saved;
            }
        }
    }
    assert(transfers == 5 && spills == 5); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "OWNER_CFG", &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    {
        char binding[1024]; assert(snprintf(binding, sizeof(binding), "%s.bind.h", argv[2]) > 0);
        assert(xl6OutputOpenFile(&out, binding));
        xl6OutputPrintf(&out, "#define OWNER_CFG_BRANCH xlir_generator_start_%u\n#define OWNER_CFG_SKIP xlir_generator_start_%u\n#define OWNER_CFG_LOOP xlir_generator_start_%u\n", targets[0], targets[1], targets[2]);
        xl6OutputPrintf(&out, "#define OWNER_CFG_STORE xlir_generator_start_%u\n", targets[3]);
        xl6OutputClose(&out);
    }
    printf("continuation owner CFG: 4 machines, %llu consumer edges, %llu resident installs, %llu mutations rejected\n",
        (unsigned long long)transfers, (unsigned long long)spills, (unsigned long long)mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
