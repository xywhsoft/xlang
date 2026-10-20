/* Offline canonical IR facts only: does not run a getter, socket or service. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned refusals;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_NATIVE_GETTER",&diagnostics):
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;unsigned targets=0,sites=0,calls=0,rollbacks=0,nullable=0;
    assert(xl6IrVerifyModule(m,NULL));
    for(size_t n=0;n<m->callSiteCount;++n){
        XL6_IrCallSite* site=&m->callSites[n];
        XL6_IrCallTarget* target=(XL6_IrCallTarget*)xl6IrCallTargetGet(m,site->targetSymbol);
        XL6_IrNode* declaration=target?(XL6_IrNode*)xl6IrNodeGet(m,target->declaration):NULL;
        if(!target||target->kind!=XL6_IR_CALL_TARGET_NATIVE_FACTORY||!declaration||declaration->sourceKind!=XL6_AST_GET)continue;
        XL6_IrPropertyAccessorSignature signature;
        assert(xl6IrGetPropertyAccessorSignature(m,site,&signature));
        assert(signature.resultType==target->returnType&&signature.parameterCount==site->argumentCount);
        assert(xl6IrCallTargetNativeClassResultField(m,target));
        assert(xl6IrCallTargetPhysicalResultType(m,target)!=signature.resultType);
        if(site->receiverGate==XL6_IR_CALL_RECEIVER_NULLABLE_GETTER)++nullable;
        XL6_IrCallTarget saved=*target;XL6_IrNode node=*declaration;XL6_IrCallSite savedSite=*site;
        target->resultPhysicalFieldDeclaration=XL6_IR_NODE_INVALID;
        assert(!xl6IrGetPropertyAccessorSignature(m,site,&signature));refused(m);*target=saved;
        target->resultPhysicalDestroySymbol=0;
        assert(!xl6IrGetPropertyAccessorSignature(m,site,&signature));refused(m);*target=saved;
        target->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;
        assert(!xl6IrGetPropertyAccessorSignature(m,site,&signature));refused(m);*target=saved;
        XL6_IrParameterAbi receiver=target->parameterAbis[0];
        target->parameterAbis[0]=XL6_IR_PARAMETER_ABI_VALUE;
        assert(!xl6IrGetPropertyAccessorSignature(m,site,&signature));refused(m);target->parameterAbis[0]=receiver;
        declaration->sourceKind=XL6_AST_CREATE;
        assert(!xl6IrGetPropertyAccessorSignature(m,site,&signature));refused(m);*declaration=node;
        site->dispatchKind=XL6_IR_CALL_DISPATCH_CLASS;site->dispatchSlot=0;
        assert(!xl6IrGetPropertyAccessorSignature(m,site,&signature));refused(m);*site=savedSite;
        ++sites;
    }
    for(size_t n=0;n<m->callTargetCount;++n){
        const XL6_IrCallTarget* target=&m->callTargets[n];
        const XL6_IrNode* declaration=xl6IrNodeGet(m,target->declaration);
        if(target->kind==XL6_IR_CALL_TARGET_NATIVE_FACTORY&&declaration&&declaration->sourceKind==XL6_AST_GET)++targets;
    }
    for(size_t n=0;n<m->instructionCount;++n){
        XL6_IrInstruction* call=&m->instructions[n];
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(m,call->targetSymbol);
        const XL6_IrNode* declaration=target?xl6IrNodeGet(m,target->declaration):NULL;
        if(call->kind==XL6_IR_INSTR_CALL&&target&&target->kind==XL6_IR_CALL_TARGET_NATIVE_FACTORY&&
            declaration&&declaration->sourceKind==XL6_AST_GET){
            assert(call->type==xl6IrCallTargetPhysicalResultType(m,target));
            XL6_IrTypeId saved=call->type;call->type=target->returnType;refused(m);call->type=saved;++calls;
        }
        if(call->kind==XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK){
            XL6_SymbolId saved=call->targetSymbol;call->targetSymbol=0;refused(m);call->targetSymbol=saved;++rollbacks;
        }
    }
    assert(targets==2&&sites>=4&&calls>=4&&nullable>=2&&rollbacks>=calls&&xl6IrVerifyModule(m,NULL));
    printf("Native getter XLIR: %u factories, %u sites (%u nullable), %u physical calls, %u owned rollbacks, %u exact refusals; no C/header on corruption\n",
        targets,sites,nullable,calls,rollbacks,refusals);
    xl6CompileUnitFree(&unit);return 0;
}
