/* Continuation liveness is derived only from Lowered instructions and CFG.
 * Both planners and the verifier use this analysis; no frame/action metadata
 * participates in the fixed point or in suspension live-set reconstruction. */
typedef struct XL6_IrContinuationLiveness {
    const XL6_IrModule* module;
    const XL6_IrFunction* function;
    size_t width;
    unsigned char* reachable;
    unsigned char* in;
    unsigned char* out;
    unsigned char* liveValue;
    unsigned char* liveStorage;
} XL6_IrContinuationLiveness;

static void xl6IrContinuationLivenessFree(XL6_IrContinuationLiveness* data)
{
    xrtFree(data->reachable); xrtFree(data->in); xrtFree(data->out); xrtFree(data->liveValue);
    memset(data, 0, sizeof(*data));
}

static void xl6IrContinuationLivenessStep(XL6_IrContinuationLiveness* data,
    const XL6_IrInstruction* instruction)
{
    const XL6_IrModule* module = data->module;
    const XL6_IrFunction* function = data->function;
    size_t local, operand;
    int use, define;
    if (xl6IrAsyncValueLocal(function, instruction->targetValue, &local)) data->liveValue[local] = 0;
    if (xl6IrAsyncValueLocal(function, instruction->result, &local)) data->liveValue[local] = 0;
    for (operand = 0; operand < instruction->operandCount; ++operand)
        if (xl6IrAsyncValueLocal(function, module->operands[instruction->firstOperand + operand], &local))
            data->liveValue[local] = 1;
    xl6IrAsyncStorageEffect(instruction->kind, &use, &define);
    if ((use || define) && instruction->targetSymbol) {
        const XL6_IrStorage* storage = xl6IrStorageGet(module, instruction->targetSymbol);
        if (storage != NULL && storage->owner == function->declaration) {
            local = (size_t)(storage - module->storages);
            if (define) data->liveStorage[local] = 0;
            if (use) data->liveStorage[local] = 1;
        }
    }
}

/* An activation executes the entry-reachable CFG, not every syntactic block
 * retained by lowering (for example an unused finally failure route). Machine
 * coverage and live sets must use this same instruction-derived boundary. */
static int xl6IrContinuationReachable(const XL6_IrModule* module,
    const XL6_IrFunction* function, unsigned char* reachable)
{
    size_t entry, b; int changed;
    if (!reachable || !function->blockCount || function->firstBlock > module->blockCount ||
        function->blockCount > module->blockCount - function->firstBlock ||
        !xl6IrAsyncFunctionBlockIndex(function, function->entryBlock, &entry)) return 0;
    memset(reachable, 0, function->blockCount); reachable[entry] = 1;
    do {
        changed = 0;
        for (b = 0; b < function->blockCount; ++b)
            if (reachable[b]) xl6IrAsyncMarkSuccessors(module, function,
                &module->blocks[function->firstBlock + b], reachable, &changed);
    } while (changed);
    return 1;
}

static int xl6IrContinuationLivenessInit(XL6_IrContinuationLiveness* data,
    const XL6_IrModule* module, const XL6_IrFunction* function)
{
    size_t blocks = function->blockCount, plane, b;
    int changed;
    memset(data, 0, sizeof(*data));
    data->module = module; data->function = function;
    if (!blocks || function->valueCount > SIZE_MAX - module->storageCount) return 0;
    data->width = function->valueCount + module->storageCount;
    if (data->width > SIZE_MAX / blocks) return 0;
    plane = data->width * blocks;
    data->reachable = (unsigned char*)xrtCalloc(blocks, 1);
    data->in = (unsigned char*)xrtCalloc(plane ? plane : 1, 1);
    data->out = (unsigned char*)xrtCalloc(plane ? plane : 1, 1);
    data->liveValue = (unsigned char*)xrtCalloc(data->width ? data->width : 1, 1);
    if (!data->reachable || !data->in || !data->out || !data->liveValue) goto failed;
    data->liveStorage = data->liveValue + function->valueCount;
    if (!xl6IrContinuationReachable(module, function, data->reachable)) goto failed;
    do {
        changed = 0;
        for (b = blocks; b-- > 0;) {
            const XL6_IrBlock* block = &module->blocks[function->firstBlock + b];
            const XL6_IrInstruction* term = block->instructionCount
                ? &module->instructions[block->firstInstruction + block->instructionCount - 1] : NULL;
            XL6_IrBlockId successors[2];
            size_t count = 0, s, bit, scan;
            if (!data->reachable[b]) continue;
            if (term != NULL && (term->kind == XL6_IR_INSTR_BRANCH || term->kind == XL6_IR_INSTR_ERROR_TEST)) {
                successors[count++] = term->trueTarget; successors[count++] = term->falseTarget;
            } else if (term != NULL && term->kind == XL6_IR_INSTR_JUMP) successors[count++] = term->trueTarget;
            memset(data->liveValue, 0, data->width);
            for (s = 0; s < count; ++s) {
                size_t target;
                if (!xl6IrAsyncFunctionBlockIndex(function, successors[s], &target)) goto failed;
                for (bit = 0; bit < data->width; ++bit)
                    data->liveValue[bit] |= data->in[target * data->width + bit];
            }
            memcpy(data->out + b * data->width, data->liveValue, data->width);
            for (scan = block->firstInstruction + block->instructionCount; scan-- > block->firstInstruction;)
                xl6IrContinuationLivenessStep(data, &module->instructions[scan]);
            if (memcmp(data->in + b * data->width, data->liveValue, data->width)) {
                memcpy(data->in + b * data->width, data->liveValue, data->width); changed = 1;
            }
        }
    } while (changed);
    return 1;
failed:
    xl6IrContinuationLivenessFree(data); return 0;
}

static void xl6IrContinuationLivenessAt(XL6_IrContinuationLiveness* data,
    size_t blockLocal, size_t suspensionInstruction)
{
    const XL6_IrModule* module = data->module;
    const XL6_IrFunction* function = data->function;
    const XL6_IrBlock* block = &module->blocks[function->firstBlock + blockLocal];
    const XL6_IrInstruction* suspension = &module->instructions[suspensionInstruction];
    size_t scan, bit, local;
    memcpy(data->liveValue, data->out + blockLocal * data->width, data->width);
    for (scan = block->firstInstruction + block->instructionCount; scan-- > suspensionInstruction + 1;)
        xl6IrContinuationLivenessStep(data, &module->instructions[scan]);
    /* AWAIT's result is supplied by the resume entry, never by a spill. */
    if (xl6IrAsyncValueLocal(function, suspension->result, &local)) data->liveValue[local] = 0;
    for (bit = 0; bit < function->valueCount; ++bit) if (data->liveValue[bit]) {
        size_t definition = SIZE_MAX;
        xl6IrValueDefinition(module, function->firstValue + (XL6_IrValueId)bit, &definition);
        if (definition >= suspensionInstruction) data->liveValue[bit] = 0;
    }
    for (bit = 0; bit < module->storageCount; ++bit) {
        const XL6_IrStorage* storage = &module->storages[bit];
        if (storage->owner != function->declaration || storage->kind == XL6_IR_STORAGE_GLOBAL ||
            storage->kind == XL6_IR_STORAGE_EXTERN_GLOBAL) data->liveStorage[bit] = 0;
    }
}

static int xl6IrContinuationVerifyLiveSets(const XL6_IrModule* module,
    const XL6_IrFunction* function, int generator, size_t machineIndex)
{
    const XL6_IrAsyncMachine* a = generator ? NULL : &module->asyncMachines[machineIndex];
    const XL6_IrGeneratorMachine* g = generator ? &module->generatorMachines[machineIndex] : NULL;
    const XL6_IrContinuationLiveSlot* live = generator ? module->generatorLiveSlots : module->asyncLiveSlots;
    const XL6_IrContinuationFrameSlot* frames = generator ? module->generatorFrameSlots : module->asyncMachineFrameSlots;
    size_t first = generator ? g->firstSuspension : a->firstSuspension;
    size_t count = generator ? g->suspensionCount : a->suspensionCount;
    XL6_IrContinuationLiveness data;
    size_t state;
    int ok = 0;
    if (!xl6IrContinuationLivenessInit(&data, module, function)) return 0;
    for (state = first; count && state < first + count; ++state) {
        const XL6_IrAsyncSuspension* as = generator ? NULL : &module->asyncSuspensions[state];
        const XL6_IrGeneratorSuspension* gs = generator ? &module->generatorSuspensions[state] : NULL;
        size_t firstLive = generator ? gs->firstLiveSlot : as->firstLiveSlot;
        size_t liveCount = generator ? gs->liveSlotCount : as->liveSlotCount;
        size_t instruction = generator ? gs->yieldInstruction : as->awaitInstruction;
        XL6_IrBlockId block = generator ? gs->block : as->block;
        size_t local, bit, cursor = 0;
        if (!xl6IrAsyncFunctionBlockIndex(function, block, &local) || !data.reachable[local]) goto done;
        xl6IrContinuationLivenessAt(&data, local, instruction);
        for (bit = 0; bit < function->valueCount; ++bit) if (data.liveValue[bit]) {
            const XL6_IrContinuationLiveSlot* slot;
            if (cursor == liveCount) goto done;
            slot = &live[firstLive + cursor++];
            if (frames[slot->frameSlot].source != XL6_IR_ASYNC_SPILL_SOURCE_VALUE ||
                slot->value != function->firstValue + bit) goto done;
        }
        for (bit = 0; bit < module->storageCount; ++bit) if (data.liveStorage[bit]) {
            const XL6_IrContinuationLiveSlot* slot;
            if (cursor == liveCount) goto done;
            slot = &live[firstLive + cursor++];
            if (frames[slot->frameSlot].source != XL6_IR_ASYNC_SPILL_SOURCE_STORAGE || slot->value ||
                frames[slot->frameSlot].storageSymbol != module->storages[bit].symbol) goto done;
        }
        if (cursor != liveCount) goto done;
    }
    ok = 1;
done:
    xl6IrContinuationLivenessFree(&data); return ok;
}
