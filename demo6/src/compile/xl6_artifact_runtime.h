#ifndef XL6_ARTIFACT_RUNTIME_H
#define XL6_ARTIFACT_RUNTIME_H
#include "src/ir/xl6_ir.h"

/* Artifact linkage identities, not source-language semantics. Lowered XLIR
 * supplies the physical ABI roots; this layer closes the resident libraries
 * and their native module requirements before an output compiler is created. */
typedef enum XL6_ArtifactRuntimeId {
    XL6_ARTIFACT_SLOT, XL6_ARTIFACT_CODE_LEASE, XL6_ARTIFACT_CLOSURE,
    XL6_ARTIFACT_CELL, XL6_ARTIFACT_FRAME_STORAGE, XL6_ARTIFACT_TASK,
    XL6_ARTIFACT_QUEUE, XL6_ARTIFACT_EXECUTOR, XL6_ARTIFACT_CONTINUATION_FUTURE,
    XL6_ARTIFACT_CONTINUATION_FRAME, XL6_ARTIFACT_FUTURE_WAIT,
    XL6_ARTIFACT_CAPTURE, XL6_ARTIFACT_VALUE_ITERATOR, XL6_ARTIFACT_FUTURE_CALLBACK,
    XL6_ARTIFACT_GENERATOR, XL6_ARTIFACT_FUTURE_PAYLOAD,
    XL6_ARTIFACT_CODE_FINALIZER, XL6_ARTIFACT_OWNERSHIP_PLAN,
    XL6_ARTIFACT_PROTOCOL, XL6_ARTIFACT_FUTURE_BRIDGE, XL6_ARTIFACT_CHANNEL,
    XL6_ARTIFACT_TASK_GROUP, XL6_ARTIFACT_FOREIGN_ERROR, XL6_ARTIFACT_NETWORK_SERVICE,
    XL6_ARTIFACT_FILE_ASYNC,
    XL6_ARTIFACT_LOG_CORE,
    XL6_ARTIFACT_LOG_CONSOLE,
    XL6_ARTIFACT_LOG_RECORD,
    XL6_ARTIFACT_LOG_TEXT,
    XL6_ARTIFACT_LOG_JSON,
    XL6_ARTIFACT_LOG_CUSTOM,
    XL6_ARTIFACT_LOG_FILE,
    XL6_ARTIFACT_LOG_FILE_TEXT,
    XL6_ARTIFACT_LOG_FILE_JSON,
    XL6_ARTIFACT_LOG_ASYNC,
    XL6_ARTIFACT_LOG_RING,
    XL6_ARTIFACT_LOG_PRINTF,
    XL6_ARTIFACT_LOG_TEXT_WRITER,
    XL6_ARTIFACT_LOG_JSON_WRITER,
    XL6_ARTIFACT_TEXT_VALUE,
    XL6_ARTIFACT_TEXT_QUERY,
    XL6_ARTIFACT_TEXT_BINARY,
    XL6_ARTIFACT_TEXT_CHARS,
    XL6_ARTIFACT_TEXT_GLOB,
    XL6_ARTIFACT_TEXT_DISTANCE,
    XL6_ARTIFACT_TEXT_SPLIT,
    XL6_ARTIFACT_TEXT_JOIN,
    XL6_ARTIFACT_TEXT_ITERATOR,
    XL6_ARTIFACT_NUMBER_FORMAT,
    XL6_ARTIFACT_NUMBER_FORMAT_BUFFER,
    XL6_ARTIFACT_NUMBER_INTEGER,
    XL6_ARTIFACT_NUMBER_FLOAT,
    XL6_ARTIFACT_NUMBER_INTEGER_BUFFER,
    XL6_ARTIFACT_NUMBER_FLOAT_BUFFER,
    XL6_ARTIFACT_UNICODE,
    XL6_ARTIFACT_UNICODE_VALIDATOR,
    XL6_ARTIFACT_HTML,
    XL6_ARTIFACT_ENCODING,
    XL6_ARTIFACT_ENCODING_META,
    XL6_ARTIFACT_RUNTIME_COUNT
} XL6_ArtifactRuntimeId;
#define XL6_ARTIFACT_BIT(Name) ((uint64)1u << XL6_ARTIFACT_##Name)
#define XL6_ARTIFACT_NATIVE_MODULE_LIMIT 512

typedef struct XL6_ArtifactRuntimeComponent {
    const char* source;
    uint64 dependencies;
    const char* nativeModules[8];
} XL6_ArtifactRuntimeComponent;

typedef struct XL6_ArtifactNativeSymbol {
    const char* symbol;
    const char* module; /* NULL: unconditional native core, not an unknown API. */
} XL6_ArtifactNativeSymbol;

typedef enum XL6_ArtifactEntryAbi {
    XL6_ARTIFACT_ENTRY_FRAGMENT = 0,
    XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,
    XL6_ARTIFACT_ENTRY_EXECUTABLE_V1
} XL6_ArtifactEntryAbi;

typedef struct XL6_ArtifactRuntimePlan {
    XL6_ArtifactEntryAbi entry;
    uint64 roots;
    uint64 components;
    size_t sourceCount;
    const char* sources[XL6_ARTIFACT_RUNTIME_COUNT];
    size_t nativeModuleCount;
    const char* nativeModules[XL6_ARTIFACT_NATIVE_MODULE_LIMIT];
} XL6_ArtifactRuntimePlan;

const XL6_ArtifactRuntimeComponent* xl6ArtifactRuntimeComponentGet(unsigned id);
uint64 xl6ArtifactRuntimeSymbolOwner(const char* symbol);
/* Exact C linkage lookup. A shared implementation may have several providers. */
const XL6_ArtifactNativeSymbol* xl6ArtifactNativeSymbolLookup(const char* symbol, size_t* count);
int xl6ArtifactRuntimeClosePlan(uint64 roots, XL6_ArtifactRuntimePlan* plan);
/* Refuses Typed/corrupt IR. Failure leaves a zero plan, never a partial set. */
int xl6ArtifactRuntimePlanBuild(const XL6_IrModule* module, XL6_ArtifactEntryAbi entry,
    XL6_ArtifactRuntimePlan* plan, XL6_DiagList* diags);
#endif
