#define main executor_catalog_plan_main
#include "executor_catalog_plan.c"
#undef main
static XL6_ArtifactOwnershipPlan plan(const char* choice)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(!strcmp(choice,"udp-resource")?XL6_ARTIFACT_EXECUTION_UDP_DATA:
        !strcmp(choice,"udp-future")?XL6_ARTIFACT_EXECUTION_UDP_FUTURE:
        !strcmp(choice,"tcp-resource")?XL6_ARTIFACT_EXECUTION_TCP:
        !strcmp(choice,"tcp-native")?XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_NATIVE:
        !strcmp(choice,"tcp-base-native")?XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_BASE_NATIVE:
        !strcmp(choice,"server-native")?XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_SERVER_NATIVE:
        !strcmp(choice,"server-future-native")?XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_SERVER_FUTURE_NATIVE:
        !strcmp(choice,"server-all-native")?XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_SERVER_FUTURE_NATIVE:
        !strcmp(choice,"server-resource")?XL6_ARTIFACT_EXECUTION_TCP_SERVER_ACCEPT:
        !strcmp(choice,"server-language")?XL6_ARTIFACT_TCP_SERVER_LANGUAGE:
        !strcmp(choice,"server-options")?XL6_ARTIFACT_TCP_SERVER_OPTIONS:
        !strcmp(choice,"net-bytes")?XL6_ARTIFACT_NET_BYTES:
        !strcmp(choice,"net-bytes-buffer")?XL6_ARTIFACT_NET_BYTES_BUFFER:
        !strcmp(choice,"net-buffer-ref-graph")?XL6_ARTIFACT_NET_BUFFER_REF:
        !strcmp(choice,"tcp-owned")?XL6_ARTIFACT_EXECUTION_TCP_SERVER_ACCEPT:
        !strcmp(choice,"tcp-recv-owned")?XL6_ARTIFACT_EXECUTION_TCP_SERVER_ACCEPT:
        !strcmp(choice,"tcp-ref-batches")?XL6_ARTIFACT_EXECUTION_TCP_SERVER_ACCEPT:
        !strcmp(choice,"tcp-chain-move")?XL6_ARTIFACT_EXECUTION_TCP_SERVER_ACCEPT:
        !strcmp(choice,"buffer-native")?XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE:
        !strcmp(choice,"config")?XL6_ARTIFACT_NET_ENGINE_CONFIG:
        !strcmp(choice,"stats")?XL6_ARTIFACT_NET_ENGINE_STATS:XL6_ARTIFACT_NET_ENGINE_LANGUAGE);
    if(!strcmp(choice,"all")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_ENGINE_CONFIG));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_ENGINE_STATS));
    }
    if(!strcmp(choice,"udp-future"))
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_EXECUTION_UDP_DATA));
    if(!strcmp(choice,"server-all-native"))
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_NATIVE));
    if(!strcmp(choice,"tcp-owned")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_SEND_REF));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_SEND_MOVE));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_BYTES_BUFFER));
    }
    if(!strcmp(choice,"tcp-recv-owned")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_RECV_OWNED_SYNC));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_RECV_OWNED_INTERRUPT));
    }
    if(!strcmp(choice,"tcp-ref-batches")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_SEND_REFS));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_ARRAY_NOMINAL_ADMISSION));
    }
    if(!strcmp(choice,"tcp-chain-move")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_BUFFER_REF));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_BUFFER_VALUE));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_TCP_SEND_CHAIN_MOVE));
    }
    if(!strcmp(choice,"net-buffer-ref-graph")){
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_FRAME_LINE));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_FRAME_LENGTH));
        assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_NET_PROXY_HANDSHAKE));
    }
    XL6_ArtifactRuntimePlan base;XL6_ArtifactOwnershipPlan result;
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base)&&xl6ArtifactOwnershipPlanBuild(&base,&result));return result;
}
int main(int argc,char** argv)
{
    if(argc==3){
        XL6_ArtifactOwnershipPlan selected=plan(argv[2]);
        if(!strcmp(argv[1],"--sources")){for(size_t i=0;i<selected.runtime.sourceCount;++i)puts(selected.runtime.sources[i]);return 0;}
        if(!strcmp(argv[1],"--modules")){for(size_t i=0;i<selected.runtime.nativeModuleCount;++i)printf("#define %s\n",selected.runtime.nativeModules[i]);return 0;}
        if(!strcmp(argv[1],"--domains")){for(size_t i=0;i<selected.domainCount;++i)printf("%s(),\n",selected.domains[i]);return 0;}
        return 2;
    }
    assert(argc==1);
    XL6_ArtifactOwnershipPlan udp=plan("udp-resource"),udpFuture=plan("udp-future");
    assert(has_module(&udp.runtime,"XRT_MODULE_NET_UDP")&&!has_module(&udp.runtime,"XRT_MODULE_NET_UDP_FUTURE"));
    assert(xl6ArtifactRuntimeSetHas(&udp.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_EXECUTION_UDP));
    assert(xl6ArtifactRuntimeSetHas(&udp.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_UDP_BASE_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&udp.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_FUTURE));
    assert(has_module(&udpFuture.runtime,"XRT_MODULE_NET_UDP_FUTURE"));
    assert(xl6ArtifactRuntimeSetHas(&udpFuture.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_UDP_NATIVE));
    assert(xl6ArtifactRuntimeSetHas(&udpFuture.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_FUTURE));
    assert(!has_module(&udpFuture.runtime,"XRT_MODULE_NET_TCP")&&!has_module(&udp.runtime,"XRT_MODULE_NET_TCP"));
    assert(!has_module(&udpFuture.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&udp.runtime,"XRT_MODULE_TASK_POOL"));
    assert(!xl6ArtifactRuntimeSetHas(&udpFuture.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionUdpSendBytesV1")==XL6_ARTIFACT_EXECUTION_UDP_DATA);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionUdpReceiveBatchAsyncV1")==XL6_ARTIFACT_EXECUTION_UDP_FUTURE);
    XL6_ArtifactOwnershipPlan buffer=plan("buffer-native");
    assert(buffer.domainCount==1&&!strcmp(buffer.domains[0],"xl6OwnershipDomainNetBufferNativeV1Get"));
    assert(has_module(&buffer.runtime,"XRT_MODULE_NET_BUFFER")&&!has_module(&buffer.runtime,"XRT_MODULE_NET_ENGINE"));
    assert(!xl6ArtifactRuntimeSetHas(&buffer.runtime.components,XL6_ARTIFACT_EXECUTION_NET_ENGINE));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainNetBufferNativeV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE);
    XL6_ArtifactOwnershipPlan tcp=plan("tcp-native");
    assert(has_module(&tcp.runtime,"XRT_MODULE_NET_TCP_FUTURE"));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainNetTcpNativeV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_NATIVE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipCatalogAddNetRuntimeV1")==XL6_ARTIFACT_OWNERSHIP_CATALOG);
    assert(xl6ArtifactRuntimeSetHas(&tcp.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_FUTURE));
    assert(xl6ArtifactRuntimeSetHas(&tcp.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE));
    assert(!has_module(&tcp.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&tcp.runtime,"XRT_MODULE_TASK_POOL"));
    XL6_ArtifactOwnershipPlan server=plan("server-native"),serverFuture=plan("server-future-native"),tcpBase=plan("tcp-base-native");
    assert(has_module(&tcpBase.runtime,"XRT_MODULE_NET_TCP")&&!has_module(&tcpBase.runtime,"XRT_MODULE_NET_TCP_FUTURE"));
    assert(has_module(&server.runtime,"XRT_MODULE_NET_TCP_SERVER")&&!has_module(&server.runtime,"XRT_MODULE_NET_TCP_SERVER_FUTURE"));
    assert(!xl6ArtifactRuntimeSetHas(&server.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_FUTURE));
    assert(!xl6ArtifactRuntimeSetHas(&server.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    assert(!xl6ArtifactRuntimeSetHas(&server.runtime.components,XL6_ARTIFACT_EXECUTION_TCP));
    assert(!has_module(&server.runtime,"XRT_MODULE_NET_TCP_FUTURE")&&!has_module(&server.runtime,"XRT_MODULE_NET_RESOLVER"));
    assert(!has_module(&server.runtime,"XRT_MODULE_TASK_POOL"));
    assert(has_module(&serverFuture.runtime,"XRT_MODULE_NET_TCP_SERVER_FUTURE"));
    assert(!has_module(&serverFuture.runtime,"XRT_MODULE_NET_TCP_FUTURE"));
    assert(xl6ArtifactRuntimeSetHas(&serverFuture.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NATIVE_FUTURE));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainNetTcpBaseNativeV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_BASE_NATIVE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainNetServerNativeV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_SERVER_NATIVE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainNetServerFutureNativeV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_SERVER_FUTURE_NATIVE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipCatalogAddListenerEventsV1")==XL6_ARTIFACT_OWNERSHIP_CATALOG);
    XL6_ArtifactOwnershipPlan serverResource=plan("server-resource"),serverLanguage=plan("server-language"),serverOptions=plan("server-options");
    assert(xl6ArtifactRuntimeSetHas(&serverResource.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_EXECUTION_TCP_SERVER));
    assert(xl6ArtifactRuntimeSetHas(&serverResource.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_EXECUTION_TCP));
    assert(!xl6ArtifactRuntimeSetHas(&serverResource.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    assert(!has_module(&serverLanguage.runtime,"XRT_MODULE_NET_TCP_SERVER_FUTURE"));
    assert(!has_module(&serverLanguage.runtime,"XRT_MODULE_NET_TCP_FUTURE"));
    assert(!xl6ArtifactRuntimeSetHas(&serverLanguage.runtime.components,XL6_ARTIFACT_FUTURE_CALLBACK_QUEUE));
    assert(!xl6ArtifactRuntimeSetHas(&serverOptions.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!has_module(&serverOptions.runtime,"XRT_MODULE_NET_ENGINE"));
    XL6_ArtifactOwnershipPlan resource=plan("tcp-resource");
    assert(xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_EXECUTION_TCP));
    assert(xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_EXECUTION_NET_ENGINE));
    assert(xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_NATIVE));
    assert(xl6ArtifactRuntimeSetHas(&resource.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_EXECUTION_TCP));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionTcpPrepareAcceptV1")==XL6_ARTIFACT_EXECUTION_TCP);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionTcpAcceptSourceV1")==XL6_ARTIFACT_EXECUTION_TCP);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6ExecutionTcpPromoteAcceptV1")==XL6_ARTIFACT_EXECUTION_TCP);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6OwnershipDomainExecutionTcpV1Get")==XL6_ARTIFACT_OWNERSHIP_DOMAIN_EXECUTION_TCP);
    assert(!has_module(&resource.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&resource.runtime,"XRT_MODULE_TASK_POOL"));
    XL6_ArtifactOwnershipPlan config=plan("config"),engine=plan("engine"),stats=plan("stats");
    assert(!xl6ArtifactRuntimeSetHas(&config.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!xl6ArtifactRuntimeSetHas(&config.runtime.components,XL6_ARTIFACT_CLOSURE));
    assert(!xl6ArtifactRuntimeSetHas(&stats.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(!has_module(&stats.runtime,"XRT_MODULE_NET_ENGINE"));
    assert(!xl6ArtifactRuntimeSetHas(&config.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&stats.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE));
    assert(xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_EXECUTION_SOURCE));
    assert(xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_ENGINE_NATIVE));
    assert(xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_BUFFER_NATIVE));
    assert(xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_DETACHED_WORK));
    assert(!xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_EXECUTOR_JOBS));
    assert(!xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_TASK));
    assert(!has_module(&engine.runtime,"XRT_MODULE_EXECUTOR")&&!has_module(&engine.runtime,"XRT_MODULE_TASK_POOL"));
    assert(!has_module(&engine.runtime,"XRT_MODULE_NET_RESOLVER")&&!has_module(&engine.runtime,"XRT_MODULE_NET_TCP"));
    assert(!xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_NET_TCP_NATIVE));
    assert(!xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_EXECUTION_TCP));
    assert(!xl6ArtifactRuntimeSetHas(&engine.runtime.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_EXECUTION_TCP));
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetEngineCreateV1")==XL6_ARTIFACT_NET_ENGINE_LANGUAGE);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetEngineConfigWithWorkersV1")==XL6_ARTIFACT_NET_ENGINE_CONFIG);
    assert(xl6ArtifactRuntimeSymbolOwner("xl6NetEngineSnapshotPostsAcceptedV1")==XL6_ARTIFACT_NET_ENGINE_STATS);
    puts("Engine plan: typed source/native/work domains, immutable leaves, no Executor/TaskPool/Resolver/TCP");return 0;
}
