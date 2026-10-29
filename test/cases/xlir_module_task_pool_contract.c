#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static size_t mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags = {0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out, ir, "BAD_POOL", &diags));
    text = xl6OutputTakeMemoryText(&out, NULL); assert(!text || !*text); xrtFree(text);
    xl6DiagFree(&diags); ++mutations;
}
static void check(const char* text, const char* path, int expected, int emit, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; size_t i;
    XL6_IrCgenOptions options = {1, 0};
    XL6_IrModuleTaskPoolAbi saved;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(text ? xl6ParseSource(text, path, &parse) : xl6ParseFile(path, &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    if (!xl6SemCheckModule(parse.root, &parse.diags, "module_pool") ||
        !xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags)) goto failed;
    assert(ir.abi.taskPoolAbi == XL6_IR_MODULE_TASK_POOL_NONE);
    ir.abi.taskPoolAbi = XL6_IR_MODULE_TASK_POOL_XRT_GUARDED_QUIESCE; reject(&ir);
    ir.abi.taskPoolAbi = XL6_IR_MODULE_TASK_POOL_NONE;
    if (!xl6IrLowerModule(&ir, &parse.diags) || !xl6IrVerifyModule(&ir, &parse.diags)) goto failed;
    saved = ir.abi.taskPoolAbi;
    assert((int)saved == expected && ir.abi.finalizerFunction);
    ir.abi.taskPoolAbi = (XL6_IrModuleTaskPoolAbi)99; reject(&ir);
    ir.abi.taskPoolAbi = expected ? XL6_IR_MODULE_TASK_POOL_NONE : XL6_IR_MODULE_TASK_POOL_XRT_GUARDED_QUIESCE; reject(&ir);
    ir.abi.taskPoolAbi = saved;
    {
        XL6_IrFunction* fini = &ir.functions[ir.abi.finalizerFunction - 1];
        XL6_IrErrorAbi error = fini->errorAbi;
        assert((error == XL6_IR_ERROR_ABI_MAY_RAISE) == xl6IrModuleHasFinalizerWork(&ir));
        fini->errorAbi = error == XL6_IR_ERROR_ABI_MAY_RAISE ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE;
        reject(&ir); fini->errorAbi = error;
    }
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    if (emit) {
        size_t constructionOperations = 0;
        for (i = 0; i < ir.instructionCount; ++i) {
            XL6_IrInstruction *operation = &ir.instructions[i], *test;
            XL6_IrBlockId failure;
            XL6_IrInstructionKind kind;
            if (operation->kind != XL6_IR_INSTR_RUNTIME_CALL ||
                (operation->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLASS_NEW &&
                 operation->runtimeOp != XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW)) continue;
            assert(i + 1 < ir.instructionCount);
            test = &ir.instructions[i + 1];
            assert(test->kind == XL6_IR_INSTR_ERROR_TEST);
            failure = test->falseTarget;
            test->falseTarget = test->trueTarget; reject(&ir); test->falseTarget = failure;
            kind = test->kind; test->kind = XL6_IR_INSTR_TRAP_IF_FALSE;
            reject(&ir); test->kind = kind;
            ++constructionOperations;
        }
        assert(constructionOperations >= 3);
        assert(xl6IrVerifyModule(&ir, &parse.diags));
        assert(xl6OutputOpenFile(&out, argv[1])); assert(xl6IrCgenEmitCEx(&out, &ir, &parse.diags, &options)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, argv[2])); assert(xl6IrCgenEmitHeader(&out, &ir, "MODULE_POOL", &parse.diags)); xl6OutputClose(&out);
        assert(xl6OutputOpenFile(&out, argv[3])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    } else {
        char* generated;
        assert(xl6OutputOpenMemory(&out));
        if (!xl6IrCgenEmitCEx(&out, &ir, &parse.diags, &options)) goto failed;
        generated = xl6OutputTakeMemoryText(&out, NULL); assert(generated && *generated);
        assert((strstr(generated,"visit(xl6TaskPoolOwnership(xlir_async_pool), context)")!=NULL)==(expected!=0));
        assert(!strstr(generated,"if (!xl6TaskRuntimeV1Get()->PoolIsClosing(xlir_async_pool) ||"));
        if(expected)assert(ir.abi.taskPoolOwnershipAbi==XL6_IR_MODULE_TASK_POOL_OWNERSHIP_RESIDENT_NATIVE_JOBS_JOIN_V1&&
            ir.abi.retirementAbi==XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4&&
            strstr(generated,"xl6TaskPoolModulePrepareV1(xlir_async_pool, teardown, stage != 0)")&&
            strstr(generated,"for (int stage = 0; stage < (wait ? 2 : 1); ++stage)"));
        xrtFree(generated);
    }
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return;
failed:
    for (i = 0; i < parse.diags.count; ++i) fprintf(stderr, "%s:%d:%d: %s\n", path, parse.diags.items[i].line, parse.diags.items[i].column, parse.diags.items[i].message);
    abort();
}
int main(int argc, char** argv)
{
    assert(argc == 4);
    check("int value = 1\n", "pool_none.xl", 0, 0, argv);
    check("async func ready() as int\nreturn 7\nend\nauto result = ready()\n", "pool_ready.xl", 0, 0, argv);
    check("async func waiting(future<int> input) as int\nreturn await input\nend\n", "pool_machine.xl", 0, 0, argv);
    check("extern \"observe\" func observe()\nasync func worker()\nobserve()\nend\nauto result = worker()\n", "pool_worker.xl", 0, 0, argv);
    check("extern \"observe\" func observe()\nasync func deadWorker()\nobserve()\nend\n", "pool_dead.xl", 0, 0, argv);
    check("async func unusedReady() as int\nreturn 7\nend\n", "pool_unused_ready.xl", 0, 0, argv);
    check("extern \"observe\" func observe()\nclass Worker\nvirtual async func run()\nobserve()\nend\nend\n", "pool_unused_virtual_void.xl", 0, 0, argv);
    check("func work() as int\nreturn 7\nend\nfunc() as int job = work\nauto result = task.spawn(job)\n", "pool_spawn.xl", 1, 0, argv);
    check(NULL, "test/cases/xlir_module_task_pool.xl", 1, 1, argv);
    printf("module pool: 9 ABI partitions, %llu rejected IR mutations, canonical resource dump\n", (unsigned long long)mutations);
    return 0;
}
