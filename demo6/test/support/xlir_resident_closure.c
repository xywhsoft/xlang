/* Test/build observer of the SAME registry used for standalone artifacts.
 * Roots are explicit audited exported symbols or directly included units.
 * Unknown roots refuse before emitting a partial dependency list. */
#include "src/compile/xl6_artifact_runtime.h"
#include <stdio.h>
#include <string.h>
static void quoted(const char* text)
{
    putchar('"');for(;*text;++text){if(*text=='"'||*text=='\\')putchar('\\');putchar(*text);}putchar('"');
}
int main(int argc,char** argv)
{
    XL6_ArtifactRuntimeSet roots={0};XL6_ArtifactRuntimePlan plan;
    if(argc<3 || !(argc%2))return 2;
    for(int argument=1;argument<argc;argument+=2){
        unsigned id=XL6_ARTIFACT_RUNTIME_COUNT;
        if(!strcmp(argv[argument],"--symbol"))id=xl6ArtifactRuntimeSymbolOwner(argv[argument+1]);
        else if(!strcmp(argv[argument],"--source")){
            for(unsigned candidate=0;candidate<XL6_ARTIFACT_RUNTIME_COUNT;++candidate){
                const XL6_ArtifactRuntimeComponent* component=xl6ArtifactRuntimeComponentGet(candidate);
                if(!strcmp(component->source,argv[argument+1])){id=candidate;break;}
            }
        }
        if(id==XL6_ARTIFACT_RUNTIME_COUNT || !xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,id)){
            fprintf(stderr,"unknown resident root: %s %s\n",argv[argument],argv[argument+1]);return 3;
        }
    }
    if(!xl6ArtifactRuntimeClosePlan(&roots,&plan))return 4;
    fputs("{\"Sources\":[",stdout);
    for(size_t i=0;i<plan.sourceCount;++i){if(i)putchar(',');quoted(plan.sources[i]);}
    fputs("],\"NativeModules\":[",stdout);
    for(size_t i=0;i<plan.nativeModuleCount;++i){if(i)putchar(',');quoted(plan.nativeModules[i]);}
    puts("]}");return 0;
}
