/* Real source builder and actual Cgen unity TU, never a synthetic type pool.
 * Proves complete native target contracts independently of language producers. */
#include "src/cgen/xl6_ir_cgen.c"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>

static void refused(const XL6_IrModule* ir,XL6_IrTypeId id)
{
    XL6_IrTypeId schema=123; XL6_IrNominalIdentitySetId set=456; XL6_IrSetKeyAbi key=(XL6_IrSetKeyAbi)789;
    assert(!xl6IrSetConstructionRecipe(ir,id,&schema,&set,&key) && schema==123 && set==456 && key==789);
    const XL6_IrType* type=xl6IrTypeGet(&ir->types,id);
    if (type && type->collectionAbi==XL6_IR_COLLECTION_SET) {
        XL6_IrValueCollectionRecipe before,after; memset(&before,0xa5,sizeof(before)); after=before;
        assert(!xl6IrValueCollectionConstructionRecipe(ir,id,&after) && !memcmp(&before,&after,sizeof(before)));
    }
    if (!xl6IrVerifyModule(ir,NULL)) {
        XL6_DiagList diags={0}; XL6_Output rejected;
        assert(xl6OutputOpenMemory(&rejected) && !xl6IrCgenEmitC(&rejected,ir,&diags) && !rejected.buffer.Size); xl6OutputDiscard(&rejected);
        assert(xl6OutputOpenMemory(&rejected) && !xl6IrCgenEmitHeader(&rejected,ir,"BAD_SET_RECIPE",&diags) && !rejected.buffer.Size); xl6OutputDiscard(&rejected);
        XL6_ArtifactRuntimePlan plan={0};
        assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&diags) && !plan.sourceCount && !plan.nativeModuleCount);
        xl6DiagFree(&diags);
    }
    XL6_Output out; assert(xl6OutputOpenMemory(&out));
    assert(xl6IrCgenEmitSetTargetCreate(&out,ir,id,"target",0)<0 && out.buffer.Size==0);
    xl6OutputDiscard(&out);
}
static void nominalSets(XL6_Output* out,const XL6_IrModule* ir)
{
    for (size_t i=0;i<ir->nominalIdentitySetCount;++i) {
        const XL6_IrNominalIdentitySet* set=&ir->nominalIdentitySets[i];
        if (set->identityCount) {
            xl6OutputPrintf(out,"    static const uint64_t xlir_nominal_ids_%u[] = {",(unsigned)(i+1));
            for (size_t j=0;j<set->identityCount;++j)
                xl6OutputPrintf(out,"0x%016llXULL,",(unsigned long long)ir->nominalIdentities[set->firstIdentity+j]);
            xl6OutputPuts("};\n",out);
        }
        xl6OutputPrintf(out,"    static const XL6_NominalAdmissionV1 xlir_nominal_set_%u = {%u,0x%016llXULL,",
            (unsigned)(i+1),(unsigned)set->kind,(unsigned long long)set->nonObjectTypeId);
        if (set->identityCount) xl6OutputPrintf(out,"xlir_nominal_ids_%u,",(unsigned)(i+1));
        else xl6OutputPuts("NULL,",out);
        xl6OutputPrintf(out,"%lu};\n",(unsigned long)set->identityCount);
    }
}
/* Independent structural oracle: count actual Set owners, not virtual Array
 * witnesses. Recursive child families spend the same finite depth budget. */
static unsigned setFactories(const XL6_IrModule* ir,XL6_IrTypeId id,unsigned depth)
{
    assert(depth<=64); const XL6_IrType* type=xl6IrTypeGet(&ir->types,id); assert(type);
    if (type->collectionAbi==XL6_IR_COLLECTION_SET) {
        XL6_IrTypeId schema=0; assert(xl6IrSetConstructionRecipe(ir,id,&schema,NULL,NULL));
        return 1+setFactories(ir,schema,depth);
    }
    if (type->collectionAbi==XL6_IR_COLLECTION_LIST) {
        XL6_IrTypeId schema=0; assert(xl6IrListConstructionRecipe(ir,id,&schema,NULL));
        return setFactories(ir,schema,depth);
    }
    const XL6_IrSequenceLayout* p=&type->sequenceLayout;
    if (p->abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1 ||
        p->abi==XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1 || p->abi==XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1)
        return setFactories(ir,p->baseElementType,depth+1);
    if (p->abi==XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1) {
        XL6_IrTypeId key=0,value=0;
        assert(xl6IrAvlConstructionRecipe(ir,p->baseElementType,&key,&value,NULL,NULL));
        return setFactories(ir,key,depth+1)+setFactories(ir,value,depth+1);
    }
    if (p->abi==XL6_IR_SEQUENCE_LAYOUT_QUEUE_REFERENCE_V1) {
        XL6_IrTypeId schema=0;
        assert(xl6IrConcurrentQueueConstructionRecipe(ir,p->baseElementType,NULL,&schema,NULL));
        return setFactories(ir,schema,depth+1);
    }
    return 0;
}
int main(int argc,char** argv)
{
    assert(argc==3); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=0; unsigned recipes=0,pending=0,mutations=0,descendants=0,sameSlot=0,erased=0,layouts=0,factories=0;
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"set_recipe_probe") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    XL6_ArtifactRuntimePlan unused={0};
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&unused,&parse.diags));
    assert(!xl6ArtifactRuntimeSetHas(&unused.components,XL6_ARTIFACT_SET_NATIVE) &&
        !xl6ArtifactRuntimeSetHas(&unused.components,XL6_ARTIFACT_SET_KEYS));
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrType* type=&ir.types.items[i]; XL6_IrTypeId id=(XL6_IrTypeId)(i+1),schemaId=0;
        XL6_IrNominalIdentitySetId set=0;
        XL6_IrSetKeyAbi keys=XL6_IR_SET_KEY_NONE;
        if (!xl6IrSetConstructionRecipe(&ir,id,&schemaId,&set,&keys)) {
            refused(&ir,id); if (type->collectionAbi==XL6_IR_COLLECTION_SET && type->collectionValueSchemaType) ++pending; continue;
        }
        ++recipes; factories+=setFactories(&ir,id,0); if (!type->paramCount) ++erased;
        XL6_IrType* schema=&ir.types.items[schemaId-1]; XL6_IrTypeId savedId=type->collectionValueSchemaType;
        assert(schemaId==savedId && schema->collectionAbi==XL6_IR_COLLECTION_ARRAY && schema->paramCount==1);
        XL6_IrValueCollectionRecipe complete={0}; assert(xl6IrValueCollectionConstructionRecipe(&ir,id,&complete));
        assert(complete.family==XL6_IR_COLLECTION_SET && complete.ownerType==id && complete.valueSchemaType==schemaId &&
            complete.elementType==schema->sequenceLayout.elementType && complete.admissionIdentitySet==set &&
            complete.slotAbi==schema->sequenceLayout.slotAbi && complete.codecAbi==schema->sequenceLayout.codecAbi &&
            complete.transferAbi==schema->sequenceLayout.transferAbi && complete.setKeyAbi==keys && keys==type->setKeyAbi);
        /* Independent oracle: physical ownership is not reference key sorting. */
        assert(keys==(schema->sequenceLayout.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ?
            XL6_IR_SET_KEY_VALUE_CONTENT_OR_REFERENCE_IDENTITY_V1 :
            schema->sequenceLayout.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF ?
            XL6_IR_SET_KEY_FUTURE_IDENTITY_V1 : XL6_IR_SET_KEY_TYPED_CONTENT_V1));
        for (unsigned candidate=0;candidate<=4;++candidate) {
            XL6_IrSetKeyAbi bad=candidate==4 ? (XL6_IrSetKeyAbi)UINT32_MAX : (XL6_IrSetKeyAbi)candidate;
            if (bad==keys) continue;
            type->setKeyAbi=bad; refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations;
        }
        type->setKeyAbi=keys;
        XL6_IrCollectionAbi savedFamily=type->collectionAbi; type->collectionAbi=XL6_IR_COLLECTION_LIST;
        refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations; type->collectionAbi=savedFamily;
        layouts|=1u<<(unsigned)schema->sequenceLayout.abi;
        size_t before=ir.types.count;
        assert(xl6IrTypeInternArray(&ir.types,schema->params[0])==schemaId && ir.types.count==before);
        type->collectionValueSchemaType=0; refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations;
        type->collectionValueSchemaType=savedId;
        XL6_IrTypeId other=0;
        for (size_t j=0;j<ir.types.count;++j) {
            const XL6_IrType* candidate=&ir.types.items[j];
            if (candidate->collectionAbi!=XL6_IR_COLLECTION_ARRAY || candidate->paramCount!=1 || candidate->params[0]==schema->params[0]) continue;
            if (!other) other=(XL6_IrTypeId)(j+1);
            if (candidate->sequenceLayout.slotAbi==schema->sequenceLayout.slotAbi &&
                candidate->sequenceLayout.transferAbi==schema->sequenceLayout.transferAbi) { other=(XL6_IrTypeId)(j+1); ++sameSlot; break; }
        }
        assert(other); type->collectionValueSchemaType=other; refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations;
        type->collectionValueSchemaType=savedId; XL6_IrSequenceLayout saved=schema->sequenceLayout;
#define BAD(FIELD,VALUE) do { schema->sequenceLayout.FIELD=(VALUE); refused(&ir,id); \
    assert(!xl6IrVerifyModule(&ir,NULL)); schema->sequenceLayout=saved; ++mutations; } while(0)
        BAD(abi,(XL6_IrSequenceLayoutAbi)UINT32_MAX); BAD(role,XL6_IR_SEQUENCE_ROLE_STACK);
        BAD(elementType,saved.elementType ? 0 : id); BAD(baseElementType,saved.baseElementType ? 0 : id);
        BAD(slotAbi,(XL6_IrNativeSlotAbi)UINT32_MAX);
        BAD(codecAbi,(XL6_IrSequenceCodecAbi)UINT32_MAX); BAD(transferAbi,(XL6_IrRuntimeResourcePayloadAbi)UINT32_MAX);
        BAD(admissionAbi,(XL6_ValueAdmissionV1)UINT32_MAX); BAD(nullable,!saved.nullable);
        BAD(defaultAbi,(XL6_IrSequenceDefaultAbi)UINT32_MAX); BAD(childRole,(XL6_IrSequenceRoleAbi)UINT32_MAX);
        BAD(childElementType,saved.childElementType ? 0 : id); BAD(childSlotAbi,(XL6_IrNativeSlotAbi)UINT32_MAX);
        BAD(childTransferAbi,(XL6_IrRuntimeResourcePayloadAbi)UINT32_MAX);
#undef BAD
        if (saved.abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1) {
            XL6_IrType* child=&ir.types.items[saved.baseElementType-1]; XL6_IrSequenceLayout childSaved=child->sequenceLayout;
            child->sequenceLayout.codecAbi=(XL6_IrSequenceCodecAbi)UINT32_MAX;
            refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++descendants; child->sequenceLayout=childSaved;
        }
        if (saved.abi==XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1) {
            XL6_IrType* child=&ir.types.items[saved.baseElementType-1];
            XL6_IrSetKeyAbi keySaved=child->setKeyAbi; child->setKeyAbi=XL6_IR_SET_KEY_NONE;
            refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++descendants; child->setKeyAbi=keySaved;
            /* Hostile self-recursion must terminate within the mixed budget. */
            XL6_IrTypeId parameter=child->params[0]; child->params[0]=saved.baseElementType;
            refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++descendants; child->params[0]=parameter;
        }
        XL6_Output badSet; assert(xl6OutputOpenMemory(&badSet));
        assert(xl6IrCgenEmitSetTargetCreate(&badSet,&ir,id,"target",UINT32_MAX)<0 && badSet.buffer.Size==0);
        xl6OutputDiscard(&badSet); assert(xl6IrVerifyModule(&ir,NULL));
    }
    assert(recipes>=40 && pending>=3 && erased && sameSlot && descendants);
    assert(xl6OutputOpenFile(&out,argv[2]));
    xl6OutputPuts("#include <__xl_runtime/xruntime_decl.h>\n"
        "#include <__xl_runtime/xl6_array_admission.h>\n"
        "#include <__xl_runtime/xl6_array_erased.h>\n"
        "#include <__xl_runtime/xl6_set.h>\n"
        "#include <__xl_runtime/xl6_list_schema.h>\n"
        "#include <__xl_runtime/xl6_set_schema.h>\n"
        "#include <__xl_runtime/xl6_tree.h>\n"
        "#include <__xl_runtime/xl6_nullable_string.h>\n"
        "#include <__xl_runtime/xl6_product_abi.h>\n"
        "#include <__xl_runtime/xl6_concurrent_queue.h>\n"
        "#include <assert.h>\n#include <stdlib.h>\n#include <limits.h>\n#include <wchar.h>\n"
        "typedef xstrview xlir_string;\n"
        "typedef struct xlir_closure xlir_closure;\n"
        "typedef struct xlir_generator xlir_generator;\n",&out);
    xl6IrCgenEmitForeignErrorDeclarations(&out,0);
    if (!xl6IrCgenEmitUnionTypes(&out,&ir,NULL,&parse.diags) ||
        !xl6IrCgenEmitProductTypes(&out,&ir,NULL,&parse.diags)) {
        xl6OutputClose(&out); goto done;
    }
    xl6OutputPuts("/* CLOSED_SET_TARGET_NATIVE_PROOF */\n"
        "bool xl6_set_recipe_probe(void) {\n"
        "    xvalue* target=NULL; xl6_error_scope scope={0};\n"
        "    if (!xl6ErrorScopeBegin(&scope)) return false;\n",&out);
    nominalSets(&out,&ir);
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrTypeId id=(XL6_IrTypeId)(i+1),schemaId; XL6_IrNominalIdentitySetId set;
        if (!xl6IrSetConstructionRecipe(&ir,id,&schemaId,&set,NULL)) continue;
        assert(xl6IrCgenEmitSetTargetCreate(&out,&ir,id,"target",set)>0);
        const XL6_IrType* schema=&ir.types.items[schemaId-1];
        XL6_IrTypeId element=schema->sequenceLayout.elementType;
        xl6OutputPrintf(&out,"    if (!target || scope.failed || xrtGetError()) { xl6ErrorScopeCaptureFailure(&scope); goto done; }\n"
            "    assert(xrtValueType(target)==XVALUE_HANDLE && xrtValueTypeId(target)==0x%016llXULL);\n"
            "    { int64_t count=-1; XL6_SetLayoutV1 actual={0}; XL6_SequenceAdmissionV3 policy={0};\n"
            "      bool got=xl6SetSchemaCopyV1(target,&actual,&policy);\n"
            "      if (!got) { xl6ErrorScopeCaptureFailure(&scope); goto done; }\n"
            "      assert(actual.identity==0x%016llXULL && actual.value.identity==0x%016llXULL && actual.value.elementIdentity==0x%016llXULL);\n"
            "      xl6SequenceAdmissionDropV3(&policy);\n"
            "      if (!xl6SetErasedCountV1(target,false,&count)) { xl6ErrorScopeCaptureFailure(&scope); goto done; }\n"
            "      assert(count==0); }\n"
            "    xrtValueRelease(target); target=NULL;\n"
            "    if (scope.failed || xrtGetError()) { xl6ErrorScopeCaptureFailure(&scope); goto done; }\n",
            (unsigned long long)ir.types.items[i].typeValueId,(unsigned long long)ir.types.items[i].typeValueId,
            (unsigned long long)schema->typeValueId,
            (unsigned long long)ir.types.items[element-1].typeValueId);
    }
    xl6OutputPuts("done:\n    xrtValueRelease(target);\n"
        "    xl6_error_result result=xl6ErrorScopeEnd(&scope);\n"
        "    if (!result.ok) xl6ErrorScopePropagateFailure(result);\n"
        "    return result.ok;\n}\n",&out);
    ok=!xl6OutputHasFailed(&out); xl6OutputClose(&out);
    printf("Set targets: %u complete, %u rejected mutations, %u pending, %u descendants, %u same-slot, %u erased, layouts=%u factories=%u\n",
        recipes,mutations,pending,descendants,sameSlot,erased,layouts,factories);
done:
    for (size_t i=0;i<parse.diags.count;++i)
        fprintf(stderr,"%d:%d %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
