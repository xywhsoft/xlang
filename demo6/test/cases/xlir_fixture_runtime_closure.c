/* Native-fixture root closure from the real current component registry.
 * This is NOT a public language artifact plan or a minimal pruning claim. */
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool selected[XL6_ARTIFACT_RUNTIME_COUNT];
static void add(unsigned id)
{
    assert(id<XL6_ARTIFACT_RUNTIME_COUNT);
    if(selected[id])return;
    selected[id]=true;
    const XL6_ArtifactRuntimeComponent* component=xl6ArtifactRuntimeComponentGet(id);assert(component);
    for(size_t i=0;i<component->dependencyCount;++i)add((unsigned)component->dependencies[i]);
}
static void quoted(const char* text)
{putchar('"');for(;*text;++text){if(*text=='"'||*text=='\\')putchar('\\');putchar(*text);}putchar('"');}
int main(int argc,char** argv)
{
    assert(argc>1);
    for(int i=1;i<argc;++i)add(xl6ArtifactRuntimeSymbolOwner(argv[i]));
    fputs("{\"Scope\":\"native-fixture registry closure, not language artifact plan\",\"Sources\":[",stdout);
    bool first=true;
    for(unsigned id=0;id<XL6_ARTIFACT_RUNTIME_COUNT;++id)if(selected[id]){
        if(!first)putchar(',');
        first=false;quoted(xl6ArtifactRuntimeComponentGet(id)->source);
    }
    fputs("],\"NativeModules\":[",stdout);first=true;
    const char* modules[XL6_ARTIFACT_NATIVE_MODULE_LIMIT];size_t count=0;
    for(unsigned id=0;id<XL6_ARTIFACT_RUNTIME_COUNT;++id)if(selected[id]){
        const XL6_ArtifactRuntimeComponent* component=xl6ArtifactRuntimeComponentGet(id);
        for(size_t i=0;i<sizeof(component->nativeModules)/sizeof(component->nativeModules[0]);++i){
            const char* name=component->nativeModules[i];if(!name)continue;
            size_t j=0;for(;j<count;++j)if(!strcmp(name,modules[j]))break;
            if(j!=count)continue;
            assert(count<XL6_ARTIFACT_NATIVE_MODULE_LIMIT);modules[count++]=name;
            if(!first)putchar(',');
            first=false;quoted(name);
        }
    }
    puts("]}");return 0;
}
