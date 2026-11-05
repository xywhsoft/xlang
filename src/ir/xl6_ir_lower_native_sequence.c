/* One stack read scope for an entire typed traversal; all escaping edges use
 * the ordinary LoopResource LIFO. No snapshot/backing or element allocation. */
static XL6_IrValueId xl6IrLowerNativeSequenceFor(XL6_IrLowerContext* ctx,XL6_IrNodeId id,const XL6_IrNode* loop)
{
 if(loop->childCount!=3 || !loop->region || loop->loopResourceAbi!=XL6_IR_LOOP_RESOURCE_NATIVE_SEQUENCE_READ_END || loop->loopErrorAbi!=XL6_IR_ERROR_ABI_MAY_RAISE)return 0;
 const XL6_IrNode* targets=xl6IrNodeGet(ctx->module,ctx->module->children[loop->firstChild]);
 const XL6_IrNode* source=xl6IrNodeGet(ctx->module,ctx->module->children[loop->firstChild+1]);
 XL6_IrTypeId element=source ? xl6IrNativeSequenceElement(ctx->module,source->storageType):0;
 const XL6_IrType* cursorType=xl6IrTypeGet(&ctx->module->types,loop->loopResourceType);
 bool pair=loop->loopAbi==XL6_IR_LOOP_ABI_NATIVE_SEQUENCE_INDEX_VALUE;
 if(!targets || targets->childCount!=(pair ? 2u:1u) || !element || !cursorType || cursorType->kind!=XL6_IR_TYPE_NATIVE_SEQUENCE_READ || !cursorType->params || cursorType->paramCount!=1 || cursorType->params[0]!=source->storageType)return 0;
 XL6_IrTypeId boolean=xl6IrLowerFindTypeByCAbi(ctx->module,XL6_IR_C_ABI_BOOL);
 if(!boolean)return xl6IrLowerDiag(ctx,id,"NativeSequence traversal requires canonical bool"),0;
 for(size_t n=0;n<targets->childCount;++n){XL6_IrNodeId tid=ctx->module->children[targets->firstChild+n];const XL6_IrNode* t=xl6IrNodeGet(ctx->module,tid);
  if(t->symbol && !xl6IrLowerRegisterStorage(ctx,XL6_IR_STORAGE_LOCAL,t->symbol,t->storageType,tid,ctx->module->functions[ctx->functionIndex].declaration))return 0;
 }
 XL6_SymbolId storage=xl6IrLowerNewHiddenLocalStorage(ctx,loop->loopResourceType,id);if(!storage)return 0;
 size_t evaluation=ctx->evaluationOwnerCount;
 XL6_IrValueId owner=xl6IrLowerNode(ctx,ctx->module->children[loop->firstChild+1]);owner=xl6IrLowerAcquireValueOwner(ctx,id,owner);
 XL6_IrValueId borrowed=xl6IrLowerProtectOwnedBorrow(ctx,id,owner);if(!borrowed)return 0;
 XL6_IrValueId cursor=xl6IrLowerEmitRuntimeCall(ctx,id,loop->loopResourceType,XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN,&borrowed,1);
 if(!cursor || !xl6IrLowerEmitRuntimeErrorEdge(ctx,id,loop,cursor) || !xl6IrLowerCompleteEvaluationBorrows(ctx,id,evaluation,&cursor) ||
  !xl6IrLowerEmit(ctx,XL6_IR_INSTR_STORE,id,loop->loopResourceType,storage,&cursor,1,NULL))return 0;
 XL6_IrBlockId condition,body,exit,after;
 if(!xl6IrLowerNewBlock(ctx,&condition) || !xl6IrLowerNewBlock(ctx,&body) || !xl6IrLowerNewBlock(ctx,&exit) || !xl6IrLowerNewBlock(ctx,&after) ||
  !xl6IrLowerGrow((void**)&ctx->loops,&ctx->loopCapacity,ctx->loopCount,sizeof(*ctx->loops)))return 0;
 /* A source break already executes its sealed LOOP_RESOURCE exit step.
  * Natural exhaustion alone enters the shared resource cleanup block. */
 ctx->loops[ctx->loopCount++]=(XL6_IrLowerLoopTarget){loop->region,after,condition};
 if(!xl6IrLowerPushLoopResource(ctx,loop,id,storage) || !xl6IrLowerEmitJump(ctx,id,condition))return 0;
 xl6IrLowerEnterBlock(ctx,condition);XL6_IrValueId read,more;
 if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_LOAD,id,loop->loopResourceType,storage,NULL,0,&read))return 0;
 more=xl6IrLowerEmitRuntimeCall(ctx,id,boolean,XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE,&read,1);
 if(!more || !xl6IrLowerEmit(ctx,XL6_IR_INSTR_BRANCH,id,XL6_IR_TYPE_INVALID,0,&more,1,NULL))return 0;
 XL6_IrInstruction* branch=&ctx->module->instructions[ctx->module->instructionCount-1];branch->trueTarget=body;branch->falseTarget=exit;
 xl6IrLowerEnterBlock(ctx,body);
 for(size_t n=0;n<targets->childCount;++n){XL6_IrNodeId tid=ctx->module->children[targets->firstChild+n];const XL6_IrNode* target=xl6IrNodeGet(ctx->module,tid);if(!target->symbol)continue;
  XL6_IrValueId value=xl6IrLowerEmitRuntimeCall(ctx,tid,target->storageType,pair && !n ? XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL:XL6_IR_RUNTIME_NATIVE_SEQUENCE_ITEM,&read,1);
  if(!value || !xl6IrLowerInitializeIterationTarget(ctx,id,loop,tid,value,pair && !n ? loop->loopKeyLifetimeAbi:loop->loopItemLifetimeAbi))return 0;
 }
 (void)xl6IrLowerNode(ctx,ctx->module->children[loop->firstChild+2]);
 if(!xl6IrLowerFinishIteration(ctx,id))return 0;
 if(!xl6IrLowerBlockTerminated(ctx) && !xl6IrLowerEmitJump(ctx,id,condition))return 0;
 xl6IrLowerEnterBlock(ctx,exit);
 if(!xl6IrLowerEmitLoopCleanup(ctx,id,1,0) || !xl6IrLowerEmitJump(ctx,id,after) || !xl6IrLowerPopLoopResource(ctx,id,loop->region,storage,XL6_IR_LOWER_LOOP_RESOURCE_HIDDEN))return 0;
 --ctx->loopCount;xl6IrLowerEnterBlock(ctx,after);return 0;
}
static XL6_IrValueId xl6IrLowerNativeSequenceLength(XL6_IrLowerContext* ctx,XL6_IrNodeId id,const XL6_IrNode* node)
{
 if(node->childCount!=1)return 0;
 const XL6_IrNode* source=xl6IrNodeGet(ctx->module,ctx->module->children[node->firstChild]);if(!source || !xl6IrNativeSequenceElement(ctx->module,source->storageType))return 0;
 size_t first=ctx->evaluationOwnerCount;XL6_IrValueId owner=xl6IrLowerNode(ctx,ctx->module->children[node->firstChild]);
 owner=xl6IrLowerAcquireValueOwner(ctx,id,owner);XL6_IrValueId borrow=xl6IrLowerProtectOwnedBorrow(ctx,id,owner);if(!borrow)return 0;
 XL6_IrValueId result=xl6IrLowerEmitRuntimeCall(ctx,id,node->storageType,XL6_IR_RUNTIME_NATIVE_SEQUENCE_COUNT,&borrow,1);
 if(!result || !xl6IrLowerEmitRuntimeErrorEdge(ctx,id,node,result) || !xl6IrLowerCompleteEvaluationBorrows(ctx,id,first,&result))return 0;
 return result;
}
