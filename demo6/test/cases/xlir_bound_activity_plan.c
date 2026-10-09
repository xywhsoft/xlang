#include "src/cgen/xl6_ir_cgen.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Symbols { unsigned erased, v1, v2, memory, atomic, thread; } Symbols;
static int visit(void* context, const char* name)
{
    Symbols* found = context;
    found->erased += !strcmp(name, "xl6ClosureBoxErasedV1");
    found->v1 += !strcmp(name, "xl6ClosureRuntimeV1Get");
    found->v2 += !strcmp(name, "xl6ClosureRuntimeV2Get");
    found->memory += !strcmp(name, "XRT_MODULE_MEMORY");
    found->atomic += !strcmp(name, "XRT_MODULE_ATOMIC");
    found->thread += !strcmp(name, "XRT_MODULE_THREAD");
    return 1;
}
static void check(const char* file, const char* source, bool binding)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_ArtifactRuntimePlan plan;
    Symbols found = {0}; xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(source ? xl6ParseSource(source, file, &parse) : xl6ParseFile(file, &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags) &&
        xl6SemCheckModule(parse.root, &parse.diags, "bound_plan"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) &&
        xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    /* This erased member path has no ordinary closure value/target that could
     * accidentally force-link the missing adapter provider. */
    assert(!ir.closureTargetCount);
    assert(xl6IrCgenVisitResidentSymbols(&ir, &parse.diags, NULL, visit, &found));
    assert((found.erased != 0) == binding);
    if (binding) assert(found.erased == 1 && found.v1 == 1 && found.v2 == 1);
    assert(xl6IrCgenVisitNativeModules(&ir, &parse.diags, NULL, visit, &found));
    if (binding) assert(found.memory && found.atomic && found.thread);
    assert(xl6ArtifactRuntimePlanBuild(&ir, XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1, &plan, &parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components, XL6_ARTIFACT_CLOSURE) == binding);
    assert(!xl6ArtifactRuntimeSetHas(&plan.components, XL6_ARTIFACT_LOG_CORE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components, XL6_ARTIFACT_NETWORK_SERVICE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components, XL6_ARTIFACT_THREAD));
    if (binding) assert(xl6ArtifactRuntimeSymbolOwner("xl6ClosureBoxErasedV1") == XL6_ARTIFACT_CLOSURE);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
}
int main(int argc, char** argv)
{
    assert(argc == 2); check(argv[1], NULL, true);
    check("bound_plan_direct.xl",
        "class DirectReceiver\n func read() as int\n return 42\n end\nend\n"
        "var receiver = DirectReceiver()\nassert int(receiver.read()) == 42\n", false);
    puts("bound plan: erased-only provider selected, direct dynamic call pruned, closed artifact dependencies; metadata proof, not packaged AOT execution");
    return 0;
}
