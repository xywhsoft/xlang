/* Real source builder and actual Cgen unity TU, never a synthetic type pool.
 * Proves native target contracts while source List remains on the old carrier. */
#include "src/cgen/xl6_ir_cgen.c"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>

static void refused(const XL6_IrModule* ir,XL6_IrTypeId id)
{
    XL6_IrTypeId schema=123; XL6_IrNominalIdentitySetId set=456;
    assert(!xl6IrListConstructionRecipe(ir,id,&schema,&set) && schema==123 && set==456);
    XL6_Output out; assert(xl6OutputOpenMemory(&out));
    assert(xl6IrCgenEmitListTargetCreate(&out,ir,id,"target",0)<0 && out.buffer.Size==0);
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
int main(int argc,char** argv)
{
    assert(argc==3); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=0; unsigned recipes=0,pending=0,mutations=0,descendants=0,sameSlot=0,erased=0,layouts=0;
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"list_recipe_probe") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrType* type=&ir.types.items[i]; XL6_IrTypeId id=(XL6_IrTypeId)(i+1),schemaId=0;
        XL6_IrNominalIdentitySetId set=0;
        if (!xl6IrListConstructionRecipe(&ir,id,&schemaId,&set)) {
            refused(&ir,id); if (type->listValueSchemaType) ++pending; continue;
        }
        ++recipes; if (!type->paramCount) ++erased;
        XL6_IrType* schema=&ir.types.items[schemaId-1]; XL6_IrTypeId savedId=type->listValueSchemaType;
        assert(schemaId==savedId && schema->collectionAbi==XL6_IR_COLLECTION_ARRAY && schema->paramCount==1);
        layouts|=1u<<(unsigned)schema->sequenceLayout.abi;
        size_t before=ir.types.count;
        assert(xl6IrTypeInternArray(&ir.types,schema->params[0])==schemaId && ir.types.count==before);
        type->listValueSchemaType=0; refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations;
        type->listValueSchemaType=savedId;
        XL6_IrTypeId other=0;
        for (size_t j=0;j<ir.types.count;++j) {
            const XL6_IrType* candidate=&ir.types.items[j];
            if (candidate->collectionAbi!=XL6_IR_COLLECTION_ARRAY || candidate->paramCount!=1 || candidate->params[0]==schema->params[0]) continue;
            if (!other) other=(XL6_IrTypeId)(j+1);
            if (candidate->sequenceLayout.slotAbi==schema->sequenceLayout.slotAbi &&
                candidate->sequenceLayout.transferAbi==schema->sequenceLayout.transferAbi) { other=(XL6_IrTypeId)(j+1); ++sameSlot; break; }
        }
        assert(other); type->listValueSchemaType=other; refused(&ir,id); assert(!xl6IrVerifyModule(&ir,NULL)); ++mutations;
        type->listValueSchemaType=savedId; XL6_IrSequenceLayout saved=schema->sequenceLayout;
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
        XL6_Output badSet; assert(xl6OutputOpenMemory(&badSet));
        assert(xl6IrCgenEmitListTargetCreate(&badSet,&ir,id,"target",UINT32_MAX)<0 && badSet.buffer.Size==0);
        xl6OutputDiscard(&badSet); assert(xl6IrVerifyModule(&ir,NULL));
    }
    assert(recipes>=40 && pending>=4 && erased && sameSlot && descendants);
    assert(xl6OutputOpenFile(&out,argv[2]));
    xl6OutputPuts("#include <__xl_runtime/xruntime_decl.h>\n"
        "#include <__xl_runtime/xl6_array_admission.h>\n"
        "#include <__xl_runtime/xl6_array_erased.h>\n"
        "#include <__xl_runtime/xl6_list.h>\n"
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
    xl6OutputPuts("/* CLOSED_LIST_TARGET_NATIVE_PROOF */\n"
        "bool xl6_list_recipe_probe(void) {\n"
        "    xvalue* target=NULL; xl6_error_scope scope={0};\n"
        "    if (!xl6ErrorScopeBegin(&scope)) return false;\n",&out);
    nominalSets(&out,&ir);
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrTypeId id=(XL6_IrTypeId)(i+1),schemaId; XL6_IrNominalIdentitySetId set;
        if (!xl6IrListConstructionRecipe(&ir,id,&schemaId,&set)) continue;
        assert(xl6IrCgenEmitListTargetCreate(&out,&ir,id,"target",set)>0);
        const XL6_IrType* schema=&ir.types.items[schemaId-1];
        XL6_IrTypeId element=schema->sequenceLayout.elementType;
        xl6OutputPrintf(&out,"    if (!target || scope.failed || xrtGetError()) { xl6ErrorScopeCaptureFailure(&scope); goto done; }\n"
            "    assert(xrtValueType(target)==XVALUE_HANDLE && xrtValueTypeId(target)==0x%016llXULL);\n"
            "    { int64_t count=-1; XL6_ListLayoutV1 actual={0}; XL6_SequenceAdmissionV3 policy={0};\n"
            "      bool got=xl6ListSchemaCopyV1(target,&actual,&policy);\n"
            "      if (!got) { xl6ErrorScopeCaptureFailure(&scope); goto done; }\n"
            "      assert(actual.identity==0x%016llXULL && actual.value.identity==0x%016llXULL && actual.value.elementIdentity==0x%016llXULL);\n"
            "      xl6SequenceAdmissionDropV3(&policy);\n"
            "      if (!xl6ListErasedCountV1(target,&count)) { xl6ErrorScopeCaptureFailure(&scope); goto done; }\n"
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
    printf("List targets: %u complete, %u rejected mutations, %u pending, %u descendants, %u same-slot, %u erased, layouts=%u\n",
        recipes,mutations,pending,descendants,sameSlot,erased,layouts);
done:
    for (size_t i=0;i<parse.diags.count;++i)
        fprintf(stderr,"%d:%d %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
