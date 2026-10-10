#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t length=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REFUSED_ITERATOR",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(int argc,char** argv)
{
    assert(argc==4); unsigned mutations=0,bindings=0;
    for (unsigned lane=1;lane<3;++lane) {
        assert(fixture_contract(argv[lane])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_VALUE_ITERATOR)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE)==(lane==1));
        for (size_t i=0;i<unit.ir.instructionCount;++i) {
            XL6_IrInstruction* item=&unit.ir.instructions[i];
            if (item->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
                (item->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE &&
                 item->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED)) continue;
            XL6_IrInstruction saved=*item;
#define MUTATE(Field,Value) do { item->Field=(Value); refused(&unit.ir); *item=saved; ++mutations; } while(0)
            MUTATE(type,XL6_IR_TYPE_INVALID); MUTATE(operandCount,0); MUTATE(operandCount,2);
            MUTATE(firstOperand,SIZE_MAX); MUTATE(targetSymbol,UINT32_MAX);
            MUTATE(calleeType,saved.type); MUTATE(opcode,XL6_IR_OPCODE_ADD);
            MUTATE(immediateText,(char*)"foreign");
#undef MUTATE
        }
        if (lane==1) {
            XL6_Output out; assert(xl6OutputOpenFile(&out,argv[3]));
            for (size_t i=0;i<unit.ir.functionCount;++i) {
                const XL6_IrFunction* f=&unit.ir.functions[i]; if (!f->debugName) continue;
                const char* token=!strcmp(f->debugName,"visitList") ? "LIST" :
                    !strcmp(f->debugName,"visitErased") ? "ERASED" : !strcmp(f->debugName,"visitBreak") ? "BREAK" : NULL;
                if (!token) continue;
                const XL6_IrType* type=xl6IrTypeGet(&unit.ir.types,f->returnType); assert(type && type->errorResultCName);
                xl6OutputPrintf(&out,"#define XL6_ITERATE_%s xlir_fn_%u\n#define XL6_ITERATE_%s_RESULT %s\n",
                    token,(unsigned)f->symbol,token,type->errorResultCName); ++bindings;
            }
            for (size_t i=0;i<unit.ir.types.count;++i) {
                const XL6_IrType* type=&unit.ir.types.items[i];
                if (type->collectionAbi!=XL6_IR_COLLECTION_LIST || type->paramCount!=1 || !type->params) continue;
                const XL6_IrType* element=xl6IrTypeGet(&unit.ir.types,type->params[0]);
                if (!element || element->cAbi!=XL6_IR_C_ABI_MANAGED_STRING || element->kind==XL6_IR_TYPE_NULLABLE) continue;
                const XL6_IrType* schema=xl6IrTypeGet(&unit.ir.types,type->listValueSchemaType); assert(schema);
                xl6OutputPrintf(&out,"#define XL6_ITERATE_OWNER 0x%016llXULL\n#define XL6_ITERATE_WITNESS 0x%016llXULL\n#define XL6_ITERATE_ELEMENT 0x%016llXULL\n",
                    (unsigned long long)type->typeValueId,(unsigned long long)schema->typeValueId,
                    (unsigned long long)element->typeValueId); ++bindings;
            }
            for (size_t i=0;i<unit.ir.storageCount;++i) {
                const XL6_IrStorage* storage=&unit.ir.storages[i]; if (storage->kind!=XL6_IR_STORAGE_GLOBAL) continue;
                for (size_t j=0;j<unit.ir.nodeCount;++j) {
                    const XL6_IrNode* node=&unit.ir.nodes[j];
                    if (node->symbol==storage->symbol && node->text && !strcmp(node->text,"iteratorCleanups")) {
                        xl6OutputPrintf(&out,"#define XL6_ITERATE_CLEANUPS s%u\n",(unsigned)storage->symbol); ++bindings; break;
                    }
                }
            }
            assert(bindings==5); xl6OutputClose(&out);
        }
        xl6CompileUnitFree(&unit);
    }
    assert(mutations>=24); printf("List iterator: %u mutations refused before C/header output; Array excludes List\n",mutations);
    return 0;
}
