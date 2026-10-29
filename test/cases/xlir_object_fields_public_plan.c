/* Source-derived pruning: never infer the native plan from generated C text. */
#include "src/compile/xl6_artifact_runtime.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==3); bool required=!strcmp(argv[2],"required");
    assert(required || !strcmp(argv[2],"absent"));
    XL6_ParseResult parse; XL6_IrModule ir; XL6_ArtifactRuntimePlan plan;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    int ok=xl6ParseFile(argv[1],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
        xl6SemCheckModule(parse.root,&parse.diags,"object_fields_public") &&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) && xl6IrLowerModule(&ir,&parse.diags) &&
        xl6IrVerifyModule(&ir,&parse.diags) &&
        xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&parse.diags);
    if(!ok) {
        for(size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 1;
    }
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_OBJECT_FIELDS)==required);
    unsigned providers=0;
    for(size_t i=0;i<plan.sourceCount;++i)
        providers+=!strcmp(plan.sources[i],"src/runtime/xl6_value_object_fields.c");
    assert(providers==(required ? 1u : 0u));
    if(required) assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CODE_FINALIZER) &&
        xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DYNAMIC_FIELDS));
    printf("Object-field public plan: provider=%u expected=%u\n",providers,required ? 1u : 0u);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
