#define XRT_MODULE_MEMORY_DEBUG
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_xrt_trim.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <stdio.h>
#include <string.h>
#define CHECK(C) do { if (!(C)) { fprintf(stderr,"artifact contract line %d: %s\n",__LINE__,#C); return 0; } } while (0)

static int empty_plan(const XL6_ArtifactRuntimePlan* plan)
{
    XL6_ArtifactRuntimePlan empty;
    memset(&empty,0,sizeof(empty));
    return !memcmp(plan,&empty,sizeof(empty));
}

static int closure_contract(void)
{
    unsigned char reaches[XL6_ARTIFACT_RUNTIME_COUNT][XL6_ARTIFACT_RUNTIME_COUNT]={{0}};
    XL6_ArtifactRuntimePlan plan;
    XL6_ArtifactRuntimeSet empty = {{0}};
    for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i) {
        const XL6_ArtifactRuntimeComponent* item=xl6ArtifactRuntimeComponentGet(i);
        CHECK(item && item->source && (!item->dependencyCount || item->dependencies));
        reaches[i][i] = 1;
        for (size_t j=0;j<item->dependencyCount;++j) {
            CHECK(item->dependencies[j] < XL6_ARTIFACT_RUNTIME_COUNT);
            reaches[i][item->dependencies[j]] = 1;
        }
    }
    for (unsigned k=0;k<XL6_ARTIFACT_RUNTIME_COUNT;++k)
        for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i)
            for (unsigned j=0;j<XL6_ARTIFACT_RUNTIME_COUNT;++j)
                reaches[i][j] |= reaches[i][k] && reaches[k][j];
    for (unsigned a=0;a<XL6_ARTIFACT_RUNTIME_COUNT;++a)
        for (unsigned b=0;b<XL6_ARTIFACT_RUNTIME_COUNT;++b) {
            XL6_ArtifactRuntimeSet expected={{0}}, roots=xl6ArtifactRuntimeSetSingleton(a);
            size_t count=0;
            CHECK(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,b));
            for (unsigned j=0;j<XL6_ARTIFACT_RUNTIME_COUNT;++j)
                if (reaches[a][j]||reaches[b][j]) {
                    CHECK(xl6ComponentSetAdd(expected.words,XL6_ARTIFACT_RUNTIME_COUNT,j)); ++count;
                }
            CHECK(xl6ArtifactRuntimeClosePlan(&roots,&plan));
            CHECK(xl6ArtifactRuntimeSetEqual(&plan.roots,&roots) &&
                xl6ArtifactRuntimeSetEqual(&plan.components,&expected) && plan.sourceCount==count);
            for (size_t i=0;i<plan.sourceCount;++i)
                for (size_t j=i+1;j<plan.sourceCount;++j) CHECK(strcmp(plan.sources[i],plan.sources[j]));
            for (size_t i=0;i<plan.nativeModuleCount;++i)
                for (size_t j=i+1;j<plan.nativeModuleCount;++j) CHECK(strcmp(plan.nativeModules[i],plan.nativeModules[j]));
            CHECK(xl6ArtifactRuntimeClosePlan(&plan.roots,&plan));
            CHECK(xl6ArtifactRuntimeSetEqual(&plan.components,&expected));
        }
    CHECK(xl6ArtifactRuntimeClosePlan(&empty,&plan) && empty_plan(&plan));
    memset(&plan,0xA5,sizeof(plan));
    CHECK(!xl6ArtifactRuntimeClosePlan(NULL,&plan) && empty_plan(&plan));
    if (XL6_ARTIFACT_RUNTIME_COUNT % 64u) {
        XL6_ArtifactRuntimeSet invalid = {{0}};
        invalid.words[XL6_ARTIFACT_RUNTIME_WORD_COUNT-1] = UINT64_C(1) << (XL6_ARTIFACT_RUNTIME_COUNT % 64u);
        CHECK(!xl6ArtifactRuntimeClosePlan(&invalid,&plan) && empty_plan(&plan));
    }
    CHECK(!xl6ArtifactRuntimeClosePlan(&empty,NULL));
    CHECK(!xl6ArtifactRuntimeComponentGet(XL6_ARTIFACT_RUNTIME_COUNT));
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SlotRuntimeV1Get")==XL6_ARTIFACT_SLOT);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SlotRuntimeV1GetAlmost")==XL6_ARTIFACT_RUNTIME_COUNT);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6FutureLooksLikeRuntime")==XL6_ARTIFACT_RUNTIME_COUNT);
    CHECK(xl6ArtifactRuntimeSymbolOwner(NULL)==XL6_ARTIFACT_RUNTIME_COUNT);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SequenceOwnershipAdapterV1")==XL6_ARTIFACT_SEQUENCE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SequenceCreateAdmittedV1")==XL6_ARTIFACT_SEQUENCE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SequenceCloneRoleV1")==XL6_ARTIFACT_SEQUENCE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6CollectionIdentityEnterV1")==XL6_ARTIFACT_SEQUENCE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6CodeOwnerScopeForeign")==XL6_ARTIFACT_CODE_LEASE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6PersistenceRestorePlanTakeV1")==XL6_ARTIFACT_ARRAY_IMPORT_CHAIN);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ListCreateTakeV1")==XL6_ARTIFACT_LIST_NATIVE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ListOwnershipAdapterV2")==XL6_ARTIFACT_LIST_NATIVE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6TreeDomainsCopyV1")==XL6_ARTIFACT_TREE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SpscQueueAdmitsSchemaV1")==XL6_ARTIFACT_TYPED_QUEUE_SPSC);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ArraySemAdmissionOpsV2Get")==XL6_ARTIFACT_ARRAY_SEMAPHORE_ADMISSION);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SyncOwnerBeginNative")==XL6_ARTIFACT_SYNC_CORE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xlSyncRetirementAcquireCheckedV1")==XL6_ARTIFACT_SYNC_CORE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xlSyncRetirementAcquireCheckedV1Almost")==XL6_ARTIFACT_RUNTIME_COUNT);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ArrayCreateV1")==XL6_ARTIFACT_ARRAY_TYPED);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6StackCreateV1")==XL6_ARTIFACT_STACK);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ArrayReadI64V1")==XL6_ARTIFACT_ARRAY_SCALAR_READ);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ArrayReadF64V1")==XL6_ARTIFACT_ARRAY_SCALAR_READ);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ArrayIteratorSnapshotV1")==XL6_ARTIFACT_ARRAY_ITERATOR);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ArrayIteratorCreateErasedV1")==XL6_ARTIFACT_ARRAY_VALUE_ITERATOR);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6ArrayIteratorAdvanceValueV1")==XL6_ARTIFACT_ARRAY_VALUE_ITERATOR);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6StackOwnershipAdapterV1")==XL6_ARTIFACT_RUNTIME_COUNT);
    XL6_ArtifactRuntimeSet sequence=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SEQUENCE);
    CHECK(xl6ArtifactRuntimeClosePlan(&sequence,&plan) && plan.sourceCount==1);
    const XL6_ArtifactRuntimeId factories[]={XL6_ARTIFACT_ARRAY_REFERENCE_ADMISSION,
        XL6_ARTIFACT_ARRAY_NOMINAL_ADMISSION,XL6_ARTIFACT_ARRAY_PRIMITIVE_ADMISSION,
        XL6_ARTIFACT_ARRAY_C_SCALAR_ADMISSION,XL6_ARTIFACT_ARRAY_SEQUENCE_ADMISSION,
        XL6_ARTIFACT_ARRAY_NULLABLE_C_SCALAR_ADMISSION,XL6_ARTIFACT_ARRAY_NUMERIC_ENUM_ADMISSION,
        XL6_ARTIFACT_ARRAY_CALLABLE_ADMISSION,XL6_ARTIFACT_ARRAY_FUTURE_ADMISSION};
    for (size_t i=0;i<sizeof(factories)/sizeof(factories[0]);++i) {
        XL6_ArtifactRuntimeSet root=xl6ArtifactRuntimeSetSingleton(factories[i]);
        CHECK(xl6ArtifactRuntimeClosePlan(&root,&plan));
        CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STACK));
        for (size_t j=0;j<plan.nativeModuleCount;++j)
            CHECK(strcmp(plan.nativeModules[j],"XRUNTIME_MODULE_TYPED_STACK"));
    }
    XL6_ArtifactRuntimeSet array=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_ARRAY_TYPED);
    CHECK(xl6ArtifactRuntimeClosePlan(&array,&plan) && plan.sourceCount==2);
    CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STACK));
    XL6_ArtifactRuntimeSet stack=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_STACK);
    CHECK(xl6ArtifactRuntimeClosePlan(&stack,&plan) && plan.sourceCount==2);
    CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED));
    XL6_ArtifactRuntimeSet typedCursor=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_ARRAY_ITERATOR);
    CHECK(xl6ArtifactRuntimeClosePlan(&typedCursor,&plan) && plan.sourceCount==2 &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_VALUE_ITERATOR) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_ITERATOR));
    XL6_ArtifactRuntimeSet valueCursor=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_ARRAY_VALUE_ITERATOR);
    CHECK(xl6ArtifactRuntimeClosePlan(&valueCursor,&plan) && plan.sourceCount==3 &&
        xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_ITERATOR) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_INPUT));
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6FutureAggregateRuntimeV1Get")==XL6_ARTIFACT_RUNTIME_COUNT);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6FutureAggregateAllV2")==XL6_ARTIFACT_FUTURE_AGGREGATE);
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6FutureAggregateAllArrayV2")==XL6_ARTIFACT_FUTURE_AGGREGATE_ARRAY);
    const XL6_ArtifactRuntimeId roots[]={XL6_ARTIFACT_FUTURE_BRIDGE,
        XL6_ARTIFACT_FUTURE_AGGREGATE,XL6_ARTIFACT_FUTURE_AGGREGATE_ARRAY};
    for (size_t i=0;i<3;++i) {
        XL6_ArtifactRuntimeSet root=xl6ArtifactRuntimeSetSingleton(roots[i]);
        CHECK(xl6ArtifactRuntimeClosePlan(&root,&plan));
        CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_AGGREGATE)==(i!=0));
        CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_AGGREGATE_ARRAY)==(i==2));
        CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_INPUT)==(i==2));
        /* Common capture lifetime now includes the native-capable iterator.
         * This is a real dependency, not an ArrayInput/Array facade request. */
        CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_ITERATOR) &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_VALUE_ITERATOR) &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED));
        CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_BRIDGE)==(i==0));
    }
    return 1;
}

static void print_set(const XL6_ArtifactRuntimeSet* set)
{
    int comma = 0;
    putchar('[');
    for (size_t id=0;id<XL6_ARTIFACT_RUNTIME_COUNT;++id)
        if (xl6ArtifactRuntimeSetHas(set,id)) { printf("%s%zu",comma?",":"",id); comma=1; }
    putchar(']');
}

static int native_catalog_contract(void)
{
    static const XL6_ArtifactNativeSymbol expected[] = {
#define XL6_ARTIFACT_NATIVE_SYMBOL(Symbol, Module) {Symbol, Module},
#include "src/compile/xl6_artifact_native_symbols.inc"
#undef XL6_ARTIFACT_NATIVE_SYMBOL
    };
    size_t total=sizeof(expected)/sizeof(expected[0]), checked=0;
    const XL6_ArtifactNativeSymbol* result;
    size_t count=99;
    CHECK(!xl6ArtifactNativeSymbolLookup(NULL,&count) && !count);
    CHECK(!xl6ArtifactNativeSymbolLookup("",&count) && !count);
    CHECK(!xl6ArtifactNativeSymbolLookup("xrtEncodingUnitSizeAlmost",&count) && !count);
    CHECK(!xl6ArtifactNativeSymbolLookup("xrtEncodingUnitSize",NULL));
    for (size_t i=0;i<total;) {
        size_t end=i+1;
        char near[256];
        while (end<total && !strcmp(expected[i].symbol,expected[end].symbol)) ++end;
        if (end<total) CHECK(strcmp(expected[i].symbol,expected[end].symbol)<0);
        result=xl6ArtifactNativeSymbolLookup(expected[i].symbol,&count);
        CHECK(result && count==end-i);
        for (size_t j=0;j<count;++j) {
            CHECK(!strcmp(result[j].symbol,expected[i+j].symbol));
            CHECK((!result[j].module && !expected[i+j].module) ||
                (result[j].module && expected[i+j].module && !strcmp(result[j].module,expected[i+j].module)));
        }
        CHECK(strlen(expected[i].symbol)+2<sizeof(near));
        snprintf(near,sizeof(near),"%s!",expected[i].symbol);
        CHECK(!xl6ArtifactNativeSymbolLookup(near,&count) && !count);
        ++checked; i=end;
    }
    CHECK(checked>3000);
    printf("{\"NativeSymbols\":%zu,\"ProviderRows\":%zu,\"NearMissesRejected\":%zu}\n",checked,total,checked);
    return 1;
}

static int has_native_module(const XL6_ArtifactRuntimePlan* plan, const char* name)
{
    for (size_t i=0;i<plan->nativeModuleCount;++i)
        if (!strcmp(plan->nativeModules[i],name)) return 1;
    return 0;
}

static int slot_key_component_contract(void)
{
    XL6_ArtifactRuntimePlan plan;
    XL6_ArtifactRuntimeSet root=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SLOT_MAP_KEY_NATIVE);
    CHECK(xl6ArtifactRuntimeClosePlan(&root,&plan) && plan.sourceCount==2);
    CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FOREIGN_ERROR) &&
        xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_NATIVE));
    CHECK(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_NATIVE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_VALUE));
    CHECK(!has_native_module(&plan,"XRT_MODULE_SLOT_MAP") && !has_native_module(&plan,"XRT_MODULE_HASH64") &&
        !has_native_module(&plan,"XRUNTIME_MODULE_TYPED_SET"));
    root=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SLOT_MAP_KEY_VALUE);
    CHECK(xl6ArtifactRuntimeClosePlan(&root,&plan));
    CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_NATIVE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_NATIVE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_NATIVE));
    CHECK(has_native_module(&plan,"XRT_MODULE_HASH64") && !has_native_module(&plan,"XRT_MODULE_SLOT_MAP") &&
        !has_native_module(&plan,"XRUNTIME_MODULE_TYPED_SET"));
    root=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SLOT_MAP_NATIVE);
    CHECK(xl6ArtifactRuntimeClosePlan(&root,&plan));
    CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_NATIVE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_VALUE));
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SlotMapKeyTypeV1")==XL6_ARTIFACT_SLOT_MAP_KEY_NATIVE &&
        xl6ArtifactRuntimeSymbolOwner("xl6SlotMapKeyBoxV1")==XL6_ARTIFACT_SLOT_MAP_KEY_VALUE);
    root=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SET_KEYS);
    CHECK(xl6ArtifactRuntimeClosePlan(&root,&plan));
    CHECK(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_LEAF_VALUE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_NATIVE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_NATIVE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SLOT_MAP_KEY_VALUE) &&
        !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_NATIVE_VALUE_ADMISSION));
    CHECK(!has_native_module(&plan,"XRT_MODULE_SLOT_MAP") && !has_native_module(&plan,"XRT_MODULE_HASH64"));
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6NativeLeafValueIsV1")==XL6_ARTIFACT_NATIVE_LEAF_VALUE);
    puts("{\"NativeValueSetIsolatedPlans\":1,\"KeyImplementationNotSelected\":true}");
    puts("{\"SlotKeyIsolatedPlans\":3,\"StorageAndProjectionSeparated\":true}");
    return 1;
}

static int count_native(void* context, const char* module)
{
    unsigned* count=context;
    if (!module || !*module) return 0;
    ++*count;
    return 1;
}

static int reject_native(void* context, const char* module)
{
    (void)module;
    ++*(unsigned*)context;
    return 0;
}

static int artifact_source_contract(void)
{
    char* text=NULL;
    const char* roots[]={"XRT_MODULE_CHARSET"};
    const char* invalid[]={NULL};
    char* packages[]={"XRT_MODULE_MATH"};
    XL6_ImportDeps deps;
    memset(&deps,0,sizeof(deps));
    CHECK(!xl6XrtTrimRequiresFull(NULL,0) && xl6XrtTrimRequiresFull(NULL,1));
    CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,0,NULL,0,&text));
    CHECK(!strstr(text,"#define XRT_MODULE_") && !strstr(text,"#define XRUNTIME_MODULE_"));
    CHECK(strstr(text,"#define XRUNTIME_IMPLEMENTATION\n"));
    xrtFree(text); text=NULL;
    deps.xrtModules.items=packages; deps.xrtModules.count=1;
    CHECK(!xl6XrtTrimRequiresFull(&deps,0));
    CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(&deps,0,roots,1,&text));
    CHECK(strstr(text,"#define XRT_MODULE_CHARSET\n") && strstr(text,"#define XRT_MODULE_MATH\n"));
    CHECK(!strstr(text,"XRT_MODULE_ALL") && !strstr(text,"XRT_MODULE_TEMP_MEMORY"));
    xrtFree(text); text=NULL;
    /* 不修改导入依赖；同一产物增加 C 源后完整编译，再无 C 源时仍精确裁剪。 */
    for (size_t sources=1;sources<3;++sources) {
        CHECK(xl6XrtTrimRequiresFull(&deps,sources));
        CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(&deps,sources,roots,1,&text));
        CHECK(strstr(text,"#define XRT_MODULE_ALL\n") && strstr(text,"#define XRUNTIME_MODULE_ALL\n"));
        xrtFree(text); text=NULL;
        CHECK(!deps.hasUndeclaredCSourceModules && deps.features==0);
    }
    CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(&deps,0,roots,1,&text));
    CHECK(!strstr(text,"XRT_MODULE_ALL") && strstr(text,"XRT_MODULE_CHARSET"));
    xrtFree(text); text=NULL;
    for (unsigned policy=0;policy<4;++policy) {
        deps.features=policy==0?XL6_IMPORT_FEATURE_TCC_RUNTIME:
            policy==1?XL6_IMPORT_FEATURE_XLANG_RUNTIME:policy==2?XL6_IMPORT_FEATURE_XLANG_COMPILER:0;
        deps.hasUndeclaredCSourceModules=policy==3;
        CHECK(xl6XrtTrimRequiresFull(&deps,0));
        CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(&deps,0,roots,1,&text));
        CHECK(strstr(text,"#define XRT_MODULE_ALL\n") && strstr(text,"#define XRUNTIME_MODULE_ALL\n"));
        xrtFree(text); text=NULL;
    }
    CHECK(!xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,0,NULL,1,&text) && !text);
    CHECK(!xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,0,invalid,1,&text) && !text);
    CHECK(!xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,0,NULL,0,NULL));
    return 1;
}

static void print_plan(const char* name, const XL6_ArtifactRuntimePlan* plan);

static int emit_native_source(const char* path, const char* output, const char* cSource)
{
    XL6_CompileUnit unit;
    XL6_ArtifactRuntimePlan plan;
    XL6_Output out;
    char* text=NULL;
    int ok=0;
    xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit,path,1) || !xl6IrLowerModule(&unit.ir,&unit.parse.diags) ||
        !xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags)) goto done;
    if (cSource) {
        /* Same explicit input policy as xlcAddCSourceFile: unrestricted C has
         * undeclared native requirements and currently selects the full XRT.
         * Do not pretend its executable uses the pure-language pruned plan. */
        FILE* sidecar=fopen(cSource,"rb");
        if (!sidecar) goto done;
        fclose(sidecar);
        unit.importDeps.hasUndeclaredCSourceModules=1;
    }
    if (!xl6XrtTrimBuildImplementationSourceFromArtifact(&unit.importDeps,0,
            plan.nativeModules,plan.nativeModuleCount,&text)) goto done;
    if (!xl6OutputOpenFile(&out,output)) goto done;
    ok=xl6OutputPuts(text,&out)!=EOF && !xl6OutputHasFailed(&out);
    xl6OutputClose(&out);
    if (ok) print_plan(path,&plan);
done:
    if (!ok) xl6DiagPrintText(&unit.parse.diags,path);
    xrtFree(text); xl6CompileUnitFree(&unit);
    return ok;
}

static void print_plan(const char* name, const XL6_ArtifactRuntimePlan* plan)
{
    printf("{\"Fixture\":\"%s\",\"Roots\":",name); print_set(&plan->roots);
    printf(",\"Components\":"); print_set(&plan->components); printf(",\"Sources\":[");
    for (size_t i=0;i<plan->sourceCount;++i) printf("%s\"%s\"",i?",":"",plan->sources[i]);
    printf("],\"NativeModules\":[");
    for (size_t i=0;i<plan->nativeModuleCount;++i) printf("%s\"%s\"",i?",":"",plan->nativeModules[i]);
    puts("],\"TypedRefused\":true,\"CorruptRefused\":true,\"Deterministic\":true}");
}

static int allocation_contract(const XL6_IrModule* module)
{
    unsigned failures=0,successes=0;
    for (unsigned i=0;i<9;++i) {
        XL6_ArtifactRuntimePlan plan;
        XL6_DiagList probe;
        xmemdebugsnapshot before,after;
        bool triggered;
        int ok;
        xl6DiagInit(&probe); xrtClearError(); xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(i==8?100000:i));
        memset(&plan,0xA5,sizeof(plan));
        ok=xl6ArtifactRuntimePlanBuild(module,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&probe);
        triggered=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        xl6DiagFree(&probe); xrtClearError(); xrtMemDebugSnapshot(&after);
        CHECK(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
        CHECK(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
        CHECK(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
            after.UseAfterFreeCount==before.UseAfterFreeCount);
        if (ok) { CHECK(!triggered && plan.sourceCount); ++successes; }
        else { CHECK(triggered && empty_plan(&plan)); ++failures; }
    }
    CHECK(failures && successes);
    printf("{\"AllocationBudgets\":9,\"ResourceFailures\":%u,\"SuccessfulBudgets\":%u,\"Balanced\":true}\n",failures,successes);
    return 1;
}

static int fixture_contract(const char* path)
{
    XL6_CompileUnit unit;
    XL6_DiagList probe;
    XL6_ArtifactRuntimePlan plan,repeat;
    XL6_IrSlotTransferAbi original;
    XL6_IrCgenOptions options={.emitScriptEntry=1,.emitMainWrapper=1};
    unsigned visits=0;
    int ok=0;
    xl6CompileUnitInit(&unit); xl6DiagInit(&probe);
    if (!xl6CompileUnitLoadFile(&unit,path,1)) goto done;
    if (xl6IrCgenVisitNativeModules(&unit.ir,&probe,&options,count_native,&visits) || visits) goto done;
    xl6DiagFree(&probe); xl6DiagInit(&probe);
    memset(&plan,0xA5,sizeof(plan));
    if (xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&probe)||!empty_plan(&plan)||!probe.count) goto done;
    xl6DiagFree(&probe); xl6DiagInit(&probe);
    if (!xl6IrLowerModule(&unit.ir,&unit.parse.diags) ||
        !xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags) ||
        !xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&repeat,&unit.parse.diags) ||
        memcmp(&plan,&repeat,sizeof(plan))) goto done;
    if (!xl6IrCgenVisitNativeModules(&unit.ir,&probe,&options,count_native,&visits) || !visits) goto done;
    visits=0;
    if (xl6IrCgenVisitNativeModules(&unit.ir,&probe,&options,reject_native,&visits) || visits!=1) goto done;
    if (xl6IrCgenVisitNativeModules(&unit.ir,&probe,&options,NULL,NULL)) goto done;
    original=unit.ir.abi.slotTransferAbi;
    unit.ir.abi.slotTransferAbi=XL6_IR_SLOT_TRANSFER_NONE;
    visits=0;
    if (xl6IrCgenVisitNativeModules(&unit.ir,&probe,&options,count_native,&visits) || visits) goto done;
    memset(&repeat,0xA5,sizeof(repeat));
    ok=!xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&repeat,&probe) && empty_plan(&repeat) && probe.count;
    unit.ir.abi.slotTransferAbi=original;
    xl6DiagFree(&probe); xl6DiagInit(&probe);
    if (xl6ArtifactRuntimePlanBuild(&unit.ir,(XL6_ArtifactEntryAbi)99,&repeat,&probe) ||
        !empty_plan(&repeat) || !probe.count) ok=0;
    if (strstr(path,"stdlib_stack_nullable_c_")) {
        const XL6_ArtifactRuntimeId descriptors[]={XL6_ARTIFACT_NULLABLE_BOOL,
            XL6_ARTIFACT_NULLABLE_I64,XL6_ARTIFACT_NULLABLE_U64,
            XL6_ARTIFACT_NULLABLE_F32,XL6_ARTIFACT_NULLABLE_F64,
            XL6_ARTIFACT_NULLABLE_NATIVE_I8,XL6_ARTIFACT_NULLABLE_NATIVE_U8,
            XL6_ARTIFACT_NULLABLE_NATIVE_I16,XL6_ARTIFACT_NULLABLE_NATIVE_U16,
            XL6_ARTIFACT_NULLABLE_NATIVE_I32,XL6_ARTIFACT_NULLABLE_NATIVE_U32};
        const XL6_ArtifactRuntimeId converters[]={XL6_ARTIFACT_NULLABLE_BOOL_VALUE,
            XL6_ARTIFACT_NULLABLE_I64_VALUE,XL6_ARTIFACT_NULLABLE_U64_VALUE,
            XL6_ARTIFACT_NULLABLE_F32_VALUE,XL6_ARTIFACT_NULLABLE_F64_VALUE,
            XL6_ARTIFACT_NULLABLE_NATIVE_I8_VALUE,XL6_ARTIFACT_NULLABLE_NATIVE_U8_VALUE,
            XL6_ARTIFACT_NULLABLE_NATIVE_I16_VALUE,XL6_ARTIFACT_NULLABLE_NATIVE_U16_VALUE,
            XL6_ARTIFACT_NULLABLE_NATIVE_I32_VALUE,XL6_ARTIFACT_NULLABLE_NATIVE_U32_VALUE};
        bool one=strstr(path,"stdlib_stack_nullable_c_i8")!=NULL;
        for (size_t i=0;i<sizeof(descriptors)/sizeof(*descriptors);++i) {
            if (xl6ArtifactRuntimeSetHas(&plan.components,descriptors[i])!=
                (!one || descriptors[i]==XL6_ARTIFACT_NULLABLE_NATIVE_I8) ||
                xl6ArtifactRuntimeSetHas(&plan.components,converters[i])) ok=0;
        }
        if (!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_STACK) ||
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED) ||
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_NULLABLE_C_SCALAR_ADMISSION)) ok=0;
    }
    if (strstr(path,"cgen_basic") && !allocation_contract(&unit.ir)) ok=0;
    if (strstr(path,"stdlib_array_shuffle_")) {
        bool useDefault=strstr(path,"default_only")!=NULL;
        ok=ok && xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_RAND_SHUFFLE)==useDefault &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_RNG_SHUFFLE)==!useDefault &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_INPUT) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_FUTURE_ADMISSION) &&
            (useDefault || !has_native_module(&plan,"XRT_MODULE_RANDOM_DEFAULT"));
    }
    if (strstr(path,"cgen_basic"))
        ok=ok && !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_RNG_SHUFFLE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_RAND_SHUFFLE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_COLLECTION_COUNT) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_SCALAR_READ);
    if (strstr(path,"stdlib_collection_count"))
        ok=ok && xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_COLLECTION_COUNT) &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_INPUT) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED);
    if (strstr(path,"stdlib_array_scalar_read"))
        ok=ok && xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_SCALAR_READ) &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_INPUT) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED);
    if (strstr(path,"stdlib_future_aggregate_") || strstr(path,"stdlib_future_metadata_only")) {
        bool metadata=strstr(path,"metadata_only")!=NULL;
        bool arrayInput=strstr(path,"aggregate_array_only")!=NULL;
        ok=ok && xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_AGGREGATE)==!metadata &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_AGGREGATE_ARRAY)==arrayInput &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_INPUT)==arrayInput &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SEQUENCE)==
                (arrayInput || xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_ITERATOR)) &&
            xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_METADATA)==metadata &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_BRIDGE);
    }
    if (strstr(path,"xlir_artifact_linkage_literal"))
        ok=ok && !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_FUTURE_BRIDGE) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TASK_GROUP) &&
            !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CHANNEL);
    if (strstr(path,"xlir_artifact_native_linkage"))
        ok=ok && has_native_module(&plan,"XRT_MODULE_CHARSET") &&
            !has_native_module(&plan,"XRT_MODULE_MATH");
    if (strstr(path,"xlir_string_bridge_abi"))
        ok=ok && has_native_module(&plan,"XRT_MODULE_UNICODE_TEXT") &&
            has_native_module(&plan,"XRT_MODULE_STRING_SPLIT");
    if (strstr(path,"xlir_artifact_native_cstring")) {
        unsigned signatures=0,mutations=0;
        for (size_t i=0;i<unit.ir.callTargetCount;++i) {
            XL6_IrCallTarget* target=&unit.ir.callTargets[i];
            if (target->resultCAbi!=XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER) continue;
            if (!xl6IrCallResultCAbiMatches(&unit.ir,target)) { ok=0; break; }
            ++signatures;
            target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_STORAGE;
            visits=0;
            if (xl6IrCallResultCAbiMatches(&unit.ir,target) ||
                xl6IrCgenVisitNativeModules(&unit.ir,&probe,&options,count_native,&visits) || visits ||
                xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&repeat,&probe) || !empty_plan(&repeat)) ok=0;
            target->resultCAbi=XL6_IR_CALL_RESULT_C_ABI_CHAR_POINTER;
            ++mutations;
            {
                char* name=target->cName;
                target->cName="not_a_native_c_string_function";
                if (xl6IrCallResultCAbiMatches(&unit.ir,target)) ok=0;
                target->cName=name; ++mutations;
            }
            if (target->parameterCount) {
                XL6_IrParameterAbi abi=target->parameterAbis[0];
                target->parameterAbis[0]=XL6_IR_PARAMETER_ABI_VALUE;
                if (xl6IrCallResultCAbiMatches(&unit.ir,target)) ok=0;
                target->parameterAbis[0]=abi; ++mutations;
            }
            xl6DiagFree(&probe); xl6DiagInit(&probe);
        }
        if (signatures!=12 || mutations!=32) ok=0;
        printf("{\"NativeCStringSignatures\":%u,\"RejectedMutations\":%u}\n",signatures,mutations);
    }
    if (ok) print_plan(path,&plan);
done:
    if (!ok) { fprintf(stderr,"artifact plan failed: %s\n",path); xl6DiagPrintText(&unit.parse.diags,path); }
    xl6DiagFree(&probe); xl6CompileUnitFree(&unit);
    return ok;
}

int main(int argc,char** argv)
{
    if (!xrtMemDebugEnable(true)) return 1;
    if (!closure_contract() || !artifact_source_contract()) return 1;
    if (argc==4 && !strcmp(argv[1],"--emit-native")) return emit_native_source(argv[2],argv[3],NULL)?0:1;
    if (argc==5 && !strcmp(argv[1],"--emit-native-with-c")) return emit_native_source(argv[2],argv[3],argv[4])?0:1;
    if (argc==2 && !strcmp(argv[1],"--catalog")) {
        for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i) {
            const XL6_ArtifactRuntimeComponent* item=xl6ArtifactRuntimeComponentGet(i);
            XL6_ArtifactRuntimePlan plan;
            XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(i);
            if (!xl6ArtifactRuntimeClosePlan(&roots,&plan)) return 1;
            printf("{\"Id\":%u,\"Source\":\"%s\",\"Dependencies\":[",i,item->source);
            for (size_t j=0;j<item->dependencyCount;++j) printf("%s%zu",j?",":"",item->dependencies[j]);
            printf("],\"Closure\":"); print_set(&plan.components); puts("}");
        }
        return 0;
    }
    if (argc<2) return 2;
    if (!native_catalog_contract() || !slot_key_component_contract()) return 1;
    for (int i=1;i<argc;++i) if (!fixture_contract(argv[i])) return 1;
    printf("artifact runtime contract complete: %u dependency pairs; typed/corrupt IR refusal and deterministic plans\n",
        (unsigned)(XL6_ARTIFACT_RUNTIME_COUNT * XL6_ARTIFACT_RUNTIME_COUNT));
    return 0;
}
