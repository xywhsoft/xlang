/* Included by xl6_ir_lower.c: typed returned-callable conversion bodies. */
static int xl6IrLowerCallableResultBody(XL6_IrLowerContext* ctx,const XL6_IrClosureTarget* closure)
{
    XL6_IrModule* module=ctx->module;
    const XL6_IrType* signature=xl6IrTypeGet(&module->types,closure->signatureType);
    const XL6_IrClosureCapture capture=module->closureCaptures[closure->firstCapture];
    const XL6_IrType* source=xl6IrTypeGet(&module->types,capture.type);
    if(!signature || !source || !xl6IrBoundMethodResultMatches(module,capture.type,closure->signatureType) ||
       !xl6IrLowerGrow((void**)&module->functions,&module->functionCapacity,module->functionCount,sizeof(*module->functions)) ||
       !xl6IrLowerGrow((void**)&module->blocks,&module->blockCapacity,module->blockCount,sizeof(*module->blocks)))return 0;
    XL6_IrFunction* function=&module->functions[module->functionCount];memset(function,0,sizeof(*function));
    function->kind=XL6_IR_FUNCTION_NAMED_ADAPTER;function->symbol=closure->targetSymbol;
    function->declaration=closure->declaration;function->debugName="<callable-result>";
    function->returnType=signature->callableResultType;function->errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE;
    function->firstBlock=module->blockCount;function->firstInstruction=module->instructionCount;
    function->firstValue=ctx->nextValue;function->firstParameter=module->functionParameterCount;
    for(size_t p=0;p<signature->paramCount;++p) {
        XL6_SymbolId symbol=xl6IrLowerNewSyntheticSymbol(ctx);
        if(!symbol || !xl6IrLowerGrow((void**)&module->functionParameters,&module->functionParameterCapacity,
            module->functionParameterCount,sizeof(*module->functionParameters)) ||
           !xl6IrLowerRegisterStorage(ctx,XL6_IR_STORAGE_PARAMETER,symbol,signature->params[p],
               closure->declaration,closure->declaration))return 0;
        module->functionParameters[module->functionParameterCount++]=symbol;++function->parameterCount;
    }
    if(!xl6IrLowerRegisterStorage(ctx,XL6_IR_STORAGE_CAPTURE,capture.storageSymbol,capture.type,
        closure->declaration,closure->declaration))return 0;
    XL6_IrBlock* block=&module->blocks[module->blockCount++];memset(block,0,sizeof(*block));
    block->firstInstruction=module->instructionCount;
    function->entryBlock=(XL6_IrBlockId)module->blockCount;function->blockCount=1;
    ctx->block=function->entryBlock;ctx->functionIndex=module->functionCount++;
    ctx->promotedParameterCount=0;ctx->evaluationOwnerCount=0;
    if(!xl6IrLowerAddFunctionFailurePlan(ctx,closure->declaration))return 0;
    XL6_IrValueId* args=xrtCalloc(signature->paramCount+1,sizeof(*args));
    XL6_IrValueId result=0;int ok=0;
    if(!args || !xl6IrLowerEmit(ctx,XL6_IR_INSTR_LOAD,closure->declaration,capture.type,
        capture.storageSymbol,NULL,0,&args[0]))goto done;
    for(size_t p=0;p<signature->paramCount;++p) {
        XL6_SymbolId symbol=module->functionParameters[function->firstParameter+p];
        XL6_IrParameterAbi abi=signature->parameterAbis[p];
        if(abi==XL6_IR_PARAMETER_ABI_INOUT || abi==XL6_IR_PARAMETER_ABI_OUT) {
            args[p+1]=xl6IrLowerEmitStorageSymbolAddress(ctx,closure->declaration,closure->declaration,symbol,signature->params[p]);
            if(!args[p+1])goto done;
        } else {
            if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_LOAD,closure->declaration,signature->params[p],symbol,NULL,0,&args[p+1]))goto done;
            if(abi==XL6_IR_PARAMETER_ABI_OWNED &&
               !xl6IrLowerProtectEvaluationStorage(ctx,closure->declaration,symbol,args[p+1],1,0))goto done;
        }
    }
    if(!xl6IrLowerCommitEvaluationOwners(ctx,closure->declaration,0))goto done;
    if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_CLOSURE_CALL,closure->declaration,source->callableResultType,
        0,args,signature->paramCount+1,&result))goto done;
    module->instructions[module->instructionCount-1].calleeType=capture.type;
    XL6_IrLowerCallCleanup cleanup={0};
    if(!xl6IrLowerCompleteCall(ctx,closure->declaration,xl6IrNodeGet(module,closure->declaration),
        NULL,&result,NULL,&cleanup))goto done;
    result=xl6IrLowerBoundMethodResult(ctx,closure->declaration,result,signature->callableResultType);
    XL6_SymbolId pending=xl6IrLowerPrepareReturnValue(ctx,closure->declaration,signature->callableResultType,result);
    if(!result || ctx->failed || !xl6IrLowerEmitTerminal(ctx,XL6_IR_INSTR_RETURN,closure->declaration,&result,1,pending))goto done;
    ok=1;
done:
    xrtFree(args);ctx->evaluationOwnerCount=0;xl6IrLowerEndFunction(ctx);return ok;
}
