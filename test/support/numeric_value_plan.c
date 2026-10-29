#define main previous_executor_plan
#include "executor_catalog_plan.c"
#undef main
static unsigned selected(const char* name)
{if(!strcmp(name,"numeric"))return XL6_ARTIFACT_VALUE_NUMERIC;
 if(!strcmp(name,"bind"))return XL6_ARTIFACT_VALUE_ADMISSION;
 assert(!strcmp(name,"add"));return XL6_ARTIFACT_VALUE_ADD;}
static XL6_ArtifactRuntimePlan plan(const char* name,bool fixture)
{
    XL6_ArtifactRuntimeSet roots={0};XL6_ArtifactRuntimePlan result;
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,selected(name)));
    if(fixture){assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_VALUE_NUMERIC));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_VALUE_ADMISSION));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NATIVE_SCALAR_VALUE));}
    assert(xl6ArtifactRuntimeClosePlan(&roots,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){bool fixture=strncmp(argv[1],"--api-",6)!=0;
        XL6_ArtifactRuntimePlan p=plan(argv[2],fixture);const char* action=argv[1]+(fixture?2:6);
        if(!strcmp(action,"sources")){for(size_t i=0;i<p.sourceCount;++i)puts(p.sources[i]);return 0;}
        assert(!strcmp(action,"modules"));for(size_t i=0;i<p.nativeModuleCount;++i)printf("#define %s\n",p.nativeModules[i]);return 0;}
    assert(argc==1);XL6_ArtifactRuntimePlan p=plan("numeric",false);
    assert(!has_module(&p,"XRUNTIME_MODULE_TYPED_ARRAY_VALUE")&&!has_module(&p,"XRT_MODULE_NET_SOCKET"));
    assert(!xl6ArtifactRuntimeSetHas(&p.components,XL6_ARTIFACT_ARRAY_ALGEBRA)&&
        !xl6ArtifactRuntimeSetHas(&p.components,XL6_ARTIFACT_VALUE_ADD));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ValueNumericBinaryV1")==XL6_ARTIFACT_VALUE_NUMERIC);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ValueAddV1")==XL6_ARTIFACT_VALUE_ADD);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ValueEnumBindTakeV1")==XL6_ARTIFACT_VALUE_ADMISSION);
    puts("Numeric value plan: separate numeric/bind/add roots and no numeric Array/network dependency");return 0;
}
