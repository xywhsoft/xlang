/* Actual Cgen unity TU and actual source XLIR builder; no synthetic type pool.
 * This closes the prerequisite recipe, NOT the public Queue API cutover. */
#include "src/cgen/xl6_ir_cgen.c"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>

static void refused(const XL6_IrModule* ir, XL6_IrTypeId id)
{
    XL6_IrRuntimeResourceAbi family=(XL6_IrRuntimeResourceAbi)UINT32_MAX;
    XL6_IrTypeId schema=123;
    XL6_IrNominalIdentitySetId set=456;
    assert(!xl6IrConcurrentQueueConstructionRecipe(ir,id,&family,&schema,&set));
    assert(family==(XL6_IrRuntimeResourceAbi)UINT32_MAX && schema==123 && set==456);
}

static void schemas(XL6_Output* out, const XL6_IrModule* ir)
{
    for (size_t i=0;i<ir->nominalIdentitySetCount;++i) {
        const XL6_IrNominalIdentitySet* set=&ir->nominalIdentitySets[i];
        if (set->identityCount) {
            xl6OutputPrintf(out,"    static const uint64_t xlir_nominal_ids_%u[] = {",(unsigned)(i+1));
            for (size_t n=0;n<set->identityCount;++n)
                xl6OutputPrintf(out,"0x%016llXULL,",(unsigned long long)ir->nominalIdentities[set->firstIdentity+n]);
            xl6OutputPuts("};\n",out);
        }
        xl6OutputPrintf(out,"    static const XL6_NominalAdmissionV1 xlir_nominal_set_%u = {%u,0x%016llXULL,",
            (unsigned)(i+1),(unsigned)set->kind,(unsigned long long)set->nonObjectTypeId);
        if (set->identityCount) xl6OutputPrintf(out,"xlir_nominal_ids_%u,",(unsigned)(i+1));
        else xl6OutputPuts("NULL,",out);
        xl6OutputPrintf(out,"%lu};\n",(unsigned long)set->identityCount);
    }
}

int main(int argc, char** argv)
{
    assert(argc==3);
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=0; unsigned recipes=0,mutations=0,pending=0,nested=0,layouts=0,families=0,sameSlot=0;
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"queue_recipe_probe") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) ||
        !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrType* type=&ir.types.items[i]; XL6_IrTypeId id=(XL6_IrTypeId)(i+1);
        XL6_IrRuntimeResourceAbi family; XL6_IrTypeId schemaId; XL6_IrNominalIdentitySetId set;
        if (!xl6IrConcurrentQueueConstructionRecipe(&ir,id,&family,&schemaId,&set)) {
            refused(&ir,id);
            if (type->concurrentQueueSchemaType) ++pending;
            continue;
        }
        ++recipes; families|=1u<<(unsigned)(family-XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE);
        XL6_IrType* schema=&ir.types.items[schemaId-1];
        assert(schema->kind==XL6_IR_TYPE_GENERIC_INSTANCE && schema->collectionAbi==XL6_IR_COLLECTION_ARRAY);
        assert(schema->paramCount==1 && schema->params[0]==type->params[0]);
        layouts|=1u<<(unsigned)schema->sequenceLayout.abi;
        size_t before=ir.types.count;
        assert(xl6IrTypeInternArray(&ir.types,type->params[0])==schemaId && ir.types.count==before);
        XL6_IrTypeId savedId=type->concurrentQueueSchemaType;
        type->concurrentQueueSchemaType=XL6_IR_TYPE_INVALID;
        refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations;
        type->concurrentQueueSchemaType=savedId;
        /* Prefer a physically identical slot/transfer with another logical
         * domain. Equal width or pointer carrier is not admission authority. */
        XL6_IrTypeId other=0;
        for (size_t n=0;n<ir.types.count;++n) {
            const XL6_IrType* candidate=&ir.types.items[n];
            if (candidate->collectionAbi!=XL6_IR_COLLECTION_ARRAY || candidate->paramCount!=1 ||
                candidate->params[0]==type->params[0]) continue;
            if (!other) other=(XL6_IrTypeId)(n+1);
            if (candidate->sequenceLayout.slotAbi==schema->sequenceLayout.slotAbi &&
                candidate->sequenceLayout.transferAbi==schema->sequenceLayout.transferAbi) {
                other=(XL6_IrTypeId)(n+1); ++sameSlot; break;
            }
        }
        assert(other); type->concurrentQueueSchemaType=other;
        refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations;
        type->concurrentQueueSchemaType=savedId;
        XL6_IrSequenceLayout saved=schema->sequenceLayout;
#define BAD(FIELD,VALUE) do { schema->sequenceLayout.FIELD=(VALUE); refused(&ir,id); \
    assert(!xl6IrVerifyModule(&ir,NULL)); schema->sequenceLayout=saved; ++mutations; } while(0)
        BAD(abi,(XL6_IrSequenceLayoutAbi)UINT32_MAX);
        BAD(role,XL6_IR_SEQUENCE_ROLE_STACK);
        BAD(elementType,XL6_IR_TYPE_INVALID); BAD(baseElementType,XL6_IR_TYPE_INVALID);
        BAD(slotAbi,(XL6_IrNativeSlotAbi)UINT32_MAX);
        BAD(codecAbi,(XL6_IrSequenceCodecAbi)UINT32_MAX);
        BAD(transferAbi,(XL6_IrRuntimeResourcePayloadAbi)UINT32_MAX);
        BAD(admissionAbi,(XL6_ValueAdmissionV1)UINT32_MAX);
        BAD(nullable,!saved.nullable); BAD(defaultAbi,(XL6_IrSequenceDefaultAbi)UINT32_MAX);
        BAD(childRole,(XL6_IrSequenceRoleAbi)UINT32_MAX);
        BAD(childElementType,saved.childElementType ? XL6_IR_TYPE_INVALID : id);
        BAD(childSlotAbi,(XL6_IrNativeSlotAbi)UINT32_MAX);
        BAD(childTransferAbi,(XL6_IrRuntimeResourcePayloadAbi)UINT32_MAX);
#undef BAD
        if (saved.abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1) {
            XL6_IrType* child=&ir.types.items[saved.baseElementType-1];
            XL6_IrSequenceLayout childSaved=child->sequenceLayout;
            child->sequenceLayout.codecAbi=(XL6_IrSequenceCodecAbi)UINT32_MAX;
            refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++nested;
            child->sequenceLayout=childSaved;
        }
        assert(xl6IrVerifyModule(&ir,NULL));
    }
    assert(recipes>=60 && families==7 && pending>=3 && nested>0 && sameSlot>0);
    unsigned creates=0,instructionMutations=0;
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            instruction->runtimeOp!=XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE) continue;
        assert(xl6IrCgenRuntimeSpecialQueueAbi(&ir,instruction)); ++creates;
        XL6_IrInstruction saved=*instruction;
        XL6_IrType* queue=&ir.types.items[instruction->type-1];
        XL6_IrTypeId savedSchema=queue->concurrentQueueSchemaType;
        for (unsigned mode=0;mode<3;++mode) {
            if (mode==0) instruction->sequenceAdmissionIdentitySet=UINT32_MAX;
            else if (mode==1) instruction->runtimeResourcePayloadAbi=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
            else queue->concurrentQueueSchemaType=XL6_IR_TYPE_INVALID;
            assert(!xl6IrVerifyModule(&ir,NULL));
            XL6_Output refusedOutput; assert(xl6OutputOpenMemory(&refusedOutput));
            assert(xl6IrCgenEmitSpecialQueueRuntime(&refusedOutput,&ir,instruction)<0);
            assert(refusedOutput.buffer.Size==0); xl6OutputDiscard(&refusedOutput);
            *instruction=saved; queue->concurrentQueueSchemaType=savedSchema; ++instructionMutations;
        }
    }
    assert(creates>=2 && xl6IrVerifyModule(&ir,NULL));
    assert(xl6OutputOpenFile(&out,argv[2]));
    /* Independent native target-plan product. Source Queue methods have not
     * been cut over and are deliberately NOT represented as acceptance here. */
    xl6OutputPuts("#include <__xl_runtime/xruntime_decl.h>\n"
        "#include <limits.h>\n#include <wchar.h>\n"
        "#include <__xl_runtime/xl6_array_admission.h>\n"
        "#include <__xl_runtime/xl6_nullable_string.h>\n"
        "typedef xstrview xlir_string;\n"
        "typedef struct xlir_closure xlir_closure;\n"
        "typedef struct xlir_generator xlir_generator;\n",&out);
    if (!xl6IrCgenEmitUnionTypes(&out,&ir,NULL,&parse.diags) ||
        !xl6IrCgenEmitProductTypes(&out,&ir,NULL,&parse.diags)) {
        xl6OutputClose(&out); goto done;
    }
    xl6OutputPuts("\n/* CLOSED_QUEUE_RECIPE_NATIVE_PROOF */\n"
        "#include \"__xl_runtime/xl6_concurrent_queue.h\"\n"
        "#include \"__xl_runtime/xl6_array_erased.h\"\n"
        "#include <assert.h>\n"
        "int xl6_queue_recipe_probe(void) {\n"
        "    xmemdebugsnapshot before={0},after={0}; assert(xrtMemDebugEnable(true)); xrtClearError();\n"
        "    xrtMemDebugSnapshot(&before); {\n",&out);
    schemas(&out,&ir);
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrTypeId id=(XL6_IrTypeId)(i+1),schemaId;
        XL6_IrRuntimeResourceAbi family; XL6_IrNominalIdentitySetId set;
        if (!xl6IrConcurrentQueueConstructionRecipe(&ir,id,&family,&schemaId,&set)) continue;
        const XL6_IrType* type=&ir.types.items[i];
        const XL6_IrSequenceLayout* plan=&ir.types.items[schemaId-1].sequenceLayout;
        const char* prefix=family==XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE ? "xl6SpscQueue" :
            family==XL6_IR_RUNTIME_RESOURCE_MPSC_QUEUE ? "xl6MpscQueue" : "xl6MpmcQueue";
        const char* projection=plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ? "NULL" :
            xl6IrCgenNativeValueConverterExpr(&ir,plan->elementType);
        xl6OutputPuts("    { xvalue* schema=NULL;\n",&out);
        assert(xl6IrCgenEmitSequenceCreateDepth(&out,&ir,schemaId,"schema",set,0,NULL,0,false)>0);
        xl6OutputPrintf(&out,"    xvalue* queue=%sCreateTakeV1(0x%016llXULL,schema,8); assert(queue);\n"
            "    assert(%sQueryV1(queue,0x%016llXULL,XL6_QUEUE_QUERY_CAPACITY_V1)==8);\n",
            prefix,(unsigned long long)type->typeValueId,prefix,(unsigned long long)type->typeValueId);
        xl6OutputPrintf(&out,"    XL6_QueueArrayResultV2 popped=%sPopArrayV2(queue,0x%016llXULL,0x%016llXULL,8,%s);\n"
            "    assert(popped.Result==XQUEUE_EMPTY && popped.Count==0 && popped.Items);\n"
            "    assert(xl6ArrayErasedSizeV1(popped.Items)==0);\n"
            "    xqueuebatchresult pushed=%sPushArrayV2(queue,0x%016llXULL,popped.Items,0x%016llXULL);\n"
            "    assert(pushed.Result==XQUEUE_OK && pushed.Count==0); xrtValueRelease(popped.Items);\n",
            prefix,(unsigned long long)type->typeValueId,(unsigned long long)ir.types.items[schemaId-1].typeValueId,
            projection,prefix,(unsigned long long)type->typeValueId,(unsigned long long)ir.types.items[schemaId-1].typeValueId);
        xl6OutputPrintf(&out,"    assert(%sCloseV1(queue,0x%016llXULL));\n"
            "    assert(%sQueryV1(queue,0x%016llXULL,XL6_QUEUE_QUERY_DRAINED_V1)==1);\n"
            "    assert(%sResetV1(queue,0x%016llXULL)); xrtValueRelease(queue); assert(!xrtGetError()); }\n",
            prefix,(unsigned long long)type->typeValueId,prefix,(unsigned long long)type->typeValueId,
            prefix,(unsigned long long)type->typeValueId);
    }
    xl6OutputPuts("    } xrtMemDebugSnapshot(&after);\n"
        "    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);\n"
        "    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);\n"
        "    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);\n"
        "    return 0; }\n",&out);
    ok=!xl6OutputHasFailed(&out); xl6OutputClose(&out);
    printf("Queue recipes: %u complete, %u rejected mutations, %u pending, %u descendant rejections, layouts=%u families=%u\n",
        recipes,mutations,pending,nested,layouts,families);
    printf("Queue instructions: %u actual creates, %u rejected before output; %u same-slot domain rejections\n",
        creates,instructionMutations,sameSlot);
done:
    for (size_t i=0;i<parse.diags.count;++i)
        fprintf(stderr,"%d:%d %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
