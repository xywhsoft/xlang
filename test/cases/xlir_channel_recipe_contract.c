/* Real source type pool, shared owning result plan and actual Cgen schema
 * factory. This prerequisite is NOT public recv language/backend acceptance. */
#include "src/cgen/xl6_ir_cgen.c"
#include "src/compile/xl6_compile_unit.h"
#include "src/runtime/xl6_channel.h"
#include <assert.h>
#include <stdio.h>

static unsigned mutations;
static void refused(const XL6_IrModule* ir,XL6_IrTypeId id)
{
    XL6_IrTypeId schema=123; XL6_IrNominalIdentitySetId set=456;
    if(xl6IrChannelConstructionRecipe(ir,id,&schema,&set)) {
        const XL6_IrType* owner=xl6IrTypeGet(&ir->types,id);
        const XL6_IrType* domain=xl6IrTypeGet(&ir->types,schema);
        fprintf(stderr,"Unexpected recipe owner=%u name=%s slot=%u transfer=%u admission=%u nullable=%u\n",
            (unsigned)id,owner->name,(unsigned)domain->sequenceLayout.slotAbi,
            (unsigned)domain->sequenceLayout.transferAbi,(unsigned)domain->sequenceLayout.admissionAbi,
            (unsigned)domain->sequenceLayout.nullable);
        assert(false);
    }
    assert(schema==123 && set==456); ++mutations;
}
static void planRefused(const XL6_IrModule* ir,XL6_IrChannelReceiveKind kind,
    XL6_IrTypeId owner,XL6_IrTypeId result)
{
    XL6_IrChannelReceivePlan plan,saved;
    memset(&plan,0xA5,sizeof(plan)); memcpy(&saved,&plan,sizeof(saved));
    if(xl6IrChannelReceivePlanBuild(ir,kind,owner,result,&plan)) {
        fprintf(stderr,"Unexpected receive plan owner=%s result=%s element=%s transfer=%u\n",
            xl6IrTypeGet(&ir->types,owner)->name,xl6IrTypeGet(&ir->types,result)->name,
            xl6IrTypeGet(&ir->types,plan.elementType)->name,(unsigned)plan.transferAbi);
        assert(false);
    }
    assert(!memcmp(&plan,&saved,sizeof(plan))); ++mutations;
}
static XL6_IrTypeId resultFor(const XL6_IrModule* ir,XL6_IrTypeId owner)
{
    for(size_t i=0;i<ir->types.count;++i)
        if(xl6IrChannelReceivePlanBuild(ir,XL6_IR_CHANNEL_RECEIVE_TRY,owner,(XL6_IrTypeId)(i+1),NULL))
            return (XL6_IrTypeId)(i+1);
    return 0;
}
static void nominalSets(XL6_Output* out,const XL6_IrModule* ir)
{
    for(size_t i=0;i<ir->nominalIdentitySetCount;++i) {
        const XL6_IrNominalIdentitySet* set=&ir->nominalIdentitySets[i];
        if(set->identityCount) {
            xl6OutputPrintf(out,"static const uint64_t xlir_nominal_ids_%u[]={",(unsigned)(i+1));
            for(size_t n=0;n<set->identityCount;++n)
                xl6OutputPrintf(out,"0x%016llXULL,",(unsigned long long)ir->nominalIdentities[set->firstIdentity+n]);
            xl6OutputPuts("};\n",out);
        }
        xl6OutputPrintf(out,"static const XL6_NominalAdmissionV1 xlir_nominal_set_%u={%u,0x%016llXULL,",
            (unsigned)(i+1),(unsigned)set->kind,(unsigned long long)set->nonObjectTypeId);
        if(set->identityCount) xl6OutputPrintf(out,"xlir_nominal_ids_%u,",(unsigned)(i+1));
        else xl6OutputPuts("NULL,",out);
        xl6OutputPrintf(out,"%lu};\n",(unsigned long)set->identityCount);
    }
}
int main(int argc,char** argv)
{
    assert(argc==3); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1) || !xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
        for(size_t i=0;i<unit.parse.diags.count;++i)
            fprintf(stderr,"%d:%d %s\n",unit.parse.diags.items[i].line,unit.parse.diags.items[i].column,unit.parse.diags.items[i].message);
        return 1;
    }
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL));
    assert(xl6IrChannelReceiveSpec(XL6_IR_CHANNEL_RECEIVE_TRY)->waitMode==XL6_CHANNEL_TRY_V1);
    assert(xl6IrChannelReceiveSpec(XL6_IR_CHANNEL_RECEIVE_WAIT)->waitMode==XL6_CHANNEL_WAIT_V1);
    assert(xl6IrChannelReceiveSpec(XL6_IR_CHANNEL_RECEIVE_FOR)->waitMode==XL6_CHANNEL_FOR_V1);
    assert(!xl6IrChannelReceiveMethod(NULL) && !xl6IrChannelReceiveMethod("recvInto") &&
        !xl6IrChannelReceiveMethod("tryRecvInto") && !xl6IrChannelReceiveMethod("recvTimeoutInto"));
    unsigned recipes=0,pending=0,unsupported=0,plans=0;
    for(size_t i=0;i<ir->types.count;++i) {
        XL6_IrType* owner=&ir->types.items[i]; XL6_IrTypeId id=(XL6_IrTypeId)(i+1),schemaId=0;
        XL6_IrNominalIdentitySetId set=0;
        if(!xl6IrChannelConstructionRecipe(ir,id,&schemaId,&set)) {
            refused(ir,id);
            if(owner->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_CHANNEL && owner->messageSchemaType) {
                const XL6_IrType* schema=xl6IrTypeGet(&ir->types,owner->messageSchemaType);
                if(schema->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_PENDING_SLOT ||
                    schema->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_PENDING_DOMAIN) ++pending;
                else ++unsupported;
            }
            continue;
        }
        ++recipes; assert(owner->messageSchemaType==schemaId);
        XL6_IrType saved=*owner; XL6_IrType* schema=&ir->types.items[schemaId-1];
        XL6_IrTypeId result=resultFor(ir,id); assert(result);
        const XL6_IrType* tuple=xl6IrTypeGet(&ir->types,result);
        for(unsigned k=XL6_IR_CHANNEL_RECEIVE_WAIT;k<=XL6_IR_CHANNEL_RECEIVE_FOR;++k) {
            XL6_IrChannelReceivePlan plan={0};
            assert(xl6IrChannelReceivePlanBuild(ir,(XL6_IrChannelReceiveKind)k,id,result,&plan)); ++plans;
            assert(plan.ownerType==id && plan.schemaType==schemaId && plan.elementType==owner->params[0] &&
                plan.transferAbi==schema->sequenceLayout.transferAbi && plan.slotAbi==schema->sequenceLayout.slotAbi &&
                plan.admissionIdentitySet==set && plan.resultValueType==tuple->params[1] &&
                plan.errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE && plan.errorPhysicalAbi==XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT);
            assert(xl6IrChannelReceiveMethod(plan.spec->name)==plan.spec);
            XL6_IrTypeId args[3]={id,tuple->params[0],owner->params[0]};
            size_t count=plan.spec->timed ? 2u : 1u;
            assert(xl6IrChannelReceiveSignatureCompatible(ir,(XL6_IrChannelReceiveKind)k,id,result,args,count));
            assert(!xl6IrChannelReceiveSignatureCompatible(ir,(XL6_IrChannelReceiveKind)k,id,result,NULL,count));
            assert(!xl6IrChannelReceiveSignatureCompatible(ir,(XL6_IrChannelReceiveKind)k,id,result,args,count+1));
            assert(!xl6IrChannelReceiveSignatureCompatible(ir,(XL6_IrChannelReceiveKind)k,id,result,args,0));
            planRefused(ir,(XL6_IrChannelReceiveKind)k,id,owner->params[0]);
        }
        planRefused(ir,XL6_IR_CHANNEL_RECEIVE_INVALID,id,result);
        planRefused(ir,(XL6_IrChannelReceiveKind)UINT32_MAX,id,result);
        XL6_IrType* status=&ir->types.items[tuple->params[0]-1]; XL6_IrType savedStatus=*status;
        status->integerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_64;
        planRefused(ir,XL6_IR_CHANNEL_RECEIVE_TRY,id,result); *status=savedStatus;
        status->cAbi=XL6_IR_C_ABI_BOOL; planRefused(ir,XL6_IR_CHANNEL_RECEIVE_TRY,id,result); *status=savedStatus;
        XL6_IrType* value=&ir->types.items[tuple->params[1]-1]; XL6_IrType savedValue=*value;
        value->ownershipAbi=value->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED ?
            XL6_IR_OWNERSHIP_ABI_TRIVIAL : XL6_IR_OWNERSHIP_ABI_MANAGED;
        planRefused(ir,XL6_IR_CHANNEL_RECEIVE_TRY,id,result); *value=savedValue;
#define BAD_OWNER(Field,Value) do { owner->Field=(Value); refused(ir,id); \
    assert(!xl6IrVerifyModule(ir,NULL)); *owner=saved; } while(0)
        BAD_OWNER(messageSchemaType,0); BAD_OWNER(ownershipAbi,XL6_IR_OWNERSHIP_ABI_TRIVIAL);
        BAD_OWNER(cAbi,XL6_IR_C_ABI_I64); BAD_OWNER(typeValueId,0);
        BAD_OWNER(collectionAbi,XL6_IR_COLLECTION_ARRAY);
#undef BAD_OWNER
        XL6_IrSequenceLayout original=schema->sequenceLayout;
#define BAD_LAYOUT(Field,Value) do { if(original.Field!=(Value)) { \
    schema->sequenceLayout.Field=(Value); refused(ir,id); \
    assert(!xl6IrVerifyModule(ir,NULL)); schema->sequenceLayout=original; } } while(0)
        BAD_LAYOUT(elementType,0); BAD_LAYOUT(baseElementType,0);
        BAD_LAYOUT(role,XL6_IR_SEQUENCE_ROLE_STACK); BAD_LAYOUT(slotAbi,XL6_IR_NATIVE_SLOT_NONE);
        BAD_LAYOUT(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF);
        BAD_LAYOUT(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY);
        BAD_LAYOUT(codecAbi,original.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1?
            XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1:XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1);
        BAD_LAYOUT(nullable,!original.nullable); BAD_LAYOUT(admissionAbi,XL6_VALUE_ADMIT_NONE);
#undef BAD_LAYOUT
        XL6_IrTypeId other=0;
        for(size_t n=0;n<ir->types.count;++n) {
            const XL6_IrType* candidate=&ir->types.items[n];
            if(candidate->collectionAbi==XL6_IR_COLLECTION_ARRAY && candidate->paramCount==1 &&
                candidate->params && candidate->params[0]!=owner->params[0]) { other=(XL6_IrTypeId)(n+1); break; }
        }
        assert(other); owner->messageSchemaType=other; refused(ir,id);
        assert(!xl6IrVerifyModule(ir,NULL)); *owner=saved;
        assert(xl6IrVerifyModule(ir,NULL));
    }
    assert(recipes>=18 && pending && unsupported==0 && plans==recipes*3);
    XL6_Output out; assert(xl6OutputOpenFile(&out,argv[2]));
    xl6OutputPuts("#include <__xl_runtime/xruntime_decl.h>\n"
        "#include <__xl_runtime/xl6_channel.h>\n#include <__xl_runtime/xl6_array_admission.h>\n"
        "#include <__xl_runtime/xl6_nullable_string.h>\n#include <assert.h>\n#include <string.h>\n"
        "#include <__xl_runtime/xl6_slot_map_key_abi.h>\n"
        "#include <__xl_runtime/xl6_nullable_slot_map_key_abi.h>\n"
        "typedef xstrview xlir_string;\n"
        "typedef struct XL6_Closure xlir_closure;\n",&out);
    assert(xl6IrCgenEmitProductTypes(&out,ir,NULL,&unit.parse.diags)); nominalSets(&out,ir);
    xl6OutputPuts("int main(void) { xmemdebugsnapshot before={0},after={0};\n"
        "assert(xrtMemDebugEnable(true)); xrtClearError(); xrtMemDebugSnapshot(&before);\n",&out);
    for(size_t i=0;i<ir->types.count;++i) {
        XL6_IrTypeId id=(XL6_IrTypeId)(i+1); XL6_IrChannelReceivePlan p={0};
        XL6_IrTypeId result=resultFor(ir,id);
        if(!result || !xl6IrChannelReceivePlanBuild(ir,XL6_IR_CHANNEL_RECEIVE_TRY,id,result,&p)) continue;
        const XL6_IrType* owner=xl6IrTypeGet(&ir->types,id);
        XL6_IrChannelValuePlan message={0};assert(xl6IrChannelValuePlanBuild(ir,id,&message));
        bool encoded=message.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT;
        xl6OutputPuts("{ xvalue* schema=NULL;\n",&out);
        assert(xl6IrCgenEmitSequenceCreateDepth(&out,ir,p.schemaType,"schema",p.admissionIdentitySet,0,NULL,0,false,true)>0);
        xl6OutputPrintf(&out,"xvalue* channel=xl6ChannelCreateTakeV1(0x%016llXULL,schema,1); assert(channel);\n"
            "%s result={0}; XL6_ChannelStatusV1 status=XL6_CHANNEL_TIMEOUT_V1;\n",
            (unsigned long long)owner->typeValueId,xl6IrCgenType(ir,result));
        const char* place=p.valueTagged ? "result.e1.value" : "result.e1";
        if(encoded){xl6OutputPuts("xvalue* encoded_slot=NULL;\n",&out);place="encoded_slot";}
        xl6OutputPrintf(&out,"assert(xl6ChannelReceiveV1(channel,0x%016llXULL,0x%016llXULL,%s,"
            "(XL6_ChannelSlotV1){&%s,sizeof(%s)},XL6_CHANNEL_TRY_V1,0,&status));\n"
            "assert(status==XL6_CHANNEL_EMPTY_V1);\n",
            (unsigned long long)owner->typeValueId,(unsigned long long)xl6IrTypeGet(&ir->types,p.elementType)->typeValueId,
            xl6IrCgenNativeSlotTypeExpr(p.slotAbi),place,place);
        const XL6_IrType* element=xl6IrTypeGet(&ir->types,p.elementType);
        const char* descriptor=xl6IrCgenNativeSlotTypeExpr(p.slotAbi);
        if(encoded || p.transferAbi>=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF) {
            /* Empty-schema factory/receive acceptance only here. Genuine
             * owned codec/Future/key transport, detached results and graph edges
             * are exercised by the owned-profile language/native fixtures;
             * a zero carrier is not a valid nonnullable input. */
            xl6OutputPrintf(&out,"assert(xl6ChannelCloseV1(channel,0x%016llXULL));\n"
                "xrtValueRelease(channel); assert(!xrtGetError()); }\n",
                (unsigned long long)owner->typeValueId);
            continue;
        }
        xl6OutputPrintf(&out,"assert(xl6ChannelReceiveV1(channel,0x%016llXULL,0x%016llXULL,%s,"
            "(XL6_ChannelSlotV1){&%s,sizeof(%s)},XL6_CHANNEL_FOR_V1,0,&status));\n"
            "assert(status==XL6_CHANNEL_TIMEOUT_V1);\n"
            "%s input={0};\n",
            (unsigned long long)owner->typeValueId,(unsigned long long)element->typeValueId,
            descriptor,place,place,xl6IrCgenType(ir,p.elementType));
        bool nullable=element->kind==XL6_IR_TYPE_NULLABLE;
        if(p.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY && !nullable)
            xl6OutputPuts("input=(xstrview){\"a\\0b\",3};\n",&out);
        if(p.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN && !nullable) {
            if(element->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)
                xl6OutputPuts("input=xrtValueBytes((xbytesview){(cbytes)\"a\\0b\",3});\n",&out);
            else if(element->kind==XL6_IR_TYPE_CLASS)
                xl6OutputPuts("input=xrtValueObject();\n",&out);
            else xl6OutputPuts("input=xrtValueInt(42);\n",&out);
            xl6OutputPuts("assert(input);\n",&out);
            if(element->kind==XL6_IR_TYPE_CLASS || element->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)
                xl6OutputPrintf(&out,"assert(xrtValueTypeIdBind(input,0x%016llXULL));\n",
                    (unsigned long long)element->typeValueId);
        }
        if(element->kind==XL6_IR_TYPE_CLASS && !nullable) {
            /* Binding a wire Object's ID does not create a genuine instance.
             * Native class-success needs the separate public producer cutover. */
            xl6OutputPrintf(&out,"assert(!xl6ChannelSendV1(channel,0x%016llXULL,0x%016llXULL,%s,"
                "(XL6_ChannelSlotV1){&input,sizeof(input)},XL6_CHANNEL_TRY_V1,0,&status));\n"
                "assert(xrtGetError()); xrtClearError(); xrtValueRelease(input);\n",
                (unsigned long long)owner->typeValueId,(unsigned long long)element->typeValueId,descriptor);
        } else {
        xl6OutputPrintf(&out,"assert(xl6ChannelSendV1(channel,0x%016llXULL,0x%016llXULL,%s,"
            "(XL6_ChannelSlotV1){&input,sizeof(input)},XL6_CHANNEL_TRY_V1,0,&status));\n"
            "assert(status==XL6_CHANNEL_OK_V1);\n"
            "assert(xl6ChannelReceiveV1(channel,0x%016llXULL,0x%016llXULL,%s,"
            "(XL6_ChannelSlotV1){&%s,sizeof(%s)},XL6_CHANNEL_WAIT_V1,0,&status));\n"
            "assert(status==XL6_CHANNEL_OK_V1); result.e0=status;\n",
            (unsigned long long)owner->typeValueId,(unsigned long long)element->typeValueId,descriptor,
            (unsigned long long)owner->typeValueId,(unsigned long long)element->typeValueId,descriptor,place,place);
        if(p.valueTagged) xl6OutputPuts("result.e1.hasValue=true; assert(result.e1.hasValue);\n",&out);
        if(p.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY && !nullable)
            xl6OutputPrintf(&out,"assert(%s.Size==3 && !memcmp(%s.Data,\"a\\0b\",3));\n",place,place);
        if(p.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN) {
            xl6OutputPrintf(&out,"assert(%s==input);\n",place);
            xl6OutputPrintf(&out,"xrtTypeDropValue(%s,&input); assert(!xrtGetError());\n",descriptor);
        }
        }
        xl6OutputPrintf(&out,"xrtTypeDropValue(%s,&%s); assert(!xrtGetError()); memset(&result,0,sizeof(result));\n"
            "assert(xl6ChannelCloseV1(channel,0x%016llXULL));\n"
            "assert(xl6ChannelReceiveV1(channel,0x%016llXULL,0x%016llXULL,%s,"
            "(XL6_ChannelSlotV1){&%s,sizeof(%s)},XL6_CHANNEL_WAIT_V1,0,&status));\n"
            "assert(status==XL6_CHANNEL_CLOSED_V1); xrtValueRelease(channel); assert(!xrtGetError()); }\n",
            descriptor,place,(unsigned long long)owner->typeValueId,
            (unsigned long long)owner->typeValueId,(unsigned long long)element->typeValueId,
            descriptor,place,place);
    }
    xl6OutputPuts("xrtMemDebugSnapshot(&after); assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);\n"
        "assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);\n"
        "assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);\n"
        "return 0; }\n",&out);
    assert(!xl6OutputHasFailed(&out)); xl6OutputClose(&out);
    printf("Channel recipes=%u plans=%u pending=%u unsupported=%u mutations=%u\n",recipes,plans,pending,unsupported,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
