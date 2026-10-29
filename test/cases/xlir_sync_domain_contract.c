/* Derive the domains from actual source, then corrupt one closed contract
 * field at a time. Verify/Cgen/selection must all refuse before publication. */
#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    XL6_Output out; assert(xl6OutputOpenMemory(&out)); assert(xl6OutputPuts("KEEP",&out)>=0);
    assert(!xl6IrCgenEmitC(&out,ir,NULL)); assert(out.buffer.Size==4 && !memcmp(out.buffer.Data,"KEEP",4));
    xl6OutputDiscard(&out);
    XL6_ArtifactRuntimePlan plan; memset(&plan,0x55,sizeof(plan));
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetEmpty(&plan.roots) && xl6ArtifactRuntimeSetEmpty(&plan.components));
    assert(!plan.sourceCount && !plan.nativeModuleCount);
}
int main(int argc,char** argv)
{
    assert(argc==4); unsigned families=(unsigned)strtoul(argv[2],NULL,10);
    bool recursive=strtoul(argv[3],NULL,10)!=0;
    XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=xl6ParseFile(argv[1],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"sync_domain") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags);
    if (!ok) {
        for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
    }
    assert(xl6IrVerifyModule(&ir,NULL)); XL6_ArtifactRuntimePlan plan;
    unsigned constructors=0;
    for(size_t i=0;i<ir.instructionCount;++i) {
        const XL6_IrInstruction* instruction=&ir.instructions[i];
        if(instruction->kind==XL6_IR_INSTR_RUNTIME_CALL &&
            (instruction->runtimeOp==XL6_IR_RUNTIME_MUTEX_CREATE ||
             instruction->runtimeOp==XL6_IR_RUNTIME_COND_CREATE ||
             instruction->runtimeOp==XL6_IR_RUNTIME_RWLOCK_CREATE ||
             instruction->runtimeOp==XL6_IR_RUNTIME_STACK_CREATE)) {
            ++constructors;
            assert(xl6IrCallInstructionErrorPhysicalAbi(&ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
            unsigned tests=0;
            for(size_t j=0;j<ir.instructionCount;++j) {
                const XL6_IrInstruction* test=&ir.instructions[j];
                if(test->kind==XL6_IR_INSTR_ERROR_TEST && test->operandCount==1 &&
                    ir.operands[test->firstOperand]==instruction->result) ++tests;
            }
            assert(tests==1);
        }
    }
    assert(constructors);
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SYNC_CORE));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_MUTEX)==((families&1)!=0));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_COND)==((families&2)!=0));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_RWLOCK)==((families&4)!=0));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_SYNC_ADMISSION)==recursive);
    unsigned domains=0,mutations=0;
    for(size_t i=0;i<ir.types.count;++i) {
        XL6_IrType* type=&ir.types.items[i]; XL6_IrSequenceLayout original=type->sequenceLayout;
        if(original.abi!=XL6_IR_SEQUENCE_LAYOUT_MUTEX_REFERENCE_V1 &&
            original.abi!=XL6_IR_SEQUENCE_LAYOUT_COND_REFERENCE_V1 &&
            original.abi!=XL6_IR_SEQUENCE_LAYOUT_RWLOCK_REFERENCE_V1) continue;
        assert(original.admissionAbi==XL6_VALUE_ADMIT_NONE); ++domains;
#define BAD(FIELD,VALUE) do {type->sequenceLayout.FIELD=(VALUE); refused(&ir); type->sequenceLayout=original; ++mutations;} while(0)
        BAD(abi,XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2); BAD(role,XL6_IR_SEQUENCE_ROLE_NONE);
        BAD(elementType,0); BAD(baseElementType,0); BAD(slotAbi,XL6_IR_NATIVE_SLOT_NONE);
        BAD(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID); BAD(codecAbi,XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1);
        BAD(admissionAbi,XL6_VALUE_ADMIT_ANY); BAD(nullable,!original.nullable);
        BAD(defaultAbi,original.nullable ? XL6_IR_SEQUENCE_DEFAULT_FORBIDDEN : XL6_IR_SEQUENCE_DEFAULT_ZERO);
        BAD(childRole,XL6_IR_SEQUENCE_ROLE_ARRAY); BAD(childElementType,original.baseElementType);
#undef BAD
        XL6_IrType* base=&ir.types.items[original.baseElementType-1]; XL6_IrType saved=*base;
        assert(base->xvalueAdmissionAbi==XL6_VALUE_ADMIT_NONE);
#define BASE(FIELD,VALUE) do {base->FIELD=(VALUE); refused(&ir); *base=saved; ++mutations;} while(0)
        BASE(runtimeResourceAbi,XL6_IR_RUNTIME_RESOURCE_SEM); BASE(kind,XL6_IR_TYPE_CLASS);
        BASE(cAbi,XL6_IR_C_ABI_RAW_POINTER); BASE(ownershipAbi,XL6_IR_OWNERSHIP_ABI_TRIVIAL);
        BASE(nullAbi,XL6_IR_NULL_ABI_POINTER_SENTINEL); BASE(typeValueId,0); BASE(returnCount,1);
#undef BASE
        assert(xl6IrVerifyModule(&ir,NULL));
    }
    assert(!recursive || domains);
    printf("Sync closed domain: %u schemas, %u actual-IR mutations rejected; native components selected\n",domains,mutations);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
