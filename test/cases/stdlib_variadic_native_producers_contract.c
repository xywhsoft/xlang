#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; assert(!xl6IrVerifyModule(ir,&diags));
    for(unsigned header=0;header<2;++header){
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_PRODUCER",&diags) : xl6IrCgenEmitC(&out,ir,&diags)));
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
       !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags)) goto failed;
    for(size_t i=0;i<ir.dynamicMethodCount;++i)
        assert(ir.dynamicMethods[i].variadicCarrierAbi==XL6_IR_COLLECTION_CARRIER_NONE && !ir.dynamicMethods[i].variadicElementIdentity);
    if(!xl6IrLowerModule(&ir,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags)) goto failed;
    for(size_t i=0;i<ir.types.count;++i)
        if(xl6IrArrayAnyElementType(&ir,(XL6_IrTypeId)(i+1))){
            assert(ir.types.items[i].typeValueId==ir.runtimeTypeMap.arrayTypeValueId);
            if(argc==5){
                uint64_t saved=ir.types.items[i].typeValueId;
                ir.types.items[i].typeValueId^=1; reject(&ir);
                ir.types.items[i].typeValueId=saved;
            }
        }
    unsigned producers=0,methods=0;
    for(size_t i=0;i<ir.instructionCount;++i){
        XL6_IrInstruction* op=&ir.instructions[i];
        if(op->kind!=XL6_IR_INSTR_RUNTIME_CALL) continue;
        assert(op->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ARRAY_NEW);
        if(op->runtimeOp!=XL6_IR_RUNTIME_ARRAY_CREATE || !xl6IrArrayAnyElementType(&ir,op->type)) continue;
        assert(op->payloadType==xl6IrArrayAnyElementType(&ir,op->type) && !op->sequenceAdmissionIdentitySet);
        ++producers;
        if(argc==5){
            XL6_IrInstruction saved=*op;
            op->payloadType=op->type; reject(&ir); *op=saved;
            op->collectionAbi=XL6_IR_COLLECTION_DICT; reject(&ir); *op=saved;
            op->sequenceAdmissionIdentitySet=UINT32_MAX; reject(&ir); *op=saved;
            op->runtimeIdentityType=op->type; reject(&ir); *op=saved;
        }
    }
    for(size_t i=0;i<ir.dynamicMethodCount;++i){
        XL6_IrDynamicMethod* method=&ir.dynamicMethods[i];
        if(method->variadicAbi!=XL6_IR_CALL_VARIADIC_ARRAY) continue;
        assert(method->adapterAbi==XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED &&
            method->variadicCarrierAbi==XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 &&
            method->variadicElementIdentity==ir.runtimeTypeMap.fallbackTypeValueId);
        ++methods;
        if(argc==5){
            XL6_IrDynamicMethod saved=*method;
            method->variadicCarrierAbi=XL6_IR_COLLECTION_CARRIER_VALUE_V1; reject(&ir); *method=saved;
            method->variadicElementIdentity=0; reject(&ir); *method=saved;
            method->variadicElementIdentity^=1; reject(&ir); *method=saved;
        }
    }
    assert(producers && methods);
    XL6_ArtifactRuntimePlan plan; bool unused=strstr(argv[1],"_unused")!=NULL;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_CALL_FRAME)==!unused);
    assert(xl6IrVerifyModule(&ir,NULL));
    assert(xl6OutputOpenFile(&out,argv[2])); assert(xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3])); assert(xl6IrCgenEmitHeader(&out,&ir,"NATIVE_PRODUCERS",&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[4])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
    printf("native producers: arrays=%u methods=%u mutations=%u selected=%d\n",producers,methods,mutations,!unused);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
