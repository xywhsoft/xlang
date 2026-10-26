/* Test-link planning from the same verified/lowered IR and registry as normal
 * artifacts. A host's own resident roots remain explicit; generated C is never
 * scanned for names and never treated as the semantic source of truth. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <stdio.h>
#include <string.h>

static void quoted(const char* text)
{
    putchar('"');
    for (; *text; ++text) {
        if (*text == '"' || *text == '\\') putchar('\\');
        putchar(*text);
    }
    putchar('"');
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    for (int argument = 1; argument < argc; ++argument) {
        XL6_CompileUnit unit;
        XL6_ArtifactRuntimePlan plan, repeat;
        xl6CompileUnitInit(&unit);
        int ok = xl6CompileUnitLoadFile(&unit, argv[argument], 1) &&
            xl6IrLowerModule(&unit.ir, &unit.parse.diags) &&
            xl6ArtifactRuntimePlanBuild(&unit.ir, XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,
                &plan, &unit.parse.diags) &&
            xl6ArtifactRuntimePlanBuild(&unit.ir, XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,
                &repeat, &unit.parse.diags);
        if (ok) {
            ok = plan.sourceCount == repeat.sourceCount &&
                plan.nativeModuleCount == repeat.nativeModuleCount &&
                !memcmp(&plan.roots, &repeat.roots, sizeof(plan.roots)) &&
                !memcmp(&plan.components, &repeat.components, sizeof(plan.components));
            for (size_t i = 0; ok && i < plan.sourceCount; ++i)
                ok = !strcmp(plan.sources[i], repeat.sources[i]);
            for (size_t i = 0; ok && i < plan.nativeModuleCount; ++i)
                ok = !strcmp(plan.nativeModules[i], repeat.nativeModules[i]);
        }
        if (!ok) {
            xl6DiagPrintText(&unit.parse.diags, argv[argument]);
            xl6CompileUnitFree(&unit);
            return 1;
        }
        fputs("{\"Fixture\":", stdout); quoted(argv[argument]);
        fputs(",\"Sources\":[", stdout);
        for (size_t i = 0; i < plan.sourceCount; ++i) {
            if (i) putchar(',');
            quoted(plan.sources[i]);
        }
        fputs("],\"NativeModules\":[", stdout);
        for (size_t i = 0; i < plan.nativeModuleCount; ++i) {
            if (i) putchar(',');
            quoted(plan.nativeModules[i]);
        }
        puts("],\"Deterministic\":true}");
        xl6CompileUnitFree(&unit);
    }
    return 0;
}
