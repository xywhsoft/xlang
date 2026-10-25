#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool source(const XL6_ArtifactRuntimePlan* p,const char* name)
{for(size_t i=0;i<p->sourceCount;++i)if(!strcmp(p->sources[i],name))return true;return false;}
int main(int argc,char** argv)
{
    assert(argc==5);
    for(int i=1;i<argc;++i){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);XL6_ArtifactRuntimePlan plan;
        assert(xl6CompileUnitLoadFile(&unit,argv[i],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        bool dynamic=i<3,empty=i==4;
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_SELECTOR)==dynamic);
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_CLASS_CALLBACK)==dynamic);
        assert(source(&plan,"src/runtime/xl6_tls_selector.c")==dynamic);
        assert(source(&plan,"src/runtime/xl6_native_class_callback.c")==dynamic);
        assert(source(&plan,"src/runtime/xl6_execution_tls_listener.c")==!empty);
        printf("Listener selector artifact plan %d: dynamic=%d empty=%d sources=%zu\n",i,dynamic,empty,plan.sourceCount);
        xl6CompileUnitFree(&unit);
    }
    puts("Listener selector pruning: 4 real lowered executable plans, exact resident sources and empty/static exclusion: ok");return 0;
}
