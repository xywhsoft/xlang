/* Private IR contract test, not a public source-language or native Cgen gate.
 * Real source pools and cleanup plans; exact typed calls are retargeted only
 * in the test before actual lowering and the whole-module verifier run. */
#include "src/ir/xl6_ir.c"
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/runtime/xl6_channel.h"
#include <assert.h>
#include <stdio.h>

static unsigned plans,mutations,dependent,pipeline;
static void diagnostics(const XL6_DiagList* diags)
{
    for(size_t i=0;i<diags->count;++i)
        fprintf(stderr,"%d:%d %s\n",diags->items[i].line,diags->items[i].column,diags->items[i].message);
}
static XL6_IrTypeId resultFor(const XL6_IrModule* ir,XL6_IrTypeId owner)
{
    for(size_t i=0;i<ir->types.count;++i)
        if(xl6IrChannelReceivePlanBuild(ir,XL6_IR_CHANNEL_RECEIVE_TRY,owner,(XL6_IrTypeId)(i+1),NULL))
            return (XL6_IrTypeId)(i+1);
    return 0;
}
static void prepareResults(XL6_IrModule* ir)
{
    /* Uncalled declarations have return TypeIds but no callable TypeId yet.
     * Intern their product with the real frontend factory, not a fake ABI. */
    for(size_t i=0;i<ir->nodeCount;++i) {
        const XL6_IrNode* function=&ir->nodes[i];
        if(function->kind!=XL6_IR_NODE_FUNC_DECL || function->childCount<3) continue;
        const XL6_IrNode* returns=xl6IrNodeGet(ir,ir->children[function->firstChild+2]);
        if(!returns || returns->childCount!=2) continue;
        XL6_IrTypeId items[2];
        for(size_t j=0;j<2;++j) items[j]=xl6IrNodeGet(ir,ir->children[returns->firstChild+j])->type;
        assert(xl6IrTypeInternTuple(ir,items,2));
    }
}
static void instructionRefused(const XL6_IrModule* ir,const XL6_IrInstruction* ins)
{
    XL6_IrChannelReceivePlan plan,saved; memset(&plan,0xA5,sizeof(plan)); memcpy(&saved,&plan,sizeof(saved));
    assert(!xl6IrChannelReceiveInstructionPlan(ir,ins,&plan));
    assert(!memcmp(&saved,&plan,sizeof(plan))); ++mutations;
}
static void closedPlans(const XL6_IrModule* ir)
{
    for(size_t n=0;n<ir->types.count;++n) {
        XL6_IrTypeId owner=(XL6_IrTypeId)(n+1),result=resultFor(ir,owner);
        if(!result) continue;
        for(unsigned k=XL6_IR_CHANNEL_RECEIVE_WAIT;k<=XL6_IR_CHANNEL_RECEIVE_FOR;++k) {
            XL6_IrChannelReceivePlan plan={0};
            assert(xl6IrChannelReceivePlanBuild(ir,(XL6_IrChannelReceiveKind)k,owner,result,&plan));
            const XL6_IrChannelReceiveSpec* spec=plan.spec;
            assert(xl6IrChannelReceiveSpecResource(spec->source)==spec && xl6IrChannelReceiveSpecRuntime(spec->runtime)==spec);
            assert(xl6IrChannelReceiveErrorPhysicalAbi(spec->runtime)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
            assert(xl6IrRuntimeResourceOpIsChannel(spec->source) && xl6IrRuntimeResourceOpMayRaise(spec->source));
            assert(!xl6IrRuntimeResourceStorageTargetType(spec->source,1,plan.elementType));
            assert(xl6IrRuntimeResourceErrorAbiForOwnerType(ir,spec->source,owner)==XL6_IR_ERROR_ABI_MAY_RAISE);
            XL6_IrTypeId status=xl6IrTypeGet(&ir->types,result)->params[0],types[2]={owner,status};
            assert(xl6IrRuntimeResourceSignatureCompatible(ir,spec->source,owner,result,types,spec->timed ? 2 : 1));
            assert(strstr(xl6IrRuntimeResourceOpName(spec->source),"channel-owned-")==xl6IrRuntimeResourceOpName(spec->source));
            assert(!strcmp(xl6IrRuntimeResourceOpName(spec->source),xl6IrRuntimeOpName(spec->runtime)));
            XL6_IrModule fake=*ir; XL6_IrValueId operands[2]={1,2};
            XL6_IrInstruction definitions[3]={{.kind=XL6_IR_INSTR_CONST,.type=owner,.result=1},
                {.kind=XL6_IR_INSTR_CONST,.type=status,.result=2},
                {.kind=XL6_IR_INSTR_RUNTIME_CALL,.runtimeOp=spec->runtime,.result=3,.type=result,
                 .operandCount=spec->timed ? 2u : 1u,.payloadType=plan.elementType,.runtimeResourcePayloadAbi=plan.transferAbi}};
            fake.instructions=definitions; fake.instructionCount=3; fake.operands=operands; fake.operandCount=2;
            XL6_IrInstruction* ins=&definitions[2]; XL6_IrInstruction saved=*ins;
            XL6_IrChannelReceivePlan actual={0}; assert(xl6IrChannelReceiveInstructionPlan(&fake,ins,&actual));
            assert(actual.spec==spec && actual.ownerType==owner && actual.schemaType==plan.schemaType &&
                actual.elementType==plan.elementType && actual.resultValueType==plan.resultValueType &&
                actual.transferAbi==plan.transferAbi && actual.slotAbi==plan.slotAbi && actual.valueTagged==plan.valueTagged);
            assert(xl6IrCallInstructionErrorPhysicalAbi(&fake,ins)==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
            assert(xl6IrInstructionOperandOwnership(&fake,ins,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
            ++plans;
#define BAD_INS(Field,Value) do { ins->Field=(Value); instructionRefused(&fake,ins); *ins=saved; } while(0)
            BAD_INS(kind,XL6_IR_INSTR_CONST); BAD_INS(result,0); BAD_INS(type,plan.elementType);
            BAD_INS(runtimeOp,XL6_IR_RUNTIME_CHANNEL_RECV_INTO);
            BAD_INS(runtimeOp,XL6_IR_RUNTIME_COUNT); BAD_INS(operandCount,0); BAD_INS(operandCount,3);
            BAD_INS(firstOperand,SIZE_MAX); BAD_INS(payloadType,owner);
            BAD_INS(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
            BAD_INS(immediateText,"bad"); BAD_INS(immediateData,(unsigned char*)"bad"); BAD_INS(immediateSize,1);
            BAD_INS(opcode,XL6_IR_OPCODE_POS); BAD_INS(targetSymbol,1); BAD_INS(targetValue,1);
            BAD_INS(calleeType,owner); BAD_INS(runtimeIdentityType,owner); BAD_INS(sequenceAdmissionIdentitySet,1);
            BAD_INS(runtimeResourceKeyType,status); BAD_INS(runtimeResourceKeyPayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY);
            BAD_INS(runtimeResourceKeyOrderAbi,XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_SIGNED_I64); BAD_INS(collectionAbi,XL6_IR_COLLECTION_ARRAY);
#undef BAD_INS
            operands[0]=2; instructionRefused(&fake,ins); operands[0]=1;
            if(spec->timed) { operands[1]=1; instructionRefused(&fake,ins); operands[1]=2; }
            XL6_IrIntrinsicOperand args[2]={{.expression=1,.type=owner,.abi=XL6_IR_INTRINSIC_OPERAND_BORROW},
                {.expression=2,.type=status,.abi=XL6_IR_INTRINSIC_OPERAND_VALUE}};
            XL6_IrIntrinsicCall call={.kind=XL6_IR_INTRINSIC_RUNTIME_RESOURCE,.admission=XL6_IR_INTRINSIC_ADMISSION_VERIFIED,
                .receiver=1,.receiverType=owner,.resultType=result,.runtimeResourceOp=spec->source,
                .operandCount=spec->timed ? 2u : 1u,.payloadType=plan.elementType,.runtimeResourcePayloadAbi=plan.transferAbi,
                .resultAbi=xl6IrRuntimeResourceResultAbiForType(spec->source,xl6IrTypeGet(&ir->types,result)),
                .errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE},original=call;
            assert(xl6IrChannelReceiveIntrinsicCompatible(ir,&call,args));
            assert(!xl6IrChannelReceiveIntrinsicCompatible(ir,&call,NULL)); ++mutations;
#define BAD_CALL(Field,Value) do { call.Field=(Value); assert(!xl6IrChannelReceiveIntrinsicCompatible(ir,&call,args)); call=original; ++mutations; } while(0)
            BAD_CALL(kind,XL6_IR_INTRINSIC_INVALID); BAD_CALL(admission,XL6_IR_INTRINSIC_ADMISSION_GENERIC_DEPENDENT);
            BAD_CALL(receiver,2); BAD_CALL(receiverType,status); BAD_CALL(resultType,owner);
            BAD_CALL(runtimeResourceOp,XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_RECV_INTO); BAD_CALL(operandCount,3);
            BAD_CALL(payloadType,owner); BAD_CALL(runtimeResourcePayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
            BAD_CALL(errorAbi,XL6_IR_ERROR_ABI_NO_RAISE);
            BAD_CALL(resultAbi,call.resultAbi==XL6_IR_INTRINSIC_RESULT_VALUE ? XL6_IR_INTRINSIC_RESULT_OWNED : XL6_IR_INTRINSIC_RESULT_VALUE);
            BAD_CALL(runtimeResourceKeyType,status); BAD_CALL(runtimeResourceKeyPayloadAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY);
            BAD_CALL(runtimeResourceKeyOrderAbi,XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_SIGNED_I64);
            BAD_CALL(collectionAbi,XL6_IR_COLLECTION_ARRAY); BAD_CALL(integerTarget,XL6_IR_INTEGER_TARGET_SIGNED_64);
#undef BAD_CALL
            args[0].abi=XL6_IR_INTRINSIC_OPERAND_STORAGE_REF;
            assert(!xl6IrChannelReceiveIntrinsicCompatible(ir,&call,args)); ++mutations;
        }
    }
    assert(plans>=39);
}
static void dependentPlans(const XL6_IrModule* ir)
{
    for(size_t i=0;i<ir->nodeCount;++i) {
        const XL6_IrNode* node=&ir->nodes[i];
        const XL6_IrNode* name=node->kind==XL6_IR_NODE_FUNC_DECL && node->childCount
            ? xl6IrNodeGet(ir,ir->children[node->firstChild]) : NULL;
        if(!name || !name->text ||
            (strcmp(name->text,"pendingReceive") && strcmp(name->text,"pendingNullableReceive"))) continue;
        const XL6_IrNode* parameters=xl6IrNodeGet(ir,ir->children[node->firstChild+1]);
        const XL6_IrNode* returns=xl6IrNodeGet(ir,ir->children[node->firstChild+2]);
        assert(parameters && parameters->childCount==1 && returns && returns->childCount==2);
        XL6_IrTypeId owner=xl6IrNodeGet(ir,ir->children[parameters->firstChild])->storageType;
        XL6_IrTypeId items[2]={xl6IrNodeGet(ir,ir->children[returns->firstChild])->type,
            xl6IrNodeGet(ir,ir->children[returns->firstChild+1])->type};
        XL6_IrTypeId result=xl6IrTypeFindTupleForItems(&ir->types,items,2); assert(result);
        const XL6_IrType* tuple=xl6IrTypeGet(&ir->types,result);
        XL6_IrTypeId args[2]={owner,tuple->params[0]};
        assert(xl6IrChannelReceivePayloadType(ir,owner));
        for(unsigned k=XL6_IR_CHANNEL_RECEIVE_WAIT;k<=XL6_IR_CHANNEL_RECEIVE_FOR;++k) {
            const XL6_IrChannelReceiveSpec* spec=xl6IrChannelReceiveSpec((XL6_IrChannelReceiveKind)k);
            size_t count=spec->timed ? 2u : 1u;
            assert(xl6IrChannelReceiveDependentSignatureCompatible(ir,(XL6_IrNodeId)(i+1),spec->source,owner,result,args,count)); ++dependent;
            assert(!xl6IrChannelReceiveSignatureCompatible(ir,spec->kind,owner,result,args,count));
            assert(!xl6IrChannelReceiveDependentSignatureCompatible(ir,ir->root,spec->source,owner,result,args,count));
            assert(!xl6IrChannelReceiveDependentSignatureCompatible(ir,(XL6_IrNodeId)(i+1),spec->source,owner,result,NULL,count));
            assert(!xl6IrChannelReceiveDependentSignatureCompatible(ir,(XL6_IrNodeId)(i+1),spec->source,owner,result,args,count+1));
            assert(!xl6IrChannelReceiveDependentSignatureCompatible(ir,(XL6_IrNodeId)(i+1),spec->source,owner,owner,args,count)); mutations+=5;
        }
    }
    assert(dependent==6);
}
static void preserveUnusedArgument(XL6_IrModule* ir,XL6_IrNodeId argument,XL6_IrNodeId statement)
{
    /* Keep the fixture's removed send argument as an unreachable expression
     * after its return. No node is orphaned and no source node ID is shifted. */
    XL6_IrNode wrapper=ir->nodes[statement-1]; XL6_IrNodeId body=wrapper.parent;
    if(wrapper.text) { wrapper.text=xrtStrDupN(wrapper.text,strlen(wrapper.text)); assert(wrapper.text); }
    assert(xl6IrGrow((void**)&ir->nodes,&ir->nodeCapacity,ir->nodeCount,sizeof(*ir->nodes)));
    size_t first=ir->childCount,bodyCount=ir->nodes[body-1].childCount;
    size_t end=first+bodyCount+2;
    XL6_IrNodeId* edges=xrtRealloc(ir->children,end*sizeof(*edges)); assert(edges);
    ir->children=edges; ir->childCapacity=ir->childCount=end;
    edges[first]=argument; memcpy(edges+first+1,edges+ir->nodes[body-1].firstChild,bodyCount*sizeof(*edges));
    XL6_IrNodeId added=(XL6_IrNodeId)(ir->nodeCount+1); edges[end-1]=added;
    wrapper.firstChild=first; wrapper.childCount=1;
    ir->nodes[ir->nodeCount++]=wrapper; ir->nodes[argument-1].parent=added;
    ir->nodes[body-1].firstChild=first+1; ir->nodes[body-1].childCount=bodyCount+1;
}
static void lowerPipeline(XL6_CompileUnit* unit)
{
    XL6_IrModule* ir=&unit->ir;
    prepareResults(ir);
    for(size_t i=0;i<ir->intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* call=&ir->intrinsicCalls[i];
        if(call->kind!=XL6_IR_INTRINSIC_RUNTIME_RESOURCE || call->runtimeResourceOp!=XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_TRY_SEND) continue;
        XL6_IrNode* source=&ir->nodes[call->origin-1];
        XL6_IrNode* declaration=source;
        while(declaration->kind!=XL6_IR_NODE_FUNC_DECL) declaration=&ir->nodes[declaration->parent-1];
        const char* name=xl6IrNodeGet(ir,ir->children[declaration->firstChild])->text; assert(name);
        size_t length=strlen(name);
        const XL6_IrChannelReceiveSpec* spec=xl6IrChannelReceiveSpec(
            !strcmp(name+length-4,"Wait") ? XL6_IR_CHANNEL_RECEIVE_WAIT :
            !strcmp(name+length-3,"For") ? XL6_IR_CHANNEL_RECEIVE_FOR : XL6_IR_CHANNEL_RECEIVE_TRY);
        XL6_IrTypeId result=resultFor(ir,call->receiverType); assert(result);
        XL6_IrNode* callee=&ir->nodes[ir->children[source->firstChild]-1];
        XL6_IrNode* args=&ir->nodes[ir->children[source->firstChild+1]-1];
        XL6_IrNodeId removed=spec->timed ? 0 : ir->children[args->firstChild],statement=source->parent;
        xrtFree(callee->text); callee->text=xrtStrDupN(spec->name,strlen(spec->name)); assert(callee->text);
        args->childCount=spec->timed ? 1u : 0u;
        source->type=source->storageType=result; source->ownership=XL6_IR_OWNERSHIP_MOVE; source->ownershipPlanned=1;
        call->resultType=result; call->resultAbi=xl6IrRuntimeResourceResultAbiForType(spec->source,xl6IrTypeGet(&ir->types,result));
        call->runtimeResourceOp=spec->source; call->operandCount=spec->timed ? 2u : 1u;
        XL6_IrIntrinsicOperand* operands=&ir->intrinsicOperands[call->firstOperand];
        assert(xl6IrChannelReceiveIntrinsicCompatible(ir,call,operands));
        if(removed) preserveUnusedArgument(ir,removed,statement);
        ++pipeline;
    }
    assert(pipeline==7);
    size_t cursor=0;
    for(size_t i=0;i<ir->intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* call=&ir->intrinsicCalls[i];
        memmove(ir->intrinsicOperands+cursor,ir->intrinsicOperands+call->firstOperand,
            call->operandCount*sizeof(*ir->intrinsicOperands));
        call->firstOperand=cursor; cursor+=call->operandCount;
    }
    ir->intrinsicOperandCount=cursor;
    assert(xl6IrBuildOwnedReadResults(ir) && xl6IrRefreshExitPlans(ir));
    memset(&ir->stats,0,sizeof(ir->stats)); ir->stats.nodeCount=ir->nodeCount; ir->stats.typeCount=ir->types.count;
    for(size_t i=0;i<ir->nodeCount;++i) {
        const XL6_IrNode* node=&ir->nodes[i]; XL6_IrNodeId id=(XL6_IrNodeId)(i+1);
        ir->stats.typedValueNodeCount+=node->valueCategory!=XL6_IR_VALUE_NONE;
        ir->stats.resolvedCallCount+=node->callKind!=XL6_SEM_CALL_NONE;
        ir->stats.ownershipPlanCount+=node->ownershipPlanned!=0;
        ir->stats.exitPlanCount+=xl6IrGeneratorNodeNeedsClosePlan(ir,id);
        if(xl6IrNodeRequiresAnyExitPlan(ir,id))
            ir->stats.exitPlanCount+=1+(node->kind==XL6_IR_NODE_BLOCK ? 1 : node->kind==XL6_IR_NODE_LOOP ? 2 : 0)+
                xl6IrNodeHasDistinctEvaluationFailure(ir,id);
    }
    if(!xl6IrVerifyModule(ir,&unit->parse.diags) || !xl6IrLowerModule(ir,&unit->parse.diags)) {
        for(size_t i=0;i<ir->instructionCount && i<16;++i) {
            const XL6_IrInstruction* ins=&ir->instructions[i];
            fprintf(stderr,"i%lu k%d op%d v%u type%u owner%d argc%lu\n",(unsigned long)i,(int)ins->kind,
                (int)ins->runtimeOp,(unsigned)ins->result,(unsigned)ins->type,
                (int)xl6IrValueOwnershipGet(ir,ins->result),(unsigned long)ins->operandCount);
        }
        diagnostics(&unit->parse.diags); assert(false);
    }
    assert(xl6IrVerifyModule(ir,NULL)); unsigned found=0;
    for(size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if(!xl6IrChannelReceiveSpecRuntime(ins->runtimeOp)) continue;
        assert(xl6IrChannelReceiveInstructionPlan(ir,ins,NULL)); ++found;
        assert(i+1<ir->instructionCount && ir->instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
        XL6_IrTypeId saved=ins->payloadType; ins->payloadType=0;
        assert(!xl6IrVerifyModule(ir,NULL)); ins->payloadType=saved;
        XL6_IrInstruction* outcome=&ir->instructions[i+1]; XL6_IrInstructionKind kind=outcome->kind;
        outcome->kind=XL6_IR_INSTR_CONST; assert(!xl6IrVerifyModule(ir,NULL)); outcome->kind=kind; mutations+=2;
    }
    assert(found==pipeline && xl6IrVerifyModule(ir,NULL));
    /* Distinct new operations must not silently select the old Into emitter
     * while public/native Cgen integration is still pending. */
    XL6_DiagList unsupported; xl6DiagInit(&unsupported);
    assert(!xl6IrCgenVerifyEmittable(ir,&unsupported) && unsupported.count);
    bool blocked=false;
    for(size_t i=0;i<unsupported.count;++i)
        if(strstr(unsupported.items[i].message,"runtime operation ABI is unresolved")) blocked=true;
    assert(blocked);
    xl6DiagFree(&unsupported);
}
int main(int argc,char** argv)
{
    assert(argc==3); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1)) { diagnostics(&unit.parse.diags); return 1; }
    prepareResults(&unit.ir); closedPlans(&unit.ir); dependentPlans(&unit.ir); xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[2],1)) { diagnostics(&unit.parse.diags); return 1; }
    lowerPipeline(&unit); xl6CompileUnitFree(&unit);
    printf("Channel receive IR plans=%u dependent=%u pipelines=%u mutations=%u\n",plans,dependent,pipeline,mutations);
    return 0;
}
