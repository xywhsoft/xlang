#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"

#include "src/compile/xl6_imports.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"

#include <stdio.h>
#include <string.h>

#define R(Name) XL6_ARTIFACT_##Name
#define D(...) (const size_t[]){__VA_ARGS__}, sizeof((const size_t[]){__VA_ARGS__})/sizeof(size_t)
static const XL6_ArtifactRuntimeComponent xl6ArtifactComponents[] = {
    {"src/runtime/xl6_slot.c", NULL, 0, {"XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_code_lease.c", NULL, 0, {"XRT_MODULE_MEMORY", "XRT_MODULE_ATOMIC", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_closure.c", D(R(CODE_LEASE),R(CELL)),
        {"XRT_MODULE_FUTURE", "XRUNTIME_MODULE_RUNTIME_CALL", "XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE"}},
    {"src/runtime/xl6_cell.c", NULL, 0, {"XRT_MODULE_MEMORY", "XRT_MODULE_ATOMIC", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_frame_storage.c", NULL, 0, {"XRT_MODULE_MEMORY"}},
    {"src/runtime/xl6_task.c", D(R(CAPTURE),R(CODE_LEASE),R(FUTURE_PAYLOAD)), {"XRT_MODULE_TASK_POOL"}},
    {"src/runtime/xl6_continuation_queue.c", D(R(CODE_LEASE)),
        {"XRT_MODULE_MUTEX", "XRT_MODULE_COND", "XRT_MODULE_ERROR"}},
    {"src/runtime/xl6_continuation_executor.c", D(R(QUEUE)), {"XRT_MODULE_THREAD", "XRT_MODULE_ERROR"}},
    {"src/runtime/xl6_continuation_future.c", D(R(QUEUE)), {"XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_continuation_frame.c", D(R(CAPTURE),R(CODE_LEASE),R(QUEUE),R(FOREIGN_ERROR),R(FUTURE_WAIT)),
        {"XRT_MODULE_THREAD", "XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_future_wait.c", D(R(CONTINUATION_FRAME),R(FUTURE_CALLBACK)), {"XRT_MODULE_FUTURE_COMBINE"}},
    {"src/runtime/xl6_capture.c", D(R(CELL),R(CLOSURE),R(FOREIGN_ERROR),R(GENERATOR),R(VALUE_ITERATOR)),
        {"XRT_MODULE_MEMORY", "XRT_MODULE_VALUE", "XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_value_iterator.c", NULL, 0, {"XRT_MODULE_MEMORY", "XRT_MODULE_ATOMIC", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_future_callback.c", D(R(CAPTURE),R(CODE_LEASE)),
        {"XRT_MODULE_FUTURE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_generator.c", D(R(CAPTURE),R(CODE_LEASE),R(FOREIGN_ERROR)), {"XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_future_payload.c", D(R(CLOSURE),R(GENERATOR)),
        {"XRT_MODULE_MEMORY", "XRT_MODULE_VALUE", "XRT_MODULE_FUTURE"}},
    {"src/runtime/xl6_code_finalizer.c", D(R(CODE_LEASE),R(FOREIGN_ERROR)), {"XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_ownership_plan.c", NULL, 0, {"XRT_MODULE_MEMORY"}},
    {"src/api/xl_protocol_registry.c", NULL, 0, {"XRT_MODULE_ONCE", "XRT_MODULE_MUTEX", "XRT_MODULE_MEMORY"}},
    {"src/api/xl_future_bridge.c", D(R(CODE_LEASE),R(FUTURE_PAYLOAD)),
        {"XRT_MODULE_FUTURE_COMBINE", "XRT_MODULE_THREAD", "XRT_MODULE_MUTEX", "XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE"}},
    {"src/api/xl_channel_bridge.c", NULL, 0, {"XRT_MODULE_CHANNEL_SELECT_CANCEL", "XRT_MODULE_CHANNEL_COROUTINE"}},
    {"src/api/xl_task_group_bridge.c", NULL, 0, {"XRT_MODULE_TASK_GROUP_POOL", "XRT_MODULE_TASK_GROUP_COROUTINE"}},
    {"src/api/xl_foreign_error.c", NULL, 0, {"XRT_MODULE_ONCE", "XRT_MODULE_ERROR"}},
    {"src/runtime/xl6_network_service.c", NULL, 0, {"XRT_MODULE_NET_ENGINE", "XRT_MODULE_NET_RESOLVER"}},
    {"src/runtime/xl6_task_file.c", D(R(TASK)), {"XRT_MODULE_FILE_ASYNC"}},
    {"src/runtime/xl6_log.c", NULL, 0, {"XRT_MODULE_LOGGER_CORE", "XRT_MODULE_STRING", "XRT_MODULE_ONCE"}},
    {"src/runtime/xl6_log_console.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_CONSOLE"}},
    {"src/runtime/xl6_log_record.c", D(R(LOG_CORE)), {"XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_log_text.c", D(R(LOG_RECORD)), {"XRT_MODULE_LOGGER_FORMAT_TEXT_BUFFER"}},
    {"src/runtime/xl6_log_json.c", D(R(LOG_RECORD)), {"XRT_MODULE_LOGGER_FORMAT_JSON_BUFFER"}},
    {"src/runtime/xl6_log_custom.c", D(R(LOG_RECORD),R(FOREIGN_ERROR)), {"XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE", "XRT_MODULE_ONCE"}},
    {"src/runtime/xl6_log_file.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_FILE"}},
    {"src/runtime/xl6_log_file_text.c", D(R(LOG_FILE),R(LOG_TEXT)), {"XRT_MODULE_LOGGER_FILE_TEXT"}},
    {"src/runtime/xl6_log_file_json.c", D(R(LOG_FILE),R(LOG_JSON)), {"XRT_MODULE_LOGGER_FILE_JSON"}},
    {"src/runtime/xl6_log_async.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_ASYNC"}},
    {"src/runtime/xl6_log_ring.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_RING"}},
    {"src/runtime/xl6_log_printf.c", D(R(LOG_CORE)), {"XRT_MODULE_LOGGER_PRINTF"}},
    {"src/runtime/xl6_log_text_writer.c", D(R(LOG_TEXT)), {"XRT_MODULE_IO"}},
    {"src/runtime/xl6_log_json_writer.c", D(R(LOG_JSON)), {"XRT_MODULE_IO"}},
    {"src/runtime/xl6_text_value.c", NULL, 0, {"XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_query.c", NULL, 0, {"XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_binary.c", NULL, 0, {"XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_text_chars.c", NULL, 0, {"XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_glob.c", NULL, 0, {"XRT_MODULE_STRING_GLOB", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_distance.c", NULL, 0, {"XRT_MODULE_UNICODE_DISTANCE", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_text_split.c", NULL, 0, {"XRT_MODULE_STRING_SPLIT", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_text_join.c", NULL, 0, {"XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_text_iterator.c", NULL, 0, {"XRT_MODULE_STRING_SPLIT", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_number_format.c", NULL, 0, {"XRT_MODULE_NUMBER_FORMAT"}},
    {"src/runtime/xl6_number_format_buffer.c", D(R(NUMBER_FORMAT)), {"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_number_integer.c", NULL, 0, {"XRT_MODULE_NUMBER_INTEGER"}},
    {"src/runtime/xl6_number_float.c", NULL, 0, {"XRT_MODULE_NUMBER_FLOAT"}},
    {"src/runtime/xl6_number_integer_buffer.c", NULL, 0, {"XRT_MODULE_NUMBER_INTEGER", "XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_number_float_buffer.c", NULL, 0, {"XRT_MODULE_NUMBER_FLOAT", "XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_unicode.c", NULL, 0, {"XRT_MODULE_UNICODE", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_unicode_validator.c", NULL, 0, {"XRT_MODULE_UNICODE", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_html.c", NULL, 0, {"XRT_MODULE_HTML_ESCAPE"}},
    {"src/runtime/xl6_encoding.c", NULL, 0, {"XRT_MODULE_CHARSET", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_encoding_meta.c", NULL, 0, {"XRT_MODULE_CHARSET"}},
    {"src/runtime/xl6_encoding_detect.c", NULL, 0, {"XRT_MODULE_CHARSET_DETECT", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_regex_core.c", NULL, 0, {"XRT_MODULE_REGEX_CORE", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_regex_match.c", D(R(REGEX_CORE)), {"XRT_MODULE_REGEX_MATCH", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE"}},
    {"src/runtime/xl6_regex_iterator.c", D(R(REGEX_MATCH)), {"XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_regex_replace.c", D(R(REGEX_CORE)), {"XRT_MODULE_REGEX_REPLACE", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_regex_replace_buffer.c", D(R(REGEX_CORE)), {"XRT_MODULE_REGEX_REPLACE", "XRT_MODULE_BUFFER", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_regex_split.c", D(R(REGEX_CORE)), {"XRT_MODULE_REGEX_SPLIT", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE", "XRT_MODULE_THREAD"}},
    {"src/runtime/xl6_regex_set.c", D(R(REGEX_CORE)), {"XRT_MODULE_REGEX_SET", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER"}},
    {"src/runtime/xl6_regex_replace_func.c", D(R(REGEX_MATCH),R(CLOSURE)), {"XRT_MODULE_REGEX_REPLACE"}},
    {"src/runtime/xl6_regex_replace_func_buffer.c", D(R(REGEX_REPLACE_FUNC)), {"XRT_MODULE_BUFFER"}},
    {"src/runtime/xl6_pattern_core.c", D(), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_pattern_options.c", D(), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_pattern_match.c", D(), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT"}},
    {"src/runtime/xl6_pattern_builder.c", D(), {"XRT_MODULE_PATTERN", "XRT_MODULE_STRING", "XRT_MODULE_UNICODE_TEXT", "XRT_MODULE_VALUE_CONTAINER", "XRT_MODULE_THREAD", "XRT_MODULE_ATOMIC"}},
    {"src/runtime/xl6_time_core.c", D(), {"XRT_MODULE_TIME"}},
    {"src/runtime/xl6_time_datetime.c", D(), {"XRT_MODULE_TIME"}},
    {"src/runtime/xl6_time_local.c", D(R(TIME_DATETIME)), {"XRT_MODULE_TIME_LOCAL"}}
};
_Static_assert(sizeof(xl6ArtifactComponents)/sizeof(xl6ArtifactComponents[0]) == XL6_ARTIFACT_RUNTIME_COUNT,
    "Every resident component needs one dependency descriptor");
#undef D
_Static_assert(XL6_ARTIFACT_RUNTIME_COUNT > 0, "Resident graph needs a population");

const XL6_ArtifactRuntimeComponent* xl6ArtifactRuntimeComponentGet(unsigned id)
{
    return id < XL6_ARTIFACT_RUNTIME_COUNT ? &xl6ArtifactComponents[id] : NULL;
}

XL6_ArtifactRuntimeId xl6ArtifactRuntimeSymbolOwner(const char* symbol)
{
    static const struct { const char* name; XL6_ArtifactRuntimeId owner; } exports[] = {
#define XL6_ARTIFACT_SYMBOL(Name, Component) {Name, R(Component)},
#include "src/compile/xl6_artifact_runtime_symbols.inc"
#undef XL6_ARTIFACT_SYMBOL
    };
    if (symbol != NULL)
        for (size_t i=0;i<sizeof(exports)/sizeof(exports[0]);++i)
            if (!strcmp(symbol,exports[i].name)) return exports[i].owner;
    return XL6_ARTIFACT_RUNTIME_COUNT;
}

const XL6_ArtifactNativeSymbol* xl6ArtifactNativeSymbolLookup(const char* symbol, size_t* count)
{
    static const XL6_ArtifactNativeSymbol exports[] = {
#define XL6_ARTIFACT_NATIVE_SYMBOL(Symbol, Module) {Symbol, Module},
#include "src/compile/xl6_artifact_native_symbols.inc"
#undef XL6_ARTIFACT_NATIVE_SYMBOL
    };
    size_t first=0, last=sizeof(exports)/sizeof(exports[0]), begin, end;
    if (!count) return NULL;
    *count=0;
    if (!symbol) return NULL;
    /* Lower bound: shared providers occupy adjacent rows in the catalog. */
    while (first<last) {
        size_t middle=first+(last-first)/2;
        if (strcmp(exports[middle].symbol,symbol)<0) first=middle+1;
        else last=middle;
    }
    begin=first;
    end=sizeof(exports)/sizeof(exports[0]);
    while (first<end && !strcmp(exports[first].symbol,symbol)) ++first;
    *count=first-begin;
    return *count ? &exports[begin] : NULL;
}

static int xl6ArtifactNativeModuleRequire(XL6_ArtifactRuntimePlan* plan, const char* module)
{
    size_t i=0;
    if (!module) return 1;
    while (i<plan->nativeModuleCount && strcmp(plan->nativeModules[i],module)) ++i;
    if (i<plan->nativeModuleCount) return 1;
    if (i==XL6_ARTIFACT_NATIVE_MODULE_LIMIT) return 0;
    plan->nativeModules[plan->nativeModuleCount++]=module;
    return 1;
}

static int xl6ArtifactVisitNativeModule(void* context, const char* module)
{
    return xl6ArtifactNativeModuleRequire(context,module);
}

static int xl6ArtifactDependencies(void* context, size_t id,
    const size_t** dependencies, size_t* count)
{
    const XL6_ArtifactRuntimeComponent* component;
    (void)context;
    if (id >= XL6_ARTIFACT_RUNTIME_COUNT) return 0;
    component = &xl6ArtifactComponents[id];
    *dependencies = component->dependencies; *count = component->dependencyCount;
    return 1;
}

int xl6ArtifactRuntimeClosePlan(const XL6_ArtifactRuntimeSet* roots, XL6_ArtifactRuntimePlan* plan)
{
    XL6_ArtifactRuntimePlan result;
    XL6_ArtifactRuntimeSet copy = {{0}};
    if (!plan) return 0;
    /* Snapshot before zeroing: roots may alias the previous output plan. */
    if (roots != NULL) copy = *roots;
    memset(plan,0,sizeof(*plan));
    if (roots == NULL || !xl6ComponentSetValid(copy.words,XL6_ARTIFACT_RUNTIME_COUNT)) return 0;
    memset(&result,0,sizeof(result));
    result.roots=copy; result.components=copy;
    if (!xl6ComponentSetClose(result.components.words,XL6_ARTIFACT_RUNTIME_COUNT,
            xl6ArtifactDependencies,NULL)) return 0;
    for (unsigned i=0;i<XL6_ARTIFACT_RUNTIME_COUNT;++i) {
        const XL6_ArtifactRuntimeComponent* item=&xl6ArtifactComponents[i];
        if (!xl6ArtifactRuntimeSetHas(&result.components,i)) continue;
        result.sources[result.sourceCount++]=item->source;
        for (size_t j=0;j<sizeof(item->nativeModules)/sizeof(item->nativeModules[0]) && item->nativeModules[j];++j) {
            size_t k=0;
            while (k<result.nativeModuleCount && strcmp(result.nativeModules[k],item->nativeModules[j])) ++k;
            if (k<result.nativeModuleCount) continue;
            if (k==XL6_ARTIFACT_NATIVE_MODULE_LIMIT) return 0;
            result.nativeModules[result.nativeModuleCount++]=item->nativeModules[j];
        }
    }
    *plan=result;
    return 1;
}

/* Each physical type is visited once, including the nested payload of a
 * nullable/product/callable. This is not a scan of unused source declarations. */
typedef struct XL6_ArtifactTypeWork {
    const XL6_IrModule* module;
    unsigned char* seen;
    XL6_IrTypeId* queue;
    size_t count;
} XL6_ArtifactTypeWork;

static void xl6ArtifactTypeRequire(XL6_ArtifactTypeWork* work, XL6_IrTypeId id)
{
    const XL6_IrType* type=xl6IrTypeGet(&work->module->types,id);
    size_t index;
    if (!type) return; /* INVALID denotes no payload, already checked by XLIR. */
    index=(size_t)(type-work->module->types.items);
    if (!work->seen[index]) { work->seen[index]=1; work->queue[work->count++]=id; }
}

static int xl6ArtifactInstructionReachable(const XL6_IrModule* module, size_t index)
{
    for (size_t i=0;i<module->functionCount;++i) {
        const XL6_IrFunction* function=&module->functions[i];
        if (index>=function->firstInstruction &&
            index-function->firstInstruction<function->instructionCount)
            return function->reachability != XL6_IR_REACHABILITY_UNREACHABLE;
    }
    return 0;
}

int xl6ArtifactRuntimePlanBuild(const XL6_IrModule* module, XL6_ArtifactEntryAbi entry,
    XL6_ArtifactRuntimePlan* plan, XL6_DiagList* diags)
{
    XL6_ArtifactTypeWork work;
    XL6_IrCgenOptions cOptions={0};
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(R(SLOT));
#define REQUIRE(...) do { const size_t ids[] = {__VA_ARGS__}; \
    if (!xl6ComponentSetAddMany(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,ids,sizeof(ids)/sizeof(ids[0]))) { \
        xrtFree(work.seen); xrtFree(work.queue); \
        xl6DiagAdd(diags,0,0,"Resident root identity is outside the component population"); return 0; \
    } } while (0)
    if (!plan) return 0;
    memset(plan,0,sizeof(*plan));
    if (entry < XL6_ARTIFACT_ENTRY_FRAGMENT || entry > XL6_ARTIFACT_ENTRY_EXECUTABLE_V1) {
        xl6DiagAdd(diags,0,0,"Artifact entry ABI is unresolved");
        return 0;
    }
    if (!module || module->stage != XL6_IR_STAGE_LOWERED) {
        xl6DiagAdd(diags,0,0,"Artifact runtime plan requires Lowered XLIR");
        return 0;
    }
    if (!xl6IrVerifyModule(module,diags)) return 0;
    memset(&work,0,sizeof(work)); work.module=module;
    if (module->types.count > SIZE_MAX/sizeof(*work.queue)) return 0;
    work.seen=xrtMalloc(module->types.count ? module->types.count : 1);
    work.queue=xrtMalloc(module->types.count ? module->types.count*sizeof(*work.queue) : 1);
    if (!work.seen || !work.queue) {
        xrtFree(work.seen); xrtFree(work.queue);
        xl6DiagAdd(diags,0,0,"Cannot allocate artifact type dependency worklist");
        return 0;
    }
    memset(work.seen,0,module->types.count);
    /* The generated checked init/run/fini boundary owns an error scope even
     * when the language body contains no fallible instruction. Its deployment
     * ABI is explicit, independent of the module's source function effects. */
    if (entry != XL6_ARTIFACT_ENTRY_FRAGMENT) REQUIRE(R(FOREIGN_ERROR));
    if (module->abi.codeOwnerAbi != XL6_IR_MODULE_CODE_OWNER_NONE) REQUIRE(R(CODE_LEASE));
    if (module->abi.continuationAbi != XL6_IR_MODULE_CONTINUATION_NONE)
        REQUIRE(R(EXECUTOR),R(CONTINUATION_FUTURE),R(CONTINUATION_FRAME));
    if (module->abi.taskPoolAbi != XL6_IR_MODULE_TASK_POOL_NONE) REQUIRE(R(TASK));
    for (size_t i=0;i<module->callTargetCount;++i)
        if (module->callTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
            xl6IrRuntimeFsBridgeUsesOwner(module->callTargets[i].runtimeBridge,"xl6_xrt_fs_async_destroy"))
            REQUIRE(R(FILE_ASYNC));
    if (module->abi.futureAggregateAbi != XL6_IR_FUTURE_AGGREGATE_NONE) REQUIRE(R(FUTURE_BRIDGE));
    if (module->abi.nativeFutureMapAbi != XL6_IR_NATIVE_FUTURE_MAP_NONE) REQUIRE(R(FUTURE_CALLBACK),R(FUTURE_PAYLOAD));
    if (module->abi.networkServiceAbi != XL6_IR_MODULE_NETWORK_SERVICE_NONE) REQUIRE(R(NETWORK_SERVICE));
    if (module->asyncMachineCount) REQUIRE(R(CONTINUATION_FRAME),R(FUTURE_PAYLOAD));
    if (module->generatorMachineCount) REQUIRE(R(GENERATOR));
    if (module->classLayoutCount) REQUIRE(R(CODE_FINALIZER));
    if (module->exportCount) REQUIRE(R(FOREIGN_ERROR));
    for (size_t i=0;i<module->storageCount;++i) {
        xl6ArtifactTypeRequire(&work,module->storages[i].type);
        xl6ArtifactTypeRequire(&work,module->storages[i].cellType);
    }
    for (size_t i=0;i<module->instructionCount;++i) {
        const XL6_IrInstruction* instruction=&module->instructions[i];
        /* Cleanup scopes are also emitted in unreachable structural bodies. */
        switch (instruction->kind) {
        case XL6_IR_INSTR_ERROR_UNWIND_BEGIN: case XL6_IR_INSTR_CLEANUP_BEGIN:
        case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN: REQUIRE(R(FOREIGN_ERROR)); break;
        default: break;
        }
        if (!xl6ArtifactInstructionReachable(module,i)) continue;
        xl6ArtifactTypeRequire(&work,instruction->type);
        xl6ArtifactTypeRequire(&work,instruction->calleeType);
        xl6ArtifactTypeRequire(&work,instruction->payloadType);
        if (instruction->kind==XL6_IR_INSTR_AWAIT) REQUIRE(R(FUTURE_PAYLOAD));
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL) continue;
        switch (instruction->runtimeOp) {
        case XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE: REQUIRE(R(FUTURE_CALLBACK),R(FUTURE_PAYLOAD)); break;
        case XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE: REQUIRE(R(TASK)); break;
        case XL6_IR_RUNTIME_FUTURE_SET_DEBUG_NAME: case XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY:
        case XL6_IR_RUNTIME_FUTURE_CREATE_TIME_MS: case XL6_IR_RUNTIME_FUTURE_COMPLETE_TIME_MS:
        case XL6_IR_RUNTIME_FUTURE_PENDING_CONTINUATIONS: REQUIRE(R(FUTURE_BRIDGE)); break;
        default: break;
        }
    }
    for (size_t i=0;i<module->callTargetCount;++i) {
        const XL6_IrCallTarget* target=&module->callTargets[i];
        if (target->reachability == XL6_IR_REACHABILITY_UNREACHABLE) continue;
        /* Exact linkage ownership is independent of source spelling/semantics. */
        XL6_ArtifactRuntimeId owner = xl6ArtifactRuntimeSymbolOwner(target->cName);
        if (owner != XL6_ARTIFACT_RUNTIME_COUNT) REQUIRE(owner);
        if (target->errorPhysicalAbi==XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE) REQUIRE(R(FOREIGN_ERROR));
        if (target->kind==XL6_IR_CALL_TARGET_INTERNAL_ASYNC) REQUIRE(R(FUTURE_PAYLOAD),R(FOREIGN_ERROR));
        xl6ArtifactTypeRequire(&work,target->returnType);
        for (size_t p=0;p<target->parameterCount;++p) xl6ArtifactTypeRequire(&work,target->parameterTypes[p]);
    }
    for (size_t i=0;i<module->classFieldCount;++i) xl6ArtifactTypeRequire(&work,module->classFields[i].type);
    for (size_t i=0;i<work.count;++i) {
        const XL6_IrType* type=xl6IrTypeGet(&module->types,work.queue[i]);
        switch (type->cAbi) {
        case XL6_IR_C_ABI_CLOSURE_PTR: REQUIRE(R(CLOSURE)); break;
        case XL6_IR_C_ABI_RUNTIME_CELL_PTR: REQUIRE(R(CELL)); break;
        case XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR: REQUIRE(R(GENERATOR)); break;
        case XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR: REQUIRE(R(VALUE_ITERATOR)); break;
        case XL6_IR_C_ABI_RUNTIME_FUTURE_PTR: REQUIRE(R(FUTURE_PAYLOAD)); break;
        default: break;
        }
        for (size_t p=0;p<type->paramCount;++p) xl6ArtifactTypeRequire(&work,type->params[p]);
        xl6ArtifactTypeRequire(&work,type->callableResultType);
    }
    xrtFree(work.seen); xrtFree(work.queue);
    if (!xl6ArtifactRuntimeClosePlan(&roots,plan)) return 0;
    {
        /* The public xlValue ABI includes graph cloning, native/callable/type
         * values and their conversions even in scalar-only artifacts. This
         * fixed deployment contract is not inferred from source or C text. */
        static const char* const baseModules[]={
            "XRT_MODULE_TEMP_MEMORY", "XRT_MODULE_THREAD_KEY", "XRT_MODULE_ONCE",
            "XRT_MODULE_VALUE_GRAPH", "XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE",
            "XRUNTIME_MODULE_RUNTIME_VALUE_TYPE", "XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE",
            "XRUNTIME_MODULE_RUNTIME_CONVERT_STRING", "XRT_MODULE_XLON_WRITE", "XRT_MODULE_TIME_TEXT"
        };
        for (size_t i=0;i<sizeof(baseModules)/sizeof(baseModules[0]);++i)
            if (!xl6ArtifactNativeModuleRequire(plan,baseModules[i])) goto native_capacity_failure;
    }
    cOptions.emitScriptEntry=entry!=XL6_ARTIFACT_ENTRY_FRAGMENT;
    cOptions.emitMainWrapper=entry==XL6_ARTIFACT_ENTRY_EXECUTABLE_V1;
    if (!xl6IrCgenVisitNativeModules(module,diags,&cOptions,xl6ArtifactVisitNativeModule,plan)) {
        memset(plan,0,sizeof(*plan));
        xl6DiagAdd(diags,0,0,"Cannot build verified C implementation dependencies");
        return 0;
    }
    for (size_t i=0;i<module->callTargetCount;++i) {
        const XL6_IrCallTarget* target=&module->callTargets[i];
        const XL6_ArtifactNativeSymbol* symbols;
        size_t count;
        if (target->kind!=XL6_IR_CALL_TARGET_EXTERN_C ||
            target->reachability==XL6_IR_REACHABILITY_UNREACHABLE) continue;
        symbols=xl6ArtifactNativeSymbolLookup(target->cName,&count);
        for (size_t j=0;j<count;++j) {
            if (!xl6ArtifactNativeModuleRequire(plan,symbols[j].module))
                goto native_capacity_failure;
        }
    }
    plan->entry=entry;
    return 1;
native_capacity_failure:
    memset(plan,0,sizeof(*plan));
    xl6DiagAdd(diags,0,0,"Artifact native module capacity exceeded");
    return 0;
}
#undef R
#undef REQUIRE

static void xl6CompileUnitBaseNameNoExt(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* p;
    const char* dot;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }

    base = path != NULL ? path : "module";
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            base = p + 1;
        }
    }

    dot = NULL;
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '.' ) {
            dot = p;
        }
    }

    len = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if ( len == 0 ) {
        base = "module";
        len = strlen(base);
    }
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
}

void xl6CompileUnitInit(XL6_CompileUnit* unit)
{
    if ( unit == NULL ) {
        return;
    }
    memset(unit, 0, sizeof(*unit));
    xl6ParseResultInit(&unit->parse);
    xl6ImportDepsInit(&unit->importDeps);
    xl6IrModuleInit(&unit->ir);
}

void xl6CompileUnitFree(XL6_CompileUnit* unit)
{
    if ( unit == NULL ) {
        return;
    }
    xl6ParseResultFree(&unit->parse);
    xl6ImportDepsFree(&unit->importDeps);
    xl6IrModuleFree(&unit->ir);
    memset(unit, 0, sizeof(*unit));
}
int xl6CompileUnitLoadFile(XL6_CompileUnit* unit, const char* path, int runSem)
{
    int ok;

    if ( unit == NULL || path == NULL ) {
        return 0;
    }

    unit->path = path;
    unit->phase = "parse";
    xl6CompileUnitBaseNameNoExt(unit->moduleName, sizeof(unit->moduleName), path);

    ok = xl6ParseFile(path, &unit->parse);
    if ( ok ) {
        unit->phase = "import";
        ok = xl6ImportsExpandModule(unit->parse.root, path, &unit->parse.diags, &unit->importDeps);
    }
    if ( ok ) {
        unit->phase = "stdlib";
        ok = xl6StdlibExpandModule(unit->parse.root, &unit->parse.diags);
    }
    if ( ok && runSem ) {
        unit->phase = "sem";
        ok = xl6SemCheckModule(unit->parse.root, &unit->parse.diags, unit->moduleName);
        unit->semChecked = 1;
    }
    if ( ok && runSem ) {
        unit->phase = "ir";
        ok = xl6IrBuildModuleFromAst(&unit->ir, unit->parse.root, &unit->parse.diags);
        unit->irReady = ok;
    }
    unit->success = ok && unit->parse.diags.count == 0;
    if ( unit->success ) {
        unit->phase = "checked";
    }
    return unit->success;
}

int xl6CompileUnitLoadSource(XL6_CompileUnit* unit, const char* source, const char* virtualPath, int runSem)
{
    const char* path;
    int ok;

    if ( unit == NULL || source == NULL ) {
        return 0;
    }

    path = virtualPath != NULL && virtualPath[0] != '\0' ? virtualPath : "<memory>";
    unit->path = path;
    unit->phase = "parse";
    xl6CompileUnitBaseNameNoExt(unit->moduleName, sizeof(unit->moduleName), path);

    ok = xl6ParseSource(source, path, &unit->parse);
    if ( ok ) {
        unit->phase = "import";
        ok = xl6ImportsExpandModule(unit->parse.root, path, &unit->parse.diags, &unit->importDeps);
    }
    if ( ok ) {
        unit->phase = "stdlib";
        ok = xl6StdlibExpandModule(unit->parse.root, &unit->parse.diags);
    }
    if ( ok && runSem ) {
        unit->phase = "sem";
        ok = xl6SemCheckModule(unit->parse.root, &unit->parse.diags, unit->moduleName);
        unit->semChecked = 1;
    }
    if ( ok && runSem ) {
        unit->phase = "ir";
        ok = xl6IrBuildModuleFromAst(&unit->ir, unit->parse.root, &unit->parse.diags);
        unit->irReady = ok;
    }
    unit->success = ok && unit->parse.diags.count == 0;
    if ( unit->success ) {
        unit->phase = "checked";
    }
    return unit->success;
}
