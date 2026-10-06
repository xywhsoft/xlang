/* Included by xl6_ir.c after the canonical operand/owner effect catalogue. */
typedef struct XL6_IrLifetimeSource {
    size_t base;
    XL6_SymbolId storage;
    unsigned char projected;
} XL6_IrLifetimeSource;

/* These operations cannot invoke source code or invalidate borrowed contents.
 * Every other runtime operation is conservatively an observation barrier for
 * mutable projections/global/cell loans. This is an IR effect, not a Cgen
 * naming convention. Storage-reference writes are checked independently. */
static int xl6IrRuntimePreservesLoans(XL6_IrRuntimeOp operation)
{
    switch (operation) {
    case XL6_IR_RUNTIME_STRING_DUP:
    case XL6_IR_RUNTIME_STRING_COPY:
    case XL6_IR_RUNTIME_STRING_CONCAT:
    case XL6_IR_RUNTIME_STRING_EQUAL:
    case XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL:
    case XL6_IR_RUNTIME_NULLABLE_SCALAR_EQUAL:
    case XL6_IR_RUNTIME_STRING_ORDER_LT:
    case XL6_IR_RUNTIME_STRING_ORDER_LE:
    case XL6_IR_RUNTIME_STRING_ORDER_GT:
    case XL6_IR_RUNTIME_STRING_ORDER_GE:
    case XL6_IR_RUNTIME_BOOL_TO_STRING:
    case XL6_IR_RUNTIME_I64_TO_STRING:
    case XL6_IR_RUNTIME_F64_TO_STRING:
    case XL6_IR_RUNTIME_TYPE_ID_TO_STRING:
    case XL6_IR_RUNTIME_STRING_TO_F64_CHECKED:
    case XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED:
    case XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE:
    case XL6_IR_RUNTIME_ERROR_MESSAGE_COPY:
    case XL6_IR_RUNTIME_ERROR_CODE:
    case XL6_IR_RUNTIME_ERROR_DOMAIN_COPY:
    case XL6_IR_RUNTIME_ERROR_OPERATION_COPY:
    case XL6_IR_RUNTIME_ERROR_DATA_COPY:
    case XL6_IR_RUNTIME_ERROR_SYSTEM_CODE:
    case XL6_IR_RUNTIME_ERROR_CAUSE_REF:
    case XL6_IR_RUNTIME_ERROR_KIND_COPY:
    case XL6_IR_RUNTIME_ERROR_KIND_IS:
    case XL6_IR_RUNTIME_U64_TO_STRING:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_U64:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW:
    case XL6_IR_RUNTIME_I64_APPROX:
    case XL6_IR_RUNTIME_U64_APPROX:
    case XL6_IR_RUNTIME_F64_APPROX:
    case XL6_IR_RUNTIME_STRING_APPROX:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE:
    case XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY:
    case XL6_IR_RUNTIME_STRING_UTF8_COUNT:
    case XL6_IR_RUNTIME_STRING_UTF8_AT:
    case XL6_IR_RUNTIME_STRING_CONTAINS:
    case XL6_IR_RUNTIME_I64_RANGE_CONTAINS:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_U64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_F64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE:
    case XL6_IR_RUNTIME_XVALUE_TYPE_ID:
    case XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BORROW:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_CLOSURE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_CLOSURE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NATIVE_HANDLE_BORROW:
    case XL6_IR_RUNTIME_STRING_NULLABLE_COPY:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL:
    case XL6_IR_RUNTIME_INTEGER_DIV_CHECKED:
    case XL6_IR_RUNTIME_INTEGER_MOD_CHECKED:
    case XL6_IR_RUNTIME_INTEGER_SHL_CHECKED:
    case XL6_IR_RUNTIME_INTEGER_SHR_CHECKED:
    case XL6_IR_RUNTIME_I64_POW:
    case XL6_IR_RUNTIME_TIME_APPROX:
    case XL6_IR_RUNTIME_F64_NAN:
    case XL6_IR_RUNTIME_F64_POSITIVE_INFINITY:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME:
    case XL6_IR_RUNTIME_ERROR_FILE_COPY:
    case XL6_IR_RUNTIME_ERROR_LINE:
    case XL6_IR_RUNTIME_ERROR_COLUMN:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_CAST_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED:
    case XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING:
    case XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST:
    case XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_ERROR_REF:
    case XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_FUTURE_COPY:
    case XL6_IR_RUNTIME_XVALUE_NULL:
    case XL6_IR_RUNTIME_XVALUE_BOOL:
    case XL6_IR_RUNTIME_XVALUE_I64:
    case XL6_IR_RUNTIME_XVALUE_U64:
    case XL6_IR_RUNTIME_XVALUE_F64:
    case XL6_IR_RUNTIME_XVALUE_TIME:
    case XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR:
    case XL6_IR_RUNTIME_XVALUE_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_FUTURE_REF:
    case XL6_IR_RUNTIME_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_ERROR_REF:
    case XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW:
    case XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW:
    case XL6_IR_RUNTIME_XVALUE_COUNT:
    case XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE:
    case XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_NEW:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW:
    case XL6_IR_RUNTIME_XVALUE_SET_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_NEW:
    case XL6_IR_RUNTIME_XVALUE_CLASS_NEW:
    case XL6_IR_RUNTIME_XVALUE_TYPE_BIND:
    case XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED:
    case XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND:
    case XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT:
    case XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX:
    case XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX:
    case XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX:
    case XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE:
    case XL6_IR_RUNTIME_XVALUE_UNION_BOX:
    case XL6_IR_RUNTIME_XVALUE_UNION_UNBOX:
    case XL6_IR_RUNTIME_XVALUE_TRUTHY:
    case XL6_IR_RUNTIME_TAGGED_ENUM_NEW:
    case XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL:
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT:
    case XL6_IR_RUNTIME_STRING_FROM_UTF8:
    case XL6_IR_RUNTIME_BYTES_FROM_DATA:
    case XL6_IR_RUNTIME_CHAR_TO_STRING:
    case XL6_IR_RUNTIME_XVALUE_CHAR:
    case XL6_IR_RUNTIME_BYTES_RETAIN:
    case XL6_IR_RUNTIME_BYTES_CONCAT:
    case XL6_IR_RUNTIME_BYTES_EQUAL:
    case XL6_IR_RUNTIME_BYTES_ORDER_LT:
    case XL6_IR_RUNTIME_BYTES_ORDER_LE:
    case XL6_IR_RUNTIME_BYTES_ORDER_GT:
    case XL6_IR_RUNTIME_BYTES_ORDER_GE:
    case XL6_IR_RUNTIME_BYTES_LENGTH:
    case XL6_IR_RUNTIME_BYTES_AT:
    case XL6_IR_RUNTIME_BYTES_SET_COPY:
    case XL6_IR_RUNTIME_STRING_SCALAR_AT:
    case XL6_IR_RUNTIME_STRING_SCALAR_SET_COPY:
    case XL6_IR_RUNTIME_STRING_CONCAT_CHAR_RIGHT:
    case XL6_IR_RUNTIME_STRING_CONCAT_CHAR_LEFT:
        return 1;
    default:
        return 0;
    }
}

static int xl6IrInstructionMayInvalidateLoans(const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    switch (instruction->kind) {
    case XL6_IR_INSTR_CALL:
    case XL6_IR_INSTR_CLOSURE_CALL:
    case XL6_IR_INSTR_VIRTUAL_CALL:
    case XL6_IR_INSTR_PROTOCOL_CALL:
    case XL6_IR_INSTR_ASYNC_START:
    case XL6_IR_INSTR_GENERATOR_START:
    case XL6_IR_INSTR_AWAIT:
    /* The caller can mutate shared storage while a generator is suspended. */
    case XL6_IR_INSTR_YIELD:
    case XL6_IR_INSTR_GENERATOR_NEXT:
    case XL6_IR_INSTR_CELL_RELEASE:
    case XL6_IR_INSTR_PRODUCT_DROP:
        return 1;
    case XL6_IR_INSTR_VALUE_RELEASE:
    case XL6_IR_INSTR_RELEASE: {
        const XL6_IrType* type = xl6IrTypeGet(&module->types, instruction->type);
        /* Freeing text/bytes/error data cannot run a language destructor. The exact
         * destination invalidation of CELL_STORE is still checked separately. */
        return type == NULL || (type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
            type->cAbi != XL6_IR_C_ABI_MANAGED_STRING &&
            type->cAbi != XL6_IR_C_ABI_MANAGED_BYTES &&
            type->cAbi != XL6_IR_C_ABI_RUNTIME_ERROR_PTR);
    }
    case XL6_IR_INSTR_RUNTIME_CALL:
        return !xl6IrRuntimePreservesLoans(instruction->runtimeOp);
    default:
        return 0;
    }
}

/* A loan has a canonical protection source, not an initializer expression.
 * Transparent values preserve that source. Borrowed member/iterator results
 * additionally depend on the contents still being current. The separately
 * checked use of each parent proves the projection was live when formed. */
static int xl6IrLifetimeSourceForValue(const XL6_IrModule* module, size_t definition,
    XL6_IrLifetimeSource* source, size_t depth)
{
    const XL6_IrInstruction* instruction;
    XL6_IrOwnership ownership;
    size_t parent;
    if (definition >= module->instructionCount || depth > module->instructionCount) return 0;
    instruction = &module->instructions[definition];
    if (instruction->kind == XL6_IR_INSTR_VALUE || instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP ||
        instruction->kind == XL6_IR_INSTR_NULLABLE_UNWRAP) {
        if (instruction->operandCount != 1 || instruction->firstOperand >= module->operandCount ||
            instruction->targetValue != 0 ||
            xl6IrValueDefinition(module, module->operands[instruction->firstOperand], &parent) == NULL) return 0;
        return xl6IrLifetimeSourceForValue(module, parent, source, depth + 1);
    }
    source->base = definition; source->storage = 0; source->projected = 0;
    if (instruction->kind == XL6_IR_INSTR_LOAD || instruction->kind == XL6_IR_INSTR_CELL_LOAD) {
        source->storage = instruction->targetSymbol;
        return source->storage != 0;
    }
    ownership = xl6IrValueOwnershipGet(module, instruction->result);
    if (ownership == XL6_IR_OWNERSHIP_MOVE) return 1;
    if (ownership == XL6_IR_OWNERSHIP_BORROW &&
        (instruction->kind == XL6_IR_INSTR_RUNTIME_CALL || instruction->kind == XL6_IR_INSTR_GENERATOR_ITEM_GET)) {
        if (instruction->operandCount == 0 || instruction->firstOperand >= module->operandCount ||
            xl6IrValueDefinition(module, module->operands[instruction->firstOperand], &parent) == NULL ||
            !xl6IrLifetimeSourceForValue(module, parent, source, depth + 1)) return 0;
        source->projected = 1;
        return 1;
    }
    return 0;
}

static int xl6IrInstructionWritesLoanStorage(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_SymbolId symbol, const size_t* definitions)
{
    size_t p;
    if (symbol == 0) return 0;
    if (instruction->targetSymbol == symbol) {
        switch (instruction->kind) {
        case XL6_IR_INSTR_STORE:
        case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
        case XL6_IR_INSTR_RELEASE:
        case XL6_IR_INSTR_MOVE:
        case XL6_IR_INSTR_STORAGE_TAKE:
        case XL6_IR_INSTR_CELL_INIT:
        case XL6_IR_INSTR_CELL_STORE:
        case XL6_IR_INSTR_CELL_EXCHANGE:
        case XL6_IR_INSTR_CELL_RELEASE:
            return 1;
        default: break;
        }
    }
    if (instruction->kind != XL6_IR_INSTR_CALL && instruction->kind != XL6_IR_INSTR_CLOSURE_CALL &&
        instruction->kind != XL6_IR_INSTR_VIRTUAL_CALL && instruction->kind != XL6_IR_INSTR_PROTOCOL_CALL &&
        instruction->kind != XL6_IR_INSTR_RUNTIME_CALL) return 0;
    for (p = 0; p < instruction->operandCount; ++p) {
        XL6_IrValueId value = module->operands[instruction->firstOperand + p];
        const XL6_IrInstruction* reference = value != 0 && value <= module->valueCount && definitions[value] != 0
            ? &module->instructions[definitions[value] - 1] : NULL;
        if (reference != NULL && reference->kind == XL6_IR_INSTR_STORAGE_ADDRESS &&
            reference->targetSymbol == symbol) return 1;
    }
    return 0;
}

/* State = protection mode (dead / borrowed storage / owned SSA) plus whether
 * this exact SSA definition has executed for the current protection lifetime.
 * Tracking exact definitions, not just transparent roots, also rejects stale
 * aliases across a producer's loop re-execution. A transfer closes the old SSA
 * lifetime: subsequent borrowing must use a new LOAD/current owner explicitly.
 * LOAD/MOVE detaches without killing a still-live projection of that value;
 * later writes to the old storage cannot invalidate the detached owner. */
int xl6IrVerifyValueLifetimes(const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t* definitions = NULL;
    size_t* canonical = NULL;
    size_t* uses = NULL;
    size_t* effectOffsets = NULL;
    unsigned char* needed = NULL;
    unsigned char* effects = NULL;
    unsigned char* barriers = NULL;
    size_t f, i, p, effectCount = 0, failed = 0, failedValue = 0;
    XL6_SymbolId failedStorage = 0;
    const char* reason = "unresolved managed borrow protection";
    int result = 0;
    definitions = (size_t*)xrtCalloc((size_t)module->valueCount + 1, sizeof(*definitions));
    canonical = (size_t*)xrtCalloc((size_t)module->valueCount + 1, sizeof(*canonical));
    uses = (size_t*)xrtCalloc(module->operandCount != 0 ? module->operandCount : 1, sizeof(*uses));
    needed = (unsigned char*)xrtCalloc(module->instructionCount != 0 ? module->instructionCount : 1, 1);
    effectOffsets = (size_t*)xrtCalloc(module->instructionCount != 0 ? module->instructionCount : 1, sizeof(*effectOffsets));
    barriers = (unsigned char*)xrtCalloc(module->instructionCount != 0 ? module->instructionCount : 1, 1);
    if (definitions == NULL || canonical == NULL || uses == NULL || needed == NULL || effectOffsets == NULL || barriers == NULL) {
        reason = "analysis allocation failed"; goto done;
    }
    for (i = 0; i < module->instructionCount; ++i) {
        XL6_IrValueId value = module->instructions[i].result;
        if (value != 0 && value <= module->valueCount) definitions[value] = i + 1;
        effectOffsets[i] = effectCount;
        if (module->instructions[i].operandCount > SIZE_MAX - effectCount) {
            reason = "analysis operand size overflow"; goto done;
        }
        effectCount += module->instructions[i].operandCount;
    }
    /* Input slices may be shared by instructions, but their consume/observe
     * effects may not. Cache effects by use site, not by operand arena slot. */
    effects = (unsigned char*)xrtCalloc(effectCount != 0 ? effectCount : 1, 1);
    if (effects == NULL) { reason = "analysis allocation failed"; goto done; }
    for (i = 0; i < module->instructionCount; ++i) {
        const XL6_IrInstruction* instruction = &module->instructions[i];
        /* Resolve effects once. The path walk must not repeatedly search the
         * module's producer/target arenas for each live value at each edge. */
        barriers[i] = (unsigned char)(xl6IrInstructionMayInvalidateLoans(module, instruction) |
            (xl6IrInstructionAlwaysTraps(module, instruction) ? 2 : 0));
        for (p = 0; p < instruction->operandCount; ++p) {
            XL6_IrValueId value = module->operands[instruction->firstOperand + p];
            const XL6_IrType* type;
            size_t definition, root;
            effects[effectOffsets[i] + p] = (unsigned char)xl6IrInstructionOperandOwnership(module, instruction, p);
            if (value == 0 || value > module->valueCount || definitions[value] == 0) continue;
            definition = definitions[value] - 1;
            type = xl6IrTypeGet(&module->types, module->instructions[definition].type);
            if (type == NULL || type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED) continue;
            root = canonical[value] != 0 ? canonical[value] - 1 : xl6IrConsumptionRoot(module, value);
            if (root == SIZE_MAX) { failed = i; failedValue = value; goto done; }
            canonical[value] = root + 1;
            if (module->instructions[root].kind == XL6_IR_INSTR_NULL) continue;
            uses[instruction->firstOperand + p] = definition + 1;
            needed[definition] = 1;
        }
    }
    for (f = 0; f < module->functionCount; ++f) {
        const XL6_IrFunction* function = &module->functions[f];
        size_t valueDefinition, limit = function->firstInstruction + function->instructionCount;
        unsigned char* seen;
        size_t* queue;
        if (function->blockCount == 0) continue;
        /* Temporary storage owns a complete activation, not a call-local
         * formatting pattern: every terminal path must release or detach it.
         * This admits borrowing a pending constructor owner and then moving
         * it out, while still rejecting missing or duplicated cleanup. */
        for (i = 0; i < module->storageCount; ++i) {
            const XL6_IrStorage* slot = &module->storages[i];
            const XL6_IrType* type;
            if (slot->owner != function->declaration || slot->kind != XL6_IR_STORAGE_TEMPORARY ||
                slot->cellType != XL6_IR_TYPE_INVALID) continue;
            type = xl6IrTypeGet(&module->types, slot->type);
            if (type != NULL && type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                !xl6IrVerifyStorageActivationFlowDetail(module, function, slot->symbol, 0, &failed)) {
                reason = "temporary owner has an incomplete or conflicting CFG activation";
                failedStorage = slot->symbol; goto done;
            }
        }
        if (function->blockCount > SIZE_MAX / (6 * sizeof(*queue))) {
            reason = "analysis size overflow"; goto done;
        }
        seen = (unsigned char*)xrtMalloc(function->blockCount);
        queue = (size_t*)xrtMalloc(6 * function->blockCount * sizeof(*queue));
        if (seen == NULL || queue == NULL) {
            xrtFree(seen); xrtFree(queue); reason = "analysis allocation failed"; goto done;
        }
        for (valueDefinition = function->firstInstruction; valueDefinition < limit; ++valueDefinition) {
            XL6_IrLifetimeSource source;
            const XL6_IrStorage* storage;
            int exposed;
            size_t head = 0, tail = 0;
            if (!needed[valueDefinition]) continue;
            failedValue = module->instructions[valueDefinition].result;
            failed = valueDefinition;
            if (!xl6IrLifetimeSourceForValue(module, valueDefinition, &source, 0) ||
                source.base < function->firstInstruction || source.base >= limit) goto flow_failed;
            storage = source.storage != 0 ? xl6IrStorageGet(module, source.storage) : NULL;
            exposed = storage != NULL && (storage->kind == XL6_IR_STORAGE_GLOBAL ||
                storage->kind == XL6_IR_STORAGE_EXTERN_GLOBAL ||
                module->instructions[source.base].kind == XL6_IR_INSTR_CELL_LOAD);
            memset(seen, 0, function->blockCount);
            i = function->entryBlock - 1 - function->firstBlock;
            seen[i] = 1; queue[tail++] = 6 * i;
            while (head < tail) {
                size_t item = queue[head++], b = item / 6;
                unsigned int mode = (unsigned int)((item % 6) / 2), valid = (unsigned int)(item % 2);
                const XL6_IrBlock* block = &module->blocks[function->firstBlock + b];
                const XL6_IrInstruction* last;
                XL6_IrBlockId edges[2];
                int trapped = 0;
                for (i = block->firstInstruction; i < block->firstInstruction + block->instructionCount; ++i) {
                    const XL6_IrInstruction* instruction = &module->instructions[i];
                    int detached = 0;
                    if (barriers[i] & 2) { trapped = 1; break; }
                    for (p = 0; p < instruction->operandCount; ++p) {
                        if (uses[instruction->firstOperand + p] == valueDefinition + 1 && (!valid || mode == 0)) {
                            reason = "use is not protected after transfer, storage invalidation or before definition";
                            failed = i; goto flow_failed;
                        }
                    }
                    /* All operands are observed at the same call boundary;
                     * transferring one must not depend on its argument index. */
                    for (p = 0; p < instruction->operandCount; ++p) {
                        XL6_IrValueId value = module->operands[instruction->firstOperand + p];
                        XL6_IrOperandOwnership effect = (XL6_IrOperandOwnership)effects[effectOffsets[i] + p];
                        if (value == 0 || value > module->valueCount || canonical[value] != source.base + 1) continue;
                        if (effect == XL6_IR_OPERAND_OWNERSHIP_DETACH) { mode = 2; detached = 1; }
                        /* CONSUME_FIELD leaves the carrier's other fields and
                         * trivial projections usable. The shared owner flow
                         * checks the selected field; TAKE has its own token. */
                        if (effect == XL6_IR_OPERAND_OWNERSHIP_CONSUME &&
                            module->instructions[source.base].kind != XL6_IR_INSTR_NULL) mode = 0;
                    }
                    if (source.storage != 0 && mode == 1 && !detached &&
                        xl6IrInstructionWritesLoanStorage(module, instruction, source.storage, definitions)) mode = 0;
                    if (barriers[i] & 1) {
                        if (source.projected) valid = 0;
                        if (exposed && mode == 1) mode = 0;
                    }
                    if (i == source.base) {
                        mode = source.storage != 0 ? 1 : 2;
                        valid = 0;
                    }
                    if (i == valueDefinition) valid = 1;
                }
                if (trapped) continue;
                last = &module->instructions[block->firstInstruction + block->instructionCount - 1];
                edges[0] = last->trueTarget; edges[1] = last->falseTarget;
                for (p = 0; p < 2; ++p) {
                    size_t target; unsigned int state = mode * 2 + valid;
                    if (edges[p] == 0) continue;
                    if (edges[p] <= function->firstBlock || edges[p] > function->firstBlock + function->blockCount) {
                        reason = "invalid lifetime CFG edge"; failed = i - 1; goto flow_failed;
                    }
                    target = edges[p] - 1 - function->firstBlock;
                    if (!(seen[target] & (1u << state))) {
                        seen[target] |= (unsigned char)(1u << state);
                        queue[tail++] = 6 * target + state;
                    }
                }
            }
        }
        xrtFree(seen); xrtFree(queue); continue;
flow_failed:
        xrtFree(seen); xrtFree(queue); goto done;
    }
    result = 1;
done:
    xrtFree(definitions); xrtFree(canonical); xrtFree(uses); xrtFree(needed);
    xrtFree(effects); xrtFree(effectOffsets); xrtFree(barriers);
    if (!result) {
        char message[256];
        snprintf(message, sizeof(message), "XLIR value lifetime: %s (%c%lu, instruction %lu)",
            reason, failedStorage != 0 ? 's' : 'v',
            (unsigned long)(failedStorage != 0 ? failedStorage : failedValue), (unsigned long)failed);
        return xl6IrDiag(diags, failed < module->instructionCount
            ? xl6IrNodeGet(module, module->instructions[failed].origin) : NULL, message);
    }
    return 1;
}
