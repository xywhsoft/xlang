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
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_ENTRY",&diags) : xl6IrCgenEmitC(&out,ir,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text);
    }
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if(!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
       !xl6SemCheckModule(parse.root,&parse.diags,argv[1]) ||
       !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags)) goto failed;
    assert(ir.abi.entryArgumentType==0 && ir.abi.entryArgumentCarrierAbi==0 && !ir.abi.entryArgumentElementIdentity);
    ir.abi.entryArgumentCarrierAbi=XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1; reject(&ir); ir.abi.entryArgumentCarrierAbi=0;
    if(!xl6IrLowerModule(&ir,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags)) goto failed;
    bool entry=ir.abi.entryKind==XL6_IR_MODULE_ENTRY_XLANG_MAIN;
    if(entry) {
        XL6_IrModuleAbi saved=ir.abi;
        assert(ir.abi.entryArgumentType && ir.abi.entryArgumentCarrierAbi==XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1);
        assert(ir.abi.entryArgumentElementIdentity==ir.runtimeTypeMap.fallbackTypeValueId);
        ir.abi.entryArgumentType=XL6_IR_TYPE_INVALID; reject(&ir); ir.abi=saved;
        ir.abi.entryArgumentType=xl6IrArrayAnyElementType(&ir,saved.entryArgumentType); reject(&ir); ir.abi=saved;
        ir.abi.entryArgumentCarrierAbi=XL6_IR_COLLECTION_CARRIER_VALUE_V1; reject(&ir); ir.abi=saved;
        ir.abi.entryArgumentElementIdentity=0; reject(&ir); ir.abi=saved;
        ir.abi.entryArgumentElementIdentity^=1; reject(&ir); ir.abi=saved;
    } else {
        ir.abi.entryArgumentType=xl6IrArrayAnyType(&ir); reject(&ir); ir.abi.entryArgumentType=0;
    }
    unsigned arrays=0;
    for(size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* op=&ir.instructions[i];
        if(op->kind!=XL6_IR_INSTR_RUNTIME_CALL) continue;
        assert(op->runtimeOp!=XL6_IR_RUNTIME_XVALUE_ARRAY_NEW);
        if(op->runtimeOp!=XL6_IR_RUNTIME_ARRAY_CREATE || !xl6IrArrayAnyElementType(&ir,op->type)) continue;
        ++arrays; XL6_IrInstruction saved=*op;
        assert(op->payloadType==xl6IrArrayAnyElementType(&ir,op->type));
        op->payloadType=op->type; reject(&ir); *op=saved;
        op->type=op->payloadType; reject(&ir); *op=saved;
    }
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_ENTRY)==entry);
    assert(xl6OutputOpenFile(&out,argv[2])); assert(xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3])); assert(xl6IrCgenEmitHeader(&out,&ir,"NATIVE_ENTRY",&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[4])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
    printf("native entry: arrays=%u mutations=%u selected=%d\n",arrays,mutations,entry);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d %s\n",parse.diags.items[i].line,parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
}
