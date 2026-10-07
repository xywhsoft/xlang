#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_callable_bridge_mutations.h"
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; assert(!xl6IrVerifyModule(ir,&diags));
    for(unsigned header=0;header<2;++header){
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_FRAME",&diags) : xl6IrCgenEmitC(&out,ir,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(text);
    }
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5 || argc==6); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if(!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
       !xl6SemCheckModule(parse.root,&parse.diags,argv[1]) ||
       !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags) ||
       !xl6IrVerifyModule(&ir,&parse.diags)) goto failed;
    unsigned arrays=0,dicts=0,fixed=0;
    for(size_t i=0;i<ir.callableBridgeCount;++i){
        const XL6_IrCallableBridge* b=&ir.callableBridges[i];
        if(b->erasureAbi==XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC){
            assert(b->variadicCarrierAbi==XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 && b->variadicElementIdentity &&
                   b->variadicElementIdentity==ir.runtimeTypeMap.fallbackTypeValueId); ++arrays;
        } else {assert(!b->variadicElementIdentity);
            if(b->erasureAbi==XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC){assert(b->variadicCarrierAbi==XL6_IR_COLLECTION_CARRIER_VALUE_V1); ++dicts;}
            else {assert(b->variadicCarrierAbi==XL6_IR_COLLECTION_CARRIER_NONE); ++fixed;}
        }
    }
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
    bool unused=strstr(argv[1],"_unused")!=NULL;
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_CALL_FRAME)==!unused);
    assert(arrays && (unused ? fixed : dicts));
    if(argc==5) {
        xlirCallableBridgeMutations(&ir,reject);
        unsigned nulls=0;
        for(size_t i=0;i<ir.dynamicCallArgumentCount;++i){
            XL6_IrDynamicCallArgument* a=&ir.dynamicCallArguments[i];
            XL6_IrNode* node=&ir.nodes[a->expression-1];
            if(node->sourceKind!=XL6_AST_NULL) continue;
            assert(a->admission==XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_VERIFIED);
            XL6_IrDynamicCallArgument saved=*a;
            a->admission=XL6_IR_DYNAMIC_CALL_ARGUMENT_ADMISSION_UNSUPPORTED_BOX_ABI; reject(&ir); *a=saved;
            XL6_IrNode original=*node;
            node->sourceKind=XL6_AST_INT; reject(&ir); *node=original;
            node->storageType=ir.callableBridges[0].signatureType; reject(&ir); *node=original;
            ++nulls;
        }
        assert(unused || nulls);
    }
    assert(xl6IrVerifyModule(&ir,NULL));
    assert(xl6OutputOpenFile(&out,argv[2])); assert(xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3])); assert(xl6IrCgenEmitHeader(&out,&ir,"FRAME_ARRAY",&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[4])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
    printf("native frame plans: arrays=%u dicts=%u fixed=%u mutations=%u selected=%d\n",arrays,dicts,fixed,mutations,!unused);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
