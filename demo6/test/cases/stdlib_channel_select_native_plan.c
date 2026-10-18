#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
static void quoted(const char* text)
{ putchar('"'); for(;*text;++text) { if(*text=='"' || *text=='\\') putchar('\\'); putchar(*text); } putchar('"'); }
int main(int argc,char** argv)
{
    assert(argc==2); XL6_ArtifactRuntimeId owner=xl6ArtifactRuntimeSymbolOwner(argv[1]);
    assert(owner!=XL6_ARTIFACT_RUNTIME_COUNT);
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(owner);
    /* The native fixture creates an exact scalar schema. This is an explicit
     * test dependency, not an invented public source-language select root. */
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,
        xl6ArtifactRuntimeSymbolOwner("xl6SequencePrimitiveCreateV1")));
    XL6_ArtifactRuntimePlan plan; assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    fputs("{\"Sources\":[",stdout);
    for(size_t i=0;i<plan.sourceCount;++i) { if(i)putchar(','); quoted(plan.sources[i]); }
    fputs("],\"NativeModules\":[",stdout);
    for(size_t i=0;i<plan.nativeModuleCount;++i) { if(i)putchar(','); quoted(plan.nativeModules[i]); }
    puts("]}"); return 0;
}
