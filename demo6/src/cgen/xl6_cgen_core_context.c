typedef struct XL6_CgenVar {
    char* name;
    const XL6_AstNode* type;
    unsigned int flags;
} XL6_CgenVar;

typedef struct XL6_CgenRootTypeSlot {
    const char* name;
    XL6_AstNode* classNode;
    XL6_AstNode* structNode;
    XL6_AstNode* unionNode;
} XL6_CgenRootTypeSlot;

#define XL6_CGEN_VAR_TYPED_CONTAINER 0x00000001u
#define XL6_CGEN_VAR_BORROWED 0x00000002u
#define XL6_CGEN_VAR_MODULE_GLOBAL 0x00000004u
#define XL6_CGEN_VAR_BYREF 0x00000008u
#define XL6_CGEN_VAR_OUT 0x00000010u

typedef enum XL6_CgenCleanupKind {
    XL6_CGEN_CLEANUP_NATIVE_CLASS,
    XL6_CGEN_CLEANUP_CLASS,
    XL6_CGEN_CLEANUP_FUNCTION,
    XL6_CGEN_CLEANUP_FUTURE,
    XL6_CGEN_CLEANUP_CHANNEL,
    XL6_CGEN_CLEANUP_THREAD,
    XL6_CGEN_CLEANUP_GENERATOR,
    XL6_CGEN_CLEANUP_SYNC,
    XL6_CGEN_CLEANUP_XVALUE,
    XL6_CGEN_CLEANUP_TYPED_CONTAINER,
    XL6_CGEN_CLEANUP_ERROR,
    XL6_CGEN_CLEANUP_ITER_SNAPSHOT,
    XL6_CGEN_CLEANUP_STRING,
    XL6_CGEN_CLEANUP_XRT_PTR,
    XL6_CGEN_CLEANUP_TYPED_CONTAINER_PARAM,
    XL6_CGEN_CLEANUP_TEMP_SCOPE
} XL6_CgenCleanupKind;

typedef struct XL6_CgenCleanup {
    char* name;
    XL6_CgenCleanupKind kind;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* classTypeNode;
} XL6_CgenCleanup;

typedef struct XL6_CgenGenericInstance {
    const XL6_AstNode* func;
    const XL6_AstNode* typeArgs;
    int ownsTypeArgs;
} XL6_CgenGenericInstance;

typedef struct XL6_CgenGenericClassInstance {
    const XL6_AstNode* classNode;
    const XL6_AstNode* typeNode;
} XL6_CgenGenericClassInstance;

typedef struct XL6_CgenRuntimeTypeDesc {
    const XL6_AstNode* typeNode;
} XL6_CgenRuntimeTypeDesc;

typedef struct XL6_CgenAnonCapture {
    char* name;
    const XL6_AstNode* type;
} XL6_CgenAnonCapture;

typedef struct XL6_CgenAnonFunc {
    const XL6_AstNode* node;
    int id;
    XL6_CgenAnonCapture* captures;
    size_t captureCount;
    size_t captureCapacity;
} XL6_CgenAnonFunc;

typedef struct XL6_CgenLoopTarget {
    size_t breakCleanupMark;
    size_t continueCleanupMark;
    int breakId;
    int continueId;
} XL6_CgenLoopTarget;

typedef struct XL6_CgenTryTarget {
    size_t cleanupMark;
    int id;
    int targetFinally;
} XL6_CgenTryTarget;

/*
    每个词法块只生成一份异常清理代码。
    块内错误点跳到这里，再按 try 边界逐层清理，避免重复展开全部局部变量。
*/
typedef struct XL6_CgenErrorCheckpoint {
    size_t cleanupCount;
    int id;
} XL6_CgenErrorCheckpoint;

typedef struct XL6_CgenErrorScope {
    size_t cleanupMark;
    XL6_CgenTryTarget target;
    XL6_CgenErrorCheckpoint* checkpoints;
    size_t checkpointCount;
    size_t checkpointCapacity;
    int hasTarget;
} XL6_CgenErrorScope;

typedef struct XL6_CgenFinallyTarget {
    size_t cleanupMark;
    size_t loopTargetCount;
    int id;
    int inFinally;
} XL6_CgenFinallyTarget;

typedef struct XL6_CgenExprReplacement {
    const XL6_AstNode* expr;
    const char* cName;
    unsigned int flags;
} XL6_CgenExprReplacement;

#define XL6_CGEN_EXPR_REPLACEMENT_XVALUE 0x1u

/*
    类型推断会被参数匹配、容器 lowering 和表达式生成反复调用。
    缓存必须记录完整生成上下文，不能只使用 AST 地址，否则泛型实例之间会串型。
*/
typedef struct XL6_CgenTypeCacheEntry {
    const XL6_AstNode* expr;
    const XL6_AstNode* genericParams;
    const XL6_AstNode* genericArgs;
    const XL6_AstNode* currentClass;
    const XL6_AstNode* currentNativeClass;
    const XL6_CgenAnonFunc* currentAnonFunc;
    unsigned long long scopeEpoch;
    XL6_AstNode* type;
} XL6_CgenTypeCacheEntry;

typedef struct XL6_CgenContext {
    XL6_Output* out;
    XL6_AstNode* root;
    XL6_DiagList* diags;
    XL6_CgenRootTypeSlot* rootTypeSlots;
    size_t rootTypeSlotCount;
    XL6_CgenVar* vars;
    size_t varCount;
    size_t varCapacity;
    char** emittedClasses;
    size_t emittedClassCount;
    size_t emittedClassCapacity;
    char** emittedUnions;
    size_t emittedUnionCount;
    size_t emittedUnionCapacity;
    char** emittedStructs;
    size_t emittedStructCount;
    size_t emittedStructCapacity;
    XL6_CgenCleanup* cleanups;
    size_t cleanupCount;
    size_t cleanupCapacity;
    XL6_CgenErrorScope* errorScopes;
    size_t errorScopeCount;
    size_t errorScopeCapacity;
    XL6_CgenLoopTarget* loopTargets;
    size_t loopTargetCount;
    size_t loopTargetCapacity;
    XL6_CgenTryTarget* tryTargets;
    size_t tryTargetCount;
    size_t tryTargetCapacity;
    XL6_CgenFinallyTarget* finallyTargets;
    size_t finallyTargetCount;
    size_t finallyTargetCapacity;
    XL6_CgenGenericInstance* genericInstances;
    size_t genericInstanceCount;
    size_t genericInstanceCapacity;
    XL6_CgenGenericClassInstance* genericClassInstances;
    size_t genericClassInstanceCount;
    size_t genericClassInstanceCapacity;
    XL6_CgenRuntimeTypeDesc* runtimeTypeDescs;
    size_t runtimeTypeDescCount;
    size_t runtimeTypeDescCapacity;
    XL6_CgenAnonFunc* anonFuncs;
    size_t anonFuncCount;
    size_t anonFuncCapacity;
    XL6_AstNode** ownedTypeNodes;
    size_t ownedTypeNodeCount;
    size_t ownedTypeNodeCapacity;
    unsigned long long* emittedFunctionReturns;
    size_t emittedFunctionReturnCount;
    size_t emittedFunctionReturnCapacity;
    const XL6_AstNode* currentGenericParams;
    const XL6_AstNode* currentGenericArgs;
    const XL6_AstNode* currentReturnType;
    const XL6_AstNode* currentReturnParams;
    const XL6_AstNode* currentReturns;
    const XL6_AstNode* currentReturnFunc;
    const XL6_AstNode* currentClass;
    const XL6_AstNode* currentNativeClass;
    const XL6_CgenAnonFunc* currentAnonFunc;
    const XL6_AstNode* currentGeneratorItemType;
    const char* currentGeneratorHandleName;
    const char* currentCatchErrorName;
    const char* currentErrorExitLabel;
    const XL6_CgenExprReplacement* exprReplacements;
    size_t exprReplacementCount;
    XL6_CgenTypeCacheEntry* typeCache;
    size_t typeCacheCount;
    size_t typeCacheCapacity;
    unsigned long long typeScopeEpoch;
    int currentAsyncWorker;
    int debugMode;
    const char* debugSourcePath;
    int needsXValueRuntime;
    int needsFunctionVarRuntime;
    int needsErrorRuntime;
    int wrapExplicitMainWithTopLevel;
    int ok;
    int indent;
    int tempId;
} XL6_CgenContext;

static int xl6CgenParamRequiredCount(const XL6_AstNode* params);
static int xl6CgenIsAsyncFunc(const XL6_AstNode* node);
static int xl6CgenIsFutureLikeType(const XL6_AstNode* typeNode);
static int xl6CgenIsChannelType(const XL6_AstNode* typeNode);
static int xl6CgenIsThreadType(const XL6_AstNode* typeNode);
static int xl6CgenIsGeneratorType(const XL6_AstNode* typeNode);
static int xl6CgenIsSyncHandleType(const XL6_AstNode* typeNode);
static int xl6CgenNodeNeedsAsyncRuntime(const XL6_AstNode* node);
static int xl6CgenNodeNeedsTaskSpawn(const XL6_AstNode* node);
static void xl6CgenEmitAsyncRuntime(XL6_Output* out, int hasTaskSpawn);
static void xl6CgenEmitGeneratorRuntime(XL6_Output* out);
static void xl6CgenEmitDebugRuntime(XL6_Output* out);
static void xl6CgenDebugProbe(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenEmitXrtTypeDescRef(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static int xl6CgenEmitClassCtorInto(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* targetExpr);
static int xl6CgenClassNeedsDestroy(XL6_CgenContext* ctx, const XL6_AstNode* classNode);
static void xl6CgenBeginTempScope(XL6_CgenContext* ctx, const XL6_AstNode* atNode);
static int xl6CgenParamTypeUsesTypedContainerStorage(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static int xl6CgenClassHasLocalVirtualSlots(const XL6_AstNode* classNode);
static int xl6CgenClassBasePath(XL6_CgenContext* ctx, const XL6_AstNode* fromClass, const XL6_AstNode* targetClass, char* buffer, size_t capacity);
static void xl6CgenEmitRecordFieldHelpers(XL6_CgenContext* ctx);
static void xl6CgenAsyncFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenAsyncFunc(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenAsyncReturnStmt(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenAwaitExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static int xl6CgenChannelCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee);
static int xl6CgenChannelMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member);
static int xl6CgenFutureMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member);
static int xl6CgenTaskNamespaceCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member);
static int xl6CgenThreadCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee);
static int xl6CgenThreadMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member);
static int xl6CgenFuncReturnsGenerator(XL6_CgenContext* ctx, const XL6_AstNode* node);
static int xl6CgenFuncIsAsyncGenerator(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenGeneratorFunc(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenYieldStmt(XL6_CgenContext* ctx, const XL6_AstNode* node);
static XL6_AstNode* xl6CgenCloneGeneratorItemType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static void xl6CgenAddGeneratorCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode);
static void xl6CgenEmitExceptionPropagate(XL6_CgenContext* ctx, const XL6_AstNode* at);
static void xl6CgenEmitDefaultErrorReturn(XL6_CgenContext* ctx);
static void xl6CgenCloseScope(XL6_CgenContext* ctx, size_t mark);
static void xl6CgenXValueFromExprForTarget(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr);
static int xl6CgenSyncCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee);
static int xl6CgenSyncDefaultCtorExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, int line, int column);
static int xl6CgenSyncMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member);
static void xl6CgenTypedParamOwnerName(char* buffer, size_t capacity, const char* name);
static void xl6CgenAddTypedContainerParamCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* typeNode);
static void xl6CgenTypedContainerDestroyValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* cName);
static void xl6CgenTypedContainerCopyExprMode(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr,
    const char* mode);

static int xl6CTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}
