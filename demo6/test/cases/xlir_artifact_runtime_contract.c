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
    for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i) {
        const XL6_ArtifactRuntimeComponent* item=xl6ArtifactRuntimeComponentGet(i);
        CHECK(item && item->source);
        for (unsigned j=0;j<XL6_ARTIFACT_RUNTIME_COUNT;++j)
            reaches[i][j]=(i==j)||((item->dependencies & ((uint32)1u<<j))!=0);
    }
    for (unsigned k=0;k<XL6_ARTIFACT_RUNTIME_COUNT;++k)
        for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i)
            for (unsigned j=0;j<XL6_ARTIFACT_RUNTIME_COUNT;++j)
                reaches[i][j] |= reaches[i][k] && reaches[k][j];
    for (unsigned a=0;a<XL6_ARTIFACT_RUNTIME_COUNT;++a)
        for (unsigned b=0;b<XL6_ARTIFACT_RUNTIME_COUNT;++b) {
            uint32 expected=0, roots=((uint32)1u<<a)|((uint32)1u<<b);
            size_t count=0;
            for (unsigned j=0;j<XL6_ARTIFACT_RUNTIME_COUNT;++j)
                if (reaches[a][j]||reaches[b][j]) { expected |= (uint32)1u<<j; ++count; }
            CHECK(xl6ArtifactRuntimeClosePlan(roots,&plan));
            CHECK(plan.roots==roots && plan.components==expected && plan.sourceCount==count);
            for (size_t i=0;i<plan.sourceCount;++i)
                for (size_t j=i+1;j<plan.sourceCount;++j) CHECK(strcmp(plan.sources[i],plan.sources[j]));
            for (size_t i=0;i<plan.nativeModuleCount;++i)
                for (size_t j=i+1;j<plan.nativeModuleCount;++j) CHECK(strcmp(plan.nativeModules[i],plan.nativeModules[j]));
        }
    CHECK(xl6ArtifactRuntimeClosePlan(0,&plan) && empty_plan(&plan));
    memset(&plan,0xA5,sizeof(plan));
    CHECK(!xl6ArtifactRuntimeClosePlan(0x80000000u,&plan) && empty_plan(&plan));
    CHECK(!xl6ArtifactRuntimeClosePlan(0,NULL));
    CHECK(!xl6ArtifactRuntimeComponentGet(XL6_ARTIFACT_RUNTIME_COUNT));
    CHECK(xl6ArtifactRuntimeSymbolOwner("xl6SlotRuntimeV1Get")==XL6_ARTIFACT_BIT(SLOT));
    CHECK(!xl6ArtifactRuntimeSymbolOwner("xl6SlotRuntimeV1GetAlmost"));
    CHECK(!xl6ArtifactRuntimeSymbolOwner("xl6FutureLooksLikeRuntime"));
    CHECK(!xl6ArtifactRuntimeSymbolOwner(NULL));
    return 1;
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
    CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,NULL,0,&text));
    CHECK(!strstr(text,"#define XRT_MODULE_") && !strstr(text,"#define XRUNTIME_MODULE_"));
    CHECK(strstr(text,"#define XRUNTIME_IMPLEMENTATION\n"));
    xrtFree(text); text=NULL;
    deps.xrtModules.items=packages; deps.xrtModules.count=1;
    CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(&deps,roots,1,&text));
    CHECK(strstr(text,"#define XRT_MODULE_CHARSET\n") && strstr(text,"#define XRT_MODULE_MATH\n"));
    CHECK(!strstr(text,"XRT_MODULE_ALL") && !strstr(text,"XRT_MODULE_TEMP_MEMORY"));
    xrtFree(text); text=NULL;
    for (unsigned policy=0;policy<4;++policy) {
        deps.features=policy==0?XL6_IMPORT_FEATURE_TCC_RUNTIME:
            policy==1?XL6_IMPORT_FEATURE_XLANG_RUNTIME:policy==2?XL6_IMPORT_FEATURE_XLANG_COMPILER:0;
        deps.hasUndeclaredCSourceModules=policy==3;
        CHECK(xl6XrtTrimBuildImplementationSourceFromArtifact(&deps,roots,1,&text));
        CHECK(strstr(text,"#define XRT_MODULE_ALL\n") && strstr(text,"#define XRUNTIME_MODULE_ALL\n"));
        xrtFree(text); text=NULL;
    }
    CHECK(!xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,NULL,1,&text) && !text);
    CHECK(!xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,invalid,1,&text) && !text);
    CHECK(!xl6XrtTrimBuildImplementationSourceFromArtifact(NULL,NULL,0,NULL));
    return 1;
}

static void print_plan(const char* name, const XL6_ArtifactRuntimePlan* plan);

static int emit_native_source(const char* path, const char* output)
{
    XL6_CompileUnit unit;
    XL6_ArtifactRuntimePlan plan;
    XL6_Output out;
    char* text=NULL;
    int ok=0;
    xl6CompileUnitInit(&unit);
    if (!xl6CompileUnitLoadFile(&unit,path,1) || !xl6IrLowerModule(&unit.ir,&unit.parse.diags) ||
        !xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags) ||
        !xl6XrtTrimBuildImplementationSourceFromArtifact(&unit.importDeps,plan.nativeModules,plan.nativeModuleCount,&text)) goto done;
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
    printf("{\"Fixture\":\"%s\",\"Roots\":%u,\"Components\":%u,\"Sources\":[",name,plan->roots,plan->components);
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
    XL6_IrCgenOptions options={1,1};
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
    if (strstr(path,"cgen_basic") && !allocation_contract(&unit.ir)) ok=0;
    if (strstr(path,"xlir_artifact_linkage_literal"))
        ok=ok && !(plan.components & (XL6_ARTIFACT_BIT(FUTURE_BRIDGE)|XL6_ARTIFACT_BIT(TASK_GROUP)|XL6_ARTIFACT_BIT(CHANNEL)));
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
    if (argc==4 && !strcmp(argv[1],"--emit-native")) return emit_native_source(argv[2],argv[3])?0:1;
    if (argc==2 && !strcmp(argv[1],"--catalog")) {
        for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i) {
            const XL6_ArtifactRuntimeComponent* item=xl6ArtifactRuntimeComponentGet(i);
            XL6_ArtifactRuntimePlan plan;
            if (!xl6ArtifactRuntimeClosePlan((uint32)1u<<i,&plan)) return 1;
            printf("{\"Id\":%u,\"Source\":\"%s\",\"Dependencies\":%u,\"Closure\":%u}\n",i,item->source,item->dependencies,plan.components);
        }
        return 0;
    }
    if (argc<2) return 2;
    if (!native_catalog_contract()) return 1;
    for (int i=1;i<argc;++i) if (!fixture_contract(argv[i])) return 1;
    printf("artifact runtime contract complete: %u dependency pairs; typed/corrupt IR refusal and deterministic plans\n",
        (unsigned)(XL6_ARTIFACT_RUNTIME_COUNT * XL6_ARTIFACT_RUNTIME_COUNT));
    return 0;
}
