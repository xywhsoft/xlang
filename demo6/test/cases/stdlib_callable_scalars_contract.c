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
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_SCALAR",&diags) : xl6IrCgenEmitC(&out,ir,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(text);
    }
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,argv[1]) ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags) ||
        !xl6IrLowerModule(&ir,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags)) goto failed;
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
    bool unused=strstr(argv[1],"_unused")!=NULL;
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_ADMISSION)==!unused);
    xlirCallableBridgeMutations(&ir,reject);
    unsigned f32=0,time=0,bindings=0;
    for (size_t i=0;i<ir.callableBridgeCount;++i) {
        const XL6_IrCallableBridge* b=&ir.callableBridges[i];
        for (size_t p=0;p<b->parameterCount+b->resultCount;++p) {
            XL6_IrCallableBridgeValueAbi decode,encode; XL6_IrTypeId id;
            if (p<b->parameterCount) {
                const XL6_IrCallableBridgeParameter* s=&ir.callableBridgeParameters[b->firstParameter+p];
                decode=s->decodeAbi; encode=s->encodeAbi; id=s->type;
                assert(s->decodedDestroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NONE || decode!=XL6_IR_CALLABLE_BRIDGE_F32);
            } else {
                const XL6_IrCallableBridgeResult* s=&ir.callableBridgeResults[b->firstResult+p-b->parameterCount];
                decode=s->decodeAbi; encode=s->encodeAbi; id=s->type;
            }
            const XL6_IrType* t=xl6IrTypeGet(&ir.types,id); assert(t);
            if (t->cAbi==XL6_IR_C_ABI_F32) { assert(decode==XL6_IR_CALLABLE_BRIDGE_F32 && encode==decode); ++f32; }
            if (t->cAbi==XL6_IR_C_ABI_TIME_I64) { assert(decode==XL6_IR_CALLABLE_BRIDGE_TIME && encode==decode); ++time; }
        }
    }
    assert(f32 && (unused || time));
    assert(xl6OutputOpenFile(&out,argv[2])); assert(xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3]));
    for (size_t i=0;i<ir.storageCount;++i) {
        const XL6_IrStorage* s=&ir.storages[i];
        const XL6_IrNode* n=xl6IrNodeGet(&ir,s->declaration); const char* name=NULL;
        if (s->kind!=XL6_IR_STORAGE_GLOBAL || !n || !n->text) continue;
        if (!strcmp(n->text,"scalarNativeF32")) name="F32";
        else if (!strcmp(n->text,"scalarNativeTime")) name="TIME";
        else if (!strcmp(n->text,"scalarNativeMixed")) name="MIXED";
        if (!name) continue;
        const XL6_IrCallableBridge* b=xl6IrCallableBridgeGet(&ir,s->type); assert(b && b->reverseSupported);
        xl6OutputPrintf(&out,"#define SCALAR_%s_NATIVE s%u\n#define SCALAR_%s_FORWARD xlir_xcall_entry_t%u\n"
            "#define SCALAR_%s_REVERSE xlir_xvalue_closure_entry_t%u\n",name,(unsigned)s->symbol,
            name,(unsigned)s->type,name,(unsigned)s->type);
        if (!strcmp(name,"MIXED")) xl6OutputPrintf(&out,"#define SCALAR_MIXED_RESULT xlir_product_t%u\n"
            "#define SCALAR_MIXED_DROP xlir_product_drop_t%u\n",(unsigned)b->resultType,(unsigned)b->resultType);
        ++bindings;
    }
    assert(unused ? !bindings : bindings==3); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[4])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
    printf("scalar bridge: f32=%u time=%u mutations=%u resident-reader=%d\n",f32,time,mutations,!unused);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
    return 1;
}
