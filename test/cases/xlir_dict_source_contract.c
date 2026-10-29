#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned mutations;
static void refuse(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_DICT_FACTORY",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==3 || argc==2);
    if (argc==2) {
        assert(fixture_contract(argv[1])); XL6_CompileUnit leaf; xl6CompileUnitInit(&leaf);
        assert(xl6CompileUnitLoadFile(&leaf,argv[1],1) && xl6IrLowerModule(&leaf.ir,&leaf.parse.diags));
        XL6_ArtifactRuntimePlan plan={0}; assert(xl6ArtifactRuntimePlanBuild(&leaf.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&leaf.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_METHODS));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_NATIVE));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_NATIVE));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_SCHEMA));
        xl6CompileUnitFree(&leaf); puts("Dict leaf closure excludes unrelated List/Set/schema providers"); return 0;
    }
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    if (!xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
        for (size_t i=0;i<unit.ir.types.count;++i) {
            const XL6_IrType* t=&unit.ir.types.items[i];
            fprintf(stderr,
                "Factory type=%zu name=%s kind=%u params=%zu schema=%u key=%u recipe=%d\n",
                i+1,t->name ? t->name : "?",(unsigned)t->kind,t->paramCount,
                (unsigned)t->collectionValueSchemaType,(unsigned)t->dictKeyAbi,
                xl6IrDictConstructionRecipe(&unit.ir,(XL6_IrTypeId)(i+1),NULL,NULL,NULL));
        }
        xl6DiagPrintText(&unit.parse.diags,argv[1]); return 1;
    }
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL)); unsigned creates=0,methods=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL) continue;
        if (ins->runtimeOp==XL6_IR_RUNTIME_DICT_CREATE) {
            assert(xl6IrDictCreateRuntimeInstructionCompatible(ir,ins));
            assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE &&
                xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT &&
                xl6IrValueOwnershipGet(ir,ins->result)==XL6_IR_OWNERSHIP_MOVE);
            XL6_IrInstruction saved=*ins;
#define BAD(Member,Value) do { ins->Member=(Value); refuse(ir); *ins=saved; } while(0)
            BAD(collectionAbi,XL6_IR_COLLECTION_NONE); BAD(collectionAbi,XL6_IR_COLLECTION_LIST);
            BAD(operandCount,1); BAD(type,XL6_IR_TYPE_INVALID); BAD(payloadType,XL6_IR_TYPE_INVALID);
            BAD(runtimeIdentityType,saved.type); BAD(targetValue,saved.result); BAD(targetSymbol,UINT32_MAX);
            BAD(immediateText,(char*)"wire"); BAD(opcode,XL6_IR_OPCODE_ADD);
            BAD(sequenceAdmissionIdentitySet,UINT32_MAX); BAD(collectionAdmissionAbi,XL6_IR_COLLECTION_ADMISSION_PRIMITIVE);
            BAD(runtimeOp,XL6_IR_RUNTIME_XVALUE_OBJECT_NEW);
#undef BAD
            XL6_IrType* type=&ir->types.items[ins->type-1];
            XL6_IrDictKeyAbi key=type->dictKeyAbi; type->dictKeyAbi=XL6_IR_DICT_KEY_NONE;
            refuse(ir); type->dictKeyAbi=key;
            XL6_IrTypeId schema=type->collectionValueSchemaType; type->collectionValueSchemaType=0;
            refuse(ir); type->collectionValueSchemaType=schema; ++creates;
        }
        if (ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_DICT_MERGE_RIGHT ||
            ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_DICT_WITHOUT_STRING_KEY ||
            (ins->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD && ins->collectionAbi==XL6_IR_COLLECTION_DICT)) {
            assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE); ++methods;
        }
    }
    assert(creates>=10 && methods>=7 && xl6IrVerifyModule(ir,NULL));
    assert(fixture_contract(argv[1]));
    XL6_Output out; unsigned bindings=0; assert(xl6OutputOpenFile(&out,argv[2]));
    for (size_t i=0;i<ir->functionCount;++i) {
        const XL6_IrFunction* f=&ir->functions[i];
        if (!f->debugName || strcmp(f->debugName,"dictFactoryRoundtrip")) continue;
        const XL6_IrType* t=xl6IrTypeGet(&ir->types,f->returnType); assert(t && t->errorResultCName);
        xl6OutputPrintf(&out,"#define XL6_DICT_FACTORY_ROUNDTRIP xlir_fn_%u\n#define XL6_DICT_FACTORY_RESULT %s\n",
            (unsigned)f->symbol,t->errorResultCName); ++bindings;
    }
    for (size_t i=0;i<ir->storageCount;++i) {
        const XL6_IrStorage* s=&ir->storages[i]; if (s->kind!=XL6_IR_STORAGE_GLOBAL) continue;
        for (size_t j=0;j<ir->nodeCount;++j) {
            const XL6_IrNode* n=&ir->nodes[j]; if (n->symbol!=s->symbol || !n->text) continue;
            const char* name=!strcmp(n->text,"numbers") ? "NUMBERS" : !strcmp(n->text,"nested") ? "NESTED" : NULL;
            if (name) { xl6OutputPrintf(&out,"#define XL6_DICT_FACTORY_%s s%u\n",name,(unsigned)s->symbol); ++bindings; break; }
        }
    }
    assert(bindings==3); xl6OutputClose(&out);
    printf("Dict source contract creates=%u methods=%u mutations=%u\n",creates,methods,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
