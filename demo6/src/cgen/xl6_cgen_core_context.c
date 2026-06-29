typedef struct XL6_CgenVar {
    char* name;
    const XL6_AstNode* type;
    unsigned int flags;
} XL6_CgenVar;

#define XL6_CGEN_VAR_TYPED_CONTAINER 0x00000001u

typedef enum XL6_CgenCleanupKind {
    XL6_CGEN_CLEANUP_CUSTOM,
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
    XL6_CGEN_CLEANUP_XRT_PTR
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

typedef struct XL6_CgenExprReplacement {
    const XL6_AstNode* expr;
    const char* cName;
} XL6_CgenExprReplacement;

typedef struct XL6_CgenContext {
    XL6_Output* out;
    XL6_AstNode* root;
    XL6_DiagList* diags;
    XL6_CgenVar* vars;
    size_t varCount;
    size_t varCapacity;
    char** emittedClasses;
    size_t emittedClassCount;
    size_t emittedClassCapacity;
    char** emittedUnions;
    size_t emittedUnionCount;
    size_t emittedUnionCapacity;
    XL6_CgenCleanup* cleanups;
    size_t cleanupCount;
    size_t cleanupCapacity;
    XL6_CgenLoopTarget* loopTargets;
    size_t loopTargetCount;
    size_t loopTargetCapacity;
    XL6_CgenTryTarget* tryTargets;
    size_t tryTargetCount;
    size_t tryTargetCapacity;
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
    const XL6_AstNode* currentCustom;
    const XL6_CgenAnonFunc* currentAnonFunc;
    const XL6_AstNode* currentGeneratorItemType;
    const char* currentGeneratorHandleName;
    const char* currentCatchErrorName;
    const char* currentErrorExitLabel;
    const XL6_CgenExprReplacement* exprReplacements;
    size_t exprReplacementCount;
    int currentAsyncWorker;
    int debugMode;
    const char* debugSourcePath;
    int needsXValueRuntime;
    int needsFunctionVarRuntime;
    int needsErrorRuntime;
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
static void xl6CgenAsyncFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenAsyncFunc(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenAsyncReturnStmt(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenAwaitExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static int xl6CgenChannelCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee);
static int xl6CgenChannelMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member);
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
static int xl6CgenSyncCtorCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* callee);
static int xl6CgenSyncMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member);

static int xl6CTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}
