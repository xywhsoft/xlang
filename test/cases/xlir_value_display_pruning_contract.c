#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Each independent fixture supplies its expected List/Set/Dict provider mask.
 * Check the real artifact closure, not merely emitted text or IR metadata. */
int main(int argc, char** argv)
{
    const XL6_ArtifactRuntimeId providers[] = {
        XL6_ARTIFACT_LIST_GRAPH, XL6_ARTIFACT_SET_GRAPH, XL6_ARTIFACT_DICT_GRAPH
    };
    assert(argc >= 3 && (argc % 2) == 1);
    /* The independently exported erased argument-pack formatter is open.
     * Its source calls all three providers, so its artifact must own them. */
    {
        XL6_ArtifactRuntimePlan plan;
        XL6_ArtifactRuntimeSet roots = xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_PRINT_VALUES);
        assert(xl6ArtifactRuntimeClosePlan(&roots, &plan));
        for (unsigned i = 0; i < 3; ++i)
            assert(xl6ArtifactRuntimeSetHas(&plan.components, providers[i]));
        puts("display erased-pack closure: all three native providers present");
    }
    for (int argument = 1; argument < argc; argument += 2) {
        XL6_CompileUnit unit;
        XL6_ArtifactRuntimePlan plan, repeat;
        unsigned expected = (unsigned)strtoul(argv[argument], NULL, 10);
        assert(expected <= 7);
        xl6CompileUnitInit(&unit);
        if (!xl6CompileUnitLoadFile(&unit, argv[argument + 1], 1) ||
            !xl6IrLowerModule(&unit.ir, &unit.parse.diags) ||
            !xl6ArtifactRuntimePlanBuild(&unit.ir, XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,
                &plan, &unit.parse.diags)) {
            xl6DiagPrintText(&unit.parse.diags, argv[argument + 1]);
            xl6CompileUnitFree(&unit);
            return 1;
        }
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir, XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,
            &repeat, NULL) && !memcmp(&plan, &repeat, sizeof(plan)));
        for (unsigned i = 0; i < 3; ++i) {
            const char* source = xl6ArtifactRuntimeComponentGet(providers[i])->source;
            size_t occurrences = 0;
            for (size_t j = 0; j < plan.sourceCount; ++j)
                occurrences += !strcmp(source, plan.sources[j]);
            assert(xl6ArtifactRuntimeSetHas(&plan.components, providers[i]) ==
                ((expected & (1u << i)) != 0));
            assert(occurrences == ((expected & (1u << i)) != 0));
        }
        printf("display artifact pruning: mask %u, %zu sources: %s\n",
            expected, plan.sourceCount, argv[argument + 1]);
        xl6CompileUnitFree(&unit);
    }
    return 0;
}
