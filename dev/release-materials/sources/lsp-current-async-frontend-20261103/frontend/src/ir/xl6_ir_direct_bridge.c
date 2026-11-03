/* Closed resident contracts are generated once into immutable, deduplicated
 * records. Runtime lookup never parses declarations or allocates a view. */
#include "xl6_ir_direct_bridge_types.h"

/* Opt-in nominal scalar contracts, not a general permission to substitute
 * enums for integers. Canonical declaration/layout identity, representation
 * and the complete closed member vocabulary are checked before emission. */
static int xl6IrDirectNumericEnumMatches(const XL6_IrModule* module,
    XL6_IrTypeId id,const XL6_IrNumericEnumContract* expected)
{
    if(!module||!expected)return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if(!type||type->kind!=XL6_IR_TYPE_NUMERIC_ENUM||
        type->cAbi!=expected->cAbi||type->integerTarget!=expected->integerTarget||
        type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL||type->nullAbi!=XL6_IR_NULL_ABI_NONE||
        type->collectionAbi!=XL6_IR_COLLECTION_NONE||type->paramCount||type->returnCount||
        !type->nominalSymbol||!type->typeValueId||
        module->enumLayoutCount>module->enumLayoutCapacity||
        (module->enumLayoutCount&&!module->enumLayouts)||
        module->enumVariantCount>module->enumVariantCapacity||
        (module->enumVariantCount&&!module->enumVariants))return 0;
    const XL6_IrEnumLayout* layout=xl6IrEnumLayoutGet(module,id);
    const XL6_IrNode* declaration=layout?xl6IrNodeGet(module,layout->declaration):NULL;
    if(!layout||layout->type!=id||!declaration||declaration->kind!=XL6_IR_NODE_ENUM_DECL||
        declaration->sourceKind!=XL6_AST_ENUM||!(declaration->flags&XL6_AST_FLAG_NUMERIC_ENUM)||
        declaration->symbol!=type->nominalSymbol||!declaration->text||
        strcmp(declaration->text,expected->declarationName)||layout->variantCount!=expected->memberCount||
        layout->firstVariant>module->enumVariantCount||
        layout->variantCount>module->enumVariantCount-layout->firstVariant)return 0;
    for(size_t n=0;n<expected->memberCount;++n){
        const XL6_IrEnumVariant* item=&module->enumVariants[layout->firstVariant+n];
        const XL6_IrNode* member=xl6IrNodeGet(module,item->declaration);
        uint64 magnitude=0;int negative=0;
        if(item->ownerType!=id||item->payloadCount||!item->name||strcmp(item->name,expected->members[n].name)||
            !member||member->kind!=XL6_IR_NODE_ENUM_VARIANT||member->sourceKind!=XL6_AST_ENUM_VARIANT||
            !item->symbol||member->symbol!=item->symbol||item->tag!=n+1||
            !xl6IrContextualIntegerLiteralValue(module,item->valueExpression,id,&magnitude,&negative)||
            magnitude!=expected->members[n].magnitude||negative!=expected->members[n].negative)return 0;
    }
    return 1;
}
#include "xl6_ir_direct_bridge_enums.inc"
#include "xl6_ir_direct_bridge_data.inc"


#include "xl6_ir_direct_bridge_lookup.inc"

static int xl6IrInternNativeCallValueSchemas(XL6_IrModule* module)
{
    if (!module) return 0;
    for(size_t n=0;n<module->nodeCount;++n) {
        const XL6_IrNode* method=&module->nodes[n];
        if(method->kind!=XL6_IR_NODE_FUNC_DECL)continue;
        const XL6_IrDirectBridgeContract* row=xl6IrDirectBridgeContractSpelling(method->text);
        if(!row || !row->details->sequenceValueResult)continue;
        XL6_IrNodeId owner=method->parent;
        while(owner) {
            const XL6_IrNode* node=xl6IrNodeGet(module,owner);
            if(!node)return 0;
            if(node->kind==XL6_IR_NODE_TYPE_DECL && node->sourceKind==XL6_AST_CLASS)break;
            owner=node->parent;
        }
        if(!owner)continue; /* invalid free-function signatures are rejected separately */
        for(size_t i=0;i<module->types.count;++i) {
            const XL6_IrType* type=&module->types.items[i];
            if(type->kind!=XL6_IR_TYPE_CLASS || type->paramCount!=1 || !type->params ||
                !xl6IrBuildClassTypeIsConcreteInstance(module,owner,(XL6_IrTypeId)(i+1)))continue;
            XL6_IrTypeId element=type->params[0];
            if(!xl6IrTypeInternArray(&module->types,element))return 0;
        }
    }
    return 1;
}

XL6_IrRuntimeBridgeKind xl6IrRuntimeDirectBridgeKind(const char* name)
{
    const XL6_IrDirectBridgeContract* contract = xl6IrDirectBridgeContract(name);
    return contract != NULL ? contract->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}

XL6_IrCallNativeArrayResultAbi xl6IrRuntimeDirectBridgeNativeArrayResultAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* contract = xl6IrDirectBridgeContract(name);
    return contract ? contract->details->nativeArrayResultAbi : XL6_IR_CALL_NATIVE_ARRAY_RESULT_NONE;
}

const char* xl6IrRuntimeNativeHandlePolicyForName(const char* name)
{
    const XL6_IrDirectBridgeContract* contract=xl6IrDirectBridgeContractSpelling(name);
    if(contract)return contract->details->nativeHandlePolicy;
    /* Dedicated bridge families use the same exact resource declaration as
     * direct calls. This is linkage metadata, not Cgen symbol heuristics. */
    if(name){
        size_t length=strlen(name);const char* canonical=name;
        if(length>=2 && name[0]=='"' && name[length-1]=='"'){canonical=name+1;length-=2;}
        static const struct {const char* owner;const char* getter;} resources[]={
#define XL6_NATIVE_RESOURCE_POLICY(Owner,Component,Getter) {Owner,#Getter},
#include "src/runtime/xl6_native_resource_policies.inc"
#undef XL6_NATIVE_RESOURCE_POLICY
        };
        for(size_t i=0;i<sizeof(resources)/sizeof(resources[0]);++i)
            if(resources[i].owner && strlen(resources[i].owner)==length && !memcmp(canonical,resources[i].owner,length))return resources[i].getter;
    }
    return NULL;
}

int xl6IrRuntimeDirectBridgeFactory(const char* name)
{ const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name); return c && c->details->resultOwner; }
int xl6IrRuntimeDirectBridgeOwnsString(const char* name)
{ const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
  return c && c->details->ownedStringResult && c->result==XL6_IR_C_ABI_RAW_POINTER && !c->details->resultOwner; }
XL6_IrErrorAbi xl6IrRuntimeDirectBridgeErrorAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    return c && !c->details->noRaise
        ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
}
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeDirectBridgeErrorPhysicalAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    if (!c) return XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
    if (c->details->noRaise) return XL6_IR_CALL_ERROR_PHYSICAL_NONE;
    if (c->details->errorPhysicalAbi) return c->details->errorPhysicalAbi;
    return c->details->nativeArrayResultAbi ? XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT
        : XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;
}
const char* xl6IrRuntimeDirectBridgeParameterOwner(const char* name, size_t index)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    return c && index < c->parameterCount ? c->parameters[index].owner : NULL;
}
int xl6IrRuntimeDirectBridgeCheckedClosureParameter(const char* name,size_t index)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c && index<c->parameterCount &&
        (c->parameters[index].nullaryCallableResultAbi!=0 || c->details->genericFutureStarterParameter==index+1 ||
         c->details->nativeClassCallbackParameter==index+1);
}
int xl6IrRuntimeDirectBridgeResultClosureParameter(const char* name,size_t index)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c&&index<c->parameterCount&&c->details->genericFutureValueCallbackParameter==index+1;
}
int xl6IrRuntimeDirectBridgeValueAddressParameter(const char* name,size_t index)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContractSpelling(name);
    return c && c->details->genericValueParameter==index+1;
}
XL6_IrCallNativeResultAbi xl6IrRuntimeDirectBridgeNativeResultAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c ? c->details->nativeResultAbi : XL6_IR_CALL_NATIVE_RESULT_NONE;
}
int xl6IrRuntimeDirectBridgeManagedHandleResult(const char* name)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c && c->details->resultOwner && c->details->managedHandleResult;
}
XL6_IrTypeId xl6IrNativeContextElementType(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if (!module || !target) return XL6_IR_TYPE_INVALID;
    if (target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1 ||
        target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1)
        return target->nativeContextType;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,target->nativeContextType);
    return owner && owner->params && owner->paramCount==1 ? owner->params[0] : XL6_IR_TYPE_INVALID;
}
const XL6_IrClassField* xl6IrNativeClassArrayContextField(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* c=target ? xl6IrDirectBridgeContract(target->cName) : NULL;
    if(!module || !c || !c->details->nativeClassArrayOwner || !c->details->nativeClassArrayParameter ||
        c->details->nativeClassArrayParameter>target->parameterCount || !target->parameterTypes)return NULL;
    XL6_IrTypeId id=target->parameterTypes[c->details->nativeClassArrayParameter-1];
    const XL6_IrType* array=xl6IrTypeGet(&module->types,id);
    if(!array || array->collectionAbi!=XL6_IR_COLLECTION_ARRAY || array->paramCount!=1 || !array->params ||
        !xl6IrSequenceConstructionRecipe(module,id,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL))return NULL;
    return xl6IrResidentNativeResourceField(module,array->params[0],c->details->nativeClassArrayOwner);
}
const XL6_IrClassField* xl6IrNativeClassContextField(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* c=target?xl6IrDirectBridgeContract(target->cName):NULL;
    if(!module || !c || !c->details->nativeClassOwner || !c->details->nativeClassParameter ||
        c->details->nativeClassParameter>target->parameterCount || !target->parameterTypes)return NULL;
    return xl6IrResidentNativeResourceField(module,
        target->parameterTypes[c->details->nativeClassParameter-1],c->details->nativeClassOwner);
}
unsigned xl6IrRuntimeDirectBridgeServices(const char* name)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c ? c->details->nativeServicesRequired : XL6_IR_CALL_NATIVE_SERVICE_NONE;
}

static XL6_IrTypeId xl6IrNativeClassResultType(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrDirectBridgeContract* c)
{
    if(!module||!target||!c||c->details->nativeContext!=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1||
        !c->details->nativeClassResultOwner)return XL6_IR_TYPE_INVALID;
    const XL6_IrType* result=xl6IrTypeGet(&module->types,target->returnType);
    if(c->details->genericFutureResult){
        if(c->details->nativeResultAbi!=XL6_IR_CALL_NATIVE_RESULT_NONE||!result||result->kind!=XL6_IR_TYPE_GENERIC_INSTANCE||
            result->cAbi!=XL6_IR_C_ABI_RUNTIME_FUTURE_PTR||result->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||
            result->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL||result->collectionAbi!=XL6_IR_COLLECTION_NONE||
            result->paramCount!=1||!result->params||result->returnCount)return XL6_IR_TYPE_INVALID;
        return result->params[0];
    }
    /* The same sealed recipe may construct a synchronous owned class directly
     * into the existing checked output-slot ABI. No language placeholder or
     * post-effect class allocation is required. */
    if(c->details->nativeResultAbi!=XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1||c->details->resultOwner||c->result||c->details->noRaise||
        !result||result->kind!=XL6_IR_TYPE_CLASS||result->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR||
        result->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||result->nullAbi!=XL6_IR_NULL_ABI_XVALUE_SENTINEL||
        result->collectionAbi!=XL6_IR_COLLECTION_NONE||result->paramCount||result->returnCount)return XL6_IR_TYPE_INVALID;
    return target->returnType;
}
const XL6_IrClassField* xl6IrNativeClassResultContextField(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* c=target?xl6IrDirectBridgeContract(target->cName):NULL;
    XL6_IrTypeId type=xl6IrNativeClassResultType(module,target,c);
    if(!type)return NULL;
    const XL6_IrClassField* field=xl6IrResidentNativeResourceField(module,type,c->details->nativeClassResultOwner);
    const XL6_IrClassLayout* layout=field?xl6IrBuildClassLayoutByType(module,type):NULL;
    if(!layout||layout->baseType||layout->baseDeclaration!=XL6_IR_NODE_INVALID||layout->fieldCount!=1||
        layout->destroyFunction||layout->destroyStepCount||layout->persistenceFormatMask||
        layout->hashAbi!=XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE||
        layout->equalityAbi!=XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE)return NULL;
    return field;
}

static const XL6_IrClassField* xl6IrNativeCallbackClassField(const XL6_IrModule* module,
    XL6_IrTypeId type,const char* owner)
{
    const XL6_IrType* t=xl6IrTypeGet(&module->types,type);
    if(!t||t->kind!=XL6_IR_TYPE_CLASS||t->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR||
        t->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||t->nullAbi!=XL6_IR_NULL_ABI_XVALUE_SENTINEL||
        t->paramCount||t->collectionAbi!=XL6_IR_COLLECTION_NONE)return NULL;
    const XL6_IrClassField* field=xl6IrResidentNativeResourceField(module,type,owner);
    const XL6_IrClassLayout* layout=field?xl6IrBuildClassLayoutByType(module,type):NULL;
    if(!layout||layout->baseType||layout->baseDeclaration!=XL6_IR_NODE_INVALID||layout->fieldCount!=1||
        layout->destroyFunction||layout->destroyStepCount||layout->persistenceFormatMask||
        layout->hashAbi!=XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE||
        layout->equalityAbi!=XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE)return NULL;
    return field;
}
int xl6IrNativeClassCallbackPlanBuild(const XL6_IrModule* module,const XL6_IrCallTarget* target,
    XL6_IrNativeClassCallbackPlan* output)
{
    const XL6_IrDirectBridgeContract* c=target?xl6IrDirectBridgeContract(target->cName):NULL;
    if(!module||!c||!xl6IrNativeContextIsClassCallback(c->details->nativeContext)||
        !c->details->nativeClassCallbackParameter||c->details->nativeClassCallbackParameter>target->parameterCount||
        !target->parameterTypes||!c->details->nativeClassCallbackArgumentOwner)return 0;
    int predicate=c->details->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_PREDICATE_V1;
    if(predicate?c->details->nativeClassCallbackResultOwner!=NULL:c->details->nativeClassCallbackResultOwner==NULL)return 0;
    XL6_IrTypeId signature=target->parameterTypes[c->details->nativeClassCallbackParameter-1];
    const XL6_IrType* f=xl6IrTypeGet(&module->types,signature);
    if(!f||f->kind!=XL6_IR_TYPE_FUNCTION||f->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR||
        f->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||f->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL||
        !f->typeValueId||f->collectionAbi!=XL6_IR_COLLECTION_NONE||f->paramCount!=1||!f->params||
        !f->parameterAbis||f->parameterAbis[0]!=XL6_IR_PARAMETER_ABI_BORROW||!f->parameterModes||
        f->parameterModes[0]!=XL6_IR_PARAMETER_MODE_DEFAULT||
        f->returnCount!=1||!f->returns||f->callableResultType!=f->returns[0]||
        f->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED)return 0;
    const XL6_IrType* r=xl6IrTypeGet(&module->types,f->returns[0]);
    if(predicate){
        if(!r||r->kind!=XL6_IR_TYPE_BUILTIN||r->cAbi!=XL6_IR_C_ABI_BOOL||
            r->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL||r->nullAbi!=XL6_IR_NULL_ABI_NONE||
            r->paramCount||r->returnCount||r->collectionAbi!=XL6_IR_COLLECTION_NONE)return 0;
        XL6_IrNativeClassCallbackPlan plan={signature,f->params[0],f->returns[0],
            xl6IrNativeCallbackClassField(module,f->params[0],c->details->nativeClassCallbackArgumentOwner),NULL};
        if(!plan.argumentField)return 0;
        if(output)*output=plan;
        return 1;
    }
    if(!r||r->kind!=XL6_IR_TYPE_NULLABLE||r->paramCount!=1||!r->params||
        r->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR||r->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||
        r->nullAbi!=XL6_IR_NULL_ABI_XVALUE_SENTINEL||r->collectionAbi!=XL6_IR_COLLECTION_NONE)return 0;
    XL6_IrNativeClassCallbackPlan plan={signature,f->params[0],r->params[0],
        xl6IrNativeCallbackClassField(module,f->params[0],c->details->nativeClassCallbackArgumentOwner),
        xl6IrNativeCallbackClassField(module,r->params[0],c->details->nativeClassCallbackResultOwner)};
    if(!plan.argumentField||!plan.resultField)return 0;
    if(output)*output=plan;
    return 1;
}
static const XL6_IrType* xl6IrNativeVoidCallbackArrayType(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrDirectBridgeContract* contract)
{
    if(!module||!target||!contract||!contract->details->nativeVoidCallbackArrayParameter||!target->parameterTypes||
        contract->details->nativeVoidCallbackArrayParameter>target->parameterCount)return NULL;
    XL6_IrTypeId id=target->parameterTypes[contract->details->nativeVoidCallbackArrayParameter-1];
    const XL6_IrType* array=xl6IrTypeGet(&module->types,id);
    if(!array||array->kind!=XL6_IR_TYPE_GENERIC_INSTANCE||array->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR||
        array->collectionAbi!=XL6_IR_COLLECTION_ARRAY||array->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||
        array->nullAbi!=XL6_IR_NULL_ABI_XVALUE_SENTINEL||array->paramCount!=1||!array->params||
        array->returnCount||array->returns||!array->typeValueId)return NULL;
    const XL6_IrType* callback=xl6IrTypeGet(&module->types,array->params[0]);
    if(!callback||callback->kind!=XL6_IR_TYPE_FUNCTION||callback->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR||
        callback->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||callback->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL||
        callback->collectionAbi!=XL6_IR_COLLECTION_NONE||callback->paramCount||callback->params||
        callback->parameterAbis||callback->parameterModes||callback->returnCount||callback->returns||
        callback->callableResultType||callback->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED||!callback->typeValueId)return NULL;
    XL6_IrSequenceLayout expected=xl6IrSequenceLayoutForType(&module->types,array);
    if(expected.abi!=XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1||expected.nullable||
        expected.elementType!=array->params[0]||expected.slotAbi!=XL6_IR_NATIVE_SLOT_XVALUE_CARRIER||
        expected.codecAbi!=XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1||
        !xl6IrSequenceLayoutEqual(&array->sequenceLayout,&expected)||
        !xl6IrSequenceConstructionRecipe(module,id,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL))return NULL;
    return array;
}
int xl6IrNativeCallValuePlanBuild(const XL6_IrModule* module,const XL6_IrCallTarget* target,
    XL6_IrNativeCallValuePlan* output)
{
    if (!module || !target) return 0;
    XL6_IrNativeCallValuePlan plan={0};
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(target->cName);
    if (c && (c->details->genericChannelParameter || c->details->sequenceValueResult)) {
        if (!target->parameterTypes || target->parameterCount!=c->parameterCount ||
            c->details->errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE) return 0;
        if (c->details->genericChannelParameter) {
            if (c->details->genericChannelParameter>target->parameterCount ||
                !xl6IrChannelValuePlanBuild(module,target->parameterTypes[c->details->genericChannelParameter-1],&plan.value)) return 0;
        } else {
            const XL6_IrType* owner=xl6IrTypeGet(&module->types,target->parameterTypes[0]);
            if (!owner || owner->kind!=XL6_IR_TYPE_CLASS || owner->paramCount!=1 || !owner->params ||
                !xl6IrChannelValuePlanFromSchema(module,
                    xl6IrArraySchemaTypeForElement(&module->types,owner->params[0]),&plan.value)) return 0;
        }
        plan.inputParameter=c->details->genericValueParameter;
        plan.output=c->details->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1;
        if ((plan.inputParameter && (plan.inputParameter>target->parameterCount ||
             target->parameterTypes[plan.inputParameter-1]!=plan.value.elementType)) ||
            (plan.output && target->returnType!=plan.value.elementType)) return 0;
    }
    if (output) *output=plan;
    return 1;
}

int xl6IrRuntimeDirectBridgeContext(const XL6_IrModule* module,const XL6_IrCallTarget* target,
    XL6_IrCallNativeContextAbi* context,XL6_IrTypeId* type,XL6_IrAsyncResultAbi* payload)
{
    if(!module || !target || !context || !type || !payload)return 0;
    *context=XL6_IR_CALL_NATIVE_CONTEXT_NONE;*type=XL6_IR_TYPE_INVALID;*payload=XL6_IR_ASYNC_RESULT_INVALID;
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(target->cName);
    if(!c || !c->details->nativeContext)return 1;
    if(xl6IrNativeContextIsClassCallback(c->details->nativeContext)){
        XL6_IrNativeClassCallbackPlan plan={0};
        if(!xl6IrNativeClassCallbackPlanBuild(module,target,&plan))return 0;
        *context=c->details->nativeContext;*type=plan.signature;return 1;
    }
    if(c->details->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_NULLARY_VOID_ARRAY_V1){
        if(!xl6IrNativeVoidCallbackArrayType(module,target,c))return 0;
        *context=c->details->nativeContext;*type=target->parameterTypes[c->details->nativeVoidCallbackArrayParameter-1];return 1;
    }
    if(c->details->genericFutureStarterParameter||c->details->genericFutureValueCallbackParameter) {
        const XL6_IrType* future=xl6IrTypeGet(&module->types,target->returnType);
        if(c->details->nativeContext!=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1 || !c->details->genericFutureResult ||
           !future || future->kind!=XL6_IR_TYPE_GENERIC_INSTANCE ||
           future->cAbi!=XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
           future->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
           future->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
           future->collectionAbi!=XL6_IR_COLLECTION_NONE || future->returnCount ||
           future->paramCount!=1 || !future->params)return 0;
        const XL6_IrType* value=xl6IrTypeGet(&module->types,future->params[0]);
        XL6_IrAsyncResultAbi abi=xl6IrBuildAsyncResultAbi(module,future->params[0]);
        if(!value || !value->typeValueId || abi==XL6_IR_ASYNC_RESULT_INVALID)return 0;
        *context=c->details->nativeContext;*type=future->params[0];*payload=abi;return 1;
    }
    if(c->details->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1) {
        if(!xl6IrNativeClassContextField(module,target))return 0;
        *context=c->details->nativeContext;*type=target->parameterTypes[c->details->nativeClassParameter-1];
        *payload=xl6IrBuildAsyncResultAbi(module,*type);
        return *payload==XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;
    }
    if(c->details->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1) {
        if(!xl6IrNativeClassResultContextField(module,target))return 0;
        *context=c->details->nativeContext;*type=xl6IrNativeClassResultType(module,target,c);*payload=xl6IrBuildAsyncResultAbi(module,*type);
        return *payload==XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;
    }
    if(c->details->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1) {
        if(!xl6IrNativeClassArrayContextField(module,target))return 0;
        *context=c->details->nativeContext;*type=target->parameterTypes[c->details->nativeClassArrayParameter-1];return 1;
    }
    if(c->details->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1) {
        if(!c->details->genericChannelParameter || c->details->genericChannelParameter>target->parameterCount || !target->parameterTypes)
            return 0;
        XL6_IrTypeId owner=target->parameterTypes[c->details->genericChannelParameter-1];
        XL6_IrNativeCallValuePlan message={0};
        if(!xl6IrNativeCallValuePlanBuild(module,target,&message))return 0;
        *context=c->details->nativeContext;*type=owner;return 1;
    }
    XL6_IrTypeId ownerId=c->details->resultOwner?target->returnType:
        target->parameterCount && target->parameterTypes?target->parameterTypes[0]:XL6_IR_TYPE_INVALID;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
    if(!owner || owner->kind!=XL6_IR_TYPE_CLASS || owner->paramCount!=1 || !owner->params)return 0;
    const XL6_IrType* value=xl6IrTypeGet(&module->types,owner->params[0]);
    if(!value || !value->typeValueId)return 0;
    XL6_IrAsyncResultAbi abi=xl6IrBuildAsyncResultAbi(module,owner->params[0]);
    if (c->details->sequenceValueResult) {
        XL6_IrNativeCallValuePlan message={0};
        if (!xl6IrNativeCallValuePlanBuild(module,target,&message)) return 0;
        if (message.value.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT)
            abi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;
    }
    if(abi==XL6_IR_ASYNC_RESULT_INVALID)return 0;
    *context=c->details->nativeContext;*type=owner->params[0];*payload=abi;return 1;
}
static int xl6IrDirectBridgeResourceMatches(const XL6_IrModule* module,
    XL6_IrTypeId id, XL6_IrNodeId fieldId, XL6_SymbolId destroyId, const char* owner)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, id);
    if (!type || type->kind != XL6_IR_TYPE_CLASS ||
        type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL || fieldId == XL6_IR_NODE_INVALID) return 0;
    if (module->stage == XL6_IR_STAGE_LOWERED) {
        /* The physical contract selects ONE native field, not a one-field
         * language class. Other managed fields retain their own ordinary
         * layout/trace/destruction contracts; the payload's leaf policy does
         * not certify or hide them. No name/order/first-field inference. */
        const XL6_IrClassField* field = xl6IrManagedNativeConstructorField(module, id);
        if (!field) return 0;
        const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, field->nativeDestroySymbol);
        if (field->declaration != fieldId || !destroy || !destroy->cName ||
            strcmp(destroy->cName, owner) || (destroyId && destroyId != field->nativeDestroySymbol)) return 0;
    }
    return 1;
}

static int xl6IrDirectBridgeResultResourceMatches(const XL6_IrModule* module,
    const XL6_IrCallTarget* target, const XL6_IrDirectBridgeContract* contract)
{
    XL6_IrTypeId owner = target->returnType;
    const XL6_IrType* type = xl6IrTypeGet(&module->types, owner);
    if (!type || (type->kind == XL6_IR_TYPE_NULLABLE) != !!contract->details->nullableResult) return 0;
    if (contract->details->nullableResult) {
        /* NULL is an ordinary success value, never an error discriminator.
         * A separate checked outcome is mandatory for optional owners. */
        if (contract->details->managedHandleResult || type->paramCount != 1 || !type->params ||
            type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
            type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL ||
            type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
            (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
             target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE)) return 0;
        owner = type->params[0];
    }
    return xl6IrDirectBridgeResourceMatches(module, owner,
        target->resultPhysicalFieldDeclaration, target->resultPhysicalDestroySymbol, contract->details->resultOwner);
}

static int xl6IrDirectBridgeCollectionMatches(const XL6_IrModule* module,
    const XL6_IrType* type, XL6_IrCAbi itemAbi, XL6_IrCollectionAbi collection,
    const XL6_IrNumericEnumContract* itemEnum)
{
    const XL6_IrType* item = type && type->paramCount == 1 && type->params
        ? xl6IrTypeGet(&module->types, type->params[0]) : NULL;
    if (!type || type->kind != XL6_IR_TYPE_GENERIC_INSTANCE || type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->collectionAbi != collection || type->returnCount != 0 ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL || !item ||
        item->paramCount != 0 ||
        item->collectionAbi != XL6_IR_COLLECTION_NONE || item->returnCount != 0 ||
        item->cAbi != itemAbi) return 0;
    if(itemEnum)return collection==XL6_IR_COLLECTION_ARRAY &&
        xl6IrDirectNumericEnumMatches(module,type->params[0],itemEnum);
    if(item->kind!=XL6_IR_TYPE_BUILTIN)return 0;
    if (itemAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR)
        return xl6IrSerializationBridgeDynamicValueType(item);
    if (itemAbi == XL6_IR_C_ABI_MANAGED_STRING)
        return item->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            item->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL;
    if (itemAbi == XL6_IR_C_ABI_MANAGED_BYTES)
        return item->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            item->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    if (itemAbi == XL6_IR_C_ABI_I64 && item->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64) return 0;
    if (itemAbi == XL6_IR_C_ABI_U64 && item->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64) return 0;
    return xl6IrBufferRuntimeBridgeResultMatches(item, itemAbi);
}

int xl6IrRuntimeDirectBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* contract;
    const XL6_IrType* result;
    size_t i;
    if (module == NULL || target == NULL ||
        (contract = xl6IrDirectBridgeContract(target->cName)) == NULL ||
        target->kind != (contract->details->resultOwner ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_EXTERN_C) ||
        target->runtimeBridge != contract->kind || target->errorAbi !=
            xl6IrRuntimeDirectBridgeErrorAbi(contract->name) ||
        target->nativeArrayResultAbi != contract->details->nativeArrayResultAbi ||
        target->nativeResultAbi != contract->details->nativeResultAbi ||
        target->nativeServicesRequired != contract->details->nativeServicesRequired ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != xl6IrRuntimeDirectBridgeErrorPhysicalAbi(contract->name)) ||
        (contract->details->resultOwner
            ? (target->resultPhysicalAbi != (contract->details->managedHandleResult
                    ? XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_HANDLE_VALUE_TAKE
                    : XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE) ||
               !target->resultPhysicalDestroySymbol ||
               !xl6IrDirectBridgeResultResourceMatches(module, target, contract))
            : (target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
               target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID || target->resultPhysicalDestroySymbol != 0)) ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        target->parameterCount != contract->parameterCount ||
        (target->parameterCount && (target->parameterTypes == NULL || target->parameterAbis == NULL ||
         target->parameterPhysicalAbis == NULL || target->parameterConstraintAbis == NULL ||
         target->parameterPhysicalFieldDeclarations == NULL))) return 0;
    result = xl6IrTypeGet(&module->types, target->returnType);
    {
        XL6_IrCallNativeContextAbi context;XL6_IrTypeId type;XL6_IrAsyncResultAbi payload;
        if(!xl6IrRuntimeDirectBridgeContext(module,target,&context,&type,&payload) ||
           context!=target->nativeContextAbi || type!=target->nativeContextType ||
           payload!=target->nativeContextPayloadAbi)return 0;
    }
    if(contract->details->genericResourceResult || contract->details->genericResourceParameter) {
        XL6_IrTypeId expected=target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_NONE
            ? result && result->kind==XL6_IR_TYPE_CLASS && result->paramCount==1 && result->params
                ? result->params[0] : XL6_IR_TYPE_INVALID
            : xl6IrNativeContextElementType(module,target);
        if(!expected && target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_NONE &&
            contract->details->genericResourceParameter &&
            contract->details->genericResourceParameter<=target->parameterCount) {
            const XL6_IrType* resource=xl6IrTypeGet(&module->types,
                target->parameterTypes[contract->details->genericResourceParameter-1]);
            if(resource && resource->kind==XL6_IR_TYPE_CLASS && resource->paramCount==1 && resource->params)
                expected=resource->params[0];
        }
        if(contract->details->genericResourceResult && (!expected || !result || result->kind!=XL6_IR_TYPE_CLASS ||
            result->paramCount!=1 || !result->params || result->params[0]!=expected))return 0;
        if(contract->details->genericResourceParameter) {
            size_t index=contract->details->genericResourceParameter-1;
            const XL6_IrType* resource=index<target->parameterCount
                ? xl6IrTypeGet(&module->types,target->parameterTypes[index]):NULL;
            if(!resource || resource->kind!=XL6_IR_TYPE_CLASS || resource->paramCount!=1 || !resource->params ||
                !expected || resource->params[0]!=expected)return 0;
        }
    }
    if (contract->details->nativeArrayResultAbi) {
        if (!result || !result->typeValueId || result->paramCount != 1 || !result->params ||
            !xl6IrDirectBridgeCollectionMatches(module, result, contract->details->resultArrayItemAbi, XL6_IR_COLLECTION_ARRAY,NULL)) return 0;
        const XL6_IrType* item = xl6IrTypeGet(&module->types, result->params[0]);
        XL6_IrSequenceLayout expected = xl6IrSequenceLayoutForType(&module->types, result);
        if (!item || !item->typeValueId || expected.abi != XL6_IR_SEQUENCE_LAYOUT_PRIMITIVE_V2 ||
            expected.nullable || expected.elementType != result->params[0] ||
            !xl6IrSequenceLayoutEqual(&result->sequenceLayout, &expected)) return 0;
    }
    if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE ||
        contract->kind == XL6_IR_RUNTIME_BRIDGE_PRINT) {
        const XL6_IrNode* declaration = xl6IrNodeGet(module, target->declaration);
        if (target->effectAbi != (contract->details->pure ? XL6_IR_CALL_EFFECT_PURE : XL6_IR_CALL_EFFECT_CONSERVATIVE) ||
            target->executionBoundaryAbi != (contract->details->nonblocking
                ? XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING : XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK) ||
            !declaration || (!contract->details->noRaise && (declaration->flags & XL6_AST_FLAG_FOREIGN_NO_RAISE))) return 0;
        if (contract->result == XL6_IR_C_ABI_I64 && (!result ||
            result->integerTarget != (contract->details->resultIntegerTarget ? contract->details->resultIntegerTarget
                : XL6_IR_INTEGER_TARGET_SIGNED_64))) return 0;
        if (contract->result == XL6_IR_C_ABI_U64 && (!result ||
            result->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64)) return 0;
    }
    if (contract->details->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1) {
        if(!result || target->returnType!=xl6IrNativeContextElementType(module,target) ||
            contract->details->resultOwner || contract->details->noRaise || contract->result ||
            target->errorPhysicalAbi==XL6_IR_CALL_ERROR_PHYSICAL_NONE)return 0;
    } else if (contract->result == XL6_IR_C_ABI_VOID) {
        if (target->returnType != XL6_IR_TYPE_INVALID) return 0;
    } else if (!contract->details->resultOwner) {
        const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
        if (declaration && (declaration->flags & XL6_AST_FLAG_FOREIGN_OWNED_STRING)) {
            if (!xl6IrRuntimeDirectBridgeOwnsString(target->cName) || !result ||
                result->kind!=XL6_IR_TYPE_BUILTIN || result->cAbi!=XL6_IR_C_ABI_MANAGED_STRING ||
                result->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED) return 0;
        } else if(contract->details->resultEnum) {
            if(contract->details->nullableResult||contract->details->resultProductAbi||contract->details->resultArrayItemAbi||
                contract->details->resultFutureElementAbi||contract->details->resultFutureAbi||
                contract->details->resultNativeValueAbi||contract->details->resultEnum->cAbi!=contract->result||
                !xl6IrDirectNumericEnumMatches(module,target->returnType,contract->details->resultEnum)||
                !xl6IrBufferRuntimeBridgeResultMatches(result,contract->result))return 0;
        } else if (contract->details->resultNativeValueAbi) {
            if (contract->result != XL6_IR_C_ABI_NATIVE_VALUE ||
                !xl6IrNativeValuePlanForType(result) ||
                result->nativeValueAbi != contract->details->resultNativeValueAbi ||
                contract->details->nullableResult || contract->details->resultProductAbi ||
                contract->details->resultArrayItemAbi || contract->details->resultFutureElementAbi || contract->details->resultFutureAbi) return 0;
        } else if (contract->details->resultProductAbi) {
            if (!result || result->cAbi != XL6_IR_C_ABI_PRODUCT ||
                result->nativeProductAbi != contract->details->resultProductAbi ||
                xl6IrNativeProductAbiForShape(&module->types,result) != contract->details->resultProductAbi ||
                result->ownershipAbi != xl6IrNativeProductOwnershipAbi(contract->details->resultProductAbi) ||
                result->nullAbi != XL6_IR_NULL_ABI_NONE ||
                strcmp(result->cName,xl6IrNativeProductAbiCName(contract->details->resultProductAbi)) != 0) return 0;
            for(size_t p=0;p<3;++p){
                const XL6_IrNumericEnumContract* expected=contract->details->resultProductEnums[p];
                if(expected){
                    if(p>=result->paramCount||!xl6IrDirectNumericEnumMatches(module,result->params[p],expected))return 0;
                }else if(p<result->paramCount){
                    const XL6_IrType* item=xl6IrTypeGet(&module->types,result->params[p]);
                    if(!item||item->kind!=XL6_IR_TYPE_BUILTIN)return 0;
                }
            }
        } else if(contract->details->genericFutureResult) {
            if(!result || result->kind!=XL6_IR_TYPE_GENERIC_INSTANCE ||
               result->cAbi!=XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
               result->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
               result->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
               result->collectionAbi!=XL6_IR_COLLECTION_NONE || result->returnCount ||
               result->paramCount!=1 || !result->params || result->params[0]!=target->nativeContextType)return 0;
        } else if (contract->details->resultFutureElementAbi || contract->details->resultFutureAbi || contract->details->resultFutureEnum) {
            const XL6_IrType* element = result && result->paramCount == 1 && result->params
                ? xl6IrTypeGet(&module->types, result->params[0]) : NULL;
            if (contract->result != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR || !result ||
                result->kind != XL6_IR_TYPE_GENERIC_INSTANCE ||
                result->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
                result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                result->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                result->collectionAbi != XL6_IR_COLLECTION_NONE || result->returnCount ||
                !element || element->paramCount || element->returnCount ||
                element->collectionAbi != XL6_IR_COLLECTION_NONE) return 0;
            if(contract->details->resultFutureEnum){
                XL6_IrAsyncResultAbi payload=contract->details->resultFutureElementAbi==XL6_IR_C_ABI_I64?
                    XL6_IR_ASYNC_RESULT_COPY_I64:contract->details->resultFutureElementAbi==XL6_IR_C_ABI_U64?
                    XL6_IR_ASYNC_RESULT_COPY_U64:XL6_IR_ASYNC_RESULT_INVALID;
                if(contract->details->resultFutureAbi||contract->details->nullableResult||
                    contract->details->resultFutureElementAbi!=contract->details->resultFutureEnum->cAbi||
                    !xl6IrDirectNumericEnumMatches(module,result->params[0],contract->details->resultFutureEnum)||
                    payload==XL6_IR_ASYNC_RESULT_INVALID||
                    xl6IrBuildAsyncResultAbi(module,result->params[0])!=payload)return 0;
            }else if(element->kind!=XL6_IR_TYPE_BUILTIN)return 0;
            else if(contract->details->resultFutureAbi){
                /* A managed codec alone may describe several builtins. An
                 * explicit element C ABI additionally seals its logical
                 * family (for example bytes, not arbitrary xvalue). */
                if((contract->details->resultFutureElementAbi&&element->cAbi!=contract->details->resultFutureElementAbi) ||
                    xl6IrBuildAsyncResultAbi(module,result->params[0])!=contract->details->resultFutureAbi)return 0;
            }else if(element->cAbi!=contract->details->resultFutureElementAbi ||
                element->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
                element->nullAbi!=XL6_IR_NULL_ABI_NONE ||
                (contract->details->resultFutureElementAbi==XL6_IR_C_ABI_I64 && element->integerTarget!=
                    (contract->details->resultIntegerTarget?contract->details->resultIntegerTarget:XL6_IR_INTEGER_TARGET_SIGNED_64)) ||
                (contract->details->resultFutureElementAbi==XL6_IR_C_ABI_U64 && element->integerTarget!=
                    XL6_IR_INTEGER_TARGET_UNSIGNED_64))return 0;
        } else if (contract->details->resultArrayItemAbi) {
            if (!xl6IrDirectBridgeCollectionMatches(module, result, contract->details->resultArrayItemAbi,
                contract->details->resultCollectionAbi ? contract->details->resultCollectionAbi : XL6_IR_COLLECTION_ARRAY,NULL)) return 0;
        } else {
            if (result == NULL || result->kind != (contract->details->nullableResult ? XL6_IR_TYPE_NULLABLE : XL6_IR_TYPE_BUILTIN) ||
                result->collectionAbi != XL6_IR_COLLECTION_NONE || result->returnCount != 0 ||
                result->paramCount != (contract->details->nullableResult ? 1 : 0)) return 0;
            if (contract->details->nullableResult) {
                const XL6_IrType* base = result->params ? xl6IrTypeGet(&module->types, result->params[0]) : NULL;
                if (!base || base->kind != XL6_IR_TYPE_BUILTIN || base->cAbi != contract->result ||
                    base->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED || base->paramCount ||
                    base->nullAbi != (contract->result==XL6_IR_C_ABI_MANAGED_BYTES
                        ? XL6_IR_NULL_ABI_XVALUE_SENTINEL : XL6_IR_NULL_ABI_POINTER_SENTINEL)) return 0;
            }
            if (contract->result == XL6_IR_C_ABI_MANAGED_STRING || contract->result == XL6_IR_C_ABI_MANAGED_BYTES) {
                if (result->cAbi != contract->result || result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                    result->nullAbi != (contract->result == XL6_IR_C_ABI_MANAGED_STRING
                        ? XL6_IR_NULL_ABI_POINTER_SENTINEL : XL6_IR_NULL_ABI_XVALUE_SENTINEL)) return 0;
            } else if (contract->result == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) {
                if (!xl6IrSerializationBridgeDynamicValueType(result)) return 0;
            } else if (!xl6IrBufferRuntimeBridgeResultMatches(result, contract->result)) return 0;
        }
    }
    for (i = 0; i < contract->parameterCount; ++i) {
        const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[i]);
        XL6_IrCAbi abi = contract->parameters[i].abi;
        if(abi==XL6_IR_C_ABI_C_BYTES || abi==XL6_IR_C_ABI_C_STRING) {
            if(!xl6IrTypeIsCOwned(type) || type->kind!=XL6_IR_TYPE_BUILTIN || type->cAbi!=abi ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_C_SPAN_BORROW ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->parameters[i].numericEnum) {
            if(contract->parameters[i].numericEnum->cAbi!=abi||
                !xl6IrDirectNumericEnumMatches(module,target->parameterTypes[i],contract->parameters[i].numericEnum)||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID||
                !xl6IrBufferRuntimeBridgeParameterMatches(module,target,i,abi,
                    XL6_IR_PARAMETER_ABI_VALUE,XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT))return 0;
            continue;
        }
        if(contract->details->genericValueParameter==i+1) {
            if(!type || target->parameterTypes[i]!=xl6IrNativeContextElementType(module,target) ||
               target->parameterAbis[i]!=xl6IrParameterAbiForMode(type,XL6_IR_PARAMETER_MODE_DEFAULT) ||
               target->parameterPhysicalAbis[i]!=xl6IrNativeValueParameterPhysicalAbi(module,target->parameterTypes[i]) ||
               target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
               target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->details->genericChannelParameter==i+1) {
            if(!type || target->parameterTypes[i]!=target->nativeContextType ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->details->nativeVoidCallbackArrayParameter==i+1){
            if(!type||target->parameterTypes[i]!=target->nativeContextType||
                !xl6IrNativeVoidCallbackArrayType(module,target,contract)||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->details->nativeClassArrayParameter==i+1) {
            if(!type || target->parameterTypes[i]!=target->nativeContextType ||
                !xl6IrNativeClassArrayContextField(module,target) ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->details->nativeClassParameter==i+1) {
            if(!type || target->parameterTypes[i]!=target->nativeContextType ||
                !xl6IrNativeClassContextField(module,target) ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if (contract->parameters[i].nativeValueAbi) {
            if (abi != XL6_IR_C_ABI_NATIVE_VALUE || !xl6IrNativeValuePlanForType(type) ||
                type->nativeValueAbi != contract->parameters[i].nativeValueAbi ||
                target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
                contract->parameters[i].owner || contract->parameters[i].dynamicBorrow ||
                contract->parameters[i].arrayItemAbi || contract->parameters[i].nullaryCallableResultAbi) return 0;
            continue;
        }
        if(contract->details->nativeClassCallbackParameter==i+1){
            XL6_IrNativeClassCallbackPlan plan={0};
            if(!xl6IrNativeClassCallbackPlanBuild(module,target,&plan)||target->nativeContextType!=plan.signature||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->details->genericFutureValueCallbackParameter==i+1){
            const XL6_IrType* future=xl6IrTypeGet(&module->types,target->returnType);
            XL6_IrTypeId element=future&&future->paramCount==1&&future->params?future->params[0]:XL6_IR_TYPE_INVALID;
            int returnsValue=type&&type->returnCount==1&&type->returns&&type->returns[0]==element&&
                type->callableResultType==element;
            int returnsVoid=type&&!type->returnCount&&!type->returns&&!type->callableResultType&&
                target->nativeContextPayloadAbi==XL6_IR_ASYNC_RESULT_VOID;
            if(!element||target->nativeContextType!=element||!type||type->kind!=XL6_IR_TYPE_FUNCTION||
                type->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR||type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||
                type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL||!type->typeValueId||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE||type->paramCount||type->params||type->parameterAbis||
                type->parameterModes||type->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED||
                (!returnsValue&&!returnsVoid)||target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_RESULT_CLOSURE_BORROW||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if (contract->parameters[i].nullaryCallableResultAbi || contract->details->genericFutureStarterParameter==i+1) {
            XL6_IrCAbi resultAbi = contract->parameters[i].nullaryCallableResultAbi;
            const XL6_IrType* returned=type && type->returnCount==1 && type->returns
                ? xl6IrTypeGet(&module->types,type->returns[0]):NULL;
            int resultMatches = contract->details->genericFutureStarterParameter==i+1
                ? returned && type->callableResultType==target->returnType &&
                    type->returns[0]==target->returnType && !contract->parameters[i].nullaryCallableResultAbi
                : resultAbi == XL6_IR_C_ABI_VOID
                ? type && !type->returnCount && !type->returns && type->callableResultType == XL6_IR_TYPE_INVALID
                : returned && type->callableResultType == type->returns[0] && returned->kind == XL6_IR_TYPE_BUILTIN &&
                  returned->cAbi == resultAbi && !returned->paramCount && !returned->returnCount &&
                  returned->collectionAbi == XL6_IR_COLLECTION_NONE &&
                  (resultAbi == XL6_IR_C_ABI_BOOL
                    ? returned->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
                      returned->nullAbi == XL6_IR_NULL_ABI_NONE &&
                      returned->integerTarget == XL6_IR_INTEGER_TARGET_INVALID
                    : resultAbi == XL6_IR_C_ABI_MANAGED_STRING &&
                      returned->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                      returned->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL);
            if (!type || type->kind!=XL6_IR_TYPE_FUNCTION || type->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR ||
                type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED || type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE || type->paramCount || type->params ||
                type->parameterAbis || type->parameterModes || type->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED ||
                !resultMatches ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (abi==XL6_IR_C_ABI_RUNTIME_ERROR_PTR) {
            if (!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->cAbi!=abi || type->paramCount || type->returnCount ||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
                type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL || target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (contract->parameters[i].owner) {
            if (target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                !xl6IrDirectBridgeResourceMatches(module, target->parameterTypes[i],
                    target->parameterPhysicalFieldDeclarations[i], 0, contract->parameters[i].owner)) return 0;
            continue;
        }
        if (contract->parameters[i].dynamicBorrow) {
            if (abi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || !xl6IrSerializationBridgeDynamicValueType(type) ||
                target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (contract->parameters[i].arrayItemAbi) {
            if (!xl6IrDirectBridgeCollectionMatches(module, type, contract->parameters[i].arrayItemAbi, XL6_IR_COLLECTION_ARRAY,contract->parameters[i].arrayItemEnum) ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
                !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi,
                    XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
            continue;
        }
        if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && abi == XL6_IR_C_ABI_I64 &&
            (!type || type->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64)) return 0;
        if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && abi == XL6_IR_C_ABI_U64 &&
            (!type || type->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64)) return 0;
        if (type == NULL || type->kind != XL6_IR_TYPE_BUILTIN ||
            type->collectionAbi != XL6_IR_COLLECTION_NONE || type->paramCount != 0 ||
            target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
            !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi,
                (abi == XL6_IR_C_ABI_MANAGED_STRING || abi == XL6_IR_C_ABI_MANAGED_BYTES)
                    ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE,
                XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
    }
    return 1;
}
