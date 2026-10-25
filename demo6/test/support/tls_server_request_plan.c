#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* symbols[]={"xl6TlsServerRequestCopyV1","xl6TlsServerRequestDestroyV1",
        "xl6TlsServerRequestNameV1","xl6TlsServerRequestProtocolCountV1","xl6TlsServerRequestProtocolAtV1"};
    for(size_t i=0;i<sizeof(symbols)/sizeof(*symbols);++i)
        assert(xl6ArtifactRuntimeSymbolOwner(symbols[i])==XL6_ARTIFACT_TLS_SERVER_REQUEST);
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_TLS_SERVER_REQUEST);
    XL6_ArtifactRuntimePlan plan;XL6_ArtifactOwnershipPlan owned;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
    assert(plan.sourceCount==1&&!strcmp(plan.sources[0],"src/runtime/xl6_tls_server_request.c"));
    assert(plan.nativeModuleCount==2);
    bool hello=false,value=false;
    for(size_t i=0;i<plan.nativeModuleCount;++i){
        hello|=!strcmp(plan.nativeModules[i],"XRT_MODULE_TLS_HELLO");
        value|=!strcmp(plan.nativeModules[i],"XRT_MODULE_VALUE");
    }
    assert(hello&&value&&xl6ArtifactOwnershipPlanBuild(&plan,&owned)&&owned.domainCount==0);
    XL6_ArtifactRuntimeSet empty={0};
    assert(xl6ArtifactRuntimeClosePlan(&empty,&plan)&&!plan.sourceCount&&!plan.nativeModuleCount);
    puts("TLS server request plan: one resident source, Hello/Value only; no session, transport or callback owners; empty pruned");
    return 0;
}
