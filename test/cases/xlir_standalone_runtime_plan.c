/* Observe the genuine executable artifact plan derived from verified XLIR.
 * The downstream standalone linker receives only these provider source rows,
 * never the compiler objects used to build this observer. */
#include "src/compile/xl6_artifact_runtime.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    XL6_ParseResult parse;
    XL6_IrModule ir;
    XL6_ArtifactRuntimePlan plan;
    xl6ParseResultInit(&parse);
    xl6IrModuleInit(&ir);
    int ok = xl6ParseFile(argv[1], &parse) &&
        xl6StdlibExpandModule(parse.root, &parse.diags) &&
        xl6SemCheckModule(parse.root, &parse.diags, "standalone_plan") &&
        xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags) &&
        xl6IrLowerModule(&ir, &parse.diags) &&
        xl6IrVerifyModule(&ir, &parse.diags) &&
        xl6ArtifactRuntimePlanBuild(&ir, XL6_ARTIFACT_ENTRY_EXECUTABLE_V1, &plan, &parse.diags);
    if (ok) {
        for (size_t i = 0; i < plan.sourceCount; ++i)
            printf("SOURCE:%s\n", plan.sources[i]);
        for (size_t i = 0; i < plan.nativeModuleCount; ++i)
            printf("NATIVE:%s\n", plan.nativeModules[i]);
    } else {
        for (size_t i = 0; i < parse.diags.count; ++i)
            fprintf(stderr, "%s\n", parse.diags.items[i].message);
    }
    xl6IrModuleFree(&ir);
    xl6ParseResultFree(&parse);
    return ok ? 0 : 1;
}
