#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_xrt_trim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void writeText(const char* prefix,const char* suffix,const char* text)
{
    char path[4096]; int count=snprintf(path,sizeof(path),"%s%s",prefix,suffix);
    assert(count>0 && (size_t)count<sizeof(path)); FILE* file=fopen(path,"wb"); assert(file);
    assert(fwrite(text,1,strlen(text),file)==strlen(text)); assert(!fclose(file));
}
int main(int argc,char** argv)
{
    assert(argc==3); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags) && xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    char* implementation=NULL;
    assert(xl6XrtTrimBuildImplementationSourceFromArtifact(&unit.importDeps,0,
        plan.nativeModules,plan.nativeModuleCount,&implementation) && implementation);
    writeText(argv[2],".xrt.c",implementation); xrtFree(implementation);
    char path[4096]; int length=snprintf(path,sizeof(path),"%s.plan.json",argv[2]);
    assert(length>0 && (size_t)length<sizeof(path)); FILE* file=fopen(path,"wb"); assert(file);
    fprintf(file,"{\"Sources\":[");
    for(size_t i=0;i<plan.sourceCount;++i)fprintf(file,"%s\"%s\"",i ? "," : "",plan.sources[i]);
    fprintf(file,"],\"NativeModules\":[");
    for(size_t i=0;i<plan.nativeModuleCount;++i)fprintf(file,"%s\"%s\"",i ? "," : "",plan.nativeModules[i]);
    fprintf(file,"]}\n"); assert(!fclose(file)); xl6CompileUnitFree(&unit);
    puts("actual selected native plan and XRT implementation emitted"); return 0;
}
