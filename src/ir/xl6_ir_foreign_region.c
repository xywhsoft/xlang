/* Fixed physical data pointers and their typed extent/observer contracts. */
static const XL6_IrNode* xl6IrRegionSignature(const XL6_IrModule* module,uint32 declaration,
    const XL6_IrNode** parameters,size_t* receiver,uint32* outputs,size_t* outputCount)
{
    const XL6_IrNode* node=xl6IrNodeGet(module,declaration);
    if(!node || node->kind!=XL6_IR_NODE_FUNC_DECL ||
        (node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC) ||
        node->firstChild>module->childCount || node->childCount>module->childCount-node->firstChild || node->childCount<3 || !module->children ||
        !xl6ForeignInputMappingsValid(node->foreignCall.inputMappingFacts) || !xl6ForeignValueOutputBitsValid(node->foreignCall.valueOutput))return NULL;
    *parameters=xl6IrNodeGet(module,module->children[node->firstChild+1]);
    *receiver=node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    if(!*parameters || (*parameters)->kind!=XL6_IR_NODE_LIST || (*parameters)->childCount+*receiver>64 ||
        (*parameters)->firstChild>module->childCount || (*parameters)->childCount>module->childCount-(*parameters)->firstChild)return NULL;
    *outputCount=0;
    if(node->foreignCall.outputIndexPlusOne){
        outputs[0]=node->foreignCall.outputIndexPlusOne-1;*outputCount=1;
        XL6_ForeignValueOutputKind mode=xl6ForeignValueOutputBase(node->foreignCall.valueOutput);
        if(mode==XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS || mode==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID){
            if(module->foreignOutputCount>module->foreignOutputCapacity || (module->foreignOutputCount && !module->foreignOutputs))return NULL;
            *outputCount=0;
            for(size_t i=0;i<module->foreignOutputCount;++i){
                const XL6_ForeignOutputContract* row=&module->foreignOutputs[i];if(row->declaration!=declaration)continue;
                if(row->ordinal>=64)return NULL;
                outputs[row->ordinal]=row->physicalIndex;++*outputCount;
            }
            if(*outputCount<2 || *outputCount>64)return NULL;
        }
    }
    return node;
}
static XL6_IrTypeId xl6IrRegionInputType(const XL6_IrModule* module,const XL6_IrNode* function,
    const XL6_IrNode* parameters,size_t receiver,size_t parameter)
{
    if(parameter<receiver)return function->declarationOwnerType;
    parameter-=receiver;
    if(parameter>=parameters->childCount)return 0;
    const XL6_IrNode* node=xl6IrNodeGet(module,module->children[parameters->firstChild+parameter]);
    return node ? node->storageType ? node->storageType:node->type:0;
}
static int xl6IrRegionInteger(const XL6_IrType* type)
{
    return type && type->kind==XL6_IR_TYPE_BUILTIN && !type->paramCount &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL && type->cAbi!=XL6_IR_C_ABI_CHAR_U32 &&
        xl6IrBuildCAbiIsIntegerScalar(type->cAbi);
}
static XL6_IrTypeId xl6IrRegionStorageType(const XL6_IrModule* module,XL6_IrTypeId typeId)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,typeId);
    if(!type)return 0;
    if(type->kind!=XL6_IR_TYPE_NULLABLE)return typeId;
    return type->paramCount==1 && type->params ? type->params[0]:0;
}
static const XL6_ForeignRegionObserverContract* xl6IrRegionObserverAt(const XL6_IrModule* module,uint32 declaration,uint32 physical)
{
    if(module->foreignRegionObserverCount>module->foreignRegionObserverCapacity ||
        (module->foreignRegionObserverCount && !module->foreignRegionObservers))return NULL;
    const XL6_ForeignRegionObserverContract* found=NULL;
    for(size_t i=0;i<module->foreignRegionObserverCount;++i){
        const XL6_ForeignRegionObserverContract* row=&module->foreignRegionObservers[i];
        if(row->declaration==declaration && row->regionPhysicalIndex==physical){if(found)return NULL;found=row;}
    }
    return found;
}
static int xl6IrRegionObserverExpected(const XL6_IrModule* module,const XL6_ForeignRegionObserverContract* row,
    XL6_ForeignRegionObserverContract* expected)
{
    const XL6_IrNode *parameters,*queryParameters;size_t receiver,outputsCount,queryReceiver,queryOutputsCount;
    uint32 outputs[64]={0},queryOutputs[64]={0};
    const XL6_IrNode* node=xl6IrRegionSignature(module,row->declaration,&parameters,&receiver,outputs,&outputsCount);
    const XL6_IrNode* query=xl6IrSymbolNodeGet(module,row->querySymbol);
    if(!node || !query || query==node || !xl6ForeignInputHasRegions(node->foreignCall.inputMappingFacts) ||
        row->regionPhysicalIndex>=64 || row->inputCount>XL6_FOREIGN_REGION_OBSERVER_MAX_INPUTS)return 0;
    uint32 queryDeclaration=(uint32)(query-module->nodes+1);
    if(!xl6IrRegionSignature(module,queryDeclaration,&queryParameters,&queryReceiver,queryOutputs,&queryOutputsCount) ||
        queryParameters->childCount+queryReceiver!=row->inputCount || queryOutputsCount!=1 ||
        !xl6ForeignInputMappingsEmpty(query->foreignCall.inputMappingFacts) || query->foreignCall.receiverDereference ||
        query->foreignCall.physicalPairs || query->foreignCall.retainedCallbacks || query->foreignCall.controlViews ||
        query->foreignCall.constAggregateAddresses || query->foreignCall.staticCallbackAddresses || query->foreignCall.textViews ||
        query->foreignCall.bytesViews || query->foreignCall.terminatedStrings || query->foreignCall.rollbackSymbol || query->foreignCall.borrowedViewResult ||
        !(query->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (query->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
        ((node->flags&XL6_AST_FLAG_FOREIGN_PURE) && !(query->flags&XL6_AST_FLAG_FOREIGN_PURE)) ||
        ((node->flags&XL6_AST_FLAG_FOREIGN_NONBLOCKING) && !(query->flags&XL6_AST_FLAG_FOREIGN_NONBLOCKING)) ||
        (xl6ForeignValueOutputBase(query->foreignCall.valueOutput)!=XL6_FOREIGN_VALUE_OUTPUT_STATUS &&
         xl6ForeignValueOutputBase(query->foreignCall.valueOutput)!=XL6_FOREIGN_VALUE_OUTPUT_VOID))return 0;
    const XL6_IrNode* returns=xl6IrNodeGet(module,module->children[query->firstChild+2]);
    const XL6_IrNode* result=returns && returns->childCount==1 && returns->firstChild<module->childCount ? xl6IrNodeGet(module,module->children[returns->firstChild]):NULL;
    const XL6_IrType* record=result ? xl6IrTypeGet(&module->types,result->type):NULL;
    if(!record || record->kind!=XL6_IR_TYPE_C_RECORD || record->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL || !record->foreignHeader)return 0;
    *expected=*row;expected->queryDeclaration=queryDeclaration;expected->recordType=result->type;
    for(size_t p=0;p<XL6_FOREIGN_REGION_OBSERVER_MAX_INPUTS;++p){
        if(p>=row->inputCount){if(row->physicalInputs[p] || row->parameters[p])return 0;continue;}
        size_t parameter=xl6ForeignRegionPhysicalInput(&node->foreignCall,parameters->childCount+receiver,receiver,outputs,outputsCount,row->physicalInputs[p]);
        if(parameter==SIZE_MAX || row->physicalInputs[p]==row->regionPhysicalIndex ||
            xl6IrRegionInputType(module,node,parameters,receiver,parameter)!=xl6IrRegionInputType(module,query,queryParameters,queryReceiver,p))return 0;
        expected->parameters[p]=(uint32)parameter;
    }
    return 1;
}
static int xl6IrRegionSelectorExpected(const XL6_IrModule* module,const XL6_IrNode* function,
    const XL6_IrNode* params,size_t receiver,const uint32* outputs,size_t outputCount,const XL6_ForeignRegionContract* region,
    const XL6_ForeignRegionSelector* selector,XL6_ForeignRegionSelector* expected)
{
    if(selector->kind<XL6_FOREIGN_REGION_SELECTOR_INPUT || selector->kind>XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD ||
        !memchr(selector->field,0,sizeof(selector->field)))return 0;
    *expected=*selector;expected->parameter=expected->recordType=expected->fieldDeclaration=expected->fieldType=0;
    XL6_IrTypeId type=0;
    if(selector->kind==XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD){
        const XL6_ForeignRegionObserverContract* observer=xl6IrRegionObserverAt(module,region->declaration,region->physicalIndex);
        if(!observer || selector->physicalIndex)return 0;
        type=observer->recordType;
    }else{
        if(selector->physicalIndex>=64 || selector->physicalIndex==region->physicalIndex)return 0;
        size_t parameter=xl6ForeignRegionPhysicalInput(&function->foreignCall,params->childCount+receiver,receiver,outputs,outputCount,selector->physicalIndex);
        if(parameter==SIZE_MAX)return 0;
        expected->parameter=(uint32)parameter;type=xl6IrRegionInputType(module,function,params,receiver,parameter);
    }
    if(selector->kind==XL6_FOREIGN_REGION_SELECTOR_INPUT){
        if(selector->field[0] || !xl6IrRegionInteger(xl6IrTypeGet(&module->types,type)))return 0;
        expected->fieldType=type;return 1;
    }
    const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(module,type);
    const XL6_IrType* record=xl6IrTypeGet(&module->types,type);
    if(!record || record->kind!=XL6_IR_TYPE_C_RECORD || !record->foreignHeader || !xl6ForeignIdentifier(selector->field) ||
        !layout || layout->firstField>module->aggregateFieldCount || layout->fieldCount>module->aggregateFieldCount-layout->firstField)return 0;
    const XL6_IrAggregateField* found=NULL;
    for(size_t i=0;i<layout->fieldCount;++i){
        const XL6_IrAggregateField* field=&module->aggregateFields[layout->firstField+i];
        if(!memchr(field->cName,0,sizeof(field->cName)))return 0;
        if(!strcmp(field->cName,selector->field)){if(found)return 0;found=field;}
    }
    if(!found || !xl6IrRegionInteger(xl6IrTypeGet(&module->types,found->type)))return 0;
    expected->recordType=type;expected->fieldDeclaration=found->declaration;expected->fieldType=found->type;return 1;
}
static int xl6IrRegionExpected(const XL6_IrModule* module,const XL6_ForeignRegionContract* row,XL6_ForeignRegionContract* expected)
{
    const XL6_IrNode* params;size_t receiver,outputCount;uint32 outputs[64]={0};
    const XL6_IrNode* node=xl6IrRegionSignature(module,row->declaration,&params,&receiver,outputs,&outputCount);
    if(!node || !xl6ForeignInputHasRegions(node->foreignCall.inputMappingFacts) || row->physicalIndex>=64 || !row->unitBytes ||
        row->unitBytes>INT32_MAX || row->nullable>1 || row->access<XL6_FOREIGN_REGION_ACCESS_READ || row->access>XL6_FOREIGN_REGION_ACCESS_WRITE ||
        !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (node->flags&XL6_AST_FLAG_FOREIGN_FIELD) || node->foreignCall.receiverDereference ||
        (row->access==XL6_FOREIGN_REGION_ACCESS_WRITE && (row->nullable || (node->flags&XL6_AST_FLAG_FOREIGN_PURE))))return 0;
    if(node->sourceKind==XL6_AST_EXTERN_FUNC && !xl6IrForeignFunctionHeaderIndex(module,row->declaration))return 0;
    size_t parameter=xl6ForeignRegionPhysicalInput(&node->foreignCall,params->childCount+receiver,receiver,outputs,outputCount,row->physicalIndex);
    if(parameter==SIZE_MAX || parameter<receiver)return 0;
    uint64 bit=UINT64_C(1)<<(parameter-receiver);
    if((node->foreignCall.constAggregateAddresses|node->foreignCall.staticCallbackAddresses|node->foreignCall.textViews|node->foreignCall.bytesViews|
        node->foreignCall.terminatedStrings|node->foreignCall.physicalPairs|node->foreignCall.retainedCallbacks)&bit ||
        (node->foreignCall.controlViews&(UINT64_C(1)<<parameter)))return 0;
    XL6_IrTypeId logical=xl6IrRegionInputType(module,node,params,receiver,parameter);
    const XL6_IrType* type=xl6IrTypeGet(&module->types,logical);
    if(!type || (type->kind==XL6_IR_TYPE_NULLABLE)!=row->nullable)return 0;
    if(type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES){
        if(row->access!=XL6_FOREIGN_REGION_ACCESS_READ || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED)return 0;
    }else{
        XL6_IrTypeId storageId=xl6IrRegionStorageType(module,logical);
        const XL6_IrType* storage=xl6IrTypeGet(&module->types,storageId);
        const XL6_IrClassField* buffer=xl6IrManagedNativeConstructorField(module,storageId);
        if(!storage || storage->kind!=XL6_IR_TYPE_CLASS || !buffer || !buffer->nativeHandlePolicyCName ||
            strcmp(buffer->nativeHandlePolicyCName,"xl6BufferHandlePolicyV1Get"))return 0;
    }
    *expected=*row;expected->parameter=(uint32)parameter;expected->logicalType=logical;
    if(!xl6IrRegionSelectorExpected(module,node,params,receiver,outputs,outputCount,row,&row->width,&expected->width) ||
        !xl6IrRegionSelectorExpected(module,node,params,receiver,outputs,outputCount,row,&row->height,&expected->height) ||
        !xl6IrRegionSelectorExpected(module,node,params,receiver,outputs,outputCount,row,&row->stride,&expected->stride))return 0;
    size_t observed=(row->width.kind==XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD)+
        (row->height.kind==XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD)+(row->stride.kind==XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD);
    return (observed!=0)==(xl6IrRegionObserverAt(module,row->declaration,row->physicalIndex)!=NULL);
}
static int xl6IrCloseRegions(XL6_IrModule* module)
{
    for(size_t i=0;i<module->foreignRegionObserverCount;++i){
        XL6_ForeignRegionObserverContract expected;
        if(!xl6IrRegionObserverExpected(module,&module->foreignRegionObservers[i],&expected))return 0;
        module->foreignRegionObservers[i]=expected;
    }
    for(size_t i=0;i<module->foreignRegionCount;++i){
        XL6_ForeignRegionContract expected;
        if(!xl6IrRegionExpected(module,&module->foreignRegions[i],&expected))return 0;
        module->foreignRegions[i]=expected;
    }
    return 1;
}
static int xl6IrVerifyRegions(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignRegionCount>module->foreignRegionCapacity || (module->foreignRegionCount && !module->foreignRegions) ||
        module->foreignRegionCount/64>module->nodeCount ||
        module->foreignRegionObserverCount>module->foreignRegionObserverCapacity || (module->foreignRegionObserverCount && !module->foreignRegionObservers) ||
        module->foreignRegionObserverCount>module->foreignRegionCount)goto invalid;
    for(size_t i=0;i<module->foreignRegionObserverCount;++i){
        const XL6_ForeignRegionObserverContract* row=&module->foreignRegionObservers[i];XL6_ForeignRegionObserverContract expected;
        if(!xl6IrRegionObserverExpected(module,row,&expected) || row->queryDeclaration!=expected.queryDeclaration || row->recordType!=expected.recordType ||
            memcmp(row->parameters,expected.parameters,sizeof(row->parameters)))goto invalid;
        size_t regions=0;for(size_t r=0;r<module->foreignRegionCount;++r)
            regions+=module->foreignRegions[r].declaration==row->declaration && module->foreignRegions[r].physicalIndex==row->regionPhysicalIndex;
        if(regions!=1 || xl6IrRegionObserverAt(module,row->declaration,row->regionPhysicalIndex)!=row)goto invalid;
    }
    for(size_t i=0;i<module->foreignRegionCount;++i){
        const XL6_ForeignRegionContract* row=&module->foreignRegions[i];XL6_ForeignRegionContract expected;
        if(!xl6IrRegionExpected(module,row,&expected) || row->parameter!=expected.parameter || row->logicalType!=expected.logicalType ||
            memcmp(&row->width,&expected.width,sizeof(row->width)) || memcmp(&row->height,&expected.height,sizeof(row->height)) ||
            memcmp(&row->stride,&expected.stride,sizeof(row->stride)))goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignRegions[j].declaration==row->declaration && module->foreignRegions[j].physicalIndex==row->physicalIndex)goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n){
        size_t count=0;for(size_t r=0;r<module->foreignRegionCount;++r)count+=module->foreignRegions[r].declaration==n+1;
        if(!xl6ForeignInputMappingsValid(module->nodes[n].foreignCall.inputMappingFacts) ||
            (count!=0)!=xl6ForeignInputHasRegions(module->nodes[n].foreignCall.inputMappingFacts))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"fixed C regions require sealed unique typed input extents, exact observer dependencies and input mapping presence");
}
const XL6_ForeignRegionContract* xl6IrForeignRegionGet(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    if(!module || !target || module->foreignRegionCount>module->foreignRegionCapacity || (module->foreignRegionCount && !module->foreignRegions))return NULL;
    for(size_t i=0;i<module->foreignRegionCount;++i)
        if(module->foreignRegions[i].declaration==target->declaration && module->foreignRegions[i].parameter==parameter)return &module->foreignRegions[i];
    return NULL;
}
const XL6_ForeignRegionObserverContract* xl6IrForeignRegionObserverGet(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignRegionContract* region=xl6IrForeignRegionGet(module,target,parameter);
    return region ? xl6IrRegionObserverAt(module,target->declaration,region->physicalIndex):NULL;
}
const XL6_IrClassField* xl6IrForeignRegionBufferField(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    if(!xl6IrForeignRegionGet(module,target,parameter) || parameter>=target->parameterCount)return NULL;
    const XL6_IrClassField* field=xl6IrManagedNativeConstructorField(module,xl6IrRegionStorageType(module,target->parameterTypes[parameter]));
    return field && field->nativeHandlePolicyCName && !strcmp(field->nativeHandlePolicyCName,"xl6BufferHandlePolicyV1Get") ? field:NULL;
}
int xl6IrForeignRegionParameterMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignRegionContract* row=xl6IrForeignRegionGet(module,target,parameter);
    if(!row || parameter>=target->parameterCount || row->logicalType!=target->parameterTypes[parameter] ||
        target->parameterAbis[parameter]!=XL6_IR_PARAMETER_ABI_BORROW || target->parameterConstraintAbis[parameter])return 0;
    const XL6_IrClassField* buffer=xl6IrForeignRegionBufferField(module,target,parameter);
    if(target->parameterPhysicalFieldDeclarations[parameter]!=(buffer ? buffer->declaration:0))return 0;
    const XL6_ForeignRegionObserverContract* observer=xl6IrForeignRegionObserverGet(module,target,parameter);
    if(observer){
        const XL6_IrCallTarget* query=xl6IrCallTargetGet(module,observer->querySymbol);
        if(!query || query->declaration!=observer->queryDeclaration || query->returnType!=observer->recordType || query->parameterCount!=observer->inputCount ||
            query->kind!=XL6_IR_CALL_TARGET_EXTERN_C || query->runtimeBridge || query->nativeContextAbi || query->nativeServicesRequired ||
            query->resultConversionAbi!=XL6_IR_CALL_RESULT_CONVERSION_NATIVE_OUTPUT_VALUE)return 0;
        for(size_t p=0;p<query->parameterCount;++p){
            if(observer->parameters[p]>=target->parameterCount || query->parameterTypes[p]!=target->parameterTypes[observer->parameters[p]])return 0;
            XL6_IrCallParameterPhysicalAbi abi=query->parameterPhysicalAbis[p];
            XL6_IrCallParameterPhysicalAbi actual=target->parameterPhysicalAbis[observer->parameters[p]];
            if(abi==XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT){if(actual!=abi)return 0;}
            else if(abi==XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW){if(actual!=abi)return 0;}
            else if(abi==XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_FOREIGN_VIEW_BORROW){
                if(actual!=abi && actual!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW)return 0;
            }else if(abi==XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_NATIVE_POINTER_DIRECT){
                if(actual!=abi && actual!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)return 0;
            }else return 0;
        }
    }
    return 1;
}
