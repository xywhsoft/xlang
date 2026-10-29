/* Read-only observer of the genuine SO, never compiler source objects. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
static void quoted(const char* text)
{
    putchar('"');for(;*text;++text){if(*text=='"'||*text=='\\')putchar('\\');putchar(*text);}putchar('"');
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    fputs("{\"Sources\":[",stdout);
    for(size_t i=0;i<plan.sourceCount;++i){if(i)putchar(',');quoted(plan.sources[i]);}
    fputs("],\"NativeModules\":[",stdout);
    for(size_t i=0;i<plan.nativeModuleCount;++i){if(i)putchar(',');quoted(plan.nativeModules[i]);}
    puts("]}");xl6CompileUnitFree(&unit);return 0;
}
