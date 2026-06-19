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
    XL6_CGEN_CLEANUP_XVALUE,
    XL6_CGEN_CLEANUP_TYPED_CONTAINER,
    XL6_CGEN_CLEANUP_AVLTB_ITER
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
    XL6_CgenCleanup* cleanups;
    size_t cleanupCount;
    size_t cleanupCapacity;
    XL6_CgenLoopTarget* loopTargets;
    size_t loopTargetCount;
    size_t loopTargetCapacity;
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
    int needsXValueRuntime;
    int needsFunctionVarRuntime;
    int ok;
    int indent;
    int tempId;
} XL6_CgenContext;

static int xl6CgenParamRequiredCount(const XL6_AstNode* params);

static int xl6CTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

