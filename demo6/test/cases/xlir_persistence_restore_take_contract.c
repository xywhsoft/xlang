#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static XL6_IrInstruction* definition(XL6_IrModule* ir,XL6_IrValueId value)
{
    for (size_t i=0;i<ir->instructionCount;++i)
        if (ir->instructions[i].result==value) return &ir->instructions[i];
    return NULL;
}
static void rejected(const XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir,&diags));
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,ir,&diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,ir,"BAD_RESTORE_TAKE",&diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(!text || !*text); xrtFree(text);
    XL6_ArtifactRuntimePlan plan={0}; assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&diags));
    assert(!plan.sourceCount && !plan.nativeModuleCount);
    xl6DiagFree(&diags); ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==3);
    XL6_ParseResult parse; XL6_IrModule ir;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argv[1],&parse)); assert(xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"restore_take"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags) && xl6IrVerifyModule(&ir,&parse.diags));
    XL6_IrInstruction *first=NULL,*second=NULL;
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* step=&ir.instructions[i];
        if (step->runtimeOp!=XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE && step->runtimeOp!=XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE) continue;
        assert(xl6IrRuntimeOperandOwnership(step->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        assert(xl6IrRuntimeOperandOwnership(step->runtimeOp,1)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
        XL6_IrInstruction* owner=definition(&ir,ir.operands[step->firstOperand+1]);
        assert(owner && owner->kind==XL6_IR_INSTR_RUNTIME_CALL && owner->runtimeOp==XL6_IR_RUNTIME_CLASS_DEFAULT_INVOKE);
        if (!first) first=step; else if (step->payloadType==first->payloadType && !second) second=step;
    }
    assert(first && second);
    XL6_IrValueId saved=ir.operands[second->firstOperand+1];
    ir.operands[second->firstOperand+1]=ir.operands[first->firstOperand+1]; rejected(&ir);
    ir.operands[second->firstOperand+1]=saved;
    saved=ir.operands[first->firstOperand+1];
    ir.operands[first->firstOperand+1]=ir.operands[first->firstOperand]; rejected(&ir);
    ir.operands[first->firstOperand+1]=saved;
    XL6_IrInstruction* owner=definition(&ir,saved); XL6_IrRuntimeOp operation=owner->runtimeOp;
    owner->runtimeOp=XL6_IR_RUNTIME_XVALUE_CLONE; rejected(&ir); owner->runtimeOp=operation;
    XL6_IrTypeId payload=owner->payloadType; owner->payloadType=XL6_IR_TYPE_INVALID; rejected(&ir); owner->payloadType=payload;
    assert(ir.persistenceRestoreRootCount && !ir.persistenceRestoreRoots[0].pending);
    ir.persistenceRestoreRoots[0].pending=XL6_IR_RESTORE_PENDING_FACTORY; rejected(&ir); ir.persistenceRestoreRoots[0].pending=0;
    assert(xl6IrVerifyModule(&ir,&parse.diags));
    XL6_ArtifactRuntimePlan plan={0}; assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_RESTORE_CLASS));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_FUTURE_ADMISSION));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_IMPORT_CHAIN));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    /* Resident DynamicFields no longer makes an otherwise closed root
     * opaque. Open and Set roots retain their actual unsupported recipes;
     * stripping either barrier must fail independent reconstruction. */
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile(argv[2],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags));
    assert(xl6SemCheckModule(parse.root,&parse.diags,"restore_mixed"));
    assert(xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
    assert(xl6IrLowerModule(&ir,&parse.diags) && xl6IrVerifyModule(&ir,&parse.diags));
    assert(ir.persistenceRestoreRootCount);
    bool opened=false, set=false, closed=false;
    for (size_t i=0;i<ir.persistenceRestoreRootCount;++i) {
        uint32 pending=ir.persistenceRestoreRoots[i].pending;
        assert(!(pending & XL6_IR_RESTORE_PENDING_OPAQUE_MODULE_OWNER));
        closed = closed || !pending;
        if (!(pending & (XL6_IR_RESTORE_PENDING_OPEN|XL6_IR_RESTORE_PENDING_SET))) continue;
        opened = opened || (pending & XL6_IR_RESTORE_PENDING_OPEN);
        set = set || (pending & XL6_IR_RESTORE_PENDING_SET);
        ir.persistenceRestoreRoots[i].pending=0;
        rejected(&ir); ir.persistenceRestoreRoots[i].pending=pending;
    }
    assert(opened && set && closed);
    assert(xl6IrVerifyModule(&ir,&parse.diags));
    memset(&plan,0,sizeof(plan));
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_RESTORE_CLASS));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("restore take: %zu corrupt ownership/plan mutations refused before output\n",mutations); return 0;
}
