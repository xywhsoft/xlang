/* Compile/lower/verify only. No network/decoder execution. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,refusals,enums,resources;
static void refused(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target)&&!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output output;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_FRAME",&errors):xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);}
    ++refusals;
}
static bool selected(const char* name)
{return name&&(!strncmp(name,"xl6NetBuffer",sizeof("xl6NetBuffer")-1)||!strncmp(name,"xl6NetFrameResult",sizeof("xl6NetFrameResult")-1)||
    !strncmp(name,"xl6NetLineFramer",sizeof("xl6NetLineFramer")-1)||!strncmp(name,"xl6NetLengthFramer",sizeof("xl6NetLengthFramer")-1));}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    XL6_IrTypeId buffer=0;
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* t=module->callTargets+i;
        if(t->cName&&!strcmp(t->cName,"xl6NetBufferCreateV1"))buffer=t->returnType;}
    assert(buffer);
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=module->callTargets+i;
        if(!selected(target->cName))continue;
        XL6_IrCallTarget saved=*target;++targets;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
#define BAD(Field,Value) do{target->Field=(Value);refused(module,target);*target=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT){
            ++resources;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);
            BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);}
        const XL6_IrType* returned=xl6IrTypeGet(&module->types,saved.returnType);
        if(returned&&returned->kind==XL6_IR_TYPE_NUMERIC_ENUM){
            ++enums;XL6_IrEnumLayout* layout=(XL6_IrEnumLayout*)xl6IrEnumLayoutGet(module,saved.returnType);assert(layout&&layout->variantCount==2);
            size_t n=layout->variantCount;--layout->variantCount;refused(module,target);layout->variantCount=n;
            XL6_IrEnumVariant* member=module->enumVariants+layout->firstVariant;char* name=member->name;
            member->name="invalidFrameStatus";refused(module,target);member->name=name;}
        for(size_t p=0;p<target->parameterCount;++p){
            XL6_IrTypeId old=target->parameterTypes[p];const XL6_IrType* type=xl6IrTypeGet(&module->types,old);
            if(type&&type->kind==XL6_IR_TYPE_CLASS){
                XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
                target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module,target);target->parameterPhysicalAbis[p]=abi;
                if(old!=buffer){target->parameterTypes[p]=buffer;refused(module,target);target->parameterTypes[p]=old;}}
        }
        assert(xl6IrVerifyModule(module,NULL));
#undef BAD
    }
    assert(targets==58&&enums==1&&resources==6&&refusals>350);xl6CompileUnitFree(&unit);
    printf("Frame XLIR: %zu targets, %zu nominal status, %zu owned results, %zu corrupt contracts refused without C/header\n",targets,enums,resources,refusals);return 0;
}
