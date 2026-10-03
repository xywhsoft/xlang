static size_t xl6IrCgenGeneratorFrameStorageSlot(
    const XL6_IrModule* module, const XL6_IrGeneratorMachine* machine,
    XL6_SymbolId symbol)
{
    size_t local;
    if ( module == NULL || machine == NULL ) return SIZE_MAX;
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrContinuationFrameSlot* frame =
            &module->generatorFrameSlots[machine->firstFrameSlot + local];
        if ( frame->source == XL6_IR_ASYNC_SPILL_SOURCE_STORAGE &&
             frame->storageSymbol == symbol ) return machine->firstFrameSlot + local;
    }
    return SIZE_MAX;
}

static const XL6_IrStorage* xl6IrCgenGeneratorLocalStorage(
    const XL6_IrModule* module, const XL6_IrGeneratorMachine* machine,
    const XL6_IrFunction* function, XL6_SymbolId symbol)
{
    const XL6_IrStorage* storage = xl6IrStorageGet(module, symbol);
    if ( storage == NULL || function == NULL ||
         xl6IrCgenGeneratorFrameStorageSlot(module, machine, symbol) != SIZE_MAX ||
         storage->owner != function->declaration ||
         (storage->kind != XL6_IR_STORAGE_LOCAL &&
          storage->kind != XL6_IR_STORAGE_TEMPORARY) ) return NULL;
    return storage;
}


static int xl6IrCgenGeneratorMachineProgramSupported(
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrGeneratorMachine* machine)
{
    const XL6_IrFunction* function;
    size_t local;
    if ( module == NULL || target == NULL || machine == NULL ||
         machine->function >= module->functionCount ) return 0;
    function = &module->functions[machine->function];
    if ( function->kind != XL6_IR_FUNCTION_GENERATOR_WORKER ||
         function->returnType != XL6_IR_TYPE_INVALID ||
         machine->itemType != target->generatorItemType ||
         machine->closeAbi != XL6_IR_GENERATOR_CLOSE_UNWIND_DRAIN_FINALLY ||
         machine->yieldAbi != target->generatorYieldAbi ) return 0;
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrContinuationFrameSlot* frame =
            &module->generatorFrameSlots[machine->firstFrameSlot + local];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, frame->type);
        if ( type == NULL || xl6IrCgenType(module, frame->type) == NULL ) return 0;
        if ( frame->destroyAbi < XL6_IR_ASYNC_FRAME_DESTROY_NONE ||
             frame->destroyAbi > XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_GENERATOR ) return 0;
        if ( frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_FRAME_STORAGE ) {
            if ( !xl6IrCgenFrameStorageSupported(module, frame, machine->initBitCount) ) return 0;
        } else if ( frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_LAUNCH_PARAMETER ) {
            if (!xl6IrCgenActivationFrameParameterSupported(module, target, frame, machine->initBitCount)) return 0;
        } else if ( frame->residence == XL6_IR_ASYNC_FRAME_RESIDENCE_SSA_SPILL ) {
            if ( frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_BORROW_STORAGE &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_COPY_VALUE &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_COPY_STRING &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_XVALUE &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_FUTURE &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CLOSURE &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_CELL &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_RETAIN_ERROR &&
                 frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED ) return 0;
        } else return 0;
    }
    for ( local = 0; local < machine->blockCount; ++local ) {
        const XL6_IrGeneratorMachineBlock* block =
            &module->generatorMachineBlocks[machine->firstBlock + local];
        size_t instructionIndex;
        for ( instructionIndex = block->firstInstruction;
              instructionIndex < block->firstInstruction + block->instructionCount;
              ++instructionIndex ) {
            const XL6_IrInstruction* instruction = &module->instructions[instructionIndex];
            switch ( instruction->kind ) {
            case XL6_IR_INSTR_PRODUCT_MAKE:
            case XL6_IR_INSTR_PRODUCT_GET:
            case XL6_IR_INSTR_PRODUCT_TAKE:
            case XL6_IR_INSTR_PRODUCT_DROP:
                if (!xl6IrCgenProductInstructionAbi(module, instruction)) return 0;
                break;
            case XL6_IR_INSTR_CELL_NEW:
            case XL6_IR_INSTR_CELL_INIT:
            case XL6_IR_INSTR_CELL_LOAD:
            case XL6_IR_INSTR_CELL_STORE:
            case XL6_IR_INSTR_CELL_EXCHANGE:
            case XL6_IR_INSTR_CELL_RETAIN:
            case XL6_IR_INSTR_CELL_RELEASE:
                if (xl6IrCgenGeneratorFrameStorageSlot(module, machine, instruction->targetSymbol) == SIZE_MAX &&
                    xl6IrCgenGeneratorLocalStorage(module, machine, function, instruction->targetSymbol) == NULL)
                    return 0;
                break;
            case XL6_IR_INSTR_CONST:
            case XL6_IR_INSTR_VALUE_RELEASE:
                break;
            case XL6_IR_INSTR_LOAD:
            case XL6_IR_INSTR_STORE:
            case XL6_IR_INSTR_MOVE:
            case XL6_IR_INSTR_STORAGE_TAKE:
            case XL6_IR_INSTR_RELEASE:
                if (xl6IrCgenGeneratorFrameStorageSlot(module, machine, instruction->targetSymbol) == SIZE_MAX &&
                    xl6IrCgenGeneratorLocalStorage(module, machine, function, instruction->targetSymbol) == NULL &&
                    xl6IrCgenSharedStorage(module, instruction->targetSymbol) == NULL) return 0;
                break;
            case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
                if (xl6IrCgenGeneratorFrameStorageSlot(module, machine, instruction->targetSymbol) == SIZE_MAX &&
                    xl6IrCgenGeneratorLocalStorage(module, machine, function, instruction->targetSymbol) == NULL) return 0;
                break;
            case XL6_IR_INSTR_BINARY:
            case XL6_IR_INSTR_UNARY:
            case XL6_IR_INSTR_VALUE:
            case XL6_IR_INSTR_DISCARD:
            case XL6_IR_INSTR_NULL:
            case XL6_IR_INSTR_NULL_TEST:
            case XL6_IR_INSTR_NULLABLE_WRAP:
            case XL6_IR_INSTR_NULLABLE_UNWRAP:
                if ( instruction->kind == XL6_IR_INSTR_NULL &&
                     !xl6IrCgenNullInstructionAbi(
                         module, instruction) ) return 0;
                if ( (instruction->kind == XL6_IR_INSTR_NULL_TEST ||
                      instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP ||
                      instruction->kind == XL6_IR_INSTR_NULLABLE_UNWRAP) &&
                     !xl6IrCgenNullableInstructionAbi(
                         module, instruction) ) return 0;
                break;
            case XL6_IR_INSTR_BRANCH:
            case XL6_IR_INSTR_JUMP:
            case XL6_IR_INSTR_RETURN:
            case XL6_IR_INSTR_THROW:
            case XL6_IR_INSTR_ERROR_TEST:
            case XL6_IR_INSTR_YIELD:
            case XL6_IR_INSTR_GENERATOR_CLOSE_TEST:
            case XL6_IR_INSTR_TRAP_IF_FALSE:
                break;
            case XL6_IR_INSTR_ERROR_RAISE:
            case XL6_IR_INSTR_ERROR_TAKE:
            case XL6_IR_INSTR_CLEANUP_BEGIN:
            case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN:
            case XL6_IR_INSTR_CLEANUP_CAPTURE:
            case XL6_IR_INSTR_CLEANUP_END:
            case XL6_IR_INSTR_ERROR_UNWIND_BEGIN:
            case XL6_IR_INSTR_ERROR_UNWIND_END:
            case XL6_IR_INSTR_ERROR_SET_TAKE:
            case XL6_IR_INSTR_ERROR_CLEAR:
                if (!xl6IrCgenErrorStateInstructionAbi(module, instruction)) return 0;
                break;
            case XL6_IR_INSTR_RUNTIME_CALL:
                /* Runtime operands/results have the same verified ABI in
                 * every execution mode. The shared instruction verifier and
                 * emitter own this contract; only frame/control operations
                 * require a machine-specific admission check. */
                break;
            case XL6_IR_INSTR_CLOSURE_MAKE:
                {
                    const XL6_IrClosureTarget* closure =
                        xl6IrCgenClosureTargetGet(
                            module, instruction->targetSymbol);
                    size_t capture;
                    if ( closure == NULL ||
                         instruction->operandCount != closure->captureCount ||
                         closure->firstCapture > module->closureCaptureCount ||
                         closure->captureCount >
                             module->closureCaptureCount -
                                 closure->firstCapture ) return 0;
                    for ( capture = 0; capture < closure->captureCount;
                          ++capture ) {
                        XL6_IrClosureCaptureAbi abi = module->closureCaptures[
                            closure->firstCapture + capture].abi;
                        if ( abi != XL6_IR_CLOSURE_CAPTURE_COPY_VALUE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_COPY_STRING &&
                             abi != XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE &&
                             abi != XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL ) return 0;
                    }
                }
                break;
            case XL6_IR_INSTR_CLOSURE_CALL:
                if ( !xl6IrCgenClosureCallAbi(
                         module, instruction) ) return 0;
                break;
            case XL6_IR_INSTR_RETAIN:
                if ( !xl6IrCgenRetainInstructionAbi(
                         module, instruction) ) return 0;
                break;
            case XL6_IR_INSTR_PROTOCOL_CALL:
                if ( !xl6IrCgenProtocolCallAbi(module, instruction) ) return 0;
                break;
            case XL6_IR_INSTR_VIRTUAL_CALL:
                if (!xl6IrCgenVirtualCallAbi(module, instruction)) return 0;
                break;
            case XL6_IR_INSTR_ASYNC_START:
                if ( !xl6IrCgenAsyncStartAbi(module, instruction) ) return 0;
                break;
            case XL6_IR_INSTR_GENERATOR_START:
                if (!xl6IrCgenLaunchCallAbi(module, instruction)) return 0;
                break;
            case XL6_IR_INSTR_GENERATOR_NEXT:
            case XL6_IR_INSTR_GENERATOR_ITEM_GET:
            case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
                /* Shared verified generator-consumer ABI. */
                break;
            case XL6_IR_INSTR_AWAIT:
                if ( !xl6IrCgenBlockingAwaitAbi(module, instruction) ) return 0;
                break;
            case XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK:
                /* The module verifier owns the failure-edge ownership proof. */
                break;
            case XL6_IR_INSTR_CALL:
                {
                    const XL6_IrCallTarget* call =
                        xl6IrCallTargetGet(module, instruction->targetSymbol);
                    if ( !xl6IrCgenDirectCallAbiSupported(
                             module, call, instruction) ) return 0;
                }
                break;
            default:
                return 0;
            }
        }
    }
    return 1;
}



static int xl6IrCgenEmitGeneratorInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallTarget* target, const XL6_IrGeneratorMachine* machine,
    const XL6_IrFunction* function, const XL6_IrGeneratorMachineBlock* block,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    size_t frameSlot;
    if (!xl6IrCgenEmitContinuationTransfers(out, module, 1, (size_t)(machine - module->generatorMachines),
            instruction, XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_BEFORE_INSTRUCTION)) return 0;
    switch ( instruction->kind ) {
    case XL6_IR_INSTR_GENERATOR_CLOSE_TEST:
        xl6OutputPrintf(out, "    v%u = generator->closing;\n", (unsigned int)instruction->result);
        break;
    case XL6_IR_INSTR_PRODUCT_MAKE:
    case XL6_IR_INSTR_PRODUCT_GET:
    case XL6_IR_INSTR_PRODUCT_TAKE:
    case XL6_IR_INSTR_PRODUCT_DROP:
        if (!xl6IrCgenEmitProductInstruction(out, module, instruction, diags)) return 0;
        break;
    case XL6_IR_INSTR_CONST:
        if (xl6IrCgenResultStorageOmitted(module, instruction)) break;
        if ( instruction->immediateText == NULL )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR generator constant has no canonical payload");
        xl6OutputPrintf(out, "    v%u = %s;\n",
                        (unsigned int)instruction->result, instruction->immediateText);
        break;
    case XL6_IR_INSTR_LOAD:
        if (xl6IrCgenResultStorageOmitted(module, instruction)) break;
        frameSlot = xl6IrCgenGeneratorFrameStorageSlot(
            module, machine, instruction->targetSymbol);
        if ( frameSlot != SIZE_MAX )
            xl6OutputPrintf(out, "    v%u = frame->f%lu;\n",
                (unsigned int)instruction->result,
                (unsigned long)(frameSlot - machine->firstFrameSlot));
        else if ( xl6IrCgenGeneratorLocalStorage(
                      module, machine, function, instruction->targetSymbol) != NULL ||
                  xl6IrCgenSharedStorage(module, instruction->targetSymbol) != NULL ) {
            xl6OutputPrintf(out, "    v%u = ", (unsigned int)instruction->result);
            xl6IrCgenPutStorageName(out, module, instruction->targetSymbol);
            xl6OutputPuts(";\n", out);
        }
        else return xl6IrCgenDiag(diags, module, instruction->origin,
                                  "XLIR generator load has no frame/local binding");
        break;
    case XL6_IR_INSTR_CELL_NEW:
    case XL6_IR_INSTR_CELL_INIT:
    case XL6_IR_INSTR_CELL_LOAD:
    case XL6_IR_INSTR_CELL_STORE:
    case XL6_IR_INSTR_CELL_EXCHANGE:
    case XL6_IR_INSTR_CELL_RETAIN:
    case XL6_IR_INSTR_CELL_RELEASE:
        frameSlot = xl6IrCgenGeneratorFrameStorageSlot(module, machine, instruction->targetSymbol);
        if (frameSlot == SIZE_MAX && xl6IrCgenGeneratorLocalStorage(
                module, machine, function, instruction->targetSymbol) == NULL)
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR generator cell has no frame/local binding");
        if (!xl6IrCgenEmitCellInstruction(out, module, instruction, NULL,
                frameSlot != SIZE_MAX ? frameSlot - machine->firstFrameSlot : SIZE_MAX,
                frameSlot != SIZE_MAX ? module->generatorFrameSlots[frameSlot].initBit : SIZE_MAX,
                diags)) return 0;
        break;
    case XL6_IR_INSTR_STORE:
    case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
        frameSlot = xl6IrCgenGeneratorFrameStorageSlot(
            module, machine, instruction->targetSymbol);
        if ( instruction->operandCount != 1 )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR generator store has no single owned value");
        if (frameSlot == SIZE_MAX && xl6IrCgenGeneratorLocalStorage(module,machine,function,instruction->targetSymbol)==NULL &&
            !(instruction->kind==XL6_IR_INSTR_STORE && xl6IrCgenSharedStorage(module,instruction->targetSymbol)!=NULL))
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                  "XLIR generator store has no frame/local binding");
        if (!xl6IrCgenEmitStorageCommit(out,module,instruction,
                frameSlot==SIZE_MAX?SIZE_MAX:frameSlot-machine->firstFrameSlot,
                frameSlot==SIZE_MAX?SIZE_MAX:module->generatorFrameSlots[frameSlot].initBit,diags)) return 0;
        break;
    case XL6_IR_INSTR_MOVE:
    case XL6_IR_INSTR_STORAGE_TAKE:
        {
            const XL6_IrContinuationFrameSlot* frame;
            frameSlot = xl6IrCgenGeneratorFrameStorageSlot(module, machine, instruction->targetSymbol);
            frame = frameSlot != SIZE_MAX ? &module->generatorFrameSlots[frameSlot] : NULL;
            if (!xl6IrCgenEmitMoveInstruction(out, module, instruction,
                    frame != NULL ? frameSlot - machine->firstFrameSlot : SIZE_MAX,
                    frame != NULL ? frame->initBit : SIZE_MAX, diags)) return 0;
        }
        break;
    case XL6_IR_INSTR_RELEASE:
    case XL6_IR_INSTR_VALUE_RELEASE:
        {
            if (instruction->kind == XL6_IR_INSTR_VALUE_RELEASE) {
                if (!xl6IrCgenEmitValueRelease(out, module, instruction, diags)) return 0;
                break;
            }
            frameSlot = xl6IrCgenGeneratorFrameStorageSlot(
                module, machine, instruction->targetSymbol);
            if ( frameSlot != SIZE_MAX ) {
                const XL6_IrContinuationFrameSlot* frame =
                    &module->generatorFrameSlots[frameSlot];
                if ( !xl6IrCgenEmitFrameRelease(out, module, frame->type,
                         frame->destroyAbi, frameSlot - machine->firstFrameSlot,
                         frame->initBit) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR generator frame release has no exact lifetime ABI");
            } else if ( (xl6IrCgenGeneratorLocalStorage(module, machine, function,
                            instruction->targetSymbol) == NULL &&
                         xl6IrCgenSharedStorage(module, instruction->targetSymbol) == NULL) ||
                        !xl6IrCgenEmitSymbolRelease(out, module, instruction->type,
                            instruction->targetSymbol, 0) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR generator release has no local binding");
        }
        break;
    case XL6_IR_INSTR_BINARY:
    case XL6_IR_INSTR_UNARY:
        {
            const char* op = xl6IrCgenOperator(instruction->opcode);
            if ( op == NULL || instruction->operandCount !=
                 (instruction->kind == XL6_IR_INSTR_BINARY ? 2u : 1u) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR generator scalar operator is unresolved");
            if ( instruction->kind == XL6_IR_INSTR_BINARY )
                xl6OutputPrintf(out, "    v%u = v%u %s v%u;\n",
                    (unsigned int)instruction->result,
                    (unsigned int)module->operands[instruction->firstOperand], op,
                    (unsigned int)module->operands[instruction->firstOperand + 1]);
            else xl6OutputPrintf(out, "    v%u = %s v%u;\n",
                    (unsigned int)instruction->result, op,
                    (unsigned int)module->operands[instruction->firstOperand]);
        }
        break;
    case XL6_IR_INSTR_VALUE:
        xl6OutputPrintf(out, "    v%u = v%u;\n",
            (unsigned int)(instruction->result != XL6_IR_VALUE_INVALID
                ? instruction->result : instruction->targetValue),
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_INSTR_DISCARD:
    case XL6_IR_INSTR_TRAP_IF_FALSE:
        if ( !xl6IrCgenEmitSimpleObservation(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_NULL:
        if ( !xl6IrCgenEmitNullInstruction(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_NULL_TEST:
    case XL6_IR_INSTR_NULLABLE_WRAP:
    case XL6_IR_INSTR_NULLABLE_UNWRAP:
        if ( !xl6IrCgenEmitNullableInstruction(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_RUNTIME_CALL:
        if (!xl6IrCgenEmitRuntimeCallInstruction(out, module, instruction,
                (size_t)(instruction - module->instructions), diags)) return 0;
        break;
    case XL6_IR_INSTR_CLOSURE_MAKE:
        if ( !xl6IrCgenEmitClosureMakeInstruction(
                out, module, instruction,
                (size_t)(instruction - module->instructions),
                diags) ) return 0;
        break;
    case XL6_IR_INSTR_CLOSURE_CALL:
        if ( !xl6IrCgenEmitClosureCallInstruction(
                out, module, instruction,
                (size_t)(instruction - module->instructions),
                diags) ) return 0;
        break;
    case XL6_IR_INSTR_RETAIN:
        if ( !xl6IrCgenEmitRetainInstruction(
                out, module, instruction) )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR generator retain ABI is unresolved");
        break;
    case XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK:
        if (!xl6IrCgenEmitForeignResultRollback(out, module, instruction)) return 0;
        break;
    case XL6_IR_INSTR_VIRTUAL_CALL:
        if (!xl6IrCgenEmitDispatchCallInstruction(out, module, instruction, diags)) return 0;
        break;
    case XL6_IR_INSTR_CALL:
        {
            size_t instructionIndex =
                (size_t)(instruction - module->instructions);
            if ( !xl6IrCgenEmitDirectCall(
                    out, module, instruction, instructionIndex, diags,
                    "XLIR generator call target is unresolved") ) return 0;
        }
        break;
    case XL6_IR_INSTR_PROTOCOL_CALL:
        if ( !xl6IrCgenEmitDispatchCallInstruction(
                out, module, instruction, diags) ) return 0;
        break;
    case XL6_IR_INSTR_ASYNC_START:
    case XL6_IR_INSTR_GENERATOR_START:
        if (!xl6IrCgenEmitLaunchCall(out, module, instruction, diags)) return 0;
        break;
    case XL6_IR_INSTR_GENERATOR_NEXT:
    case XL6_IR_INSTR_GENERATOR_ITEM_GET:
    case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
        if (!xl6IrCgenEmitGeneratorConsumerInstruction(out, module, instruction)) return 0;
        break;
    case XL6_IR_INSTR_AWAIT:
        /* A generator<T> has a synchronous next() ABI.  Await therefore
         * consumes the verified Future payload here before the next yield;
         * it is not an implicit second generator suspension kind. */
        if ( !xl6IrCgenEmitBlockingAwaitInstruction(
                out, module, instruction, diags,
                "XLIR generator blocking await ABI is unresolved") ) return 0;
        break;
    case XL6_IR_INSTR_BRANCH:
        if ( block->trueTarget == SIZE_MAX || block->falseTarget == SIZE_MAX )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR generator branch targets are unresolved");
        xl6OutputPrintf(out, "    if (v%u) goto gp%lu; else goto gp%lu;\n",
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned long)block->trueTarget,
            (unsigned long)block->falseTarget);
        break;
    case XL6_IR_INSTR_ERROR_TEST:
        {
            XL6_IrValueId tested;
            size_t sourceIndex = SIZE_MAX;
            const XL6_IrInstruction* source;
            if ( block->trueTarget == SIZE_MAX ||
                 block->falseTarget == SIZE_MAX ||
                 instruction->operandCount != 1 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR generator error test has no exact carrier or targets");
            tested = module->operands[instruction->firstOperand];
            source = xl6IrCgenValueDefinition(
                module, tested, &sourceIndex);
            if ( source == NULL || sourceIndex == SIZE_MAX )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR generator error test has no source instruction");
            if (source->kind == XL6_IR_INSTR_AWAIT) {
                xl6OutputPrintf(out, "    if (await_ok_%u) goto gp%lu; else goto gp%lu;\n",
                    (unsigned int)tested, (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
            } else if (source->kind == XL6_IR_INSTR_GENERATOR_NEXT) {
                xl6IrCgenEmitGeneratorNextTest(out, sourceIndex, tested, "gp",
                    block->trueTarget, block->falseTarget);
            } else if (source->kind == XL6_IR_INSTR_CLEANUP_END) {
                xl6OutputPrintf(out, "    if (v%u) goto gp%lu; else goto gp%lu;\n",
                    (unsigned int)tested, (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
            } else if (xl6IrCallInstructionErrorAbi(module, source) == XL6_IR_ERROR_ABI_MAY_RAISE &&
                       xl6IrCallInstructionErrorPhysicalAbi(module, source) != XL6_IR_CALL_ERROR_PHYSICAL_INVALID) {
                if (!xl6IrCgenEmitCallErrorTest(out, module, source, sourceIndex, "gp",
                        block->trueTarget, block->falseTarget, diags)) return 0;
            } else if ( source->kind == XL6_IR_INSTR_CLOSURE_CALL ) {
                if ( !xl6IrCgenClosureCallAbi(module, source) )
                    return xl6IrCgenDiag(diags, module,
                        instruction->origin,
                        "XLIR generator error test has no closure-call carrier ABI");
                xl6OutputPrintf(out,
                    "    if (c%lu) goto gp%lu; else goto gp%lu;\n",
                    (unsigned long)sourceIndex,
                    (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
            } else if ( source->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                        source->runtimeOp ==
                            XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE ) {
                xl6OutputPrintf(out,
                    "    if (v%u != NULL) goto gp%lu; else goto gp%lu;\n",
                    (unsigned int)tested,
                    (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
            } else {
                xl6OutputPrintf(out,
                    "    if (xrtGetError() == NULL) goto gp%lu; else goto gp%lu;\n",
                    (unsigned long)block->trueTarget,
                    (unsigned long)block->falseTarget);
            }
        }
        break;
    case XL6_IR_INSTR_JUMP:
        if ( block->trueTarget == SIZE_MAX ) return xl6IrCgenDiag(diags, module,
            instruction->origin, "XLIR generator jump target is unresolved");
        xl6OutputPrintf(out, "    goto gp%lu;\n", (unsigned long)block->trueTarget);
        break;
    case XL6_IR_INSTR_ERROR_RAISE:
    case XL6_IR_INSTR_CLEANUP_BEGIN:
    case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN:
    case XL6_IR_INSTR_CLEANUP_CAPTURE:
    case XL6_IR_INSTR_CLEANUP_END:
    case XL6_IR_INSTR_ERROR_UNWIND_BEGIN:
    case XL6_IR_INSTR_ERROR_UNWIND_END:
    case XL6_IR_INSTR_ERROR_TAKE:
    case XL6_IR_INSTR_ERROR_SET_TAKE:
    case XL6_IR_INSTR_ERROR_CLEAR:
        if (!xl6IrCgenEmitErrorStateInstruction(out, module, instruction))
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR error-state ownership ABI is unresolved");
        break;
    case XL6_IR_INSTR_YIELD:
        {
            const XL6_IrGeneratorSuspension* suspension;
            size_t action;
            if ( block->kind != XL6_IR_GENERATOR_MACHINE_BLOCK_YIELD ||
                 block->suspension >= module->generatorSuspensionCount )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR generator yield has no suspension record");
            suspension = &module->generatorSuspensions[block->suspension];
            for ( action = 0; action < suspension->spillActionCount; ++action ) {
                const XL6_IrContinuationFrameAction* spill =
                    &module->generatorFrameActions[suspension->firstSpillAction + action];
                const XL6_IrContinuationFrameSlot* frame =
                    &module->generatorFrameSlots[spill->frameSlot];
                size_t field = spill->frameSlot - machine->firstFrameSlot;
                int acquire = frame->initBit != SIZE_MAX && frame->transferAbi != XL6_IR_ASYNC_FRAME_TRANSFER_MOVE_OWNED;
                if (acquire) xl6OutputPrintf(out, "    if (!frame->i%lu) {\n", (unsigned long)frame->initBit);
                {
                    char place[64],input[32],initialized[64]; int acquired;
                    const char* type=xl6IrCgenType(module,frame->type);
                    snprintf(place,sizeof(place),"frame->f%lu",(unsigned long)field);
                    snprintf(input,sizeof(input),"v%u",(unsigned int)frame->value);
                    snprintf(initialized,sizeof(initialized),"frame->i%lu",(unsigned long)frame->initBit);
                    if (!xl6IrCgenEmitFrameAcquisitionBegin(out,type,frame->transferAbi,input,&acquired) ||
                        !xl6IrCgenEmitFrameAcquisitionCommit(out,type,place,frame->initBit==SIZE_MAX?NULL:initialized)) return 0;
                }
                if (acquire) xl6OutputPuts("    }\n", out);
            }
            xl6OutputPrintf(out,
                "    if (!xlir_generator_put(generator, &v%u, sizeof(v%u))) {\n"
                "        xlir_generator_finish(generator); return false;\n"
                "    }\n"
                "    xlir_generator_set_state(generator, %uU);\n",
                (unsigned int)suspension->itemValue,
                (unsigned int)suspension->itemValue,
                (unsigned int)(suspension->stateId + 1));
            if (!xl6IrCgenEmitContinuationTransfers(out, module, 1, (size_t)(machine - module->generatorMachines),
                    instruction, XL6_IR_ASYNC_FRAME_ACTION_TRANSFER_ON_YIELD_COMMIT)) return 0;
            xl6OutputPuts("    return true;\n", out);
        }
        break;
    case XL6_IR_INSTR_RETURN:
    case XL6_IR_INSTR_THROW:
        if ( instruction->kind == XL6_IR_INSTR_THROW )
            xl6OutputPuts("    xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()});\n", out);
        xl6OutputPuts("    xlir_generator_finish(generator); return false;\n", out);
        break;
    default:
        return xl6IrCgenDiag(diags, module, instruction->origin,
                             "XLIR generator machine instruction is not implemented");
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitGeneratorMachine(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCallTarget* target, XL6_DiagList* diags)
{
    const XL6_IrGeneratorMachine* machine =
        xl6IrCgenGeneratorMachine(module, target);
    const XL6_IrFunction* function;
    const char* itemKind;
    size_t local;
    if ( machine == NULL || !xl6IrCgenGeneratorMachineProgramSupported(
            module, target, machine) )
        return xl6IrCgenDiag(diags, module, target->declaration,
                             "XLIR C emitter generator machine ABI is not implemented");
    function = &module->functions[machine->function];
    /* Serialize the closed, verified transfer ABI. No producer cleanup or
     * ownership callback is synthesized from a C type or source name. */
        switch (machine->yieldAbi) {
        case XL6_IR_GENERATOR_YIELD_COPY_BOOL:
        case XL6_IR_GENERATOR_YIELD_COPY_CHAR:
        case XL6_IR_GENERATOR_YIELD_COPY_I64:
    case XL6_IR_GENERATOR_YIELD_COPY_U64:
    case XL6_IR_GENERATOR_YIELD_COPY_TYPE_ID:
    case XL6_IR_GENERATOR_YIELD_COPY_F64:
    case XL6_IR_GENERATOR_YIELD_COPY_TIME:
    case XL6_IR_GENERATOR_YIELD_COPY_RAW_POINTER:
    case XL6_IR_GENERATOR_YIELD_COPY_TAGGED: itemKind = "XL6_CAPTURE_TRIVIAL"; break;
    case XL6_IR_GENERATOR_YIELD_MOVE_STRING: itemKind = "XL6_CAPTURE_STRING"; break;
    case XL6_IR_GENERATOR_YIELD_MOVE_XVALUE: itemKind = "XL6_CAPTURE_XVALUE"; break;
    case XL6_IR_GENERATOR_YIELD_MOVE_CLOSURE: itemKind = "XL6_CAPTURE_CLOSURE"; break;
    case XL6_IR_GENERATOR_YIELD_MOVE_FUTURE: itemKind = "XL6_CAPTURE_FUTURE"; break;
    case XL6_IR_GENERATOR_YIELD_MOVE_ERROR: itemKind = "XL6_CAPTURE_ERROR"; break;
    case XL6_IR_GENERATOR_YIELD_MOVE_GENERATOR: itemKind = "XL6_CAPTURE_GENERATOR"; break;
    default: return xl6IrCgenDiag(diags, module, target->declaration,
        "XLIR generator item storage ABI is unresolved");
    }
    xl6OutputPrintf(out, "typedef struct xlir_generator_frame_%u {\n",
                    (unsigned int)target->symbol);
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrContinuationFrameSlot* frame =
            &module->generatorFrameSlots[machine->firstFrameSlot + local];
        xl6OutputPrintf(out, "    %s f%lu;",
            xl6IrCgenType(module, frame->type), (unsigned long)local);
        if ( frame->initBit != SIZE_MAX )
            xl6OutputPrintf(out, " unsigned char i%lu;", (unsigned long)frame->initBit);
        xl6OutputPuts("\n", out);
    }
    if ( machine->frameSlotCount == 0 ) xl6OutputPuts("    unsigned char empty;\n", out);
    xl6OutputPrintf(out, "} xlir_generator_frame_%u;\n",
                    (unsigned int)target->symbol);
    if (!xl6IrCgenEmitActivationLayout(out, "xlir_generator", target->symbol,
            machine->frameSlotCount ? module->generatorFrameSlots + machine->firstFrameSlot : NULL,
            machine->frameSlotCount, machine->firstFrameSlot,
            machine->destroyActionCount ? module->generatorFrameActions + machine->firstDestroyAction : NULL,
            machine->destroyActionCount)) return 0;
    xl6OutputPrintf(out,
        "static const XL6_CaptureSlot xlir_generator_item_slot_%u = {%s, 0, sizeof(%s)};\n"
        "static const XL6_CaptureLayoutV1 xlir_generator_item_layout_%u = {sizeof(XL6_CaptureLayoutV1), sizeof(%s), 1, &xlir_generator_item_slot_%u};\n",
        (unsigned int)target->symbol, itemKind, xl6IrCgenType(module, machine->itemType),
        (unsigned int)target->symbol, xl6IrCgenType(module, machine->itemType), (unsigned int)target->symbol);
    xl6OutputPrintf(out,
        "static bool xlir_generator_resume_%u(xlir_generator* generator)\n{\n"
        "    xlir_generator_frame_%u* frame = (xlir_generator_frame_%u*)generator->frame;\n",
        (unsigned int)target->symbol, (unsigned int)target->symbol,
        (unsigned int)target->symbol);
    xl6IrCgenDeclareUnwindScope(out, module, function);
    for ( local = 0; local < function->valueCount; ++local ) {
        XL6_IrValueId value = (XL6_IrValueId)(function->firstValue + local);
        const XL6_IrInstruction* definition = xl6IrCgenValueDefinition(module, value, NULL);
        if ( definition != NULL &&
             !xl6IrCgenResultStorageOmitted(module, definition) &&
             xl6IrCgenType(module, definition->type) != NULL )
            xl6OutputPrintf(out, "    %s v%u;\n",
                xl6IrCgenType(module, definition->type), (unsigned int)value);
    }
    for ( local = 0; local < module->storageCount; ++local ) {
        const XL6_IrStorage* storage = &module->storages[local];
        const XL6_IrType* type;
        const char* cType;
        if ( xl6IrCgenGeneratorLocalStorage(
                 module, machine, function, storage->symbol) == NULL ) continue;
        type = xl6IrTypeGet(&module->types,
            storage->cellType != XL6_IR_TYPE_INVALID ? storage->cellType : storage->type);
        cType = xl6IrCgenType(module,
            storage->cellType != XL6_IR_TYPE_INVALID ? storage->cellType : storage->type);
        if ( type == NULL || cType == NULL ) return xl6IrCgenDiag(diags, module,
            storage->declaration, "XLIR generator local storage has no C ABI");
        if (type->cAbi == XL6_IR_C_ABI_MANAGED_STRING)
            xl6OutputPrintf(out, "    %s s%u = {0};\n",
                cType, (unsigned int)storage->symbol);
        else
            xl6OutputPrintf(out, type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED ||
                                  storage->cellType != XL6_IR_TYPE_INVALID
                ? "    %s s%u = NULL;\n" : "    %s s%u;\n",
                cType, (unsigned int)storage->symbol);
    }
    xl6OutputPuts("    switch (generator->state) {\n", out);
    for ( local = 0; local < machine->entryCount; ++local ) {
        const XL6_IrGeneratorMachineEntry* entry =
            &module->generatorMachineEntries[machine->firstEntry + local];
        if ( entry->kind == XL6_IR_GENERATOR_MACHINE_ENTRY_START )
            xl6OutputPrintf(out, "    case 0: goto gp%lu;\n",
                            (unsigned long)entry->programBlock);
        else {
            const XL6_IrGeneratorSuspension* suspension =
                &module->generatorSuspensions[entry->suspension];
            size_t action;
            xl6OutputPrintf(out, "    case %u:\n",
                            (unsigned int)(suspension->stateId + 1));
            for ( action = 0; action < suspension->resumeActionCount; ++action ) {
                const XL6_IrContinuationFrameAction* project =
                    &module->generatorFrameActions[suspension->firstResumeAction + action];
                const XL6_IrContinuationFrameSlot* frame =
                    &module->generatorFrameSlots[project->frameSlot];
                xl6OutputPrintf(out, "        v%u = frame->f%lu;\n",
                    (unsigned int)project->value,
                    (unsigned long)(project->frameSlot - machine->firstFrameSlot));
            }
            xl6OutputPrintf(out, "        goto gp%lu;\n",
                            (unsigned long)entry->programBlock);
        }
    }
    xl6OutputPuts("    default: xlir_generator_finish(generator); return false;\n    }\n", out);
    for ( local = 0; local < machine->blockCount; ++local ) {
        const XL6_IrGeneratorMachineBlock* block =
            &module->generatorMachineBlocks[machine->firstBlock + local];
        size_t instructionIndex;
        /* A block may begin with a checked-call result declaration. C11
         * labels attach to statements, including this empty statement. */
        xl6OutputPrintf(out, "gp%lu:;\n",
                        (unsigned long)(machine->firstBlock + local));
        for ( instructionIndex = block->firstInstruction;
              instructionIndex < block->firstInstruction + block->instructionCount;
              ++instructionIndex )
            if ( !xl6IrCgenEmitGeneratorInstruction(out, module, target, machine,
                    function, block, &module->instructions[instructionIndex], diags) ) return 0;
    }
    xl6OutputPuts("    xlir_generator_finish(generator); return false;\n}\n", out);
    xl6OutputPrintf(out, "static xlir_generator* xlir_generator_start_raw_%u(",
                    (unsigned int)target->symbol);
    if ( target->parameterCount == 0 ) xl6OutputPuts("void", out);
    for ( local = 0; local < target->parameterCount; ++local ) {
        if ( local != 0 ) xl6OutputPuts(", ", out);
        xl6OutputPrintf(out, "%s a%lu",
            xl6IrCgenType(module, target->parameterTypes[local]), (unsigned long)local);
    }
    xl6OutputPrintf(out,
        ")\n{\n    xlir_generator_frame_%u* frame = (xlir_generator_frame_%u*)xlir_generator_frame_allocate(sizeof(*frame), &xlir_generator_layout_%u);\n"
        "    xlir_generator* generator;\n"
        "    if (frame == NULL) {\n",
        (unsigned int)target->symbol, (unsigned int)target->symbol, (unsigned int)target->symbol);
    if (!xl6IrCgenEmitActivationInputRollback(out, module, target, diags)) return 0;
    xl6OutputPuts("    return NULL; }\n", out);
    for ( local = 0; local < machine->frameSlotCount; ++local ) {
        const XL6_IrContinuationFrameSlot* frame =
            &module->generatorFrameSlots[machine->firstFrameSlot + local];
        if ( frame->initBit != SIZE_MAX )
            xl6OutputPrintf(out, "    frame->f%lu = %s; frame->i%lu = 0;\n",
                (unsigned long)local,
                xl6IrTypeGet(&module->types, frame->type)->cAbi == XL6_IR_C_ABI_MANAGED_STRING
                    ? "(xlir_string){0}" : "NULL",
                (unsigned long)frame->initBit);
    }
    for ( local = 0; local < machine->prologueActionCount; ++local ) {
        const XL6_IrContinuationFrameAction* action =
            &module->generatorFrameActions[machine->firstPrologueAction + local];
        const XL6_IrContinuationFrameSlot* frame =
            &module->generatorFrameSlots[action->frameSlot];
        size_t field = action->frameSlot - machine->firstFrameSlot;
        if ( action->kind == XL6_IR_ASYNC_FRAME_ACTION_BIND_STORAGE ) continue;
        {
            char place[64], input[32], initialized[64];
            snprintf(place, sizeof(place), "frame->f%lu", (unsigned long)field);
            snprintf(input, sizeof(input), "a%lu", (unsigned long)frame->parameter);
            snprintf(initialized, sizeof(initialized), "frame->i%lu", (unsigned long)frame->initBit);
            if (!xl6IrCgenEmitActivationCapture(out, xl6IrCgenType(module,frame->type), frame->transferAbi, place, input,
                    frame->initBit == SIZE_MAX ? NULL : initialized, "xlir_generator_frame_discard(frame);"))
                return xl6IrCgenDiag(diags, module, target->declaration,
                    "XLIR generator launch capture ABI is unresolved");
        }
    }
    xl6OutputPrintf(out,
        "    generator = xlir_generator_create(frame, xlir_generator_resume_%u, &xlir_generator_item_layout_%u);\n"
        "    if (generator == NULL) xlir_generator_frame_discard(frame);\n"
        "    return generator;\n}\n\n",
        (unsigned int)target->symbol, (unsigned int)target->symbol);
    return !xl6OutputHasFailed(out);
}
