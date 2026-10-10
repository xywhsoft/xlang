#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static void reject(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_VALUE_COPY",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(int argc,char** argv)
{
    assert((argc==3 || argc==4) && fixture_contract(argv[1]));
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    if (argc==4) {
        assert(!strcmp(argv[3],"absent"));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY_GRAPH) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_GRAPH) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE));
        xl6CompileUnitFree(&unit); puts("Leaf/scalar actual plan excludes List copy/graph providers: verified"); return 0;
    }
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY) &&
        xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY_GRAPH) &&
        xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_GRAPH));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ValueCopyV1")==XL6_ARTIFACT_VALUE_COPY &&
        xl6ArtifactRuntimeSymbolOwner("xl6ValueCloneGraphV1")==XL6_ARTIFACT_VALUE_COPY_GRAPH &&
        xl6ArtifactRuntimeSymbolOwner("xl6ListGraphProviderV1")==XL6_ARTIFACT_LIST_GRAPH);
    unsigned operations=0,mutations=0,providers[4]={0};
    for (size_t i=0;i<unit.ir.instructionCount;++i) {
        XL6_IrInstruction* ins=&unit.ir.instructions[i];
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || (ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_CLONE &&
            ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_DEEP_CLONE)) continue;
        XL6_IrValueCopyProviderAbi provider=xl6IrValueCopyProviderAbi(&unit.ir,ins);
        assert(provider>XL6_IR_VALUE_COPY_INVALID && provider<=XL6_IR_VALUE_COPY_ARRAY_LIST_WIRE_V1); ++providers[provider];
        assert(xl6IrCallInstructionErrorAbi(&unit.ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(&unit.ir,ins)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        XL6_IrInstruction saved=*ins;
        ins->operandCount=0; assert(xl6IrValueCopyProviderAbi(&unit.ir,ins)==XL6_IR_VALUE_COPY_INVALID);
        reject(&unit.ir); *ins=saved; ++mutations;
        ins->type=XL6_IR_TYPE_INVALID; assert(xl6IrValueCopyProviderAbi(&unit.ir,ins)==XL6_IR_VALUE_COPY_INVALID);
        reject(&unit.ir); *ins=saved; ++mutations;
        assert(i+1<unit.ir.instructionCount); XL6_IrInstruction* test=&unit.ir.instructions[i+1]; saved=*test;
        assert(test->kind==XL6_IR_INSTR_ERROR_TEST);
        test->falseTarget=test->trueTarget; reject(&unit.ir); *test=saved; ++mutations;
        test->kind=XL6_IR_INSTR_DISCARD; reject(&unit.ir); *test=saved; ++mutations; ++operations;
    }
    assert(operations==5 && mutations==20 && providers[XL6_IR_VALUE_COPY_ARRAY_WIRE_V1]==1 &&
        providers[XL6_IR_VALUE_COPY_LIST_V1]==1 && providers[XL6_IR_VALUE_COPY_ARRAY_LIST_WIRE_V1]==3);
    XL6_Output out; assert(xl6OutputOpenFile(&out,argv[2])); unsigned count=0;
    for (size_t i=0;i<unit.ir.functionCount;++i) {
        const XL6_IrFunction* f=&unit.ir.functions[i]; if (!f->debugName || !f->parameterCount) continue;
        if (strcmp(f->debugName,"copyOpen") && strcmp(f->debugName,"cloneOpen") && strcmp(f->debugName,"cloneList")) continue;
        const XL6_IrType* result=xl6IrTypeGet(&unit.ir.types,f->returnType); assert(result);
        xl6OutputPrintf(&out,"#define XL6_COPY_%s xlir_fn_%u\n#define XL6_COPY_%s_RESULT %s\n",
            f->debugName,(unsigned)f->symbol,f->debugName,result->errorResultCName); ++count;
        if (!strcmp(f->debugName,"cloneList")) {
            const XL6_IrStorage* p=xl6IrStorageGet(&unit.ir,unit.ir.functionParameters[f->firstParameter]);
            const XL6_IrType* owner=p ? xl6IrTypeGet(&unit.ir.types,p->type) : NULL;
            const XL6_IrType* schema=owner ? xl6IrTypeGet(&unit.ir.types,owner->listValueSchemaType) : NULL;
            const XL6_IrType* element=owner ? xl6IrTypeGet(&unit.ir.types,owner->params[0]) : NULL; assert(schema && element);
            xl6OutputPrintf(&out,"#define XL6_COPY_OWNER UINT64_C(0x%016llX)\n"
                "#define XL6_COPY_SCHEMA UINT64_C(0x%016llX)\n#define XL6_COPY_ELEMENT UINT64_C(0x%016llX)\n",
                (unsigned long long)owner->typeValueId,(unsigned long long)schema->typeValueId,(unsigned long long)element->typeValueId);
        }
    }
    for (size_t i=0;i<unit.ir.storageCount;++i) {
        const XL6_IrStorage* s=&unit.ir.storages[i]; if (s->kind!=XL6_IR_STORAGE_GLOBAL) continue;
        for (size_t j=0;j<unit.ir.nodeCount;++j) {
            const XL6_IrNode* n=&unit.ir.nodes[j];
            if (n->symbol==s->symbol && n->text && !strcmp(n->text,"valueCopyCleanups")) {
                xl6OutputPrintf(&out,"#define XL6_COPY_CLEANUPS s%u\n",(unsigned)s->symbol); ++count; break;
            }
        }
    }
    assert(count==4); xl6OutputClose(&out); xl6CompileUnitFree(&unit);
    printf("Value copy %u operations, %u pre-output mutations, provider closure and bindings verified\n",operations,mutations); return 0;
}
