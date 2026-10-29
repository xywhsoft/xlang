/* Pure classification-unit coverage, not a substitute for full verifier or
 * generated-language deployment tests. No physical runtime graph is forged. */
#include "src/ir/xl6_ir.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    XL6_IrModule module={0};XL6_IrFunction function={0};XL6_IrCallTarget target={0};XL6_IrInstruction instruction={0};
    module.stage=XL6_IR_STAGE_LOWERED;module.functions=&function;module.functionCount=1;
    module.callTargets=&target;module.callTargetCount=1;module.instructions=&instruction;
    function.symbol=1;function.kind=XL6_IR_FUNCTION_ASYNC_WORKER;target.symbol=1;
    target.asyncDispatch=XL6_IR_ASYNC_DISPATCH_FIXED_READY_QUEUE;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_NONE);
    function.reachability=XL6_IR_REACHABILITY_ROOT;target.reachability=XL6_IR_REACHABILITY_ROOT;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    function.kind=XL6_IR_FUNCTION_USER;function.instructionCount=1;module.instructionCount=1;
    instruction.kind=XL6_IR_INSTR_RUNTIME_CALL;instruction.runtimeOp=XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE;
    function.reachability=XL6_IR_REACHABILITY_UNREACHABLE;target.reachability=XL6_IR_REACHABILITY_UNREACHABLE;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_NONE);
    function.reachability=XL6_IR_REACHABILITY_TRANSITIVE;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    function.reachability=XL6_IR_REACHABILITY_UNREACHABLE;module.stage=XL6_IR_STAGE_TYPED;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    module.stage=XL6_IR_STAGE_LOWERED;instruction.kind=XL6_IR_INSTR_AWAIT;function.kind=XL6_IR_FUNCTION_ASYNC_WORKER;
    function.reachability=XL6_IR_REACHABILITY_UNREACHABLE;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_NONE);
    function.reachability=XL6_IR_REACHABILITY_ROOT;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    module.functionCount=0;target.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;
    target.reachability=XL6_IR_REACHABILITY_UNREACHABLE;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_NONE);
    target.reachability=XL6_IR_REACHABILITY_ROOT;
    assert(xl6IrModuleContinuationRequirement(&module)==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    puts("continuation requirements: retained workers/services/closures only; preclosure conservative");return 0;
}
