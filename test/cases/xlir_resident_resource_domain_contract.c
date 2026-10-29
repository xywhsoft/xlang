/* The test mutates actual parsed/typed/lowered source, not a synthetic pool.
 * Refusal must happen before C emission or publication of an artifact plan. */
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
    XL6_ArtifactRuntimePlan plan;
    memset(&plan,0x55,sizeof(plan)); assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetEmpty(&plan.roots) && xl6ArtifactRuntimeSetEmpty(&plan.components));
    assert(!plan.sourceCount && !plan.nativeModuleCount);
}
int main(int argc,char** argv)
{
    assert(argc==3);
    XL6_ParseResult parse; XL6_IrModule ir; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=xl6ParseFile(argv[1],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"resident_domain") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags);
    if (!ok) {
        for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
    }
    assert(xl6IrVerifyModule(&ir,NULL));
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    unsigned mask=0;
    const size_t components[]={XL6_ARTIFACT_QUEUE_SPSC_ADMISSION,XL6_ARTIFACT_QUEUE_MPSC_ADMISSION,XL6_ARTIFACT_QUEUE_MPMC_ADMISSION};
    for (unsigned f=0;f<3;++f) if (xl6ArtifactRuntimeSetHas(&plan.components,components[f])) mask|=1u<<f;
    assert(mask==(unsigned)strtoul(argv[2],NULL,10));
    unsigned domains=0,trees=0,queues=0,mutations=0;
    for (size_t i=0;i<ir.types.count;++i) {
        XL6_IrType* type=&ir.types.items[i]; XL6_IrSequenceLayout original=type->sequenceLayout;
        if (original.abi!=XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1 && original.abi!=XL6_IR_SEQUENCE_LAYOUT_QUEUE_REFERENCE_V1) continue;
        ++domains;
        if (original.abi==XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1) ++trees; else ++queues;
#define BAD(FIELD,VALUE) do { type->sequenceLayout.FIELD=(VALUE); refused(&ir); type->sequenceLayout=original; ++mutations; } while(0)
        BAD(abi,XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2);
        BAD(role,XL6_IR_SEQUENCE_ROLE_NONE); BAD(elementType,0); BAD(baseElementType,0);
        BAD(slotAbi,XL6_IR_NATIVE_SLOT_NONE); BAD(transferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID);
        BAD(codecAbi,(XL6_IrSequenceCodecAbi)UINT32_MAX); BAD(admissionAbi,XL6_VALUE_ADMIT_ANY);
        BAD(nullable,!original.nullable); BAD(defaultAbi,XL6_IR_SEQUENCE_DEFAULT_ZERO);
        BAD(childRole,XL6_IR_SEQUENCE_ROLE_ARRAY); BAD(childElementType,original.baseElementType);
        BAD(childSlotAbi,XL6_IR_NATIVE_SLOT_XVALUE_CARRIER); BAD(childTransferAbi,XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
#undef BAD
        XL6_IrType* base=&ir.types.items[original.baseElementType-1];
        XL6_IrTypeId* schema=original.abi==XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1 ?
            &base->treeValueSchemaType : &base->messageSchemaType;
        XL6_IrTypeId saved=*schema; *schema=0; refused(&ir); *schema=saved; ++mutations;
        XL6_IrTypeId savedParam=type->params[0]; type->params[0]=(XL6_IrTypeId)(i+1);
        refused(&ir); type->params[0]=savedParam; ++mutations;
        assert(xl6IrVerifyModule(&ir,NULL));
    }
    assert(!mask || (domains && queues));
    printf("Resident domains: %u Tree, %u Queue, %u rejected actual-IR mutations; component mask=%u; output retained\n",trees,queues,mutations,mask);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
