/* White-box the actual Cgen unity TU. The runner excludes its ordinary object,
 * so this is not a duplicate or mocked factory emitter. No product API is
 * exposed merely to let tests call this compiler-private plan renderer. */
#include "src/cgen/xl6_ir_cgen.c"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>

static void refused(const XL6_IrModule* ir,XL6_IrTypeId id,XL6_IrNominalIdentitySetId set)
{
    XL6_Output out; assert(xl6OutputOpenMemory(&out));
    assert(xl6IrCgenEmitSequenceCreate(&out,ir,id,id,set)<0);
    assert(out.buffer.Size==0); xl6OutputDiscard(&out);
    size_t count=0;
    if (!xl6IrArrayImportChainRecipe(ir,id,NULL,0,&count)) {
        assert(xl6OutputOpenMemory(&out));
        assert(xl6IrCgenEmitArrayImportChain(&out,ir,id,id,"wire",false)<0);
        assert(out.buffer.Size==0); xl6OutputDiscard(&out);
    }
}
static void schemas(XL6_Output* out,const XL6_IrModule* ir)
{
    for (size_t i=0;i<ir->nominalIdentitySetCount;++i) {
        const XL6_IrNominalIdentitySet* set=&ir->nominalIdentitySets[i];
        if (set->identityCount) {
            xl6OutputPrintf(out,"    static const uint64_t xlir_nominal_ids_%u[] = {",(unsigned)(i+1));
            for (size_t n=0;n<set->identityCount;++n) xl6OutputPrintf(out,"0x%016llXULL,",
                (unsigned long long)ir->nominalIdentities[set->firstIdentity+n]);
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
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir); int ok=0; unsigned recipes=0,mutations=0;
    unsigned layouts=0;
    unsigned nestedMutations=0;
    unsigned importPlans=0;
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"recipe_probe") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrType* type=&ir.types.items[i]; XL6_IrTypeId id=(XL6_IrTypeId)(i+1);
        XL6_IrNominalIdentitySetId set=0;
        if (!xl6IrSequenceConstructionRecipe(&ir,id,type->sequenceLayout.role,&set)) continue;
        ++recipes; layouts|=1u<<(unsigned)type->sequenceLayout.abi;
        if (type->sequenceLayout.role==XL6_IR_SEQUENCE_ROLE_ARRAY) {
            XL6_IrTypeId chain[65]={0}; size_t count=123,queried=456;
            assert(xl6IrArrayImportChainRecipe(&ir,id,NULL,0,&queried));
            assert(queried>0 && queried<=65 && xl6IrArrayImportChainRecipe(&ir,id,chain,65,&count));
            assert(count==queried && chain[0]==id);
            for (size_t n=1;n<count;++n) assert(chain[n]==ir.types.items[chain[n-1]-1].sequenceLayout.baseElementType);
            const XL6_IrSequenceLayout* leaf=&ir.types.items[chain[count-1]-1].sequenceLayout;
            assert(leaf->abi!=XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1 || leaf->childRole!=XL6_IR_SEQUENCE_ROLE_ARRAY);
            XL6_IrTypeId untouched[65]; memcpy(untouched,chain,sizeof(chain)); count=123;
            assert(!xl6IrArrayImportChainRecipe(&ir,id,chain,queried-1,&count));
            assert(count==123 && !memcmp(chain,untouched,sizeof(chain)));
            XL6_IrSequenceLayout savedLeaf=ir.types.items[chain[queried-1]-1].sequenceLayout;
            ir.types.items[chain[queried-1]-1].sequenceLayout.codecAbi=(XL6_IrSequenceCodecAbi)UINT32_MAX;
            assert(!xl6IrArrayImportChainRecipe(&ir,id,chain,65,&count));
            assert(count==123 && !memcmp(chain,untouched,sizeof(chain)));
            ir.types.items[chain[queried-1]-1].sequenceLayout=savedLeaf;
            ++importPlans;
        } else {
            size_t untouched=123;
            assert(!xl6IrArrayImportChainRecipe(&ir,id,NULL,0,&untouched) && untouched==123);
        }
        XL6_IrSequenceLayout saved=type->sequenceLayout;
#define BAD(FIELD,VALUE) do { type->sequenceLayout.FIELD=(VALUE); refused(&ir,id,set); \
    type->sequenceLayout=saved; ++mutations; } while (0)
        BAD(abi,(XL6_IrSequenceLayoutAbi)UINT32_MAX);
        BAD(role,(XL6_IrSequenceRoleAbi)UINT32_MAX);
        BAD(elementType,XL6_IR_TYPE_INVALID); BAD(baseElementType,XL6_IR_TYPE_INVALID);
        BAD(slotAbi,(XL6_IrNativeSlotAbi)UINT32_MAX);
        BAD(codecAbi,(XL6_IrSequenceCodecAbi)UINT32_MAX);
        BAD(transferAbi,(XL6_IrRuntimeResourcePayloadAbi)UINT32_MAX);
        BAD(admissionAbi,(XL6_ValueAdmissionV1)UINT32_MAX); BAD(nullable,!saved.nullable);
        BAD(defaultAbi,(XL6_IrSequenceDefaultAbi)UINT32_MAX);
        BAD(childRole,(XL6_IrSequenceRoleAbi)UINT32_MAX);
        BAD(childElementType,saved.childElementType ? XL6_IR_TYPE_INVALID : (XL6_IrTypeId)1);
        BAD(childSlotAbi,(XL6_IrNativeSlotAbi)UINT32_MAX);
        BAD(childTransferAbi,(XL6_IrRuntimeResourcePayloadAbi)UINT32_MAX);
#undef BAD
        refused(&ir,id,set ? 0 : 1); ++mutations;
        if (saved.abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1) {
            XL6_IrType* child=&ir.types.items[saved.baseElementType-1];
            XL6_IrSequenceLayout childSaved=child->sequenceLayout;
            child->sequenceLayout.admissionAbi=(XL6_ValueAdmissionV1)UINT32_MAX;
            refused(&ir,id,set); ++nestedMutations;
            child->sequenceLayout=childSaved;
            if (childSaved.abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1) {
                XL6_IrType* grandchild=&ir.types.items[childSaved.baseElementType-1];
                XL6_IrSequenceLayout grandchildSaved=grandchild->sequenceLayout;
                grandchild->sequenceLayout.defaultAbi=(XL6_IrSequenceDefaultAbi)UINT32_MAX;
                refused(&ir,id,set); ++nestedMutations;
                grandchild->sequenceLayout=grandchildSaved;
            }
        }
    }
    assert(recipes && xl6IrVerifyModule(&ir,NULL));
    printf("Array import chain recipes: %u complete plans; short/malformed outputs unchanged\n",importPlans);
    if (!xl6OutputOpenFile(&out,argv[2])) goto done;
    /* This is an independent native target-plan product, not an injection
     * into a source module whose selected declarations intentionally omit
     * these target-only factories. Use the actual Cgen type emitters, too. */
    xl6OutputPuts("#include <__xl_runtime/xruntime_decl.h>\n"
        "#include <assert.h>\n#include <limits.h>\n#include <wchar.h>\n"
        "#include <__xl_runtime/xl6_array_admission.h>\n"
        "#include <__xl_runtime/xl6_array_erased.h>\n"
        "#include <__xl_runtime/xl6_array_import_chain.h>\n"
        "#include <__xl_runtime/xl6_nullable_string.h>\n"
        "typedef xstrview xlir_string;\n"
        "typedef struct xlir_closure xlir_closure;\n"
        "typedef struct xlir_generator xlir_generator;\n",&out);
    if (!xl6IrCgenEmitUnionTypes(&out,&ir,NULL,&parse.diags) ||
        !xl6IrCgenEmitProductTypes(&out,&ir,NULL,&parse.diags)) { xl6OutputClose(&out); goto done; }
    xl6OutputPuts(
        "int xl6_recipe_probe(void)\n{\n    assert(xrtMemDebugEnable(true));\n"
        "    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);\n",&out);
    schemas(&out,&ir);
    xl6OutputPuts("    for (unsigned cycle=0;cycle<100;++cycle) {\n",&out);
    for (size_t i=0;i<ir.types.count;++i) {
        const XL6_IrType* type=&ir.types.items[i]; XL6_IrTypeId id=(XL6_IrTypeId)(i+1);
        XL6_IrNominalIdentitySetId set=0;
        if (!xl6IrSequenceConstructionRecipe(&ir,id,type->sequenceLayout.role,&set)) continue;
        const XL6_IrSequenceLayout* plan=&type->sequenceLayout;
        const XL6_IrType* element=xl6IrTypeGet(&ir.types,plan->elementType);
        const char* physicalType=xl6IrCgenType(&ir,plan->elementType);
        if (plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1) physicalType="xvalue*";
        if (plan->abi==XL6_IR_SEQUENCE_LAYOUT_FUTURE_REFERENCE_V1) physicalType="xfuture*";
        xl6OutputPrintf(&out,"    { xvalue* v%u=NULL;\n",(unsigned)id);
        int rendered=xl6IrCgenEmitSequenceCreate(&out,&ir,id,id,set);
        if (rendered<0) fprintf(stderr,"factory id=%u name=%s layout=%u slot=%u base=%u child=%u child-slot=%u transfer=%u\n",
            (unsigned)id,type->name,(unsigned)plan->abi,(unsigned)plan->slotAbi,
            (unsigned)plan->baseElementType,(unsigned)plan->childElementType,(unsigned)plan->childSlotAbi,(unsigned)plan->transferAbi);
        assert(rendered>0);
        xl6OutputPrintf(&out,
            "    assert(v%u && xl6SequenceAdmitsRoleV1(v%u,%u,0x%016llXULL,0x%016llXULL,%u,sizeof(%s),%s));\n"
            "    xrtValueRelease(v%u); assert(!xrtGetError()); }\n",
            (unsigned)id,(unsigned)id,(unsigned)plan->role,(unsigned long long)type->typeValueId,
            (unsigned long long)element->typeValueId,(unsigned)plan->transferAbi,physicalType,
            xl6IrCgenNativeSlotTypeExpr(plan->slotAbi),(unsigned)id);
    }
    xl6OutputPuts("    /* Explicit Array data-import plans, separate from creation. */\n",&out);
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrTypeId id=(XL6_IrTypeId)(i+1); size_t count=0;
        if (!xl6IrArrayImportChainRecipe(&ir,id,NULL,0,&count)) continue;
        xl6OutputPrintf(&out,"    { xvalue* wire=xrtValueArray(); xvalue* v%u=NULL; assert(wire);\n",(unsigned)id);
        assert(xl6IrCgenEmitArrayImportChain(&out,&ir,id,id,"wire",false)>0);
        xl6OutputPrintf(&out,"    assert(v%u && xl6ArrayErasedSizeV1(v%u)==0);\n"
            "    xrtValueRelease(v%u); xrtValueRelease(wire); assert(!xrtGetError()); }\n",
            (unsigned)id,(unsigned)id,(unsigned)id);
    }
    xl6OutputPuts("    }\n    xrtMemDebugSnapshot(&after);\n"
        "    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);\n"
        "    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);\n"
        "    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);\n"
        "    return 0;\n}\n",&out);
    ok=!xl6OutputHasFailed(&out); xl6OutputClose(&out);
    printf("Sequence recipe emitter: %u actual recipes; %u refused mutations; layout-mask=%u; source carrier unchanged\n",recipes,mutations,layouts);
    printf("Nested schema rejection: %u descendant mutations refused before output\n",nestedMutations);
done:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
