#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reject(XL6_IrModule* module)
{
    XL6_DiagList diags={0}; XL6_Output out; size_t length;
    assert(!xl6IrVerifyModule(module,&diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,module,&diags));
    char* text=xl6OutputTakeMemoryText(&out,&length);assert(!length);xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,module,"BAD_NATIVE",&diags));
    text=xl6OutputTakeMemoryText(&out,&length);assert(!length);xrtFree(text);
    xl6DiagFree(&diags);
}
int main(int argc,char** argv)
{
    assert(argc==3); XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    size_t methods=0,fields=0,defaults=0,mutations=0;
    for(size_t m=0;m<unit.ir.dynamicMethodCount;++m) {
        XL6_IrDynamicMethod* method=&unit.ir.dynamicMethods[m];
        XL6_IrCallTarget* target=NULL;
        for(size_t t=0;t<unit.ir.callTargetCount;++t)
            if(unit.ir.callTargets[t].symbol==method->targetSymbol)target=&unit.ir.callTargets[t];
        if(!target || target->kind!=XL6_IR_CALL_TARGET_EXTERN_C ||
            method->adapterAbi!=XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED ||
            target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        assert(xl6IrDynamicMethodCallTargetSupported(&unit.ir,target));++methods;
        for(size_t p=0;p<target->parameterCount;++p) {
            if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW)continue;
            ++fields;XL6_IrNodeId saved=target->parameterPhysicalFieldDeclarations[p];
            target->parameterPhysicalFieldDeclarations[p]=XL6_IR_NODE_INVALID;
            reject(&unit.ir);++mutations;target->parameterPhysicalFieldDeclarations[p]=saved;
            XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK;
            reject(&unit.ir);++mutations;target->parameterPhysicalAbis[p]=abi;
        }
        XL6_IrCallErrorPhysicalAbi physical=target->errorPhysicalAbi;
        target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
        reject(&unit.ir);++mutations;target->errorPhysicalAbi=physical;
        for(size_t p=0;p<method->parameterCount;++p) {
            XL6_IrDynamicMethodParameter* item=&unit.ir.dynamicMethodParameters[method->firstParameter+p];
            if(item->defaultAbi!=XL6_IR_DYNAMIC_METHOD_DEFAULT_I64)continue;
            ++defaults;char* saved=item->defaultLiteral;
            item->defaultLiteral=!strcmp(saved,"0LL") ? "1LL" : "0LL";
            reject(&unit.ir);++mutations;item->defaultLiteral=saved;
        }
    }
    assert(methods>=3 && fields>=4 && defaults>=2 && mutations>=12);
    FILE* bindings=fopen(argv[2],"wb");assert(bindings);size_t exports=0;
    for(size_t i=0;i<unit.ir.exportCount;++i) {
        const XL6_IrExport* entry=&unit.ir.exports[i];
        const char* macro=strstr(entry->cName,"nativeSizeProbe") ? "NATIVE_SIZE_CALL" :
            strstr(entry->cName,"nativeTextProbe") ? "NATIVE_TEXT_CALL" :
            strstr(entry->cName,"nativeAppendProbe") ? "NATIVE_APPEND_CALL" : NULL;
        if(macro){assert(!entry->parameterCount);fprintf(bindings,"#define %s %s\n",macro,entry->cName);++exports;}
    }
    assert(exports==3 && !fclose(bindings));
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    printf("native dynamic methods=%zu fields=%zu defaults=%zu mutations=%zu rejected before output\n",methods,fields,defaults,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
