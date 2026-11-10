/* Reject forged owning nullable contracts before any C/header is emitted. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"forged nullable factory accepted: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output output;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,ir,"BAD_NULLABLE_FACTORY",NULL):xl6IrCgenEmitC(&output,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&output,&bytes);assert(text && !bytes);
        xrtFree(text);xl6OutputDiscard(&output);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned found=0;
    for(size_t i=0;i<ir->callTargetCount;++i){
        XL6_IrCallTarget* target=&ir->callTargets[i];
        if(!target->cName || (strcmp(target->cName,"nullableFactoryCreate") && strcmp(target->cName,"nullableFactoryMethod")))continue;
        const XL6_IrType* result=xl6IrTypeGet(&ir->types,target->returnType);
        if(!result || result->kind!=XL6_IR_TYPE_NULLABLE)continue;
        ++found;assert(result->paramCount==1 && result->params);
        XL6_IrType* native=(XL6_IrType*)xl6IrTypeGet(&ir->types,result->params[0]);
        XL6_IrNode* declaration=&ir->nodes[target->declaration-1];XL6_ForeignCallFacts facts=declaration->foreignCall;
        XL6_ForeignLifetimeAbi lifetime=native->foreignLifetime;
        native->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(ir,"caller storage");native->foreignLifetime=lifetime;
        native->foreignLifetime=XL6_FOREIGN_LIFETIME_OWNER;refused(ir,"dependent owner view");native->foreignLifetime=lifetime;
        char* policy=native->foreignPolicy;native->foreignPolicy=NULL;refused(ir,"missing root policy");native->foreignPolicy=policy;
        declaration->foreignCall.inplaceOutput=XL6_FOREIGN_INPLACE_STATUS;refused(ir,"in-place allocation");declaration->foreignCall=facts;
        declaration->foreignCall.outputIndexPlusOne=0;refused(ir,"missing output slot");declaration->foreignCall=facts;
        declaration->foreignCall.rollbackSymbol=0;refused(ir,"missing owner rollback");declaration->foreignCall=facts;
        declaration->foreignCall.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_OWNER_NATIVE;
        refused(ir,"borrowed child instead of owning factory");declaration->foreignCall=facts;
        uint64 flags=declaration->flags;declaration->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;
        refused(ir,"raising C producer");declaration->flags=flags;
        declaration->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(ir,"pure owning factory");declaration->flags=flags;
        XL6_IrCallTarget* rollback=(XL6_IrCallTarget*)xl6IrCallTargetGet(ir,facts.rollbackSymbol);
        assert(rollback && rollback->parameterTypes[0]==result->params[0]);
        XL6_IrTypeId old=rollback->parameterTypes[0];rollback->parameterTypes[0]=target->returnType;
        refused(ir,"nullable rollback argument hides the exact non-null duty");rollback->parameterTypes[0]=old;
    }
    assert(found>=2 && mutations>=20 && xl6IrVerifyModule(ir,NULL));
    printf("%u owning nullable targets: %u forged contracts rejected before C/header output\n",found,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
