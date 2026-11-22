/* Generated from the exact reviewed native IR declarations.
 * Admit only the complete producer source/header/dependency contract. */
static inline int exitBook_XL6_IrSequenceLayout(xls_exit_pending* p,const XL6_IrSequenceLayout* src){
if(!exitNumber(p,(uint64_t)src->abi))return 0;
if(!exitNumber(p,(uint64_t)src->role))return 0;
if(!exitNumber(p,(uint64_t)src->elementType))return 0;
if(!exitNumber(p,(uint64_t)src->baseElementType))return 0;
if(!exitNumber(p,(uint64_t)src->slotAbi))return 0;
if(!exitNumber(p,(uint64_t)src->codecAbi))return 0;
if(!exitNumber(p,(uint64_t)src->transferAbi))return 0;
if(!exitNumber(p,(uint64_t)src->admissionAbi))return 0;
if(!exitNumber(p,(uint64_t)src->nullable))return 0;
if(!exitNumber(p,(uint64_t)src->defaultAbi))return 0;
if(!exitNumber(p,(uint64_t)src->childRole))return 0;
if(!exitNumber(p,(uint64_t)src->childElementType))return 0;
if(!exitNumber(p,(uint64_t)src->childSlotAbi))return 0;
if(!exitNumber(p,(uint64_t)src->childTransferAbi))return 0;
return 1;}
static inline int exitBook_XL6_IrType(xls_exit_pending* p,const XL6_IrType* src){
if(!exitNumber(p,(uint64_t)src->kind))return 0;
if(!exitNumber(p,(uint64_t)src->cAbi))return 0;
if(!exitNumber(p,(uint64_t)src->collectionAbi))return 0;
if(!exitNumber(p,(uint64_t)src->collectionCarrierAbi))return 0;
if(!exitNumber(p,(uint64_t)src->runtimeResourceAbi))return 0;
if(!exitNumber(p,(uint64_t)src->ownershipAbi))return 0;
if(!exitNumber(p,(uint64_t)src->nullAbi))return 0;
if(!exitNumber(p,(uint64_t)src->persistenceValueAbi))return 0;
if(!exitNumber(p,(uint64_t)src->persistenceRestoreScalarAbi))return 0;
if(!exitNumber(p,(uint64_t)src->persistenceRestoreCarrierAbi))return 0;
if(!exitNumber(p,(uint64_t)src->persistencePayloadType))return 0;
if(!exitNumber(p,(uint64_t)src->persistenceWireFormat))return 0;
if(!exitNumber(p,(uint64_t)src->integerTarget))return 0;
if(!exitNumber(p,(uint64_t)src->nativeProductAbi))return 0;
if(!exitNumber(p,(uint64_t)src->nativeValueAbi))return 0;
if(!exitNumber(p,(uint64_t)src->nativeSlotAbi))return 0;
if(!exitBook_XL6_IrSequenceLayout(p,&src->sequenceLayout))return 0;
if(!exitNumber(p,(uint64_t)src->messageSchemaType))return 0;
if(!exitNumber(p,(uint64_t)src->treeKeySchemaType))return 0;
if(!exitNumber(p,(uint64_t)src->treeValueSchemaType))return 0;
if(!exitNumber(p,(uint64_t)src->collectionValueSchemaType))return 0;
if(!exitNumber(p,(uint64_t)src->setKeyAbi))return 0;
if(!exitNumber(p,(uint64_t)src->dictKeyAbi))return 0;
if(!exitNumber(p,(uint64_t)src->xvalueAdmissionAbi))return 0;
if(!exitNumber(p,(uint64_t)src->xvalueAdmissionNullable))return 0;
if(!exitNumber(p,(uint64_t)src->foreignRecordAbi))return 0;
if(!exitNumber(p,(uint64_t)src->xvalueAdmissionTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->sourceTypeId))return 0;
if(!exitNumber(p,(uint64_t)src->abiId))return 0;
if(!exitNumber(p,(uint64_t)src->typeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->nominalSymbol))return 0;
if(!exitNumber(p,src->name!=NULL))return 0;
if(src->name&&!exitText(p,src->name))return 0;
if(!exitNumber(p,src->displayName!=NULL))return 0;
if(src->displayName&&!exitText(p,src->displayName))return 0;
if(!exitAppend(p,&src->cName,sizeof(src->cName)))return 0;
if(!exitNumber(p,src->foreignHeader!=NULL))return 0;
if(src->foreignHeader&&!exitText(p,src->foreignHeader))return 0;
if(!exitNumber(p,src->foreignRecordSource!=NULL))return 0;
if(src->foreignRecordSource&&!exitText(p,src->foreignRecordSource))return 0;
if(!exitNumber(p,(uint64_t)src->foreignBase))return 0;
if(!exitNumber(p,(uint64_t)src->foreignLifetime))return 0;
if(!exitNumber(p,(uint64_t)src->foreignOwner))return 0;
if(!exitNumber(p,src->foreignPolicy!=NULL))return 0;
if(src->foreignPolicy&&!exitText(p,src->foreignPolicy))return 0;
if(!exitNumber(p,src->foreignScope!=NULL))return 0;
if(src->foreignScope&&!exitText(p,src->foreignScope))return 0;
if(!exitNumber(p,src->foreignWatch!=NULL))return 0;
if(src->foreignWatch&&!exitText(p,src->foreignWatch))return 0;
if(!exitNumber(p,src->foreignDomain!=NULL))return 0;
if(src->foreignDomain&&!exitText(p,src->foreignDomain))return 0;
if(!exitNumber(p,(uint64_t)src->nativeArrayExtent))return 0;
if(!exitAppend(p,&src->errorResultCName,sizeof(src->errorResultCName)))return 0;
if(!exitNumber(p,src->params!=NULL))return 0;
if(src->params&&(!exitVector(p,src->params,src->paramCount,0,sizeof(*src->params))))return 0;
if(!exitNumber(p,src->parameterModes!=NULL))return 0;
if(src->parameterModes&&(!exitVector(p,src->parameterModes,src->paramCount,0,sizeof(*src->parameterModes))))return 0;
if(!exitNumber(p,src->parameterAbis!=NULL))return 0;
if(src->parameterAbis&&(!exitVector(p,src->parameterAbis,src->paramCount,0,sizeof(*src->parameterAbis))))return 0;
if(!exitNumber(p,(uint64_t)src->paramCount))return 0;
if(!exitNumber(p,src->returns!=NULL))return 0;
if(src->returns&&(!exitVector(p,src->returns,src->returnCount,0,sizeof(*src->returns))))return 0;
if(!exitNumber(p,(uint64_t)src->returnCount))return 0;
if(!exitNumber(p,(uint64_t)src->callableResultType))return 0;
if(!exitNumber(p,(uint64_t)src->callableErasureAbi))return 0;
if(!exitNumber(p,(uint64_t)src->hash))return 0;
return 1;}
static inline int exitBook_XL6_IrEnumLayout(xls_exit_pending* p,const XL6_IrEnumLayout* src){
if(!exitNumber(p,(uint64_t)src->type))return 0;
if(!exitNumber(p,(uint64_t)src->declaration))return 0;
if(!exitNumber(p,(uint64_t)src->firstVariant))return 0;
if(!exitNumber(p,(uint64_t)src->variantCount))return 0;
return 1;}
static inline int exitBook_XL6_IrEnumVariant(xls_exit_pending* p,const XL6_IrEnumVariant* src){
if(!exitNumber(p,(uint64_t)src->ownerType))return 0;
if(!exitNumber(p,(uint64_t)src->declaration))return 0;
if(!exitNumber(p,(uint64_t)src->symbol))return 0;
if(!exitNumber(p,(uint64_t)src->tag))return 0;
if(!exitNumber(p,(uint64_t)src->valueExpression))return 0;
if(!exitNumber(p,(uint64_t)src->firstPayload))return 0;
if(!exitNumber(p,(uint64_t)src->payloadCount))return 0;
if(!exitNumber(p,src->name!=NULL))return 0;
if(src->name&&!exitText(p,src->name))return 0;
return 1;}
static inline int exitBook_XL6_IrEnumPayload(xls_exit_pending* p,const XL6_IrEnumPayload* src){
if(!exitNumber(p,(uint64_t)src->ownerType))return 0;
if(!exitNumber(p,(uint64_t)src->type))return 0;
if(!exitNumber(p,(uint64_t)src->declaration))return 0;
if(!exitNumber(p,(uint64_t)src->abi))return 0;
if(!exitNumber(p,src->name!=NULL))return 0;
if(src->name&&!exitText(p,src->name))return 0;
return 1;}
static inline int exitBook_XL6_IrFunction(xls_exit_pending* p,const XL6_IrFunction* src){
if(!exitNumber(p,(uint64_t)src->kind))return 0;
if(!exitNumber(p,(uint64_t)src->reachability))return 0;
if(!exitNumber(p,(uint64_t)src->symbol))return 0;
if(!exitNumber(p,(uint64_t)src->declaration))return 0;
if(!exitNumber(p,src->debugName!=NULL))return 0;
if(src->debugName&&!exitText(p,src->debugName))return 0;
if(!exitNumber(p,(uint64_t)src->returnType))return 0;
if(!exitNumber(p,(uint64_t)src->errorAbi))return 0;
if(!exitNumber(p,(uint64_t)src->entryBlock))return 0;
if(!exitNumber(p,(uint64_t)src->firstBlock))return 0;
if(!exitNumber(p,(uint64_t)src->blockCount))return 0;
if(!exitNumber(p,(uint64_t)src->firstInstruction))return 0;
if(!exitNumber(p,(uint64_t)src->instructionCount))return 0;
if(!exitNumber(p,(uint64_t)src->firstValue))return 0;
if(!exitNumber(p,(uint64_t)src->valueCount))return 0;
if(!exitNumber(p,(uint64_t)src->firstParameter))return 0;
if(!exitNumber(p,(uint64_t)src->parameterCount))return 0;
if(!exitNumber(p,src->parameterCells!=NULL))return 0;
if(src->parameterCells&&(!exitVector(p,src->parameterCells,src->parameterCount,0,sizeof(*src->parameterCells))))return 0;
if(!exitNumber(p,(uint64_t)src->defaultClosureSymbol))return 0;
return 1;}
static inline int exitBook_XL6_IrGenericSpecialization(xls_exit_pending* p,const XL6_IrGenericSpecialization* src){
if(!exitNumber(p,(uint64_t)src->sourceDeclaration))return 0;
if(!exitNumber(p,(uint64_t)src->declaration))return 0;
if(!exitNumber(p,(uint64_t)src->symbol))return 0;
if(!exitNumber(p,src->typeArgs!=NULL))return 0;
if(src->typeArgs&&(!exitVector(p,src->typeArgs,src->typeArgCount,0,sizeof(*src->typeArgs))))return 0;
if(!exitNumber(p,(uint64_t)src->typeArgCount))return 0;
return 1;}
static inline int exitBook_XL6_IrValueInitializer(xls_exit_pending* p,const XL6_IrValueInitializer* src){
if(!exitNumber(p,(uint64_t)src->declaration))return 0;
if(!exitNumber(p,(uint64_t)src->type))return 0;
if(!exitNumber(p,(uint64_t)src->expression))return 0;
if(!exitNumber(p,(uint64_t)src->abi))return 0;
if(!exitNumber(p,(uint64_t)src->scalarConversionAbi))return 0;
if(!exitNumber(p,(uint64_t)src->runtimeOp))return 0;
if(!exitNumber(p,(uint64_t)src->runtimeResourceOp))return 0;
if(!exitNumber(p,(uint64_t)src->payloadType))return 0;
if(!exitNumber(p,(uint64_t)src->runtimeResourcePayloadAbi))return 0;
if(!exitNumber(p,(uint64_t)src->runtimeResourceKeyType))return 0;
if(!exitNumber(p,(uint64_t)src->runtimeResourceKeyPayloadAbi))return 0;
if(!exitNumber(p,(uint64_t)src->runtimeResourceKeyOrderAbi))return 0;
if(!exitNumber(p,(uint64_t)src->callableErasureAbi))return 0;
if(!exitNumber(p,(uint64_t)src->errorAbi))return 0;
return 1;}
static inline int exitBook_XL6_IrRuntimeTypeMap(xls_exit_pending* p,const XL6_IrRuntimeTypeMap* src){
if(!exitNumber(p,(uint64_t)src->abiVersion))return 0;
if(!exitNumber(p,(uint64_t)src->fallbackTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->nullTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->boolTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->i64TypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->f64TypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->stringTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->bytesTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->timeTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->pointerTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->handleTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->arrayTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->intMapTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->setTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->objectTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->u64TypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->charTypeValueId))return 0;
if(!exitNumber(p,(uint64_t)src->typeTypeValueId))return 0;
return 1;}
static inline int exitBook_XL6_IrSpan(xls_exit_pending* p,const XL6_IrSpan* src){
if(!exitNumber(p,(uint64_t)src->line))return 0;
if(!exitNumber(p,(uint64_t)src->column))return 0;
return 1;}
static inline int exitBook_XL6_ForeignCallFacts(xls_exit_pending* p,const XL6_ForeignCallFacts* src){
if(!exitNumber(p,(uint64_t)src->constAggregateAddresses))return 0;
if(!exitNumber(p,(uint64_t)src->staticCallbackAddresses))return 0;
if(!exitNumber(p,(uint64_t)src->textViews))return 0;
if(!exitNumber(p,(uint64_t)src->bytesViews))return 0;
if(!exitNumber(p,(uint64_t)src->terminatedStrings))return 0;
if(!exitNumber(p,(uint64_t)src->physicalPairs))return 0;
if(!exitNumber(p,(uint64_t)src->retainedCallbacks))return 0;
if(!exitNumber(p,(uint64_t)src->controlViews))return 0;
if(!exitNumber(p,(uint64_t)src->borrowedViewResult))return 0;
if(!exitNumber(p,(uint64_t)src->valueOutput))return 0;
if(!exitNumber(p,(uint64_t)src->receiverDereference))return 0;
if(!exitNumber(p,(uint64_t)src->inplaceOutput))return 0;
if(!exitNumber(p,(uint64_t)src->inputMappingFacts))return 0;
if(!exitNumber(p,(uint64_t)src->spanPrefixCount))return 0;
if(!exitNumber(p,(uint64_t)src->dataSnapshotCount))return 0;
if(!exitNumber(p,(uint64_t)src->resultOwnerIndexPlusOne))return 0;
if(!exitNumber(p,(uint64_t)src->outputIndexPlusOne))return 0;
if(!exitNumber(p,(uint64_t)src->successCode))return 0;
if(!exitNumber(p,(uint64_t)src->rollbackSymbol))return 0;
return 1;}
static int exitNodeKey(xls_exit_pending* p,const XL6_IrNode* src){
unsigned char encoded[4+28*10];size_t bytes=4;uint32_t present=0;
unsigned category=!src->text?0:!src->text[0]?1:!strcmp(src->text,"_")?2:!strcmp(src->text,"destroy")?3:4;
if(category)present|=UINT32_C(1)<<27;
if(src->kind){present|=UINT32_C(1)<<0;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->kind);}
if(src->sourceKind){present|=UINT32_C(1)<<1;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->sourceKind);}
if(src->opcode){present|=UINT32_C(1)<<2;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->opcode);}
if(src->operatorErrorAbi){present|=UINT32_C(1)<<3;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->operatorErrorAbi);}
if(src->castErrorAbi){present|=UINT32_C(1)<<4;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->castErrorAbi);}
if(src->implicitConversionErrorAbi){present|=UINT32_C(1)<<5;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->implicitConversionErrorAbi);}
if(src->type){present|=UINT32_C(1)<<6;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->type);}
if(src->storageType){present|=UINT32_C(1)<<7;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->storageType);}
if(src->symbol){present|=UINT32_C(1)<<8;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->symbol);}
if(src->targetSymbol){present|=UINT32_C(1)<<9;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->targetSymbol);}
if(src->compoundOperatorSymbol){present|=UINT32_C(1)<<10;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->compoundOperatorSymbol);}
if(src->callKind){present|=UINT32_C(1)<<11;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->callKind);}
if(src->implicitNameAbi){present|=UINT32_C(1)<<12;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->implicitNameAbi);}
if(src->memberAbi){present|=UINT32_C(1)<<13;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->memberAbi);}
if(src->projectionErrorAbi){present|=UINT32_C(1)<<14;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->projectionErrorAbi);}
if(src->evaluationErrorAbi){present|=UINT32_C(1)<<15;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->evaluationErrorAbi);}
if(src->constructionErrorAbi){present|=UINT32_C(1)<<16;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->constructionErrorAbi);}
if(src->indexWriteErrorAbi){present|=UINT32_C(1)<<17;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->indexWriteErrorAbi);}
if(src->lvalueKind){present|=UINT32_C(1)<<18;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->lvalueKind);}
if(src->exitKind){present|=UINT32_C(1)<<19;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->exitKind);}
if(src->scopeDepth){present|=UINT32_C(1)<<20;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->scopeDepth);}
if(src->targetScopeDepth){present|=UINT32_C(1)<<21;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->targetScopeDepth);}
if(src->region){present|=UINT32_C(1)<<22;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->region);}
if(src->targetRegion){present|=UINT32_C(1)<<23;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->targetRegion);}
if(src->parent){present|=UINT32_C(1)<<24;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->parent);}
if(src->firstChild){present|=UINT32_C(1)<<25;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->firstChild);}
if(src->childCount){present|=UINT32_C(1)<<26;bytes+=xlsIrMemoNumberBytes(encoded+bytes,(uint64_t)src->childCount);}
if(category)bytes+=xlsIrMemoNumberBytes(encoded+bytes,category);
memcpy(encoded,&present,4);return exitAppend(p,encoded,bytes);}
static int exitInputKey(xls_exit_pending* p,const XL6_IrModule* m){
if(!exitAppend(p,&m->root,sizeof(m->root))||!exitNumber(p,m->nodeCount)||!exitNumber(p,m->childCount)||!exitBook_XL6_IrRuntimeTypeMap(p,&m->runtimeTypeMap))return 0;
if(m->childCount&&!exitVector(p,m->children,m->childCount,0,sizeof(*m->children)))return 0;
for(size_t i=0;i<m->nodeCount;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!exitNodeKey(p,&m->nodes[i]))return 0;}
if(m->types.count>m->types.capacity||(m->types.count&&!m->types.items)||!exitNumber(p,m->types.count))return 0;
for(size_t i=0;i<m->types.count;++i)if(!exitBook_XL6_IrType(p,&m->types.items[i]))return 0;
if(m->enumLayoutCount>m->enumLayoutCapacity||(m->enumLayoutCount&&!m->enumLayouts)||!exitNumber(p,m->enumLayoutCount))return 0;
for(size_t i=0;i<m->enumLayoutCount;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!exitBook_XL6_IrEnumLayout(p,&m->enumLayouts[i]))return 0;}
if(m->enumVariantCount>m->enumVariantCapacity||(m->enumVariantCount&&!m->enumVariants)||!exitNumber(p,m->enumVariantCount))return 0;
for(size_t i=0;i<m->enumVariantCount;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!exitBook_XL6_IrEnumVariant(p,&m->enumVariants[i]))return 0;}
if(m->enumPayloadCount>m->enumPayloadCapacity||(m->enumPayloadCount&&!m->enumPayloads)||!exitNumber(p,m->enumPayloadCount))return 0;
for(size_t i=0;i<m->enumPayloadCount;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!exitBook_XL6_IrEnumPayload(p,&m->enumPayloads[i]))return 0;}
if(m->functionCount>m->functionCapacity||(m->functionCount&&!m->functions)||!exitNumber(p,m->functionCount))return 0;
for(size_t i=0;i<m->functionCount;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!exitBook_XL6_IrFunction(p,&m->functions[i]))return 0;}
if(m->genericSpecializationCount>m->genericSpecializationCapacity||(m->genericSpecializationCount&&!m->genericSpecializations)||!exitNumber(p,m->genericSpecializationCount))return 0;
for(size_t i=0;i<m->genericSpecializationCount;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!exitBook_XL6_IrGenericSpecialization(p,&m->genericSpecializations[i]))return 0;}
if(m->valueInitializerCount>m->valueInitializerCapacity||(m->valueInitializerCount&&!m->valueInitializers)||!exitNumber(p,m->valueInitializerCount))return 0;
for(size_t i=0;i<m->valueInitializerCount;++i){if(!(i%64)&&xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;if(!exitBook_XL6_IrValueInitializer(p,&m->valueInitializers[i]))return 0;}
return 1;}
