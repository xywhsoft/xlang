#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_xrt_trim.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    assert(argc==2 || argc==3);
    if(argc==3){
        XL6_CompileUnit unit;XL6_ArtifactRuntimePlan selected={0};xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[2],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&selected,&unit.parse.diags));
        assert(!xl6ArtifactRuntimeSetHas(&selected.components,XL6_ARTIFACT_NULLABLE_SLOT_MAP_KEY));
        for(size_t i=0;i<selected.sourceCount;++i)
            assert(strcmp(selected.sources[i],"src/runtime/xl6_nullable_slot_map_key.c"));
        xl6CompileUnitFree(&unit);puts("unused nullable native supplier pruned");return 0;
    }
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_NULLABLE_SLOT_MAP_KEY);
    XL6_ArtifactRuntimePlan plan={0},repeat={0};
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan) && xl6ArtifactRuntimeClosePlan(&roots,&repeat));
    assert(!memcmp(&plan,&repeat,sizeof(plan)));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STACK));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_NATIVE));
    char* text=NULL;assert(xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,0,plan.nativeModules,plan.nativeModuleCount,&text));
    assert(!strstr(text,"XRT_MODULE_ALL") && !strstr(text,"XRUNTIME_MODULE_ALL"));
    XL6_Output out;assert(xl6OutputOpenFile(&out,argv[1]));assert(xl6OutputPuts(text,&out)!=EOF && !xl6OutputHasFailed(&out));
    xl6OutputClose(&out);xrtFree(text);
    printf("{\"Sources\":[");for(size_t i=0;i<plan.sourceCount;++i)printf("%s\"%s\"",i?",":"",plan.sources[i]);puts("]}");
    return 0;
}
