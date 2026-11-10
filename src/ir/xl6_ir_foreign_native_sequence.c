/* A sealed, independently owned immutable Native POD span output. */
static int xl6IrNativeArrayGuardDominates(const XL6_IrModule*,const XL6_IrInstruction*,const XL6_IrInstruction*,const XL6_IrBlock*);
static int xl6IrForeignOwnershipHelperMatches(const XL6_IrModule*,XL6_SymbolId,XL6_IrTypeId,int);
int xl6IrNativeSequencePod(const XL6_IrModule* m,XL6_IrTypeId id,size_t depth)
{
 const XL6_IrType* t=xl6IrTypeGet(&m->types,id);if(!t || depth>16 || t->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL)return 0;
 if(t->kind==XL6_IR_TYPE_BUILTIN && !t->paramCount && t->cAbi>=XL6_IR_C_ABI_C_CHAR && t->cAbi<=XL6_IR_C_ABI_C_WCHAR)return 1;
 if(t->kind!=XL6_IR_TYPE_C_RECORD || t->cAbi!=XL6_IR_C_ABI_C_RECORD || !t->foreignHeader || !t->cName[0])return 0;
 const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(m,id);
 if(!layout || layout->firstField>m->aggregateFieldCount || layout->fieldCount>m->aggregateFieldCount-layout->firstField || (layout->fieldCount && !m->aggregateFields))return 0;
 for(size_t f=0;f<layout->fieldCount;++f)if(!xl6IrNativeSequencePod(m,m->aggregateFields[layout->firstField+f].type,depth+1))return 0;
 return 1;
}
XL6_IrTypeId xl6IrNativeSequenceElement(const XL6_IrModule* m,XL6_IrTypeId id)
{
 const XL6_IrType* t=xl6IrTypeGet(&m->types,id);
 return t && t->kind==XL6_IR_TYPE_GENERIC_INSTANCE && t->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_NATIVE_SEQUENCE &&
  t->cAbi==XL6_IR_C_ABI_RUNTIME_XVALUE_PTR && t->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED && t->typeValueId &&
  t->paramCount==1 && t->params && xl6IrNativeSequencePod(m,t->params[0],0) ? t->params[0]:0;
}
const char* xl6IrNativeSequenceStorageCName(const XL6_IrModule* m,XL6_IrTypeId id)
{
 const XL6_IrType* t=xl6IrTypeGet(&m->types,id);if(!t)return NULL;
 if(t->cAbi==XL6_IR_C_ABI_C_RECORD)return t->foreignRecordAbi==XL6_IR_FOREIGN_RECORD_PROJECTED ?
  t->foreignRecordSource:t->foreignRecordAbi==XL6_IR_FOREIGN_RECORD_EXACT ? t->cName:NULL;
 return t->cAbi==XL6_IR_C_ABI_C_SSIZE ? "intptr_t":t->cAbi==XL6_IR_C_ABI_C_WCHAR ? "wchar_t":xl6IrPackScalarCName(t);
}
const XL6_ForeignNativeSequenceContract* xl6IrForeignNativeSequenceGet(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
 if(!m || !t || m->foreignNativeSequenceCount>m->foreignNativeSequenceCapacity ||
  m->foreignNativeSequenceCount>m->nodeCount || (m->foreignNativeSequenceCount && !m->foreignNativeSequences))return NULL;
 const XL6_ForeignNativeSequenceContract* found=NULL;
 for(size_t i=0;i<m->foreignNativeSequenceCount;++i)if(m->foreignNativeSequences[i].declaration==t->declaration){if(found)return NULL;found=&m->foreignNativeSequences[i];}
 return found;
}
static int xl6IrNativeSequenceExpected(const XL6_IrModule* m,const XL6_ForeignNativeSequenceContract* row,XL6_ForeignNativeSequenceContract* result)
{
 const XL6_IrNode* n=xl6IrNodeGet(m,row->declaration);
 if(!n || n->kind!=XL6_IR_NODE_FUNC_DECL || n->childCount<3 || n->firstChild>m->childCount || n->childCount>m->childCount-n->firstChild ||
  xl6ForeignValueOutputBase(n->foreignCall.valueOutput)!=XL6_FOREIGN_VALUE_OUTPUT_NATIVE_SEQUENCE_STATUS ||
  n->foreignCall.outputIndexPlusOne!=row->dataIndex+1 || n->foreignCall.successCode!=row->successCode || row->dataIndex>=row->countIndex || row->countIndex>=64 ||
  (row->countAbi!=XL6_FOREIGN_SPAN_COUNT_INT && row->countAbi!=XL6_FOREIGN_SPAN_COUNT_SIZE) ||
  (row->retainSuccess!=XL6_FOREIGN_RETAIN_ZERO && row->retainSuccess!=XL6_FOREIGN_RETAIN_POSITIVE) ||
  !(n->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (n->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
  n->foreignCall.inplaceOutput || n->foreignCall.borrowedViewResult || n->foreignCall.receiverDereference || n->foreignCall.rollbackSymbol)return 0;
 const XL6_IrNode* returns=xl6IrNodeGet(m,m->children[n->firstChild+2]);
 const XL6_IrNode* params=xl6IrNodeGet(m,m->children[n->firstChild+1]);
 if(!returns || returns->childCount!=1 || returns->firstChild>=m->childCount || !params || params->firstChild>m->childCount || params->childCount>m->childCount-params->firstChild)return 0;
 const XL6_IrNode* value=xl6IrNodeGet(m,m->children[returns->firstChild]);
 XL6_IrTypeId element=value ? xl6IrNativeSequenceElement(m,value->type):0;if(!element)return 0;
 size_t receiver=(n->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || n->sourceKind==XL6_AST_GET) && !(n->flags&XL6_AST_FLAG_STATIC) ? 1:0;
 if(row->ownerParameter>=params->childCount+receiver)return 0;
 const XL6_IrNode* parameter=row->ownerParameter<receiver ? NULL:xl6IrNodeGet(m,m->children[params->firstChild+row->ownerParameter-receiver]);
 XL6_IrTypeId owner=row->ownerParameter<receiver ? n->declarationOwnerType:parameter ? parameter->storageType:0;
 const XL6_IrType* type=xl6IrTypeGet(&m->types,owner);
 if(!type || type->kind!=XL6_IR_TYPE_CLASS || type->foreignLifetime!=XL6_FOREIGN_LIFETIME_OWNED || !type->foreignHeader || !type->foreignPolicy || type->foreignWatch ||
  (m->callTargetCount && (!xl6IrForeignOwnershipHelperMatches(m,row->retainSymbol,owner,1) || !xl6IrForeignOwnershipHelperMatches(m,row->rollbackSymbol,owner,0))))return 0;
 XL6_IrCallTarget target={0};target.declaration=row->declaration;target.foreignHeaderIndexPlusOne=xl6IrForeignFunctionHeaderIndex(m,row->declaration);
 const XL6_IrType* declarationOwner=xl6IrTypeGet(&m->types,n->declarationOwnerType);
 if(!xl6IrForeignFunctionHeaderGet(m,&target) && !(declarationOwner && declarationOwner->foreignHeader))return 0;
 const char* cname=xl6IrNativeSequenceStorageCName(m,element);
 if(!cname || strlen(cname)+8>=sizeof(result->dataCName))return 0;
 *result=*row;result->sequenceType=value->type;result->elementType=element;result->ownerType=owner;
 snprintf(result->dataCName,sizeof(result->dataCName),"const %s*",cname);return 1;
}
static int xl6IrCloseNativeSequences(XL6_IrModule* m)
{
 for(size_t i=0;i<m->foreignNativeSequenceCount;++i){XL6_ForeignNativeSequenceContract row;
  if(!xl6IrNativeSequenceExpected(m,&m->foreignNativeSequences[i],&row))return 0;
  m->foreignNativeSequences[i]=row;}
 return 1;
}
static int xl6IrVerifyNativeSequences(const XL6_IrModule* m,XL6_DiagList* diags)
{
 if(m->foreignNativeSequenceCount>m->foreignNativeSequenceCapacity || m->foreignNativeSequenceCount>m->nodeCount || (m->foreignNativeSequenceCount && !m->foreignNativeSequences))goto invalid;
 for(size_t n=0;n<m->nodeCount;++n){XL6_IrCallTarget t={0};t.declaration=(XL6_IrNodeId)n+1;
  const XL6_ForeignNativeSequenceContract* row=xl6IrForeignNativeSequenceGet(m,&t);
  if((row!=NULL)!=(xl6ForeignValueOutputBase(m->nodes[n].foreignCall.valueOutput)==XL6_FOREIGN_VALUE_OUTPUT_NATIVE_SEQUENCE_STATUS))goto invalid;
  if(row){XL6_ForeignNativeSequenceContract expected;if(!xl6IrNativeSequenceExpected(m,row,&expected) ||
   row->sequenceType!=expected.sequenceType || row->elementType!=expected.elementType || row->ownerType!=expected.ownerType ||
   !memchr(row->dataCName,0,sizeof(row->dataCName)) || strcmp(row->dataCName,expected.dataCName))goto invalid;}
 }
 for(size_t i=0;i<m->types.count;++i){const XL6_IrType* type=&m->types.items[i];
  if(type->kind==XL6_IR_TYPE_C_RECORD){
   if(type->foreignRecordAbi==XL6_IR_FOREIGN_RECORD_PROJECTED){
    if(!type->foreignRecordSource || !type->foreignHeader || !xl6ForeignIdentifier(type->foreignRecordSource) ||
     !xl6IrNativeSequencePod(m,(XL6_IrTypeId)i+1,0))goto invalid;
   }else if(type->foreignRecordAbi!=XL6_IR_FOREIGN_RECORD_EXACT || type->foreignRecordSource)goto invalid;
  }else if(type->foreignRecordAbi!=XL6_IR_FOREIGN_RECORD_NONE || type->foreignRecordSource)goto invalid;
  if(type->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_NATIVE_SEQUENCE &&
   type->kind!=XL6_IR_TYPE_BUILTIN && !xl6IrNativeSequenceElement(m,(XL6_IrTypeId)i+1))goto invalid;
  if(type->kind==XL6_IR_TYPE_GENERIC_INSTANCE && (type->collectionAbi!=XL6_IR_COLLECTION_NONE || type->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
   (type->name && !strcmp(type->name,"generator")) ||
   (type->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_NONE && type->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_NATIVE_SEQUENCE))){
   if(type->paramCount && !type->params)goto invalid;
   for(size_t n=0;n<type->paramCount;++n){const XL6_IrType* item=xl6IrTypeGet(&m->types,type->params[n]);
    if(item && item->kind==XL6_IR_TYPE_NULLABLE && item->paramCount==1 && item->params)item=xl6IrTypeGet(&m->types,item->params[0]);
    if(item && item->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_NATIVE_SEQUENCE)goto invalid;
   }
  }
 }
 return 1;
invalid:return xl6IrDiag(diags,NULL,"NativeSequence requires closed immutable POD backing, exact independent owner and explicit typed count/Retain contract");
}
int xl6IrForeignNativeSequenceMatches(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
 const XL6_ForeignNativeSequenceContract* row=xl6IrForeignNativeSequenceGet(m,t);XL6_ForeignNativeSequenceContract expected;
 if(!row || !xl6IrNativeSequenceExpected(m,row,&expected) || t->returnType!=row->sequenceType || row->ownerParameter>=t->parameterCount ||
  !t->parameterTypes || t->parameterTypes[row->ownerParameter]!=row->ownerType || !t->parameterAbis || t->parameterAbis[row->ownerParameter]!=XL6_IR_PARAMETER_ABI_BORROW ||
  t->kind!=XL6_IR_CALL_TARGET_EXTERN_C || t->runtimeBridge || t->variadicAbi || t->nativeServicesRequired || t->nativeContextAbi || t->nativeResultAbi || t->nativeArrayResultAbi ||
  t->outputResultAbi!=XL6_IR_CALL_OUTPUT_RESULT_INT_STATUS || t->resultConversionAbi!=XL6_IR_CALL_RESULT_CONVERSION_NATIVE_OUTPUT_VALUE)return 0;
 size_t steps=xl6IrForeignPhysicalStepCount(m,t);if(steps>t->parameterCount+2)return 0;
 for(size_t i=0;i<steps;++i)if(!xl6IrForeignPhysicalStepGet(m,t,i,NULL))return 0;
 return 1;
}

int xl6IrNativeSequenceResourceSignature(const XL6_IrModule* m,XL6_IrRuntimeResourceOp op,
 XL6_IrTypeId receiver,XL6_IrTypeId result,const XL6_IrTypeId* inputs,size_t count)
{
 if(op==XL6_IR_RUNTIME_RESOURCE_OP_NATIVE_SEQUENCE_CREATE)return !receiver && !count && xl6IrNativeSequenceElement(m,result);
 if(!inputs || !count || inputs[0]!=receiver || !xl6IrNativeSequenceElement(m,receiver))return 0;
 if(op==XL6_IR_RUNTIME_RESOURCE_OP_NATIVE_SEQUENCE_COUNT){const XL6_IrType* r=xl6IrTypeGet(&m->types,result);return count==1 && r && r->cAbi==XL6_IR_C_ABI_I64;}
 if(op!=XL6_IR_RUNTIME_RESOURCE_OP_NATIVE_SEQUENCE_SLICE || result!=receiver || count<2 || count>3)return 0;
 for(size_t i=1;i<count;++i){const XL6_IrType* t=xl6IrTypeGet(&m->types,inputs[i]);if(!t || t->cAbi!=XL6_IR_C_ABI_I64)return 0;}
 return 1;
}
int xl6IrNativeSequenceInstructionCompatible(const XL6_IrModule* m,const XL6_IrInstruction* i)
{
 if(!m || !i || i->kind!=XL6_IR_INSTR_RUNTIME_CALL || i->runtimeOp<XL6_IR_RUNTIME_NATIVE_SEQUENCE_CREATE || i->runtimeOp>XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL ||
  !i->result || i->operandCount>3 || i->firstOperand>m->operandCount || i->operandCount>m->operandCount-i->firstOperand ||
  i->payloadType || i->runtimeIdentityType || i->targetSymbol || i->targetValue || i->calleeType || i->immediateText || i->opcode ||
  i->runtimeResourcePayloadAbi || i->runtimeResourceKeyType || i->runtimeResourceKeyPayloadAbi || i->runtimeResourceKeyOrderAbi)return 0;
 XL6_IrTypeId inputs[3]={0};for(size_t n=0;n<i->operandCount;++n){const XL6_IrInstruction* def=xl6IrValueDefinition(m,m->operands[i->firstOperand+n],NULL);if(!def)return 0;inputs[n]=def->type;}
 if(i->runtimeOp<=XL6_IR_RUNTIME_NATIVE_SEQUENCE_SLICE)
  return xl6IrNativeSequenceResourceSignature(m,(XL6_IrRuntimeResourceOp)(XL6_IR_RUNTIME_RESOURCE_OP_NATIVE_SEQUENCE_CREATE+i->runtimeOp-XL6_IR_RUNTIME_NATIVE_SEQUENCE_CREATE),i->operandCount ? inputs[0]:0,i->type,inputs,i->operandCount);
 if(i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_INDEX){const XL6_IrType* index=xl6IrTypeGet(&m->types,inputs[1]);return i->operandCount==2 && index && index->cAbi==XL6_IR_C_ABI_I64 && i->type==xl6IrNativeSequenceElement(m,inputs[0]);}
 const XL6_IrType* cursor=i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN ? xl6IrTypeGet(&m->types,i->type):xl6IrTypeGet(&m->types,inputs[0]);
 if(i->operandCount!=1 || !cursor || cursor->kind!=XL6_IR_TYPE_NATIVE_SEQUENCE_READ || cursor->cAbi!=XL6_IR_C_ABI_NATIVE_SEQUENCE_READ_PTR || cursor->paramCount!=1 || !cursor->params)return 0;
 XL6_IrTypeId element=xl6IrNativeSequenceElement(m,cursor->params[0]);if(!element)return 0;
 if(i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN)return inputs[0]==cursor->params[0];
 if(i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ITEM || i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL){
  const XL6_IrBlock* block=NULL;size_t at=(size_t)(i-m->instructions);
  for(size_t b=0;b<m->blockCount;++b)if(at>=m->blocks[b].firstInstruction && at-m->blocks[b].firstInstruction<m->blocks[b].instructionCount){block=&m->blocks[b];break;}
  if(!block)return 0;
  bool guarded=false;for(size_t n=0;n<m->instructionCount;++n){const XL6_IrInstruction* proof=&m->instructions[n];
   if(proof->kind==XL6_IR_INSTR_RUNTIME_CALL && proof->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE && proof->operandCount==1 && proof->firstOperand<m->operandCount &&
    m->operands[proof->firstOperand]==m->operands[i->firstOperand] && xl6IrNativeArrayGuardDominates(m,proof,i,block)){guarded=true;break;}}
  if(!guarded)return 0;
 }
 const XL6_IrType* result=xl6IrTypeGet(&m->types,i->type);if(!result)return 0;
 return i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ITEM ? i->type==element:
  i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE ? result->cAbi==XL6_IR_C_ABI_BOOL:
  i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL ? result->cAbi==XL6_IR_C_ABI_I64:0;
}

XL6_IrTypeId xl6IrTypeInternNativeSequenceRead(XL6_IrModule* m,XL6_IrTypeId sequence)
{
 if(!xl6IrNativeSequenceElement(m,sequence))return 0;
 for(size_t n=0;n<m->types.count;++n){const XL6_IrType* t=&m->types.items[n];if(t->kind==XL6_IR_TYPE_NATIVE_SEQUENCE_READ && t->paramCount==1 && t->params && t->params[0]==sequence)return (XL6_IrTypeId)n+1;}
 XL6_IrTypeId* params=xrtCalloc(1,sizeof(*params));if(!params)return 0;params[0]=sequence;
 if(!xl6IrGrow((void**)&m->types.items,&m->types.capacity,m->types.count,sizeof(*m->types.items))){xrtFree(params);return 0;}
 XL6_IrType* t=&m->types.items[m->types.count];memset(t,0,sizeof(*t));t->kind=XL6_IR_TYPE_NATIVE_SEQUENCE_READ;t->cAbi=XL6_IR_C_ABI_NATIVE_SEQUENCE_READ_PTR;
 t->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;t->nullAbi=XL6_IR_NULL_ABI_POINTER_SENTINEL;t->name=xrtStrDup("native-sequence-read");t->params=params;t->paramCount=1;
 if(!t->name || !xl6IrTypeFinalizeDisplayName(&m->types,t)){xrtFree(t->name);xrtFree(t->displayName);xrtFree(params);memset(t,0,sizeof(*t));return 0;}
 return (XL6_IrTypeId)++m->types.count;
}

/* Compiler-only cursor lifetime follows its actual unique owner through
 * temporary result preservation, STORAGE_TAKE and the hidden LoopResource.
 * One exact owner location reaches each join; no TLS read scope may suspend. */
static int xl6IrVerifyNativeSequenceReads(const XL6_IrModule* m,XL6_DiagList* diags)
{
 if(m->stage!=XL6_IR_STAGE_LOWERED)return 1;
 for(size_t r=0;r<m->storageCount;++r){const XL6_IrStorage* storage=&m->storages[r];const XL6_IrType* type=xl6IrTypeGet(&m->types,storage->type);
  if(!type || type->kind!=XL6_IR_TYPE_NATIVE_SEQUENCE_READ)continue;
  const XL6_IrNode* origin=xl6IrNodeGet(m,storage->declaration);
  if((storage->kind!=XL6_IR_STORAGE_LOCAL && storage->kind!=XL6_IR_STORAGE_TEMPORARY) || storage->cellType || !origin || origin->sourceKind!=XL6_AST_FOR || origin->loopResourceType!=storage->type || origin->loopResourceAbi!=XL6_IR_LOOP_RESOURCE_NATIVE_SEQUENCE_READ_END)goto invalid;
  bool prior=false;for(size_t q=0;q<r;++q)if(m->storages[q].declaration==storage->declaration && m->storages[q].type==storage->type){prior=true;break;}if(prior)continue;
  const XL6_IrFunction* function=NULL;for(size_t f=0;f<m->functionCount;++f)if(m->functions[f].declaration==storage->owner){function=&m->functions[f];break;}
  if(!function || function->firstBlock>m->blockCount || function->blockCount>m->blockCount-function->firstBlock || !function->entryBlock || function->entryBlock>m->blockCount)goto invalid;
  uint64* incoming=xrtMalloc((m->blockCount ? m->blockCount:1)*sizeof(*incoming));size_t* queue=xrtMalloc((m->blockCount ? m->blockCount:1)*sizeof(*queue));
  if(!incoming || !queue){xrtFree(incoming);xrtFree(queue);return xl6IrDiag(diags,NULL,"cannot verify NativeSequence read lifetime");}
  for(size_t n=0;n<m->blockCount;++n)incoming[n]=UINT64_MAX;
  size_t head=0,tail=0;incoming[function->entryBlock-1]=0;queue[tail++]=function->entryBlock-1;bool ok=true;
  while(head<tail && ok){size_t blockId=queue[head++];uint64 owner=incoming[blockId];
   if(blockId<function->firstBlock || blockId>=function->firstBlock+function->blockCount){ok=false;break;}
   const XL6_IrBlock* b=&m->blocks[blockId];if(b->firstInstruction>m->instructionCount || b->instructionCount>m->instructionCount-b->firstInstruction || !b->instructionCount){ok=false;break;}
   for(size_t n=0;n<b->instructionCount;++n){const XL6_IrInstruction* i=&m->instructions[b->firstInstruction+n];
    if(owner && (i->kind==XL6_IR_INSTR_AWAIT || i->kind==XL6_IR_INSTR_GENERATOR_PREPARE_YIELD)){ok=false;break;}
    if(i->kind==XL6_IR_INSTR_RUNTIME_CALL && i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN && i->origin==storage->declaration){if(owner){ok=false;break;}owner=(uint64)i->result<<1;}
    const XL6_IrStorage* slot=i->targetSymbol ? xl6IrStorageGet(m,i->targetSymbol):NULL;
    bool related=slot && slot->type==storage->type && slot->declaration==storage->declaration;
    if(related){
     if(i->type!=storage->type || (i->kind!=XL6_IR_INSTR_STORE && i->kind!=XL6_IR_INSTR_LOAD && i->kind!=XL6_IR_INSTR_RELEASE && i->kind!=XL6_IR_INSTR_STORAGE_TAKE)){ok=false;break;}
     uint64 key=((uint64)slot->symbol<<1)|1;
     if(i->kind==XL6_IR_INSTR_STORE){if(i->operandCount!=1 || i->firstOperand>=m->operandCount || owner!=((uint64)m->operands[i->firstOperand]<<1)){ok=false;break;}owner=key;}
     else if(i->kind==XL6_IR_INSTR_STORAGE_TAKE){if(owner!=key || !i->result){ok=false;break;}owner=(uint64)i->result<<1;}
     else if(i->kind==XL6_IR_INSTR_RELEASE){if(owner==key)owner=0;}
     else if(owner!=key){ok=false;break;}
    }
    if(i->kind==XL6_IR_INSTR_RUNTIME_CALL && i->runtimeOp>=XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE && i->runtimeOp<=XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL && i->operandCount==1 && i->firstOperand<m->operandCount){
     const XL6_IrInstruction* read=xl6IrValueDefinition(m,m->operands[i->firstOperand],NULL);const XL6_IrStorage* borrowed=read && read->kind==XL6_IR_INSTR_LOAD ? xl6IrStorageGet(m,read->targetSymbol):NULL;
     if(borrowed && borrowed->declaration==storage->declaration && borrowed->type==storage->type && owner!=(((uint64)borrowed->symbol<<1)|1)){ok=false;break;}
     if(read && read->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN && read->origin==storage->declaration && owner!=((uint64)read->result<<1)){ok=false;break;}
    }
    if(i->kind==XL6_IR_INSTR_VALUE_RELEASE && i->operandCount==1 && i->firstOperand<m->operandCount && owner==((uint64)m->operands[i->firstOperand]<<1))owner=0;
   }
   if(!ok)break;
   const XL6_IrInstruction* last=&m->instructions[b->firstInstruction+b->instructionCount-1];XL6_IrBlockId edges[2]={last->trueTarget,last->falseTarget};
   if(owner && !edges[0] && !edges[1]){ok=false;break;}
   for(size_t e=0;e<2;++e){if(!edges[e])continue;if(edges[e]>m->blockCount){ok=false;break;}uint64 nextOwner=owner;
    if(e==1 && last->kind==XL6_IR_INSTR_ERROR_TEST && last->operandCount==1 && last->firstOperand<m->operandCount){const XL6_IrInstruction* def=xl6IrValueDefinition(m,m->operands[last->firstOperand],NULL);
     if(def && def->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN && def->origin==storage->declaration && owner==((uint64)def->result<<1))nextOwner=0;}
    size_t next=(size_t)edges[e]-1;if(incoming[next]==UINT64_MAX){incoming[next]=nextOwner;queue[tail++]=next;}else if(incoming[next]!=nextOwner){ok=false;break;}
   }
  }
  xrtFree(incoming);xrtFree(queue);if(!ok)goto invalid;
 }
 for(size_t n=0;n<m->instructionCount;++n){const XL6_IrInstruction* i=&m->instructions[n];const XL6_IrType* t=xl6IrTypeGet(&m->types,i->type);
  if(t && t->kind==XL6_IR_TYPE_NATIVE_SEQUENCE_READ && i->kind!=XL6_IR_INSTR_LOAD && i->kind!=XL6_IR_INSTR_STORE && i->kind!=XL6_IR_INSTR_RELEASE && i->kind!=XL6_IR_INSTR_STORAGE_TAKE && i->kind!=XL6_IR_INSTR_VALUE_RELEASE &&
   !(i->kind==XL6_IR_INSTR_RUNTIME_CALL && i->runtimeOp==XL6_IR_RUNTIME_NATIVE_SEQUENCE_READ_BEGIN))goto invalid;
  for(size_t p=0;p<i->operandCount;++p){if(i->firstOperand>m->operandCount || i->operandCount>m->operandCount-i->firstOperand)goto invalid;
   const XL6_IrInstruction* d=xl6IrValueDefinition(m,m->operands[i->firstOperand+p],NULL);const XL6_IrType* dt=d ? xl6IrTypeGet(&m->types,d->type):NULL;
   if(dt && dt->kind==XL6_IR_TYPE_NATIVE_SEQUENCE_READ && i->kind!=XL6_IR_INSTR_STORE && i->kind!=XL6_IR_INSTR_VALUE_RELEASE && i->kind!=XL6_IR_INSTR_ERROR_TEST &&
    !(i->kind==XL6_IR_INSTR_RUNTIME_CALL && i->runtimeOp>=XL6_IR_RUNTIME_NATIVE_SEQUENCE_ADVANCE && i->runtimeOp<=XL6_IR_RUNTIME_NATIVE_SEQUENCE_ORDINAL))goto invalid;
  }
 }
 return 1;
invalid:return xl6IrDiag(diags,NULL,"NativeSequence read cursor cannot escape, copy, suspend or outlive its exact lexical loop cleanup");
}
