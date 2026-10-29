#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_xrt_trim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void writeText(const char* prefix,const char* suffix,const char* text)
{
    char path[4096];int length=snprintf(path,sizeof(path),"%s%s",prefix,suffix);
    assert(length>0 && (size_t)length<sizeof(path));FILE* file=fopen(path,"wb");assert(file);
    assert(fwrite(text,1,strlen(text),file)==strlen(text) && !fclose(file));
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_ArtifactRuntimePlan plan={0};XL6_ImportDeps imports={0};
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_STATIC_MODULE);
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_STACK));
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_CODE_FINALIZER));
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    XL6_ArtifactOwnershipPlan ownership={0};assert(xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
    plan=ownership.runtime;
    char declarations[8192]={0};size_t used=0;
    for(size_t i=0;i<ownership.domainCount;++i){
        int bytes=snprintf(declarations+used,sizeof(declarations)-used,"%s(),\n",ownership.domains[i]);
        assert(bytes>0 && (size_t)bytes<sizeof(declarations)-used);used+=(size_t)bytes;
    }
    writeText(argv[1],".domains.h",declarations);
    assert(!xl6XrtTrimRequiresFull(&imports,0));char* implementation=NULL;
    assert(xl6XrtTrimBuildImplementationSourceFromArtifact(&imports,0,plan.nativeModules,plan.nativeModuleCount,&implementation));
    assert(implementation);writeText(argv[1],".xrt.c",implementation);xrtFree(implementation);
    char path[4096];assert(snprintf(path,sizeof(path),"%s.plan.json",argv[1])>0);FILE* file=fopen(path,"wb");assert(file);
    fputs("{\"Sources\":[",file);
    for(size_t i=0;i<plan.sourceCount;++i)fprintf(file,"%s\"%s\"",i ? "," : "",plan.sources[i]);
    fputs("],\"NativeModules\":[",file);
    for(size_t i=0;i<plan.nativeModuleCount;++i)fprintf(file,"%s\"%s\"",i ? "," : "",plan.nativeModules[i]);
    fputs("],\"Domains\":[",file);
    for(size_t i=0;i<ownership.domainCount;++i)fprintf(file,"%s\"%s\"",i ? "," : "",ownership.domains[i]);
    fputs("]}\n",file);assert(!fclose(file));return 0;
}
