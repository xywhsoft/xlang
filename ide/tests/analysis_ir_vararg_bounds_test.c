#include "src/ir/xl6_ir.h"
#include <stdio.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ir_vararg_bounds_test:%d: %s\n",__LINE__,#v);return 1;}}while(0)
int main(void)
{
    XL6_IrNode nodes[2]={{.storageType=1},{.storageType=2}};
    XL6_IrType types[2]={{.cAbi=XL6_IR_C_ABI_MANAGED_STRING},{.cAbi=XL6_IR_C_ABI_I64,.ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL}};
    XL6_IrCallArgument arguments[3]={{.expression=1},{.expression=2},{.expression=1}};
    XL6_IrCallSite site={.origin=9,.variadicAbi=XL6_IR_CALL_VARIADIC_C,.firstArgument=0,.argumentCount=3,.fixedParameterCount=1};
    XL6_IrModule module={.nodes=nodes,.nodeCount=2,.nodeCapacity=2,.types={.items=types,.count=2,.capacity=2},.callSites=&site,.callSiteCount=1,.callSiteCapacity=1,.callArguments=arguments,.callArgumentCount=3,.callArgumentCapacity=3};
    CHECK(!xl6IrCVarargCallSiteHasText(NULL,9));CHECK(xl6IrCVarargCallSiteHasText(&module,9));CHECK(!xl6IrCVarargCallSiteHasText(&module,0));CHECK(!xl6IrCVarargCallSiteHasText(&module,8));
    arguments[2].expression=2;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));arguments[2].expression=1;
    module.callSites=NULL;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));module.callSites=&site;
    module.callSiteCapacity=0;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));module.callSiteCapacity=1;
    module.callSiteCount=0;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));module.callSiteCount=1;
    module.callArguments=NULL;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));module.callArguments=arguments;
    module.callArgumentCapacity=2;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));module.callArgumentCapacity=3;
    site.firstArgument=4;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.firstArgument=SIZE_MAX;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.firstArgument=0;
    site.argumentCount=4;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.argumentCount=SIZE_MAX;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.argumentCount=3;
    site.fixedParameterCount=4;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.fixedParameterCount=1;
    site.firstArgument=1;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.firstArgument=0;
    arguments[2].expression=UINT32_MAX;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));arguments[2].expression=1;
    site.variadicAbi=XL6_IR_CALL_VARIADIC_NONE;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.variadicAbi=XL6_IR_CALL_VARIADIC_C;
    site.fixedParameterCount=3;CHECK(!xl6IrCVarargCallSiteHasText(&module,9));site.fixedParameterCount=1;CHECK(xl6IrCVarargCallSiteHasText(&module,9));
    printf("analysis_ir_vararg_bounds_test passed: %u checks; exact variadic suffix,arena bounds,NULL and overflow refusal\n",checks);return 0;
}
