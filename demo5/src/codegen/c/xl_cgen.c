#include "xl_cgen.h"

typedef struct XL_CgenBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
	int iIndent;
} XL_CgenBuffer;

typedef struct XL_CgenLoopFrame {
	char sBreakLabel[64];
	char sContinueLabel[64];
} XL_CgenLoopFrame;

typedef struct XL_CgenTryFrame {
	char sExitLabel[64];
} XL_CgenTryFrame;

typedef struct XL_CgenFinallyFrame {
	char sFinallyLabel[64];
	char sDispatchLabel[64];
	int bInFinally;
	int iTryDepthBase;
} XL_CgenFinallyFrame;

typedef struct XL_CgenContext {
	XL_CgenBuffer buf;
	const XL_MirNode* pRoot;
	const XL_LowerResult* pLowerResult;
	const XL_ModuleLinkResult* pModules;
	const struct XL_CgenFuncInfo* pCurrentFuncInfo;
	int iGeneratorExprResumeState;
	XL_CgenLoopFrame arrLoopStack[64];
	int iLoopDepth;
	XL_CgenTryFrame arrTryStack[64];
	int iTryDepth;
	XL_CgenFinallyFrame arrFinallyStack[64];
	int iFinallyDepth;
} XL_CgenContext;

typedef struct XL_CgenFuncInfo {
	const char* sName;
	const char* sSymbolName;
	const char* sTypeName;
	const char* sReturnCTypeName;
	const char* sReturnDefaultValue;
	int bAsync;
	int bGenerator;
	const char* sGeneratorTypeName;
	const char* sGeneratorNextFuncName;
	const char* sGeneratorSelfName;
	const char* sGeneratorValueOutName;
	const char* sGeneratorVarName;
	const char* sGeneratorDoneLabel;
	const char* sEscapeKindName;
	const char* sEscapeLevelName;
	const char* sEscapeValueName;
	const XL_AsyncSmFunc* pAsyncFunc;
	const XL_GeneratorSmFunc* pGeneratorFunc;
	const XL_HybridSmFunc* pHybridFunc;
	int* pGeneratorSuspendIndex;
} XL_CgenFuncInfo;

typedef struct XL_CgenGeneratorField {
	const char* sName;
	const char* sTypeName;
} XL_CgenGeneratorField;

typedef struct XL_CgenGeneratorTryFrameField {
	char sName[64];
} XL_CgenGeneratorTryFrameField;

typedef struct XL_CgenAsyncLinearResumeStep {
	const XL_MirNode* pAwaitBind;
	const XL_MirNode* pAwaitStmt;
	const XL_MirNode* pAwaitInner;
	const XL_MirNode* pAwaitNode;
	const XL_MirNode* pAwaitInnerAlt;
	const XL_MirNode* pAwaitNodeAlt;
	const XL_MirNode* arrPostBinds[64];
	size_t iPostBindCount;
	const XL_MirNode* arrPostStmts[64];
	size_t iPostStmtCount;
	size_t iAwaitStmtIndex;
	int bExprAwait;
	int bReturnExprAwait;
} XL_CgenAsyncLinearResumeStep;

typedef struct XL_CgenAsyncLinearResumeCandidate {
	const XL_MirNode* pReturn;
	const XL_MirNode* arrPreBinds[64];
	XL_CgenAsyncLinearResumeStep arrSteps[16];
	size_t iPreBindCount;
	size_t iStepCount;
} XL_CgenAsyncLinearResumeCandidate;

typedef enum XL_TaskHelperKind {
	XL_TASK_HELPER_NONE = 0,
	XL_TASK_HELPER_SPAWN,
	XL_TASK_HELPER_ALL,
	XL_TASK_HELPER_RACE,
	XL_TASK_HELPER_CANCEL,
	XL_TASK_HELPER_DELAY
} XL_TaskHelperKind;

static const char* __xlInferExprTypeName(const XL_CgenContext* pCtx, const XL_MirNode* pNode);
static int __xlIsTextLikeType(const char* sTypeName);
static int __xlCallHasExpandedMultiReturnArg(const XL_MirNode* pCall);
static const XL_MirNode* __xlContainerMethodBase(const XL_MirNode* pCallNode);
static const char* __xlContainerMethodName(const XL_MirNode* pCallNode);
static int __xlCanAsyncLinearResolveStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt
);
static int __xlCanAsyncLinearResolveStmt(XL_CgenContext* pCtx, const XL_MirNode* pStmt, const XL_MirNode* pAwaitNode);
static int __xlDiscoverAsyncLinearDualAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode** ppPrimaryAwaitNode,
	const XL_MirNode** ppAltAwaitNode,
	int* pbPrimaryOnTrue
);
static int __xlAnalyzeAsyncLinearDualAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	int* pbPrimaryOnTrue
);
static int __xlFindAsyncLinearBlockAwaitStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
);
static int __xlFindAsyncLinearBlockAwaitStmt(
	XL_CgenContext* pCtx,
	const XL_MirNode* pBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
);
static int __xlMirTryConstBoolValue(const XL_MirNode* pNode, int* pbValue);
static const XL_MirNode* __xlSelectAsyncLinearBranchDualAwaitBlock(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode** ppPrimaryAwaitNode,
	const XL_MirNode** ppAltAwaitNode,
	int* pbPrimaryOnTrue
);
static const XL_MirNode* __xlSelectAsyncLinearBranchAwaitBlock(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode
);
static int __xlAnalyzeAsyncLinearDynamicBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppAwaitBlock,
	const XL_MirNode** ppSyncBlock,
	int* pbAwaitOnTrue
);
static int __xlAnalyzeAsyncLinearDualBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode** ppThenBlock,
	const XL_MirNode** ppElseBlock,
	const XL_MirNode** ppThenAwaitNode,
	const XL_MirNode** ppElseAwaitNode
);
static int __xlAnalyzeAsyncLinearDynamicAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	int* pbAwaitOnTrue
);
static int __xlFindAsyncLinearTryAwaitStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pTryBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
);
static int __xlFindAsyncLinearTryAwaitStmt(
	XL_CgenContext* pCtx,
	const XL_MirNode* pTryBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
);
static int __xlCanAsyncLinearResolveTryFinallyStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	const XL_MirNode** ppFinallyBlock,
	size_t* piInnerStmtIndex
);
static int __xlCanAsyncLinearResolveTryCatchStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	const XL_MirNode** ppCatchName,
	const XL_MirNode** ppCatchBlock,
	const XL_MirNode** ppFinallyBlock,
	size_t* piInnerStmtIndex
);
static void __xlCgenAsyncLinearStateCleanup(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	const char* sStateName
);
static void __xlCgenAsyncLinearStmtPrelude(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	int bResumePath,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	size_t iStepIndex,
	const char* sStateName
);
static void __xlCgenAsyncLinearBranchSelectorFieldName(char* sOut, size_t iOutSize, size_t iStepIndex);
static void __xlCgenAsyncLinearBranchSelectorLocalName(char* sOut, size_t iOutSize, size_t iStepIndex);
static int __xlCgenAsyncLinearEmitDynamicBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const char* sAwaitExprType,
	const char* sFutureValueName,
	size_t iStepIndex,
	const char* sStateName,
	int bResumePath
);
static int __xlCgenAsyncLinearEmitDualBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pPrimaryAwaitNode,
	const XL_MirNode* pAltAwaitNode,
	const char* sAwaitExprType,
	const char* sFutureValueName,
	size_t iStepIndex,
	const char* sStateName,
	int bResumePath
);
static int __xlCgenAsyncLinearEmitDynamicAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const char* sAwaitExprType,
	const char* sFutureValueName,
	size_t iStepIndex,
	const char* sStateName,
	int bResumePath
);
static void __xlCgenAsyncLinearTryHandleFailureFromResult(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const char* sResultName
);
static int __xlAnalyzeAsyncLinearStepBranchSelector(
	XL_CgenContext* pCtx,
	const XL_CgenAsyncLinearResumeStep* pStep,
	int* pbPrimaryOnTrue,
	int* pbDualBranch
);

typedef enum XL_CorePrintHelperKind {
	XL_CORE_PRINT_NONE = 0,
	XL_CORE_PRINT,
	XL_CORE_PRINTLN
} XL_CorePrintHelperKind;

typedef enum XL_IoHelperKind {
	XL_IO_HELPER_NONE = 0,
	XL_IO_HELPER_EPRINT,
	XL_IO_HELPER_EPRINTLN,
	XL_IO_HELPER_READ_LINE,
	XL_IO_HELPER_FLUSH
} XL_IoHelperKind;

typedef enum XL_CoreLenHelperKind {
	XL_CORE_LEN_NONE = 0,
	XL_CORE_LEN
} XL_CoreLenHelperKind;

typedef enum XL_TimeHelperKind {
	XL_TIME_HELPER_NONE = 0,
	XL_TIME_HELPER_NOW,
	XL_TIME_HELPER_TO_STRING,
	XL_TIME_HELPER_NOW_TEXT,
	XL_TIME_HELPER_DATE_TEXT,
	XL_TIME_HELPER_TIME_TEXT,
	XL_TIME_HELPER_RELATIVE,
	XL_TIME_HELPER_PARSE,
	XL_TIME_HELPER_FORMAT,
	XL_TIME_HELPER_PARSE_FORMAT,
	XL_TIME_HELPER_SLEEP,
	XL_TIME_HELPER_IS_LEAP_YEAR
} XL_TimeHelperKind;

typedef enum XL_PathHelperKind {
	XL_PATH_HELPER_NONE = 0,
	XL_PATH_HELPER_JOIN,
	XL_PATH_HELPER_NAME_EXT,
	XL_PATH_HELPER_NAME,
	XL_PATH_HELPER_EXT,
	XL_PATH_HELPER_DIR,
	XL_PATH_HELPER_EXISTS,
	XL_PATH_HELPER_IS_ABS,
	XL_PATH_HELPER_RANDOM
} XL_PathHelperKind;

typedef enum XL_FsHelperKind {
	XL_FS_HELPER_NONE = 0,
	XL_FS_HELPER_EXISTS,
	XL_FS_HELPER_IS_FILE,
	XL_FS_HELPER_IS_DIR,
	XL_FS_HELPER_SIZE,
	XL_FS_HELPER_SET_SIZE,
	XL_FS_HELPER_ACCESS_TIME,
	XL_FS_HELPER_CHANGE_TIME,
	XL_FS_HELPER_READ_ALL,
	XL_FS_HELPER_WRITE_ALL,
	XL_FS_HELPER_APPEND_ALL,
	XL_FS_HELPER_MKDIR_ALL,
	XL_FS_HELPER_COPY,
	XL_FS_HELPER_MOVE,
	XL_FS_HELPER_DELETE
} XL_FsHelperKind;

typedef enum XL_StrHelperKind {
	XL_STR_HELPER_NONE = 0,
	XL_STR_HELPER_EMPTY,
	XL_STR_HELPER_BYTE_LEN,
	XL_STR_HELPER_CONTAINS,
	XL_STR_HELPER_INDEX_OF,
	XL_STR_HELPER_LAST_INDEX_OF,
	XL_STR_HELPER_SLICE,
	XL_STR_HELPER_REPEAT,
	XL_STR_HELPER_SPLIT,
	XL_STR_HELPER_SPLIT_LINES,
	XL_STR_HELPER_STARTS_WITH,
	XL_STR_HELPER_ENDS_WITH,
	XL_STR_HELPER_TRIM,
	XL_STR_HELPER_TRIM_LEFT,
	XL_STR_HELPER_TRIM_RIGHT,
	XL_STR_HELPER_LOWER,
	XL_STR_HELPER_UPPER,
	XL_STR_HELPER_REPLACE,
	XL_STR_HELPER_REVERSE,
	XL_STR_HELPER_PAD_LEFT,
	XL_STR_HELPER_PAD_RIGHT
} XL_StrHelperKind;

static XL_StrHelperKind __xlStrHelperKind(const char* sCalleeName);

typedef enum XL_RandHelperKind {
	XL_RAND_HELPER_NONE = 0,
	XL_RAND_HELPER_INT32,
	XL_RAND_HELPER_BOOL,
	XL_RAND_HELPER_RANGE,
	XL_RAND_HELPER_TEXT,
	XL_RAND_HELPER_TEXT_FROM
} XL_RandHelperKind;

typedef enum XL_HostHelperKind {
	XL_HOST_HELPER_NONE = 0,
	XL_HOST_HELPER_NAME,
	XL_HOST_HELPER_IP,
	XL_HOST_HELPER_USER,
	XL_HOST_HELPER_OS,
	XL_HOST_HELPER_ARCH,
	XL_HOST_HELPER_EXECUTABLE
} XL_HostHelperKind;

typedef enum XL_FmtHelperKind {
	XL_FMT_HELPER_NONE = 0,
	XL_FMT_HELPER_INT_TEXT,
	XL_FMT_HELPER_FLOAT_TEXT,
	XL_FMT_HELPER_BOOL_TEXT
} XL_FmtHelperKind;

typedef enum XL_DebugHelperKind {
	XL_DEBUG_HELPER_NONE = 0,
	XL_DEBUG_HELPER_TEXT,
	XL_DEBUG_HELPER_PRINT,
	XL_DEBUG_HELPER_PRINTLN
} XL_DebugHelperKind;

typedef enum XL_LogHelperKind {
	XL_LOG_HELPER_NONE = 0,
	XL_LOG_HELPER_DEBUG,
	XL_LOG_HELPER_INFO,
	XL_LOG_HELPER_WARN,
	XL_LOG_HELPER_ERROR
} XL_LogHelperKind;

typedef enum XL_TraceHelperKind {
	XL_TRACE_HELPER_NONE = 0,
	XL_TRACE_HELPER_WRITE,
	XL_TRACE_HELPER_ERROR
} XL_TraceHelperKind;

typedef enum XL_RegexHelperKind {
	XL_REGEX_HELPER_NONE = 0,
	XL_REGEX_HELPER_IS_MATCH,
	XL_REGEX_HELPER_FIND,
	XL_REGEX_HELPER_CAPTURES,
	XL_REGEX_HELPER_CAPTURE_COUNT,
	XL_REGEX_HELPER_REPLACE_FIRST,
	XL_REGEX_HELPER_REPLACE_ALL
} XL_RegexHelperKind;

typedef enum XL_CryptoHelperKind {
	XL_CRYPTO_HELPER_NONE = 0,
	XL_CRYPTO_HELPER_HMAC_SHA256,
	XL_CRYPTO_HELPER_HMAC_SHA512
} XL_CryptoHelperKind;

typedef enum XL_EncodeHelperKind {
	XL_ENCODE_HELPER_NONE = 0,
	XL_ENCODE_HELPER_HEX_ENCODE,
	XL_ENCODE_HELPER_HEX_DECODE,
	XL_ENCODE_HELPER_BASE64_ENCODE,
	XL_ENCODE_HELPER_BASE64_DECODE
} XL_EncodeHelperKind;

typedef enum XL_HashHelperKind {
	XL_HASH_HELPER_NONE = 0,
	XL_HASH_HELPER_HASH32,
	XL_HASH_HELPER_HASH64,
	XL_HASH_HELPER_HASH64_MICRO,
	XL_HASH_HELPER_HASH64_NANO,
	XL_HASH_HELPER_SHA256,
	XL_HASH_HELPER_SHA512
} XL_HashHelperKind;

typedef enum XL_MathHelperKind {
	XL_MATH_HELPER_NONE = 0,
	XL_MATH_HELPER_ABS,
	XL_MATH_HELPER_MIN,
	XL_MATH_HELPER_MAX,
	XL_MATH_HELPER_CLAMP,
	XL_MATH_HELPER_FLOOR,
	XL_MATH_HELPER_CEIL,
	XL_MATH_HELPER_ROUND,
	XL_MATH_HELPER_TRUNC,
	XL_MATH_HELPER_SQRT,
	XL_MATH_HELPER_POW,
	XL_MATH_HELPER_SIN,
	XL_MATH_HELPER_COS,
	XL_MATH_HELPER_TAN,
	XL_MATH_HELPER_EXP,
	XL_MATH_HELPER_LOG,
	XL_MATH_HELPER_LOG10
} XL_MathHelperKind;

typedef enum XL_PackHelperKind {
	XL_PACK_HELPER_NONE = 0,
	XL_PACK_HELPER_WRITE_U8,
	XL_PACK_HELPER_WRITE_U16_LE,
	XL_PACK_HELPER_WRITE_U16_BE,
	XL_PACK_HELPER_WRITE_U32_LE,
	XL_PACK_HELPER_WRITE_U32_BE,
	XL_PACK_HELPER_READ_U8,
	XL_PACK_HELPER_READ_U16_LE,
	XL_PACK_HELPER_READ_U16_BE,
	XL_PACK_HELPER_READ_U32_LE,
	XL_PACK_HELPER_READ_U32_BE
} XL_PackHelperKind;

typedef enum XL_JsonHelperKind {
	XL_JSON_HELPER_NONE = 0,
	XL_JSON_HELPER_ENCODE,
	XL_JSON_HELPER_DECODE
} XL_JsonHelperKind;

typedef enum XL_EnvHelperKind {
	XL_ENV_HELPER_NONE = 0,
	XL_ENV_HELPER_GET,
	XL_ENV_HELPER_EXISTS,
	XL_ENV_HELPER_SET,
	XL_ENV_HELPER_CWD,
	XL_ENV_HELPER_TEMP,
	XL_ENV_HELPER_HOME
} XL_EnvHelperKind;

typedef enum XL_DirHelperKind {
	XL_DIR_HELPER_NONE = 0,
	XL_DIR_HELPER_CREATE,
	XL_DIR_HELPER_LIST,
	XL_DIR_HELPER_FILES,
	XL_DIR_HELPER_DIRS,
	XL_DIR_HELPER_TREE,
	XL_DIR_HELPER_COPY,
	XL_DIR_HELPER_MOVE,
	XL_DIR_HELPER_DELETE
} XL_DirHelperKind;

typedef enum XL_ProcessHelperKind {
	XL_PROCESS_HELPER_NONE = 0,
	XL_PROCESS_HELPER_EXEC,
	XL_PROCESS_HELPER_CAPTURE,
	XL_PROCESS_HELPER_EXEC_IN,
	XL_PROCESS_HELPER_CAPTURE_IN,
	XL_PROCESS_HELPER_TERMINAL_SUPPORTED
} XL_ProcessHelperKind;

static int __xlCgenBufGrow(XL_CgenBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pBuf->iCap == 0) ? 512 : pBuf->iCap;
	while ( iNewCap < (pBuf->iLen + iNeed + 1) ) {
		iNewCap *= 2;
	}

	sNewData = (char*)realloc(pBuf->sData, iNewCap);
	if ( sNewData == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pBuf->sData = sNewData;
	pBuf->iCap = iNewCap;
	return XL_RC_OK;
}

static void __xlCgenAppend(XL_CgenBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlCgenBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static int __xlTypeHasBaseName(const char* sTypeName, const char* sBaseName)
{
	size_t iBaseLen;

	if ( (sTypeName == NULL) || (sBaseName == NULL) ) {
		return 0;
	}

	iBaseLen = strlen(sBaseName);
	if ( strncmp(sTypeName, sBaseName, iBaseLen) != 0 ) {
		return 0;
	}
	return (sTypeName[iBaseLen] == '\0') || (sTypeName[iBaseLen] == '<');
}

static int __xlIsFutureTypeName(const char* sTypeName)
{
	return __xlTypeHasBaseName(sTypeName, "future");
}

static int __xlIsTaskTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) && (strcmp(sTypeName, "task") == 0);
}

static int __xlIsFunctionTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) && (strncmp(sTypeName, "function<", 9) == 0);
}

static int __xlExtractContainerValueType(const char* sTypeName, const char* sBaseName, char* sOut, size_t iOutSize)
{
	const char* sStart;
	const char* sEnd;
	size_t iBaseLen;
	size_t iCopyLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return 0;
	}

	sOut[0] = '\0';
	if ( !__xlTypeHasBaseName(sTypeName, sBaseName) ) {
		return 0;
	}

	iBaseLen = strlen(sBaseName);
	if ( sTypeName[iBaseLen] == '\0' ) {
		snprintf(sOut, iOutSize, "%s", "any");
		return 1;
	}

	sStart = sTypeName + iBaseLen + 1;
	sEnd = strrchr(sStart, '>');
	if ( (sEnd == NULL) || (sEnd <= sStart) ) {
		return 0;
	}

	iCopyLen = (size_t)(sEnd - sStart);
	if ( iCopyLen >= iOutSize ) {
		iCopyLen = iOutSize - 1;
	}
	memcpy(sOut, sStart, iCopyLen);
	sOut[iCopyLen] = '\0';
	return 1;
}

static int __xlExtractFutureValueType(const char* sTypeName, char* sOut, size_t iOutSize)
{
	return __xlExtractContainerValueType(sTypeName, "future", sOut, iOutSize);
}

static char* __xlCgenDupTextRange(const char* sText, size_t iLen)
{
	char* sCopy;

	if ( sText == NULL ) {
		return NULL;
	}

	sCopy = (char*)malloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( iLen != 0 ) {
		memcpy(sCopy, sText, iLen);
	}
	sCopy[iLen] = '\0';
	return sCopy;
}

static void __xlCgenFreeTypeList(char** arrTypes, size_t iCount)
{
	size_t i;

	if ( arrTypes == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; ++i ) {
		free(arrTypes[i]);
	}
	free(arrTypes);
}

static int __attribute__((unused)) __xlParseFunctionTypeName(
	const char* sTypeName,
	char** psReturnType,
	char*** parrParamTypes,
	size_t* piParamCount
)
{
	const char* sScan;
	const char* sReturnStart;
	const char* sParamsStart;
	const char* sParamsEnd;
	char** arrParamTypes;
	size_t iAngleDepth;
	size_t iCount;

	if ( psReturnType != NULL ) {
		*psReturnType = NULL;
	}
	if ( parrParamTypes != NULL ) {
		*parrParamTypes = NULL;
	}
	if ( piParamCount != NULL ) {
		*piParamCount = 0;
	}
	if ( !__xlIsFunctionTypeName(sTypeName) ) {
		return 0;
	}

	sReturnStart = sTypeName + 9;
	sScan = sReturnStart;
	sParamsStart = NULL;
	iAngleDepth = 0;
	while ( *sScan != '\0' ) {
		if ( *sScan == '<' ) {
			++iAngleDepth;
		} else if ( *sScan == '>' ) {
			if ( iAngleDepth == 0 ) {
				break;
			}
			--iAngleDepth;
		} else if ( (*sScan == '(') && (iAngleDepth == 0) ) {
			sParamsStart = sScan;
			break;
		}
		++sScan;
	}
	if ( (sParamsStart == NULL) || (sParamsStart == sReturnStart) ) {
		return 0;
	}
	if ( psReturnType != NULL ) {
		*psReturnType = __xlCgenDupTextRange(sReturnStart, (size_t)(sParamsStart - sReturnStart));
		if ( *psReturnType == NULL ) {
			return 0;
		}
	}

	sScan = sParamsStart + 1;
	sParamsEnd = NULL;
	iAngleDepth = 0;
	while ( *sScan != '\0' ) {
		if ( *sScan == '<' ) {
			++iAngleDepth;
		} else if ( *sScan == '>' ) {
			if ( iAngleDepth == 0 ) {
				break;
			}
			--iAngleDepth;
		} else if ( (*sScan == ')') && (iAngleDepth == 0) ) {
			sParamsEnd = sScan;
			break;
		}
		++sScan;
	}
	if ( (sParamsEnd == NULL) || (sParamsEnd[1] != '>') || (sParamsEnd[2] != '\0') ) {
		free((psReturnType != NULL) ? *psReturnType : NULL);
		if ( psReturnType != NULL ) {
			*psReturnType = NULL;
		}
		return 0;
	}

	if ( parrParamTypes == NULL ) {
		return 1;
	}
	if ( sParamsEnd == (sParamsStart + 1) ) {
		return 1;
	}

	iCount = 1;
	sScan = sParamsStart + 1;
	iAngleDepth = 0;
	while ( sScan < sParamsEnd ) {
		if ( *sScan == '<' ) {
			++iAngleDepth;
		} else if ( *sScan == '>' ) {
			if ( iAngleDepth != 0 ) {
				--iAngleDepth;
			}
		} else if ( (*sScan == ',') && (iAngleDepth == 0) ) {
			++iCount;
		}
		++sScan;
	}

	arrParamTypes = (char**)calloc(iCount, sizeof(char*));
	if ( arrParamTypes == NULL ) {
		free((psReturnType != NULL) ? *psReturnType : NULL);
		if ( psReturnType != NULL ) {
			*psReturnType = NULL;
		}
		return 0;
	}

	iCount = 0;
	sScan = sParamsStart + 1;
	{
		const char* sPartStart = sScan;
		iAngleDepth = 0;
		while ( sScan <= sParamsEnd ) {
			if ( (sScan == sParamsEnd) || ((*sScan == ',') && (iAngleDepth == 0)) ) {
				arrParamTypes[iCount] = __xlCgenDupTextRange(sPartStart, (size_t)(sScan - sPartStart));
				if ( arrParamTypes[iCount] == NULL ) {
					__xlCgenFreeTypeList(arrParamTypes, iCount);
					free((psReturnType != NULL) ? *psReturnType : NULL);
					if ( psReturnType != NULL ) {
						*psReturnType = NULL;
					}
					return 0;
				}
				++iCount;
				sPartStart = sScan + 1;
			} else if ( *sScan == '<' ) {
				++iAngleDepth;
			} else if ( (*sScan == '>') && (iAngleDepth != 0) ) {
				--iAngleDepth;
			}
			++sScan;
		}
	}

	*parrParamTypes = arrParamTypes;
	if ( piParamCount != NULL ) {
		*piParamCount = iCount;
	}
	return 1;
}

static int __xlIsValueBackedTypeName(const char* sTypeName)
{
	return __xlTypeHasBaseName(sTypeName, "array") ||
		__xlTypeHasBaseName(sTypeName, "list") ||
		__xlTypeHasBaseName(sTypeName, "set") ||
		__xlTypeHasBaseName(sTypeName, "dict") ||
		((sTypeName != NULL) && (strcmp(sTypeName, "channel") == 0)) ||
		((sTypeName != NULL) && (strcmp(sTypeName, "var") == 0));
}

static int __xlIsVarTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) && (strcmp(sTypeName, "var") == 0);
}

static int __xlIsPointTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) && (strcmp(sTypeName, "point") == 0);
}

static int __xlIsIntegerTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) &&
		((strcmp(sTypeName, "int") == 0) ||
		(strcmp(sTypeName, "int8") == 0) ||
		(strcmp(sTypeName, "uint8") == 0) ||
		(strcmp(sTypeName, "int16") == 0) ||
		(strcmp(sTypeName, "uint16") == 0) ||
		(strcmp(sTypeName, "int32") == 0) ||
		(strcmp(sTypeName, "uint32") == 0) ||
		(strcmp(sTypeName, "int64") == 0) ||
		(strcmp(sTypeName, "uint64") == 0));
}

static int __xlIsFloatTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) &&
		((strcmp(sTypeName, "float") == 0) ||
		(strcmp(sTypeName, "float32") == 0) ||
		(strcmp(sTypeName, "float64") == 0));
}

static int __xlIsNumericTypeName(const char* sTypeName)
{
	return __xlIsIntegerTypeName(sTypeName) || __xlIsFloatTypeName(sTypeName);
}

static int __xlIsTypeCtorTargetName(const char* sTypeName)
{
	return __xlIsIntegerTypeName(sTypeName) ||
		__xlIsFloatTypeName(sTypeName) ||
		((sTypeName != NULL) && (strcmp(sTypeName, "bool") == 0)) ||
		((sTypeName != NULL) && (strcmp(sTypeName, "string") == 0)) ||
		__xlIsPointTypeName(sTypeName) ||
		((sTypeName != NULL) && (strcmp(sTypeName, "time") == 0)) ||
		((sTypeName != NULL) && (strcmp(sTypeName, "var") == 0));
}

static void __xlCgenAppendFormat(XL_CgenBuffer* pBuf, const char* sFormat, ...)
{
	va_list args;
	va_list args2;
	int iNeed;

	va_start(args, sFormat);
	va_copy(args2, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(args2);
		return;
	}

	if ( __xlCgenBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlCgenCopyText(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1;
	}
	if ( iLen != 0 ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
}

static void __xlReplaceFixedToken(char* sText, const char* sNeedle, const char* sReplacement)
{
	size_t iLen;
	char* pScan;

	if ( (sText == NULL) || (sNeedle == NULL) || (sReplacement == NULL) ) {
		return;
	}
	iLen = strlen(sNeedle);
	if ( (iLen == 0) || (iLen != strlen(sReplacement)) ) {
		return;
	}

	pScan = strstr(sText, sNeedle);
	while ( pScan != NULL ) {
		memcpy(pScan, sReplacement, iLen);
		pScan = strstr(pScan + iLen, sNeedle);
	}
}

static void __xlNormalizeGeneratedFutureTypeNames(char* sText)
{
	__xlReplaceFixedToken(sText, "XL_Futureint", "XL_FutureInt");
	__xlReplaceFixedToken(sText, "XL_Futurebool", "XL_FutureBool");
	__xlReplaceFixedToken(sText, "XL_Futurefloat", "XL_FutureFloat");
	__xlReplaceFixedToken(sText, "XL_Futurestring", "XL_FutureString");
	__xlReplaceFixedToken(sText, "XL_Futureerror", "XL_FutureError");
	__xlReplaceFixedToken(sText, "XL_Futureptr", "XL_FuturePtr");
}

static void __xlCgenAppendIndent(XL_CgenBuffer* pBuf)
{
	int i;

	for ( i = 0; i < pBuf->iIndent; ++i ) {
		__xlCgenAppend(pBuf, "\t");
	}
}

static void __xlCgenAppendCString(XL_CgenBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	__xlCgenAppend(pBuf, "\"");
	if ( sText == NULL ) {
		__xlCgenAppend(pBuf, "\"");
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlCgenAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlCgenAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlCgenAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlCgenAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlCgenAppend(pBuf, "\\t");
				break;
			default:
			{
				char sTmp[2];

				sTmp[0] = (char)(*pScan);
				sTmp[1] = '\0';
				__xlCgenAppend(pBuf, sTmp);
				break;
			}
		}
	}

	__xlCgenAppend(pBuf, "\"");
}

static void __xlCgenSanitizeIdentPart(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iOut;
	const unsigned char* pScan;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	iOut = 0;
	for ( pScan = (const unsigned char*)((sText != NULL) ? sText : ""); (*pScan != 0) && (iOut + 1 < iOutSize); ++pScan ) {
		if ( ((*pScan >= 'A') && (*pScan <= 'Z')) ||
			((*pScan >= 'a') && (*pScan <= 'z')) ||
			((*pScan >= '0') && (*pScan <= '9')) ||
			(*pScan == '_') ) {
			sOut[iOut++] = (char)(*pScan);
		} else {
			sOut[iOut++] = '_';
		}
	}
	sOut[iOut] = '\0';
}

static void __xlCgenBuildModuleSymbol(const char* sModuleKey, const char* sSuffix, char* sOut, size_t iOutSize)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];

	__xlCgenSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sModuleKey);
	snprintf(
		sOut,
		iOutSize,
		"xl_mod_%s__%s",
		(sModuleKeySafe[0] != '\0') ? sModuleKeySafe : "module",
		(sSuffix != NULL) ? sSuffix : ""
	);
}

static int __xlCgenModuleKeyHasCollision(const XL_ModuleLinkResult* pModules, const XL_ModuleSummary* pTarget)
{
	size_t i;
	size_t iMatchCount;
	const char* sTargetKey;

	if ( (pModules == NULL) || (pModules->pModules == NULL) || (pTarget == NULL) ) {
		return 0;
	}

	sTargetKey = ((pTarget->sKey != NULL) && (pTarget->sKey[0] != '\0')) ? pTarget->sKey : "<root>";
	iMatchCount = 0;
	for ( i = 0; i < pModules->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;
		const char* sModuleKey;

		pModule = &pModules->pModules[i];
		sModuleKey = ((pModule != NULL) && (pModule->sKey != NULL) && (pModule->sKey[0] != '\0')) ? pModule->sKey : "<root>";
		if ( strcmp(sModuleKey, sTargetKey) != 0 ) {
			continue;
		}
		++iMatchCount;
		if ( iMatchCount > 1u ) {
			return 1;
		}
	}

	return 0;
}

static void __xlCgenBuildUniqueModuleSymbol(const XL_ModuleSummary* pModule, const char* sSuffix, char* sOut, size_t iOutSize)
{
	char sUniqueModuleKey[XL_PATH_MAX_LEN];
	const char* sModuleKey;

	if ( (pModule != NULL) &&
		!pModule->bIsRoot &&
		(pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
		sModuleKey = ((pModule->sKey != NULL) && (pModule->sKey[0] != '\0')) ? pModule->sKey : "module";
		snprintf(sUniqueModuleKey, sizeof(sUniqueModuleKey), "%s__%s", pModule->sPackageAbiId, sModuleKey);
		__xlCgenBuildModuleSymbol(sUniqueModuleKey, sSuffix, sOut, iOutSize);
		return;
	}

	__xlCgenBuildModuleSymbol((pModule != NULL) ? pModule->sKey : NULL, sSuffix, sOut, iOutSize);
}

static void __xlCgenBuildPackageAbiId(const char* sPackageName, char* sOut, size_t iOutSize)
{
	snprintf(
		sOut,
		iOutSize,
		"%s@0.0.0#abi1",
		((sPackageName != NULL) && (sPackageName[0] != '\0')) ? sPackageName : "package"
	);
}

static void __xlCgenBuildModuleAbiId(const XL_ModuleSummary* pModule, char* sOut, size_t iOutSize)
{
	char sPackageAbiId[XL_PATH_MAX_LEN];
	const char* sPackageAbiIdText;
	const char* sModuleKey;

	sPackageAbiIdText = ((pModule != NULL) && (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0')) ?
		pModule->sPackageAbiId : NULL;
	if ( sPackageAbiIdText == NULL ) {
		__xlCgenBuildPackageAbiId((pModule != NULL) ? pModule->sPackageName : NULL, sPackageAbiId, sizeof(sPackageAbiId));
		sPackageAbiIdText = sPackageAbiId;
	}
	sModuleKey = ((pModule != NULL) && (pModule->sKey != NULL) && (pModule->sKey[0] != '\0')) ? pModule->sKey : "<root>";
	snprintf(sOut, iOutSize, "%s::%s", sPackageAbiIdText, sModuleKey);
}

static void __xlCgenPushLoopFrame(XL_CgenContext* pCtx, const char* sBreakLabel, const char* sContinueLabel)
{
	XL_CgenLoopFrame* pFrame;

	if ( (pCtx == NULL) || (pCtx->iLoopDepth >= (int)XL_COUNTOF(pCtx->arrLoopStack)) ) {
		return;
	}

	pFrame = &pCtx->arrLoopStack[pCtx->iLoopDepth++];
	memset(pFrame, 0, sizeof(*pFrame));
	__xlCgenCopyText(pFrame->sBreakLabel, sizeof(pFrame->sBreakLabel), sBreakLabel);
	__xlCgenCopyText(pFrame->sContinueLabel, sizeof(pFrame->sContinueLabel), sContinueLabel);
}

static void __xlCgenPopLoopFrame(XL_CgenContext* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->iLoopDepth <= 0) ) {
		return;
	}

	pCtx->iLoopDepth -= 1;
	memset(&pCtx->arrLoopStack[pCtx->iLoopDepth], 0, sizeof(pCtx->arrLoopStack[pCtx->iLoopDepth]));
}

static const XL_CgenLoopFrame* __xlCgenFindLoopFrame(const XL_CgenContext* pCtx, int iLevel)
{
	int iIndex;

	if ( (pCtx == NULL) || (iLevel <= 0) || (iLevel > pCtx->iLoopDepth) ) {
		return NULL;
	}

	iIndex = pCtx->iLoopDepth - iLevel;
	return &pCtx->arrLoopStack[iIndex];
}

static void __xlCgenPushTryFrame(XL_CgenContext* pCtx, const char* sExitLabel)
{
	XL_CgenTryFrame* pFrame;

	if ( (pCtx == NULL) || (pCtx->iTryDepth >= (int)XL_COUNTOF(pCtx->arrTryStack)) ) {
		return;
	}

	pFrame = &pCtx->arrTryStack[pCtx->iTryDepth++];
	memset(pFrame, 0, sizeof(*pFrame));
	__xlCgenCopyText(pFrame->sExitLabel, sizeof(pFrame->sExitLabel), sExitLabel);
}

static void __xlCgenPopTryFrame(XL_CgenContext* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->iTryDepth <= 0) ) {
		return;
	}

	pCtx->iTryDepth -= 1;
	memset(&pCtx->arrTryStack[pCtx->iTryDepth], 0, sizeof(pCtx->arrTryStack[pCtx->iTryDepth]));
}

static const XL_CgenTryFrame* __xlCgenCurrentTryFrame(const XL_CgenContext* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->iTryDepth <= 0) ) {
		return NULL;
	}

	return &pCtx->arrTryStack[pCtx->iTryDepth - 1];
}

static void __xlCgenPushFinallyFrame(XL_CgenContext* pCtx, const char* sFinallyLabel, const char* sDispatchLabel)
{
	XL_CgenFinallyFrame* pFrame;

	if ( (pCtx == NULL) || (pCtx->iFinallyDepth >= (int)XL_COUNTOF(pCtx->arrFinallyStack)) ) {
		return;
	}

	pFrame = &pCtx->arrFinallyStack[pCtx->iFinallyDepth++];
	memset(pFrame, 0, sizeof(*pFrame));
	__xlCgenCopyText(pFrame->sFinallyLabel, sizeof(pFrame->sFinallyLabel), sFinallyLabel);
	__xlCgenCopyText(pFrame->sDispatchLabel, sizeof(pFrame->sDispatchLabel), sDispatchLabel);
	pFrame->iTryDepthBase = pCtx->iTryDepth;
}

static void __xlCgenPopFinallyFrame(XL_CgenContext* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->iFinallyDepth <= 0) ) {
		return;
	}

	pCtx->iFinallyDepth -= 1;
	memset(&pCtx->arrFinallyStack[pCtx->iFinallyDepth], 0, sizeof(pCtx->arrFinallyStack[pCtx->iFinallyDepth]));
}

static XL_CgenFinallyFrame* __xlCgenCurrentFinallyFrame(XL_CgenContext* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->iFinallyDepth <= 0) ) {
		return NULL;
	}

	return &pCtx->arrFinallyStack[pCtx->iFinallyDepth - 1];
}

static const XL_CgenFinallyFrame* __xlCgenOuterFinallyFrame(const XL_CgenContext* pCtx)
{
	if ( (pCtx == NULL) || (pCtx->iFinallyDepth <= 1) ) {
		return NULL;
	}

	return &pCtx->arrFinallyStack[pCtx->iFinallyDepth - 2];
}

static void __xlCgenSetCurrentFinallyBody(XL_CgenContext* pCtx, int bInFinally)
{
	XL_CgenFinallyFrame* pFrame;

	pFrame = __xlCgenCurrentFinallyFrame(pCtx);
	if ( pFrame != NULL ) {
		pFrame->bInFinally = bInFinally;
	}
}

static const char* __xlFutureTypeName(const char* sTypeName);
static const char* __xlCgenAbstractGeneratorKindName(const char* sTypeName);

static const char* __xlCTypeName(const char* sTypeName)
{
	static char sFutureValueType[128];

	if ( sTypeName == NULL ) {
		return "void";
	}
	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		return __xlFutureTypeName(sFutureValueType);
	}
	if ( strcmp(sTypeName, "int") == 0 ) {
		return "int";
	}
	if ( strcmp(sTypeName, "int8") == 0 ) {
		return "int8_t";
	}
	if ( strcmp(sTypeName, "uint8") == 0 ) {
		return "uint8_t";
	}
	if ( strcmp(sTypeName, "int16") == 0 ) {
		return "int16_t";
	}
	if ( strcmp(sTypeName, "uint16") == 0 ) {
		return "uint16_t";
	}
	if ( strcmp(sTypeName, "int32") == 0 ) {
		return "int32_t";
	}
	if ( strcmp(sTypeName, "uint32") == 0 ) {
		return "uint32_t";
	}
	if ( strcmp(sTypeName, "int64") == 0 ) {
		return "int64_t";
	}
	if ( strcmp(sTypeName, "uint64") == 0 ) {
		return "uint64_t";
	}
	if ( strcmp(sTypeName, "float32") == 0 ) {
		return "float";
	}
	if ( (strcmp(sTypeName, "float") == 0) || (strcmp(sTypeName, "float64") == 0) ) {
		return "double";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "int";
	}
	if ( strcmp(sTypeName, "generator") == 0 ) {
		return "XL_GeneratedGeneratorValue";
	}
	if ( __xlIsPointTypeName(sTypeName) ) {
		return "void*";
	}
	if ( __xlIsFunctionTypeName(sTypeName) ) {
		return "xfunction";
	}
	if ( __xlIsValueBackedTypeName(sTypeName) ) {
		return "xvalue";
	}
	if ( (strcmp(sTypeName, "string") == 0) || (strcmp(sTypeName, "type") == 0) ) {
		return "const char*";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "XL_GeneratedError*";
	}
	if ( strcmp(sTypeName, "time") == 0 ) {
		return "xtime";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "void";
	}
	return "void*";
}

static const char* __xlFutureTypeName(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "XL_FutureVoid";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "XL_FutureInt";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "XL_FutureBool";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "XL_FutureFloat";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "XL_FutureString";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "XL_FutureError";
	}
	return "XL_FuturePtr";
}

static const char* __xlFutureReadyFunc(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_future_void_ready";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_future_int_ready";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_future_bool_ready";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_future_float_ready";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_future_string_ready";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_future_error_ready";
	}
	return "xl_future_ptr_ready";
}

static const char* __xlPromiseResolveFunc(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_generated_promise_resolve_void";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_generated_promise_resolve_int";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_generated_promise_resolve_bool";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_generated_promise_resolve_float";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generated_promise_resolve_string";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_generated_promise_resolve_error";
	}
	return "xl_generated_promise_resolve_ptr";
}

static const char* __xlFutureAwaitFunc(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_future_void_await";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_future_int_await";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_future_bool_await";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_future_float_await";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_future_string_await";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_future_error_await";
	}
	return "xl_future_ptr_await";
}

static const char* __xlFutureAwaitStateFunc(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_future_void_await_state";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_future_int_await_state";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_future_bool_await_state";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_future_float_await_state";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_future_string_await_state";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_future_error_await_state";
	}
	return "xl_future_ptr_await_state";
}

static const char* __xlFutureForwardStateFunc(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_future_void_forward_state";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_future_int_forward_state";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_future_bool_forward_state";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_future_float_forward_state";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_future_string_forward_state";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_future_error_forward_state";
	}
	return "xl_future_ptr_forward_state";
}

static const char* __xlFutureTypeDebugName(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "future<void>";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "future<int>";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "future<bool>";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "future<float>";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "future<string>";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "future<error>";
	}
	return "future<ptr>";
}

static const char* __xlFutureErrorFunc(const char* sTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTypeName = sFutureValueType;
	}
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_future_void_error";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_future_int_error";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_future_bool_error";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_future_float_error";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_future_string_error";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_future_error_error";
	}
	return "xl_future_ptr_error";
}

static const char* __xlFutureValueTypeName(const char* sTypeName)
{
	static char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		return sFutureValueType;
	}
	return sTypeName;
}

static const char* __xlGeneratorItemCType(const char* sTypeName)
{
	if ( __xlIsVarTypeName(sTypeName) ) {
		return "xvalue";
	}
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || __xlIsIntegerTypeName(sTypeName) ) {
		return "int";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "double";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "const char*";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "XL_GeneratedError*";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "int";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "void*";
	}
	return "int";
}

static const char* __xlCgenAbstractGeneratorKindName(const char* sTypeName)
{
	if ( __xlIsVarTypeName(sTypeName) ) {
		return "XL_GENERATED_GENERATOR_KIND_VAR";
	}
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || __xlIsIntegerTypeName(sTypeName) ) {
		return "XL_GENERATED_GENERATOR_KIND_INT";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "XL_GENERATED_GENERATOR_KIND_FLOAT";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "XL_GENERATED_GENERATOR_KIND_STRING";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "XL_GENERATED_GENERATOR_KIND_ERROR";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "XL_GENERATED_GENERATOR_KIND_BOOL";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "XL_GENERATED_GENERATOR_KIND_VOID";
	}
	return "XL_GENERATED_GENERATOR_KIND_INT";
}

static const char* __xlCDefaultReturnValue(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "0";
	}
	if ( __xlIsVarTypeName(sTypeName) ) {
		return "NULL";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return NULL;
	}
	if ( strcmp(sTypeName, "generator") == 0 ) {
		return "xl_generated_generator_null()";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "0.0";
	}
	if ( __xlIsVarTypeName(sTypeName) ) {
		return "xvoCreateNull()";
	}
	if ( __xlIsPointTypeName(sTypeName) ) {
		return "NULL";
	}
	if ( __xlIsFunctionTypeName(sTypeName) ) {
		return "NULL";
	}
	if ( __xlIsValueBackedTypeName(sTypeName) ) {
		return "NULL";
	}
	if ( (strcmp(sTypeName, "string") == 0) || (strcmp(sTypeName, "type") == 0) || (strcmp(sTypeName, "error") == 0) ) {
		return "NULL";
	}
	return "0";
}

static const char* __xlCgenEscapeValueTypeName(const XL_CgenFuncInfo* pFuncInfo)
{
	if ( (pFuncInfo == NULL) || pFuncInfo->bGenerator ) {
		return NULL;
	}
	if ( (pFuncInfo->sTypeName == NULL) || (pFuncInfo->sTypeName[0] == '\0') || (strcmp(pFuncInfo->sTypeName, "void") == 0) ) {
		return NULL;
	}
	return pFuncInfo->sTypeName;
}

static const char* __xlCgenFuncReturnCTypeName(const XL_CgenFuncInfo* pFuncInfo)
{
	if ( (pFuncInfo != NULL) && (pFuncInfo->sReturnCTypeName != NULL) && (pFuncInfo->sReturnCTypeName[0] != '\0') ) {
		return pFuncInfo->sReturnCTypeName;
	}
	return __xlCTypeName((pFuncInfo != NULL) ? pFuncInfo->sTypeName : "void");
}

static const char* __xlCgenFuncReturnDefaultValue(const XL_CgenFuncInfo* pFuncInfo)
{
	if ( (pFuncInfo != NULL) && (pFuncInfo->sReturnDefaultValue != NULL) && (pFuncInfo->sReturnDefaultValue[0] != '\0') ) {
		return pFuncInfo->sReturnDefaultValue;
	}
	return __xlCDefaultReturnValue((pFuncInfo != NULL) ? pFuncInfo->sTypeName : "void");
}

static void __xlCgenEmitEscapeDispatch(XL_CgenContext* pCtx, const XL_CgenFuncInfo* pFuncInfo)
{
	const char* sEscapeKindName;
	const char* sEscapeLevelName;
	const char* sEscapeValueName;
	const char* sEscapeValueTypeName;
	const char* sDefaultReturn;
	int i;

	if ( (pCtx == NULL) || (pFuncInfo == NULL) || (pFuncInfo->sEscapeKindName == NULL) || (pFuncInfo->sEscapeKindName[0] == '\0') ) {
		return;
	}

	sEscapeKindName = pFuncInfo->sEscapeKindName;
	sEscapeLevelName = pFuncInfo->sEscapeLevelName;
	sEscapeValueName = pFuncInfo->sEscapeValueName;
	sEscapeValueTypeName = __xlCgenEscapeValueTypeName(pFuncInfo);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s != 0 )\n", sEscapeKindName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	if ( pFuncInfo->bGenerator ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == 1 ) goto %s;\n", sEscapeKindName, pFuncInfo->sGeneratorDoneLabel);
	} else if ( pFuncInfo->bAsync ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == 1 )\n", sEscapeKindName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		if ( sEscapeValueTypeName != NULL ) {
			__xlCgenAppendFormat(&pCtx->buf, "return %s(%s);\n", __xlFutureReadyFunc(pFuncInfo->sTypeName), sEscapeValueName);
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "return %s();\n", __xlFutureReadyFunc("void"));
		}
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
	} else {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == 1 )\n", sEscapeKindName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		if ( sEscapeValueTypeName != NULL ) {
			__xlCgenAppendFormat(&pCtx->buf, "return %s;\n", sEscapeValueName);
		} else {
			sDefaultReturn = __xlCgenFuncReturnDefaultValue(pFuncInfo);
			if ( sDefaultReturn == NULL ) {
				__xlCgenAppend(&pCtx->buf, "return;\n");
			} else {
				__xlCgenAppendFormat(&pCtx->buf, "return %s;\n", sDefaultReturn);
			}
		}
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
	}

	for ( i = 1; i <= pCtx->iLoopDepth; ++i ) {
		const XL_CgenLoopFrame* pFrame;

		pFrame = __xlCgenFindLoopFrame(pCtx, i);
		if ( pFrame == NULL ) {
			continue;
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( (%s == 2) && (%s == %d) ) goto %s;\n", sEscapeKindName, sEscapeLevelName, i, pFrame->sBreakLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( (%s == 3) && (%s == %d) ) goto %s;\n", sEscapeKindName, sEscapeLevelName, i, pFrame->sContinueLabel);
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = 0;\n", sEscapeKindName);
	if ( (sEscapeLevelName != NULL) && (sEscapeLevelName[0] != '\0') ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = 0;\n", sEscapeLevelName);
	}

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenEmitEscapeToCurrentFinally(
	XL_CgenContext* pCtx,
	const XL_CgenFuncInfo* pFuncInfo,
	int iEscapeKind,
	int iEscapeLevel
)
{
	const XL_CgenFinallyFrame* pFinallyFrame;
	int iTargetTryDepth;
	int i;

	pFinallyFrame = __xlCgenCurrentFinallyFrame(pCtx);
	if ( (pCtx == NULL) || (pFuncInfo == NULL) || (pFinallyFrame == NULL) || (pFuncInfo->sEscapeKindName == NULL) ) {
		return;
	}

	iTargetTryDepth = pFinallyFrame->bInFinally ? pFinallyFrame->iTryDepthBase : (pFinallyFrame->iTryDepthBase + 1);
	if ( !pFuncInfo->bGenerator ) {
		for ( i = pCtx->iTryDepth; i > iTargetTryDepth; --i ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xl_generated_exception_frame_pop();\n");
		}
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = %d;\n", pFuncInfo->sEscapeKindName, iEscapeKind);
	if ( (pFuncInfo->sEscapeLevelName != NULL) && (pFuncInfo->sEscapeLevelName[0] != '\0') ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = %d;\n", pFuncInfo->sEscapeLevelName, iEscapeLevel);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"goto %s;\n",
		pFinallyFrame->bInFinally ? pFinallyFrame->sDispatchLabel : pFinallyFrame->sFinallyLabel
	);
}

static const XL_MirNode* __xlFindFuncNode(const XL_MirNode* pRoot, const char* sName)
{
	const XL_MirNode* pNode;
	size_t i;

	if ( (pRoot == NULL) || (sName == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		pNode = pRoot->ppChildren[i];
		if ( (pNode == NULL) || (pNode->eKind != XL_MIR_FUNC) || (pNode->sText == NULL) ) {
			continue;
		}
		if ( strcmp(pNode->sText, sName) == 0 ) {
			return pNode;
		}
	}

	return NULL;
}

static const char* __xlCallCalleeName(const XL_MirNode* pCall);

static const XL_MirNode* __xlFindFuncNodeByIndex(const XL_MirNode* pRoot, int iFuncIndex)
{
	const XL_MirNode* pNode;
	size_t i;

	if ( (pRoot == NULL) || (iFuncIndex < 0) ) {
		return NULL;
	}

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		pNode = pRoot->ppChildren[i];
		if ( (pNode == NULL) || (pNode->eKind != XL_MIR_FUNC) ) {
			continue;
		}
		if ( pNode->iFuncIndex == iFuncIndex ) {
			return pNode;
		}
	}

	return NULL;
}

static const XL_MirNode* __xlResolveCallFuncNode(const XL_CgenContext* pCtx, const XL_MirNode* pCallNode)
{
	const char* sCalleeName;
	const XL_MirNode* pFunc;

	if ( (pCtx == NULL) || (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) ) {
		return NULL;
	}

	if ( pCallNode->iFuncIndex >= 0 ) {
		pFunc = __xlFindFuncNodeByIndex(pCtx->pRoot, pCallNode->iFuncIndex);
		if ( pFunc != NULL ) {
			return pFunc;
		}
	}

	sCalleeName = __xlCallCalleeName(pCallNode);
	return (sCalleeName != NULL) ? __xlFindFuncNode(pCtx->pRoot, sCalleeName) : NULL;
}

static int __xlResolveAsyncCallType(const XL_CgenContext* pCtx, const XL_MirNode* pCallNode, const char** psTypeName)
{
	const XL_MirNode* pFunc;
	static char sFutureValueType[128];

	if ( psTypeName != NULL ) {
		*psTypeName = NULL;
	}

	pFunc = __xlResolveCallFuncNode(pCtx, pCallNode);
	if ( (pFunc == NULL) || (pFunc->bFlag == 0) ) {
		return 0;
	}

	if ( psTypeName != NULL ) {
		if ( __xlExtractFutureValueType(pFunc->sTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
			*psTypeName = sFutureValueType;
		} else {
			*psTypeName = ((pFunc->sTypeName != NULL) && (pFunc->sTypeName[0] != '\0')) ? pFunc->sTypeName : "void";
		}
	}
	return 1;
}

static int __xlMirContainsKind(const XL_MirNode* pNode, XL_MirKind eKind)
{
	size_t i;

	if ( pNode == NULL ) {
		return 0;
	}
	if ( pNode->eKind == eKind ) {
		return 1;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlMirContainsKind(pNode->ppChildren[i], eKind) ) {
			return 1;
		}
	}

	return 0;
}

static int __xlMirCountKind(const XL_MirNode* pNode, XL_MirKind eKind)
{
	size_t i;
	int iCount;

	if ( pNode == NULL ) {
		return 0;
	}

	iCount = (pNode->eKind == eKind) ? 1 : 0;
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		iCount += __xlMirCountKind(pNode->ppChildren[i], eKind);
	}
	return iCount;
}

static int __xlStmtNestedYieldExprCount(const XL_MirNode* pNode)
{
	if ( pNode == NULL ) {
		return 0;
	}

	switch ( pNode->eKind ) {
		case XL_MIR_BIND:
		case XL_MIR_RETURN:
		case XL_MIR_THROW:
		case XL_MIR_IMPORT:
		case XL_MIR_EXPORT:
			return __xlMirCountKind((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, XL_MIR_YIELD);
		case XL_MIR_BRANCH:
		case XL_MIR_WHILE:
			return __xlMirCountKind((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, XL_MIR_YIELD);
		case XL_MIR_ASSERT:
			return __xlMirCountKind((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, XL_MIR_YIELD) +
				__xlMirCountKind((pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, XL_MIR_YIELD);
		case XL_MIR_FORIN:
			return __xlMirCountKind((pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, XL_MIR_YIELD) +
				__xlMirCountKind((pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, XL_MIR_YIELD);
		case XL_MIR_TRY:
		case XL_MIR_BLOCK:
		case XL_MIR_YIELD:
			return 0;
		default:
			return __xlMirCountKind(pNode, XL_MIR_YIELD);
	}
}

static const XL_MirNode* __xlMirFindFirstKind(const XL_MirNode* pNode, XL_MirKind eKind)
{
	size_t i;
	const XL_MirNode* pFound;

	if ( pNode == NULL ) {
		return NULL;
	}
	if ( pNode->eKind == eKind ) {
		return pNode;
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		pFound = __xlMirFindFirstKind(pNode->ppChildren[i], eKind);
		if ( pFound != NULL ) {
			return pFound;
		}
	}
	return NULL;
}

static const XL_MirNode* __xlAwaitInnerExpr(const XL_MirNode* pNode)
{
	if ( (pNode == NULL) ||
		(pNode->eKind != XL_MIR_VALUE) ||
		(pNode->iChildCount != 1) ||
		(pNode->sText == NULL) ||
		(strcmp(pNode->sText, "await") != 0) ) {
		return NULL;
	}

	return pNode->ppChildren[0];
}

static const XL_MirNode* __xlMirFindFirstAwaitExpr(const XL_MirNode* pNode)
{
	size_t i;
	const XL_MirNode* pFound;

	if ( pNode == NULL ) {
		return NULL;
	}
	if ( __xlAwaitInnerExpr(pNode) != NULL ) {
		return pNode;
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		pFound = __xlMirFindFirstAwaitExpr(pNode->ppChildren[i]);
		if ( pFound != NULL ) {
			return pFound;
		}
	}
	return NULL;
}

static int __xlMirContainsAwaitExpr(const XL_MirNode* pNode)
{
	size_t i;

	if ( pNode == NULL ) {
		return 0;
	}
	if ( __xlAwaitInnerExpr(pNode) != NULL ) {
		return 1;
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlMirContainsAwaitExpr(pNode->ppChildren[i]) ) {
			return 1;
		}
	}
	return 0;
}

static int __xlMirCountAwaitExpr(const XL_MirNode* pNode)
{
	size_t i;
	int iCount;

	if ( pNode == NULL ) {
		return 0;
	}
	iCount = (__xlAwaitInnerExpr(pNode) != NULL) ? 1 : 0;
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		iCount += __xlMirCountAwaitExpr(pNode->ppChildren[i]);
	}
	return iCount;
}

static int __xlMirTryConstBoolValue(const XL_MirNode* pNode, int* pbValue)
{
	if ( pbValue != NULL ) {
		*pbValue = 0;
	}
	if ( (pNode == NULL) || (pNode->eKind != XL_MIR_VALUE) || (pNode->sText == NULL) ) {
		return 0;
	}
	if ( strcmp(pNode->sText, "true") == 0 ) {
		if ( pbValue != NULL ) {
			*pbValue = 1;
		}
		return 1;
	}
	if ( strcmp(pNode->sText, "false") == 0 ) {
		if ( pbValue != NULL ) {
			*pbValue = 0;
		}
		return 1;
	}
	return 0;
}

static int __xlCanAsyncLinearReplaceAwaitExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_MirNode* pAwaitNode)
{
	if ( pNode == NULL ) {
		return 0;
	}
	if ( pNode == pAwaitNode ) {
		return 1;
	}
	if ( !__xlMirContainsAwaitExpr(pNode) ) {
		return 1;
	}
	if ( (pNode->eKind == XL_MIR_MEMBER) && (pNode->iChildCount == 1) ) {
		return __xlCanAsyncLinearReplaceAwaitExpr(pCtx, pNode->ppChildren[0], pAwaitNode);
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 1) ) {
		if ( (pNode->sText != NULL) && (strcmp(pNode->sText, "await") == 0) ) {
			return (pNode == pAwaitNode);
		}
		return __xlCanAsyncLinearReplaceAwaitExpr(pCtx, pNode->ppChildren[0], pAwaitNode);
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) && (strcmp(pNode->sText, "=") == 0) ) {
		const XL_MirNode* pLeft = pNode->ppChildren[0];
		const XL_MirNode* pRight = pNode->ppChildren[1];
		const char* sLeftTypeName = __xlInferExprTypeName(pCtx, pLeft);
		const char* sRightTypeName = __xlInferExprTypeName(pCtx, pRight);

		if ( (pLeft != NULL) &&
			(pLeft->iChildCount == 2) &&
			((pLeft->sText == NULL) || (pLeft->sText[0] == '\0')) ) {
			const XL_MirNode* pBase = pLeft->ppChildren[0];
			const XL_MirNode* pIndex = pLeft->ppChildren[1];
			const char* sBaseTypeName = __xlInferExprTypeName(pCtx, pBase);

			if ( __xlTypeHasBaseName(sBaseTypeName, "array") ||
				__xlTypeHasBaseName(sBaseTypeName, "list") ||
				__xlTypeHasBaseName(sBaseTypeName, "dict") ) {
				return !__xlMirContainsAwaitExpr(pBase) &&
					__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pIndex, pAwaitNode) &&
					__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pRight, pAwaitNode);
			}
		}
		if ( __xlMirContainsAwaitExpr(pLeft) ||
			__xlIsVarTypeName(sLeftTypeName) ||
			__xlIsVarTypeName(sRightTypeName) ||
			__xlIsFunctionTypeName(sLeftTypeName) ||
			__xlIsFunctionTypeName(sRightTypeName) ) {
			return 0;
		}
		return __xlCanAsyncLinearReplaceAwaitExpr(pCtx, pRight, pAwaitNode);
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		const char* sLeftTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[0]);
		const char* sRightTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[1]);

		if ( (strcmp(pNode->sText, "**") == 0) ||
			(strcmp(pNode->sText, "**=") == 0) ||
			(strcmp(pNode->sText, "?=") == 0) ||
			(strcmp(pNode->sText, "~=") == 0) ||
			(strcmp(pNode->sText, "??") == 0) ||
			(strcmp(pNode->sText, "in") == 0) ||
			(strcmp(pNode->sText, "=") == 0) ||
			__xlIsVarTypeName(sLeftTypeName) ||
			__xlIsVarTypeName(sRightTypeName) ) {
			return 0;
		}
		return __xlCanAsyncLinearReplaceAwaitExpr(pCtx, pNode->ppChildren[0], pAwaitNode) &&
			__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pNode->ppChildren[1], pAwaitNode);
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 3) && (pNode->sText != NULL) && (strcmp(pNode->sText, "?:") == 0) ) {
		return __xlCanAsyncLinearReplaceAwaitExpr(pCtx, pNode->ppChildren[0], pAwaitNode) &&
			__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pNode->ppChildren[1], pAwaitNode) &&
			__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pNode->ppChildren[2], pAwaitNode);
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlStrHelperKind(__xlCallCalleeName(pNode)) != XL_STR_HELPER_NONE) ) {
		const XL_MirNode* pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
		size_t i;

		for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
			if ( !__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pArgs->ppChildren[i], pAwaitNode) ) {
				return 0;
			}
		}
		return 1;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlContainerMethodBase(pNode) != NULL) ) {
		const XL_MirNode* pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
		const XL_MirNode* pBase = __xlContainerMethodBase(pNode);
		size_t i;

		if ( __xlMirContainsAwaitExpr(pBase) ) {
			return 0;
		}
		for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
			if ( !__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pArgs->ppChildren[i], pAwaitNode) ) {
				return 0;
			}
		}
		return 1;
	}
	if ( pNode->eKind == XL_MIR_CALL ) {
		const XL_MirNode* pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
		const XL_MirNode* pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
		const XL_MirNode* pFunc = __xlResolveCallFuncNode(pCtx, pNode);
		const char* sCalleeType = __xlInferExprTypeName(pCtx, pCallee);
		size_t i;

		if ( __xlCallHasExpandedMultiReturnArg(pNode) || __xlMirContainsAwaitExpr(pCallee) ) {
			return 0;
		}
		if ( (pFunc == NULL) && !__xlIsFunctionTypeName(sCalleeType) ) {
			return 0;
		}
		for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
			if ( !__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pArgs->ppChildren[i], pAwaitNode) ) {
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

static int __xlCanAsyncLinearResolveTryFinallyStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	const XL_MirNode** ppFinallyBlock,
	size_t* piInnerStmtIndex
)
{
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pInnerStmt;
	const XL_MirNode* pFinallyBlock;
	size_t iInnerStmtIndex;

	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = NULL;
	}
	if ( ppFinallyBlock != NULL ) {
		*ppFinallyBlock = NULL;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = 0;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) || (pStmt->eKind != XL_MIR_TRY) || (pAwaitNode == NULL) ) {
		return 0;
	}

	if ( ((pStmt->bFlag & 1) != 0) || ((pStmt->bFlag & 2) == 0) ) {
		return 0;
	}

	pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	pFinallyBlock = (pStmt->iChildCount > 3) ? pStmt->ppChildren[3] : NULL;
	if ( (pTryBlock == NULL) || (pTryBlock->eKind != XL_MIR_BLOCK) || (pTryBlock->iChildCount == 0) || (pFinallyBlock == NULL) ) {
		return 0;
	}

	if ( !__xlFindAsyncLinearTryAwaitStmtEx(pCtx, pTryBlock, pAwaitNode, pAwaitNodeAlt, &pInnerStmt, &iInnerStmtIndex) ||
		__xlMirContainsAwaitExpr(pFinallyBlock) ||
		(__xlMirCountKind(pFinallyBlock, XL_MIR_RETURN) != 0) ||
		(__xlMirCountKind(pFinallyBlock, XL_MIR_BREAK) != 0) ||
		(__xlMirCountKind(pFinallyBlock, XL_MIR_CONTINUE) != 0) ) {
		return 0;
	}

	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = pInnerStmt;
	}
	if ( ppFinallyBlock != NULL ) {
		*ppFinallyBlock = pFinallyBlock;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = iInnerStmtIndex;
	}
	return 1;
}

static int __xlCanAsyncLinearResolveTryFinallyStmt(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppInnerStmt,
	const XL_MirNode** ppFinallyBlock,
	size_t* piInnerStmtIndex
)
{
	return __xlCanAsyncLinearResolveTryFinallyStmtEx(
		pCtx,
		pStmt,
		pAwaitNode,
		NULL,
		ppInnerStmt,
		ppFinallyBlock,
		piInnerStmtIndex
	);
}

static int __xlAnalyzeAsyncLinearDualBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode** ppThenBlock,
	const XL_MirNode** ppElseBlock,
	const XL_MirNode** ppThenAwaitNode,
	const XL_MirNode** ppElseAwaitNode
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	const XL_MirNode* pThenAwaitNode;
	const XL_MirNode* pElseAwaitNode;
	const char* sThenAwaitType;
	const char* sElseAwaitType;
	const char* sThenValueType;
	const char* sElseValueType;

	if ( ppThenBlock != NULL ) {
		*ppThenBlock = NULL;
	}
	if ( ppElseBlock != NULL ) {
		*ppElseBlock = NULL;
	}
	if ( ppThenAwaitNode != NULL ) {
		*ppThenAwaitNode = NULL;
	}
	if ( ppElseAwaitNode != NULL ) {
		*ppElseAwaitNode = NULL;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) || (pStmt->eKind != XL_MIR_BRANCH) ) {
		return 0;
	}

	pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	pThenBlock = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
	pElseBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
	if ( __xlMirContainsAwaitExpr(pCond) ||
		(pThenBlock == NULL) ||
		(pElseBlock == NULL) ||
		(pThenBlock->eKind != XL_MIR_BLOCK) ||
		(pElseBlock->eKind != XL_MIR_BLOCK) ||
		(__xlMirCountAwaitExpr(pThenBlock) != 1) ||
		(__xlMirCountAwaitExpr(pElseBlock) != 1) ) {
		return 0;
	}

	pThenAwaitNode = __xlMirFindFirstAwaitExpr(pThenBlock);
	pElseAwaitNode = __xlMirFindFirstAwaitExpr(pElseBlock);
	if ( (pThenAwaitNode == NULL) ||
		(pElseAwaitNode == NULL) ||
		!__xlFindAsyncLinearBlockAwaitStmt(pCtx, pThenBlock, pThenAwaitNode, NULL, NULL) ||
		!__xlFindAsyncLinearBlockAwaitStmt(pCtx, pElseBlock, pElseAwaitNode, NULL, NULL) ) {
		return 0;
	}

	sThenAwaitType = __xlInferExprTypeName(pCtx, __xlAwaitInnerExpr(pThenAwaitNode));
	sElseAwaitType = __xlInferExprTypeName(pCtx, __xlAwaitInnerExpr(pElseAwaitNode));
	sThenValueType = __xlInferExprTypeName(pCtx, pThenAwaitNode);
	sElseValueType = __xlInferExprTypeName(pCtx, pElseAwaitNode);
	if ( (strcmp((sThenAwaitType != NULL) ? sThenAwaitType : "", (sElseAwaitType != NULL) ? sElseAwaitType : "") != 0) ||
		(strcmp((sThenValueType != NULL) ? sThenValueType : "", (sElseValueType != NULL) ? sElseValueType : "") != 0) ) {
		return 0;
	}

	if ( ppThenBlock != NULL ) {
		*ppThenBlock = pThenBlock;
	}
	if ( ppElseBlock != NULL ) {
		*ppElseBlock = pElseBlock;
	}
	if ( ppThenAwaitNode != NULL ) {
		*ppThenAwaitNode = pThenAwaitNode;
	}
	if ( ppElseAwaitNode != NULL ) {
		*ppElseAwaitNode = pElseAwaitNode;
	}
	return 1;
}

static int __xlAnalyzeAsyncLinearDynamicAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	int* pbAwaitOnTrue
)
{
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pInnerStmt;

	if ( pbAwaitOnTrue != NULL ) {
		*pbAwaitOnTrue = 0;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) || (pAwaitNode == NULL) ) {
		return 0;
	}
	if ( pStmt->eKind == XL_MIR_BRANCH ) {
		if ( __xlSelectAsyncLinearBranchAwaitBlock(pCtx, pStmt, pAwaitNode) != NULL ) {
			return 0;
		}
		return __xlAnalyzeAsyncLinearDynamicBranchAwait(pCtx, pStmt, pAwaitNode, NULL, NULL, pbAwaitOnTrue);
	}
	if ( pStmt->eKind == XL_MIR_BLOCK ) {
		if ( !__xlFindAsyncLinearBlockAwaitStmt(pCtx, pStmt, pAwaitNode, &pInnerStmt, NULL) ) {
			return 0;
		}
		return __xlAnalyzeAsyncLinearDynamicAwaitStep(pCtx, pInnerStmt, pAwaitNode, pbAwaitOnTrue);
	}
	if ( pStmt->eKind == XL_MIR_TRY ) {
		pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		if ( !__xlFindAsyncLinearTryAwaitStmt(pCtx, pTryBlock, pAwaitNode, &pInnerStmt, NULL) ) {
			return 0;
		}
		return __xlAnalyzeAsyncLinearDynamicAwaitStep(pCtx, pInnerStmt, pAwaitNode, pbAwaitOnTrue);
	}
	return 0;
}

static int __xlDiscoverAsyncLinearDualAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode** ppPrimaryAwaitNode,
	const XL_MirNode** ppAltAwaitNode,
	int* pbPrimaryOnTrue
)
{
	const XL_MirNode* pInnerStmt;
	size_t iInnerStmtIndex;
	size_t i;

	if ( ppPrimaryAwaitNode != NULL ) {
		*ppPrimaryAwaitNode = NULL;
	}
	if ( ppAltAwaitNode != NULL ) {
		*ppAltAwaitNode = NULL;
	}
	if ( pbPrimaryOnTrue != NULL ) {
		*pbPrimaryOnTrue = 0;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) ) {
		return 0;
	}
	if ( pStmt->eKind == XL_MIR_BRANCH ) {
		if ( __xlSelectAsyncLinearBranchDualAwaitBlock(pCtx, pStmt, ppPrimaryAwaitNode, ppAltAwaitNode, pbPrimaryOnTrue) != NULL ) {
			return 1;
		}
		const XL_MirNode* pThenAwaitNode;
		const XL_MirNode* pElseAwaitNode;

		if ( !__xlAnalyzeAsyncLinearDualBranchAwait(pCtx, pStmt, NULL, NULL, &pThenAwaitNode, &pElseAwaitNode) ) {
			return 0;
		}
		if ( ppPrimaryAwaitNode != NULL ) {
			*ppPrimaryAwaitNode = pThenAwaitNode;
		}
		if ( ppAltAwaitNode != NULL ) {
			*ppAltAwaitNode = pElseAwaitNode;
		}
		if ( pbPrimaryOnTrue != NULL ) {
			*pbPrimaryOnTrue = 1;
		}
		return 1;
	}
	if ( pStmt->eKind == XL_MIR_BLOCK ) {
		if ( __xlMirCountAwaitExpr(pStmt) != 2 ) {
			return 0;
		}
		pInnerStmt = NULL;
		iInnerStmtIndex = 0;
		for ( i = 0; i < pStmt->iChildCount; ++i ) {
			const XL_MirNode* pChild = pStmt->ppChildren[i];

			if ( pChild == NULL ) {
				return 0;
			}
			if ( __xlMirContainsAwaitExpr(pChild) ) {
				if ( (pInnerStmt != NULL) ||
					!__xlDiscoverAsyncLinearDualAwaitStep(pCtx, pChild, ppPrimaryAwaitNode, ppAltAwaitNode, pbPrimaryOnTrue) ) {
					return 0;
				}
				pInnerStmt = pChild;
				iInnerStmtIndex = i;
				continue;
			}
			if ( (__xlMirCountKind(pChild, XL_MIR_RETURN) != 0) ||
				(__xlMirCountKind(pChild, XL_MIR_BREAK) != 0) ||
				(__xlMirCountKind(pChild, XL_MIR_CONTINUE) != 0) ) {
				return 0;
			}
			if ( (pInnerStmt == NULL) && (pChild->eKind == XL_MIR_BIND) ) {
				return 0;
			}
		}
		(void)iInnerStmtIndex;
		return pInnerStmt != NULL;
	}
	if ( pStmt->eKind == XL_MIR_TRY ) {
		const XL_MirNode* pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;

		if ( (pTryBlock == NULL) || (pTryBlock->eKind != XL_MIR_BLOCK) ) {
			return 0;
		}
		return __xlDiscoverAsyncLinearDualAwaitStep(pCtx, pTryBlock, ppPrimaryAwaitNode, ppAltAwaitNode, pbPrimaryOnTrue);
	}
	return 0;
}

static int __xlAnalyzeAsyncLinearDualAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	int* pbPrimaryOnTrue
)
{
	const XL_MirNode* pPrimaryAwaitNode;
	const XL_MirNode* pAltAwaitNode;
	int bPrimaryOnTrue;

	if ( pbPrimaryOnTrue != NULL ) {
		*pbPrimaryOnTrue = 0;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) || (pAwaitNode == NULL) || (pAwaitNodeAlt == NULL) ) {
		return 0;
	}
	if ( !__xlDiscoverAsyncLinearDualAwaitStep(pCtx, pStmt, &pPrimaryAwaitNode, &pAltAwaitNode, &bPrimaryOnTrue) ) {
		return 0;
	}
	if ( (pPrimaryAwaitNode == pAwaitNode) && (pAltAwaitNode == pAwaitNodeAlt) ) {
		if ( pbPrimaryOnTrue != NULL ) {
			*pbPrimaryOnTrue = bPrimaryOnTrue;
		}
		return 1;
	}
	if ( (pPrimaryAwaitNode == pAwaitNodeAlt) && (pAltAwaitNode == pAwaitNode) ) {
		if ( pbPrimaryOnTrue != NULL ) {
			*pbPrimaryOnTrue = bPrimaryOnTrue ? 0 : 1;
		}
		return 1;
	}
	return 0;
}

static int __xlAnalyzeAsyncLinearStepBranchSelector(
	XL_CgenContext* pCtx,
	const XL_CgenAsyncLinearResumeStep* pStep,
	int* pbPrimaryOnTrue,
	int* pbDualBranch
)
{
	if ( pbPrimaryOnTrue != NULL ) {
		*pbPrimaryOnTrue = 0;
	}
	if ( pbDualBranch != NULL ) {
		*pbDualBranch = 0;
	}
	if ( (pCtx == NULL) || (pStep == NULL) || (pStep->pAwaitStmt == NULL) ) {
		return 0;
	}
	if ( pStep->pAwaitNodeAlt != NULL ) {
		if ( __xlAnalyzeAsyncLinearDualAwaitStep(pCtx, pStep->pAwaitStmt, pStep->pAwaitNode, pStep->pAwaitNodeAlt, pbPrimaryOnTrue) ) {
			if ( pbDualBranch != NULL ) {
				*pbDualBranch = 1;
			}
			return 1;
		}
		return 0;
	}
	return __xlAnalyzeAsyncLinearDynamicAwaitStep(pCtx, pStep->pAwaitStmt, pStep->pAwaitNode, pbPrimaryOnTrue);
}

static int __xlCanAsyncLinearResolveTryCatchStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	const XL_MirNode** ppCatchName,
	const XL_MirNode** ppCatchBlock,
	const XL_MirNode** ppFinallyBlock,
	size_t* piInnerStmtIndex
)
{
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pInnerStmt;
	const XL_MirNode* pCatchName;
	const XL_MirNode* pCatchBlock;
	const XL_MirNode* pFinallyBlock;
	const char* sCatchTypeName;
	size_t iInnerStmtIndex;

	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = NULL;
	}
	if ( ppCatchName != NULL ) {
		*ppCatchName = NULL;
	}
	if ( ppCatchBlock != NULL ) {
		*ppCatchBlock = NULL;
	}
	if ( ppFinallyBlock != NULL ) {
		*ppFinallyBlock = NULL;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = 0;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) || (pStmt->eKind != XL_MIR_TRY) || (pAwaitNode == NULL) ) {
		return 0;
	}
	if ( (pStmt->bFlag & 1) == 0 ) {
		return 0;
	}

	pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	pCatchName = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
	pCatchBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
	pFinallyBlock = (pStmt->iChildCount > 3) ? pStmt->ppChildren[3] : NULL;
	if ( (pTryBlock == NULL) || (pTryBlock->eKind != XL_MIR_BLOCK) || (pTryBlock->iChildCount == 0) || (pCatchBlock == NULL) ) {
		return 0;
	}

	sCatchTypeName = ((pCatchName != NULL) && (pCatchName->sTypeName != NULL) && (pCatchName->sTypeName[0] != '\0')) ? pCatchName->sTypeName : "error";
	if ( strcmp(sCatchTypeName, "error") != 0 ) {
		return 0;
	}

	if ( !__xlFindAsyncLinearTryAwaitStmtEx(pCtx, pTryBlock, pAwaitNode, pAwaitNodeAlt, &pInnerStmt, &iInnerStmtIndex) ||
		__xlMirContainsAwaitExpr(pCatchBlock) ||
		((pFinallyBlock != NULL) && __xlMirContainsAwaitExpr(pFinallyBlock)) ||
		(__xlMirCountKind(pCatchBlock, XL_MIR_RETURN) != 0) ||
		(__xlMirCountKind(pCatchBlock, XL_MIR_BREAK) != 0) ||
		(__xlMirCountKind(pCatchBlock, XL_MIR_CONTINUE) != 0) ||
		((pFinallyBlock != NULL) && (__xlMirCountKind(pFinallyBlock, XL_MIR_RETURN) != 0)) ||
		((pFinallyBlock != NULL) && (__xlMirCountKind(pFinallyBlock, XL_MIR_BREAK) != 0)) ||
		((pFinallyBlock != NULL) && (__xlMirCountKind(pFinallyBlock, XL_MIR_CONTINUE) != 0)) ) {
		return 0;
	}

	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = pInnerStmt;
	}
	if ( ppCatchName != NULL ) {
		*ppCatchName = pCatchName;
	}
	if ( ppCatchBlock != NULL ) {
		*ppCatchBlock = pCatchBlock;
	}
	if ( ppFinallyBlock != NULL ) {
		*ppFinallyBlock = pFinallyBlock;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = iInnerStmtIndex;
	}
	return 1;
}

static int __xlCanAsyncLinearResolveTryCatchStmt(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppInnerStmt,
	const XL_MirNode** ppCatchName,
	const XL_MirNode** ppCatchBlock,
	const XL_MirNode** ppFinallyBlock,
	size_t* piInnerStmtIndex
)
{
	return __xlCanAsyncLinearResolveTryCatchStmtEx(
		pCtx,
		pStmt,
		pAwaitNode,
		NULL,
		ppInnerStmt,
		ppCatchName,
		ppCatchBlock,
		ppFinallyBlock,
		piInnerStmtIndex
	);
}

static int __xlFindAsyncLinearTryAwaitStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pTryBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
)
{
	const XL_MirNode* pInnerStmt;
	size_t i;
	size_t iInnerStmtIndex;

	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = NULL;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = 0;
	}
	if ( (pCtx == NULL) || (pTryBlock == NULL) || (pTryBlock->eKind != XL_MIR_BLOCK) || (pAwaitNode == NULL) ) {
		return 0;
	}
	if ( pAwaitNodeAlt != NULL ) {
		if ( __xlMirCountAwaitExpr(pTryBlock) != 2 ) {
			return 0;
		}
	} else if ( (__xlMirCountAwaitExpr(pTryBlock) != 1) || (__xlMirFindFirstAwaitExpr(pTryBlock) != pAwaitNode) ) {
		return 0;
	}

	pInnerStmt = NULL;
	iInnerStmtIndex = 0;
	for ( i = 0; i < pTryBlock->iChildCount; ++i ) {
		const XL_MirNode* pChild = pTryBlock->ppChildren[i];

		if ( pChild == NULL ) {
			return 0;
		}
		if ( __xlMirContainsAwaitExpr(pChild) ) {
			if ( (pInnerStmt != NULL) ||
				!__xlCanAsyncLinearResolveStmtEx(pCtx, pChild, pAwaitNode, pAwaitNodeAlt) ) {
				return 0;
			}
			pInnerStmt = pChild;
			iInnerStmtIndex = i;
			continue;
		}
		if ( (__xlMirCountKind(pChild, XL_MIR_RETURN) != 0) ||
			(__xlMirCountKind(pChild, XL_MIR_BREAK) != 0) ||
			(__xlMirCountKind(pChild, XL_MIR_CONTINUE) != 0) ) {
			return 0;
		}
		if ( (pInnerStmt == NULL) && (pChild->eKind == XL_MIR_BIND) ) {
			return 0;
		}
	}
	if ( pInnerStmt == NULL ) {
		return 0;
	}
	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = pInnerStmt;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = iInnerStmtIndex;
	}
	return 1;
}

static int __xlFindAsyncLinearTryAwaitStmt(
	XL_CgenContext* pCtx,
	const XL_MirNode* pTryBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
)
{
	return __xlFindAsyncLinearTryAwaitStmtEx(
		pCtx,
		pTryBlock,
		pAwaitNode,
		NULL,
		ppInnerStmt,
		piInnerStmtIndex
	);
}

static int __xlFindAsyncLinearBlockAwaitStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
)
{
	const XL_MirNode* pInnerStmt;
	size_t i;
	size_t iInnerStmtIndex;

	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = NULL;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = 0;
	}
	if ( (pCtx == NULL) || (pBlock == NULL) || (pBlock->eKind != XL_MIR_BLOCK) || (pAwaitNode == NULL) ) {
		return 0;
	}
	if ( pAwaitNodeAlt != NULL ) {
		if ( __xlMirCountAwaitExpr(pBlock) != 2 ) {
			return 0;
		}
	} else if ( (__xlMirCountAwaitExpr(pBlock) != 1) || (__xlMirFindFirstAwaitExpr(pBlock) != pAwaitNode) ) {
		return 0;
	}

	pInnerStmt = NULL;
	iInnerStmtIndex = 0;
	for ( i = 0; i < pBlock->iChildCount; ++i ) {
		const XL_MirNode* pChild = pBlock->ppChildren[i];

		if ( pChild == NULL ) {
			return 0;
		}
		if ( __xlMirContainsAwaitExpr(pChild) ) {
			if ( (pInnerStmt != NULL) ||
				!__xlCanAsyncLinearResolveStmtEx(pCtx, pChild, pAwaitNode, pAwaitNodeAlt) ) {
				return 0;
			}
			pInnerStmt = pChild;
			iInnerStmtIndex = i;
			continue;
		}
		if ( (__xlMirCountKind(pChild, XL_MIR_RETURN) != 0) ||
			(__xlMirCountKind(pChild, XL_MIR_BREAK) != 0) ||
			(__xlMirCountKind(pChild, XL_MIR_CONTINUE) != 0) ) {
			return 0;
		}
		if ( (pInnerStmt == NULL) && (pChild->eKind == XL_MIR_BIND) ) {
			return 0;
		}
	}
	if ( pInnerStmt == NULL ) {
		return 0;
	}
	if ( ppInnerStmt != NULL ) {
		*ppInnerStmt = pInnerStmt;
	}
	if ( piInnerStmtIndex != NULL ) {
		*piInnerStmtIndex = iInnerStmtIndex;
	}
	return 1;
}

static int __xlFindAsyncLinearBlockAwaitStmt(
	XL_CgenContext* pCtx,
	const XL_MirNode* pBlock,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppInnerStmt,
	size_t* piInnerStmtIndex
)
{
	return __xlFindAsyncLinearBlockAwaitStmtEx(
		pCtx,
		pBlock,
		pAwaitNode,
		NULL,
		ppInnerStmt,
		piInnerStmtIndex
	);
}

static const XL_MirNode* __xlSelectAsyncLinearBranchDualAwaitBlock(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode** ppPrimaryAwaitNode,
	const XL_MirNode** ppAltAwaitNode,
	int* pbPrimaryOnTrue
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	const XL_MirNode* pSelectedBlock;
	int bCondValue;

	if ( ppPrimaryAwaitNode != NULL ) {
		*ppPrimaryAwaitNode = NULL;
	}
	if ( ppAltAwaitNode != NULL ) {
		*ppAltAwaitNode = NULL;
	}
	if ( pbPrimaryOnTrue != NULL ) {
		*pbPrimaryOnTrue = 0;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) || (pStmt->eKind != XL_MIR_BRANCH) ) {
		return NULL;
	}

	pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	pThenBlock = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
	pElseBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
	if ( __xlMirContainsAwaitExpr(pCond) || !__xlMirTryConstBoolValue(pCond, &bCondValue) ) {
		return NULL;
	}

	pSelectedBlock = bCondValue ? pThenBlock : pElseBlock;
	if ( (pSelectedBlock == NULL) ||
		(pSelectedBlock->eKind != XL_MIR_BLOCK) ||
		(__xlMirCountAwaitExpr(pSelectedBlock) != 2) ||
		!__xlDiscoverAsyncLinearDualAwaitStep(pCtx, pSelectedBlock, ppPrimaryAwaitNode, ppAltAwaitNode, pbPrimaryOnTrue) ) {
		return NULL;
	}
	return pSelectedBlock;
}

static const XL_MirNode* __xlSelectAsyncLinearBranchAwaitBlock(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	const XL_MirNode* pSelectedBlock;
	int bCondValue;

	if ( (pCtx == NULL) || (pStmt == NULL) || (pStmt->eKind != XL_MIR_BRANCH) || (pAwaitNode == NULL) ) {
		return NULL;
	}

	pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	pThenBlock = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
	pElseBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
	if ( __xlMirContainsAwaitExpr(pCond) || !__xlMirTryConstBoolValue(pCond, &bCondValue) ) {
		return NULL;
	}

	pSelectedBlock = bCondValue ? pThenBlock : pElseBlock;
	if ( (pSelectedBlock == NULL) || (pSelectedBlock->eKind != XL_MIR_BLOCK) ) {
		return NULL;
	}
	if ( (__xlMirCountAwaitExpr(pSelectedBlock) != 1) || (__xlMirFindFirstAwaitExpr(pSelectedBlock) != pAwaitNode) ) {
		return NULL;
	}
	if ( !__xlFindAsyncLinearBlockAwaitStmt(pCtx, pSelectedBlock, pAwaitNode, NULL, NULL) ) {
		return NULL;
	}
	return pSelectedBlock;
}

static int __xlAnalyzeAsyncLinearDynamicBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode** ppAwaitBlock,
	const XL_MirNode** ppSyncBlock,
	int* pbAwaitOnTrue
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	const XL_MirNode* pAwaitBlock;
	const XL_MirNode* pSyncBlock;
	int bAwaitOnTrue;

	if ( ppAwaitBlock != NULL ) {
		*ppAwaitBlock = NULL;
	}
	if ( ppSyncBlock != NULL ) {
		*ppSyncBlock = NULL;
	}
	if ( pbAwaitOnTrue != NULL ) {
		*pbAwaitOnTrue = 0;
	}
	if ( (pCtx == NULL) || (pStmt == NULL) || (pStmt->eKind != XL_MIR_BRANCH) || (pAwaitNode == NULL) ) {
		return 0;
	}

	pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	pThenBlock = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
	pElseBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
	if ( __xlMirContainsAwaitExpr(pCond) ) {
		return 0;
	}
	if ( (pThenBlock != NULL) && (__xlMirContainsAwaitExpr(pThenBlock)) ) {
		pAwaitBlock = pThenBlock;
		pSyncBlock = pElseBlock;
		bAwaitOnTrue = 1;
	} else if ( (pElseBlock != NULL) && (__xlMirContainsAwaitExpr(pElseBlock)) ) {
		pAwaitBlock = pElseBlock;
		pSyncBlock = pThenBlock;
		bAwaitOnTrue = 0;
	} else {
		return 0;
	}

	if ( (__xlMirCountAwaitExpr(pAwaitBlock) != 1) ||
		(__xlMirFindFirstAwaitExpr(pAwaitBlock) != pAwaitNode) ||
		!__xlFindAsyncLinearBlockAwaitStmt(pCtx, pAwaitBlock, pAwaitNode, NULL, NULL) ) {
		return 0;
	}
	if ( __xlAnalyzeAsyncLinearDualBranchAwait(pCtx, pStmt, NULL, NULL, NULL, NULL) ) {
		return 0;
	}
	if ( pSyncBlock != NULL ) {
		if ( __xlMirContainsAwaitExpr(pSyncBlock) ||
			(__xlMirCountKind(pSyncBlock, XL_MIR_RETURN) != 0) ||
			(__xlMirCountKind(pSyncBlock, XL_MIR_BREAK) != 0) ||
			(__xlMirCountKind(pSyncBlock, XL_MIR_CONTINUE) != 0) ) {
			return 0;
		}
	}

	if ( ppAwaitBlock != NULL ) {
		*ppAwaitBlock = pAwaitBlock;
	}
	if ( ppSyncBlock != NULL ) {
		*ppSyncBlock = pSyncBlock;
	}
	if ( pbAwaitOnTrue != NULL ) {
		*pbAwaitOnTrue = bAwaitOnTrue;
	}
	return 1;
}

static int __xlCanAsyncLinearResolveStmtEx(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	const XL_MirNode* pBody;

	if ( (pCtx == NULL) || (pStmt == NULL) || (pAwaitNode == NULL) ) {
		return 0;
	}

	if ( pStmt->eKind == XL_MIR_BRANCH ) {
		pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		pThenBlock = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
		pElseBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
		if ( pAwaitNodeAlt != NULL ) {
			if ( __xlSelectAsyncLinearBranchDualAwaitBlock(pCtx, pStmt, NULL, NULL, NULL) != NULL ) {
				return 1;
			}
			return __xlAnalyzeAsyncLinearDualAwaitStep(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, NULL);
		}
		if ( (__xlMirCountAwaitExpr(pCond) != 1) ||
			(__xlMirFindFirstAwaitExpr(pCond) != pAwaitNode) ||
			!__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pCond, pAwaitNode) ||
			__xlMirContainsAwaitExpr(pThenBlock) ||
			__xlMirContainsAwaitExpr(pElseBlock) ) {
			if ( __xlSelectAsyncLinearBranchAwaitBlock(pCtx, pStmt, pAwaitNode) != NULL ) {
				return 1;
			}
			if ( __xlAnalyzeAsyncLinearDualBranchAwait(pCtx, pStmt, NULL, NULL, NULL, NULL) ) {
				return 1;
			}
			return __xlAnalyzeAsyncLinearDynamicBranchAwait(pCtx, pStmt, pAwaitNode, NULL, NULL, NULL);
		}
		return 1;
	}
	if ( pStmt->eKind == XL_MIR_BLOCK ) {
		return __xlFindAsyncLinearBlockAwaitStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, NULL, NULL);
	}
	if ( pStmt->eKind == XL_MIR_WHILE ) {
		if ( pAwaitNodeAlt != NULL ) {
			return 0;
		}
		pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		pBody = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
		if ( (__xlMirCountAwaitExpr(pCond) != 1) ||
			(__xlMirFindFirstAwaitExpr(pCond) != pAwaitNode) ||
			!__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pCond, pAwaitNode) ||
			__xlMirContainsAwaitExpr(pBody) ||
			(__xlMirCountKind(pBody, XL_MIR_RETURN) != 0) ) {
			return 0;
		}
		return 1;
	}
	if ( pStmt->eKind == XL_MIR_TRY ) {
		if ( __xlCanAsyncLinearResolveTryCatchStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, NULL, NULL, NULL, NULL, NULL) ) {
			return 1;
		}
		return __xlCanAsyncLinearResolveTryFinallyStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, NULL, NULL, NULL);
	}

	if ( pAwaitNodeAlt != NULL ) {
		return __xlAnalyzeAsyncLinearDualAwaitStep(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, NULL);
	}
	return __xlCanAsyncLinearReplaceAwaitExpr(pCtx, pStmt, pAwaitNode);
}

static int __xlCanAsyncLinearResolveStmt(XL_CgenContext* pCtx, const XL_MirNode* pStmt, const XL_MirNode* pAwaitNode)
{
	return __xlCanAsyncLinearResolveStmtEx(pCtx, pStmt, pAwaitNode, NULL);
}

static int __xlCollectAsyncLinearResumeCandidate(
	XL_CgenContext* pCtx,
	const XL_MirNode* pBody,
	XL_CgenAsyncLinearResumeCandidate* pCandidate
)
{
	size_t i;
	XL_CgenAsyncLinearResumeStep* pCurrentStep;

	if ( pCandidate != NULL ) {
		memset(pCandidate, 0, sizeof(*pCandidate));
	}
	if ( (pBody == NULL) || (pBody->eKind != XL_MIR_BLOCK) || (pBody->iChildCount < 2) || (pCandidate == NULL) ) {
		return 0;
	}

	pCurrentStep = NULL;
	for ( i = 0; i < pBody->iChildCount; ++i ) {
		const XL_MirNode* pStmt = pBody->ppChildren[i];

		if ( pStmt == NULL ) {
			return 0;
		}
		if ( pStmt->eKind == XL_MIR_BIND ) {
			const XL_MirNode* pInit = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
			const XL_MirNode* pAwaitInner = __xlAwaitInnerExpr(pInit);
			int iAwaitCount;

			if ( pAwaitInner != NULL ) {
				if ( (i + 1) >= pBody->iChildCount || (pCandidate->iStepCount >= XL_COUNTOF(pCandidate->arrSteps)) ) {
					return 0;
				}
				pCurrentStep = &pCandidate->arrSteps[pCandidate->iStepCount++];
				memset(pCurrentStep, 0, sizeof(*pCurrentStep));
				pCurrentStep->pAwaitBind = pStmt;
				pCurrentStep->pAwaitInner = pAwaitInner;
				pCurrentStep->pAwaitNode = pInit;
				pCurrentStep->iAwaitStmtIndex = i;
				continue;
			}
			iAwaitCount = __xlMirCountAwaitExpr(pInit);
			if ( iAwaitCount == 1 ) {
				const XL_MirNode* pAwaitNode = __xlMirFindFirstAwaitExpr(pInit);

				if ( (i + 1) >= pBody->iChildCount ||
					(pAwaitNode == NULL) ||
					(pCandidate->iStepCount >= XL_COUNTOF(pCandidate->arrSteps)) ||
					!__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pInit, pAwaitNode) ) {
					return 0;
				}
				pCurrentStep = &pCandidate->arrSteps[pCandidate->iStepCount++];
				memset(pCurrentStep, 0, sizeof(*pCurrentStep));
				pCurrentStep->pAwaitBind = pStmt;
				pCurrentStep->pAwaitInner = __xlAwaitInnerExpr(pAwaitNode);
				pCurrentStep->pAwaitNode = pAwaitNode;
				pCurrentStep->iAwaitStmtIndex = i;
				pCurrentStep->bExprAwait = 1;
				continue;
			}
			if ( iAwaitCount > 1 ) {
				return 0;
			}
			if ( pCandidate->iStepCount == 0 ) {
				if ( pCandidate->iPreBindCount >= XL_COUNTOF(pCandidate->arrPreBinds) ) {
					return 0;
				}
				pCandidate->arrPreBinds[pCandidate->iPreBindCount++] = pStmt;
			} else {
				if ( (pCurrentStep == NULL) || (pCurrentStep->iPostBindCount >= XL_COUNTOF(pCurrentStep->arrPostBinds)) ) {
					return 0;
				}
				pCurrentStep->arrPostBinds[pCurrentStep->iPostBindCount++] = pStmt;
			}
			continue;
		}
		if ( pStmt->eKind == XL_MIR_RETURN ) {
			const XL_MirNode* pExpr = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
			int iAwaitCount;

			if ( (i + 1) != pBody->iChildCount ) {
				return 0;
			}
			if ( pStmt->iChildCount != 1 ) {
				return 0;
			}
			iAwaitCount = __xlMirCountAwaitExpr(pExpr);
			if ( iAwaitCount == 0 ) {
				if ( pCandidate->iStepCount == 0 ) {
					return 0;
				}
			} else if ( iAwaitCount == 1 ) {
				const XL_MirNode* pAwaitNode = __xlMirFindFirstAwaitExpr(pExpr);

				if ( (pAwaitNode == NULL) ||
					(pCandidate->iStepCount >= XL_COUNTOF(pCandidate->arrSteps)) ||
					!__xlCanAsyncLinearReplaceAwaitExpr(pCtx, pExpr, pAwaitNode) ) {
					return 0;
				}
				pCurrentStep = &pCandidate->arrSteps[pCandidate->iStepCount++];
				memset(pCurrentStep, 0, sizeof(*pCurrentStep));
				pCurrentStep->pAwaitInner = __xlAwaitInnerExpr(pAwaitNode);
				pCurrentStep->pAwaitNode = pAwaitNode;
				pCurrentStep->iAwaitStmtIndex = i;
				pCurrentStep->bReturnExprAwait = 1;
			} else {
				return 0;
			}
			pCandidate->pReturn = pStmt;
			return 1;
		}
		if ( __xlMirCountAwaitExpr(pStmt) == 0 ) {
			if ( (pCandidate->iStepCount == 0) ||
				(pCurrentStep == NULL) ||
				(pCurrentStep->iPostStmtCount >= XL_COUNTOF(pCurrentStep->arrPostStmts)) ) {
				return 0;
			}
			pCurrentStep->arrPostStmts[pCurrentStep->iPostStmtCount++] = pStmt;
			continue;
		}
		if ( __xlMirCountAwaitExpr(pStmt) == 1 ) {
			const XL_MirNode* pAwaitNode = __xlMirFindFirstAwaitExpr(pStmt);

			if ( (i + 1) >= pBody->iChildCount ||
				(pAwaitNode == NULL) ||
				(pCandidate->iStepCount >= XL_COUNTOF(pCandidate->arrSteps)) ||
				!__xlCanAsyncLinearResolveStmt(pCtx, pStmt, pAwaitNode) ) {
				return 0;
			}
			pCurrentStep = &pCandidate->arrSteps[pCandidate->iStepCount++];
			memset(pCurrentStep, 0, sizeof(*pCurrentStep));
			pCurrentStep->pAwaitStmt = pStmt;
			pCurrentStep->pAwaitInner = __xlAwaitInnerExpr(pAwaitNode);
			pCurrentStep->pAwaitNode = pAwaitNode;
			pCurrentStep->iAwaitStmtIndex = i;
			pCurrentStep->bExprAwait = 1;
			continue;
		}
		if ( (pStmt->eKind == XL_MIR_BRANCH) && (__xlMirCountAwaitExpr(pStmt) == 2) ) {
			const XL_MirNode* pConstAwaitBlock;
			const XL_MirNode* pThenAwaitNode;
			const XL_MirNode* pElseAwaitNode;

			if ( (i + 1) >= pBody->iChildCount ||
				(pCandidate->iStepCount >= XL_COUNTOF(pCandidate->arrSteps)) ) {
				return 0;
			}
			pConstAwaitBlock = __xlSelectAsyncLinearBranchDualAwaitBlock(pCtx, pStmt, &pThenAwaitNode, &pElseAwaitNode, NULL);
			if ( (pConstAwaitBlock == NULL) &&
				!__xlAnalyzeAsyncLinearDualBranchAwait(pCtx, pStmt, NULL, NULL, &pThenAwaitNode, &pElseAwaitNode) ) {
				return 0;
			}
			pCurrentStep = &pCandidate->arrSteps[pCandidate->iStepCount++];
			memset(pCurrentStep, 0, sizeof(*pCurrentStep));
			pCurrentStep->pAwaitStmt = (pConstAwaitBlock != NULL) ? pConstAwaitBlock : pStmt;
			pCurrentStep->pAwaitInner = __xlAwaitInnerExpr(pThenAwaitNode);
			pCurrentStep->pAwaitNode = pThenAwaitNode;
			pCurrentStep->pAwaitInnerAlt = __xlAwaitInnerExpr(pElseAwaitNode);
			pCurrentStep->pAwaitNodeAlt = pElseAwaitNode;
			pCurrentStep->iAwaitStmtIndex = i;
			pCurrentStep->bExprAwait = 1;
			continue;
		}
		if ( (pStmt->eKind == XL_MIR_TRY) && (__xlMirCountAwaitExpr(pStmt) == 2) ) {
			const XL_MirNode* pPrimaryAwaitNode;
			const XL_MirNode* pAltAwaitNode;

			if ( (i + 1) >= pBody->iChildCount ||
				(pCandidate->iStepCount >= XL_COUNTOF(pCandidate->arrSteps)) ||
				!__xlDiscoverAsyncLinearDualAwaitStep(pCtx, pStmt, &pPrimaryAwaitNode, &pAltAwaitNode, NULL) ||
				(!__xlCanAsyncLinearResolveTryCatchStmtEx(pCtx, pStmt, pPrimaryAwaitNode, pAltAwaitNode, NULL, NULL, NULL, NULL, NULL) &&
				 !__xlCanAsyncLinearResolveTryFinallyStmtEx(pCtx, pStmt, pPrimaryAwaitNode, pAltAwaitNode, NULL, NULL, NULL)) ) {
				return 0;
			}
			pCurrentStep = &pCandidate->arrSteps[pCandidate->iStepCount++];
			memset(pCurrentStep, 0, sizeof(*pCurrentStep));
			pCurrentStep->pAwaitStmt = pStmt;
			pCurrentStep->pAwaitInner = __xlAwaitInnerExpr(pPrimaryAwaitNode);
			pCurrentStep->pAwaitNode = pPrimaryAwaitNode;
			pCurrentStep->pAwaitInnerAlt = __xlAwaitInnerExpr(pAltAwaitNode);
			pCurrentStep->pAwaitNodeAlt = pAltAwaitNode;
			pCurrentStep->iAwaitStmtIndex = i;
			pCurrentStep->bExprAwait = 1;
			continue;
		}
		return 0;
	}

	return 0;
}

static const XL_HybridSmFunc* __xlCgenFindHybridFunc(const XL_CgenContext* pCtx, const XL_MirNode* pFunc)
{
	const XL_HybridSm* pSm;
	const XL_HybridSmFunc* pNameMatch;
	size_t iNameMatchCount;
	size_t i;

	if ( (pCtx == NULL) || (pCtx->pLowerResult == NULL) || (pFunc == NULL) ) {
		return NULL;
	}

	pSm = &pCtx->pLowerResult->hybridSm;
	pNameMatch = NULL;
	iNameMatchCount = 0;
	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		const XL_HybridSmFunc* pHybridFunc = &pSm->pFuncs[i];

		if ( (pFunc->sAbiName != NULL) && (pFunc->sAbiName[0] != '\0') ) {
			if ( strcmp((pHybridFunc->sAbiName != NULL) ? pHybridFunc->sAbiName : "", pFunc->sAbiName) == 0 ) {
				return pHybridFunc;
			}
		}
		if ( pHybridFunc->iLine != pFunc->iLine || pHybridFunc->iColumn != pFunc->iColumn ) {
			if ( strcmp((pHybridFunc->sFuncName != NULL) ? pHybridFunc->sFuncName : "", (pFunc->sText != NULL) ? pFunc->sText : "") == 0 ) {
				pNameMatch = pHybridFunc;
				++iNameMatchCount;
			}
			continue;
		}
		if ( strcmp((pHybridFunc->sFuncName != NULL) ? pHybridFunc->sFuncName : "", (pFunc->sText != NULL) ? pFunc->sText : "") != 0 ) {
			continue;
		}
		return pHybridFunc;
	}
	if ( iNameMatchCount == 1 ) {
		return pNameMatch;
	}
	return NULL;
}

static const XL_GeneratorSmFunc* __xlCgenFindGeneratorFunc(const XL_CgenContext* pCtx, const XL_MirNode* pFunc)
{
	const XL_GeneratorSm* pSm;
	const XL_GeneratorSmFunc* pNameMatch;
	size_t iNameMatchCount;
	size_t i;

	if ( (pCtx == NULL) || (pCtx->pLowerResult == NULL) || (pFunc == NULL) ) {
		return NULL;
	}

	pSm = &pCtx->pLowerResult->generatorSm;
	pNameMatch = NULL;
	iNameMatchCount = 0;
	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		const XL_GeneratorSmFunc* pGeneratorFunc = &pSm->pFuncs[i];

		if ( (pFunc->sAbiName != NULL) && (pFunc->sAbiName[0] != '\0') ) {
			if ( strcmp((pGeneratorFunc->sAbiName != NULL) ? pGeneratorFunc->sAbiName : "", pFunc->sAbiName) == 0 ) {
				return pGeneratorFunc;
			}
		}
		if ( pGeneratorFunc->iLine != pFunc->iLine || pGeneratorFunc->iColumn != pFunc->iColumn ) {
			if ( strcmp((pGeneratorFunc->sFuncName != NULL) ? pGeneratorFunc->sFuncName : "", (pFunc->sText != NULL) ? pFunc->sText : "") == 0 ) {
				pNameMatch = pGeneratorFunc;
				++iNameMatchCount;
			}
			continue;
		}
		if ( strcmp((pGeneratorFunc->sFuncName != NULL) ? pGeneratorFunc->sFuncName : "", (pFunc->sText != NULL) ? pFunc->sText : "") != 0 ) {
			continue;
		}
		return pGeneratorFunc;
	}
	if ( iNameMatchCount == 1 ) {
		return pNameMatch;
	}
	return NULL;
}

static const XL_AsyncSmFunc* __xlCgenFindAsyncFunc(const XL_CgenContext* pCtx, const XL_MirNode* pFunc)
{
	const XL_AsyncSm* pSm;
	const XL_AsyncSmFunc* pNameMatch;
	size_t iNameMatchCount;
	size_t i;

	if ( (pCtx == NULL) || (pCtx->pLowerResult == NULL) || (pFunc == NULL) ) {
		return NULL;
	}

	pSm = &pCtx->pLowerResult->asyncSm;
	pNameMatch = NULL;
	iNameMatchCount = 0;
	for ( i = 0; i < pSm->iFuncCount; ++i ) {
		const XL_AsyncSmFunc* pAsyncFunc = &pSm->pFuncs[i];

		if ( (pFunc->sAbiName != NULL) && (pFunc->sAbiName[0] != '\0') ) {
			if ( strcmp((pAsyncFunc->sAbiName != NULL) ? pAsyncFunc->sAbiName : "", pFunc->sAbiName) == 0 ) {
				return pAsyncFunc;
			}
		}
		if ( pAsyncFunc->iLine != pFunc->iLine || pAsyncFunc->iColumn != pFunc->iColumn ) {
			if ( strcmp((pAsyncFunc->sFuncName != NULL) ? pAsyncFunc->sFuncName : "", (pFunc->sText != NULL) ? pFunc->sText : "") == 0 ) {
				pNameMatch = pAsyncFunc;
				++iNameMatchCount;
			}
			continue;
		}
		if ( strcmp((pAsyncFunc->sFuncName != NULL) ? pAsyncFunc->sFuncName : "", (pFunc->sText != NULL) ? pFunc->sText : "") != 0 ) {
			continue;
		}
		return pAsyncFunc;
	}
	if ( iNameMatchCount == 1 ) {
		return pNameMatch;
	}
	return NULL;
}

static int __xlCgenFindAsyncStateId(const XL_AsyncSmFunc* pFunc, const XL_MirNode* pNode, const char* sLabel)
{
	size_t i;
	int iBestStateId;
	int iBestColumn;

	if ( (pFunc == NULL) || (pNode == NULL) || (sLabel == NULL) ) {
		return -1;
	}

	iBestStateId = -1;
	iBestColumn = -1;
	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		const XL_AsyncSmState* pState = &pFunc->pStates[i];

		if ( strcmp((pState->sLabel != NULL) ? pState->sLabel : "", sLabel) != 0 ) {
			continue;
		}
		if ( pState->iLine != pNode->iLine ) {
			continue;
		}
		if ( pState->iColumn > pNode->iColumn ) {
			continue;
		}
		if ( pState->iColumn >= iBestColumn ) {
			iBestColumn = pState->iColumn;
			iBestStateId = pState->iStateId;
		}
	}
	return iBestStateId;
}

static int __xlCgenFindGeneratorStateId(const XL_GeneratorSmFunc* pFunc, const XL_MirNode* pNode)
{
	size_t i;
	int iBestStateId;
	int iBestColumn;

	if ( (pFunc == NULL) || (pNode == NULL) ) {
		return -1;
	}

	iBestStateId = -1;
	iBestColumn = -1;
	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		const XL_GeneratorSmState* pState = &pFunc->pStates[i];

		if ( !pState->bSuspend ) {
			continue;
		}
		if ( pState->iLine != pNode->iLine ) {
			continue;
		}
		if ( pState->iColumn > pNode->iColumn ) {
			continue;
		}
		if ( pState->iColumn >= iBestColumn ) {
			iBestColumn = pState->iColumn;
			iBestStateId = pState->iStateId;
		}
	}
	return iBestStateId;
}

static int __xlCgenCountGeneratorSuspendStates(const XL_GeneratorSmFunc* pFunc)
{
	size_t i;
	int iCount;

	if ( pFunc == NULL ) {
		return 0;
	}

	iCount = 0;
	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		if ( pFunc->pStates[i].bSuspend ) {
			iCount += 1;
		}
	}
	return iCount;
}

static int __xlCgenFindGeneratorSuspendStateIdByIndex(const XL_GeneratorSmFunc* pFunc, int iSuspendIndex)
{
	size_t i;
	int iCurrentIndex;

	if ( (pFunc == NULL) || (iSuspendIndex < 0) ) {
		return -1;
	}

	iCurrentIndex = 0;
	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		const XL_GeneratorSmState* pState = &pFunc->pStates[i];

		if ( !pState->bSuspend || (pState->iStateId <= 0) ) {
			continue;
		}
		if ( iCurrentIndex == iSuspendIndex ) {
			return pState->iStateId;
		}
		iCurrentIndex += 1;
	}
	return -1;
}

static int __xlCgenFindFirstGeneratorSuspendStateId(const XL_GeneratorSmFunc* pFunc)
{
	return __xlCgenFindGeneratorSuspendStateIdByIndex(pFunc, 0);
}

static int __xlCgenFindHybridGeneratorStateId(const XL_HybridSmFunc* pFunc, const XL_MirNode* pNode);

static void __xlCgenConsumeGeneratorSuspendState(
	const XL_GeneratorSmFunc* pFunc,
	int* pSuspendIndex,
	int iStateId
)
{
	size_t i;
	int iCurrentIndex;

	if ( (pFunc == NULL) || (pSuspendIndex == NULL) || (iStateId <= 0) ) {
		return;
	}

	iCurrentIndex = 0;
	for ( i = 0; i < pFunc->iStateCount; ++i ) {
		const XL_GeneratorSmState* pState = &pFunc->pStates[i];

		if ( !pState->bSuspend || (pState->iStateId <= 0) ) {
			continue;
		}
		if ( pState->iStateId == iStateId ) {
			*pSuspendIndex = iCurrentIndex + 1;
			return;
		}
		iCurrentIndex += 1;
	}
}

static int __xlCgenResolveGeneratorResumeState(
	const XL_CgenFuncInfo* pFuncInfo,
	const XL_MirNode* pNode,
	int iResumeState,
	int bExternalResumeState,
	int bFinalizeFallback
)
{
	if ( pFuncInfo == NULL ) {
		return iResumeState;
	}
	if ( !bExternalResumeState && (pFuncInfo->pHybridFunc != NULL) ) {
		iResumeState = __xlCgenFindHybridGeneratorStateId(pFuncInfo->pHybridFunc, pNode);
	}
	if ( !bExternalResumeState && (iResumeState <= 0) && (pFuncInfo->pGeneratorFunc != NULL) ) {
		iResumeState = __xlCgenFindGeneratorStateId(pFuncInfo->pGeneratorFunc, pNode);
	}
	if ( !bExternalResumeState &&
		(iResumeState <= 0) &&
		(pFuncInfo->pGeneratorFunc != NULL) &&
		(pFuncInfo->pGeneratorSuspendIndex != NULL) ) {
		iResumeState = __xlCgenFindGeneratorSuspendStateIdByIndex(
			pFuncInfo->pGeneratorFunc,
			*pFuncInfo->pGeneratorSuspendIndex
		);
		if ( iResumeState > 0 ) {
			*pFuncInfo->pGeneratorSuspendIndex += 1;
		}
	}
	if ( !bExternalResumeState &&
		(iResumeState <= 0) &&
		(pFuncInfo->pGeneratorFunc != NULL) &&
		(pFuncInfo->pGeneratorSuspendIndex != NULL) ) {
		if ( iResumeState > 0 ) {
			__xlCgenConsumeGeneratorSuspendState(
				pFuncInfo->pGeneratorFunc,
				pFuncInfo->pGeneratorSuspendIndex,
				iResumeState
			);
		}
	}
	if ( bFinalizeFallback && (iResumeState <= 0) ) {
		iResumeState = __xlCgenFindFirstGeneratorSuspendStateId(pFuncInfo->pGeneratorFunc);
	}
	return iResumeState;
}

static int __xlCgenFindHybridGeneratorStateId(const XL_HybridSmFunc* pFunc, const XL_MirNode* pNode)
{
	size_t i;

	if ( (pFunc == NULL) || (pNode == NULL) ) {
		return 0;
	}
	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		const XL_HybridSmPoint* pPoint = &pFunc->pPoints[i];

		if ( !pPoint->bYield ) {
			continue;
		}
		if ( pPoint->iLine != pNode->iLine || pPoint->iColumn != pNode->iColumn ) {
			continue;
		}
		if ( pPoint->iGeneratorStateId > 0 ) {
			return pPoint->iGeneratorStateId;
		}
	}
	return 0;
}

static int __xlCgenFindHybridProjectedGeneratorStateId(const XL_HybridSmFunc* pFunc, const XL_MirNode* pNode)
{
	size_t i;
	int iBestStateId;
	int iBestColumn;

	if ( (pFunc == NULL) || (pNode == NULL) ) {
		return 0;
	}
	iBestStateId = 0;
	iBestColumn = -1;
	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		const XL_HybridSmPoint* pPoint = &pFunc->pPoints[i];

		if ( !pPoint->bAwait ) {
			continue;
		}
		if ( pPoint->iProjectedGeneratorStateId <= 0 ) {
			continue;
		}
		if ( pPoint->iLine == pNode->iLine && pPoint->iColumn == pNode->iColumn ) {
			return pPoint->iProjectedGeneratorStateId;
		}
		if ( pPoint->iLine != pNode->iLine ) {
			continue;
		}
		if ( pPoint->iColumn > pNode->iColumn ) {
			continue;
		}
		if ( pPoint->iColumn >= iBestColumn ) {
			iBestColumn = pPoint->iColumn;
			iBestStateId = pPoint->iProjectedGeneratorStateId;
		}
	}
	return iBestStateId;
}

static const XL_HybridSmPoint* __xlCgenFindHybridPointByGeneratorStateId(const XL_HybridSmFunc* pFunc, int iGeneratorStateId)
{
	size_t i;

	if ( (pFunc == NULL) || (iGeneratorStateId <= 0) ) {
		return NULL;
	}
	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		const XL_HybridSmPoint* pPoint = &pFunc->pPoints[i];

		if ( !pPoint->bYield ) {
			continue;
		}
		if ( pPoint->iGeneratorStateId == iGeneratorStateId ) {
			return pPoint;
		}
	}
	return NULL;
}

static int __xlCgenCountHybridAwaitsInGroup(const XL_HybridSmFunc* pFunc, int iProjectionGroupId)
{
	size_t i;
	int iCount;

	if ( (pFunc == NULL) || (iProjectionGroupId < 0) ) {
		return 0;
	}
	iCount = 0;
	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		const XL_HybridSmPoint* pPoint = &pFunc->pPoints[i];

		if ( !pPoint->bAwait ) {
			continue;
		}
		if ( pPoint->iProjectionGroupId == iProjectionGroupId ) {
			iCount += 1;
		}
	}
	return iCount;
}

static void __xlCgenAppendHybridGroupTraceEvent(
	XL_CgenContext* pCtx,
	const XL_CgenFuncInfo* pFuncInfo,
	int iGeneratorStateId,
	const char* sPhase
)
{
	const XL_HybridSmPoint* pHybridPoint;

	if ( (pCtx == NULL) || (pFuncInfo == NULL) || (pFuncInfo->pHybridFunc == NULL) || (sPhase == NULL) ) {
		return;
	}
	pHybridPoint = __xlCgenFindHybridPointByGeneratorStateId(pFuncInfo->pHybridFunc, iGeneratorStateId);
	if ( pHybridPoint == NULL ) {
		return;
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"xl_generated_generator_trace_event(\"%s_%d\", ",
		sPhase,
		pHybridPoint->iProjectionGroupId
	);
	__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
	__xlCgenAppend(&pCtx->buf, ");\n");
}

static void __xlCgenAppendHybridGroupTraceEventInline(
	XL_CgenContext* pCtx,
	const XL_CgenFuncInfo* pFuncInfo,
	int iGeneratorStateId,
	const char* sPhase
)
{
	const XL_HybridSmPoint* pHybridPoint;

	if ( (pCtx == NULL) || (pFuncInfo == NULL) || (pFuncInfo->pHybridFunc == NULL) || (sPhase == NULL) ) {
		return;
	}
	pHybridPoint = __xlCgenFindHybridPointByGeneratorStateId(pFuncInfo->pHybridFunc, iGeneratorStateId);
	if ( pHybridPoint == NULL ) {
		return;
	}
	__xlCgenAppendFormat(
		&pCtx->buf,
		"xl_generated_generator_trace_event(\"%s_%d\", ",
		sPhase,
		pHybridPoint->iProjectionGroupId
	);
	__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
	__xlCgenAppend(&pCtx->buf, "); ");
}

static int __xlCgenCountHybridYieldPoints(const XL_HybridSmFunc* pFunc)
{
	size_t i;
	int iCount;

	if ( pFunc == NULL ) {
		return 0;
	}

	iCount = 0;
	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		if ( pFunc->pPoints[i].bYield ) {
			iCount += 1;
		}
	}
	return iCount;
}

static int __xlCgenIsHybridProjectionComplete(const XL_HybridSmFunc* pFunc)
{
	size_t i;

	if ( pFunc == NULL ) {
		return 0;
	}

	for ( i = 0; i < pFunc->iPointCount; ++i ) {
		const XL_HybridSmPoint* pPoint = &pFunc->pPoints[i];

		if ( pPoint->bAwait ) {
			if ( (pPoint->iProjectionGroupId < 0) ||
				(pPoint->iProjectedGeneratorStateId <= 0) ||
				(pPoint->iProjectedPointIndex < 0) ||
				((size_t)pPoint->iProjectedPointIndex >= pFunc->iPointCount) ) {
				return 0;
			}
		}
		if ( pPoint->bYield ) {
			if ( (pPoint->iProjectionGroupId < 0) ||
				(pPoint->iGeneratorStateId <= 0) ||
				(pPoint->iProjectedGeneratorStateId != pPoint->iGeneratorStateId) ) {
				return 0;
			}
		}
	}

	return 1;
}

static void __xlCgenGeneratorTypeNameForFunc(char* sOut, size_t iOutSize, const XL_MirNode* pFunc)
{
	const char* sStem;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	sStem = ((pFunc != NULL) && (pFunc->sAbiName != NULL) && (pFunc->sAbiName[0] != '\0')) ? pFunc->sAbiName : NULL;
	if ( (sStem == NULL) || (sStem[0] == '\0') ) {
		sStem = (pFunc != NULL) ? pFunc->sText : "generator";
	}
	snprintf(sOut, iOutSize, "%s__generator_state", (sStem != NULL) ? sStem : "generator");
}

static void __xlCgenGeneratorNextFuncNameForFunc(char* sOut, size_t iOutSize, const XL_MirNode* pFunc)
{
	const char* sStem;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	sStem = ((pFunc != NULL) && (pFunc->sAbiName != NULL) && (pFunc->sAbiName[0] != '\0')) ? pFunc->sAbiName : NULL;
	if ( (sStem == NULL) || (sStem[0] == '\0') ) {
		sStem = (pFunc != NULL) ? pFunc->sText : "generator";
	}
	snprintf(sOut, iOutSize, "%s__generator_next", (sStem != NULL) ? sStem : "generator");
}

static int __xlCgenGeneratorFieldIndex(
	const XL_CgenGeneratorField* pFields,
	int iFieldCount,
	const char* sName
)
{
	int i;

	if ( sName == NULL ) {
		return -1;
	}

	for ( i = 0; i < iFieldCount; ++i ) {
		if ( (pFields[i].sName != NULL) && (strcmp(pFields[i].sName, sName) == 0) ) {
			return i;
		}
	}
	return -1;
}

static void __xlCgenCollectGeneratorBindFields(
	const XL_MirNode* pNode,
	XL_CgenGeneratorField* pFields,
	int iFieldCap,
	int* piFieldCount
)
{
	size_t i;

	if ( (pNode == NULL) || (pFields == NULL) || (piFieldCount == NULL) ) {
		return;
	}

	if ( (pNode->eKind == XL_MIR_BIND) &&
		(pNode->sText != NULL) &&
		(pNode->sText[0] != '\0') &&
		(__xlCgenGeneratorFieldIndex(pFields, *piFieldCount, pNode->sText) < 0) &&
		(*piFieldCount < iFieldCap) ) {
		pFields[*piFieldCount].sName = pNode->sText;
		pFields[*piFieldCount].sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
		*piFieldCount += 1;
	}
	if ( (pNode->eKind == XL_MIR_TRY) && (pNode->iChildCount > 1) ) {
		const XL_MirNode* pCatchName;

		pCatchName = pNode->ppChildren[1];
		if ( (pCatchName != NULL) &&
			(pCatchName->sText != NULL) &&
			(pCatchName->sText[0] != '\0') &&
			(__xlCgenGeneratorFieldIndex(pFields, *piFieldCount, pCatchName->sText) < 0) &&
			(*piFieldCount < iFieldCap) ) {
			pFields[*piFieldCount].sName = pCatchName->sText;
			pFields[*piFieldCount].sTypeName = ((pCatchName->sTypeName != NULL) && (pCatchName->sTypeName[0] != '\0')) ? pCatchName->sTypeName : "error";
			*piFieldCount += 1;
		}
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		__xlCgenCollectGeneratorBindFields(pNode->ppChildren[i], pFields, iFieldCap, piFieldCount);
	}
}

static int __xlCgenCollectGeneratorFields(
	const XL_MirNode* pFunc,
	XL_CgenGeneratorField* pFields,
	int iFieldCap
)
{
	const XL_MirNode* pParams;
	const XL_MirNode* pBody;
	int iFieldCount;
	size_t i;

	if ( (pFunc == NULL) || (pFields == NULL) || (iFieldCap <= 0) ) {
		return 0;
	}

	iFieldCount = 0;
	pParams = (pFunc->iChildCount != 0) ? pFunc->ppChildren[0] : NULL;
	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount) && (iFieldCount < iFieldCap); ++i ) {
		const XL_MirNode* pParam;

		pParam = pParams->ppChildren[i];
		if ( (pParam == NULL) || (pParam->sText == NULL) || (pParam->sText[0] == '\0') ) {
			continue;
		}
		if ( __xlCgenGeneratorFieldIndex(pFields, iFieldCount, pParam->sText) >= 0 ) {
			continue;
		}
		pFields[iFieldCount].sName = pParam->sText;
		pFields[iFieldCount].sTypeName = ((pParam->sTypeName != NULL) && (pParam->sTypeName[0] != '\0')) ? pParam->sTypeName : "int";
		iFieldCount += 1;
	}

	pBody = (pFunc->iChildCount > 1) ? pFunc->ppChildren[1] : NULL;
	__xlCgenCollectGeneratorBindFields(pBody, pFields, iFieldCap, &iFieldCount);
	return iFieldCount;
}

static void __xlCgenAppendGeneratorField(
	XL_CgenGeneratorField* pFields,
	int iFieldCap,
	int* piFieldCount,
	const char* sName,
	const char* sTypeName
)
{
	if ( (pFields == NULL) || (piFieldCount == NULL) || (sName == NULL) || (sTypeName == NULL) ) {
		return;
	}
	if ( (*piFieldCount < 0) || (*piFieldCount >= iFieldCap) ) {
		return;
	}
	if ( __xlCgenGeneratorFieldIndex(pFields, *piFieldCount, sName) >= 0 ) {
		return;
	}

	pFields[*piFieldCount].sName = sName;
	pFields[*piFieldCount].sTypeName = sTypeName;
	*piFieldCount += 1;
}

static int __xlCgenGeneratorTryFrameFieldIndex(
	const XL_CgenGeneratorTryFrameField* pFields,
	int iFieldCount,
	const char* sName
)
{
	int i;

	if ( (pFields == NULL) || (sName == NULL) ) {
		return -1;
	}

	for ( i = 0; i < iFieldCount; ++i ) {
		if ( strcmp(pFields[i].sName, sName) == 0 ) {
			return i;
		}
	}

	return -1;
}

static void __xlCgenCollectGeneratorTryFrameFieldsNode(
	const XL_MirNode* pNode,
	XL_CgenGeneratorTryFrameField* pFields,
	int iFieldCap,
	int* piFieldCount
)
{
	size_t i;

	if ( (pNode == NULL) || (pFields == NULL) || (piFieldCount == NULL) ) {
		return;
	}

	if ( (pNode->eKind == XL_MIR_TRY) && (*piFieldCount < iFieldCap) ) {
		char sFieldName[64];

		snprintf(sFieldName, sizeof(sFieldName), "xl_try_frame_%d_%d", pNode->iLine, pNode->iColumn);
		if ( __xlCgenGeneratorTryFrameFieldIndex(pFields, *piFieldCount, sFieldName) < 0 ) {
			snprintf(pFields[*piFieldCount].sName, sizeof(pFields[*piFieldCount].sName), "%s", sFieldName);
			*piFieldCount += 1;
		}
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		__xlCgenCollectGeneratorTryFrameFieldsNode(pNode->ppChildren[i], pFields, iFieldCap, piFieldCount);
	}
}

static int __xlCgenCollectGeneratorTryFrameFields(
	const XL_MirNode* pFunc,
	XL_CgenGeneratorTryFrameField* pFields,
	int iFieldCap
)
{
	const XL_MirNode* pBody;
	int iFieldCount;

	if ( (pFunc == NULL) || (pFields == NULL) || (iFieldCap <= 0) ) {
		return 0;
	}

	iFieldCount = 0;
	pBody = (pFunc->iChildCount > 1) ? pFunc->ppChildren[1] : NULL;
	__xlCgenCollectGeneratorTryFrameFieldsNode(pBody, pFields, iFieldCap, &iFieldCount);
	return iFieldCount;
}

static void __xlCgenGeneratorFieldAliasLines(
	XL_CgenContext* pCtx,
	const XL_CgenGeneratorField* pFields,
	int iFieldCount,
	const char* sSelfName,
	int bDefine
)
{
	int i;

	if ( (pCtx == NULL) || (pFields == NULL) || (sSelfName == NULL) ) {
		return;
	}

	for ( i = 0; i < iFieldCount; ++i ) {
		if ( (pFields[i].sName == NULL) || (pFields[i].sName[0] == '\0') ) {
			continue;
		}
		__xlCgenAppendIndent(&pCtx->buf);
		if ( bDefine ) {
			__xlCgenAppendFormat(&pCtx->buf, "#define %s ((%s)->%s)\n", pFields[i].sName, sSelfName, pFields[i].sName);
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "#undef %s\n", pFields[i].sName);
		}
	}
}

static int __xlIsGeneratorFuncNode(const XL_MirNode* pNode)
{
	const XL_MirNode* pBody;

	if ( (pNode == NULL) || (pNode->eKind != XL_MIR_FUNC) ) {
		return 0;
	}

	pBody = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	return __xlMirContainsKind(pBody, XL_MIR_YIELD);
}

static const XL_MirNode* __xlFindEntryFunc(const XL_MirNode* pRoot)
{
	static const char* arrPreferred[] = { "入口", "main", "调用" };
	const XL_MirNode* pFunc;
	const XL_MirNode* pParams;
	size_t i;
	size_t j;

	if ( pRoot == NULL ) {
		return NULL;
	}

	for ( i = 0; i < XL_COUNTOF(arrPreferred); ++i ) {
		for ( j = 0; j < pRoot->iChildCount; ++j ) {
			pFunc = pRoot->ppChildren[j];
			if ( (pFunc == NULL) || (pFunc->eKind != XL_MIR_FUNC) || (pFunc->sText == NULL) ) {
				continue;
			}
			if ( strcmp(pFunc->sText, arrPreferred[i]) != 0 ) {
				continue;
			}

			pParams = (pFunc->iChildCount != 0) ? pFunc->ppChildren[0] : NULL;
			if ( (pParams == NULL) || (pParams->iChildCount == 0) ) {
				return pFunc;
			}
		}
	}

	return NULL;
}

static const char* __xlInferExprTypeName(const XL_CgenContext* pCtx, const XL_MirNode* pNode);
static const char* __xlMirFuncSymbolName(const XL_MirNode* pNode);
static int __xlResolveGeneratorSourceInfo(
	const XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode** ppFuncNode,
	const char** psValueTypeName,
	const char** psFuncName,
	int iDepth
);
static int __xlResolveGeneratorReturnSourceInfo(
	const XL_CgenContext* pCtx,
	const XL_MirNode* pFuncNode,
	const XL_MirNode** ppFuncNode,
	const char** psValueTypeName,
	const char** psFuncName,
	int iDepth
);
static void __xlCgenExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode);
static void __xlCgenBoxValueExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode);
static void __xlCgenExprAsType(XL_CgenContext* pCtx, const XL_MirNode* pNode, const char* sExpectedTypeName);
static void __xlCgenValueExprAsType(XL_CgenContext* pCtx, const char* sValueExpr, const char* sExpectedTypeName);
static void __xlCgenStmt(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo);
static const char* __xlCDefaultReturnValue(const char* sTypeName);
static void __xlCgenPropagatePendingException(XL_CgenContext* pCtx, const XL_CgenFuncInfo* pFuncInfo);
static void __xlCgenBoxTypedCExpr(XL_CgenContext* pCtx, const char* sTypeName, const char* sExpr);
static int __xlCgenEmitAbstractGeneratorExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode, int bCloneAbstract);

static const XL_MirNode* __xlFindFuncNodeBySymbolName(const XL_MirNode* pRoot, const char* sSymbolName)
{
	const XL_MirNode* pNode;
	size_t i;

	if ( (pRoot == NULL) || (sSymbolName == NULL) || (sSymbolName[0] == '\0') ) {
		return NULL;
	}

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		pNode = pRoot->ppChildren[i];
		if ( (pNode == NULL) || (pNode->eKind != XL_MIR_FUNC) ) {
			continue;
		}
		if ( strcmp(__xlMirFuncSymbolName(pNode), sSymbolName) == 0 ) {
			return pNode;
		}
	}

	return NULL;
}

static const XL_MirNode* __xlResolveFunctionRefNode(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pFunc;

	if ( (pCtx == NULL) || (pNode == NULL) ) {
		return NULL;
	}
	if ( pNode->eKind == XL_MIR_CALL ) {
		return __xlResolveCallFuncNode(pCtx, pNode);
	}
	if ( pNode->iFuncIndex >= 0 ) {
		pFunc = __xlFindFuncNodeByIndex(pCtx->pRoot, pNode->iFuncIndex);
		if ( pFunc != NULL ) {
			return pFunc;
		}
	}
	if ( (pNode->sAbiName != NULL) && (pNode->sAbiName[0] != '\0') ) {
		pFunc = __xlFindFuncNodeBySymbolName(pCtx->pRoot, pNode->sAbiName);
		if ( pFunc != NULL ) {
			return pFunc;
		}
	}
	if ( (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		return __xlFindFuncNode(pCtx->pRoot, pNode->sText);
	}
	return NULL;
}

static int __xlCanEmitFunctionValueThunk(const XL_MirNode* pFunc)
{
	const XL_MirNode* pParams;
	size_t i;

	if ( (pFunc == NULL) ||
		(pFunc->eKind != XL_MIR_FUNC) ||
		__xlIsGeneratorFuncNode(pFunc) ||
		((pFunc->sTypeName != NULL) && (strcmp(pFunc->sTypeName, "generator") == 0)) ||
		pFunc->bFlag ) {
		return 0;
	}

	pParams = (pFunc->iChildCount != 0) ? pFunc->ppChildren[0] : NULL;
	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		const XL_MirNode* pParam = pParams->ppChildren[i];

		if ( (pParam != NULL) && (pParam->sTypeName != NULL) && (strcmp(pParam->sTypeName, "generator") == 0) ) {
			return 0;
		}
	}

	return (pFunc != NULL) &&
		(pFunc->eKind == XL_MIR_FUNC);
}

static void __xlCgenFunctionValueThunkName(char* sOut, size_t iOutSize, const XL_MirNode* pFunc)
{
	const char* sSymbolName;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	sSymbolName = __xlMirFuncSymbolName(pFunc);
	snprintf(sOut, iOutSize, "%s__boxed", ((sSymbolName != NULL) && (sSymbolName[0] != '\0')) ? sSymbolName : "xl_func");
}

static void __xlCgenFunctionValueThunkRef(XL_CgenBuffer* pBuf, const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	char sThunkName[256];
	const XL_MirNode* pFunc;

	if ( (pNode != NULL) && (pNode->sAbiName != NULL) && (pNode->sAbiName[0] != '\0') ) {
		__xlCgenAppendFormat(pBuf, "((xfunction)%s__boxed)", pNode->sAbiName);
		return;
	}

	pFunc = __xlResolveFunctionRefNode(pCtx, pNode);
	if ( __xlCanEmitFunctionValueThunk(pFunc) ) {
		__xlCgenFunctionValueThunkName(sThunkName, sizeof(sThunkName), pFunc);
		__xlCgenAppendFormat(pBuf, "((xfunction)%s)", sThunkName);
		return;
	}

	if ( (pNode != NULL) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		__xlCgenAppendFormat(pBuf, "((xfunction)%s)", pNode->sText);
		return;
	}
	__xlCgenAppend(pBuf, "((xfunction)NULL)");
}

static const char* __xlMirFuncSymbolName(const XL_MirNode* pNode)
{
	if ( pNode == NULL ) {
		return NULL;
	}
	if ( (pNode->sAbiName != NULL) && (pNode->sAbiName[0] != '\0') ) {
		return pNode->sAbiName;
	}
	return pNode->sText;
}

static int __xlMirFuncHasExternTarget(const XL_MirNode* pNode)
{
	return (pNode != NULL) && (pNode->sExternName != NULL) && (pNode->sExternName[0] != '\0');
}

static int __xlHasTopLevelInitNode(const XL_MirNode* pRoot)
{
	size_t i;

	if ( pRoot == NULL ) {
		return 0;
	}

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		const XL_MirNode* pNode;

		pNode = pRoot->ppChildren[i];
		if ( (pNode == NULL) || (pNode->eKind == XL_MIR_FUNC) || (pNode->eKind == XL_MIR_IMPORT) || (pNode->eKind == XL_MIR_EXPORT) ) {
			continue;
		}
		return 1;
	}

	return 0;
}

static int __xlHasTopLevelBind(const XL_MirNode* pRoot)
{
	size_t i;

	if ( pRoot == NULL ) {
		return 0;
	}

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		const XL_MirNode* pNode;

		pNode = pRoot->ppChildren[i];
		if ( (pNode != NULL) && (pNode->eKind == XL_MIR_BIND) ) {
			return 1;
		}
	}

	return 0;
}

static void __xlCgenTopLevelBindDecl(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const char* sTypeName;
	const char* sDefaultValue;
	const XL_MirNode* pGeneratorFunc;
	char sGeneratorTypeName[256];

	if ( (pCtx == NULL) || (pNode == NULL) || (pNode->eKind != XL_MIR_BIND) ) {
		return;
	}

	sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
	pGeneratorFunc = NULL;
	if ( (strcmp(sTypeName, "generator") == 0) &&
		__xlResolveGeneratorSourceInfo(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, &pGeneratorFunc, NULL, NULL, 0) ) {
		__xlCgenGeneratorTypeNameForFunc(sGeneratorTypeName, sizeof(sGeneratorTypeName), pGeneratorFunc);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"static %s %s",
			sGeneratorTypeName,
			(pNode->sText != NULL) ? pNode->sText : "tmp"
		);
		__xlCgenAppend(&pCtx->buf, ";\n");
		return;
	}
	sDefaultValue = __xlCDefaultReturnValue(sTypeName);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"static %s %s",
		__xlCTypeName(sTypeName),
		(pNode->sText != NULL) ? pNode->sText : "tmp"
	);
	if ( sDefaultValue != NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, " = %s", sDefaultValue);
	}
	__xlCgenAppend(&pCtx->buf, ";\n");
}

static void __xlCgenTopLevelInitBind(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo)
{
	if ( (pCtx == NULL) || (pNode == NULL) || (pNode->eKind != XL_MIR_BIND) ) {
		return;
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = ", (pNode->sText != NULL) ? pNode->sText : "tmp");
	__xlCgenExpr(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenPropagatePendingException(pCtx, pFuncInfo);
}

static void __xlCgenModuleInit(XL_CgenContext* pCtx, const XL_MirNode* pRoot)
{
	XL_CgenFuncInfo initInfo;
	const XL_CgenFuncInfo* pPrevFuncInfo;
	size_t i;

	if ( pCtx == NULL ) {
		return;
	}

	memset(&initInfo, 0, sizeof(initInfo));
	initInfo.sName = "xl_generated_module_init";
	initInfo.sTypeName = "void";
	pPrevFuncInfo = pCtx->pCurrentFuncInfo;
	pCtx->pCurrentFuncInfo = &initInfo;

	__xlCgenAppend(&pCtx->buf, "static int xl_generated_module_init_done = 0;\n");
	__xlCgenAppend(&pCtx->buf, "static void xl_generated_module_init(void)\n{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "if ( xl_generated_module_init_done )\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "return;\n");
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");

	if ( __xlHasTopLevelInitNode(pRoot) ) {
		for ( i = 0; i < pRoot->iChildCount; ++i ) {
			const XL_MirNode* pNode;

			pNode = pRoot->ppChildren[i];
			if ( (pNode == NULL) || (pNode->eKind == XL_MIR_FUNC) || (pNode->eKind == XL_MIR_IMPORT) || (pNode->eKind == XL_MIR_EXPORT) ) {
				continue;
			}
			if ( pNode->eKind == XL_MIR_BIND ) {
				__xlCgenTopLevelInitBind(pCtx, pNode, &initInfo);
			} else {
				__xlCgenStmt(pCtx, pNode, &initInfo);
			}
		}
	}
	pCtx->pCurrentFuncInfo = pPrevFuncInfo;

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "xl_generated_module_init_done = 1;\n");
	pCtx->buf.iIndent -= 1;
	__xlCgenAppend(&pCtx->buf, "}\n\n");
}

static void __xlCgenModuleAbiObjects(XL_CgenContext* pCtx)
{
	size_t i;

	if ( (pCtx == NULL) || (pCtx->pModules == NULL) || (pCtx->pModules->pModules == NULL) ) {
		return;
	}

	for ( i = 0; i < pCtx->pModules->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;
		char sInitSymbol[XL_PATH_MAX_LEN];
		char sExportsSymbol[XL_PATH_MAX_LEN];
		char sDescriptorSymbol[XL_PATH_MAX_LEN];
		char sModuleAbiId[XL_PATH_MAX_LEN];
		int bKeyCollision;
		size_t j;

		pModule = &pCtx->pModules->pModules[i];
		bKeyCollision = __xlCgenModuleKeyHasCollision(pCtx->pModules, pModule);
		if ( bKeyCollision ) {
			__xlCgenBuildUniqueModuleSymbol(pModule, "init", sInitSymbol, sizeof(sInitSymbol));
		} else if ( (pModule != NULL) && (pModule->sInitSymbol != NULL) && (pModule->sInitSymbol[0] != '\0') ) {
			snprintf(sInitSymbol, sizeof(sInitSymbol), "%s", pModule->sInitSymbol);
		} else {
			__xlCgenBuildModuleSymbol((pModule != NULL) ? pModule->sKey : NULL, "init", sInitSymbol, sizeof(sInitSymbol));
		}
		if ( bKeyCollision ) {
			__xlCgenBuildUniqueModuleSymbol(pModule, "exports", sExportsSymbol, sizeof(sExportsSymbol));
		} else if ( (pModule != NULL) && (pModule->sExportsSymbol != NULL) && (pModule->sExportsSymbol[0] != '\0') ) {
			snprintf(sExportsSymbol, sizeof(sExportsSymbol), "%s", pModule->sExportsSymbol);
		} else {
			__xlCgenBuildModuleSymbol((pModule != NULL) ? pModule->sKey : NULL, "exports", sExportsSymbol, sizeof(sExportsSymbol));
		}
		if ( bKeyCollision ) {
			__xlCgenBuildUniqueModuleSymbol(pModule, "descriptor", sDescriptorSymbol, sizeof(sDescriptorSymbol));
		} else if ( (pModule != NULL) && (pModule->sDescriptorSymbol != NULL) && (pModule->sDescriptorSymbol[0] != '\0') ) {
			snprintf(sDescriptorSymbol, sizeof(sDescriptorSymbol), "%s", pModule->sDescriptorSymbol);
		} else {
			__xlCgenBuildModuleSymbol((pModule != NULL) ? pModule->sKey : NULL, "descriptor", sDescriptorSymbol, sizeof(sDescriptorSymbol));
		}
		__xlCgenBuildModuleAbiId(pModule, sModuleAbiId, sizeof(sModuleAbiId));

		__xlCgenAppendFormat(&pCtx->buf, "const XL_GeneratedModuleExport %s[] = {\n", sExportsSymbol);
		if ( (pModule == NULL) || (pModule->iExportCount == 0) ) {
			__xlCgenAppend(&pCtx->buf, "\t{ NULL, NULL }\n");
		} else {
			for ( j = 0; (pModule != NULL) && (j < pModule->iExportCount); ++j ) {
				__xlCgenAppend(&pCtx->buf, "\t{ ");
				__xlCgenAppendCString(&pCtx->buf, pModule->pExports[j].sName);
				__xlCgenAppend(&pCtx->buf, ", ");
				__xlCgenAppendCString(&pCtx->buf, pModule->pExports[j].sSymbol);
				__xlCgenAppend(&pCtx->buf, " }");
				if ( (j + 1) != pModule->iExportCount ) {
					__xlCgenAppend(&pCtx->buf, ",");
				}
				__xlCgenAppend(&pCtx->buf, "\n");
			}
		}
		__xlCgenAppend(&pCtx->buf, "};\n");

		__xlCgenAppendFormat(&pCtx->buf, "void %s(void)\n{\n", sInitSymbol);
		__xlCgenAppend(&pCtx->buf, "\txl_generated_module_init();\n");
		__xlCgenAppend(&pCtx->buf, "}\n");

		__xlCgenAppendFormat(&pCtx->buf, "const XL_GeneratedModuleDescriptor %s = {\n", sDescriptorSymbol);
		__xlCgenAppend(&pCtx->buf, "\t");
		__xlCgenAppendCString(&pCtx->buf, (pModule != NULL) ? pModule->sKey : "");
		__xlCgenAppend(&pCtx->buf, ",\n\t");
		__xlCgenAppendCString(&pCtx->buf, sModuleAbiId);
		__xlCgenAppend(&pCtx->buf, ",\n\t");
		__xlCgenAppendCString(&pCtx->buf, sInitSymbol);
		__xlCgenAppend(&pCtx->buf, ",\n\t");
		__xlCgenAppendCString(&pCtx->buf, sExportsSymbol);
		__xlCgenAppend(&pCtx->buf, ",\n\t");
		__xlCgenAppendCString(&pCtx->buf, sDescriptorSymbol);
		__xlCgenAppendFormat(&pCtx->buf, ",\n\t%llu,\n\t%s\n};\n\n", (unsigned long long)((pModule != NULL) ? pModule->iExportCount : 0), sExportsSymbol);
	}
}

static void __xlCgenFutureSupportTyped(XL_CgenBuffer* pBuf, const char* sSuffix, const char* sCType, const char* sHelperStem, const char* sDefaultValue)
{
	__xlCgenAppendFormat(
		pBuf,
		"typedef struct XL_Future%s {\n\txfuture* handle;\n} XL_Future%s;\n\n",
		sSuffix,
		sSuffix
	);
	if ( strcmp(sHelperStem, "int") == 0 ) {
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_ready((ptr)(intptr_t)value, \"future<int>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sCType,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_error(XL_GeneratedError* error)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_error(error, \"future<int>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {\n\t\treturn %s;\n\t}\n\treturn (int)(intptr_t)result.pValue;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {\n\t\treturn %s;\n\t}\n\treturn (int)(intptr_t)result.pValue;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_forward_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\tXL_Future%s out;\n\tout.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, \"future<int>\");\n\treturn out;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix,
			sSuffix
		);
		return;
	}
	if ( strcmp(sHelperStem, "bool") == 0 ) {
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_ready((ptr)(intptr_t)(value ? 1 : 0), \"future<bool>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sCType,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_error(XL_GeneratedError* error)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_error(error, \"future<bool>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {\n\t\treturn %s;\n\t}\n\treturn ((intptr_t)result.pValue != 0) ? 1 : 0;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {\n\t\treturn %s;\n\t}\n\treturn ((intptr_t)result.pValue != 0) ? 1 : 0;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_forward_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\tXL_Future%s out;\n\tout.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, \"future<bool>\");\n\treturn out;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix,
			sSuffix
		);
		return;
	}
	if ( strcmp(sHelperStem, "float") == 0 ) {
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tdouble* pValue = (double*)xrtMalloc(sizeof(double));\n\tif ( pValue == NULL ) {\n\t\tfuture.handle = xl_generated_future_create_error(xl_generated_error_make(\"Error\", \"future<float> alloc failed\", 0, \"\", \"\"), \"future<float>\");\n\t\treturn future;\n\t}\n\t*pValue = value;\n\tfuture.handle = xl_generated_future_create_ready((ptr)pValue, \"future<float>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sCType,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_error(XL_GeneratedError* error)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_error(error, \"future<float>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txfuture_result result;\n\tdouble* pValue;\n\tif ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {\n\t\treturn %s;\n\t}\n\tpValue = (double*)result.pValue;\n\treturn (pValue != NULL) ? *pValue : %s;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\txfuture_result result;\n\tdouble* pValue;\n\tif ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {\n\t\treturn %s;\n\t}\n\tpValue = (double*)result.pValue;\n\treturn (pValue != NULL) ? *pValue : %s;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_forward_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\tXL_Future%s out;\n\tout.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, \"future<float>\");\n\treturn out;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix,
			sSuffix
		);
		return;
	}
	if ( strcmp(sHelperStem, "string") == 0 ) {
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_ready((ptr)((value != NULL) ? value : \"\"), \"future<string>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sCType,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_error(XL_GeneratedError* error)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_error(error, \"future<string>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {\n\t\treturn %s;\n\t}\n\treturn (const char*)((result.pValue != NULL) ? result.pValue : \"\");\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {\n\t\treturn %s;\n\t}\n\treturn (const char*)((result.pValue != NULL) ? result.pValue : \"\");\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_forward_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\tXL_Future%s out;\n\tout.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, \"future<string>\");\n\treturn out;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix,
			sSuffix
		);
		return;
	}
	if ( strcmp(sHelperStem, "error") == 0 ) {
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_ready((ptr)value, \"future<error>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sCType,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_error(XL_GeneratedError* error)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_error(error, \"future<error>\");\n\treturn future;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {\n\t\treturn %s;\n\t}\n\treturn (XL_GeneratedError*)result.pValue;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static %s xl_future_%s_await_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {\n\t\treturn %s;\n\t}\n\treturn (XL_GeneratedError*)result.pValue;\n}\n\n",
			sCType,
			sHelperStem,
			sSuffix,
			sDefaultValue
		);
		__xlCgenAppendFormat(
			pBuf,
			"static XL_Future%s xl_future_%s_forward_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\tXL_Future%s out;\n\tout.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, \"future<error>\");\n\treturn out;\n}\n\n",
			sSuffix,
			sHelperStem,
			sSuffix,
			sSuffix
		);
		return;
	}
	__xlCgenAppendFormat(
		pBuf,
		"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_ready((ptr)value, \"future<ptr>\");\n\treturn future;\n}\n\n",
		sSuffix,
		sHelperStem,
		sCType,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static XL_Future%s xl_future_%s_error(XL_GeneratedError* error)\n{\n\tXL_Future%s future;\n\tfuture.handle = xl_generated_future_create_error(error, \"future<ptr>\");\n\treturn future;\n}\n\n",
		sSuffix,
		sHelperStem,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result(sName, future.handle, &result) ) {\n\t\treturn %s;\n\t}\n\treturn (%s)result.pValue;\n}\n\n",
		sCType,
		sHelperStem,
		sSuffix,
		sDefaultValue,
		sCType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_future_%s_await_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\txfuture_result result;\n\tif ( !xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId) ) {\n\t\treturn %s;\n\t}\n\treturn (%s)result.pValue;\n}\n\n",
		sCType,
		sHelperStem,
		sSuffix,
		sDefaultValue,
		sCType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static XL_Future%s xl_future_%s_forward_state(const char* sName, XL_Future%s future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\tXL_Future%s out;\n\tout.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, \"future<ptr>\");\n\treturn out;\n}\n\n",
		sSuffix,
		sHelperStem,
		sSuffix,
		sSuffix
	);
}

static const char* __xlTaskAbiTypeToken(const char* sTypeName)
{
	if ( sTypeName == NULL ) {
		return "int";
	}
	if ( strcmp(sTypeName, "int") == 0 ) {
		return "int";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "float";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "string";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "bool";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "error";
	}
	return "ptr";
}

static const char* __xlTaskFutureAbiTypeToken(const char* sTypeName)
{
	if ( sTypeName == NULL ) {
		return "future_x3Cint_x3E";
	}
	if ( strcmp(sTypeName, "int") == 0 ) {
		return "future_x3Cint_x3E";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "future_x3Cfloat_x3E";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "future_x3Cstring_x3E";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "future_x3Cbool_x3E";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "future_x3Cerror_x3E";
	}
	return "future_x3Cptr_x3E";
}

static void __xlCgenTaskSupportTyped(XL_CgenBuffer* pBuf, const char* sHelperStem, const char* sTypeName, const char* sCType, const char* sDefaultValue)
{
	const char* sFutureType;
	const char* sAbiType;
	const char* sFutureAbiType;

	sFutureType = __xlFutureTypeName(sTypeName);
	sAbiType = __xlTaskAbiTypeToken(sTypeName);
	sFutureAbiType = __xlTaskFutureAbiTypeToken(sTypeName);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_spawn_%s(const char* sName, %s value)\n{\n\txl_generated_task_trace_event(\"spawn\", (sName != NULL) ? sName : \"\");\n\treturn value;\n}\n\n",
		sFutureType,
		sHelperStem,
		sFutureType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_all_%s(const char* sName, %s left, %s right)\n{\n\t%s future;\n\txfuture* arrFuture[2];\n\txfuture* pAll;\n\txl_generated_task_trace_event(\"all\", (sName != NULL) ? sName : \"\");\n\tif ( (left.handle == NULL) || (right.handle == NULL) ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.all received an empty future\", 0, \"\", \"\"));\n\t}\n\tarrFuture[0] = left.handle;\n\tarrFuture[1] = right.handle;\n\tpAll = xFutureWhenAll(arrFuture, 2);\n\tif ( pAll == NULL ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.all could not create group future\", 0, \"\", \"\"));\n\t}\n\tfuture.handle = xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL);\n\txFutureRelease(pAll);\n\tif ( future.handle == NULL ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.all could not map group future\", 0, \"\", \"\"));\n\t}\n\treturn future;\n}\n\n",
		sFutureType,
		sHelperStem,
		sFutureType,
		sFutureType,
		sFutureType,
		__xlFutureErrorFunc(sTypeName),
		__xlFutureErrorFunc(sTypeName),
		__xlFutureErrorFunc(sTypeName)
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_race_%s(const char* sName, %s left, %s right)\n{\n\t%s future;\n\txfuture* arrFuture[2];\n\txl_generated_task_trace_event(\"race\", (sName != NULL) ? sName : \"\");\n\tif ( (left.handle == NULL) || (right.handle == NULL) ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.race received an empty future\", 0, \"\", \"\"));\n\t}\n\tarrFuture[0] = left.handle;\n\tarrFuture[1] = right.handle;\n\tfuture.handle = xFutureRace(arrFuture, 2);\n\tif ( future.handle == NULL ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.race could not create race future\", 0, \"\", \"\"));\n\t}\n\treturn future;\n}\n\n",
		sFutureType,
		sHelperStem,
		sFutureType,
		sFutureType,
		sFutureType,
		__xlFutureErrorFunc(sTypeName),
		__xlFutureErrorFunc(sTypeName)
	);
	__xlCgenAppendFormat(
		pBuf,
		"static int xl_task_cancel_%s(const char* sName, %s value)\n{\n\txl_generated_task_trace_event(\"cancel\", (sName != NULL) ? sName : \"\");\n\tif ( value.handle == NULL ) {\n\t\treturn 0;\n\t}\n\t(void)xFutureRequestCancel(value.handle);\n\treturn 1;\n}\n\n",
		sHelperStem,
		sFutureType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl__xl_mod_std_task__spawn__arity1__value__%s__ret__future_x3C%s_x3E(%s value)\n{\n\treturn xl_task_spawn_%s(\"xl_mod_std_task__spawn\", value);\n}\n\n",
		sFutureType,
		sFutureAbiType,
		sAbiType,
		sFutureType,
		sHelperStem
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl__xl_mod_std_task__all__arity2__left__%s__right__%s__ret__future_x3C%s_x3E(%s left, %s right)\n{\n\treturn xl_task_all_%s(\"xl_mod_std_task__all\", left, right);\n}\n\n",
		sFutureType,
		sFutureAbiType,
		sFutureAbiType,
		sAbiType,
		sFutureType,
		sFutureType,
		sHelperStem
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl__xl_mod_std_task__race__arity2__left__%s__right__%s__ret__future_x3C%s_x3E(%s left, %s right)\n{\n\treturn xl_task_race_%s(\"xl_mod_std_task__race\", left, right);\n}\n\n",
		sFutureType,
		sFutureAbiType,
		sFutureAbiType,
		sAbiType,
		sFutureType,
		sFutureType,
		sHelperStem
	);
	__xlCgenAppendFormat(
		pBuf,
		"static int xl__xl_mod_std_task__cancel__arity1__value__%s__ret__int(%s value)\n{\n\treturn xl_task_cancel_%s(\"xl_mod_std_task__cancel\", value);\n}\n\n",
		sFutureAbiType,
		sFutureType,
		sHelperStem
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_default_%s(void)\n{\n\treturn %s;\n}\n\n",
		sCType,
		sHelperStem,
		sDefaultValue
	);
}

static void __xlCgenTaskDelaySupport(XL_CgenBuffer* pBuf)
{
	__xlCgenAppend(
		pBuf,
		"static XL_FutureInt xl_task_delay(const char* sName, int ms)\n{\n"
		"\txl_generated_task_trace_event(\"delay\", (sName != NULL) ? sName : \"\");\n"
		"\t{\n"
		"\t\tXL_FutureInt future;\n"
		"\t\tuint32_t iDelay = (ms < 0) ? 0u : (uint32_t)ms;\n"
		"\t\tfuture.handle = xTaskRunDelayed(NULL, 0u, iDelay, xl_generated_task_delay_engine, (ptr)(intptr_t)ms);\n"
		"\t\tif ( future.handle == NULL ) {\n"
		"\t\t\treturn xl_future_int_error(xl_generated_error_make(\"Error\", \"task.delay could not schedule future\", 0, \"\", \"\"));\n"
		"\t\t}\n"
		"\t\treturn future;\n"
		"\t}\n"
		"}\n\n"
		"static XL_FutureInt xl__xl_mod_std_task__delay__arity1__ms__int__ret__future_x3Cint_x3E(int ms)\n{\n"
		"\treturn xl_task_delay(\"xl_mod_std_task__delay\", ms);\n"
		"}\n\n"
	);
}

static void __xlCgenGeneratorSupportTyped(XL_CgenBuffer* pBuf, const char* sSuffix, const char* sCType, const char* sHelperStem)
{
	__xlCgenAppendFormat(
		pBuf,
		"typedef struct XL_Generator%s {\n\tint count;\n\tint index;\n\t%s items[256];\n} XL_Generator%s;\n\n",
		sSuffix,
		sCType,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static void xl_generator_%s_init(XL_Generator%s* pGen)\n{\n\tif ( pGen == NULL ) {\n\t\treturn;\n\t}\n\tpGen->count = 0;\n\tpGen->index = 0;\n}\n\n",
		sHelperStem,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static void xl_generator_%s_yield(const char* sName, XL_Generator%s* pGen, %s value)\n{\n\tif ( pGen == NULL ) {\n\t\treturn;\n\t}\n\tif ( pGen->count < 256 ) {\n\t\tpGen->items[pGen->count++] = value;\n\t}\n\txl_generated_generator_trace_event(\"yield\", (sName != NULL) ? sName : \"\");\n}\n\n",
		sHelperStem,
		sSuffix,
		sCType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static void xl_generator_%s_done(const char* sName, XL_Generator%s* pGen)\n{\n\t(void)pGen;\n\txl_generated_generator_trace_event(\"done\", (sName != NULL) ? sName : \"\");\n}\n\n",
		sHelperStem,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static int xl_generator_%s_next(const char* sName, XL_Generator%s* pGen, %s* pValue)\n{\n\tif ( (pGen == NULL) || (pValue == NULL) ) {\n\t\treturn 0;\n\t}\n\tif ( pGen->index >= pGen->count ) {\n\t\txl_generated_generator_trace_event(\"eof\", (sName != NULL) ? sName : \"\");\n\t\treturn 0;\n\t}\n\t*pValue = pGen->items[pGen->index++];\n\txl_generated_generator_trace_event(\"next\", (sName != NULL) ? sName : \"\");\n\treturn 1;\n}\n\n",
		sHelperStem,
		sSuffix,
		sCType
	);
}

static void __xlCgenRuntimeSupport(XL_CgenBuffer* pBuf)
{
	__xlCgenAppend(pBuf, "#ifndef XL_GENERATED_XVO_TYPE_LOCAL\n");
	__xlCgenAppend(pBuf, "#define XL_GENERATED_XVO_TYPE_LOCAL 1\n");
	__xlCgenAppend(pBuf, "#define xvoType(value) (((value) == NULL) ? XVO_DT_NULL : ((value)->Type))\n");
	__xlCgenAppend(pBuf, "#define xvoTypeName(kind) ((str)(((kind) == XVO_DT_NULL) ? \"null\" : ((kind) == XVO_DT_BOOL) ? \"bool\" : ((kind) == XVO_DT_INT) ? \"int\" : ((kind) == XVO_DT_FLOAT) ? \"float\" : ((kind) == XVO_DT_TEXT) ? \"string\" : ((kind) == XVO_DT_TIME) ? \"time\" : ((kind) == XVO_DT_POINT) ? \"point\" : ((kind) == XVO_DT_FUNC) ? \"function\" : ((kind) == XVO_DT_ARRAY) ? \"array\" : ((kind) == XVO_DT_LIST) ? \"list\" : ((kind) == XVO_DT_COLL) ? \"set\" : ((kind) == XVO_DT_TABLE) ? \"dict\" : ((kind) == XVO_DT_CLASS) ? \"class\" : ((kind) == XVO_DT_CUSTOM) ? \"custom\" : \"unknown\"))\n");
	__xlCgenAppend(pBuf, "#endif\n\n");
	__xlCgenAppend(pBuf, "typedef struct XL_GeneratedError XL_GeneratedError;\n");
	__xlCgenAppend(pBuf, "struct XL_GeneratedError {\n");
	__xlCgenAppend(pBuf, "\tconst char* type;\n");
	__xlCgenAppend(pBuf, "\tconst char* message;\n");
	__xlCgenAppend(pBuf, "\tint code;\n");
	__xlCgenAppend(pBuf, "\tconst char* data;\n");
	__xlCgenAppend(pBuf, "\tconst char* stack;\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedError* cause;\n");
	__xlCgenAppend(pBuf, "\tchar text[256];\n");
	__xlCgenAppend(pBuf, "};\n\n");
	__xlCgenAppend(pBuf, "typedef struct XL_GeneratedExceptionFrame {\n");
	__xlCgenAppend(pBuf, "\tint phase;\n");
	__xlCgenAppend(pBuf, "\tint has_exception;\n");
	__xlCgenAppend(pBuf, "\tint needs_rethrow;\n");
	__xlCgenAppend(pBuf, "\tint kind;\n");
	__xlCgenAppend(pBuf, "\tconst char* text;\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedError* error;\n");
	__xlCgenAppend(pBuf, "\tlong long int_value;\n");
	__xlCgenAppend(pBuf, "\tdouble float_value;\n");
	__xlCgenAppend(pBuf, "} XL_GeneratedExceptionFrame;\n\n");
	__xlCgenAppend(pBuf, "static void xl_generated_raise_error(XL_GeneratedError* value);\n");
	__xlCgenAppend(pBuf, "static int xl_generated_capture_native_error(void);\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError xl_generated_error_slots[16];\n");
	__xlCgenAppend(pBuf, "static int xl_generated_error_next = 0;\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedExceptionFrame xl_generated_exception_frames[32];\n");
	__xlCgenAppend(pBuf, "static int xl_generated_exception_frame_depth = 0;\n");
	__xlCgenAppend(pBuf, "int xl_generated_had_exception = 0;\n");
	__xlCgenAppend(pBuf, "int xl_generated_exception_kind = 0;\n");
	__xlCgenAppend(pBuf, "const char* xl_generated_exception_text = NULL;\n");
	__xlCgenAppend(pBuf, "XL_GeneratedError* xl_generated_exception_error = NULL;\n");
	__xlCgenAppend(pBuf, "long long xl_generated_exception_int = 0;\n");
	__xlCgenAppend(pBuf, "double xl_generated_exception_float = 0.0;\n");
	__xlCgenAppend(pBuf, "char xl_generated_async_trace[8192];\n");
	__xlCgenAppend(pBuf, "int xl_generated_async_trace_len = 0;\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_async_trace_append(const char* sText)\n{\n");
	__xlCgenAppend(pBuf, "\tsize_t iLen;\n");
	__xlCgenAppend(pBuf, "\tif ( sText == NULL ) {\n\t\treturn;\n\t}\n");
	__xlCgenAppend(pBuf, "\tiLen = strlen(sText);\n");
	__xlCgenAppend(pBuf, "\tif ( (xl_generated_async_trace_len + (int)iLen) >= (int)(sizeof(xl_generated_async_trace) - 1) ) {\n");
	__xlCgenAppend(pBuf, "\t\tiLen = (size_t)((int)(sizeof(xl_generated_async_trace) - 1) - xl_generated_async_trace_len);\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\tif ( iLen == 0 ) {\n\t\treturn;\n\t}\n");
	__xlCgenAppend(pBuf, "\tmemcpy(xl_generated_async_trace + xl_generated_async_trace_len, sText, iLen);\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_len += (int)iLen;\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace[xl_generated_async_trace_len] = '\\0';\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_async_trace_event(const char* sEvent, const char* sFunc)\n{\n");
	__xlCgenAppend(pBuf, "\tchar sLine[256];\n");
	__xlCgenAppend(pBuf, "\tif ( sEvent == NULL ) {\n\t\tsEvent = \"event\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( sFunc == NULL ) {\n\t\tsFunc = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tsnprintf(sLine, sizeof(sLine), \"async.%s=%s\\n\", sEvent, sFunc);\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_append(sLine);\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static void xl_generated_async_trace_state_event(const char* sFunc, int iStateId)\n{\n");
	__xlCgenAppend(pBuf, "\tchar sEvent[32];\n");
	__xlCgenAppend(pBuf, "\tif ( iStateId < 0 ) {\n\t\treturn;\n\t}\n");
	__xlCgenAppend(pBuf, "\tsnprintf(sEvent, sizeof(sEvent), \"state_%d\", iStateId);\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_event(sEvent, sFunc);\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_task_trace_event(const char* sEvent, const char* sName)\n{\n");
	__xlCgenAppend(pBuf, "\tchar sLine[256];\n");
	__xlCgenAppend(pBuf, "\tif ( sEvent == NULL ) {\n\t\tsEvent = \"event\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( sName == NULL ) {\n\t\tsName = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tsnprintf(sLine, sizeof(sLine), \"task.%s=%s\\n\", sEvent, sName);\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_append(sLine);\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_generator_trace_event(const char* sEvent, const char* sName)\n{\n");
	__xlCgenAppend(pBuf, "\tchar sLine[256];\n");
	__xlCgenAppend(pBuf, "\tif ( sEvent == NULL ) {\n\t\tsEvent = \"event\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( sName == NULL ) {\n\t\tsName = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tsnprintf(sLine, sizeof(sLine), \"generator.%s=%s\\n\", sEvent, sName);\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_append(sLine);\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "typedef struct XL_GeneratedGeneratorExecutor {\n");
	__xlCgenAppend(pBuf, "\tint state;\n");
	__xlCgenAppend(pBuf, "\tint done;\n");
	__xlCgenAppend(pBuf, "} XL_GeneratedGeneratorExecutor;\n\n");
	__xlCgenAppend(pBuf, "static void xl_generated_generator_executor_init(XL_GeneratedGeneratorExecutor* pExec)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( pExec == NULL ) {\n\t\treturn;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpExec->state = 0;\n");
	__xlCgenAppend(pBuf, "\tpExec->done = 0;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_generator_executor_enter(XL_GeneratedGeneratorExecutor* pExec, const char* sName)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( pExec == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( pExec->done ) {\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_generator_trace_event(\"eof\", (sName != NULL) ? sName : \"\");\n");
	__xlCgenAppend(pBuf, "\t\treturn 0;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_generator_executor_can_resume(const XL_GeneratedGeneratorExecutor* pExec, int iStateId)\n{\n");
	__xlCgenAppend(pBuf, "\treturn (pExec != NULL) && !pExec->done && (pExec->state == iStateId);\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static void xl_generated_generator_executor_suspend(XL_GeneratedGeneratorExecutor* pExec, int iStateId, const char* sName)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( pExec == NULL ) {\n\t\treturn;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpExec->state = iStateId;\n");
	__xlCgenAppend(pBuf, "\txl_generated_generator_trace_event(\"yield\", (sName != NULL) ? sName : \"\");\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_generator_executor_resume(XL_GeneratedGeneratorExecutor* pExec, int iStateId)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( !xl_generated_generator_executor_can_resume(pExec, iStateId) ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpExec->state = 0;\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static void xl_generated_generator_executor_finish(XL_GeneratedGeneratorExecutor* pExec, const char* sName)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( pExec == NULL ) {\n\t\treturn;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpExec->done = 1;\n");
	__xlCgenAppend(pBuf, "\tpExec->state = -1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_generator_trace_event(\"done\", (sName != NULL) ? sName : \"\");\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "typedef void (*XL_GeneratedGeneratorNextRaw)(void);\n");
	__xlCgenAppend(pBuf, "typedef int (*XL_GeneratedGeneratorNextInt)(const char* sName, void* pState, int* pValue, xvalue xl_resume_value);\n");
	__xlCgenAppend(pBuf, "typedef int (*XL_GeneratedGeneratorNextFloat)(const char* sName, void* pState, double* pValue, xvalue xl_resume_value);\n");
	__xlCgenAppend(pBuf, "typedef int (*XL_GeneratedGeneratorNextText)(const char* sName, void* pState, const char** pValue, xvalue xl_resume_value);\n");
	__xlCgenAppend(pBuf, "typedef int (*XL_GeneratedGeneratorNextError)(const char* sName, void* pState, XL_GeneratedError** pValue, xvalue xl_resume_value);\n");
	__xlCgenAppend(pBuf, "typedef int (*XL_GeneratedGeneratorNextVar)(const char* sName, void* pState, xvalue* pValue, xvalue xl_resume_value);\n");
	__xlCgenAppend(pBuf, "enum {\n");
	__xlCgenAppend(pBuf, "\tXL_GENERATED_GENERATOR_KIND_INT = 1,\n");
	__xlCgenAppend(pBuf, "\tXL_GENERATED_GENERATOR_KIND_FLOAT = 2,\n");
	__xlCgenAppend(pBuf, "\tXL_GENERATED_GENERATOR_KIND_STRING = 3,\n");
	__xlCgenAppend(pBuf, "\tXL_GENERATED_GENERATOR_KIND_ERROR = 4,\n");
	__xlCgenAppend(pBuf, "\tXL_GENERATED_GENERATOR_KIND_BOOL = 5,\n");
	__xlCgenAppend(pBuf, "\tXL_GENERATED_GENERATOR_KIND_VAR = 6,\n");
	__xlCgenAppend(pBuf, "\tXL_GENERATED_GENERATOR_KIND_VOID = 7\n");
	__xlCgenAppend(pBuf, "};\n");
	__xlCgenAppend(pBuf, "typedef struct XL_GeneratedGeneratorValue {\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedGeneratorNextRaw pNext;\n");
	__xlCgenAppend(pBuf, "\tvoid* pState;\n");
	__xlCgenAppend(pBuf, "\tsize_t iStateSize;\n");
	__xlCgenAppend(pBuf, "\tint iValueKind;\n");
	__xlCgenAppend(pBuf, "\tconst char* sName;\n");
	__xlCgenAppend(pBuf, "} XL_GeneratedGeneratorValue;\n\n");
	__xlCgenAppend(pBuf, "static const char* xl_generated_error_text(XL_GeneratedError* value);\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedGeneratorValue xl_generated_generator_null(void)\n{\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedGeneratorValue value;\n");
	__xlCgenAppend(pBuf, "\tmemset(&value, 0, sizeof(value));\n");
	__xlCgenAppend(pBuf, "\treturn value;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedGeneratorValue xl_generated_generator_make(XL_GeneratedGeneratorNextRaw pNext, size_t iStateSize, const void* pState, int iValueKind, const char* sName)\n{\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedGeneratorValue value = xl_generated_generator_null();\n");
	__xlCgenAppend(pBuf, "\tvoid* pCopy;\n");
	__xlCgenAppend(pBuf, "\tif ( (pNext == NULL) || (pState == NULL) || (iStateSize == 0u) ) {\n\t\treturn value;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpCopy = xrtMalloc(iStateSize);\n");
	__xlCgenAppend(pBuf, "\tif ( pCopy == NULL ) {\n\t\treturn value;\n\t}\n");
	__xlCgenAppend(pBuf, "\tmemcpy(pCopy, pState, iStateSize);\n");
	__xlCgenAppend(pBuf, "\tvalue.pNext = pNext;\n");
	__xlCgenAppend(pBuf, "\tvalue.pState = pCopy;\n");
	__xlCgenAppend(pBuf, "\tvalue.iStateSize = iStateSize;\n");
	__xlCgenAppend(pBuf, "\tvalue.iValueKind = iValueKind;\n");
	__xlCgenAppend(pBuf, "\tvalue.sName = sName;\n");
	__xlCgenAppend(pBuf, "\treturn value;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedGeneratorValue xl_generated_generator_clone(XL_GeneratedGeneratorValue value)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( (value.pNext == NULL) || (value.pState == NULL) || (value.iStateSize == 0u) ) {\n\t\treturn xl_generated_generator_null();\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn xl_generated_generator_make(value.pNext, value.iStateSize, value.pState, value.iValueKind, value.sName);\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_generator_step(XL_GeneratedGeneratorValue* pGen, xvalue xl_resume_value, xvalue* pValue)\n{\n");
	__xlCgenAppend(pBuf, "\tint ok = 0;\n");
	__xlCgenAppend(pBuf, "\tif ( pValue != NULL ) {\n\t\t*pValue = NULL;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( (pGen == NULL) || (pGen->pNext == NULL) || (pGen->pState == NULL) ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tswitch ( pGen->iValueKind ) {\n");
	__xlCgenAppend(pBuf, "\t\tcase XL_GENERATED_GENERATOR_KIND_FLOAT:\n\t\t{\n\t\t\tdouble value = 0.0;\n\t\t\tok = ((XL_GeneratedGeneratorNextFloat)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : \"generator\", pGen->pState, &value, xl_resume_value);\n\t\t\tif ( ok && (pValue != NULL) ) {\n\t\t\t\t*pValue = xvoCreateFloat(value);\n\t\t\t}\n\t\t\treturn ok;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tcase XL_GENERATED_GENERATOR_KIND_STRING:\n\t\t{\n\t\t\tconst char* value = NULL;\n\t\t\tok = ((XL_GeneratedGeneratorNextText)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : \"generator\", pGen->pState, &value, xl_resume_value);\n\t\t\tif ( ok && (pValue != NULL) ) {\n\t\t\t\tconst char* sText = (value != NULL) ? value : \"\";\n\t\t\t\t*pValue = xvoCreateText((ptr)sText, (uint32)strlen(sText), FALSE);\n\t\t\t}\n\t\t\treturn ok;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tcase XL_GENERATED_GENERATOR_KIND_ERROR:\n\t\t{\n\t\t\tXL_GeneratedError* value = NULL;\n\t\t\tok = ((XL_GeneratedGeneratorNextError)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : \"generator\", pGen->pState, &value, xl_resume_value);\n\t\t\tif ( ok && (pValue != NULL) ) {\n\t\t\t\tconst char* sText = xl_generated_error_text(value);\n\t\t\t\tif ( sText == NULL ) {\n\t\t\t\t\tsText = \"\";\n\t\t\t\t}\n\t\t\t\t*pValue = xvoCreateText((ptr)sText, (uint32)strlen(sText), FALSE);\n\t\t\t}\n\t\t\treturn ok;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tcase XL_GENERATED_GENERATOR_KIND_BOOL:\n\t\t{\n\t\t\tint value = 0;\n\t\t\tok = ((XL_GeneratedGeneratorNextInt)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : \"generator\", pGen->pState, &value, xl_resume_value);\n\t\t\tif ( ok && (pValue != NULL) ) {\n\t\t\t\t*pValue = xvoCreateBool(value ? TRUE : FALSE);\n\t\t\t}\n\t\t\treturn ok;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tcase XL_GENERATED_GENERATOR_KIND_VAR:\n\t\t{\n\t\t\txvalue value = NULL;\n\t\t\tok = ((XL_GeneratedGeneratorNextVar)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : \"generator\", pGen->pState, &value, xl_resume_value);\n\t\t\tif ( ok && (pValue != NULL) ) {\n\t\t\t\t*pValue = (value != NULL) ? xvoCopy(value) : xvoCreateNull();\n\t\t\t}\n\t\t\treturn ok;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tcase XL_GENERATED_GENERATOR_KIND_VOID:\n\t\t{\n\t\t\tvoid* value = NULL;\n\t\t\tok = ((XL_GeneratedGeneratorNextText)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : \"generator\", pGen->pState, (const char**)&value, xl_resume_value);\n\t\t\tif ( ok && (pValue != NULL) ) {\n\t\t\t\t*pValue = xvoCreateNull();\n\t\t\t}\n\t\t\treturn ok;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tcase XL_GENERATED_GENERATOR_KIND_INT:\n\t\tdefault:\n\t\t{\n\t\t\tint value = 0;\n\t\t\tok = ((XL_GeneratedGeneratorNextInt)pGen->pNext)((pGen->sName != NULL) ? pGen->sName : \"generator\", pGen->pState, &value, xl_resume_value);\n\t\t\tif ( ok && (pValue != NULL) ) {\n\t\t\t\t*pValue = xvoCreateInt((int64)value);\n\t\t\t}\n\t\t\treturn ok;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_async_trace_flush_to_env_file(void)\n{\n");
	__xlCgenAppend(pBuf, "\tconst char* sTraceFile = getenv(\"XL_GENERATED_ASYNC_TRACE_FILE\");\n");
	__xlCgenAppend(pBuf, "\tFILE* pFile;\n");
	__xlCgenAppend(pBuf, "\tif ( (sTraceFile == NULL) || (sTraceFile[0] == '\\0') || (xl_generated_async_trace_len <= 0) ) {\n");
	__xlCgenAppend(pBuf, "\t\treturn;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\tpFile = fopen(sTraceFile, \"wb\");\n");
	__xlCgenAppend(pBuf, "\tif ( pFile == NULL ) {\n");
	__xlCgenAppend(pBuf, "\t\treturn;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\t(void)fwrite(xl_generated_async_trace, 1u, (size_t)xl_generated_async_trace_len, pFile);\n");
	__xlCgenAppend(pBuf, "\t(void)fclose(pFile);\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_runtime_reset(void)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_int = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_float = 0.0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_len = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace[0] = '\\0';\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_frame_depth = 0;\n");
	__xlCgenAppend(pBuf, "\txlrt_native_error_clear();\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_clear_exception(void)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_int = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_float = 0.0;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_restore_exception(int kind, const char* text, XL_GeneratedError* errorValue, long long intValue, double floatValue)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = (kind != 0);\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = kind;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = text;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = errorValue;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_int = intValue;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_float = floatValue;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedExceptionFrame* xl_generated_exception_frame_push(void)\n{\n");
	__xlCgenAppend(pBuf, "\tint iIndex = xl_generated_exception_frame_depth;\n");
	__xlCgenAppend(pBuf, "\tif ( iIndex >= (int)(sizeof(xl_generated_exception_frames) / sizeof(xl_generated_exception_frames[0])) ) {\n");
	__xlCgenAppend(pBuf, "\t\tiIndex = (int)(sizeof(xl_generated_exception_frames) / sizeof(xl_generated_exception_frames[0])) - 1;\n");
	__xlCgenAppend(pBuf, "\t} else {\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_exception_frame_depth += 1;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\tmemset(&xl_generated_exception_frames[iIndex], 0, sizeof(xl_generated_exception_frames[iIndex]));\n");
	__xlCgenAppend(pBuf, "\treturn &xl_generated_exception_frames[iIndex];\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_exception_frame_pop(void)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( xl_generated_exception_frame_depth > 0 ) {\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_exception_frame_depth -= 1;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static int xl_generated_exception_frame_capture(XL_GeneratedExceptionFrame* pFrame)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( (pFrame == NULL) || !xl_generated_had_exception ) {\n");
	__xlCgenAppend(pBuf, "\t\treturn 0;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\tpFrame->has_exception = 1;\n");
	__xlCgenAppend(pBuf, "\tpFrame->kind = xl_generated_exception_kind;\n");
	__xlCgenAppend(pBuf, "\tpFrame->text = xl_generated_exception_text;\n");
	__xlCgenAppend(pBuf, "\tpFrame->error = xl_generated_exception_error;\n");
	__xlCgenAppend(pBuf, "\tpFrame->int_value = xl_generated_exception_int;\n");
	__xlCgenAppend(pBuf, "\tpFrame->float_value = xl_generated_exception_float;\n");
	__xlCgenAppend(pBuf, "\txl_generated_clear_exception();\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_exception_frame_restore(const XL_GeneratedExceptionFrame* pFrame)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( (pFrame == NULL) || !pFrame->has_exception ) {\n");
	__xlCgenAppend(pBuf, "\t\treturn;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\txl_generated_restore_exception(pFrame->kind, pFrame->text, pFrame->error, pFrame->int_value, pFrame->float_value);\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_error_alloc(void)\n{\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedError* pErr = &xl_generated_error_slots[xl_generated_error_next++ & 15];\n");
	__xlCgenAppend(pBuf, "\tmemset(pErr, 0, sizeof(*pErr));\n");
	__xlCgenAppend(pBuf, "\treturn pErr;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static const char* xl_generated_error_text(XL_GeneratedError* value)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( value == NULL ) {\n\t\treturn \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( value->type == NULL ) {\n\t\tvalue->type = \"Error\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( value->message == NULL ) {\n\t\tvalue->message = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( (strcmp(value->type, \"Error\") == 0) || (value->type[0] == '\\0') ) {\n");
	__xlCgenAppend(pBuf, "\t\tsnprintf(value->text, sizeof(value->text), \"%s\", value->message);\n");
	__xlCgenAppend(pBuf, "\t} else if ( value->message[0] == '\\0' ) {\n");
	__xlCgenAppend(pBuf, "\t\tsnprintf(value->text, sizeof(value->text), \"%s\", value->type);\n");
	__xlCgenAppend(pBuf, "\t} else {\n");
	__xlCgenAppend(pBuf, "\t\tsnprintf(value->text, sizeof(value->text), \"%s: %s\", value->type, value->message);\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\treturn value->text;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_error_make_ex(const char* kind, const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedError* pErr = xl_generated_error_alloc();\n");
	__xlCgenAppend(pBuf, "\tpErr->type = (kind != NULL) ? kind : \"Error\";\n");
	__xlCgenAppend(pBuf, "\tpErr->message = (message != NULL) ? message : \"\";\n");
	__xlCgenAppend(pBuf, "\tpErr->code = code;\n");
	__xlCgenAppend(pBuf, "\tpErr->data = (data != NULL) ? data : \"\";\n");
	__xlCgenAppend(pBuf, "\tpErr->stack = (stack != NULL) ? stack : \"\";\n");
	__xlCgenAppend(pBuf, "\tpErr->cause = cause;\n");
	__xlCgenAppend(pBuf, "\t(void)xl_generated_error_text(pErr);\n");
	__xlCgenAppend(pBuf, "\treturn pErr;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_error_make(const char* kind, const char* message, int code, const char* data, const char* stack)\n{\n");
	__xlCgenAppend(pBuf, "\treturn xl_generated_error_make_ex(kind, message, code, data, stack, NULL);\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_error_from_text(const char* text)\n{\n");
	__xlCgenAppend(pBuf, "\tconst char* sSep;\n");
	__xlCgenAppend(pBuf, "\tchar sKind[64];\n");
	__xlCgenAppend(pBuf, "\tsize_t iLen;\n");
	__xlCgenAppend(pBuf, "\tif ( text == NULL ) {\n\t\treturn xl_generated_error_make(\"Error\", \"\", 0, \"\", \"\");\n\t}\n");
	__xlCgenAppend(pBuf, "\tsSep = strstr(text, \": \");\n");
	__xlCgenAppend(pBuf, "\tif ( sSep == NULL ) {\n\t\treturn xl_generated_error_make(\"Error\", text, 0, \"\", \"\");\n\t}\n");
	__xlCgenAppend(pBuf, "\tiLen = (size_t)(sSep - text);\n");
	__xlCgenAppend(pBuf, "\tif ( iLen >= sizeof(sKind) ) {\n\t\tiLen = sizeof(sKind) - 1;\n\t}\n");
	__xlCgenAppend(pBuf, "\tmemcpy(sKind, text, iLen);\n");
	__xlCgenAppend(pBuf, "\tsKind[iLen] = '\\0';\n");
	__xlCgenAppend(pBuf, "\treturn xl_generated_error_make(sKind, sSep + 2, 0, \"\", \"\");\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static const char* xl_generated_exception_value_text(int kind, const char* text, long long intValue, double floatValue)\n{\n");
	__xlCgenAppend(pBuf, "\tstatic char sBuffer[128];\n");
	__xlCgenAppend(pBuf, "\tif ( kind == 3 ) {\n\t\treturn (text != NULL) ? text : \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( kind == 4 ) {\n\t\treturn (text != NULL) ? text : \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( kind == 1 ) {\n\t\tsnprintf(sBuffer, sizeof(sBuffer), \"%lld\", intValue);\n\t\treturn sBuffer;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( kind == 2 ) {\n\t\tsnprintf(sBuffer, sizeof(sBuffer), \"%g\", floatValue);\n\t\treturn sBuffer;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn \"\";\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_exception_value_error(int kind, const char* text, XL_GeneratedError* errorValue, long long intValue, double floatValue)\n{\n");
	__xlCgenAppend(pBuf, "\tchar sBuffer[128];\n");
	__xlCgenAppend(pBuf, "\tif ( (kind == 4) && (errorValue != NULL) ) {\n\t\treturn errorValue;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( kind == 3 ) {\n\t\treturn xl_generated_error_from_text(text);\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( kind == 1 ) {\n\t\tsnprintf(sBuffer, sizeof(sBuffer), \"%lld\", intValue);\n\t\treturn xl_generated_error_make(\"Error\", sBuffer, 0, \"\", \"\");\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( kind == 2 ) {\n\t\tsnprintf(sBuffer, sizeof(sBuffer), \"%g\", floatValue);\n\t\treturn xl_generated_error_make(\"Error\", sBuffer, 0, \"\", \"\");\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn xl_generated_error_make(\"Error\", (text != NULL) ? text : \"\", 0, \"\", \"\");\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_capture_exception_error(void)\n{\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedError* value = xl_generated_exception_value_error(\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_exception_kind,\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_exception_text,\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_exception_error,\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_exception_int,\n");
	__xlCgenAppend(pBuf, "\t\txl_generated_exception_float\n");
	__xlCgenAppend(pBuf, "\t);\n");
	__xlCgenAppend(pBuf, "\txl_generated_clear_exception();\n");
	__xlCgenAppend(pBuf, "\treturn value;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static const char* xl_generated_copy_text(const char* sText)\n{\n");
	__xlCgenAppend(pBuf, "\tconst char* sValue = (sText != NULL) ? sText : \"\";\n");
	__xlCgenAppend(pBuf, "\tconst char* sCopy = (const char*)xrtCopyStr((str)sValue, strlen(sValue));\n");
	__xlCgenAppend(pBuf, "\treturn (sCopy != NULL) ? sCopy : sValue;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static xfuture* xl_generated_future_create_ready(ptr pValue, const char* sDebugName)\n{\n");
	__xlCgenAppend(pBuf, "\txfuture* pFuture = xFutureCreate();\n");
	__xlCgenAppend(pBuf, "\txpromise* pPromise;\n");
	__xlCgenAppend(pBuf, "\tif ( pFuture == NULL ) {\n\t\treturn NULL;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( (sDebugName != NULL) && (sDebugName[0] != '\\0') ) {\n\t\t(void)xFutureSetDebugName(pFuture, (str)sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\tpPromise = xPromiseCreate(pFuture);\n");
	__xlCgenAppend(pBuf, "\tif ( pPromise == NULL ) {\n\t\txFutureRelease(pFuture);\n\t\treturn NULL;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( !xPromiseResolve(pPromise, pValue) ) {\n\t\txPromiseDestroy(pPromise);\n\t\txFutureRelease(pFuture);\n\t\treturn NULL;\n\t}\n");
	__xlCgenAppend(pBuf, "\txPromiseDestroy(pPromise);\n");
	__xlCgenAppend(pBuf, "\treturn pFuture;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static xfuture* xl_generated_future_create_error(XL_GeneratedError* error, const char* sDebugName)\n{\n");
	__xlCgenAppend(pBuf, "\txfuture* pFuture = xFutureCreate();\n");
	__xlCgenAppend(pBuf, "\txpromise* pPromise;\n");
	__xlCgenAppend(pBuf, "\tconst char* sText;\n");
	__xlCgenAppend(pBuf, "\tstr sOwned;\n");
	__xlCgenAppend(pBuf, "\tif ( pFuture == NULL ) {\n\t\treturn NULL;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( (sDebugName != NULL) && (sDebugName[0] != '\\0') ) {\n\t\t(void)xFutureSetDebugName(pFuture, (str)sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\tpPromise = xPromiseCreate(pFuture);\n");
	__xlCgenAppend(pBuf, "\tif ( pPromise == NULL ) {\n\t\txFutureRelease(pFuture);\n\t\treturn NULL;\n\t}\n");
	__xlCgenAppend(pBuf, "\tsText = xl_generated_error_text(error);\n");
	__xlCgenAppend(pBuf, "\tsOwned = xrtCopyStr((str)((sText != NULL) ? sText : \"await failed\"), 0);\n");
	__xlCgenAppend(pBuf, "\tif ( sOwned == NULL ) {\n\t\tsOwned = xrtCopyStr((str)\"await failed\", 0);\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( !xPromiseReject(pPromise, XRT_NET_ERROR, sOwned) ) {\n\t\tif ( sOwned != NULL ) {\n\t\t\txrtFree(sOwned);\n\t\t}\n\t\txPromiseDestroy(pPromise);\n\t\txFutureRelease(pFuture);\n\t\treturn NULL;\n\t}\n");
	__xlCgenAppend(pBuf, "\txPromiseDestroy(pPromise);\n");
	__xlCgenAppend(pBuf, "\treturn pFuture;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "typedef struct XL_GeneratedAsyncForwardStateCtx {\n\txpromise* pPromise;\n\tconst char* sAwaitName;\n\tconst char* sFuncName;\n\tint iResumeStateId;\n} XL_GeneratedAsyncForwardStateCtx;\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_async_has_pending_error(void)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( xl_generated_had_exception ) {\n\t\treturn 1;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn xl_generated_capture_native_error();\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_reject_text(xpromise* pPromise, int32 iStatus, const char* sText)\n{\n");
	__xlCgenAppend(pBuf, "\tstr sOwned;\n");
	__xlCgenAppend(pBuf, "\tif ( pPromise == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tsOwned = xrtCopyStr((str)((sText != NULL) ? sText : \"await failed\"), 0);\n");
	__xlCgenAppend(pBuf, "\tif ( (sOwned == NULL) && ((sText == NULL) || (strcmp(sText, \"await failed\") != 0)) ) {\n\t\tsOwned = xrtCopyStr((str)\"await failed\", 0);\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( (iStatus == XRT_NET_OK) || (iStatus == XRT_NET_AGAIN) ) {\n\t\tiStatus = XRT_NET_ERROR;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( !xPromiseReject(pPromise, iStatus, sOwned) ) {\n\t\tif ( sOwned != NULL ) {\n\t\t\txrtFree(sOwned);\n\t\t}\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_reject_result(xpromise* pPromise, const xfuture_result* pIn, const char* sFallback)\n{\n");
	__xlCgenAppend(pBuf, "\tconst char* sText;\n\tstr sOwned;\n\tint32 iStatus;\n");
	__xlCgenAppend(pBuf, "\tif ( pPromise == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tsText = (sFallback != NULL) ? sFallback : \"await failed\";\n");
	__xlCgenAppend(pBuf, "\tiStatus = XRT_NET_ERROR;\n");
	__xlCgenAppend(pBuf, "\tif ( pIn != NULL ) {\n\t\tif ( (pIn->sError != NULL) && (pIn->sError[0] != '\\0') ) {\n\t\t\tsText = (const char*)pIn->sError;\n\t\t}\n\t\tif ( (pIn->iStatus != XRT_NET_OK) && (pIn->iStatus != XRT_NET_AGAIN) ) {\n\t\t\tiStatus = pIn->iStatus;\n\t\t}\n\t}\n");
	__xlCgenAppend(pBuf, "\tsOwned = xrtCopyStr((str)((sText != NULL) ? sText : \"await failed\"), 0);\n");
	__xlCgenAppend(pBuf, "\tif ( (sOwned == NULL) && ((sText == NULL) || (strcmp(sText, \"await failed\") != 0)) ) {\n\t\tsOwned = xrtCopyStr((str)\"await failed\", 0);\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( !xPromiseReject(pPromise, iStatus, sOwned) ) {\n\t\tif ( sOwned != NULL ) {\n\t\t\txrtFree(sOwned);\n\t\t}\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_reject_current(xpromise* pPromise, const char* sFallback)\n{\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedError* error;\n");
	__xlCgenAppend(pBuf, "\tif ( pPromise == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( !xl_generated_async_has_pending_error() ) {\n\t\treturn xl_generated_promise_reject_text(pPromise, XRT_NET_ERROR, sFallback);\n\t}\n");
	__xlCgenAppend(pBuf, "\terror = xl_generated_capture_exception_error();\n");
	__xlCgenAppend(pBuf, "\treturn xl_generated_promise_reject_text(pPromise, (error != NULL) ? error->code : XRT_NET_ERROR, xl_generated_error_text(error));\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_resolve_void(xpromise* pPromise)\n{\n\treturn (pPromise != NULL) ? xPromiseResolve(pPromise, NULL) : 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_resolve_int(xpromise* pPromise, intptr_t value)\n{\n\treturn (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)value) : 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_resolve_bool(xpromise* pPromise, int value)\n{\n\treturn (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)(intptr_t)(value ? 1 : 0)) : 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_resolve_float(xpromise* pPromise, double value)\n{\n");
	__xlCgenAppend(pBuf, "\tdouble* pHeapValue;\n");
	__xlCgenAppend(pBuf, "\tif ( pPromise == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpHeapValue = (double*)xrtMalloc(sizeof(double));\n\tif ( pHeapValue == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\t*pHeapValue = value;\n");
	__xlCgenAppend(pBuf, "\tif ( !xPromiseResolve(pPromise, (ptr)pHeapValue) ) {\n\t\txrtFree(pHeapValue);\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_resolve_string(xpromise* pPromise, const char* value)\n{\n\treturn (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)((value != NULL) ? value : \"\")) : 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_resolve_error(xpromise* pPromise, XL_GeneratedError* value)\n{\n\treturn (pPromise != NULL) ? xPromiseResolve(pPromise, (ptr)value) : 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_promise_resolve_ptr(xpromise* pPromise, ptr value)\n{\n\treturn (pPromise != NULL) ? xPromiseResolve(pPromise, value) : 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static void xl_generated_async_forward_state_finally(const xfuture_result* pIn, ptr pArg)\n{\n");
	__xlCgenAppend(pBuf, "\tXL_GeneratedAsyncForwardStateCtx* pCtx = (XL_GeneratedAsyncForwardStateCtx*)pArg;\n");
	__xlCgenAppend(pBuf, "\tif ( pCtx == NULL ) {\n\t\treturn;\n\t}\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_event(\"resume\", (pCtx->sAwaitName != NULL) ? pCtx->sAwaitName : \"\");\n");
	__xlCgenAppend(pBuf, "\tif ( pCtx->iResumeStateId >= 0 ) {\n\t\txl_generated_async_trace_state_event(pCtx->sFuncName, pCtx->iResumeStateId);\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( pCtx->pPromise != NULL ) {\n");
	__xlCgenAppend(pBuf, "\t\tif ( (pIn != NULL) && (pIn->iStatus == XRT_NET_OK) ) {\n\t\t\t(void)xPromiseResolve(pCtx->pPromise, pIn->pValue);\n\t\t} else {\n\t\t\t(void)xl_generated_promise_reject_result(pCtx->pPromise, pIn, \"await failed\");\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\txPromiseDestroy(pCtx->pPromise);\n\t}\n");
	__xlCgenAppend(pBuf, "\txrtFree(pCtx);\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static xfuture* xl_generated_future_forward_state(const char* sName, xfuture* pFuture, const char* sFunc, int iAwaitStateId, int iResumeStateId, const char* sDebugName)\n{\n");
	__xlCgenAppend(pBuf, "\txfuture* pOut;\n\txpromise* pPromise;\n\txfuture* pHook;\n\tXL_GeneratedAsyncForwardStateCtx* pCtx;\n");
	__xlCgenAppend(pBuf, "\tif ( iAwaitStateId >= 0 ) {\n\t\txl_generated_async_trace_state_event(sFunc, iAwaitStateId);\n\t}\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_event(\"await\", (sName != NULL) ? sName : \"\");\n");
	__xlCgenAppend(pBuf, "\tif ( pFuture == NULL ) {\n\t\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n\t\tif ( iResumeStateId >= 0 ) {\n\t\t\txl_generated_async_trace_state_event(sFunc, iResumeStateId);\n\t\t}\n\t\treturn xl_generated_future_create_error(xl_generated_error_make(\"Error\", \"future unavailable\", 0, \"\", \"\"), sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\tpOut = xFutureCreate();\n\tif ( pOut == NULL ) {\n\t\treturn xl_generated_future_create_error(xl_generated_error_make(\"Error\", \"async await forward alloc failed\", 0, \"\", \"\"), sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( (sDebugName != NULL) && (sDebugName[0] != '\\0') ) {\n\t\t(void)xFutureSetDebugName(pOut, (str)sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\tpPromise = xPromiseCreate(pOut);\n\tif ( pPromise == NULL ) {\n\t\txFutureRelease(pOut);\n\t\treturn xl_generated_future_create_error(xl_generated_error_make(\"Error\", \"async await forward promise failed\", 0, \"\", \"\"), sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\tpCtx = (XL_GeneratedAsyncForwardStateCtx*)xrtMalloc(sizeof(*pCtx));\n\tif ( pCtx == NULL ) {\n\t\txPromiseDestroy(pPromise);\n\t\txFutureRelease(pOut);\n\t\treturn xl_generated_future_create_error(xl_generated_error_make(\"Error\", \"async await forward context failed\", 0, \"\", \"\"), sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\tpCtx->pPromise = pPromise;\n\tpCtx->sAwaitName = sName;\n\tpCtx->sFuncName = sFunc;\n\tpCtx->iResumeStateId = iResumeStateId;\n");
	__xlCgenAppend(pBuf, "\tpHook = xFutureFinallyEngine(pFuture, NULL, 0u, xl_generated_async_forward_state_finally, pCtx);\n");
	__xlCgenAppend(pBuf, "\tif ( pHook == NULL ) {\n\t\txrtFree(pCtx);\n\t\txPromiseDestroy(pPromise);\n\t\txFutureRelease(pOut);\n\t\treturn xl_generated_future_create_error(xl_generated_error_make(\"Error\", \"async await forward hook failed\", 0, \"\", \"\"), sDebugName);\n\t}\n");
	__xlCgenAppend(pBuf, "\txFutureRelease(pHook);\n");
	__xlCgenAppend(pBuf, "\treturn pOut;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_future_wait_result(const char* sName, xfuture* pFuture, xfuture_result* pResult)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( pResult != NULL ) {\n\t\tmemset(pResult, 0, sizeof(*pResult));\n\t}\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_event(\"await\", (sName != NULL) ? sName : \"\");\n");
	__xlCgenAppend(pBuf, "\tif ( (pFuture == NULL) || (pResult == NULL) ) {\n\t\txl_generated_raise_error(xl_generated_error_make(\"Error\", \"future unavailable\", 0, \"\", \"\"));\n\t\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\t(void)xFutureWait(pFuture);\n");
	__xlCgenAppend(pBuf, "\t(void)xFutureGetResult(pFuture, pResult);\n");
	__xlCgenAppend(pBuf, "\tif ( pResult->iStatus != XRT_NET_OK ) {\n\t\txl_generated_raise_error(xl_generated_error_make(\"Error\", (const char*)((pResult->sError != NULL) ? pResult->sError : (str)\"await failed\"), pResult->iStatus, \"\", \"\"));\n\t\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_future_wait_result_state(const char* sName, xfuture* pFuture, xfuture_result* pResult, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( iAwaitStateId >= 0 ) {\n\t\txl_generated_async_trace_state_event(sFunc, iAwaitStateId);\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( !xl_generated_future_wait_result(sName, pFuture, pResult) ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( iResumeStateId >= 0 ) {\n\t\txl_generated_async_trace_state_event(sFunc, iResumeStateId);\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int32 xl_generated_future_all_pick_last(const xfuture_result* pIn, ptr pArg, xfuture_result* pOut)\n{\n");
	__xlCgenAppend(pBuf, "\txfuture_all_value* pAll;\n");
	__xlCgenAppend(pBuf, "\t(void)pArg;\n");
	__xlCgenAppend(pBuf, "\tif ( pOut == NULL ) {\n\t\treturn XRT_NET_ERROR;\n\t}\n");
	__xlCgenAppend(pBuf, "\tmemset(pOut, 0, sizeof(*pOut));\n");
	__xlCgenAppend(pBuf, "\tif ( pIn == NULL ) {\n\t\tpOut->iStatus = XRT_NET_ERROR;\n\t\tpOut->sError = (str)\"future all mapper input is null.\";\n\t\treturn pOut->iStatus;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( pIn->iStatus != XRT_NET_OK ) {\n\t\tpOut->iStatus = pIn->iStatus;\n\t\tpOut->sError = pIn->sError;\n\t\treturn pOut->iStatus;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpAll = (xfuture_all_value*)pIn->pValue;\n");
	__xlCgenAppend(pBuf, "\tif ( (pAll == NULL) || (pAll->iCount <= 0) || (pAll->arrValue == NULL) ) {\n\t\tpOut->iStatus = XRT_NET_ERROR;\n\t\tpOut->sError = (str)\"future all mapper missing values.\";\n\t\treturn pOut->iStatus;\n\t}\n");
	__xlCgenAppend(pBuf, "\tpOut->iStatus = XRT_NET_OK;\n");
	__xlCgenAppend(pBuf, "\tpOut->pValue = pAll->arrValue[pAll->iCount - 1];\n");
	__xlCgenAppend(pBuf, "\treturn pOut->iStatus;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static int32 xl_generated_task_delay_engine(xnetworker* pWorker, ptr pArg, xfuture_result* pOut)\n{\n");
	__xlCgenAppend(pBuf, "\t(void)pWorker;\n");
	__xlCgenAppend(pBuf, "\tif ( pOut == NULL ) {\n\t\treturn XRT_NET_ERROR;\n\t}\n");
	__xlCgenAppend(pBuf, "\tmemset(pOut, 0, sizeof(*pOut));\n");
	__xlCgenAppend(pBuf, "\tpOut->iStatus = XRT_NET_OK;\n");
	__xlCgenAppend(pBuf, "\tpOut->pValue = pArg;\n");
	__xlCgenAppend(pBuf, "\treturn pOut->iStatus;\n");
	__xlCgenAppend(pBuf, "}\n\n");
	__xlCgenAppend(pBuf, "static const char* xl_generated_type_of_value(xvalue value)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( value == NULL ) {\n\t\treturn \"null\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tswitch ( value->Type ) {\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_NULL: return \"null\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_BOOL: return \"bool\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_INT: return \"int\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_FLOAT: return \"float\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_TEXT: return \"string\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_TIME: return \"time\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_POINT: return \"point\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_FUNC: return \"function\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_ARRAY: return \"array\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_LIST: return \"list\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_COLL: return \"set\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_TABLE: return \"dict\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_CLASS: return \"class\";\n");
	__xlCgenAppend(pBuf, "\t\tcase XVO_DT_CUSTOM: return \"custom\";\n");
	__xlCgenAppend(pBuf, "\t\tdefault: return \"unknown\";\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static int xl_generated_capture_native_error(void)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( !xlrt_native_error_has() ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tconst char* sKind = xl_generated_copy_text(xlrt_native_error_kind());\n");
	__xlCgenAppend(pBuf, "\tconst char* sMessage = xl_generated_copy_text(xlrt_native_error_message());\n");
	__xlCgenAppend(pBuf, "\tconst char* sData = xl_generated_copy_text(xlrt_native_error_data());\n");
	__xlCgenAppend(pBuf, "\txl_generated_raise_error(xl_generated_error_make_ex(\n");
	__xlCgenAppend(pBuf, "\t\tsKind,\n");
	__xlCgenAppend(pBuf, "\t\tsMessage,\n");
	__xlCgenAppend(pBuf, "\t\txlrt_native_error_code(),\n");
	__xlCgenAppend(pBuf, "\t\tsData,\n");
	__xlCgenAppend(pBuf, "\t\t\"\",\n");
	__xlCgenAppend(pBuf, "\t\tNULL\n");
	__xlCgenAppend(pBuf, "\t));\n");
	__xlCgenAppend(pBuf, "\txlrt_native_error_clear();\n");
	__xlCgenAppend(pBuf, "\treturn 1;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_int(long long value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_int = value;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = NULL;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_float(double value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 2;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_float = value;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = NULL;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_string(const char* value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 3;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = value;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_text(const char* value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 3;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = NULL;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = value;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_error(XL_GeneratedError* value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 4;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_error = value;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = xl_generated_error_text(value);\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_int = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_float = 0.0;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static XL_GeneratedError* Error(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"Error\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* TypeError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"TypeError\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* ValueError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"ValueError\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* IOError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"IOError\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* TimeoutError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"TimeoutError\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* CancelledError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"CancelledError\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* NotImplementedError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"NotImplementedError\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* NativeError(const char* message, int code, const char* data, const char* stack, XL_GeneratedError* cause)\n{\n\treturn xl_generated_error_make_ex(\"NativeError\", message, code, data, stack, cause);\n}\n\n");
	__xlCgenAppend(pBuf, "static const char* xl_generated_error_type(XL_GeneratedError* value)\n{\n\treturn (value != NULL && value->type != NULL) ? value->type : \"Error\";\n}\n\n");
	__xlCgenAppend(pBuf, "static const char* xl_generated_error_message(XL_GeneratedError* value)\n{\n\treturn (value != NULL && value->message != NULL) ? value->message : \"\";\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_error_code(XL_GeneratedError* value)\n{\n\treturn (value != NULL) ? value->code : 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static const char* xl_generated_error_data(XL_GeneratedError* value)\n{\n\treturn (value != NULL && value->data != NULL) ? value->data : \"\";\n}\n\n");
	__xlCgenAppend(pBuf, "static const char* xl_generated_error_stack(XL_GeneratedError* value)\n{\n\treturn (value != NULL && value->stack != NULL) ? value->stack : \"\";\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_error_cause(XL_GeneratedError* value)\n{\n\treturn (value != NULL) ? value->cause : NULL;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_text_equal(const char* left, const char* right)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( left == NULL ) {\n\t\tleft = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( right == NULL ) {\n\t\tright = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn strcmp(left, right) == 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_number_approx(double left, double right)\n{\n");
	__xlCgenAppend(pBuf, "\tdouble diff = fabs(left - right);\n");
	__xlCgenAppend(pBuf, "\tdouble scale = fmax(fabs(left), fabs(right));\n");
	__xlCgenAppend(pBuf, "\tif ( scale < 1.0 ) {\n\t\tscale = 1.0;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn diff <= (0.000001 * scale);\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_time_approx(xtime left, xtime right)\n{\n");
	__xlCgenAppend(pBuf, "\txtime diff = left - right;\n");
	__xlCgenAppend(pBuf, "\tif ( diff < 0 ) {\n\t\tdiff = -diff;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn diff <= 1;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_text_has_wildcard(const char* value)\n{\n");
	__xlCgenAppend(pBuf, "\tsize_t i;\n");
	__xlCgenAppend(pBuf, "\tif ( value == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\tfor ( i = 0; value[i] != '\\0'; ++i ) {\n");
	__xlCgenAppend(pBuf, "\t\tif ( (value[i] == '*') || (value[i] == '?') ) {\n\t\t\treturn 1;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\treturn 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_text_pattern_match(const char* value, const char* pattern)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( value == NULL ) {\n\t\tvalue = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( pattern == NULL ) {\n\t\tpattern = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\twhile ( *pattern != '\\0' ) {\n");
	__xlCgenAppend(pBuf, "\t\tif ( *pattern == '*' ) {\n");
	__xlCgenAppend(pBuf, "\t\t\twhile ( *pattern == '*' ) {\n\t\t\t\t++pattern;\n\t\t\t}\n");
	__xlCgenAppend(pBuf, "\t\t\tif ( *pattern == '\\0' ) {\n\t\t\t\treturn 1;\n\t\t\t}\n");
	__xlCgenAppend(pBuf, "\t\t\twhile ( *value != '\\0' ) {\n");
	__xlCgenAppend(pBuf, "\t\t\t\tif ( xl_generated_text_pattern_match(value, pattern) ) {\n\t\t\t\t\treturn 1;\n\t\t\t\t}\n");
	__xlCgenAppend(pBuf, "\t\t\t\t++value;\n\t\t\t}\n");
	__xlCgenAppend(pBuf, "\t\t\treturn 0;\n");
	__xlCgenAppend(pBuf, "\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tif ( *value == '\\0' ) {\n\t\t\treturn 0;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tif ( (*pattern != '?') && (*pattern != *value) ) {\n\t\t\treturn 0;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\t++pattern;\n");
	__xlCgenAppend(pBuf, "\t\t++value;\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\treturn *value == '\\0';\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_text_approx(const char* left, const char* right)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( left == NULL ) {\n\t\tleft = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( right == NULL ) {\n\t\tright = \"\";\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( xl_generated_text_pattern_match(left, right) ) {\n\t\treturn 1;\n\t}\n");
	__xlCgenAppend(pBuf, "\tif ( xl_generated_text_has_wildcard(left) ) {\n\t\treturn xl_generated_text_pattern_match(right, left);\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn 0;\n}\n\n");
	__xlCgenAppend(pBuf, "static const char* xl_generated_null_assign_text(const char** pTarget, const char* value)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( (pTarget != NULL) && (*pTarget == NULL) ) {\n\t\t*pTarget = value;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn (pTarget != NULL) ? *pTarget : value;\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_GeneratedError* xl_generated_null_assign_error(XL_GeneratedError** pTarget, XL_GeneratedError* value)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( (pTarget != NULL) && (*pTarget == NULL) ) {\n\t\t*pTarget = value;\n\t}\n");
	__xlCgenAppend(pBuf, "\treturn (pTarget != NULL) ? *pTarget : value;\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_power_int(int base, int exponent)\n{\n");
	__xlCgenAppend(pBuf, "\tint result = 1;\n");
	__xlCgenAppend(pBuf, "\tint factor = base;\n");
	__xlCgenAppend(pBuf, "\tint power = exponent;\n");
	__xlCgenAppend(pBuf, "\tif ( power < 0 ) {\n");
	__xlCgenAppend(pBuf, "\t\tif ( base == 1 ) {\n\t\t\treturn 1;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tif ( base == -1 ) {\n\t\t\treturn ((power & 1) == 0) ? 1 : -1;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\twhile ( power > 0 ) {\n");
	__xlCgenAppend(pBuf, "\t\tif ( (power & 1) != 0 ) {\n\t\t\tresult *= factor;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t\tpower >>= 1;\n");
	__xlCgenAppend(pBuf, "\t\tif ( power > 0 ) {\n\t\t\tfactor *= factor;\n\t\t}\n");
	__xlCgenAppend(pBuf, "\t}\n");
	__xlCgenAppend(pBuf, "\treturn result;\n}\n\n");
	__xlCgenAppend(pBuf, "static double xl_generated_power_float(double base, double exponent)\n{\n\treturn pow(base, exponent);\n}\n\n");
	__xlCgenAppend(pBuf, "static int xl_generated_power_assign_int(int* pTarget, int exponent)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( pTarget == NULL ) {\n\t\treturn 0;\n\t}\n");
	__xlCgenAppend(pBuf, "\t*pTarget = xl_generated_power_int(*pTarget, exponent);\n");
	__xlCgenAppend(pBuf, "\treturn *pTarget;\n}\n\n");
	__xlCgenAppend(pBuf, "static double xl_generated_power_assign_float(double* pTarget, double exponent)\n{\n");
	__xlCgenAppend(pBuf, "\tif ( pTarget == NULL ) {\n\t\treturn 0.0;\n\t}\n");
	__xlCgenAppend(pBuf, "\t*pTarget = xl_generated_power_float(*pTarget, exponent);\n");
	__xlCgenAppend(pBuf, "\treturn *pTarget;\n}\n\n");

	__xlCgenFutureSupportTyped(pBuf, "Int", "int", "int", "0");
	__xlCgenFutureSupportTyped(pBuf, "Bool", "int", "bool", "0");
	__xlCgenFutureSupportTyped(pBuf, "Float", "double", "float", "0.0");
	__xlCgenFutureSupportTyped(pBuf, "String", "const char*", "string", "NULL");
	__xlCgenFutureSupportTyped(pBuf, "Error", "XL_GeneratedError*", "error", "NULL");
	__xlCgenFutureSupportTyped(pBuf, "Ptr", "void*", "ptr", "NULL");
	__xlCgenGeneratorSupportTyped(pBuf, "Int", "int", "int");
	__xlCgenGeneratorSupportTyped(pBuf, "Float", "double", "float");
	__xlCgenGeneratorSupportTyped(pBuf, "String", "const char*", "string");
	__xlCgenGeneratorSupportTyped(pBuf, "Error", "XL_GeneratedError*", "error");
	__xlCgenGeneratorSupportTyped(pBuf, "Bool", "int", "bool");
	__xlCgenGeneratorSupportTyped(pBuf, "Ptr", "void*", "ptr");
	__xlCgenTaskSupportTyped(pBuf, "int", "int", "int", "0");
	__xlCgenTaskSupportTyped(pBuf, "float", "float", "double", "0.0");
	__xlCgenTaskSupportTyped(pBuf, "string", "string", "const char*", "NULL");
	__xlCgenTaskSupportTyped(pBuf, "error", "error", "XL_GeneratedError*", "NULL");
	__xlCgenTaskSupportTyped(pBuf, "bool", "bool", "int", "0");
	__xlCgenTaskDelaySupport(pBuf);

	__xlCgenAppend(pBuf, "typedef struct XL_FutureVoid {\n\txfuture* handle;\n} XL_FutureVoid;\n\n");
	__xlCgenAppend(pBuf, "static XL_FutureVoid xl_future_void_ready(void)\n{\n\tXL_FutureVoid future;\n\tfuture.handle = xl_generated_future_create_ready(NULL, \"future<void>\");\n\treturn future;\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_FutureVoid xl_future_void_error(XL_GeneratedError* error)\n{\n\tXL_FutureVoid future;\n\tfuture.handle = xl_generated_future_create_error(error, \"future<void>\");\n\treturn future;\n}\n\n");
	__xlCgenAppend(pBuf, "static void xl_future_void_await(const char* sName, XL_FutureVoid future)\n{\n\txfuture_result result;\n\t(void)xl_generated_future_wait_result(sName, future.handle, &result);\n}\n\n");
	__xlCgenAppend(pBuf, "static void xl_future_void_await_state(const char* sName, XL_FutureVoid future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\txfuture_result result;\n\t(void)xl_generated_future_wait_result_state(sName, future.handle, &result, sFunc, iAwaitStateId, iResumeStateId);\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_FutureVoid xl_future_void_forward_state(const char* sName, XL_FutureVoid future, const char* sFunc, int iAwaitStateId, int iResumeStateId)\n{\n\tXL_FutureVoid out;\n\tout.handle = xl_generated_future_forward_state(sName, future.handle, sFunc, iAwaitStateId, iResumeStateId, \"future<void>\");\n\treturn out;\n}\n\n");
	__xlCgenAppend(pBuf, "typedef struct XL_GeneratedModuleExport {\n\tconst char* name;\n\tconst char* symbol;\n} XL_GeneratedModuleExport;\n\n");
	__xlCgenAppend(pBuf, "typedef struct XL_GeneratedModuleDescriptor {\n\tconst char* key;\n\tconst char* abiId;\n\tconst char* initSymbol;\n\tconst char* exportsSymbol;\n\tconst char* descriptorSymbol;\n\tsize_t exportCount;\n\tconst XL_GeneratedModuleExport* exports;\n} XL_GeneratedModuleDescriptor;\n\n");
}

static void __xlCgenThrowRaise(XL_CgenContext* pCtx, const XL_MirNode* pValue)
{
	const char* sTypeName;

	sTypeName = __xlInferExprTypeName(pCtx, pValue);
	__xlCgenAppendIndent(&pCtx->buf);
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_raise_int((long long)(");
		__xlCgenExpr(pCtx, pValue);
		__xlCgenAppend(&pCtx->buf, "));\n");
		return;
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_raise_float((double)(");
		__xlCgenExpr(pCtx, pValue);
		__xlCgenAppend(&pCtx->buf, "));\n");
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "string") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_raise_string((const char*)(");
		__xlCgenExpr(pCtx, pValue);
		__xlCgenAppend(&pCtx->buf, "));\n");
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "error") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_raise_error((");
		__xlCgenAppend(&pCtx->buf, __xlCTypeName("error"));
		__xlCgenAppend(&pCtx->buf, ")(");
		__xlCgenExpr(pCtx, pValue);
		__xlCgenAppend(&pCtx->buf, "));\n");
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "bool") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_raise_int((long long)(");
		__xlCgenExpr(pCtx, pValue);
		__xlCgenAppend(&pCtx->buf, "));\n");
		return;
	}

	if ( (pValue != NULL) && (pValue->sText != NULL) && (pValue->sText[0] != '\0') ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_raise_text(");
		__xlCgenAppendCString(&pCtx->buf, pValue->sText);
		__xlCgenAppend(&pCtx->buf, ");\n");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "xl_generated_raise_text(\"throw\");\n");
}

static void __xlCgenPropagateRaisedException(XL_CgenContext* pCtx, const XL_CgenFuncInfo* pFuncInfo)
{
	const XL_CgenTryFrame* pTryFrame;
	const char* sDefaultReturn;

	pTryFrame = __xlCgenCurrentTryFrame(pCtx);
	__xlCgenAppendIndent(&pCtx->buf);
	if ( (pTryFrame != NULL) && (pTryFrame->sExitLabel[0] != '\0') ) {
		__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pTryFrame->sExitLabel);
		return;
	}

	if ( (pFuncInfo != NULL) && pFuncInfo->bGenerator ) {
		__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pFuncInfo->sGeneratorDoneLabel);
		return;
	}
	if ( (pFuncInfo != NULL) && pFuncInfo->bAsync ) {
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_capture_exception_error());\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		return;
	}

	sDefaultReturn = __xlCgenFuncReturnDefaultValue(pFuncInfo);
	if ( sDefaultReturn == NULL ) {
		__xlCgenAppend(&pCtx->buf, "return;\n");
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "return %s;\n", sDefaultReturn);
	}
}

static void __xlCgenPropagatePendingException(XL_CgenContext* pCtx, const XL_CgenFuncInfo* pFuncInfo)
{
	if ( pFuncInfo == NULL ) {
		return;
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "if ( xl_generated_had_exception )\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenPropagateRaisedException(pCtx, pFuncInfo);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "if ( xl_generated_capture_native_error() )\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenPropagateRaisedException(pCtx, pFuncInfo);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static int __xlIsExpandedMultiReturnArg(const XL_MirNode* pNode)
{
	return (pNode != NULL) &&
		(pNode->eKind == XL_MIR_VALUE) &&
		(pNode->iChildCount == 2) &&
		(pNode->sText != NULL) &&
		(strcmp(pNode->sText, "__mret_item__") == 0);
}

static const XL_MirNode* __xlExpandedMultiReturnBaseCall(const XL_MirNode* pNode)
{
	return __xlIsExpandedMultiReturnArg(pNode) ? pNode->ppChildren[0] : NULL;
}

static int __xlCallHasExpandedMultiReturnArg(const XL_MirNode* pCall)
{
	const XL_MirNode* pArgs;
	size_t i;

	pArgs = ((pCall != NULL) && (pCall->iChildCount > 1)) ? pCall->ppChildren[1] : NULL;
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		if ( __xlIsExpandedMultiReturnArg(pArgs->ppChildren[i]) ) {
			return 1;
		}
	}
	return 0;
}

static const XL_MirNode* __xlFindMirSymbolDeclNode(const XL_MirNode* pNode, int iSymbolIndex)
{
	const XL_MirNode* pFound;
	size_t i;

	if ( (pNode == NULL) || (iSymbolIndex < 0) ) {
		return NULL;
	}
	if ( (pNode->iSymbolIndex == iSymbolIndex) &&
		(pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') &&
		((pNode->eKind == XL_MIR_BIND) || (pNode->eKind == XL_MIR_VALUE) || (pNode->eKind == XL_MIR_MEMBER)) ) {
		return pNode;
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		pFound = __xlFindMirSymbolDeclNode(pNode->ppChildren[i], iSymbolIndex);
		if ( pFound != NULL ) {
			return pFound;
		}
	}
	return NULL;
}

static const XL_MirNode* __xlFindMirBindDeclNode(const XL_MirNode* pNode, int iSymbolIndex)
{
	const XL_MirNode* pFound;
	size_t i;

	if ( (pNode == NULL) || (iSymbolIndex < 0) ) {
		return NULL;
	}
	if ( (pNode->iSymbolIndex == iSymbolIndex) && (pNode->eKind == XL_MIR_BIND) ) {
		return pNode;
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		pFound = __xlFindMirBindDeclNode(pNode->ppChildren[i], iSymbolIndex);
		if ( pFound != NULL ) {
			return pFound;
		}
	}
	return NULL;
}

static int __xlCollectGeneratorReturnSourceInfo(
	const XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode** ppFuncNode,
	const char** psValueTypeName,
	const char** psFuncName,
	int* pbFound,
	int iDepth
)
{
	const XL_MirNode* pResolvedFunc;
	const char* sResolvedValueType;
	const char* sResolvedFuncName;
	size_t i;

	if ( (pbFound == NULL) || (iDepth > 16) ) {
		return 0;
	}
	if ( pNode == NULL ) {
		return 1;
	}

	if ( pNode->eKind == XL_MIR_RETURN ) {
		if ( pNode->iChildCount == 0 ) {
			return 0;
		}
		pResolvedFunc = NULL;
		sResolvedValueType = NULL;
		sResolvedFuncName = NULL;
		if ( !__xlResolveGeneratorSourceInfo(
			pCtx,
			pNode->ppChildren[0],
			&pResolvedFunc,
			&sResolvedValueType,
			&sResolvedFuncName,
			iDepth + 1
		) ) {
			return 0;
		}
		if ( !(*pbFound) ) {
			if ( ppFuncNode != NULL ) {
				*ppFuncNode = pResolvedFunc;
			}
			if ( psValueTypeName != NULL ) {
				*psValueTypeName = sResolvedValueType;
			}
			if ( psFuncName != NULL ) {
				*psFuncName = sResolvedFuncName;
			}
			*pbFound = 1;
			return 1;
		}
		if ( (ppFuncNode != NULL) && (*ppFuncNode != pResolvedFunc) ) {
			return 0;
		}
		if ( (psValueTypeName != NULL) && (*psValueTypeName != NULL) && (sResolvedValueType != NULL) &&
			(strcmp(*psValueTypeName, sResolvedValueType) != 0) ) {
			return 0;
		}
		return 1;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( !__xlCollectGeneratorReturnSourceInfo(
			pCtx,
			pNode->ppChildren[i],
			ppFuncNode,
			psValueTypeName,
			psFuncName,
			pbFound,
			iDepth + 1
		) ) {
			return 0;
		}
	}

	return 1;
}

static int __xlResolveGeneratorReturnSourceInfo(
	const XL_CgenContext* pCtx,
	const XL_MirNode* pFuncNode,
	const XL_MirNode** ppFuncNode,
	const char** psValueTypeName,
	const char** psFuncName,
	int iDepth
)
{
	const XL_MirNode* pBody;
	const XL_MirNode* pResolvedFuncNode;
	const char* sResolvedValueTypeName;
	const char* sResolvedFuncName;
	int bFound;

	if ( ppFuncNode != NULL ) {
		*ppFuncNode = NULL;
	}
	if ( psValueTypeName != NULL ) {
		*psValueTypeName = NULL;
	}
	if ( psFuncName != NULL ) {
		*psFuncName = NULL;
	}
	if ( (pFuncNode == NULL) || (pFuncNode->eKind != XL_MIR_FUNC) || (iDepth > 16) ) {
		return 0;
	}

	pBody = (pFuncNode->iChildCount > 1) ? pFuncNode->ppChildren[1] : NULL;
	pResolvedFuncNode = NULL;
	sResolvedValueTypeName = NULL;
	sResolvedFuncName = NULL;
	bFound = 0;
	if ( !__xlCollectGeneratorReturnSourceInfo(
		pCtx,
		pBody,
		&pResolvedFuncNode,
		&sResolvedValueTypeName,
		&sResolvedFuncName,
		&bFound,
		iDepth + 1
	) ) {
		return 0;
	}
	if ( bFound ) {
		if ( ppFuncNode != NULL ) {
			*ppFuncNode = pResolvedFuncNode;
		}
		if ( psValueTypeName != NULL ) {
			*psValueTypeName = sResolvedValueTypeName;
		}
		if ( psFuncName != NULL ) {
			*psFuncName = sResolvedFuncName;
		}
	}
	return bFound;
}

static int __xlResolveGeneratorSourceInfo(
	const XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode** ppFuncNode,
	const char** psValueTypeName,
	const char** psFuncName,
	int iDepth
)
{
	const XL_MirNode* pFunc;
	const XL_MirNode* pDecl;

	if ( ppFuncNode != NULL ) {
		*ppFuncNode = NULL;
	}
	if ( psValueTypeName != NULL ) {
		*psValueTypeName = NULL;
	}
	if ( psFuncName != NULL ) {
		*psFuncName = NULL;
	}
	if ( (pCtx == NULL) || (pNode == NULL) || (iDepth > 16) ) {
		return 0;
	}

	if ( pNode->eKind == XL_MIR_CALL ) {
		pFunc = __xlResolveCallFuncNode(pCtx, pNode);
		if ( __xlIsGeneratorFuncNode(pFunc) ) {
			if ( ppFuncNode != NULL ) {
				*ppFuncNode = pFunc;
			}
			if ( psValueTypeName != NULL ) {
				*psValueTypeName = ((pFunc->sTypeName != NULL) && (pFunc->sTypeName[0] != '\0')) ? pFunc->sTypeName : "int";
			}
			if ( psFuncName != NULL ) {
				*psFuncName = (pFunc != NULL) ? pFunc->sText : __xlCallCalleeName(pNode);
			}
			return 1;
		}
		if ( (pFunc != NULL) &&
			(pFunc->sTypeName != NULL) &&
			(strcmp(pFunc->sTypeName, "generator") == 0) ) {
			return __xlResolveGeneratorReturnSourceInfo(pCtx, pFunc, ppFuncNode, psValueTypeName, psFuncName, iDepth + 1);
		}
	}

	if ( (pNode->iSymbolIndex >= 0) && (pCtx->pRoot != NULL) ) {
		pDecl = __xlFindMirBindDeclNode(pCtx->pRoot, pNode->iSymbolIndex);
		if ( (pDecl != NULL) && (pDecl != pNode) && (pDecl->eKind == XL_MIR_BIND) && (pDecl->iChildCount != 0) ) {
			return __xlResolveGeneratorSourceInfo(
				pCtx,
				pDecl->ppChildren[0],
				ppFuncNode,
				psValueTypeName,
				psFuncName,
				iDepth + 1
			);
		}
	}

	return 0;
}

static int __xlIsAbstractGeneratorBindRef(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pDecl;

	if ( (pCtx == NULL) || (pNode == NULL) || (pNode->iSymbolIndex < 0) || (pCtx->pRoot == NULL) ) {
		return 0;
	}

	pDecl = __xlFindMirBindDeclNode(pCtx->pRoot, pNode->iSymbolIndex);
	if ( (pDecl == NULL) || (pDecl->iChildCount == 0) || (pDecl->sTypeName == NULL) || (strcmp(pDecl->sTypeName, "generator") != 0) ) {
		return 0;
	}

	return __xlResolveGeneratorSourceInfo(pCtx, pDecl->ppChildren[0], NULL, NULL, NULL, 0) ? 0 : 1;
}

static int __xlCgenEmitAbstractGeneratorExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode, int bCloneAbstract)
{
	const XL_MirNode* pSourceFunc;
	const char* sValueTypeName;
	const char* sFuncName;
	char sGeneratorTypeName[256];
	char sGeneratorNextFuncName[256];
	char sTempName[64];

	if ( pCtx == NULL ) {
		return 0;
	}

	pSourceFunc = NULL;
	sValueTypeName = "var";
	sFuncName = "generator";
	if ( __xlResolveGeneratorSourceInfo(pCtx, pNode, &pSourceFunc, &sValueTypeName, &sFuncName, 0) ) {
		__xlCgenGeneratorTypeNameForFunc(sGeneratorTypeName, sizeof(sGeneratorTypeName), pSourceFunc);
		__xlCgenGeneratorNextFuncNameForFunc(sGeneratorNextFuncName, sizeof(sGeneratorNextFuncName), pSourceFunc);
		snprintf(sTempName, sizeof(sTempName), "xl_gen_box_%d_%d", (pNode != NULL) ? pNode->iLine : 0, (pNode != NULL) ? pNode->iColumn : 0);
		__xlCgenAppend(&pCtx->buf, "({ ");
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", sGeneratorTypeName, sTempName);
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, "; ");
		__xlCgenAppendFormat(
			&pCtx->buf,
			"xl_generated_generator_make((XL_GeneratedGeneratorNextRaw)%s, sizeof(%s), &%s, %s, ",
			sGeneratorNextFuncName,
			sTempName,
			sTempName,
			__xlCgenAbstractGeneratorKindName(sValueTypeName)
		);
		__xlCgenAppendCString(&pCtx->buf, sFuncName);
		__xlCgenAppend(&pCtx->buf, "); })");
		return 1;
	}

	if ( bCloneAbstract ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_generator_clone(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	return 0;
}

static void __xlCgenExpandedMultiReturnItemFromTemp(XL_CgenContext* pCtx, const char* sTempName, const XL_MirNode* pNode)
{
	const XL_MirNode* pIndex;
	const char* sValueType;
	int64_t iIndexValue;

	pIndex = (pNode != NULL) && (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	sValueType = __xlInferExprTypeName(pCtx, pNode);
	iIndexValue = (pIndex != NULL) ? pIndex->iIntValue : 0;

	if ( (sValueType != NULL) && (strcmp(sValueType, "int") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoArrayGetInt((xvalue)(%s), (uint32)(%lld))", sTempName, (long long)iIndexValue);
		return;
	}
	if ( (sValueType != NULL) && (strcmp(sValueType, "float") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoArrayGetFloat((xvalue)(%s), (uint32)(%lld))", sTempName, (long long)iIndexValue);
		return;
	}
	if ( (sValueType != NULL) && (strcmp(sValueType, "bool") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoArrayGetBool((xvalue)(%s), (uint32)(%lld))", sTempName, (long long)iIndexValue);
		return;
	}
	if ( (sValueType != NULL) && (strcmp(sValueType, "string") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoArrayGetText((xvalue)(%s), (uint32)(%lld))", sTempName, (long long)iIndexValue);
		return;
	}
	if ( (sValueType != NULL) && (strcmp(sValueType, "time") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoArrayGetTime((xvalue)(%s), (uint32)(%lld))", sTempName, (long long)iIndexValue);
		return;
	}
	__xlCgenAppendFormat(&pCtx->buf, "xvoArrayGetValue((xvalue)(%s), (uint32)(%lld))", sTempName, (long long)iIndexValue);
}

static void __xlCgenCallArgs(XL_CgenContext* pCtx, const XL_MirNode* pCall)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pFunc;
	const XL_MirNode* pParams;
	const XL_MirNode* pParam;
	size_t i;

	pArgs = ((pCall != NULL) && (pCall->iChildCount > 1)) ? pCall->ppChildren[1] : NULL;
	pFunc = __xlResolveCallFuncNode(pCtx, pCall);
	pParams = ((pFunc != NULL) && (pFunc->iChildCount != 0)) ? pFunc->ppChildren[0] : NULL;

	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		if ( i != 0 ) {
			__xlCgenAppend(&pCtx->buf, ", ");
		}
		pParam = ((pParams != NULL) && (i < pParams->iChildCount)) ? pParams->ppChildren[i] : NULL;
		if ( (pParam != NULL) && (pParam->sTypeName != NULL) && (strcmp(pParam->sTypeName, "generator") == 0) &&
			__xlCgenEmitAbstractGeneratorExpr(pCtx, pArgs->ppChildren[i], 1) ) {
			continue;
		}
		__xlCgenExprAsType(
			pCtx,
			pArgs->ppChildren[i],
			(pParam != NULL) ? pParam->sTypeName : NULL
		);
	}
}

static void __xlCgenBoxTypedCExpr(XL_CgenContext* pCtx, const char* sTypeName, const char* sExpr)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || (strcmp(sTypeName, "void") == 0) || (strcmp(sTypeName, "null") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreateNull()");
		return;
	}
	if ( __xlIsVarTypeName(sTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "((%s) != NULL ? xvoCopy((xvalue)(%s)) : xvoCreateNull())", (sExpr != NULL) ? sExpr : "NULL", (sExpr != NULL) ? sExpr : "NULL");
		return;
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoCreateInt((int64)(%s))", (sExpr != NULL) ? sExpr : "0");
		return;
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoCreateFloat((double)(%s))", (sExpr != NULL) ? sExpr : "0");
		return;
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoCreateBool((%s) ? TRUE : FALSE)", (sExpr != NULL) ? sExpr : "0");
		return;
	}
	if ( (strcmp(sTypeName, "string") == 0) || (strcmp(sTypeName, "type") == 0) ) {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"({ const char* xl_text_value = (%s != NULL) ? (%s) : \"\"; xvoCreateText((ptr)xl_text_value, (uint32)strlen(xl_text_value), FALSE); })",
			(sExpr != NULL) ? sExpr : "NULL",
			(sExpr != NULL) ? sExpr : "NULL"
		);
		return;
	}
	if ( __xlIsPointTypeName(sTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoCreatePoint((ptr)(%s))", (sExpr != NULL) ? sExpr : "NULL");
		return;
	}
	if ( __xlIsFunctionTypeName(sTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoCreateFunc((xfunction)(%s))", (sExpr != NULL) ? sExpr : "NULL");
		return;
	}
	if ( strcmp(sTypeName, "time") == 0 ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoCreateTime(%s)", (sExpr != NULL) ? sExpr : "0");
		return;
	}
	if ( __xlIsValueBackedTypeName(sTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoCopy((xvalue)(%s))", (sExpr != NULL) ? sExpr : "NULL");
		return;
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		__xlCgenAppendFormat(&pCtx->buf, "({ XL_GeneratedError* xl_err_value = (%s); const char* xl_err_text = xl_generated_error_text(xl_err_value); xvoCreateText((ptr)xl_err_text, (uint32)strlen((xl_err_text != NULL) ? xl_err_text : \"\"), FALSE); })", (sExpr != NULL) ? sExpr : "NULL");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "xvoCreateNull()");
}

static void __xlCgenYieldExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_CgenFuncInfo* pFuncInfo;
	int iResumeState;
	int bExternalResumeState;
	char sResumeResultName[64];

	pFuncInfo = (pCtx != NULL) ? pCtx->pCurrentFuncInfo : NULL;
	if ( (pFuncInfo == NULL) || !pFuncInfo->bGenerator ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreateNull()");
		return;
	}

	iResumeState = 0;
	bExternalResumeState = 0;
	if ( (pCtx != NULL) && (pCtx->iGeneratorExprResumeState > 0) ) {
		iResumeState = pCtx->iGeneratorExprResumeState;
		bExternalResumeState = 1;
		pCtx->iGeneratorExprResumeState = 0;
	}
	iResumeState = __xlCgenResolveGeneratorResumeState(
		pFuncInfo,
		pNode,
		iResumeState,
		bExternalResumeState,
		1
	);
	snprintf(sResumeResultName, sizeof(sResumeResultName), "xl_gen_resume_result_%d_%d", (pNode != NULL) ? pNode->iLine : 0, (pNode != NULL) ? pNode->iColumn : 0);

	if ( bExternalResumeState ) {
		__xlCgenAppend(&pCtx->buf, "({ ");
		__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = NULL; ", sResumeResultName);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"if ( xl_generated_generator_executor_can_resume(&%s->executor, %d) ) { ",
			(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
			iResumeState
		);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"(void)xl_generated_generator_executor_resume(&%s->executor, %d); ",
			(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
			iResumeState
		);
		__xlCgenAppendHybridGroupTraceEventInline(pCtx, pFuncInfo, iResumeState, "resume_group");
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s = ((xl_gen_resume_value != NULL) ? xvoCopy((xvalue)(xl_gen_resume_value)) : xvoCreateNull()); ",
			sResumeResultName
		);
		__xlCgenAppend(&pCtx->buf, "} else { ");
		__xlCgenAppendFormat(
			&pCtx->buf,
			"*(%s) = ",
			(pFuncInfo->sGeneratorValueOutName != NULL) ? pFuncInfo->sGeneratorValueOutName : "pValue"
		);
		if ( (pNode != NULL) && (pNode->iChildCount != 0) ) {
			__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		} else {
			__xlCgenAppend(&pCtx->buf, __xlCDefaultReturnValue(pFuncInfo->sTypeName));
		}
		__xlCgenAppend(&pCtx->buf, "; ");
		__xlCgenPropagatePendingException(pCtx, pFuncInfo);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"xl_generated_generator_executor_suspend(&%s->executor, %d, ",
			(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
			iResumeState
		);
		__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
		__xlCgenAppend(&pCtx->buf, "); ");
		__xlCgenAppendHybridGroupTraceEventInline(pCtx, pFuncInfo, iResumeState, "group");
		__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"next\", ");
		__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
		__xlCgenAppend(&pCtx->buf, "); ");
		__xlCgenAppend(&pCtx->buf, "return 1; } ");
		__xlCgenAppend(&pCtx->buf, sResumeResultName);
		__xlCgenAppend(&pCtx->buf, "; })");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "({ ");
	__xlCgenAppendFormat(
		&pCtx->buf,
		"*(%s) = ",
		(pFuncInfo->sGeneratorValueOutName != NULL) ? pFuncInfo->sGeneratorValueOutName : "pValue"
	);
	if ( (pNode != NULL) && (pNode->iChildCount != 0) ) {
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
	} else {
		__xlCgenAppend(&pCtx->buf, __xlCDefaultReturnValue(pFuncInfo->sTypeName));
	}
	__xlCgenAppend(&pCtx->buf, "; ");
	__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"xl_generated_generator_executor_suspend(&%s->executor, %d, ",
		(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
		iResumeState
	);
	__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
	__xlCgenAppend(&pCtx->buf, "); ");
	__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"next\", ");
	__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
	__xlCgenAppend(&pCtx->buf, "); ");
	__xlCgenAppend(&pCtx->buf, "return 1; ");
	__xlCgenAppendFormat(&pCtx->buf, "xl_gen_resume_%d: ", iResumeState);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"(void)xl_generated_generator_executor_resume(&%s->executor, %d); ",
		(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
		iResumeState
	);
	__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = ", sResumeResultName);
	__xlCgenAppend(&pCtx->buf, "((xl_gen_resume_value != NULL) ? xvoCopy((xvalue)(xl_gen_resume_value)) : xvoCreateNull()); ");
	__xlCgenAppend(&pCtx->buf, sResumeResultName);
	__xlCgenAppend(&pCtx->buf, "; })");
}

static void __xlCgenFunctionValueCallExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pCallee;
	const char* sCalleeType;
	const char* sResultType;
	size_t i;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sCalleeType = __xlInferExprTypeName(pCtx, pCallee);
	sResultType = __xlInferExprTypeName(pCtx, pNode);

	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_call_args = xvoCreateArray(); ");
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		const XL_MirNode* pArg;

		pArg = pArgs->ppChildren[i];
		__xlCgenAppend(&pCtx->buf, "(void)xvoArrayAppendValue(xl_call_args, ");
		__xlCgenBoxValueExpr(pCtx, pArg);
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	}
	__xlCgenAppend(&pCtx->buf, "xvalue xl_call_result_value = ((xfunction)(");
	__xlCgenExprAsType(pCtx, pCallee, sCalleeType);
	__xlCgenAppend(&pCtx->buf, "))(NULL, xl_call_args); xvoUnref(xl_call_args); ");
	if ( (sResultType == NULL) || (sResultType[0] == '\0') || (strcmp(sResultType, "void") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "if ( xl_call_result_value != NULL ) xvoUnref(xl_call_result_value); })");
		return;
	}

	__xlCgenAppendFormat(&pCtx->buf, "%s xl_call_result = ", __xlCTypeName(sResultType));
	__xlCgenValueExprAsType(pCtx, "xl_call_result_value", sResultType);
	__xlCgenAppend(&pCtx->buf, "; if ( xl_call_result_value != NULL ) xvoUnref(xl_call_result_value); xl_call_result; })");
}

static void __xlCgenCallExprWithExpandedArgs(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pFunc;
	const XL_MirNode* pArgs;
	const XL_MirNode* pCallee;
	const char* sCalleeType;
	const char* sFuncSymbol;
	const char* sResultType;
	size_t i;

	pFunc = __xlResolveCallFuncNode(pCtx, pNode);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sCalleeType = __xlInferExprTypeName(pCtx, pCallee);
	sFuncSymbol = __xlMirFuncSymbolName(pFunc);
	sResultType = __xlInferExprTypeName(pCtx, pNode);

	__xlCgenAppend(&pCtx->buf, "({ ");
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		const XL_MirNode* pArg;
		const XL_MirNode* pBaseCall;
		const XL_MirNode* pPrevBaseCall;
		char sTempName[64];

		pArg = pArgs->ppChildren[i];
		if ( !__xlIsExpandedMultiReturnArg(pArg) ) {
			continue;
		}
		pBaseCall = __xlExpandedMultiReturnBaseCall(pArg);
		pPrevBaseCall = (i > 0) ? __xlExpandedMultiReturnBaseCall(pArgs->ppChildren[i - 1]) : NULL;
		if ( (pPrevBaseCall != NULL) && (pBaseCall != NULL) &&
			(pPrevBaseCall->iLine == pBaseCall->iLine) && (pPrevBaseCall->iColumn == pBaseCall->iColumn) ) {
			continue;
		}
		snprintf(sTempName, sizeof(sTempName), "xl_mret_%d_%d_%llu", (pBaseCall != NULL) ? pBaseCall->iLine : 0, (pBaseCall != NULL) ? pBaseCall->iColumn : 0, (unsigned long long)i);
		__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = ", sTempName);
		__xlCgenExpr(pCtx, pBaseCall);
		__xlCgenAppend(&pCtx->buf, "; ");
	}

	if ( (sResultType != NULL) && (sResultType[0] != '\0') && (strcmp(sResultType, "void") != 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s xl_call_result = ", __xlCTypeName(sResultType));
	}
	if ( (pFunc != NULL) && (sFuncSymbol != NULL) && (sFuncSymbol[0] != '\0') ) {
		__xlCgenAppend(&pCtx->buf, sFuncSymbol);
	} else if ( __xlIsFunctionTypeName(sCalleeType) ) {
		__xlCgenFunctionValueCallExpr(pCtx, pNode);
		return;
	} else {
		__xlCgenExpr(pCtx, pCallee);
	}
	__xlCgenAppend(&pCtx->buf, "(");
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		const XL_MirNode* pArg;
		const XL_MirNode* pBaseCall;
		const XL_MirNode* pPrevBaseCall;
		const XL_MirNode* pParams;
		const XL_MirNode* pParam;
		size_t iTempGroup;
		char sTempName[64];

		if ( i != 0 ) {
			__xlCgenAppend(&pCtx->buf, ", ");
		}
		pArg = pArgs->ppChildren[i];
		pParams = ((pFunc != NULL) && (pFunc->iChildCount != 0)) ? pFunc->ppChildren[0] : NULL;
		pParam = ((pParams != NULL) && (i < pParams->iChildCount)) ? pParams->ppChildren[i] : NULL;
		if ( __xlIsExpandedMultiReturnArg(pArg) ) {
			pBaseCall = __xlExpandedMultiReturnBaseCall(pArg);
			iTempGroup = i;
			while ( iTempGroup > 0 ) {
				pPrevBaseCall = __xlExpandedMultiReturnBaseCall(pArgs->ppChildren[iTempGroup - 1]);
				if ( (pPrevBaseCall == NULL) || (pBaseCall == NULL) ||
					(pPrevBaseCall->iLine != pBaseCall->iLine) || (pPrevBaseCall->iColumn != pBaseCall->iColumn) ) {
					break;
				}
				--iTempGroup;
			}
			snprintf(sTempName, sizeof(sTempName), "xl_mret_%d_%d_%llu", (pBaseCall != NULL) ? pBaseCall->iLine : 0, (pBaseCall != NULL) ? pBaseCall->iColumn : 0, (unsigned long long)iTempGroup);
			__xlCgenExpandedMultiReturnItemFromTemp(pCtx, sTempName, pArg);
		} else {
			if ( (pParam != NULL) && (pParam->sTypeName != NULL) && (strcmp(pParam->sTypeName, "generator") == 0) &&
				__xlCgenEmitAbstractGeneratorExpr(pCtx, pArg, 1) ) {
				continue;
			}
			__xlCgenExprAsType(
				pCtx,
				pArg,
				(pParam != NULL) ? pParam->sTypeName : NULL
			);
		}
	}
	__xlCgenAppend(&pCtx->buf, ")");
	if ( (sResultType != NULL) && (sResultType[0] != '\0') && (strcmp(sResultType, "void") != 0) ) {
		__xlCgenAppend(&pCtx->buf, "; xl_call_result; })");
	} else {
		__xlCgenAppend(&pCtx->buf, "; 0; })");
	}
}

static void __xlCgenTryStmt(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo)
{
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pCatchName;
	const XL_MirNode* pCatchBlock;
	const XL_MirNode* pFinallyBlock;
	const char* sCatchName;
	char sExitLabel[64];
	char sCheckLabel[64];
	char sFinallyLabel[64];
	char sDispatchLabel[64];
	char sEndLabel[64];
	char sFrameName[64];
	char sGeneratorFrameFieldName[64];
	int bHasCatch;
	int bHasFinally;
	int bGeneratorMode;
	const XL_CgenFinallyFrame* pOuterFinallyFrame;

	pTryBlock = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pCatchName = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pCatchBlock = (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL;
	pFinallyBlock = (pNode->iChildCount > 3) ? pNode->ppChildren[3] : NULL;
	sCatchName = ((pCatchName != NULL) && (pCatchName->sText != NULL) && (pCatchName->sText[0] != '\0')) ? pCatchName->sText : NULL;
	bHasCatch = (pCatchBlock != NULL);
	bHasFinally = (pFinallyBlock != NULL);

	snprintf(sExitLabel, sizeof(sExitLabel), "xl_try_exit_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sCheckLabel, sizeof(sCheckLabel), "xl_try_check_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sFinallyLabel, sizeof(sFinallyLabel), "xl_try_finally_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sDispatchLabel, sizeof(sDispatchLabel), "xl_try_dispatch_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sEndLabel, sizeof(sEndLabel), "xl_try_end_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sFrameName, sizeof(sFrameName), "xl_try_frame_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sGeneratorFrameFieldName, sizeof(sGeneratorFrameFieldName), "xl_try_frame_%d_%d", pNode->iLine, pNode->iColumn);

	bHasCatch = (pNode->bFlag & 1) != 0;
	bHasFinally = (pNode->bFlag & 2) != 0;
	bGeneratorMode = (pFuncInfo != NULL) && pFuncInfo->bGenerator;
	pOuterFinallyFrame = NULL;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	if ( bGeneratorMode ) {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"XL_GeneratedExceptionFrame* %s = &%s->%s;\n",
			sFrameName,
			(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
			sGeneratorFrameFieldName
		);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "memset(%s, 0, sizeof(*%s));\n", sFrameName, sFrameName);
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "XL_GeneratedExceptionFrame* %s = xl_generated_exception_frame_push();\n", sFrameName);
	}
	if ( sCatchName != NULL ) {
		__xlCgenAppendIndent(&pCtx->buf);
		if ( bGeneratorMode ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s = NULL;\n", sCatchName);
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = NULL;\n", __xlCTypeName("error"), sCatchName);
		}
	}
	if ( bHasFinally ) {
		__xlCgenPushFinallyFrame(pCtx, sFinallyLabel, sDispatchLabel);
		pOuterFinallyFrame = __xlCgenOuterFinallyFrame(pCtx);
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "/* try */\n");
	__xlCgenPushTryFrame(pCtx, sExitLabel);
	__xlCgenStmt(pCtx, pTryBlock, pFuncInfo);
	__xlCgenPopTryFrame(pCtx);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", sCheckLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sExitLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sCheckLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( xl_generated_exception_frame_capture(%s) )\n", sFrameName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	if ( bHasCatch ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s->phase == 0 )\n", sFrameName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		if ( sCatchName != NULL ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(
				&pCtx->buf,
				"%s = xl_generated_exception_value_error(%s->kind, %s->text, %s->error, %s->int_value, %s->float_value);\n",
				sCatchName,
				sFrameName,
				sFrameName,
				sFrameName,
				sFrameName,
				sFrameName
			);
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s->phase = 1;\n", sFrameName);
		__xlCgenPushTryFrame(pCtx, sExitLabel);
		__xlCgenStmt(pCtx, pCatchBlock, pFuncInfo);
		__xlCgenPopTryFrame(pCtx);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", bHasFinally ? sFinallyLabel : sEndLabel);
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s->needs_rethrow = 1;\n", sFrameName);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	if ( bHasFinally ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", sFinallyLabel);
	}

	if ( bHasFinally ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sFinallyLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenSetCurrentFinallyBody(pCtx, 1);
		__xlCgenStmt(pCtx, pFinallyBlock, pFuncInfo);
		__xlCgenSetCurrentFinallyBody(pCtx, 0);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sDispatchLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		if ( (pFuncInfo != NULL) && (pFuncInfo->sEscapeKindName != NULL) && (pFuncInfo->sEscapeKindName[0] != '\0') ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( %s != 0 )\n", pFuncInfo->sEscapeKindName);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			if ( !bGeneratorMode ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_exception_frame_pop();\n");
			}
			if ( (pOuterFinallyFrame != NULL) && (pOuterFinallyFrame->sFinallyLabel[0] != '\0') ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pOuterFinallyFrame->sFinallyLabel);
			} else {
				__xlCgenEmitEscapeDispatch(pCtx, pFuncInfo);
			}
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		}
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s->needs_rethrow && !xl_generated_had_exception )\n", sFrameName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xl_generated_exception_frame_restore(%s);\n", sFrameName);
	if ( !bGeneratorMode ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xl_generated_exception_frame_pop();\n");
	}
	__xlCgenPropagateRaisedException(pCtx, pFuncInfo);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sEndLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	if ( !bGeneratorMode ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xl_generated_exception_frame_pop();\n");
	}
	if ( bHasFinally ) {
		__xlCgenPopFinallyFrame(pCtx);
	}
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static const char* __xlCallCalleeName(const XL_MirNode* pCall)
{
	const XL_MirNode* pCallee;

	if ( (pCall == NULL) || (pCall->eKind != XL_MIR_CALL) || (pCall->iChildCount == 0) ) {
		return NULL;
	}

	pCallee = pCall->ppChildren[0];
	if ( (pCallee == NULL) || (pCallee->sText == NULL) || (pCallee->sText[0] == '\0') || (pCallee->iChildCount != 0) ) {
		return NULL;
	}

	return pCallee->sText;
}

static XL_TaskHelperKind __xlTaskHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_TASK_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_task__spawn") == 0 ) {
		return XL_TASK_HELPER_SPAWN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_task__all") == 0 ) {
		return XL_TASK_HELPER_ALL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_task__race") == 0 ) {
		return XL_TASK_HELPER_RACE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_task__cancel") == 0 ) {
		return XL_TASK_HELPER_CANCEL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_task__delay") == 0 ) {
		return XL_TASK_HELPER_DELAY;
	}
	return XL_TASK_HELPER_NONE;
}

static XL_CorePrintHelperKind __xlCorePrintHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_CORE_PRINT_NONE;
	}
	if ( (strcmp(sCalleeName, "xl_mod_std_core__print") == 0) ||
		(strcmp(sCalleeName, "xl_mod_std_io__print") == 0) ) {
		return XL_CORE_PRINT;
	}
	if ( (strcmp(sCalleeName, "xl_mod_std_core__println") == 0) ||
		(strcmp(sCalleeName, "xl_mod_std_io__println") == 0) ) {
		return XL_CORE_PRINTLN;
	}
	return XL_CORE_PRINT_NONE;
}

static XL_IoHelperKind __xlIoHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_IO_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_io__eprint") == 0 ) {
		return XL_IO_HELPER_EPRINT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_io__eprintln") == 0 ) {
		return XL_IO_HELPER_EPRINTLN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_io__read_line") == 0 ) {
		return XL_IO_HELPER_READ_LINE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_io__flush") == 0 ) {
		return XL_IO_HELPER_FLUSH;
	}
	return XL_IO_HELPER_NONE;
}

static XL_CoreLenHelperKind __xlCoreLenHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_CORE_LEN_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_core__len") == 0 ) {
		return XL_CORE_LEN;
	}
	return XL_CORE_LEN_NONE;
}

static const XL_MirNode* __xlContainerMethodBase(const XL_MirNode* pCallNode)
{
	const XL_MirNode* pCallee;
	const char* sCalleeName;
	const char* sBaseTypeName;
	const XL_MirNode* pArgs;

	if ( (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) || (pCallNode->iChildCount == 0) ) {
		return NULL;
	}
	pCallee = pCallNode->ppChildren[0];
	if ( (pCallee != NULL) && (pCallee->eKind == XL_MIR_MEMBER) && (pCallee->iChildCount != 0) ) {
		sBaseTypeName = (pCallee->ppChildren[0] != NULL) ? pCallee->ppChildren[0]->sTypeName : NULL;
		if ( __xlTypeHasBaseName(sBaseTypeName, "array") ||
			__xlTypeHasBaseName(sBaseTypeName, "list") ||
			__xlTypeHasBaseName(sBaseTypeName, "set") ||
			__xlTypeHasBaseName(sBaseTypeName, "dict") ) {
			return pCallee->ppChildren[0];
		}
		return NULL;
	}

	sCalleeName = __xlCallCalleeName(pCallNode);
	if ( (sCalleeName == NULL) ||
		((strcmp(sCalleeName, "__xl_method_has") != 0) &&
		(strcmp(sCalleeName, "__xl_method_append") != 0) &&
		(strcmp(sCalleeName, "__xl_method_join") != 0) &&
		(strcmp(sCalleeName, "__xl_method_clear") != 0) &&
		(strcmp(sCalleeName, "__xl_method_remove") != 0)) ) {
		return NULL;
	}

	pArgs = (pCallNode->iChildCount > 1) ? pCallNode->ppChildren[1] : NULL;
	return ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
}

static const XL_MirNode* __xlChannelMethodBase(const XL_MirNode* pCallNode)
{
	const XL_MirNode* pCallee;
	const char* sCalleeName;
	const char* sMethodName;
	const char* sBaseTypeName;
	const XL_MirNode* pArgs;

	if ( (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) || (pCallNode->iChildCount == 0) ) {
		return NULL;
	}
	pCallee = pCallNode->ppChildren[0];
	if ( (pCallee != NULL) && (pCallee->eKind == XL_MIR_MEMBER) && (pCallee->iChildCount != 0) ) {
		sMethodName = pCallee->sText;
		sBaseTypeName = (pCallee->ppChildren[0] != NULL) ? pCallee->ppChildren[0]->sTypeName : NULL;
		if ( (sMethodName != NULL) && (sBaseTypeName != NULL) && (strcmp(sBaseTypeName, "channel") == 0) &&
			((strcmp(sMethodName, "send") == 0) || (strcmp(sMethodName, "recv") == 0) || (strcmp(sMethodName, "close") == 0)) ) {
			return pCallee->ppChildren[0];
		}
		return NULL;
	}

	sCalleeName = __xlCallCalleeName(pCallNode);
	if ( (sCalleeName == NULL) ||
		((strcmp(sCalleeName, "__xl_method_channel_send") != 0) &&
		(strcmp(sCalleeName, "__xl_method_channel_recv") != 0) &&
		(strcmp(sCalleeName, "__xl_method_channel_close") != 0)) ) {
		return NULL;
	}

	pArgs = (pCallNode->iChildCount > 1) ? pCallNode->ppChildren[1] : NULL;
	return ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
}

static const char* __xlContainerMethodName(const XL_MirNode* pCallNode)
{
	const XL_MirNode* pCallee;
	const char* sCalleeName;

	if ( (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) || (pCallNode->iChildCount == 0) ) {
		return NULL;
	}
	pCallee = pCallNode->ppChildren[0];
	if ( (pCallee != NULL) && (pCallee->eKind == XL_MIR_MEMBER) ) {
		return pCallee->sText;
	}

	sCalleeName = __xlCallCalleeName(pCallNode);
	if ( sCalleeName == NULL ) {
		return NULL;
	}
	if ( strcmp(sCalleeName, "__xl_method_has") == 0 ) {
		return "has";
	}
	if ( strcmp(sCalleeName, "__xl_method_append") == 0 ) {
		return "append";
	}
	if ( strcmp(sCalleeName, "__xl_method_join") == 0 ) {
		return "join";
	}
	if ( strcmp(sCalleeName, "__xl_method_clear") == 0 ) {
		return "clear";
	}
	if ( strcmp(sCalleeName, "__xl_method_remove") == 0 ) {
		return "remove";
	}
	return NULL;
}

static const char* __xlChannelMethodName(const XL_MirNode* pCallNode)
{
	const XL_MirNode* pCallee;
	const char* sCalleeName;

	if ( (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) || (pCallNode->iChildCount == 0) ) {
		return NULL;
	}
	pCallee = pCallNode->ppChildren[0];
	if ( (pCallee != NULL) && (pCallee->eKind == XL_MIR_MEMBER) ) {
		return pCallee->sText;
	}

	sCalleeName = __xlCallCalleeName(pCallNode);
	if ( sCalleeName == NULL ) {
		return NULL;
	}
	if ( strcmp(sCalleeName, "__xl_method_channel_send") == 0 ) {
		return "send";
	}
	if ( strcmp(sCalleeName, "__xl_method_channel_recv") == 0 ) {
		return "recv";
	}
	if ( strcmp(sCalleeName, "__xl_method_channel_close") == 0 ) {
		return "close";
	}
	return NULL;
}

static const XL_MirNode* __xlGeneratorMethodBase(const XL_MirNode* pCallNode)
{
	const XL_MirNode* pCallee;
	const char* sCalleeName;
	const char* sMethodName;
	const char* sBaseTypeName;
	const XL_MirNode* pArgs;

	if ( (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) || (pCallNode->iChildCount == 0) ) {
		return NULL;
	}
	pCallee = pCallNode->ppChildren[0];
	if ( (pCallee != NULL) && (pCallee->eKind == XL_MIR_MEMBER) && (pCallee->iChildCount != 0) ) {
		sMethodName = pCallee->sText;
		sBaseTypeName = (pCallee->ppChildren[0] != NULL) ? pCallee->ppChildren[0]->sTypeName : NULL;
		if ( (sMethodName != NULL) && (sBaseTypeName != NULL) && (strcmp(sBaseTypeName, "generator") == 0) &&
			((strcmp(sMethodName, "next") == 0) || (strcmp(sMethodName, "resume") == 0)) ) {
			return pCallee->ppChildren[0];
		}
		return NULL;
	}

	sCalleeName = __xlCallCalleeName(pCallNode);
	if ( (sCalleeName == NULL) ||
		((strcmp(sCalleeName, "__xl_method_generator_next") != 0) &&
		(strcmp(sCalleeName, "__xl_method_generator_resume") != 0)) ) {
		return NULL;
	}

	pArgs = (pCallNode->iChildCount > 1) ? pCallNode->ppChildren[1] : NULL;
	return ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
}

static const char* __xlGeneratorMethodName(const XL_MirNode* pCallNode)
{
	const XL_MirNode* pCallee;
	const char* sCalleeName;

	if ( (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) || (pCallNode->iChildCount == 0) ) {
		return NULL;
	}
	pCallee = pCallNode->ppChildren[0];
	if ( (pCallee != NULL) && (pCallee->eKind == XL_MIR_MEMBER) ) {
		return pCallee->sText;
	}

	sCalleeName = __xlCallCalleeName(pCallNode);
	if ( sCalleeName == NULL ) {
		return NULL;
	}
	if ( strcmp(sCalleeName, "__xl_method_generator_next") == 0 ) {
		return "next";
	}
	if ( strcmp(sCalleeName, "__xl_method_generator_resume") == 0 ) {
		return "resume";
	}
	return NULL;
}

static int __xlGeneratorBaseCanMutateInPlace(const XL_MirNode* pBase)
{
	return (pBase != NULL) &&
		(pBase->iChildCount == 0) &&
		(pBase->sText != NULL) &&
		(pBase->sText[0] != '\0');
}

static void __xlCgenGeneratorMethodCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pSourceFunc;
	const XL_CgenFuncInfo* pFuncInfo;
	const char* sMethodName;
	const char* sFuncName;
	const char* sValueTypeName;
	const char* sGeneratorPtrExpr;
	char sGeneratorTypeName[256];
	char sGeneratorNextFuncName[256];
	char sTempGeneratorName[64];
	char sTempGeneratorPtrName[64];
	char sResumeArgName[64];
	char sValueName[64];
	char sOkName[64];
	char sResultName[64];

	if ( (pCtx == NULL) || (pNode == NULL) ) {
		return;
	}

	pBase = __xlGeneratorMethodBase(pNode);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	pFuncInfo = pCtx->pCurrentFuncInfo;
	sMethodName = __xlGeneratorMethodName(pNode);
	pSourceFunc = NULL;
	sFuncName = "generator";
	sValueTypeName = "var";
	sGeneratorTypeName[0] = '\0';
	sGeneratorNextFuncName[0] = '\0';

	(void)__xlResolveGeneratorSourceInfo(pCtx, pBase, &pSourceFunc, &sValueTypeName, &sFuncName, 0);
	if ( __xlIsAbstractGeneratorBindRef(pCtx, pBase) ) {
		pSourceFunc = NULL;
		sValueTypeName = "var";
		sFuncName = "generator";
	}
	if ( pSourceFunc != NULL ) {
		__xlCgenGeneratorTypeNameForFunc(sGeneratorTypeName, sizeof(sGeneratorTypeName), pSourceFunc);
		__xlCgenGeneratorNextFuncNameForFunc(sGeneratorNextFuncName, sizeof(sGeneratorNextFuncName), pSourceFunc);
	}

	snprintf(sTempGeneratorName, sizeof(sTempGeneratorName), "xl_gen_tmp_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sTempGeneratorPtrName, sizeof(sTempGeneratorPtrName), "xl_gen_ptr_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sResumeArgName, sizeof(sResumeArgName), "xl_gen_resume_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sValueName, sizeof(sValueName), "xl_gen_value_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sOkName, sizeof(sOkName), "xl_gen_ok_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sResultName, sizeof(sResultName), "xl_gen_result_%d_%d", pNode->iLine, pNode->iColumn);

	__xlCgenAppend(&pCtx->buf, "({ ");
	if ( (pSourceFunc != NULL) && __xlGeneratorBaseCanMutateInPlace(pBase) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s* %s = &(", sGeneratorTypeName, sTempGeneratorPtrName);
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "); ");
		sGeneratorPtrExpr = sTempGeneratorPtrName;
	} else if ( pSourceFunc != NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", sGeneratorTypeName, sTempGeneratorName);
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "; ");
		__xlCgenAppendFormat(&pCtx->buf, "%s* %s = &%s; ", sGeneratorTypeName, sTempGeneratorPtrName, sTempGeneratorName);
		sGeneratorPtrExpr = sTempGeneratorPtrName;
	} else {
		sGeneratorPtrExpr = NULL;
	}

	__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = ", sResumeArgName);
	if ( (sMethodName != NULL) && (strcmp(sMethodName, "resume") == 0) ) {
		__xlCgenBoxValueExpr(pCtx, pArg0);
	} else {
		__xlCgenAppend(&pCtx->buf, "NULL");
	}
	__xlCgenAppend(&pCtx->buf, "; ");

	if ( sGeneratorPtrExpr != NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = %s; ", __xlGeneratorItemCType(sValueTypeName), sValueName, __xlCDefaultReturnValue(sValueTypeName));
		__xlCgenAppendFormat(
			&pCtx->buf,
			"int %s = %s(",
			sOkName,
			sGeneratorNextFuncName
		);
		__xlCgenAppendCString(&pCtx->buf, sFuncName);
		__xlCgenAppendFormat(&pCtx->buf, ", %s, &%s, %s); ", sGeneratorPtrExpr, sValueName, sResumeArgName);
	} else {
		__xlCgenAppend(&pCtx->buf, "XL_GeneratedGeneratorValue xl_gen_abs = ");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "; ");
		__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = NULL; ", sValueName);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"int %s = xl_generated_generator_step(&xl_gen_abs, %s, &%s); ",
			sOkName,
			sResumeArgName,
			sValueName
		);
	}
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s != NULL ) xvoUnref(%s); ", sResumeArgName, sResumeArgName);
	if ( pFuncInfo != NULL ) {
		__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	}
	__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = xvoCreateArray(); ", sResultName);
	__xlCgenAppendFormat(&pCtx->buf, "(void)xvoArrayAppendValue(%s, xvoCreateBool(%s ? TRUE : FALSE), TRUE); ", sResultName, sOkName);
	__xlCgenAppendFormat(&pCtx->buf, "(void)xvoArrayAppendValue(%s, ", sResultName);
	if ( sGeneratorPtrExpr != NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s ? ", sOkName);
		__xlCgenBoxTypedCExpr(pCtx, sValueTypeName, sValueName);
		__xlCgenAppend(&pCtx->buf, " : xvoCreateNull()");
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "%s ? ((%s != NULL) ? %s : xvoCreateNull()) : xvoCreateNull()", sOkName, sValueName, sValueName);
	}
	__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	if ( sGeneratorPtrExpr == NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, "if ( (%s != NULL) && !%s ) xvoUnref(%s); ", sValueName, sOkName, sValueName);
	}
	__xlCgenAppend(&pCtx->buf, sResultName);
	__xlCgenAppend(&pCtx->buf, "; })");
}

static XL_TimeHelperKind __xlTimeHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_TIME_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__now") == 0 ) {
		return XL_TIME_HELPER_NOW;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__to_string") == 0 ) {
		return XL_TIME_HELPER_TO_STRING;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__now_text") == 0 ) {
		return XL_TIME_HELPER_NOW_TEXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__date_text") == 0 ) {
		return XL_TIME_HELPER_DATE_TEXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__time_text") == 0 ) {
		return XL_TIME_HELPER_TIME_TEXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__relative") == 0 ) {
		return XL_TIME_HELPER_RELATIVE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__parse") == 0 ) {
		return XL_TIME_HELPER_PARSE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__format") == 0 ) {
		return XL_TIME_HELPER_FORMAT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__parse_format") == 0 ) {
		return XL_TIME_HELPER_PARSE_FORMAT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__sleep") == 0 ) {
		return XL_TIME_HELPER_SLEEP;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_time__is_leap_year") == 0 ) {
		return XL_TIME_HELPER_IS_LEAP_YEAR;
	}
	return XL_TIME_HELPER_NONE;
}

static XL_PathHelperKind __xlPathHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_PATH_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__join") == 0 ) {
		return XL_PATH_HELPER_JOIN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__name_ext") == 0 ) {
		return XL_PATH_HELPER_NAME_EXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__name") == 0 ) {
		return XL_PATH_HELPER_NAME;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__ext") == 0 ) {
		return XL_PATH_HELPER_EXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__dir") == 0 ) {
		return XL_PATH_HELPER_DIR;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__exists") == 0 ) {
		return XL_PATH_HELPER_EXISTS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__is_abs") == 0 ) {
		return XL_PATH_HELPER_IS_ABS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_path__random") == 0 ) {
		return XL_PATH_HELPER_RANDOM;
	}
	return XL_PATH_HELPER_NONE;
}

static XL_FsHelperKind __xlFsHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_FS_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__exists") == 0 ) {
		return XL_FS_HELPER_EXISTS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__is_file") == 0 ) {
		return XL_FS_HELPER_IS_FILE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__is_dir") == 0 ) {
		return XL_FS_HELPER_IS_DIR;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__size") == 0 ) {
		return XL_FS_HELPER_SIZE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__set_size") == 0 ) {
		return XL_FS_HELPER_SET_SIZE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__access_time") == 0 ) {
		return XL_FS_HELPER_ACCESS_TIME;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__change_time") == 0 ) {
		return XL_FS_HELPER_CHANGE_TIME;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__read_all") == 0 ) {
		return XL_FS_HELPER_READ_ALL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__write_all") == 0 ) {
		return XL_FS_HELPER_WRITE_ALL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__append_all") == 0 ) {
		return XL_FS_HELPER_APPEND_ALL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__mkdir_all") == 0 ) {
		return XL_FS_HELPER_MKDIR_ALL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__copy") == 0 ) {
		return XL_FS_HELPER_COPY;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__move") == 0 ) {
		return XL_FS_HELPER_MOVE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fs__delete") == 0 ) {
		return XL_FS_HELPER_DELETE;
	}
	return XL_FS_HELPER_NONE;
}

static XL_StrHelperKind __xlStrHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_STR_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__contains") == 0 ) {
		return XL_STR_HELPER_CONTAINS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__empty") == 0 ) {
		return XL_STR_HELPER_EMPTY;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__byte_len") == 0 ) {
		return XL_STR_HELPER_BYTE_LEN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__index_of") == 0 ) {
		return XL_STR_HELPER_INDEX_OF;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__last_index_of") == 0 ) {
		return XL_STR_HELPER_LAST_INDEX_OF;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__slice") == 0 ) {
		return XL_STR_HELPER_SLICE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__repeat") == 0 ) {
		return XL_STR_HELPER_REPEAT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__split") == 0 ) {
		return XL_STR_HELPER_SPLIT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__split_lines") == 0 ) {
		return XL_STR_HELPER_SPLIT_LINES;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__starts_with") == 0 ) {
		return XL_STR_HELPER_STARTS_WITH;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__ends_with") == 0 ) {
		return XL_STR_HELPER_ENDS_WITH;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__trim") == 0 ) {
		return XL_STR_HELPER_TRIM;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__trim_left") == 0 ) {
		return XL_STR_HELPER_TRIM_LEFT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__trim_right") == 0 ) {
		return XL_STR_HELPER_TRIM_RIGHT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__lower") == 0 ) {
		return XL_STR_HELPER_LOWER;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__upper") == 0 ) {
		return XL_STR_HELPER_UPPER;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__replace") == 0 ) {
		return XL_STR_HELPER_REPLACE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__reverse") == 0 ) {
		return XL_STR_HELPER_REVERSE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__pad_left") == 0 ) {
		return XL_STR_HELPER_PAD_LEFT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__pad_right") == 0 ) {
		return XL_STR_HELPER_PAD_RIGHT;
	}
	return XL_STR_HELPER_NONE;
}

static XL_RandHelperKind __xlRandHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_RAND_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_rand__int32") == 0 ) {
		return XL_RAND_HELPER_INT32;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_rand__bool") == 0 ) {
		return XL_RAND_HELPER_BOOL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_rand__range") == 0 ) {
		return XL_RAND_HELPER_RANGE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_rand__text") == 0 ) {
		return XL_RAND_HELPER_TEXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_rand__text_from") == 0 ) {
		return XL_RAND_HELPER_TEXT_FROM;
	}
	return XL_RAND_HELPER_NONE;
}

static XL_HostHelperKind __xlHostHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_HOST_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_host__name") == 0 ) {
		return XL_HOST_HELPER_NAME;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_host__ip") == 0 ) {
		return XL_HOST_HELPER_IP;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_host__user") == 0 ) {
		return XL_HOST_HELPER_USER;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_host__os") == 0 ) {
		return XL_HOST_HELPER_OS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_host__arch") == 0 ) {
		return XL_HOST_HELPER_ARCH;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_host__executable") == 0 ) {
		return XL_HOST_HELPER_EXECUTABLE;
	}
	return XL_HOST_HELPER_NONE;
}

static XL_FmtHelperKind __xlFmtHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_FMT_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fmt__int_text") == 0 ) {
		return XL_FMT_HELPER_INT_TEXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fmt__float_text") == 0 ) {
		return XL_FMT_HELPER_FLOAT_TEXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_fmt__bool_text") == 0 ) {
		return XL_FMT_HELPER_BOOL_TEXT;
	}
	return XL_FMT_HELPER_NONE;
}

static XL_DebugHelperKind __xlDebugHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_DEBUG_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_debug__text") == 0 ) {
		return XL_DEBUG_HELPER_TEXT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_debug__print") == 0 ) {
		return XL_DEBUG_HELPER_PRINT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_debug__println") == 0 ) {
		return XL_DEBUG_HELPER_PRINTLN;
	}
	return XL_DEBUG_HELPER_NONE;
}

static XL_LogHelperKind __xlLogHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_LOG_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_log__debug") == 0 ) {
		return XL_LOG_HELPER_DEBUG;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_log__info") == 0 ) {
		return XL_LOG_HELPER_INFO;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_log__warn") == 0 ) {
		return XL_LOG_HELPER_WARN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_log__error") == 0 ) {
		return XL_LOG_HELPER_ERROR;
	}
	return XL_LOG_HELPER_NONE;
}

static XL_TraceHelperKind __xlTraceHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_TRACE_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_trace__write") == 0 ) {
		return XL_TRACE_HELPER_WRITE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_trace__error") == 0 ) {
		return XL_TRACE_HELPER_ERROR;
	}
	return XL_TRACE_HELPER_NONE;
}

static XL_RegexHelperKind __xlRegexHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_REGEX_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_regex__is_match") == 0 ) {
		return XL_REGEX_HELPER_IS_MATCH;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_regex__find") == 0 ) {
		return XL_REGEX_HELPER_FIND;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_regex__captures") == 0 ) {
		return XL_REGEX_HELPER_CAPTURES;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_regex__capture_count") == 0 ) {
		return XL_REGEX_HELPER_CAPTURE_COUNT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_regex__replace_first") == 0 ) {
		return XL_REGEX_HELPER_REPLACE_FIRST;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_regex__replace_all") == 0 ) {
		return XL_REGEX_HELPER_REPLACE_ALL;
	}
	return XL_REGEX_HELPER_NONE;
}

static XL_CryptoHelperKind __xlCryptoHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_CRYPTO_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_crypto__hmac_sha256") == 0 ) {
		return XL_CRYPTO_HELPER_HMAC_SHA256;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_crypto__hmac_sha512") == 0 ) {
		return XL_CRYPTO_HELPER_HMAC_SHA512;
	}
	return XL_CRYPTO_HELPER_NONE;
}

static XL_EncodeHelperKind __xlEncodeHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_ENCODE_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_encode__hex_encode") == 0 ) {
		return XL_ENCODE_HELPER_HEX_ENCODE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_encode__hex_decode") == 0 ) {
		return XL_ENCODE_HELPER_HEX_DECODE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_encode__base64_encode") == 0 ) {
		return XL_ENCODE_HELPER_BASE64_ENCODE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_encode__base64_decode") == 0 ) {
		return XL_ENCODE_HELPER_BASE64_DECODE;
	}
	return XL_ENCODE_HELPER_NONE;
}

static XL_HashHelperKind __xlHashHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_HASH_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_hash__hash32") == 0 ) {
		return XL_HASH_HELPER_HASH32;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_hash__hash64") == 0 ) {
		return XL_HASH_HELPER_HASH64;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_hash__hash64_micro") == 0 ) {
		return XL_HASH_HELPER_HASH64_MICRO;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_hash__hash64_nano") == 0 ) {
		return XL_HASH_HELPER_HASH64_NANO;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_hash__sha256") == 0 ) {
		return XL_HASH_HELPER_SHA256;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_hash__sha512") == 0 ) {
		return XL_HASH_HELPER_SHA512;
	}
	return XL_HASH_HELPER_NONE;
}

static XL_MathHelperKind __xlMathHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_MATH_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__abs") == 0 ) {
		return XL_MATH_HELPER_ABS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__min") == 0 ) {
		return XL_MATH_HELPER_MIN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__max") == 0 ) {
		return XL_MATH_HELPER_MAX;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__clamp") == 0 ) {
		return XL_MATH_HELPER_CLAMP;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__floor") == 0 ) {
		return XL_MATH_HELPER_FLOOR;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__ceil") == 0 ) {
		return XL_MATH_HELPER_CEIL;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__round") == 0 ) {
		return XL_MATH_HELPER_ROUND;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__trunc") == 0 ) {
		return XL_MATH_HELPER_TRUNC;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__sqrt") == 0 ) {
		return XL_MATH_HELPER_SQRT;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__pow") == 0 ) {
		return XL_MATH_HELPER_POW;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__sin") == 0 ) {
		return XL_MATH_HELPER_SIN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__cos") == 0 ) {
		return XL_MATH_HELPER_COS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__tan") == 0 ) {
		return XL_MATH_HELPER_TAN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__exp") == 0 ) {
		return XL_MATH_HELPER_EXP;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__log") == 0 ) {
		return XL_MATH_HELPER_LOG;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_math__log10") == 0 ) {
		return XL_MATH_HELPER_LOG10;
	}
	return XL_MATH_HELPER_NONE;
}

static XL_PackHelperKind __xlPackHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_PACK_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__write_u8") == 0 ) {
		return XL_PACK_HELPER_WRITE_U8;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__write_u16_le") == 0 ) {
		return XL_PACK_HELPER_WRITE_U16_LE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__write_u16_be") == 0 ) {
		return XL_PACK_HELPER_WRITE_U16_BE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__write_u32_le") == 0 ) {
		return XL_PACK_HELPER_WRITE_U32_LE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__write_u32_be") == 0 ) {
		return XL_PACK_HELPER_WRITE_U32_BE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__read_u8") == 0 ) {
		return XL_PACK_HELPER_READ_U8;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__read_u16_le") == 0 ) {
		return XL_PACK_HELPER_READ_U16_LE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__read_u16_be") == 0 ) {
		return XL_PACK_HELPER_READ_U16_BE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__read_u32_le") == 0 ) {
		return XL_PACK_HELPER_READ_U32_LE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_pack__read_u32_be") == 0 ) {
		return XL_PACK_HELPER_READ_U32_BE;
	}
	return XL_PACK_HELPER_NONE;
}

static XL_JsonHelperKind __xlJsonHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_JSON_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_json__encode") == 0 ) {
		return XL_JSON_HELPER_ENCODE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_json__decode") == 0 ) {
		return XL_JSON_HELPER_DECODE;
	}
	return XL_JSON_HELPER_NONE;
}

static XL_EnvHelperKind __xlEnvHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_ENV_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_env__get") == 0 ) {
		return XL_ENV_HELPER_GET;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_env__exists") == 0 ) {
		return XL_ENV_HELPER_EXISTS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_env__set") == 0 ) {
		return XL_ENV_HELPER_SET;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_env__cwd") == 0 ) {
		return XL_ENV_HELPER_CWD;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_env__temp") == 0 ) {
		return XL_ENV_HELPER_TEMP;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_env__home") == 0 ) {
		return XL_ENV_HELPER_HOME;
	}
	return XL_ENV_HELPER_NONE;
}

static XL_DirHelperKind __xlDirHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_DIR_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__list") == 0 ) {
		return XL_DIR_HELPER_LIST;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__create") == 0 ) {
		return XL_DIR_HELPER_CREATE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__files") == 0 ) {
		return XL_DIR_HELPER_FILES;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__dirs") == 0 ) {
		return XL_DIR_HELPER_DIRS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__tree") == 0 ) {
		return XL_DIR_HELPER_TREE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__copy") == 0 ) {
		return XL_DIR_HELPER_COPY;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__move") == 0 ) {
		return XL_DIR_HELPER_MOVE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__delete") == 0 ) {
		return XL_DIR_HELPER_DELETE;
	}
	return XL_DIR_HELPER_NONE;
}

static XL_ProcessHelperKind __xlProcessHelperKind(const char* sCalleeName)
{
	if ( sCalleeName == NULL ) {
		return XL_PROCESS_HELPER_NONE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_process__exec") == 0 ) {
		return XL_PROCESS_HELPER_EXEC;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_process__capture") == 0 ) {
		return XL_PROCESS_HELPER_CAPTURE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_process__exec_in") == 0 ) {
		return XL_PROCESS_HELPER_EXEC_IN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_process__capture_in") == 0 ) {
		return XL_PROCESS_HELPER_CAPTURE_IN;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_process__terminal_supported") == 0 ) {
		return XL_PROCESS_HELPER_TERMINAL_SUPPORTED;
	}
	return XL_PROCESS_HELPER_NONE;
}

static const char* __xlInferExprTypeName(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pFunc;
	const char* sCalleeName;
	XL_TaskHelperKind eTaskKind;

	if ( pNode == NULL ) {
		return NULL;
	}
	if ( (pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') ) {
		return pNode->sTypeName;
	}
	if ( pNode->eKind == XL_MIR_CALL ) {
		sCalleeName = __xlCallCalleeName(pNode);
		pFunc = __xlResolveCallFuncNode(pCtx, pNode);
		if ( (pFunc != NULL) && (pFunc->bFlag != 0) ) {
			return ((pFunc->sTypeName != NULL) && (pFunc->sTypeName[0] != '\0')) ? pFunc->sTypeName : "future<void>";
		}
		if ( __xlCorePrintHelperKind(sCalleeName) != XL_CORE_PRINT_NONE ) {
			return "void";
		}
		if ( __xlIoHelperKind(sCalleeName) == XL_IO_HELPER_READ_LINE ) {
			return "string";
		}
		if ( __xlIoHelperKind(sCalleeName) != XL_IO_HELPER_NONE ) {
			return "void";
		}
		if ( __xlCoreLenHelperKind(sCalleeName) != XL_CORE_LEN_NONE ) {
			return "int";
		}
		eTaskKind = __xlTaskHelperKind(sCalleeName);
		if ( eTaskKind == XL_TASK_HELPER_CANCEL ) {
			return "int";
		}
		pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
		if ( (eTaskKind != XL_TASK_HELPER_NONE) && (pArgs != NULL) && (pArgs->iChildCount != 0) ) {
			return __xlInferExprTypeName(pCtx, pArgs->ppChildren[0]);
		}
		return NULL;
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) ) {
		const char* sLeftType;
		const char* sRightType;

		sLeftType = __xlInferExprTypeName(pCtx, pNode->ppChildren[0]);
		sRightType = __xlInferExprTypeName(pCtx, pNode->ppChildren[1]);
		if ( strcmp(pNode->sText, "??") == 0 ) {
			if ( (sLeftType != NULL) && (strcmp(sLeftType, "null") == 0) ) {
				return sRightType;
			}
			return sLeftType;
		}
		if ( (strcmp(pNode->sText, "==") == 0) || (strcmp(pNode->sText, "!=") == 0) ||
			(strcmp(pNode->sText, ">") == 0) || (strcmp(pNode->sText, ">=") == 0) ||
			(strcmp(pNode->sText, "<") == 0) || (strcmp(pNode->sText, "<=") == 0) ||
			(strcmp(pNode->sText, "~=") == 0) ) {
			return "bool";
		}
		if ( ((sLeftType != NULL) && (strcmp(sLeftType, "float") == 0)) ||
			((sRightType != NULL) && (strcmp(sRightType, "float") == 0)) ) {
			return "float";
		}
		if ( ((sLeftType != NULL) && (strcmp(sLeftType, "string") == 0)) ||
			((sRightType != NULL) && (strcmp(sRightType, "string") == 0)) ) {
			return "string";
		}
		if ( ((sLeftType != NULL) && (strcmp(sLeftType, "bool") == 0)) ||
			((sRightType != NULL) && (strcmp(sRightType, "bool") == 0)) ) {
			return "bool";
		}
		return "int";
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 3) && (pNode->sText != NULL) && (strcmp(pNode->sText, "?:") == 0) ) {
		return __xlInferExprTypeName(pCtx, pNode->ppChildren[1]);
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 1) && (pNode->sText != NULL) &&
		(strcmp(pNode->sText, "await") == 0) ) {
		static char sFutureValueType[128];
		const char* sInnerTypeName;

		sInnerTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[0]);
		if ( __xlExtractFutureValueType(sInnerTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
			return sFutureValueType;
		}
		return sInnerTypeName;
	}
	return NULL;
}

static const char* __xlStorageTypeName(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pDecl;

	if ( pNode == NULL ) {
		return NULL;
	}
	if ( (pNode->bFlag & XL_MIR_FLAG_STORAGE_VAR) != 0 ) {
		return "var";
	}
	if ( (pCtx != NULL) && (pCtx->pRoot != NULL) && (pNode->iSymbolIndex >= 0) ) {
		pDecl = __xlFindMirSymbolDeclNode(pCtx->pRoot, pNode->iSymbolIndex);
		if ( (pDecl != NULL) && (pDecl->sTypeName != NULL) && (strcmp(pDecl->sTypeName, "var") == 0) ) {
			return "var";
		}
	}
	return pNode->sTypeName;
}

static int __xlIsContainerIndexExpr(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pBase;
	const char* sBaseType;

	if ( (pCtx == NULL) || (pNode == NULL) ) {
		return 0;
	}
	if ( (pNode->eKind != XL_MIR_VALUE) || (pNode->iChildCount != 2) ) {
		return 0;
	}
	if ( (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		return 0;
	}

	pBase = pNode->ppChildren[0];
	sBaseType = __xlInferExprTypeName(pCtx, pBase);
	return __xlTypeHasBaseName(sBaseType, "array") ||
		__xlTypeHasBaseName(sBaseType, "list") ||
		__xlTypeHasBaseName(sBaseType, "dict");
}

static const char* __xlTaskTypeSuffix(const char* sTypeName)
{
	sTypeName = __xlFutureValueTypeName(sTypeName);
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "int";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "float";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "string";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "error";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "bool";
	}
	return "int";
}

static const char* __xlCorePrintTypeSuffix(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "string";
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		return "int";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "float";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "bool";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "string";
	}
	return "string";
}

static const char* __xlCoreLenTypeSuffix(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "string";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "string";
	}
	if ( __xlTypeHasBaseName(sTypeName, "array") ) {
		return "array";
	}
	if ( __xlTypeHasBaseName(sTypeName, "list") ) {
		return "list";
	}
	if ( __xlTypeHasBaseName(sTypeName, "set") ) {
		return "set";
	}
	if ( __xlTypeHasBaseName(sTypeName, "dict") ) {
		return "dict";
	}
	return "string";
}

static int __xlIsLengthPropertyName(const char* sMemberName)
{
	return (sMemberName != NULL) && (strcmp(sMemberName, "length") == 0);
}

static int __xlIsTextLikeType(const char* sTypeName)
{
	if ( sTypeName == NULL ) {
		return 0;
	}
	return (strcmp(sTypeName, "string") == 0) || (strcmp(sTypeName, "type") == 0) || (strcmp(sTypeName, "error") == 0);
}

static int __xlIsTimeTypeName(const char* sTypeName)
{
	return (sTypeName != NULL) && (strcmp(sTypeName, "time") == 0);
}

static int __xlIsArrayLiteralCall(const XL_MirNode* pNode)
{
	return (pNode != NULL) &&
		(pNode->eKind == XL_MIR_CALL) &&
		(__xlCallCalleeName(pNode) != NULL) &&
		(strcmp(__xlCallCalleeName(pNode), "__array_literal") == 0);
}

static int __xlIsHiddenCallNamed(const XL_MirNode* pNode, const char* sName)
{
	return (pNode != NULL) &&
		(sName != NULL) &&
		(pNode->eKind == XL_MIR_CALL) &&
		(__xlCallCalleeName(pNode) != NULL) &&
		(strcmp(__xlCallCalleeName(pNode), sName) == 0);
}

static int __xlIsListCtorCall(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	if ( __xlIsHiddenCallNamed(pNode, "__list_literal") ) {
		return 1;
	}
	return (pCtx != NULL) &&
		(pNode != NULL) &&
		(pNode->eKind == XL_MIR_CALL) &&
		(__xlCallCalleeName(pNode) != NULL) &&
		(strcmp(__xlCallCalleeName(pNode), "list") == 0) &&
		(__xlResolveCallFuncNode(pCtx, pNode) == NULL);
}

static int __xlIsIndexedListLiteralCall(const XL_MirNode* pNode)
{
	return __xlIsHiddenCallNamed(pNode, "__list_literal_indexed");
}

static int __xlIsSetCtorCall(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	if ( __xlIsHiddenCallNamed(pNode, "__set_literal") ) {
		return 1;
	}
	return (pCtx != NULL) &&
		(pNode != NULL) &&
		(pNode->eKind == XL_MIR_CALL) &&
		(__xlCallCalleeName(pNode) != NULL) &&
		(strcmp(__xlCallCalleeName(pNode), "set") == 0) &&
		(__xlResolveCallFuncNode(pCtx, pNode) == NULL);
}

static int __xlIsDictCtorCall(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	if ( __xlIsHiddenCallNamed(pNode, "__dict_literal") ) {
		return 1;
	}
	return (pCtx != NULL) &&
		(pNode != NULL) &&
		(pNode->eKind == XL_MIR_CALL) &&
		(__xlCallCalleeName(pNode) != NULL) &&
		(strcmp(__xlCallCalleeName(pNode), "dict") == 0) &&
		(__xlResolveCallFuncNode(pCtx, pNode) == NULL);
}

static int __xlIsTypeCtorCall(const XL_MirNode* pNode)
{
	const XL_MirNode* pCallee;
	const XL_MirNode* pArgs;

	if ( (pNode == NULL) || (pNode->eKind != XL_MIR_CALL) || (pNode->iFuncIndex >= 0) || !__xlIsTypeCtorTargetName(pNode->sTypeName) ) {
		return 0;
	}

	pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	return (pCallee != NULL) &&
		(pCallee->iChildCount == 0) &&
		(pCallee->sText != NULL) &&
		(pCallee->sText[0] != '\0') &&
		(pArgs != NULL) &&
		(pArgs->iChildCount == 1);
}

static void __xlCgenBoxValueExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const char* sTypeName;
	const char* sStorageTypeName;

	sTypeName = __xlInferExprTypeName(pCtx, pNode);
	sStorageTypeName = __xlStorageTypeName(pCtx, pNode);
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || (strcmp(sTypeName, "null") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreateNull()");
		return;
	}
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreateInt((int64)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreateFloat((double)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreateBool((");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ") ? TRUE : FALSE)");
		return;
	}
	if ( (strcmp(sTypeName, "string") == 0) || (strcmp(sTypeName, "type") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_text_value = ");
		__xlCgenExprAsType(pCtx, pNode, sTypeName);
		__xlCgenAppend(&pCtx->buf, "; xvoCreateText((ptr)xl_text_value, (uint32)strlen((xl_text_value != NULL) ? xl_text_value : \"\"), FALSE); })");
		return;
	}
	if ( __xlIsPointTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreatePoint((ptr)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	if ( __xlIsFunctionTypeName(sTypeName) ) {
		if ( __xlIsVarTypeName(sStorageTypeName) ) {
			__xlCgenAppend(&pCtx->buf, "xvoCopy((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, "))");
			return;
		}
		__xlCgenAppend(&pCtx->buf, "xvoCreateFunc((xfunction)(");
		__xlCgenExprAsType(pCtx, pNode, sTypeName);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	if ( strcmp(sTypeName, "time") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreateTime(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( __xlIsValueBackedTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCopy((xvalue)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "xvoCreateNull()");
}

static void __xlCgenValueCompareOperand(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	if ( __xlIsVarTypeName(__xlInferExprTypeName(pCtx, pNode)) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCopy((xvalue)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	__xlCgenBoxValueExpr(pCtx, pNode);
}

static void __xlCgenVarCompare(XL_CgenContext* pCtx, const XL_MirNode* pLeft, const XL_MirNode* pRight, const char* sOp)
{
	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_cmp_left = ");
	__xlCgenValueCompareOperand(pCtx, pLeft);
	__xlCgenAppend(&pCtx->buf, "; xvalue xl_cmp_right = ");
	__xlCgenValueCompareOperand(pCtx, pRight);
	__xlCgenAppend(&pCtx->buf, "; int xl_cmp_ok = xvoCanCompareBasic(xl_cmp_left, xl_cmp_right) ? 1 : 0; int xl_cmp_result = xl_cmp_ok ? xvoBasicCompare(xl_cmp_left, xl_cmp_right) : 2; int xl_cmp_value = ");
	if ( strcmp(sOp, "==") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "(xl_cmp_ok && (xl_cmp_result == 0))");
	} else if ( strcmp(sOp, "!=") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "((!xl_cmp_ok) || (xl_cmp_result != 0))");
	} else if ( strcmp(sOp, ">") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "(xl_cmp_ok && (xl_cmp_result > 0))");
	} else if ( strcmp(sOp, ">=") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "(xl_cmp_ok && (xl_cmp_result >= 0))");
	} else if ( strcmp(sOp, "<") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "(xl_cmp_ok && (xl_cmp_result < 0))");
	} else if ( strcmp(sOp, "<=") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "(xl_cmp_ok && (xl_cmp_result <= 0))");
	} else {
		__xlCgenAppend(&pCtx->buf, "(xl_cmp_ok && (xl_cmp_result == 0))");
	}
	__xlCgenAppend(&pCtx->buf, "; xvoUnref(xl_cmp_left); xvoUnref(xl_cmp_right); xl_cmp_value; })");
}

static void __xlCgenArrayLiteral(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	size_t i;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_arr_literal = xvoCreateArray(); ");
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		__xlCgenAppend(&pCtx->buf, "(void)xvoArrayAppendValue(xl_arr_literal, ");
		__xlCgenBoxValueExpr(pCtx, pArgs->ppChildren[i]);
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	}
	__xlCgenAppend(&pCtx->buf, "xl_arr_literal; })");
}

static void __xlCgenListCtor(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	size_t i;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_list_ctor = xvoCreateList(); ");
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		__xlCgenAppendFormat(&pCtx->buf, "(void)xvoListSetValue(xl_list_ctor, (int64)(%llu), ", (unsigned long long)i);
		__xlCgenBoxValueExpr(pCtx, pArgs->ppChildren[i]);
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	}
	__xlCgenAppend(&pCtx->buf, "xl_list_ctor; })");
}

static void __xlCgenIndexedListCtor(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	size_t i;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_list_ctor = xvoCreateList(); ");
	for ( i = 0; (pArgs != NULL) && ((i + 1) < pArgs->iChildCount); i += 2 ) {
		__xlCgenAppend(&pCtx->buf, "(void)xvoListSetValue(xl_list_ctor, (int64)(");
		__xlCgenExprAsType(pCtx, pArgs->ppChildren[i], "int");
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenBoxValueExpr(pCtx, pArgs->ppChildren[i + 1]);
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	}
	__xlCgenAppend(&pCtx->buf, "xl_list_ctor; })");
}

static void __xlCgenSetCtor(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	size_t i;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_set_ctor = xvoCreateColl(); ");
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		__xlCgenAppend(&pCtx->buf, "(void)xvoCollSetValue(xl_set_ctor, ");
		__xlCgenBoxValueExpr(pCtx, pArgs->ppChildren[i]);
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	}
	__xlCgenAppend(&pCtx->buf, "xl_set_ctor; })");
}

static void __xlCgenDictCtor(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pKeyNode;
	size_t i;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_dict_ctor = xvoCreateTable(); ");
	for ( i = 0; (pArgs != NULL) && ((i + 1) < pArgs->iChildCount); i += 2 ) {
		pKeyNode = pArgs->ppChildren[i];
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pKeyNode, "string");
		__xlCgenAppend(&pCtx->buf, "; (void)xvoTableSetValue(xl_dict_ctor, (const void*)xl_dict_key, (uint32)strlen((xl_dict_key != NULL) ? xl_dict_key : \"\"), ");
		__xlCgenBoxValueExpr(pCtx, pArgs->ppChildren[i + 1]);
		__xlCgenAppend(&pCtx->buf, ", TRUE); }); ");
	}
	__xlCgenAppend(&pCtx->buf, "xl_dict_ctor; })");
}

static void __xlCgenSetContains(XL_CgenContext* pCtx, const XL_MirNode* pLeft, const XL_MirNode* pRight)
{
	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_set_item = ");
	__xlCgenBoxValueExpr(pCtx, pLeft);
	__xlCgenAppend(&pCtx->buf, "; int xl_set_has = xvoCollExists((xvalue)(");
	__xlCgenExpr(pCtx, pRight);
	__xlCgenAppend(&pCtx->buf, "), xl_set_item) ? 1 : 0; xvoUnref(xl_set_item); xl_set_has; })");
}

static void __xlCgenDictIndex(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pIndex;
	const char* sValueType;

	pBase = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pIndex = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	sValueType = __xlInferExprTypeName(pCtx, pNode);

	if ( (sValueType != NULL) && (strcmp(sValueType, "int") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pIndex, "string");
		__xlCgenAppend(&pCtx->buf, "; xvoTableGetInt((xvalue)(");
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "float") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pIndex, "string");
		__xlCgenAppend(&pCtx->buf, "; xvoTableGetFloat((xvalue)(");
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "bool") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pIndex, "string");
		__xlCgenAppend(&pCtx->buf, "; xvoTableGetBool((xvalue)(");
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "string") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pIndex, "string");
		__xlCgenAppend(&pCtx->buf, "; xvoTableGetText((xvalue)(");
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "time") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pIndex, "string");
		__xlCgenAppend(&pCtx->buf, "; xvoTableGetTime((xvalue)(");
	} else {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pIndex, "string");
		__xlCgenAppend(&pCtx->buf, "; xvoTableGetValue((xvalue)(");
	}

	__xlCgenExpr(pCtx, pBase);
	__xlCgenAppend(&pCtx->buf, "), (const void*)xl_dict_key, (uint32)strlen((xl_dict_key != NULL) ? xl_dict_key : \"\")); })");
}

static void __xlCgenIndexAssign(XL_CgenContext* pCtx, const XL_MirNode* pLeft, const XL_MirNode* pRight)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pIndex;
	const char* sBaseTypeName;
	const char* sValueTypeName;
	const char* sSetFuncName;
	const char* sIndexTypeName;
	char sBaseName[64];
	char sIndexName[64];
	char sValueName[64];
	char sKeyName[64];

	pBase = (pLeft != NULL) && (pLeft->iChildCount != 0) ? pLeft->ppChildren[0] : NULL;
	pIndex = (pLeft != NULL) && (pLeft->iChildCount > 1) ? pLeft->ppChildren[1] : NULL;
	sBaseTypeName = __xlInferExprTypeName(pCtx, pBase);
	sValueTypeName = __xlInferExprTypeName(pCtx, pLeft);

	snprintf(sBaseName, sizeof(sBaseName), "xl_assign_base_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);
	snprintf(sIndexName, sizeof(sIndexName), "xl_assign_index_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);
	snprintf(sValueName, sizeof(sValueName), "xl_assign_value_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);
	snprintf(sKeyName, sizeof(sKeyName), "xl_assign_key_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);

	if ( __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "({ xvalue ");
		__xlCgenAppend(&pCtx->buf, sBaseName);
		__xlCgenAppend(&pCtx->buf, " = ");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "; const char* ");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, " = ");
		__xlCgenExprAsType(pCtx, pIndex, "string");
		__xlCgenAppend(&pCtx->buf, "; xvalue ");
		__xlCgenAppend(&pCtx->buf, sValueName);
		__xlCgenAppend(&pCtx->buf, " = ");
		__xlCgenBoxValueExpr(pCtx, pRight);
		__xlCgenAppend(&pCtx->buf, "; (void)xvoTableSetValue((xvalue)(");
		__xlCgenAppend(&pCtx->buf, sBaseName);
		__xlCgenAppend(&pCtx->buf, "), (const void*)");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, ", (uint32)strlen((");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, " != NULL) ? ");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, " : \"\"), ");
		__xlCgenAppend(&pCtx->buf, sValueName);
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
		__xlCgenValueExprAsType(pCtx, sValueName, sValueTypeName);
		__xlCgenAppend(&pCtx->buf, "; })");
		return;
	}

	sSetFuncName = __xlTypeHasBaseName(sBaseTypeName, "list") ? "xvoListSetValue" : "xvoArraySetValue";
	sIndexTypeName = __xlTypeHasBaseName(sBaseTypeName, "list") ? "int64" : "uint32";
	__xlCgenAppend(&pCtx->buf, "({ xvalue ");
	__xlCgenAppend(&pCtx->buf, sBaseName);
	__xlCgenAppend(&pCtx->buf, " = ");
	__xlCgenExpr(pCtx, pBase);
	__xlCgenAppend(&pCtx->buf, "; ");
	__xlCgenAppend(&pCtx->buf, sIndexTypeName);
	__xlCgenAppend(&pCtx->buf, " ");
	__xlCgenAppend(&pCtx->buf, sIndexName);
	__xlCgenAppend(&pCtx->buf, " = (");
	__xlCgenAppend(&pCtx->buf, sIndexTypeName);
	__xlCgenAppend(&pCtx->buf, ")(");
	__xlCgenExpr(pCtx, pIndex);
	__xlCgenAppend(&pCtx->buf, "); xvalue ");
	__xlCgenAppend(&pCtx->buf, sValueName);
	__xlCgenAppend(&pCtx->buf, " = ");
	__xlCgenBoxValueExpr(pCtx, pRight);
	__xlCgenAppend(&pCtx->buf, "; (void)");
	__xlCgenAppend(&pCtx->buf, sSetFuncName);
	__xlCgenAppend(&pCtx->buf, "((xvalue)(");
	__xlCgenAppend(&pCtx->buf, sBaseName);
	__xlCgenAppend(&pCtx->buf, "), ");
	__xlCgenAppend(&pCtx->buf, sIndexName);
	__xlCgenAppend(&pCtx->buf, ", ");
	__xlCgenAppend(&pCtx->buf, sValueName);
	__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	__xlCgenValueExprAsType(pCtx, sValueName, sValueTypeName);
	__xlCgenAppend(&pCtx->buf, "; })");
}

static void __xlCgenIndexExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode, const char* sBaseTypeName)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pIndex;
	const char* sGetterPrefix;
	const char* sIndexCastType;
	const char* sValueType;

	pBase = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pIndex = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	sValueType = __xlInferExprTypeName(pCtx, pNode);
	sGetterPrefix = __xlTypeHasBaseName(sBaseTypeName, "list") ? "xvoListGet" : "xvoArrayGet";
	sIndexCastType = __xlTypeHasBaseName(sBaseTypeName, "list") ? "int64" : "uint32";

	if ( (sValueType != NULL) && (strcmp(sValueType, "int") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%sInt((xvalue)(", sGetterPrefix);
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "float") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%sFloat((xvalue)(", sGetterPrefix);
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "bool") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%sBool((xvalue)(", sGetterPrefix);
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "string") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%sText((xvalue)(", sGetterPrefix);
	} else if ( (sValueType != NULL) && (strcmp(sValueType, "time") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%sTime((xvalue)(", sGetterPrefix);
	} else if ( __xlIsValueBackedTypeName(sValueType) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%sValue((xvalue)(", sGetterPrefix);
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "%sValue((xvalue)(", sGetterPrefix);
	}

	__xlCgenExpr(pCtx, pBase);
	__xlCgenAppendFormat(&pCtx->buf, "), (%s)(", sIndexCastType);
	__xlCgenExpr(pCtx, pIndex);
	__xlCgenAppend(&pCtx->buf, "))");
}

static const char* __xlCLogicalBinaryOp(const char* sOp)
{
	if ( sOp == NULL ) {
		return NULL;
	}
	if ( strcmp(sOp, "and") == 0 ) {
		return "&&";
	}
	if ( strcmp(sOp, "or") == 0 ) {
		return "||";
	}
	return sOp;
}

static const char* __xlCUnaryOp(const char* sOp)
{
	if ( sOp == NULL ) {
		return NULL;
	}
	if ( strcmp(sOp, "not") == 0 ) {
		return "!";
	}
	return sOp;
}

static void __xlCgenPowerCall(XL_CgenContext* pCtx, const XL_MirNode* pBase, const XL_MirNode* pExponent)
{
	if ( __xlIsFloatTypeName(__xlInferExprTypeName(pCtx, pBase)) ||
		__xlIsFloatTypeName(__xlInferExprTypeName(pCtx, pExponent)) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_power_float(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pExponent);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "xl_generated_power_int(");
	__xlCgenExpr(pCtx, pBase);
	__xlCgenAppend(&pCtx->buf, ", ");
	__xlCgenExpr(pCtx, pExponent);
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenPowerAssign(XL_CgenContext* pCtx, const XL_MirNode* pLeft, const XL_MirNode* pRight)
{
	if ( __xlIsFloatTypeName(__xlInferExprTypeName(pCtx, pLeft)) ||
		__xlIsFloatTypeName(__xlInferExprTypeName(pCtx, pRight)) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_power_assign_float(&(");
		__xlCgenExpr(pCtx, pLeft);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenExpr(pCtx, pRight);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "xl_generated_power_assign_int(&(");
	__xlCgenExpr(pCtx, pLeft);
	__xlCgenAppend(&pCtx->buf, "), ");
	__xlCgenExpr(pCtx, pRight);
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenTextExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const char* sTypeName;

	sTypeName = __xlInferExprTypeName(pCtx, pNode);
	if ( __xlIsVarTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_value_to_string((xvalue)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	if ( __xlIsPointTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_point_to_string(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "error") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_text(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	__xlCgenExpr(pCtx, pNode);
}

static void __xlCgenApproxCompare(XL_CgenContext* pCtx, const XL_MirNode* pLeft, const XL_MirNode* pRight)
{
	const char* sLeftTypeName;
	const char* sRightTypeName;

	sLeftTypeName = __xlInferExprTypeName(pCtx, pLeft);
	sRightTypeName = __xlInferExprTypeName(pCtx, pRight);
	if ( __xlIsTextLikeType(sLeftTypeName) || __xlIsTextLikeType(sRightTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_text_approx(");
		__xlCgenTextExpr(pCtx, pLeft);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenTextExpr(pCtx, pRight);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( __xlIsTimeTypeName(sLeftTypeName) && __xlIsTimeTypeName(sRightTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_time_approx(");
		__xlCgenExpr(pCtx, pLeft);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pRight);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( __xlIsNumericTypeName(sLeftTypeName) && __xlIsNumericTypeName(sRightTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_number_approx(");
		__xlCgenExpr(pCtx, pLeft);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pRight);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "0");
}

static void __xlCgenNullCoalesce(XL_CgenContext* pCtx, const XL_MirNode* pLeft, const XL_MirNode* pRight)
{
	__xlCgenAppend(&pCtx->buf, "(");
	__xlCgenExpr(pCtx, pLeft);
	__xlCgenAppend(&pCtx->buf, " != NULL ? ");
	__xlCgenExpr(pCtx, pLeft);
	__xlCgenAppend(&pCtx->buf, " : ");
	__xlCgenExpr(pCtx, pRight);
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenNullAssign(XL_CgenContext* pCtx, const XL_MirNode* pLeft, const XL_MirNode* pRight)
{
	const char* sLeftTypeName;

	sLeftTypeName = __xlInferExprTypeName(pCtx, pLeft);
	if ( (sLeftTypeName != NULL) && (strcmp(sLeftTypeName, "error") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_null_assign_error(&(");
		__xlCgenExpr(pCtx, pLeft);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenExpr(pCtx, pRight);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "xl_generated_null_assign_text(&(");
	__xlCgenExpr(pCtx, pLeft);
	__xlCgenAppend(&pCtx->buf, "), ");
	__xlCgenExpr(pCtx, pRight);
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenExprAsType(XL_CgenContext* pCtx, const XL_MirNode* pNode, const char* sExpectedTypeName)
{
	const char* sActualTypeName;
	const char* sStorageTypeName;

	sActualTypeName = __xlInferExprTypeName(pCtx, pNode);
	sStorageTypeName = __xlStorageTypeName(pCtx, pNode);
	if ( __xlIsVarTypeName(sExpectedTypeName) ) {
		__xlCgenBoxValueExpr(pCtx, pNode);
		return;
	}
	if ( __xlIsFunctionTypeName(sExpectedTypeName) &&
		(pNode != NULL) &&
		(pNode->iChildCount == 0) &&
		__xlIsFunctionTypeName(sActualTypeName) &&
		!__xlIsVarTypeName(sStorageTypeName) ) {
		__xlCgenFunctionValueThunkRef(&pCtx->buf, pCtx, pNode);
		return;
	}
	if ( __xlIsFunctionTypeName(sExpectedTypeName) &&
		__xlIsFunctionTypeName(sActualTypeName) &&
		__xlIsContainerIndexExpr(pCtx, pNode) ) {
		__xlCgenAppend(&pCtx->buf, "((xfunction)xvoGetFunc((xvalue)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")))");
		return;
	}
	if ( __xlIsFunctionTypeName(sExpectedTypeName) && __xlIsVarTypeName(sStorageTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "((xfunction)xvoGetFunc((xvalue)(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")))");
		return;
	}
	if ( __xlIsVarTypeName(sStorageTypeName) && !__xlIsVarTypeName(sActualTypeName) ) {
		if ( __xlIsIntegerTypeName(sExpectedTypeName) ) {
			__xlCgenAppendFormat(&pCtx->buf, "((%s)xvoGetInt((xvalue)(", __xlCTypeName(sExpectedTypeName));
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")))");
			return;
		}
		if ( __xlIsFloatTypeName(sExpectedTypeName) ) {
			__xlCgenAppendFormat(&pCtx->buf, "((%s)xvoGetFloat((xvalue)(", __xlCTypeName(sExpectedTypeName));
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")))");
			return;
		}
		if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "bool") == 0) ) {
			__xlCgenAppend(&pCtx->buf, "(xvoGetBool((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")) ? 1 : 0)");
			return;
		}
		if ( (sExpectedTypeName != NULL) && ((strcmp(sExpectedTypeName, "string") == 0) || (strcmp(sExpectedTypeName, "type") == 0)) ) {
			__xlCgenAppend(&pCtx->buf, "xl_generated_copy_text(xl_value_to_string((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")))");
			return;
		}
		if ( __xlIsPointTypeName(sExpectedTypeName) ) {
			__xlCgenAppend(&pCtx->buf, "xvoGetPoint((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, "))");
			return;
		}
		if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "time") == 0) ) {
			__xlCgenAppend(&pCtx->buf, "xvoGetTime((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, "))");
			return;
		}
	}
	if ( __xlIsVarTypeName(sActualTypeName) ) {
		if ( __xlIsFunctionTypeName(sExpectedTypeName) ) {
			__xlCgenAppend(&pCtx->buf, "((xfunction)xvoGetFunc((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")))");
			return;
		}
		if ( __xlIsIntegerTypeName(sExpectedTypeName) ) {
			__xlCgenAppendFormat(&pCtx->buf, "((%s)xvoGetInt((xvalue)(", __xlCTypeName(sExpectedTypeName));
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")))");
			return;
		}
		if ( __xlIsFloatTypeName(sExpectedTypeName) ) {
			__xlCgenAppendFormat(&pCtx->buf, "((%s)xvoGetFloat((xvalue)(", __xlCTypeName(sExpectedTypeName));
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")))");
			return;
		}
		if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "bool") == 0) ) {
			__xlCgenAppend(&pCtx->buf, "(xvoGetBool((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")) ? 1 : 0)");
			return;
		}
		if ( (sExpectedTypeName != NULL) && ((strcmp(sExpectedTypeName, "string") == 0) || (strcmp(sExpectedTypeName, "type") == 0)) ) {
			__xlCgenAppend(&pCtx->buf, "xl_generated_copy_text(xl_value_to_string((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ")))");
			return;
		}
		if ( __xlIsPointTypeName(sExpectedTypeName) ) {
			__xlCgenAppend(&pCtx->buf, "xvoGetPoint((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, "))");
			return;
		}
		if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "time") == 0) ) {
			__xlCgenAppend(&pCtx->buf, "xvoGetTime((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, "))");
			return;
		}
		if ( __xlIsValueBackedTypeName(sExpectedTypeName) ) {
			__xlCgenAppend(&pCtx->buf, "xvoCopy((xvalue)(");
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, "))");
			return;
		}
	}
	if ( (sExpectedTypeName != NULL) && ((strcmp(sExpectedTypeName, "string") == 0) || (strcmp(sExpectedTypeName, "type") == 0)) &&
		(sActualTypeName != NULL) && (strcmp(sActualTypeName, "error") == 0) ) {
		__xlCgenTextExpr(pCtx, pNode);
		return;
	}
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "error") == 0) &&
		(sActualTypeName != NULL) && (strcmp(sActualTypeName, "string") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_from_text(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( (sExpectedTypeName != NULL) && ((strcmp(sExpectedTypeName, "string") == 0) || (strcmp(sExpectedTypeName, "type") == 0)) &&
		__xlIsPointTypeName(sActualTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_point_to_string(");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	__xlCgenExpr(pCtx, pNode);
}

static void __xlCgenValueExprAsType(XL_CgenContext* pCtx, const char* sValueExpr, const char* sExpectedTypeName)
{
	char sFutureValueType[128];

	if ( __xlExtractFutureValueType(sExpectedTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureReadyFunc(sFutureValueType));
		__xlCgenValueExprAsType(pCtx, sValueExpr, sFutureValueType);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( __xlIsVarTypeName(sExpectedTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCopy((xvalue)(");
		__xlCgenAppend(&pCtx->buf, (sValueExpr != NULL) ? sValueExpr : "NULL");
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	if ( __xlIsIntegerTypeName(sExpectedTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "((%s)xvoGetInt((xvalue)(%s)))", __xlCTypeName(sExpectedTypeName), (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( __xlIsFunctionTypeName(sExpectedTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "((xfunction)xvoGetFunc((xvalue)(%s)))", (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( __xlIsFloatTypeName(sExpectedTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "((%s)xvoGetFloat((xvalue)(%s)))", __xlCTypeName(sExpectedTypeName), (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "bool") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "(xvoGetBool((xvalue)(%s)) ? 1 : 0)", (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( (sExpectedTypeName != NULL) && ((strcmp(sExpectedTypeName, "string") == 0) || (strcmp(sExpectedTypeName, "type") == 0)) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xl_generated_copy_text(xl_value_to_string((xvalue)(%s)))", (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "error") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xl_generated_error_from_text(xl_value_to_string((xvalue)(%s)))", (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( __xlIsPointTypeName(sExpectedTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoGetPoint((xvalue)(%s))", (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "time") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvoGetTime((xvalue)(%s))", (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( __xlIsValueBackedTypeName(sExpectedTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCopy((xvalue)(");
		__xlCgenAppend(&pCtx->buf, (sValueExpr != NULL) ? sValueExpr : "NULL");
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
	__xlCgenAppend(&pCtx->buf, (sValueExpr != NULL) ? sValueExpr : "NULL");
}

static void __xlCgenTypeCtorCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	if ( __xlIsVarTypeName(pNode->sTypeName) ) {
		__xlCgenBoxValueExpr(pCtx, pArg0);
		return;
	}

	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_type_ctor_value = ");
	__xlCgenBoxValueExpr(pCtx, pArg0);
	__xlCgenAppend(&pCtx->buf, "; ");
	__xlCgenValueExprAsType(pCtx, "xl_type_ctor_value", pNode->sTypeName);
	__xlCgenAppend(&pCtx->buf, "; })");
}

static int __xlIsChannelCtorCall(const XL_MirNode* pNode)
{
	const XL_MirNode* pCallee;
	const char* sCalleeName;

	if ( (pNode == NULL) || (pNode->eKind != XL_MIR_CALL) || (pNode->iFuncIndex >= 0) ||
		(pNode->sTypeName == NULL) || (strcmp(pNode->sTypeName, "channel") != 0) ) {
		return 0;
	}

	pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sCalleeName = (pCallee != NULL) ? pCallee->sText : NULL;
	return (sCalleeName != NULL) && (strcmp(sCalleeName, "channel") == 0);
}

static int __xlIsTypeQueryCall(const XL_MirNode* pNode)
{
	const XL_MirNode* pCallee;

	if ( (pNode == NULL) || (pNode->eKind != XL_MIR_CALL) || (pNode->iFuncIndex >= 0) || (pNode->iChildCount < 2) ) {
		return 0;
	}

	pCallee = pNode->ppChildren[0];
	return (pCallee != NULL) &&
		(pCallee->iChildCount == 0) &&
		(pCallee->sText != NULL) &&
		(strcmp(pCallee->sText, "Type") == 0);
}

static void __xlCgenChannelCtorCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;

	__xlCgenAppend(&pCtx->buf, "xl_channel_create(");
	if ( pArg0 != NULL ) {
		__xlCgenExprAsType(pCtx, pArg0, "int");
	} else {
		__xlCgenAppend(&pCtx->buf, "0");
	}
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenTypeQueryCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sArgTypeName;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sArgTypeName = __xlInferExprTypeName(pCtx, pArg0);

	if ( __xlIsVarTypeName(sArgTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_type_of_value((xvalue)(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}

	__xlCgenAppendCString(&pCtx->buf, (sArgTypeName != NULL) ? sArgTypeName : "");
}

static void __xlCgenTaskArgExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const char* sArgTypeName;

	sArgTypeName = __xlInferExprTypeName(pCtx, pNode);
	if ( __xlIsFutureTypeName(sArgTypeName) || __xlIsTaskTypeName(sArgTypeName) ) {
		__xlCgenExpr(pCtx, pNode);
		return;
	}

	__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureReadyFunc(sArgTypeName));
	__xlCgenExpr(pCtx, pNode);
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenTaskCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sCalleeName;
	const char* sTypeName;
	const char* sSuffix;
	const char* sTraceName;
	XL_TaskHelperKind eKind;

	sCalleeName = __xlCallCalleeName(pNode);
	eKind = __xlTaskHelperKind(sCalleeName);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sTypeName = __xlInferExprTypeName(pCtx, pNode);
	sSuffix = __xlTaskTypeSuffix(sTypeName);
	sTraceName = (sCalleeName != NULL) ? sCalleeName : "task";

	switch ( eKind ) {
		case XL_TASK_HELPER_SPAWN:
			__xlCgenAppendFormat(&pCtx->buf, "xl_task_spawn_%s(", sSuffix);
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTaskArgExpr(pCtx, (pArgs != NULL) ? pArgs->ppChildren[0] : NULL);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_TASK_HELPER_ALL:
			__xlCgenAppendFormat(&pCtx->buf, "xl_task_all_%s(", sSuffix);
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTaskArgExpr(pCtx, ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTaskArgExpr(pCtx, ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_TASK_HELPER_RACE:
			__xlCgenAppendFormat(&pCtx->buf, "xl_task_race_%s(", sSuffix);
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTaskArgExpr(pCtx, ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTaskArgExpr(pCtx, ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_TASK_HELPER_CANCEL:
			sSuffix = __xlTaskTypeSuffix(__xlInferExprTypeName(pCtx, pArg0));
			__xlCgenAppendFormat(&pCtx->buf, "xl_task_cancel_%s(", sSuffix);
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTaskArgExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_TASK_HELPER_DELAY:
			__xlCgenAppend(&pCtx->buf, "xl_task_delay(");
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, (pArgs != NULL) ? pArgs->ppChildren[0] : NULL);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenCorePrintCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sCalleeName;
	const char* sArgTypeName;
	const char* sSuffix;
	XL_CorePrintHelperKind eKind;

	sCalleeName = __xlCallCalleeName(pNode);
	eKind = __xlCorePrintHelperKind(sCalleeName);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sArgTypeName = __xlInferExprTypeName(pCtx, pArg0);
	sSuffix = __xlCorePrintTypeSuffix(sArgTypeName);

	if ( eKind == XL_CORE_PRINT_NONE ) {
		return;
	}

	if ( (eKind == XL_CORE_PRINTLN) && ((pArgs == NULL) || (pArgs->iChildCount == 0)) ) {
		__xlCgenAppend(&pCtx->buf, "xl_core_println_void()");
		return;
	}

	__xlCgenAppendFormat(
		&pCtx->buf,
		(eKind == XL_CORE_PRINT) ? "xl_core_print_%s(" : "xl_core_println_%s(",
		sSuffix
	);
	if ( (sArgTypeName != NULL) && (strcmp(sArgTypeName, "error") == 0) ) {
		__xlCgenTextExpr(pCtx, pArg0);
	} else {
		__xlCgenExprAsType(pCtx, pArg0, sArgTypeName);
	}
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenIoCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sArgTypeName;
	const char* sSuffix;
	XL_IoHelperKind eKind;

	eKind = __xlIoHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sArgTypeName = __xlInferExprTypeName(pCtx, pArg0);
	sSuffix = __xlCorePrintTypeSuffix(sArgTypeName);

	switch ( eKind ) {
		case XL_IO_HELPER_EPRINT:
			__xlCgenAppendFormat(&pCtx->buf, "xl_io_eprint_%s(", sSuffix);
			if ( (sArgTypeName != NULL) && (strcmp(sArgTypeName, "error") == 0) ) {
				__xlCgenTextExpr(pCtx, pArg0);
			} else {
				__xlCgenExprAsType(pCtx, pArg0, sArgTypeName);
			}
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_IO_HELPER_EPRINTLN:
			if ( (pArgs == NULL) || (pArgs->iChildCount == 0) ) {
				__xlCgenAppend(&pCtx->buf, "xl_io_eprintln_void()");
				return;
			}
			__xlCgenAppendFormat(&pCtx->buf, "xl_io_eprintln_%s(", sSuffix);
			if ( (sArgTypeName != NULL) && (strcmp(sArgTypeName, "error") == 0) ) {
				__xlCgenTextExpr(pCtx, pArg0);
			} else {
				__xlCgenExprAsType(pCtx, pArg0, sArgTypeName);
			}
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_IO_HELPER_READ_LINE:
			__xlCgenAppend(&pCtx->buf, "xl_io_read_line()");
			return;
		case XL_IO_HELPER_FLUSH:
			__xlCgenAppend(&pCtx->buf, "xl_io_flush()");
			return;
		default:
			return;
	}
}

static void __xlCgenCoreLenCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sArgTypeName;
	const char* sSuffix;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sArgTypeName = __xlInferExprTypeName(pCtx, pArg0);
	sSuffix = __xlCoreLenTypeSuffix(sArgTypeName);

	__xlCgenAppendFormat(&pCtx->buf, "xl_core_len_%s(", sSuffix);
	if ( (sArgTypeName != NULL) && (strcmp(sArgTypeName, "string") == 0) ) {
		__xlCgenExprAsType(pCtx, pArg0, "string");
	} else {
		__xlCgenExpr(pCtx, pArg0);
	}
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenLengthMemberExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pBase;
	const char* sBaseTypeName;
	const char* sSuffix;

	pBase = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sBaseTypeName = __xlInferExprTypeName(pCtx, pBase);
	sSuffix = __xlCoreLenTypeSuffix(sBaseTypeName);

	__xlCgenAppendFormat(&pCtx->buf, "xl_core_len_%s(", sSuffix);
	if ( (sBaseTypeName != NULL) && (strcmp(sBaseTypeName, "string") == 0) ) {
		__xlCgenExprAsType(pCtx, pBase, "string");
	} else {
		__xlCgenExpr(pCtx, pBase);
	}
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenContainerMethodCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sCalleeName;
	const char* sBaseTypeName;
	const char* sMethodName;

	pBase = __xlContainerMethodBase(pNode);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	sCalleeName = __xlCallCalleeName(pNode);
	if ( (sCalleeName != NULL) &&
		((strcmp(sCalleeName, "__xl_method_has") == 0) ||
		(strcmp(sCalleeName, "__xl_method_append") == 0) ||
		(strcmp(sCalleeName, "__xl_method_join") == 0) ||
		(strcmp(sCalleeName, "__xl_method_remove") == 0)) ) {
		pArg0 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	} else {
		pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	}
	sBaseTypeName = __xlInferExprTypeName(pCtx, pBase);
	sMethodName = __xlContainerMethodName(pNode);

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "len") == 0) ) {
		if ( __xlTypeHasBaseName(sBaseTypeName, "array") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_array(");
		} else if ( __xlTypeHasBaseName(sBaseTypeName, "list") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_list(");
		} else if ( __xlTypeHasBaseName(sBaseTypeName, "set") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_set(");
		} else if ( __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_dict(");
		} else {
			__xlCgenAppend(&pCtx->buf, "0");
			return;
		}
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "has") == 0) && __xlTypeHasBaseName(sBaseTypeName, "set") ) {
		__xlCgenAppend(&pCtx->buf, "({ xvalue xl_set_item = ");
		__xlCgenBoxValueExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, "; int xl_set_has = xvoCollExists((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), xl_set_item) ? 1 : 0; xvoUnref(xl_set_item); xl_set_has; })");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "has") == 0) && __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pArg0, "string");
		__xlCgenAppend(&pCtx->buf, "; int xl_dict_has = xvoTableExists((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), (str)xl_dict_key, (uint32)strlen((xl_dict_key != NULL) ? xl_dict_key : \"\")) ? 1 : 0; xl_dict_has; })");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "append") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_append((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenBoxValueExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "append") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_append((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenBoxValueExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "join") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_join((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenExprAsType(pCtx, pArg0, "string");
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "join") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_join((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenExprAsType(pCtx, pArg0, "string");
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "set") ) {
		__xlCgenAppend(&pCtx->buf, "xl_set_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "xl_dict_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenExprAsType(pCtx, pArg0, "int");
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenExprAsType(pCtx, pArg0, "int");
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "set") ) {
		__xlCgenAppend(&pCtx->buf, "({ xvalue xl_set_item = ");
		__xlCgenBoxValueExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, "; int xl_set_removed = xl_set_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), xl_set_item); xvoUnref(xl_set_item); xl_set_removed; })");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		__xlCgenExprAsType(pCtx, pArg0, "string");
		__xlCgenAppend(&pCtx->buf, "; xl_dict_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), xl_dict_key); })");
		return;
	}
}

static void __xlCgenChannelMethodCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sCalleeName;
	const char* sMethodName;

	pBase = __xlChannelMethodBase(pNode);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	sCalleeName = __xlCallCalleeName(pNode);
	if ( (sCalleeName != NULL) && (strcmp(sCalleeName, "__xl_method_channel_send") == 0) ) {
		pArg0 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	} else {
		pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	}
	sMethodName = __xlChannelMethodName(pNode);

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "send") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_channel_send((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		__xlCgenBoxValueExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "recv") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_future_ptr_ready(xl_channel_recv((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")))");
		return;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "close") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_channel_close((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return;
	}
}

static void __xlCgenTimeCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	XL_TimeHelperKind eKind;

	eKind = __xlTimeHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;

	if ( eKind == XL_TIME_HELPER_NOW ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_now()");
		return;
	}
	if ( eKind == XL_TIME_HELPER_TO_STRING ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_to_string(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( eKind == XL_TIME_HELPER_NOW_TEXT ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_now_text()");
		return;
	}
	if ( eKind == XL_TIME_HELPER_DATE_TEXT ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_date_text()");
		return;
	}
	if ( eKind == XL_TIME_HELPER_TIME_TEXT ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_time_text()");
		return;
	}
	if ( eKind == XL_TIME_HELPER_RELATIVE ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_relative(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pArg1);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( eKind == XL_TIME_HELPER_PARSE ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_parse(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( eKind == XL_TIME_HELPER_FORMAT ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_format(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pArg1);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( eKind == XL_TIME_HELPER_PARSE_FORMAT ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_parse_format(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pArg1);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( eKind == XL_TIME_HELPER_SLEEP ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_sleep(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( eKind == XL_TIME_HELPER_IS_LEAP_YEAR ) {
		__xlCgenAppend(&pCtx->buf, "xl_time_is_leap_year(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
	}
}

static void __xlCgenPathCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	const XL_MirNode* pArg2;
	XL_PathHelperKind eKind;

	eKind = __xlPathHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	pArg2 = ((pArgs != NULL) && (pArgs->iChildCount > 2)) ? pArgs->ppChildren[2] : NULL;

	switch ( eKind ) {
		case XL_PATH_HELPER_JOIN:
			__xlCgenAppend(&pCtx->buf, "xl_path_join(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_NAME_EXT:
			__xlCgenAppend(&pCtx->buf, "xl_path_name_ext(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_NAME:
			__xlCgenAppend(&pCtx->buf, "xl_path_name(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_EXT:
			__xlCgenAppend(&pCtx->buf, "xl_path_ext(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_DIR:
			__xlCgenAppend(&pCtx->buf, "xl_path_dir(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_IS_ABS:
			__xlCgenAppend(&pCtx->buf, "xl_path_is_abs(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_EXISTS:
			__xlCgenAppend(&pCtx->buf, "xl_path_exists(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_RANDOM:
			__xlCgenAppend(&pCtx->buf, "xl_path_random(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg2);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenFsCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	XL_FsHelperKind eKind;

	eKind = __xlFsHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;

	switch ( eKind ) {
		case XL_FS_HELPER_EXISTS:
			__xlCgenAppend(&pCtx->buf, "xl_fs_exists(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_IS_FILE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_is_file(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_IS_DIR:
			__xlCgenAppend(&pCtx->buf, "xl_fs_is_dir(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_SIZE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_size(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_SET_SIZE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_set_size(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_ACCESS_TIME:
			__xlCgenAppend(&pCtx->buf, "xl_fs_access_time(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_CHANGE_TIME:
			__xlCgenAppend(&pCtx->buf, "xl_fs_change_time(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_READ_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_read_all(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_WRITE_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_write_all(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_APPEND_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_append_all(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_MKDIR_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_mkdir_all(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_COPY:
			__xlCgenAppend(&pCtx->buf, "xl_fs_copy(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_MOVE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_move(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_DELETE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_delete(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenStrCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	const XL_MirNode* pArg2;
	XL_StrHelperKind eKind;

	eKind = __xlStrHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	pArg2 = ((pArgs != NULL) && (pArgs->iChildCount > 2)) ? pArgs->ppChildren[2] : NULL;

	switch ( eKind ) {
		case XL_STR_HELPER_EMPTY:
			__xlCgenAppend(&pCtx->buf, "xl_str_empty(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_BYTE_LEN:
			__xlCgenAppend(&pCtx->buf, "xl_str_byte_len(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_CONTAINS:
			__xlCgenAppend(&pCtx->buf, "xl_str_contains(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_INDEX_OF:
			__xlCgenAppend(&pCtx->buf, "xl_str_index_of(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_LAST_INDEX_OF:
			__xlCgenAppend(&pCtx->buf, "xl_str_last_index_of(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_SLICE:
			__xlCgenAppend(&pCtx->buf, "xl_str_slice(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "int");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg2, "int");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_REPEAT:
			__xlCgenAppend(&pCtx->buf, "xl_str_repeat(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "int");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_SPLIT:
			__xlCgenAppend(&pCtx->buf, "xl_str_split(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_SPLIT_LINES:
			__xlCgenAppend(&pCtx->buf, "xl_str_split_lines(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_STARTS_WITH:
			__xlCgenAppend(&pCtx->buf, "xl_str_starts_with(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_ENDS_WITH:
			__xlCgenAppend(&pCtx->buf, "xl_str_ends_with(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_TRIM:
			__xlCgenAppend(&pCtx->buf, "xl_str_trim(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_TRIM_LEFT:
			__xlCgenAppend(&pCtx->buf, "xl_str_trim_left(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_TRIM_RIGHT:
			__xlCgenAppend(&pCtx->buf, "xl_str_trim_right(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_LOWER:
			__xlCgenAppend(&pCtx->buf, "xl_str_lower(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_UPPER:
			__xlCgenAppend(&pCtx->buf, "xl_str_upper(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_REPLACE:
			__xlCgenAppend(&pCtx->buf, "xl_str_replace(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg2, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_REVERSE:
			__xlCgenAppend(&pCtx->buf, "xl_str_reverse(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_PAD_LEFT:
			__xlCgenAppend(&pCtx->buf, "xl_str_pad_left(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "int");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg2, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_PAD_RIGHT:
			__xlCgenAppend(&pCtx->buf, "xl_str_pad_right(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "int");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg2, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenRandCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	XL_RandHelperKind eKind;

	eKind = __xlRandHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;

	switch ( eKind ) {
		case XL_RAND_HELPER_INT32:
			__xlCgenAppend(&pCtx->buf, "xl_rand_int32()");
			return;
		case XL_RAND_HELPER_BOOL:
			__xlCgenAppend(&pCtx->buf, "xl_rand_bool()");
			return;
		case XL_RAND_HELPER_RANGE:
			__xlCgenAppend(&pCtx->buf, "xl_rand_range(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_RAND_HELPER_TEXT:
			__xlCgenAppend(&pCtx->buf, "xl_rand_text(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_RAND_HELPER_TEXT_FROM:
			__xlCgenAppend(&pCtx->buf, "xl_rand_text_from(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenHostCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	XL_HostHelperKind eKind;

	eKind = __xlHostHelperKind(__xlCallCalleeName(pNode));
	switch ( eKind ) {
		case XL_HOST_HELPER_NAME:
			__xlCgenAppend(&pCtx->buf, "xl_host_name()");
			return;
		case XL_HOST_HELPER_IP:
			__xlCgenAppend(&pCtx->buf, "xl_host_ip()");
			return;
		case XL_HOST_HELPER_USER:
			__xlCgenAppend(&pCtx->buf, "xl_host_user()");
			return;
		case XL_HOST_HELPER_OS:
			__xlCgenAppend(&pCtx->buf, "xl_host_os()");
			return;
		case XL_HOST_HELPER_ARCH:
			__xlCgenAppend(&pCtx->buf, "xl_host_arch()");
			return;
		case XL_HOST_HELPER_EXECUTABLE:
			__xlCgenAppend(&pCtx->buf, "xl_host_executable()");
			return;
		default:
			return;
	}
}

static void __xlCgenFmtCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	XL_FmtHelperKind eKind;

	eKind = __xlFmtHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;

	switch ( eKind ) {
		case XL_FMT_HELPER_INT_TEXT:
			__xlCgenAppend(&pCtx->buf, "xl_fmt_int_text(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FMT_HELPER_FLOAT_TEXT:
			__xlCgenAppend(&pCtx->buf, "xl_fmt_float_text(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FMT_HELPER_BOOL_TEXT:
			__xlCgenAppend(&pCtx->buf, "xl_fmt_bool_text(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenDebugCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sType0;
	XL_DebugHelperKind eKind;

	eKind = __xlDebugHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sType0 = __xlInferExprTypeName(pCtx, pArg0);

	switch ( eKind ) {
		case XL_DEBUG_HELPER_TEXT:
			if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
				__xlCgenAppend(&pCtx->buf, "xl_generated_error_text(");
				__xlCgenExpr(pCtx, pArg0);
				__xlCgenAppend(&pCtx->buf, ")");
			} else {
				__xlCgenAppend(&pCtx->buf, "xl_debug_text(");
				__xlCgenExprAsType(pCtx, pArg0, "var");
				__xlCgenAppend(&pCtx->buf, ")");
			}
			return;
		case XL_DEBUG_HELPER_PRINT:
			if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
				__xlCgenAppend(&pCtx->buf, "xl_core_print_string(xl_generated_error_text(");
				__xlCgenExpr(pCtx, pArg0);
				__xlCgenAppend(&pCtx->buf, "))");
			} else {
				__xlCgenAppend(&pCtx->buf, "xl_debug_print(");
				__xlCgenExprAsType(pCtx, pArg0, "var");
				__xlCgenAppend(&pCtx->buf, ")");
			}
			return;
		case XL_DEBUG_HELPER_PRINTLN:
			if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
				__xlCgenAppend(&pCtx->buf, "xl_core_println_string(xl_generated_error_text(");
				__xlCgenExpr(pCtx, pArg0);
				__xlCgenAppend(&pCtx->buf, "))");
			} else {
				__xlCgenAppend(&pCtx->buf, "xl_debug_println(");
				__xlCgenExprAsType(pCtx, pArg0, "var");
				__xlCgenAppend(&pCtx->buf, ")");
			}
			return;
		default:
			return;
	}
}

static void __xlCgenLogCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sType0;
	XL_LogHelperKind eKind;

	eKind = __xlLogHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sType0 = __xlInferExprTypeName(pCtx, pArg0);

	switch ( eKind ) {
		case XL_LOG_HELPER_DEBUG:
			__xlCgenAppend(&pCtx->buf, "xl_log_debug(");
			break;
		case XL_LOG_HELPER_INFO:
			__xlCgenAppend(&pCtx->buf, "xl_log_info(");
			break;
		case XL_LOG_HELPER_WARN:
			__xlCgenAppend(&pCtx->buf, "xl_log_warn(");
			break;
		case XL_LOG_HELPER_ERROR:
			__xlCgenAppend(&pCtx->buf, "xl_log_error(");
			break;
		default:
			return;
	}

	if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_text(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
	} else {
		__xlCgenExprAsType(pCtx, pArg0, "string");
	}
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenTraceCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sType0;
	XL_TraceHelperKind eKind;

	eKind = __xlTraceHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	sType0 = __xlInferExprTypeName(pCtx, pArg0);

	switch ( eKind ) {
		case XL_TRACE_HELPER_WRITE:
		case XL_TRACE_HELPER_ERROR:
			__xlCgenAppend(&pCtx->buf, "xl_trace_write(");
			break;
		default:
			return;
	}

	if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_text(");
		__xlCgenExpr(pCtx, pArg0);
		__xlCgenAppend(&pCtx->buf, ")");
	} else {
		__xlCgenExprAsType(pCtx, pArg0, "string");
	}
	__xlCgenAppend(&pCtx->buf, ")");
}

static void __xlCgenRegexCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	const XL_MirNode* pArg2;
	XL_RegexHelperKind eKind;

	eKind = __xlRegexHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	pArg2 = ((pArgs != NULL) && (pArgs->iChildCount > 2)) ? pArgs->ppChildren[2] : NULL;

	switch ( eKind ) {
		case XL_REGEX_HELPER_IS_MATCH:
			__xlCgenAppend(&pCtx->buf, "xl_regex_is_match(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_REGEX_HELPER_FIND:
			__xlCgenAppend(&pCtx->buf, "xl_regex_find(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_REGEX_HELPER_CAPTURES:
			__xlCgenAppend(&pCtx->buf, "xl_regex_captures(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_REGEX_HELPER_CAPTURE_COUNT:
			__xlCgenAppend(&pCtx->buf, "xl_regex_capture_count(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_REGEX_HELPER_REPLACE_FIRST:
			__xlCgenAppend(&pCtx->buf, "xl_regex_replace_first(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg2, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_REGEX_HELPER_REPLACE_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_regex_replace_all(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg2, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenEnvCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	XL_EnvHelperKind eKind;

	eKind = __xlEnvHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;

	switch ( eKind ) {
		case XL_ENV_HELPER_GET:
			__xlCgenAppend(&pCtx->buf, "xl_env_get(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_ENV_HELPER_EXISTS:
			__xlCgenAppend(&pCtx->buf, "xl_env_exists(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_ENV_HELPER_SET:
			__xlCgenAppend(&pCtx->buf, "xl_env_set(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_ENV_HELPER_CWD:
			__xlCgenAppend(&pCtx->buf, "xl_env_cwd()");
			return;
		case XL_ENV_HELPER_TEMP:
			__xlCgenAppend(&pCtx->buf, "xl_env_temp()");
			return;
		case XL_ENV_HELPER_HOME:
			__xlCgenAppend(&pCtx->buf, "xl_env_home()");
			return;
		default:
			return;
	}
}

static void __xlCgenDirCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	XL_DirHelperKind eKind;

	eKind = __xlDirHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;

	switch ( eKind ) {
		case XL_DIR_HELPER_CREATE:
			__xlCgenAppend(&pCtx->buf, "xl_dir_create(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_DIR_HELPER_LIST:
			__xlCgenAppend(&pCtx->buf, "xl_dir_list(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_DIR_HELPER_FILES:
			__xlCgenAppend(&pCtx->buf, "xl_dir_files(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_DIR_HELPER_DIRS:
			__xlCgenAppend(&pCtx->buf, "xl_dir_dirs(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_DIR_HELPER_TREE:
			__xlCgenAppend(&pCtx->buf, "xl_dir_tree(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_DIR_HELPER_COPY:
			__xlCgenAppend(&pCtx->buf, "xl_dir_copy(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_DIR_HELPER_MOVE:
			__xlCgenAppend(&pCtx->buf, "xl_dir_move(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExprAsType(pCtx, pArg1, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_DIR_HELPER_DELETE:
			__xlCgenAppend(&pCtx->buf, "xl_dir_delete(");
			__xlCgenExprAsType(pCtx, pArg0, "string");
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		default:
			return;
	}
}

static void __xlCgenProcessCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	XL_ProcessHelperKind eKind;

	eKind = __xlProcessHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;

	switch ( eKind ) {
		case XL_PROCESS_HELPER_EXEC:
			__xlCgenAppend(&pCtx->buf, "xl_process_exec(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PROCESS_HELPER_CAPTURE:
			__xlCgenAppend(&pCtx->buf, "xl_process_capture(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PROCESS_HELPER_EXEC_IN:
			__xlCgenAppend(&pCtx->buf, "xl_process_exec_in(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PROCESS_HELPER_CAPTURE_IN:
			__xlCgenAppend(&pCtx->buf, "xl_process_capture_in(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PROCESS_HELPER_TERMINAL_SUPPORTED:
			__xlCgenAppend(&pCtx->buf, "xl_process_terminal_supported()");
			return;
		default:
			return;
	}
}

static const XL_MirNode* __xlCgenParamNode(const XL_MirNode* pParams, size_t iIndex)
{
	if ( (pParams == NULL) || (iIndex >= pParams->iChildCount) ) {
		return NULL;
	}
	return pParams->ppChildren[iIndex];
}

static const char* __xlCgenParamName(const XL_MirNode* pParam)
{
	if ( (pParam == NULL) || (pParam->sText == NULL) || (pParam->sText[0] == '\0') ) {
		return "arg";
	}
	return pParam->sText;
}

static const char* __xlCgenParamTypeName(const XL_MirNode* pParam)
{
	if ( (pParam == NULL) || (pParam->sTypeName == NULL) || (pParam->sTypeName[0] == '\0') ) {
		return "int";
	}
	return pParam->sTypeName;
}

static void __xlCgenEmitFuncParamList(XL_CgenContext* pCtx, const XL_MirNode* pParams)
{
	size_t i;

	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		const XL_MirNode* pParam;

		pParam = pParams->ppChildren[i];
		if ( i != 0 ) {
			__xlCgenAppend(&pCtx->buf, ", ");
		}
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s",
			__xlCTypeName(__xlCgenParamTypeName(pParam)),
			__xlCgenParamName(pParam)
		);
	}
}

static void __xlCgenEmitHelperFuncOpen(
	XL_CgenContext* pCtx,
	const XL_CgenFuncInfo* pFuncInfo,
	const XL_MirNode* pParams
)
{
	__xlCgenAppendFormat(
		&pCtx->buf,
		"static %s %s(",
		__xlCTypeName((pFuncInfo != NULL) ? pFuncInfo->sTypeName : "void"),
		(pFuncInfo != NULL) ? pFuncInfo->sSymbolName : "anon"
	);
	__xlCgenEmitFuncParamList(pCtx, pParams);
	__xlCgenAppend(&pCtx->buf, ")\n{\n");
	pCtx->buf.iIndent += 1;
}

static void __xlCgenEmitHelperFuncClose(XL_CgenContext* pCtx)
{
	pCtx->buf.iIndent -= 1;
	__xlCgenAppend(&pCtx->buf, "}\n\n");
}

static int __xlCgenEmitBuiltinHelperFunc(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_CgenFuncInfo* pFuncInfo
)
{
	const XL_MirNode* pParams;
	const XL_MirNode* pParam0;
	const XL_MirNode* pParam1;
	const XL_MirNode* pParam2;
	const char* sParam0;
	const char* sParam1;
	const char* sParam2;
	const char* sType0;
	const char* sSuffix;
	XL_CorePrintHelperKind eCorePrintKind;
	XL_IoHelperKind eIoKind;
	XL_CoreLenHelperKind eCoreLenKind;
	XL_TimeHelperKind eTimeKind;
	XL_PathHelperKind ePathKind;
	XL_FsHelperKind eFsKind;
	XL_StrHelperKind eStrKind;
	XL_RandHelperKind eRandKind;
	XL_HostHelperKind eHostKind;
	XL_FmtHelperKind eFmtKind;
	XL_DebugHelperKind eDebugKind;
	XL_LogHelperKind eLogKind;
	XL_TraceHelperKind eTraceKind;
	XL_RegexHelperKind eRegexKind;
	XL_CryptoHelperKind eCryptoKind;
	XL_EncodeHelperKind eEncodeKind;
	XL_HashHelperKind eHashKind;
	XL_MathHelperKind eMathKind;
	XL_PackHelperKind ePackKind;
	XL_JsonHelperKind eJsonKind;
	XL_EnvHelperKind eEnvKind;
	XL_DirHelperKind eDirKind;
	XL_ProcessHelperKind eProcessKind;

	if ( (pCtx == NULL) || (pNode == NULL) || (pFuncInfo == NULL) ) {
		return 0;
	}

	pParams = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pParam0 = __xlCgenParamNode(pParams, 0);
	pParam1 = __xlCgenParamNode(pParams, 1);
	pParam2 = __xlCgenParamNode(pParams, 2);
	sParam0 = __xlCgenParamName(pParam0);
	sParam1 = __xlCgenParamName(pParam1);
	sParam2 = __xlCgenParamName(pParam2);
	sType0 = __xlCgenParamTypeName(pParam0);

	eCorePrintKind = __xlCorePrintHelperKind(pFuncInfo->sName);
	if ( eCorePrintKind != XL_CORE_PRINT_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		if ( (eCorePrintKind == XL_CORE_PRINTLN) && ((pParams == NULL) || (pParams->iChildCount == 0)) ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xl_core_println_void();\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "return;\n");
			__xlCgenEmitHelperFuncClose(pCtx);
			return 1;
		}

		sSuffix = __xlCorePrintTypeSuffix(sType0);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			(eCorePrintKind == XL_CORE_PRINT) ? "xl_core_print_%s(" : "xl_core_println_%s(",
			sSuffix
		);
		if ( strcmp(sType0, "error") == 0 ) {
			__xlCgenAppendFormat(&pCtx->buf, "xl_generated_error_text(%s)", sParam0);
		} else {
			__xlCgenAppend(&pCtx->buf, sParam0);
		}
		__xlCgenAppend(&pCtx->buf, ");\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return;\n");
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eIoKind = __xlIoHelperKind(pFuncInfo->sName);
	if ( eIoKind != XL_IO_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		sSuffix = __xlCorePrintTypeSuffix(sType0);
		switch ( eIoKind ) {
			case XL_IO_HELPER_EPRINT:
				__xlCgenAppendFormat(&pCtx->buf, "xl_io_eprint_%s(", sSuffix);
				if ( strcmp(sType0, "error") == 0 ) {
					__xlCgenAppendFormat(&pCtx->buf, "xl_generated_error_text(%s)", sParam0);
				} else {
					__xlCgenAppend(&pCtx->buf, sParam0);
				}
				__xlCgenAppend(&pCtx->buf, ");\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return;\n");
				__xlCgenEmitHelperFuncClose(pCtx);
				return 1;
			case XL_IO_HELPER_EPRINTLN:
				if ( (pParams == NULL) || (pParams->iChildCount == 0) ) {
					__xlCgenAppend(&pCtx->buf, "xl_io_eprintln_void();\n");
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "return;\n");
					__xlCgenEmitHelperFuncClose(pCtx);
					return 1;
				}
				__xlCgenAppendFormat(&pCtx->buf, "xl_io_eprintln_%s(", sSuffix);
				if ( strcmp(sType0, "error") == 0 ) {
					__xlCgenAppendFormat(&pCtx->buf, "xl_generated_error_text(%s)", sParam0);
				} else {
					__xlCgenAppend(&pCtx->buf, sParam0);
				}
				__xlCgenAppend(&pCtx->buf, ");\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return;\n");
				__xlCgenEmitHelperFuncClose(pCtx);
				return 1;
			case XL_IO_HELPER_READ_LINE:
				__xlCgenAppend(&pCtx->buf, "return xl_io_read_line();\n");
				__xlCgenEmitHelperFuncClose(pCtx);
				return 1;
			case XL_IO_HELPER_FLUSH:
				__xlCgenAppend(&pCtx->buf, "xl_io_flush();\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return;\n");
				__xlCgenEmitHelperFuncClose(pCtx);
				return 1;
			default:
				break;
		}
	}

	eCoreLenKind = __xlCoreLenHelperKind(pFuncInfo->sName);
	if ( eCoreLenKind != XL_CORE_LEN_NONE ) {
		(void)eCoreLenKind;
		sSuffix = __xlCoreLenTypeSuffix(sType0);
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return xl_core_len_%s(%s);\n", sSuffix, sParam0);
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eTimeKind = __xlTimeHelperKind(pFuncInfo->sName);
	if ( eTimeKind != XL_TIME_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eTimeKind ) {
			case XL_TIME_HELPER_NOW: __xlCgenAppend(&pCtx->buf, "return xl_time_now();\n"); break;
			case XL_TIME_HELPER_TO_STRING: __xlCgenAppendFormat(&pCtx->buf, "return xl_time_to_string(%s);\n", sParam0); break;
			case XL_TIME_HELPER_NOW_TEXT: __xlCgenAppend(&pCtx->buf, "return xl_time_now_text();\n"); break;
			case XL_TIME_HELPER_DATE_TEXT: __xlCgenAppend(&pCtx->buf, "return xl_time_date_text();\n"); break;
			case XL_TIME_HELPER_TIME_TEXT: __xlCgenAppend(&pCtx->buf, "return xl_time_time_text();\n"); break;
			case XL_TIME_HELPER_RELATIVE: __xlCgenAppendFormat(&pCtx->buf, "return xl_time_relative(%s, %s);\n", sParam0, sParam1); break;
			case XL_TIME_HELPER_PARSE: __xlCgenAppendFormat(&pCtx->buf, "return xl_time_parse(%s);\n", sParam0); break;
			case XL_TIME_HELPER_FORMAT: __xlCgenAppendFormat(&pCtx->buf, "return xl_time_format(%s, %s);\n", sParam0, sParam1); break;
			case XL_TIME_HELPER_PARSE_FORMAT: __xlCgenAppendFormat(&pCtx->buf, "return xl_time_parse_format(%s, %s);\n", sParam0, sParam1); break;
			case XL_TIME_HELPER_SLEEP: __xlCgenAppendFormat(&pCtx->buf, "xl_time_sleep(%s);\n", sParam0); __xlCgenAppendIndent(&pCtx->buf); __xlCgenAppend(&pCtx->buf, "return;\n"); break;
			case XL_TIME_HELPER_IS_LEAP_YEAR: __xlCgenAppendFormat(&pCtx->buf, "return xl_time_is_leap_year(%s);\n", sParam0); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	ePathKind = __xlPathHelperKind(pFuncInfo->sName);
	if ( ePathKind != XL_PATH_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( ePathKind ) {
			case XL_PATH_HELPER_JOIN: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_join(%s, %s);\n", sParam0, sParam1); break;
			case XL_PATH_HELPER_NAME_EXT: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_name_ext(%s);\n", sParam0); break;
			case XL_PATH_HELPER_NAME: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_name(%s);\n", sParam0); break;
			case XL_PATH_HELPER_EXT: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_ext(%s);\n", sParam0); break;
			case XL_PATH_HELPER_DIR: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_dir(%s);\n", sParam0); break;
			case XL_PATH_HELPER_EXISTS: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_exists(%s);\n", sParam0); break;
			case XL_PATH_HELPER_IS_ABS: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_is_abs(%s);\n", sParam0); break;
			case XL_PATH_HELPER_RANDOM: __xlCgenAppendFormat(&pCtx->buf, "return xl_path_random(%s, %s, %s);\n", sParam0, sParam1, sParam2); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eFsKind = __xlFsHelperKind(pFuncInfo->sName);
	if ( eFsKind != XL_FS_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eFsKind ) {
			case XL_FS_HELPER_EXISTS: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_exists(%s);\n", sParam0); break;
			case XL_FS_HELPER_IS_FILE: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_is_file(%s);\n", sParam0); break;
			case XL_FS_HELPER_IS_DIR: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_is_dir(%s);\n", sParam0); break;
			case XL_FS_HELPER_SIZE: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_size(%s);\n", sParam0); break;
			case XL_FS_HELPER_SET_SIZE: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_set_size(%s, %s);\n", sParam0, sParam1); break;
			case XL_FS_HELPER_ACCESS_TIME: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_access_time(%s);\n", sParam0); break;
			case XL_FS_HELPER_CHANGE_TIME: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_change_time(%s);\n", sParam0); break;
			case XL_FS_HELPER_READ_ALL: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_read_all(%s);\n", sParam0); break;
			case XL_FS_HELPER_WRITE_ALL: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_write_all(%s, %s);\n", sParam0, sParam1); break;
			case XL_FS_HELPER_APPEND_ALL: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_append_all(%s, %s);\n", sParam0, sParam1); break;
			case XL_FS_HELPER_MKDIR_ALL: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_mkdir_all(%s);\n", sParam0); break;
			case XL_FS_HELPER_COPY: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_copy(%s, %s);\n", sParam0, sParam1); break;
			case XL_FS_HELPER_MOVE: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_move(%s, %s);\n", sParam0, sParam1); break;
			case XL_FS_HELPER_DELETE: __xlCgenAppendFormat(&pCtx->buf, "return xl_fs_delete(%s);\n", sParam0); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eStrKind = __xlStrHelperKind(pFuncInfo->sName);
	if ( eStrKind != XL_STR_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eStrKind ) {
			case XL_STR_HELPER_EMPTY: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_empty(%s);\n", sParam0); break;
			case XL_STR_HELPER_BYTE_LEN: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_byte_len(%s);\n", sParam0); break;
			case XL_STR_HELPER_CONTAINS: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_contains(%s, %s);\n", sParam0, sParam1); break;
			case XL_STR_HELPER_INDEX_OF: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_index_of(%s, %s);\n", sParam0, sParam1); break;
			case XL_STR_HELPER_LAST_INDEX_OF: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_last_index_of(%s, %s);\n", sParam0, sParam1); break;
			case XL_STR_HELPER_SLICE: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_slice(%s, %s, %s);\n", sParam0, sParam1, sParam2); break;
			case XL_STR_HELPER_REPEAT: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_repeat(%s, %s);\n", sParam0, sParam1); break;
			case XL_STR_HELPER_SPLIT: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_split(%s, %s);\n", sParam0, sParam1); break;
			case XL_STR_HELPER_SPLIT_LINES: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_split_lines(%s);\n", sParam0); break;
			case XL_STR_HELPER_STARTS_WITH: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_starts_with(%s, %s);\n", sParam0, sParam1); break;
			case XL_STR_HELPER_ENDS_WITH: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_ends_with(%s, %s);\n", sParam0, sParam1); break;
			case XL_STR_HELPER_TRIM: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_trim(%s);\n", sParam0); break;
			case XL_STR_HELPER_TRIM_LEFT: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_trim_left(%s);\n", sParam0); break;
			case XL_STR_HELPER_TRIM_RIGHT: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_trim_right(%s);\n", sParam0); break;
			case XL_STR_HELPER_LOWER: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_lower(%s);\n", sParam0); break;
			case XL_STR_HELPER_UPPER: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_upper(%s);\n", sParam0); break;
			case XL_STR_HELPER_REPLACE: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_replace(%s, %s, %s);\n", sParam0, sParam1, sParam2); break;
			case XL_STR_HELPER_REVERSE: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_reverse(%s);\n", sParam0); break;
			case XL_STR_HELPER_PAD_LEFT: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_pad_left(%s, %s, %s);\n", sParam0, sParam1, sParam2); break;
			case XL_STR_HELPER_PAD_RIGHT: __xlCgenAppendFormat(&pCtx->buf, "return xl_str_pad_right(%s, %s, %s);\n", sParam0, sParam1, sParam2); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eRandKind = __xlRandHelperKind(pFuncInfo->sName);
	if ( eRandKind != XL_RAND_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eRandKind ) {
			case XL_RAND_HELPER_INT32: __xlCgenAppend(&pCtx->buf, "return xl_rand_int32();\n"); break;
			case XL_RAND_HELPER_BOOL: __xlCgenAppend(&pCtx->buf, "return xl_rand_bool();\n"); break;
			case XL_RAND_HELPER_RANGE: __xlCgenAppendFormat(&pCtx->buf, "return xl_rand_range(%s, %s);\n", sParam0, sParam1); break;
			case XL_RAND_HELPER_TEXT: __xlCgenAppendFormat(&pCtx->buf, "return xl_rand_text(%s);\n", sParam0); break;
			case XL_RAND_HELPER_TEXT_FROM: __xlCgenAppendFormat(&pCtx->buf, "return xl_rand_text_from(%s, %s);\n", sParam0, sParam1); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eHostKind = __xlHostHelperKind(pFuncInfo->sName);
	if ( eHostKind != XL_HOST_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eHostKind ) {
			case XL_HOST_HELPER_NAME: __xlCgenAppend(&pCtx->buf, "return xl_host_name();\n"); break;
			case XL_HOST_HELPER_IP: __xlCgenAppend(&pCtx->buf, "return xl_host_ip();\n"); break;
			case XL_HOST_HELPER_USER: __xlCgenAppend(&pCtx->buf, "return xl_host_user();\n"); break;
			case XL_HOST_HELPER_OS: __xlCgenAppend(&pCtx->buf, "return xl_host_os();\n"); break;
			case XL_HOST_HELPER_ARCH: __xlCgenAppend(&pCtx->buf, "return xl_host_arch();\n"); break;
			case XL_HOST_HELPER_EXECUTABLE: __xlCgenAppend(&pCtx->buf, "return xl_host_executable();\n"); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eFmtKind = __xlFmtHelperKind(pFuncInfo->sName);
	if ( eFmtKind != XL_FMT_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eFmtKind ) {
			case XL_FMT_HELPER_INT_TEXT: __xlCgenAppendFormat(&pCtx->buf, "return xl_fmt_int_text(%s, %s);\n", sParam0, sParam1); break;
			case XL_FMT_HELPER_FLOAT_TEXT: __xlCgenAppendFormat(&pCtx->buf, "return xl_fmt_float_text(%s, %s);\n", sParam0, sParam1); break;
			case XL_FMT_HELPER_BOOL_TEXT: __xlCgenAppendFormat(&pCtx->buf, "return xl_fmt_bool_text(%s);\n", sParam0); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eDebugKind = __xlDebugHelperKind(pFuncInfo->sName);
	if ( eDebugKind != XL_DEBUG_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eDebugKind ) {
			case XL_DEBUG_HELPER_TEXT:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_debug_text(%s);\n", sParam0);
				break;
			case XL_DEBUG_HELPER_PRINT:
				__xlCgenAppendFormat(&pCtx->buf, "xl_debug_print(%s);\n", sParam0);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return;\n");
				break;
			case XL_DEBUG_HELPER_PRINTLN:
				__xlCgenAppendFormat(&pCtx->buf, "xl_debug_println(%s);\n", sParam0);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return;\n");
				break;
			default:
				break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eLogKind = __xlLogHelperKind(pFuncInfo->sName);
	if ( eLogKind != XL_LOG_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eLogKind ) {
			case XL_LOG_HELPER_DEBUG:
				if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_debug(xl_generated_error_text(%s));\n", sParam0);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_debug(%s);\n", sParam0);
				}
				break;
			case XL_LOG_HELPER_INFO:
				if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_info(xl_generated_error_text(%s));\n", sParam0);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_info(%s);\n", sParam0);
				}
				break;
			case XL_LOG_HELPER_WARN:
				if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_warn(xl_generated_error_text(%s));\n", sParam0);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_warn(%s);\n", sParam0);
				}
				break;
			case XL_LOG_HELPER_ERROR:
				if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_error(xl_generated_error_text(%s));\n", sParam0);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "xl_log_error(%s);\n", sParam0);
				}
				break;
			default:
				break;
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return;\n");
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eTraceKind = __xlTraceHelperKind(pFuncInfo->sName);
	if ( eTraceKind != XL_TRACE_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eTraceKind ) {
			case XL_TRACE_HELPER_WRITE:
			case XL_TRACE_HELPER_ERROR:
				if ( (sType0 != NULL) && (strcmp(sType0, "error") == 0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "xl_trace_write(xl_generated_error_text(%s));\n", sParam0);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "xl_trace_write(%s);\n", sParam0);
				}
				break;
			default:
				break;
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return;\n");
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eRegexKind = __xlRegexHelperKind(pFuncInfo->sName);
	if ( eRegexKind != XL_REGEX_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eRegexKind ) {
			case XL_REGEX_HELPER_IS_MATCH:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_regex_is_match(%s, %s);\n", sParam0, sParam1);
				break;
			case XL_REGEX_HELPER_FIND:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_regex_find(%s, %s);\n", sParam0, sParam1);
				break;
			case XL_REGEX_HELPER_CAPTURES:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_regex_captures(%s, %s);\n", sParam0, sParam1);
				break;
			case XL_REGEX_HELPER_CAPTURE_COUNT:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_regex_capture_count(%s);\n", sParam0);
				break;
			case XL_REGEX_HELPER_REPLACE_FIRST:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_regex_replace_first(%s, %s, %s);\n", sParam0, sParam1, sParam2);
				break;
			case XL_REGEX_HELPER_REPLACE_ALL:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_regex_replace_all(%s, %s, %s);\n", sParam0, sParam1, sParam2);
				break;
			default:
				break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eCryptoKind = __xlCryptoHelperKind(pFuncInfo->sName);
	if ( eCryptoKind != XL_CRYPTO_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eCryptoKind ) {
			case XL_CRYPTO_HELPER_HMAC_SHA256: __xlCgenAppendFormat(&pCtx->buf, "return xl_crypto_hmac_sha256(%s, %s);\n", sParam0, sParam1); break;
			case XL_CRYPTO_HELPER_HMAC_SHA512: __xlCgenAppendFormat(&pCtx->buf, "return xl_crypto_hmac_sha512(%s, %s);\n", sParam0, sParam1); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eEncodeKind = __xlEncodeHelperKind(pFuncInfo->sName);
	if ( eEncodeKind != XL_ENCODE_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eEncodeKind ) {
			case XL_ENCODE_HELPER_HEX_ENCODE: __xlCgenAppendFormat(&pCtx->buf, "return xl_encode_hex_encode(%s);\n", sParam0); break;
			case XL_ENCODE_HELPER_HEX_DECODE: __xlCgenAppendFormat(&pCtx->buf, "return xl_encode_hex_decode(%s);\n", sParam0); break;
			case XL_ENCODE_HELPER_BASE64_ENCODE: __xlCgenAppendFormat(&pCtx->buf, "return xl_encode_base64_encode(%s);\n", sParam0); break;
			case XL_ENCODE_HELPER_BASE64_DECODE: __xlCgenAppendFormat(&pCtx->buf, "return xl_encode_base64_decode(%s);\n", sParam0); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eHashKind = __xlHashHelperKind(pFuncInfo->sName);
	if ( eHashKind != XL_HASH_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eHashKind ) {
			case XL_HASH_HELPER_HASH32: __xlCgenAppendFormat(&pCtx->buf, "return xl_hash_hash32(%s);\n", sParam0); break;
			case XL_HASH_HELPER_HASH64: __xlCgenAppendFormat(&pCtx->buf, "return xl_hash_hash64(%s);\n", sParam0); break;
			case XL_HASH_HELPER_HASH64_MICRO: __xlCgenAppendFormat(&pCtx->buf, "return xl_hash_hash64_micro(%s);\n", sParam0); break;
			case XL_HASH_HELPER_HASH64_NANO: __xlCgenAppendFormat(&pCtx->buf, "return xl_hash_hash64_nano(%s);\n", sParam0); break;
			case XL_HASH_HELPER_SHA256: __xlCgenAppendFormat(&pCtx->buf, "return xl_hash_sha256(%s);\n", sParam0); break;
			case XL_HASH_HELPER_SHA512: __xlCgenAppendFormat(&pCtx->buf, "return xl_hash_sha512(%s);\n", sParam0); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eMathKind = __xlMathHelperKind(pFuncInfo->sName);
	if ( eMathKind != XL_MATH_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eMathKind ) {
			case XL_MATH_HELPER_ABS:
				if ( __xlIsFloatTypeName(sType0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_abs_float(%s);\n", sParam0);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_abs_int(%s);\n", sParam0);
				}
				break;
			case XL_MATH_HELPER_MIN:
				if ( __xlIsFloatTypeName(sType0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_min_float(%s, %s);\n", sParam0, sParam1);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_min_int(%s, %s);\n", sParam0, sParam1);
				}
				break;
			case XL_MATH_HELPER_MAX:
				if ( __xlIsFloatTypeName(sType0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_max_float(%s, %s);\n", sParam0, sParam1);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_max_int(%s, %s);\n", sParam0, sParam1);
				}
				break;
			case XL_MATH_HELPER_CLAMP:
				if ( __xlIsFloatTypeName(sType0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_clamp_float(%s, %s, %s);\n", sParam0, sParam1, sParam2);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_clamp_int(%s, %s, %s);\n", sParam0, sParam1, sParam2);
				}
				break;
			case XL_MATH_HELPER_FLOOR: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_floor(%s);\n", sParam0); break;
			case XL_MATH_HELPER_CEIL: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_ceil(%s);\n", sParam0); break;
			case XL_MATH_HELPER_ROUND: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_round(%s);\n", sParam0); break;
			case XL_MATH_HELPER_TRUNC: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_trunc(%s);\n", sParam0); break;
			case XL_MATH_HELPER_SQRT:
				if ( __xlIsFloatTypeName(sType0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_sqrt(%s);\n", sParam0);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_sqrt_int(%s);\n", sParam0);
				}
				break;
			case XL_MATH_HELPER_POW:
				if ( __xlIsFloatTypeName(sType0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_pow(%s, %s);\n", sParam0, sParam1);
				} else {
					__xlCgenAppendFormat(&pCtx->buf, "return xl_math_pow_int(%s, %s);\n", sParam0, sParam1);
				}
				break;
			case XL_MATH_HELPER_SIN: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_sin(%s);\n", sParam0); break;
			case XL_MATH_HELPER_COS: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_cos(%s);\n", sParam0); break;
			case XL_MATH_HELPER_TAN: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_tan(%s);\n", sParam0); break;
			case XL_MATH_HELPER_EXP: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_exp(%s);\n", sParam0); break;
			case XL_MATH_HELPER_LOG: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_log(%s);\n", sParam0); break;
			case XL_MATH_HELPER_LOG10: __xlCgenAppendFormat(&pCtx->buf, "return xl_math_log10(%s);\n", sParam0); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	ePackKind = __xlPackHelperKind(pFuncInfo->sName);
	if ( ePackKind != XL_PACK_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( ePackKind ) {
			case XL_PACK_HELPER_WRITE_U8: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_write_u8(%s);\n", sParam0); break;
			case XL_PACK_HELPER_WRITE_U16_LE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_write_u16_le(%s);\n", sParam0); break;
			case XL_PACK_HELPER_WRITE_U16_BE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_write_u16_be(%s);\n", sParam0); break;
			case XL_PACK_HELPER_WRITE_U32_LE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_write_u32_le(%s);\n", sParam0); break;
			case XL_PACK_HELPER_WRITE_U32_BE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_write_u32_be(%s);\n", sParam0); break;
			case XL_PACK_HELPER_READ_U8: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_read_u8(%s, %s);\n", sParam0, sParam1); break;
			case XL_PACK_HELPER_READ_U16_LE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_read_u16_le(%s, %s);\n", sParam0, sParam1); break;
			case XL_PACK_HELPER_READ_U16_BE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_read_u16_be(%s, %s);\n", sParam0, sParam1); break;
			case XL_PACK_HELPER_READ_U32_LE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_read_u32_le(%s, %s);\n", sParam0, sParam1); break;
			case XL_PACK_HELPER_READ_U32_BE: __xlCgenAppendFormat(&pCtx->buf, "return xl_pack_read_u32_be(%s, %s);\n", sParam0, sParam1); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eJsonKind = __xlJsonHelperKind(pFuncInfo->sName);
	if ( eJsonKind != XL_JSON_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eJsonKind ) {
			case XL_JSON_HELPER_ENCODE:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_json_encode(%s);\n", sParam0);
				break;
			case XL_JSON_HELPER_DECODE:
				__xlCgenAppendFormat(&pCtx->buf, "return xl_json_decode(%s);\n", sParam0);
				break;
			default:
				break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eEnvKind = __xlEnvHelperKind(pFuncInfo->sName);
	if ( eEnvKind != XL_ENV_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eEnvKind ) {
			case XL_ENV_HELPER_GET: __xlCgenAppendFormat(&pCtx->buf, "return xl_env_get(%s);\n", sParam0); break;
			case XL_ENV_HELPER_EXISTS: __xlCgenAppendFormat(&pCtx->buf, "return xl_env_exists(%s);\n", sParam0); break;
			case XL_ENV_HELPER_SET: __xlCgenAppendFormat(&pCtx->buf, "return xl_env_set(%s, %s);\n", sParam0, sParam1); break;
			case XL_ENV_HELPER_CWD: __xlCgenAppend(&pCtx->buf, "return xl_env_cwd();\n"); break;
			case XL_ENV_HELPER_TEMP: __xlCgenAppend(&pCtx->buf, "return xl_env_temp();\n"); break;
			case XL_ENV_HELPER_HOME: __xlCgenAppend(&pCtx->buf, "return xl_env_home();\n"); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eDirKind = __xlDirHelperKind(pFuncInfo->sName);
	if ( eDirKind != XL_DIR_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eDirKind ) {
			case XL_DIR_HELPER_CREATE: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_create(%s);\n", sParam0); break;
			case XL_DIR_HELPER_LIST: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_list(%s);\n", sParam0); break;
			case XL_DIR_HELPER_FILES: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_files(%s);\n", sParam0); break;
			case XL_DIR_HELPER_DIRS: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_dirs(%s);\n", sParam0); break;
			case XL_DIR_HELPER_TREE: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_tree(%s);\n", sParam0); break;
			case XL_DIR_HELPER_COPY: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_copy(%s, %s);\n", sParam0, sParam1); break;
			case XL_DIR_HELPER_MOVE: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_move(%s, %s);\n", sParam0, sParam1); break;
			case XL_DIR_HELPER_DELETE: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_delete(%s);\n", sParam0); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	eProcessKind = __xlProcessHelperKind(pFuncInfo->sName);
	if ( eProcessKind != XL_PROCESS_HELPER_NONE ) {
		__xlCgenEmitHelperFuncOpen(pCtx, pFuncInfo, pParams);
		__xlCgenAppendIndent(&pCtx->buf);
		switch ( eProcessKind ) {
			case XL_PROCESS_HELPER_EXEC: __xlCgenAppendFormat(&pCtx->buf, "return xl_process_exec(%s);\n", sParam0); break;
			case XL_PROCESS_HELPER_CAPTURE: __xlCgenAppendFormat(&pCtx->buf, "return xl_process_capture(%s);\n", sParam0); break;
			case XL_PROCESS_HELPER_EXEC_IN: __xlCgenAppendFormat(&pCtx->buf, "return xl_process_exec_in(%s, %s);\n", sParam0, sParam1); break;
			case XL_PROCESS_HELPER_CAPTURE_IN: __xlCgenAppendFormat(&pCtx->buf, "return xl_process_capture_in(%s, %s);\n", sParam0, sParam1); break;
			case XL_PROCESS_HELPER_TERMINAL_SUPPORTED: __xlCgenAppend(&pCtx->buf, "return xl_process_terminal_supported();\n"); break;
			default: break;
		}
		__xlCgenEmitHelperFuncClose(pCtx);
		return 1;
	}

	return 0;
}

static void __xlCgenAwaitExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pInner;
	const XL_CgenFuncInfo* pFuncInfo;
	const XL_AsyncSmFunc* pAsyncFunc;
	const char* sAsyncTypeName;
	const char* sCalleeName;
	const char* sInnerTypeName;
	const char* sTraceName;
	int iProjectedGeneratorState;
	int iAwaitStateId;
	int iResumeStateId;
	int bUseAsyncStateTrace;
	char sFutureValueType[128];

	pFuncInfo = (pCtx != NULL) ? pCtx->pCurrentFuncInfo : NULL;
	pAsyncFunc = ((pFuncInfo != NULL) && pFuncInfo->bAsync && !pFuncInfo->bGenerator) ? pFuncInfo->pAsyncFunc : NULL;
	pInner = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sAsyncTypeName = NULL;
	sCalleeName = __xlCallCalleeName(pInner);
	iProjectedGeneratorState = 0;
	iAwaitStateId = -1;
	iResumeStateId = -1;
	bUseAsyncStateTrace = 0;
	if ( (pFuncInfo != NULL) && pFuncInfo->bGenerator && (pFuncInfo->pHybridFunc != NULL) ) {
		iProjectedGeneratorState = __xlCgenFindHybridProjectedGeneratorStateId(pFuncInfo->pHybridFunc, pNode);
	}
	if ( pAsyncFunc != NULL ) {
		iAwaitStateId = __xlCgenFindAsyncStateId(pAsyncFunc, pNode, "await");
		iResumeStateId = __xlCgenFindAsyncStateId(pAsyncFunc, pNode, "resume");
		bUseAsyncStateTrace = 1;
	}
	if ( __xlResolveAsyncCallType(pCtx, pInner, &sAsyncTypeName) ) {
		if ( iProjectedGeneratorState > 0 ) {
			__xlCgenAppendFormat(&pCtx->buf, "/* hybrid.await->yield state %d */ ", iProjectedGeneratorState);
		}
		if ( bUseAsyncStateTrace ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitStateFunc(sAsyncTypeName));
			__xlCgenAppendCString(&pCtx->buf, sCalleeName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pInner);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenAppendCString(&pCtx->buf, (pFuncInfo->sName != NULL) ? pFuncInfo->sName : "");
			__xlCgenAppendFormat(&pCtx->buf, ", %d, %d)", iAwaitStateId, iResumeStateId);
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitFunc(sAsyncTypeName));
			__xlCgenAppendCString(&pCtx->buf, sCalleeName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pInner);
			__xlCgenAppend(&pCtx->buf, ")");
		}
		return;
	}

	sInnerTypeName = __xlInferExprTypeName(pCtx, pInner);
	if ( __xlExtractFutureValueType(sInnerTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTraceName = "";
		if ( sCalleeName != NULL ) {
			sTraceName = sCalleeName;
		} else if ( (pInner != NULL) && (pInner->sText != NULL) ) {
			sTraceName = pInner->sText;
		}
		if ( iProjectedGeneratorState > 0 ) {
			__xlCgenAppendFormat(&pCtx->buf, "/* hybrid.await->yield state %d */ ", iProjectedGeneratorState);
		}
		if ( bUseAsyncStateTrace ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitStateFunc(sFutureValueType));
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pInner);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenAppendCString(&pCtx->buf, (pFuncInfo->sName != NULL) ? pFuncInfo->sName : "");
			__xlCgenAppendFormat(&pCtx->buf, ", %d, %d)", iAwaitStateId, iResumeStateId);
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitFunc(sFutureValueType));
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pInner);
			__xlCgenAppend(&pCtx->buf, ")");
		}
		return;
	}

	__xlCgenExpr(pCtx, pInner);
}

static void __xlCgenErrorMemberAccess(XL_CgenBuffer* pBuf, const char* sFieldName, const char* sBaseExpr)
{
	if ( sFieldName == NULL ) {
		__xlCgenAppend(pBuf, "NULL");
		return;
	}
	if ( strcmp(sFieldName, "type") == 0 ) {
		__xlCgenAppend(pBuf, "xl_generated_error_type(");
		__xlCgenAppend(pBuf, sBaseExpr);
		__xlCgenAppend(pBuf, ")");
		return;
	}
	if ( strcmp(sFieldName, "message") == 0 ) {
		__xlCgenAppend(pBuf, "xl_generated_error_message(");
		__xlCgenAppend(pBuf, sBaseExpr);
		__xlCgenAppend(pBuf, ")");
		return;
	}
	if ( strcmp(sFieldName, "code") == 0 ) {
		__xlCgenAppend(pBuf, "xl_generated_error_code(");
		__xlCgenAppend(pBuf, sBaseExpr);
		__xlCgenAppend(pBuf, ")");
		return;
	}
	if ( strcmp(sFieldName, "data") == 0 ) {
		__xlCgenAppend(pBuf, "xl_generated_error_data(");
		__xlCgenAppend(pBuf, sBaseExpr);
		__xlCgenAppend(pBuf, ")");
		return;
	}
	if ( strcmp(sFieldName, "stack") == 0 ) {
		__xlCgenAppend(pBuf, "xl_generated_error_stack(");
		__xlCgenAppend(pBuf, sBaseExpr);
		__xlCgenAppend(pBuf, ")");
		return;
	}
	if ( strcmp(sFieldName, "cause") == 0 ) {
		__xlCgenAppend(pBuf, "xl_generated_error_cause(");
		__xlCgenAppend(pBuf, sBaseExpr);
		__xlCgenAppend(pBuf, ")");
		return;
	}

	__xlCgenAppend(pBuf, "NULL");
}

static const char* __xlCgenErrorMemberNullValue(const char* sFieldName)
{
	if ( (sFieldName != NULL) && (strcmp(sFieldName, "code") == 0) ) {
		return "0";
	}
	return "NULL";
}

static void __xlCgenErrorMemberExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pBase;
	const char* sFieldName;

	pBase = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sFieldName = pNode->sText;
	if ( sFieldName == NULL ) {
		__xlCgenAppend(&pCtx->buf, "NULL");
		return;
	}

	if ( pNode->bFlag != 0 ) {
		__xlCgenAppend(&pCtx->buf, "({ XL_GeneratedError* xl_safe_error = ");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "; (xl_safe_error != NULL) ? ");
		__xlCgenErrorMemberAccess(&pCtx->buf, sFieldName, "xl_safe_error");
		__xlCgenAppendFormat(&pCtx->buf, " : %s; })", __xlCgenErrorMemberNullValue(sFieldName));
		return;
	}

	if ( strcmp(sFieldName, "type") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_type(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( strcmp(sFieldName, "message") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_message(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( strcmp(sFieldName, "code") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_code(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( strcmp(sFieldName, "data") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_data(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( strcmp(sFieldName, "stack") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_stack(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}
	if ( strcmp(sFieldName, "cause") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_cause(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "NULL");
}

static void __xlCgenExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	if ( pNode == NULL ) {
		__xlCgenAppend(&pCtx->buf, "0");
		return;
	}

	if ( __xlIsArrayLiteralCall(pNode) ) {
		__xlCgenArrayLiteral(pCtx, pNode);
		return;
	}

	if ( __xlIsListCtorCall(pCtx, pNode) ) {
		__xlCgenListCtor(pCtx, pNode);
		return;
	}

	if ( __xlIsIndexedListLiteralCall(pNode) ) {
		__xlCgenIndexedListCtor(pCtx, pNode);
		return;
	}

	if ( __xlIsSetCtorCall(pCtx, pNode) ) {
		__xlCgenSetCtor(pCtx, pNode);
		return;
	}

	if ( __xlIsDictCtorCall(pCtx, pNode) ) {
		__xlCgenDictCtor(pCtx, pNode);
		return;
	}

	if ( __xlIsTypeCtorCall(pNode) ) {
		__xlCgenTypeCtorCall(pCtx, pNode);
		return;
	}
	if ( __xlIsChannelCtorCall(pNode) ) {
		__xlCgenChannelCtorCall(pCtx, pNode);
		return;
	}
	if ( __xlIsTypeQueryCall(pNode) ) {
		__xlCgenTypeQueryCall(pCtx, pNode);
		return;
	}

	if ( (pNode->eKind == XL_MIR_CALL) && (__xlTaskHelperKind(__xlCallCalleeName(pNode)) != XL_TASK_HELPER_NONE) ) {
		__xlCgenTaskCall(pCtx, pNode);
		return;
	}

	if ( (pNode->eKind == XL_MIR_CALL) && (__xlCorePrintHelperKind(__xlCallCalleeName(pNode)) != XL_CORE_PRINT_NONE) ) {
		__xlCgenCorePrintCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlIoHelperKind(__xlCallCalleeName(pNode)) != XL_IO_HELPER_NONE) ) {
		__xlCgenIoCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlCoreLenHelperKind(__xlCallCalleeName(pNode)) != XL_CORE_LEN_NONE) ) {
		__xlCgenCoreLenCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlContainerMethodBase(pNode) != NULL) ) {
		__xlCgenContainerMethodCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlChannelMethodBase(pNode) != NULL) ) {
		__xlCgenChannelMethodCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlGeneratorMethodBase(pNode) != NULL) ) {
		__xlCgenGeneratorMethodCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlTimeHelperKind(__xlCallCalleeName(pNode)) != XL_TIME_HELPER_NONE) ) {
		__xlCgenTimeCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlPathHelperKind(__xlCallCalleeName(pNode)) != XL_PATH_HELPER_NONE) ) {
		__xlCgenPathCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlFsHelperKind(__xlCallCalleeName(pNode)) != XL_FS_HELPER_NONE) ) {
		__xlCgenFsCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlStrHelperKind(__xlCallCalleeName(pNode)) != XL_STR_HELPER_NONE) ) {
		__xlCgenStrCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlRandHelperKind(__xlCallCalleeName(pNode)) != XL_RAND_HELPER_NONE) ) {
		__xlCgenRandCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlHostHelperKind(__xlCallCalleeName(pNode)) != XL_HOST_HELPER_NONE) ) {
		__xlCgenHostCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlFmtHelperKind(__xlCallCalleeName(pNode)) != XL_FMT_HELPER_NONE) ) {
		__xlCgenFmtCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlDebugHelperKind(__xlCallCalleeName(pNode)) != XL_DEBUG_HELPER_NONE) ) {
		__xlCgenDebugCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlLogHelperKind(__xlCallCalleeName(pNode)) != XL_LOG_HELPER_NONE) ) {
		__xlCgenLogCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlTraceHelperKind(__xlCallCalleeName(pNode)) != XL_TRACE_HELPER_NONE) ) {
		__xlCgenTraceCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlRegexHelperKind(__xlCallCalleeName(pNode)) != XL_REGEX_HELPER_NONE) ) {
		__xlCgenRegexCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlEnvHelperKind(__xlCallCalleeName(pNode)) != XL_ENV_HELPER_NONE) ) {
		__xlCgenEnvCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlDirHelperKind(__xlCallCalleeName(pNode)) != XL_DIR_HELPER_NONE) ) {
		__xlCgenDirCall(pCtx, pNode);
		return;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlProcessHelperKind(__xlCallCalleeName(pNode)) != XL_PROCESS_HELPER_NONE) ) {
		__xlCgenProcessCall(pCtx, pNode);
		return;
	}
	if ( pNode->eKind == XL_MIR_MEMBER ) {
		if ( __xlIsLengthPropertyName(pNode->sText) ) {
			__xlCgenLengthMemberExpr(pCtx, pNode);
			return;
		}
		if ( (pNode->iChildCount != 0) && (pNode->ppChildren[0] != NULL) &&
			(pNode->ppChildren[0]->sTypeName != NULL) &&
			(strcmp(pNode->ppChildren[0]->sTypeName, "type") == 0) &&
			(pNode->sText != NULL) && (strcmp(pNode->sText, "name") == 0) ) {
			__xlCgenExpr(pCtx, pNode->ppChildren[0]);
			return;
		}
		if ( (pNode->iChildCount != 0) && (pNode->ppChildren[0] != NULL) &&
			(pNode->ppChildren[0]->sTypeName != NULL) &&
			(strcmp(pNode->ppChildren[0]->sTypeName, "error") == 0) ) {
			__xlCgenErrorMemberExpr(pCtx, pNode);
			return;
		}
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		__xlCgenAppendFormat(&pCtx->buf, ".%s", (pNode->sText != NULL) ? pNode->sText : "");
		return;
	}

	if ( pNode->eKind == XL_MIR_CALL ) {
		if ( __xlCallHasExpandedMultiReturnArg(pNode) ) {
			__xlCgenCallExprWithExpandedArgs(pCtx, pNode);
			return;
		}
		const XL_MirNode* pFunc;
		const XL_MirNode* pCallee;
		const char* sCalleeType;
		const char* sFuncSymbol;

		pFunc = __xlResolveCallFuncNode(pCtx, pNode);
		pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
		sCalleeType = __xlInferExprTypeName(pCtx, pCallee);
		sFuncSymbol = __xlMirFuncSymbolName(pFunc);
		if ( (pFunc != NULL) && (sFuncSymbol != NULL) && (sFuncSymbol[0] != '\0') ) {
			__xlCgenAppend(&pCtx->buf, sFuncSymbol);
		} else if ( __xlIsFunctionTypeName(sCalleeType) ) {
			__xlCgenFunctionValueCallExpr(pCtx, pNode);
			return;
		} else {
			__xlCgenExpr(pCtx, pCallee);
		}
		__xlCgenAppend(&pCtx->buf, "(");
		__xlCgenCallArgs(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( __xlIsExpandedMultiReturnArg(pNode) ) {
		__xlCgenIndexExpr(pCtx, pNode, "array<var>");
		return;
	}

	if ( pNode->eKind == XL_MIR_YIELD ) {
		__xlCgenYieldExpr(pCtx, pNode);
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) &&
		((pNode->sText == NULL) || (pNode->sText[0] == '\0')) &&
		(pNode->ppChildren[0] != NULL) &&
		(__xlTypeHasBaseName(__xlInferExprTypeName(pCtx, pNode->ppChildren[0]), "array") ||
			__xlTypeHasBaseName(__xlInferExprTypeName(pCtx, pNode->ppChildren[0]), "list")) ) {
		__xlCgenIndexExpr(pCtx, pNode, __xlInferExprTypeName(pCtx, pNode->ppChildren[0]));
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) &&
		((pNode->sText == NULL) || (pNode->sText[0] == '\0')) &&
		(pNode->ppChildren[0] != NULL) &&
		__xlTypeHasBaseName(__xlInferExprTypeName(pCtx, pNode->ppChildren[0]), "dict") ) {
		__xlCgenDictIndex(pCtx, pNode);
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		const char* sLeftTypeName;
		const char* sRightTypeName;

		sLeftTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[0]);
		sRightTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[1]);
		if ( strcmp(pNode->sText, "**") == 0 ) {
			__xlCgenPowerCall(pCtx, pNode->ppChildren[0], pNode->ppChildren[1]);
			return;
		}
		if ( strcmp(pNode->sText, "**=") == 0 ) {
			__xlCgenPowerAssign(pCtx, pNode->ppChildren[0], pNode->ppChildren[1]);
			return;
		}
		if ( strcmp(pNode->sText, "?=") == 0 ) {
			__xlCgenNullAssign(pCtx, pNode->ppChildren[0], pNode->ppChildren[1]);
			return;
		}
		if ( strcmp(pNode->sText, "~=") == 0 ) {
			__xlCgenApproxCompare(pCtx, pNode->ppChildren[0], pNode->ppChildren[1]);
			return;
		}
		if ( strcmp(pNode->sText, "??") == 0 ) {
			__xlCgenNullCoalesce(pCtx, pNode->ppChildren[0], pNode->ppChildren[1]);
			return;
		}
		if ( (strcmp(pNode->sText, "=") == 0) &&
			(pNode->ppChildren[0] != NULL) &&
			(pNode->ppChildren[0]->iChildCount == 2) &&
			((pNode->ppChildren[0]->sText == NULL) || (pNode->ppChildren[0]->sText[0] == '\0')) ) {
			const char* sAssignBaseTypeName;

			sAssignBaseTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[0]->ppChildren[0]);
			if ( __xlTypeHasBaseName(sAssignBaseTypeName, "array") ||
				__xlTypeHasBaseName(sAssignBaseTypeName, "list") ||
				__xlTypeHasBaseName(sAssignBaseTypeName, "dict") ) {
				__xlCgenIndexAssign(pCtx, pNode->ppChildren[0], pNode->ppChildren[1]);
				return;
			}
		}
		if ( __xlIsVarTypeName(sLeftTypeName) || __xlIsVarTypeName(sRightTypeName) ) {
			if ( (strcmp(pNode->sText, "=") == 0) && (pNode->iChildCount == 2) ) {
				__xlCgenAppend(&pCtx->buf, "(");
				__xlCgenExpr(pCtx, pNode->ppChildren[0]);
				__xlCgenAppend(&pCtx->buf, " = ");
				__xlCgenExprAsType(pCtx, pNode->ppChildren[1], sLeftTypeName);
				__xlCgenAppend(&pCtx->buf, ")");
				return;
			}
			if ( (strcmp(pNode->sText, "==") == 0) || (strcmp(pNode->sText, "!=") == 0) ||
				(strcmp(pNode->sText, ">") == 0) || (strcmp(pNode->sText, ">=") == 0) ||
				(strcmp(pNode->sText, "<") == 0) || (strcmp(pNode->sText, "<=") == 0) ||
				(strcmp(pNode->sText, "~=") == 0) ) {
				__xlCgenVarCompare(pCtx, pNode->ppChildren[0], pNode->ppChildren[1], pNode->sText);
				return;
			}
		}
		if ( (strcmp(pNode->sText, "in") == 0) &&
			__xlTypeHasBaseName(__xlInferExprTypeName(pCtx, pNode->ppChildren[1]), "set") ) {
			__xlCgenSetContains(pCtx, pNode->ppChildren[0], pNode->ppChildren[1]);
			return;
		}
		if ( (strcmp(pNode->sText, "+") == 0) &&
			(__xlIsTextLikeType(sLeftTypeName) || __xlIsTextLikeType(sRightTypeName)) ) {
			__xlCgenAppend(&pCtx->buf, "xl_str_concat(");
			__xlCgenTextExpr(pCtx, pNode->ppChildren[0]);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTextExpr(pCtx, pNode->ppChildren[1]);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		}
		if ( ((strcmp(pNode->sText, "==") == 0) || (strcmp(pNode->sText, "!=") == 0)) &&
			(__xlIsTextLikeType(sLeftTypeName) || __xlIsTextLikeType(sRightTypeName)) ) {
			if ( strcmp(pNode->sText, "!=") == 0 ) {
				__xlCgenAppend(&pCtx->buf, "(!xl_generated_text_equal(");
			} else {
				__xlCgenAppend(&pCtx->buf, "xl_generated_text_equal(");
			}
			__xlCgenTextExpr(pCtx, pNode->ppChildren[0]);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTextExpr(pCtx, pNode->ppChildren[1]);
			__xlCgenAppend(&pCtx->buf, (strcmp(pNode->sText, "!=") == 0) ? "))" : ")");
			return;
		}
		__xlCgenAppend(&pCtx->buf, "(");
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		__xlCgenAppendFormat(&pCtx->buf, " %s ", __xlCLogicalBinaryOp(pNode->sText));
		__xlCgenExpr(pCtx, pNode->ppChildren[1]);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 3) && (pNode->sText != NULL) && (strcmp(pNode->sText, "?:") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "(");
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		__xlCgenAppend(&pCtx->buf, " ? ");
		__xlCgenExpr(pCtx, pNode->ppChildren[1]);
		__xlCgenAppend(&pCtx->buf, " : ");
		__xlCgenExpr(pCtx, pNode->ppChildren[2]);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 1) && (pNode->sText != NULL) && (strcmp(pNode->sText, "await") == 0) ) {
		__xlCgenAwaitExpr(pCtx, pNode);
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 1) &&
		((pNode->sText == NULL) || (pNode->sText[0] == '\0')) ) {
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		return;
	}

	if ( ((pNode->sTypeName != NULL) && (strcmp(pNode->sTypeName, "null") == 0)) ||
		((pNode->sText != NULL) && (strcmp(pNode->sText, "null") == 0)) ) {
		__xlCgenAppend(&pCtx->buf, "NULL");
		return;
	}
	if ( (pNode->iChildCount == 0) && (pNode->sTypeName != NULL) && (strcmp(pNode->sTypeName, "string") == 0) &&
		((pNode->sText == NULL) || (pNode->sText[0] == '\0')) ) {
		__xlCgenAppendCString(&pCtx->buf, "");
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 1) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		__xlCgenAppendFormat(&pCtx->buf, "(%s", __xlCUnaryOp(pNode->sText));
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (pNode->iChildCount == 0) && __xlIsFunctionTypeName(pNode->sTypeName) &&
		(pNode->sAbiName != NULL) && (pNode->sAbiName[0] != '\0') ) {
		__xlCgenAppendFormat(&pCtx->buf, "(void*)%s", pNode->sAbiName);
		return;
	}
	if ( (pNode->iChildCount == 0) && (pNode->sTypeName != NULL) && (strcmp(pNode->sTypeName, "type") == 0) &&
		(pNode->iSymbolIndex < 0) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		__xlCgenAppendCString(&pCtx->buf, pNode->sText);
		return;
	}

	if ( (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		__xlCgenAppend(&pCtx->buf, pNode->sText);
		return;
	}

	__xlCgenAppend(&pCtx->buf, "0");
}

static void __xlCgenTraceEventStmt(XL_CgenContext* pCtx, const char* sEvent, const char* sFuncName)
{
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_event(");
	__xlCgenAppendCString(&pCtx->buf, sEvent);
	__xlCgenAppend(&pCtx->buf, ", ");
	__xlCgenAppendCString(&pCtx->buf, (sFuncName != NULL) ? sFuncName : "");
	__xlCgenAppend(&pCtx->buf, ");\n");
}

static int __xlIsRangeCall(const XL_MirNode* pNode)
{
	const XL_MirNode* pCallee;

	if ( (pNode == NULL) || (pNode->eKind != XL_MIR_CALL) || (pNode->iChildCount < 2) ) {
		return 0;
	}

	pCallee = pNode->ppChildren[0];
	return (pCallee != NULL) &&
		(pCallee->sText != NULL) &&
		(strcmp(pCallee->sText, "range") == 0);
}

static int __xlIsGeneratorCall(
	const XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode** ppFuncNode,
	const char** psValueTypeName,
	const char** psFuncName
)
{
	return __xlResolveGeneratorSourceInfo(pCtx, pNode, ppFuncNode, psValueTypeName, psFuncName, 0);
}

static void __xlCgenForInRange(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo)
{
	const XL_MirNode* pTargets;
	const XL_MirNode* pIterable;
	const XL_MirNode* pArgs;
	const XL_MirNode* pBody;
	const XL_MirNode* pTarget0;
	const XL_MirNode* pTarget1;
	const char* sTarget0;
	const char* sTarget1;
	char sStartName[64];
	char sStopName[64];
	char sStepName[64];
	char sIndexName[64];
	char sValueName[64];
	char sBreakLabel[64];
	char sContinueLabel[64];

	pTargets = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pIterable = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pBody = (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL;
	pArgs = ((pIterable != NULL) && (pIterable->iChildCount > 1)) ? pIterable->ppChildren[1] : NULL;
	pTarget0 = ((pTargets != NULL) && (pTargets->iChildCount != 0)) ? pTargets->ppChildren[0] : NULL;
	pTarget1 = ((pTargets != NULL) && (pTargets->iChildCount > 1)) ? pTargets->ppChildren[1] : NULL;
	sTarget0 = ((pTarget0 != NULL) && (pTarget0->sText != NULL) && (pTarget0->sText[0] != '\0')) ? pTarget0->sText : "item";
	sTarget1 = ((pTarget1 != NULL) && (pTarget1->sText != NULL) && (pTarget1->sText[0] != '\0')) ? pTarget1->sText : "value";

	snprintf(sStartName, sizeof(sStartName), "xl_for_start_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sStopName, sizeof(sStopName), "xl_for_stop_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sStepName, sizeof(sStepName), "xl_for_step_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sIndexName, sizeof(sIndexName), "xl_for_index_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sValueName, sizeof(sValueName), "xl_for_value_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sBreakLabel, sizeof(sBreakLabel), "xl_for_break_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sContinueLabel, sizeof(sContinueLabel), "xl_for_continue_%d_%d", pNode->iLine, pNode->iColumn);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", sStartName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", sStopName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 1;\n", sStepName);

	if ( (pArgs != NULL) && (pArgs->iChildCount == 1) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = ", sStopName);
		__xlCgenExpr(pCtx, pArgs->ppChildren[0]);
		__xlCgenAppend(&pCtx->buf, ";\n");
	} else if ( (pArgs != NULL) && (pArgs->iChildCount == 2) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = ", sStartName);
		__xlCgenExpr(pCtx, pArgs->ppChildren[0]);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = ", sStopName);
		__xlCgenExpr(pCtx, pArgs->ppChildren[1]);
		__xlCgenAppend(&pCtx->buf, ";\n");
	} else if ( (pArgs != NULL) && (pArgs->iChildCount >= 3) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = ", sStartName);
		__xlCgenExpr(pCtx, pArgs->ppChildren[0]);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = ", sStopName);
		__xlCgenExpr(pCtx, pArgs->ppChildren[1]);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = ", sStepName);
		__xlCgenExpr(pCtx, pArgs->ppChildren[2]);
		__xlCgenAppend(&pCtx->buf, ";\n");
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s != 0 )\n", sStepName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"for ( int %s = 0, %s = %s; (%s > 0) ? (%s < %s) : (%s > %s); %s += %s, ++%s )\n",
		sIndexName,
		sValueName,
		sStartName,
		sStepName,
		sValueName,
		sStopName,
		sValueName,
		sStopName,
		sValueName,
		sStepName,
		sIndexName
	);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	if ( (pTargets != NULL) && (pTargets->iChildCount > 1) ) {
		__xlCgenAppendFormat(&pCtx->buf, "int %s = %s;\n", sTarget0, sIndexName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "int %s = %s;\n", sTarget1, sValueName);
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "int %s = %s;\n", sTarget0, sValueName);
	}

	__xlCgenPushLoopFrame(pCtx, sBreakLabel, sContinueLabel);
	if ( pBody != NULL ) {
		if ( (pBody->eKind == XL_MIR_BLOCK) && (pBody->iChildCount != 0) ) {
			size_t i;

			for ( i = 0; i < pBody->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pBody->ppChildren[i], pFuncInfo);
			}
		} else {
			__xlCgenStmt(pCtx, pBody, pFuncInfo);
		}
	}
	__xlCgenPopLoopFrame(pCtx);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sContinueLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sBreakLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenForInGenerator(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo)
{
	const XL_MirNode* pTargets;
	const XL_MirNode* pIterable;
	const XL_MirNode* pBody;
	const XL_MirNode* pGeneratorFunc;
	const XL_MirNode* pTarget0;
	const XL_MirNode* pTarget1;
	const char* sTarget0;
	const char* sTarget1;
	const char* sTarget0Type;
	const char* sTarget1Type;
	const char* sValueTypeName;
	const char* sFuncName;
	int bAbstractGenerator;
	char sGeneratorName[64];
	char sNextFlagName[64];
	char sIndexName[64];
	char sCurrentValueName[64];
	char sBreakLabel[64];
	char sContinueLabel[64];
	char sGeneratorTypeName[256];
	char sGeneratorNextFuncName[256];

	pTargets = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pIterable = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pBody = (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL;
	pTarget0 = ((pTargets != NULL) && (pTargets->iChildCount != 0)) ? pTargets->ppChildren[0] : NULL;
	pTarget1 = ((pTargets != NULL) && (pTargets->iChildCount > 1)) ? pTargets->ppChildren[1] : NULL;
	sTarget0 = ((pTarget0 != NULL) && (pTarget0->sText != NULL) && (pTarget0->sText[0] != '\0')) ? pTarget0->sText : "item";
	sTarget1 = ((pTarget1 != NULL) && (pTarget1->sText != NULL) && (pTarget1->sText[0] != '\0')) ? pTarget1->sText : "value";
	sTarget0Type = (pTarget0 != NULL) ? __xlInferExprTypeName(pCtx, pTarget0) : "int";
	sTarget1Type = (pTarget1 != NULL) ? __xlInferExprTypeName(pCtx, pTarget1) : "int";
	pGeneratorFunc = NULL;
	bAbstractGenerator = 0;

	if ( !__xlIsGeneratorCall(pCtx, pIterable, &pGeneratorFunc, &sValueTypeName, &sFuncName) ) {
		const char* sIterableType = __xlInferExprTypeName(pCtx, pIterable);

		if ( (sIterableType == NULL) || (strcmp(sIterableType, "generator") != 0) ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "/* generator iterable is reserved for a later stage */\n");
			return;
		}
		bAbstractGenerator = 1;
		sValueTypeName = (pTarget1 != NULL) ? sTarget1Type : sTarget0Type;
		sFuncName = "generator";
	}

	snprintf(sGeneratorName, sizeof(sGeneratorName), "xl_gen_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sNextFlagName, sizeof(sNextFlagName), "xl_gen_has_next_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sIndexName, sizeof(sIndexName), "xl_gen_index_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sCurrentValueName, sizeof(sCurrentValueName), "xl_gen_value_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sBreakLabel, sizeof(sBreakLabel), "xl_for_break_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sContinueLabel, sizeof(sContinueLabel), "xl_for_continue_%d_%d", pNode->iLine, pNode->iColumn);
	if ( !bAbstractGenerator ) {
		__xlCgenGeneratorTypeNameForFunc(sGeneratorTypeName, sizeof(sGeneratorTypeName), pGeneratorFunc);
		__xlCgenGeneratorNextFuncNameForFunc(sGeneratorNextFuncName, sizeof(sGeneratorNextFuncName), pGeneratorFunc);
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	if ( bAbstractGenerator ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName("generator"), sGeneratorName);
		if ( !__xlCgenEmitAbstractGeneratorExpr(pCtx, pIterable, 1) ) {
			__xlCgenExprAsType(pCtx, pIterable, "generator");
		}
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", sGeneratorTypeName, sGeneratorName);
		__xlCgenExpr(pCtx, pIterable);
	}
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAppendIndent(&pCtx->buf);
	if ( bAbstractGenerator ) {
		__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = NULL;\n", sCurrentValueName);
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "%s %s;\n", __xlGeneratorItemCType(sValueTypeName), sCurrentValueName);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", sNextFlagName);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "for ( int %s = 0; ; ++%s )\n", sIndexName, sIndexName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	if ( bAbstractGenerator ) {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s = xl_generated_generator_step(&%s, NULL, &%s);\n",
			sNextFlagName,
			sGeneratorName,
			sCurrentValueName
		);
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "%s = %s(", sNextFlagName, sGeneratorNextFuncName);
		__xlCgenAppendCString(&pCtx->buf, sFuncName);
		__xlCgenAppendFormat(&pCtx->buf, ", &%s, &%s, NULL);\n", sGeneratorName, sCurrentValueName);
	}
	__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( !%s )\n", sNextFlagName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", sBreakLabel);

	__xlCgenAppendIndent(&pCtx->buf);
	if ( (pTargets != NULL) && (pTargets->iChildCount > 1) ) {
		__xlCgenAppendFormat(&pCtx->buf, "int %s = %s;\n", sTarget0, sIndexName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget1Type), sTarget1);
		if ( bAbstractGenerator ) {
			__xlCgenValueExprAsType(pCtx, sCurrentValueName, sTarget1Type);
		} else {
			__xlCgenAppend(&pCtx->buf, sCurrentValueName);
		}
		__xlCgenAppend(&pCtx->buf, ";\n");
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget0Type), sTarget0);
		if ( bAbstractGenerator ) {
			__xlCgenValueExprAsType(pCtx, sCurrentValueName, sTarget0Type);
		} else {
			__xlCgenAppend(&pCtx->buf, sCurrentValueName);
		}
		__xlCgenAppend(&pCtx->buf, ";\n");
	}

	__xlCgenPushLoopFrame(pCtx, sBreakLabel, sContinueLabel);
	if ( pBody != NULL ) {
		if ( (pBody->eKind == XL_MIR_BLOCK) && (pBody->iChildCount != 0) ) {
			size_t i;

			for ( i = 0; i < pBody->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pBody->ppChildren[i], pFuncInfo);
			}
		} else {
			__xlCgenStmt(pCtx, pBody, pFuncInfo);
		}
	}
	__xlCgenPopLoopFrame(pCtx);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sContinueLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	if ( bAbstractGenerator ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s != NULL ) { xvoUnref(%s); %s = NULL; }\n", sCurrentValueName, sCurrentValueName, sCurrentValueName);
	}

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sBreakLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	if ( bAbstractGenerator ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s != NULL ) { xvoUnref(%s); %s = NULL; }\n", sCurrentValueName, sCurrentValueName, sCurrentValueName);
	}

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenForInContainer(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo)
{
	const XL_MirNode* pTargets;
	const XL_MirNode* pIterable;
	const XL_MirNode* pBody;
	const XL_MirNode* pTarget0;
	const XL_MirNode* pTarget1;
	const char* sTarget0;
	const char* sTarget1;
	const char* sTarget0Type;
	const char* sTarget1Type;
	const char* sIterableType;
	char sContainerName[64];
	char sCountName[64];
	char sIndexName[64];
	char sNodeName[64];
	char sBreakLabel[64];
	char sContinueLabel[64];

	pTargets = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pIterable = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pBody = (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL;
	pTarget0 = ((pTargets != NULL) && (pTargets->iChildCount != 0)) ? pTargets->ppChildren[0] : NULL;
	pTarget1 = ((pTargets != NULL) && (pTargets->iChildCount > 1)) ? pTargets->ppChildren[1] : NULL;
	sTarget0 = ((pTarget0 != NULL) && (pTarget0->sText != NULL) && (pTarget0->sText[0] != '\0')) ? pTarget0->sText : "item";
	sTarget1 = ((pTarget1 != NULL) && (pTarget1->sText != NULL) && (pTarget1->sText[0] != '\0')) ? pTarget1->sText : "value";
	sTarget0Type = (pTarget0 != NULL) ? __xlInferExprTypeName(pCtx, pTarget0) : "int";
	sTarget1Type = (pTarget1 != NULL) ? __xlInferExprTypeName(pCtx, pTarget1) : "int";
	sIterableType = __xlInferExprTypeName(pCtx, pIterable);

	snprintf(sContainerName, sizeof(sContainerName), "xl_iter_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sCountName, sizeof(sCountName), "xl_iter_count_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sIndexName, sizeof(sIndexName), "xl_iter_index_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sNodeName, sizeof(sNodeName), "xl_iter_node_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sBreakLabel, sizeof(sBreakLabel), "xl_for_break_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sContinueLabel, sizeof(sContinueLabel), "xl_for_continue_%d_%d", pNode->iLine, pNode->iColumn);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = ", sContainerName);
	__xlCgenExpr(pCtx, pIterable);
	__xlCgenAppend(&pCtx->buf, ";\n");

	if ( __xlTypeHasBaseName(sIterableType, "array") || __xlTypeHasBaseName(sIterableType, "list") ) {
		const char* sGetterName = __xlTypeHasBaseName(sIterableType, "list") ? "xvoListGetValue" : "xvoArrayGetValue";
		const char* sCountGetterName = __xlTypeHasBaseName(sIterableType, "list") ? "xvoListItemCount" : "xvoArrayItemCount";
		const char* sIndexCast = __xlTypeHasBaseName(sIterableType, "list") ? "int64" : "uint32";
		char sItemExpr[256];

		snprintf(sItemExpr, sizeof(sItemExpr), "%s((xvalue)(%s), (%s)(%s))", sGetterName, sContainerName, sIndexCast, sIndexName);

		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "uint32_t %s = (uint32_t)%s((xvalue)(%s));\n", sCountName, sCountGetterName, sContainerName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "for ( uint32_t %s = 0; %s < %s; ++%s )\n", sIndexName, sIndexName, sCountName, sIndexName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;

		__xlCgenAppendIndent(&pCtx->buf);
		if ( (pTargets != NULL) && (pTargets->iChildCount > 1) ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = ((%s)%s);\n", __xlCTypeName(sTarget0Type), sTarget0, __xlCTypeName(sTarget0Type), sIndexName);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget1Type), sTarget1);
			__xlCgenValueExprAsType(pCtx, sItemExpr, sTarget1Type);
			__xlCgenAppend(&pCtx->buf, ";\n");
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget0Type), sTarget0);
			__xlCgenValueExprAsType(pCtx, sItemExpr, sTarget0Type);
			__xlCgenAppend(&pCtx->buf, ";\n");
		}
	} else if ( __xlTypeHasBaseName(sIterableType, "set") ) {
		char sSetValueExpr[160];

		snprintf(sSetValueExpr, sizeof(sSetValueExpr), "%s->Value", sNodeName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xavltree %s_tree = xvoGetColl((xvalue)(%s));\n", sContainerName, sContainerName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", sIndexName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s_tree != NULL ) xrtAVLTreeIterBegin(%s_tree);\n", sContainerName, sContainerName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"for ( Coll_Key* %s = (%s_tree != NULL) ? (Coll_Key*)xrtAVLTreeIterNext(%s_tree) : NULL; %s != NULL; %s = (Coll_Key*)xrtAVLTreeIterNext(%s_tree), ++%s )\n",
			sNodeName,
			sContainerName,
			sContainerName,
			sNodeName,
			sNodeName,
			sContainerName,
			sIndexName
		);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;

		__xlCgenAppendIndent(&pCtx->buf);
		if ( (pTargets != NULL) && (pTargets->iChildCount > 1) ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = %s;\n", __xlCTypeName(sTarget0Type), sTarget0, sIndexName);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget1Type), sTarget1);
			__xlCgenValueExprAsType(pCtx, sSetValueExpr, sTarget1Type);
			__xlCgenAppend(&pCtx->buf, ";\n");
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget0Type), sTarget0);
			__xlCgenValueExprAsType(pCtx, sSetValueExpr, sTarget0Type);
			__xlCgenAppend(&pCtx->buf, ";\n");
		}
	} else if ( __xlTypeHasBaseName(sIterableType, "dict") ) {
		char sDictValueExpr[160];

		snprintf(sDictValueExpr, sizeof(sDictValueExpr), "(*(xvalue*)(&%s[1]))", sNodeName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xdict %s_table = xvoGetTable((xvalue)(%s));\n", sContainerName, sContainerName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", sIndexName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s_table != NULL ) xrtAVLTB_IterBegin((xavltbase)%s_table);\n", sContainerName, sContainerName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"for ( Dict_Key* %s = (%s_table != NULL) ? (Dict_Key*)xrtAVLTB_IterNext((xavltbase)%s_table) : NULL; %s != NULL; %s = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)%s_table), ++%s )\n",
			sNodeName,
			sContainerName,
			sContainerName,
			sNodeName,
			sNodeName,
			sContainerName,
			sIndexName
		);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;

		__xlCgenAppendIndent(&pCtx->buf);
		if ( (pTargets != NULL) && (pTargets->iChildCount > 1) ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = (const char*)%s->Key;\n", __xlCTypeName(sTarget0Type), sTarget0, sNodeName);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget1Type), sTarget1);
			__xlCgenValueExprAsType(pCtx, sDictValueExpr, sTarget1Type);
			__xlCgenAppend(&pCtx->buf, ";\n");
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTarget0Type), sTarget0);
			__xlCgenValueExprAsType(pCtx, sDictValueExpr, sTarget0Type);
			__xlCgenAppend(&pCtx->buf, ";\n");
		}
	} else {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "/* for-in iterable is reserved for a later stage */\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		return;
	}

	__xlCgenPushLoopFrame(pCtx, sBreakLabel, sContinueLabel);
	if ( pBody != NULL ) {
		if ( (pBody->eKind == XL_MIR_BLOCK) && (pBody->iChildCount != 0) ) {
			size_t i;

			for ( i = 0; i < pBody->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pBody->ppChildren[i], pFuncInfo);
			}
		} else {
			__xlCgenStmt(pCtx, pBody, pFuncInfo);
		}
	}
	__xlCgenPopLoopFrame(pCtx);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sContinueLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sBreakLabel);
	if ( __xlTypeHasBaseName(sIterableType, "set") ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s_tree != NULL ) xrtAVLTreeIterEnd(%s_tree);\n", sContainerName, sContainerName);
	} else if ( __xlTypeHasBaseName(sIterableType, "dict") ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s_table != NULL ) xrtAVLTB_IterEnd((xavltbase)%s_table);\n", sContainerName, sContainerName);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenWhile(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pBody;
	int bDoWhile;
	char sCondName[64];
	char sBreakLabel[64];
	char sContinueLabel[64];

	pCond = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pBody = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	bDoWhile = pNode->bFlag;

	snprintf(sCondName, sizeof(sCondName), "xl_while_cond_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sBreakLabel, sizeof(sBreakLabel), "xl_while_break_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sContinueLabel, sizeof(sContinueLabel), "xl_while_continue_%d_%d", pNode->iLine, pNode->iColumn);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", sCondName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "while ( 1 )\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	if ( !bDoWhile ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = ", sCondName);
		__xlCgenExpr(pCtx, pCond);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenPropagatePendingException(pCtx, pFuncInfo);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( !%s )\n", sCondName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", sBreakLabel);
	}

	__xlCgenPushLoopFrame(pCtx, sBreakLabel, sContinueLabel);
	if ( pBody != NULL ) {
		if ( (pBody->eKind == XL_MIR_BLOCK) && (pBody->iChildCount != 0) ) {
			size_t i;

			for ( i = 0; i < pBody->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pBody->ppChildren[i], pFuncInfo);
			}
		} else {
			__xlCgenStmt(pCtx, pBody, pFuncInfo);
		}
	}
	__xlCgenPopLoopFrame(pCtx);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sContinueLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = ", sCondName);
	__xlCgenExpr(pCtx, pCond);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( !%s )\n", sCondName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", sBreakLabel);

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sBreakLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenAsyncDefaultReturn(XL_CgenContext* pCtx, const XL_CgenFuncInfo* pFuncInfo)
{
	const char* sAsyncValueType;
	const char* sDefaultReturn;

	sAsyncValueType = __xlFutureValueTypeName((pFuncInfo != NULL) ? pFuncInfo->sTypeName : NULL);
	__xlCgenAppendIndent(&pCtx->buf);
	if ( (sAsyncValueType == NULL) || (strcmp(sAsyncValueType, "void") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "return %s();\n", __xlFutureReadyFunc("void"));
		return;
	}

	sDefaultReturn = __xlCDefaultReturnValue(sAsyncValueType);
	__xlCgenAppendFormat(&pCtx->buf, "return %s(", __xlFutureReadyFunc(pFuncInfo->sTypeName));
	if ( sDefaultReturn != NULL ) {
		__xlCgenAppend(&pCtx->buf, sDefaultReturn);
	}
	__xlCgenAppend(&pCtx->buf, ");\n");
}

static void __xlCgenStmt(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	const char* sDefaultReturn;
	int iNestedYieldCount;
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	if ( (pCtx != NULL) && (pFuncInfo != NULL) && pFuncInfo->bGenerator && (pNode->eKind != XL_MIR_YIELD) ) {
		iNestedYieldCount = __xlStmtNestedYieldExprCount(pNode);
		if ( iNestedYieldCount == 1 ) {
			const XL_MirNode* pYieldExpr;

			pYieldExpr = __xlMirFindFirstKind(pNode, XL_MIR_YIELD);
			pCtx->iGeneratorExprResumeState = __xlCgenResolveGeneratorResumeState(
				pFuncInfo,
				pYieldExpr,
				0,
				0,
				0
			);
		}
		if ( pCtx->iGeneratorExprResumeState > 0 ) {
			if ( pFuncInfo->pHybridFunc != NULL ) {
				const XL_HybridSmPoint* pHybridPoint = __xlCgenFindHybridPointByGeneratorStateId(pFuncInfo->pHybridFunc, pCtx->iGeneratorExprResumeState);

				if ( pHybridPoint != NULL ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppendFormat(
						&pCtx->buf,
						"/* hybrid.group %d awaitCount=%d */\n",
						pHybridPoint->iProjectionGroupId,
						__xlCgenCountHybridAwaitsInGroup(pFuncInfo->pHybridFunc, pHybridPoint->iProjectionGroupId)
					);
				}
			}
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "xl_gen_resume_%d:\n", pCtx->iGeneratorExprResumeState);
		}
	}

	switch ( pNode->eKind ) {
		case XL_MIR_BLOCK:
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pNode->ppChildren[i], pFuncInfo);
			}
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			return;
		case XL_MIR_BIND:
		{
			int bEmitAbstractGeneratorBind;

			bEmitAbstractGeneratorBind = 0;
			__xlCgenAppendIndent(&pCtx->buf);
			if ( (pFuncInfo != NULL) && pFuncInfo->bGenerator ) {
				__xlCgenAppendFormat(&pCtx->buf, "%s = ", (pNode->sText != NULL) ? pNode->sText : "tmp");
			} else if ( (pNode->sTypeName != NULL) && (strcmp(pNode->sTypeName, "generator") == 0) ) {
				const XL_MirNode* pGeneratorFunc;
				char sGeneratorTypeName[256];

				pGeneratorFunc = NULL;
				if ( __xlResolveGeneratorSourceInfo(
					pCtx,
					(pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL,
					&pGeneratorFunc,
					NULL,
					NULL,
					0
				) ) {
					__xlCgenGeneratorTypeNameForFunc(sGeneratorTypeName, sizeof(sGeneratorTypeName), pGeneratorFunc);
					__xlCgenAppendFormat(
						&pCtx->buf,
						"%s %s = ",
						sGeneratorTypeName,
						(pNode->sText != NULL) ? pNode->sText : "tmp"
					);
				} else {
					__xlCgenAppendFormat(
						&pCtx->buf,
						"%s %s = ",
						__xlCTypeName("generator"),
						(pNode->sText != NULL) ? pNode->sText : "tmp"
					);
					bEmitAbstractGeneratorBind = 1;
				}
			} else {
				__xlCgenAppendFormat(
					&pCtx->buf,
					"%s %s = ",
					__xlCTypeName(((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int"),
					(pNode->sText != NULL) ? pNode->sText : "tmp"
				);
			}
			if ( bEmitAbstractGeneratorBind &&
				__xlCgenEmitAbstractGeneratorExpr(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL, 1) ) {
				/* abstract generator value emitted inline */
			} else {
				__xlCgenExprAsType(
					pCtx,
					(pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL,
					((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int"
				);
			}
			__xlCgenAppend(&pCtx->buf, ";\n");
			__xlCgenPropagatePendingException(pCtx, pFuncInfo);
			return;
		}
		case XL_MIR_BRANCH:
			pCond = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
			pThenBlock = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
			pElseBlock = (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "int xl_if_cond_%d_%d = ", pNode->iLine, pNode->iColumn);
			__xlCgenExpr(pCtx, pCond);
			__xlCgenAppend(&pCtx->buf, ";\n");
			__xlCgenPropagatePendingException(pCtx, pFuncInfo);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( ");
			__xlCgenAppendFormat(&pCtx->buf, "xl_if_cond_%d_%d", pNode->iLine, pNode->iColumn);
			__xlCgenAppend(&pCtx->buf, " )\n");
			__xlCgenStmt(pCtx, pThenBlock, pFuncInfo);
			if ( pElseBlock != NULL ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "else\n");
				__xlCgenStmt(pCtx, pElseBlock, pFuncInfo);
			}
			return;
		case XL_MIR_TRY:
			__xlCgenTryStmt(pCtx, pNode, pFuncInfo);
			return;
		case XL_MIR_FORIN:
		{
			const XL_MirNode* pIterable = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
			const char* sIterableType = __xlInferExprTypeName(pCtx, pIterable);

			if ( __xlIsRangeCall(pIterable) ) {
				__xlCgenForInRange(pCtx, pNode, pFuncInfo);
			} else if ( __xlIsGeneratorCall(pCtx, pIterable, NULL, NULL, NULL) ||
				((sIterableType != NULL) && (strcmp(sIterableType, "generator") == 0)) ) {
				__xlCgenForInGenerator(pCtx, pNode, pFuncInfo);
			} else {
				__xlCgenForInContainer(pCtx, pNode, pFuncInfo);
			}
			return;
		}
		case XL_MIR_WHILE:
			__xlCgenWhile(pCtx, pNode, pFuncInfo);
			return;
		case XL_MIR_RETURN:
		{
			const XL_CgenFinallyFrame* pFinallyFrame;
			const XL_CgenTryFrame* pTryFrame;

			pFinallyFrame = __xlCgenCurrentFinallyFrame(pCtx);
			pTryFrame = __xlCgenCurrentTryFrame(pCtx);
			if ( (pFuncInfo != NULL) && pFuncInfo->bGenerator ) {
				if ( pNode->iChildCount != 0 ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "(void)(");
					__xlCgenExpr(pCtx, pNode->ppChildren[0]);
					__xlCgenAppend(&pCtx->buf, ");\n");
				}
				if ( pFinallyFrame != NULL ) {
					__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 1, 0);
					return;
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pFuncInfo->sGeneratorDoneLabel);
				return;
			}
			if ( (pFuncInfo != NULL) && pFuncInfo->bAsync ) {
				const char* sAsyncValueType = __xlFutureValueTypeName(pFuncInfo->sTypeName);
				const XL_MirNode* pAwaitInner = NULL;
				const char* sAwaitExprType = NULL;
				const char* sTraceName = NULL;
				char sAwaitValueType[128];
				int iAwaitStateId = -1;
				int iResumeStateId = -1;

				if ( (sAsyncValueType == NULL) || (strcmp(sAsyncValueType, "void") == 0) ) {
					if ( pFinallyFrame != NULL ) {
						__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
						__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 1, 0);
						return;
					}
					__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppendFormat(&pCtx->buf, "return %s();\n", __xlFutureReadyFunc("void"));
				} else {
					if ( (pFinallyFrame == NULL) && (pTryFrame == NULL) && (pNode->iChildCount != 0) ) {
						pAwaitInner = __xlAwaitInnerExpr(pNode->ppChildren[0]);
						sAwaitExprType = __xlInferExprTypeName(pCtx, pAwaitInner);
						if ( __xlExtractFutureValueType(sAwaitExprType, sAwaitValueType, sizeof(sAwaitValueType)) &&
							(strcmp(sAwaitValueType, sAsyncValueType) == 0) ) {
							sTraceName = __xlCallCalleeName(pAwaitInner);
							if ( (sTraceName == NULL) || (sTraceName[0] == '\0') ) {
								sTraceName = (pAwaitInner != NULL && pAwaitInner->sText != NULL) ? pAwaitInner->sText : "await";
							}
							if ( pFuncInfo->pAsyncFunc != NULL ) {
								iAwaitStateId = __xlCgenFindAsyncStateId(pFuncInfo->pAsyncFunc, pNode->ppChildren[0], "await");
								iResumeStateId = __xlCgenFindAsyncStateId(pFuncInfo->pAsyncFunc, pNode->ppChildren[0], "resume");
							}
							__xlCgenAppendIndent(&pCtx->buf);
							__xlCgenAppendFormat(
								&pCtx->buf,
								"%s xl_async_future_%d_%d = ",
								__xlCTypeName(sAwaitExprType),
								pNode->iLine,
								pNode->iColumn
							);
							__xlCgenExprAsType(pCtx, pAwaitInner, sAwaitExprType);
							__xlCgenAppend(&pCtx->buf, ";\n");
							__xlCgenPropagatePendingException(pCtx, pFuncInfo);
							__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
							__xlCgenAppendIndent(&pCtx->buf);
							__xlCgenAppendFormat(&pCtx->buf, "return %s(", __xlFutureForwardStateFunc(pFuncInfo->sTypeName));
							__xlCgenAppendCString(&pCtx->buf, sTraceName);
							__xlCgenAppendFormat(&pCtx->buf, ", xl_async_future_%d_%d, ", pNode->iLine, pNode->iColumn);
							__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
							__xlCgenAppendFormat(&pCtx->buf, ", %d, %d);\n", iAwaitStateId, iResumeStateId);
							return;
						}
					}
					if ( pNode->iChildCount != 0 ) {
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(
							&pCtx->buf,
							"%s xl_async_ret_%d_%d = ",
							__xlCTypeName(sAsyncValueType),
							pNode->iLine,
							pNode->iColumn
						);
						__xlCgenExprAsType(pCtx, pNode->ppChildren[0], sAsyncValueType);
						__xlCgenAppend(&pCtx->buf, ";\n");
						__xlCgenPropagatePendingException(pCtx, pFuncInfo);
						if ( pFinallyFrame != NULL ) {
							if ( (pFuncInfo->sEscapeValueName != NULL) && (pFuncInfo->sEscapeValueName[0] != '\0') ) {
								__xlCgenAppendIndent(&pCtx->buf);
								__xlCgenAppendFormat(&pCtx->buf, "%s = xl_async_ret_%d_%d;\n", pFuncInfo->sEscapeValueName, pNode->iLine, pNode->iColumn);
							}
							__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
							__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 1, 0);
							return;
						}
						__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_async_ret_%d_%d);\n", __xlFutureReadyFunc(pFuncInfo->sTypeName), pNode->iLine, pNode->iColumn);
					} else {
						sDefaultReturn = __xlCDefaultReturnValue(sAsyncValueType);
						if ( pFinallyFrame != NULL ) {
							if ( (sDefaultReturn != NULL) && (pFuncInfo->sEscapeValueName != NULL) && (pFuncInfo->sEscapeValueName[0] != '\0') ) {
								__xlCgenAppendIndent(&pCtx->buf);
								__xlCgenAppendFormat(&pCtx->buf, "%s = %s;\n", pFuncInfo->sEscapeValueName, sDefaultReturn);
							}
							__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
							__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 1, 0);
							return;
						}
						__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(&pCtx->buf, "return %s(", __xlFutureReadyFunc(pFuncInfo->sTypeName));
						if ( sDefaultReturn != NULL ) {
							__xlCgenAppend(&pCtx->buf, sDefaultReturn);
						}
						__xlCgenAppend(&pCtx->buf, ");\n");
					}
				}
				return;
			}

			if ( pNode->iChildCount != 0 ) {
				__xlCgenAppend(&pCtx->buf, " ");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"%s xl_ret_%d_%d = ",
					(pFuncInfo != NULL) ? __xlCgenFuncReturnCTypeName(pFuncInfo) : __xlCTypeName(pNode->ppChildren[0]->sTypeName),
					pNode->iLine,
					pNode->iColumn
				);
				if ( (pFuncInfo != NULL) &&
					(pFuncInfo->sTypeName != NULL) &&
					(strcmp(pFuncInfo->sTypeName, "generator") == 0) &&
					(strcmp(__xlCgenFuncReturnCTypeName(pFuncInfo), __xlCTypeName("generator")) == 0) &&
					__xlCgenEmitAbstractGeneratorExpr(pCtx, pNode->ppChildren[0], 1) ) {
					/* abstract generator return emitted inline */
				} else {
					__xlCgenExprAsType(
						pCtx,
						pNode->ppChildren[0],
						(pFuncInfo != NULL) ? pFuncInfo->sTypeName : pNode->ppChildren[0]->sTypeName
					);
				}
				__xlCgenAppend(&pCtx->buf, ";\n");
				__xlCgenPropagatePendingException(pCtx, pFuncInfo);
				if ( pFinallyFrame != NULL ) {
					if ( (pFuncInfo != NULL) && (pFuncInfo->sEscapeValueName != NULL) && (pFuncInfo->sEscapeValueName[0] != '\0') ) {
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(&pCtx->buf, "%s = xl_ret_%d_%d;\n", pFuncInfo->sEscapeValueName, pNode->iLine, pNode->iColumn);
					}
					__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 1, 0);
					return;
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "return xl_ret_%d_%d;\n", pNode->iLine, pNode->iColumn);
			} else {
				if ( pFinallyFrame != NULL ) {
					if ( (pFuncInfo != NULL) &&
						(pFuncInfo->sTypeName != NULL) &&
						(strcmp(pFuncInfo->sTypeName, "void") != 0) &&
						(pFuncInfo->sEscapeValueName != NULL) &&
						(pFuncInfo->sEscapeValueName[0] != '\0') ) {
						sDefaultReturn = __xlCgenFuncReturnDefaultValue(pFuncInfo);
						if ( sDefaultReturn != NULL ) {
							__xlCgenAppendIndent(&pCtx->buf);
							__xlCgenAppendFormat(&pCtx->buf, "%s = %s;\n", pFuncInfo->sEscapeValueName, sDefaultReturn);
						}
					}
					__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 1, 0);
					return;
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return;\n");
			}
			return;
		}
		case XL_MIR_BREAK:
		{
			const XL_CgenLoopFrame* pFrame;
			const XL_CgenFinallyFrame* pFinallyFrame;

			pFrame = __xlCgenFindLoopFrame(pCtx, (int)pNode->iIntValue);
			pFinallyFrame = __xlCgenCurrentFinallyFrame(pCtx);
			if ( pFinallyFrame != NULL ) {
				__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 2, (int)pNode->iIntValue);
				return;
			}
			__xlCgenAppendIndent(&pCtx->buf);
			if ( (pFrame != NULL) && (pFrame->sBreakLabel[0] != '\0') ) {
				__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pFrame->sBreakLabel);
			} else {
				__xlCgenAppend(&pCtx->buf, "break;\n");
			}
			return;
		}
		case XL_MIR_CONTINUE:
		{
			const XL_CgenLoopFrame* pFrame;
			const XL_CgenFinallyFrame* pFinallyFrame;

			pFrame = __xlCgenFindLoopFrame(pCtx, (int)pNode->iIntValue);
			pFinallyFrame = __xlCgenCurrentFinallyFrame(pCtx);
			if ( pFinallyFrame != NULL ) {
				__xlCgenEmitEscapeToCurrentFinally(pCtx, pFuncInfo, 3, (int)pNode->iIntValue);
				return;
			}
			__xlCgenAppendIndent(&pCtx->buf);
			if ( (pFrame != NULL) && (pFrame->sContinueLabel[0] != '\0') ) {
				__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pFrame->sContinueLabel);
			} else {
				__xlCgenAppend(&pCtx->buf, "continue;\n");
			}
			return;
		}
		case XL_MIR_THROW:
			if ( (pFuncInfo != NULL) && pFuncInfo->bAsync ) {
				__xlCgenTraceEventStmt(pCtx, "throw", pFuncInfo->sName);
			}
			__xlCgenThrowRaise(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL);
			__xlCgenPropagateRaisedException(pCtx, pFuncInfo);
			return;
		case XL_MIR_ASSERT:
		{
			const XL_MirNode* pAssertCond;
			const XL_MirNode* pAssertMsg;

			pAssertCond = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
			pAssertMsg = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "int xl_assert_cond_%d_%d = ", pNode->iLine, pNode->iColumn);
			__xlCgenExpr(pCtx, pAssertCond);
			__xlCgenAppend(&pCtx->buf, ";\n");
			__xlCgenPropagatePendingException(pCtx, pFuncInfo);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( !xl_assert_cond_%d_%d )\n", pNode->iLine, pNode->iColumn);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			if ( pAssertMsg != NULL ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "const char* xl_assert_msg_%d_%d = ", pNode->iLine, pNode->iColumn);
				__xlCgenExprAsType(pCtx, pAssertMsg, "string");
				__xlCgenAppend(&pCtx->buf, ";\n");
				__xlCgenPropagatePendingException(pCtx, pFuncInfo);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"xl_generated_raise_error(Error(xl_assert_msg_%d_%d, 0, \"\", \"\", NULL));\n",
					pNode->iLine,
					pNode->iColumn
				);
			} else {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_raise_error(Error(\"assert failed\", 0, \"\", \"\", NULL));\n");
			}
			__xlCgenPropagateRaisedException(pCtx, pFuncInfo);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			return;
		}
		case XL_MIR_YIELD:
			if ( (pFuncInfo != NULL) && pFuncInfo->bGenerator ) {
				int iResumeState;

				iResumeState = 0;
				iResumeState = __xlCgenResolveGeneratorResumeState(
					pFuncInfo,
					pNode,
					iResumeState,
					0,
					1
				);
				if ( pFuncInfo->pHybridFunc != NULL ) {
					const XL_HybridSmPoint* pHybridPoint = __xlCgenFindHybridPointByGeneratorStateId(pFuncInfo->pHybridFunc, iResumeState);

					if ( pHybridPoint != NULL ) {
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(
							&pCtx->buf,
							"/* hybrid.group %d awaitCount=%d */\n",
							pHybridPoint->iProjectionGroupId,
							__xlCgenCountHybridAwaitsInGroup(pFuncInfo->pHybridFunc, pHybridPoint->iProjectionGroupId)
						);
					}
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"*(%s) = ",
					(pFuncInfo->sGeneratorValueOutName != NULL) ? pFuncInfo->sGeneratorValueOutName : "pValue"
				);
				if ( pNode->iChildCount != 0 ) {
					__xlCgenExpr(pCtx, pNode->ppChildren[0]);
				} else {
					__xlCgenAppend(&pCtx->buf, __xlCDefaultReturnValue(pFuncInfo->sTypeName));
				}
				__xlCgenAppend(&pCtx->buf, ";\n");
				__xlCgenPropagatePendingException(pCtx, pFuncInfo);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"xl_generated_generator_executor_suspend(&%s->executor, %d, ",
					(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
					iResumeState
				);
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppend(&pCtx->buf, ");\n");
				__xlCgenAppendHybridGroupTraceEvent(pCtx, pFuncInfo, iResumeState, "group");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"next\", ");
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppend(&pCtx->buf, ");\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return 1;\n");
				if ( pFuncInfo->pHybridFunc != NULL ) {
					const XL_HybridSmPoint* pHybridPoint = __xlCgenFindHybridPointByGeneratorStateId(pFuncInfo->pHybridFunc, iResumeState);

					if ( pHybridPoint != NULL ) {
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(
							&pCtx->buf,
							"/* hybrid.group %d awaitCount=%d */\n",
							pHybridPoint->iProjectionGroupId,
							__xlCgenCountHybridAwaitsInGroup(pFuncInfo->pHybridFunc, pHybridPoint->iProjectionGroupId)
						);
					}
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "xl_gen_resume_%d:\n", iResumeState);
				__xlCgenAppendHybridGroupTraceEvent(pCtx, pFuncInfo, iResumeState, "resume_group");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"(void)xl_generated_generator_executor_resume(&%s->executor, %d);\n",
					(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen"
					,
					iResumeState
				);
				return;
			}
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "/* yield is reserved for a later stage */\n");
			return;
		case XL_MIR_IMPORT:
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "/* import ");
			__xlCgenExpr(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL);
			__xlCgenAppend(&pCtx->buf, " */\n");
			return;
		case XL_MIR_EXPORT:
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "/* export ");
			__xlCgenExpr(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL);
			__xlCgenAppend(&pCtx->buf, " */\n");
			return;
		default:
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenExpr(pCtx, pNode);
			__xlCgenAppend(&pCtx->buf, ";\n");
			__xlCgenPropagatePendingException(pCtx, pFuncInfo);
			return;
	}
}

static void __xlCgenAsyncLinearPendingErrorCheck(XL_CgenContext* pCtx, const char* sStateName, const char* sFallback)
{
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "if ( xl_generated_async_has_pending_error() )\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "(void)xl_generated_promise_reject_current(%s->pPromise, ", sStateName);
	__xlCgenAppendCString(&pCtx->buf, sFallback);
	__xlCgenAppend(&pCtx->buf, ");\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s->pPromise != NULL ) {\n", sStateName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s->pPromise);\n", sStateName);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "return;\n");
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenAsyncLinearRejectCurrentAndCleanup(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	const char* sStateName,
	const char* sFallback
)
{
	if ( (pCtx == NULL) || (pCandidate == NULL) || (sStateName == NULL) ) {
		return;
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "(void)xl_generated_promise_reject_current(%s->pPromise, ", sStateName);
	__xlCgenAppendCString(&pCtx->buf, (sFallback != NULL) ? sFallback : "await failed");
	__xlCgenAppend(&pCtx->buf, ");\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s->pPromise != NULL ) {\n", sStateName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s->pPromise);\n", sStateName);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAsyncLinearStateCleanup(pCtx, pParams, pCandidate, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "return;\n");
}

static void __xlCgenAsyncLinearRejectResultAndCleanup(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	const char* sStateName,
	const char* sResultName,
	const char* sFallback
)
{
	if ( (pCtx == NULL) || (pCandidate == NULL) || (sStateName == NULL) ) {
		return;
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "(void)xl_generated_promise_reject_result(%s->pPromise, %s, ", sStateName, (sResultName != NULL) ? sResultName : "NULL");
	__xlCgenAppendCString(&pCtx->buf, (sFallback != NULL) ? sFallback : "await failed");
	__xlCgenAppend(&pCtx->buf, ");\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s->pPromise != NULL ) {\n", sStateName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s->pPromise);\n", sStateName);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAsyncLinearStateCleanup(pCtx, pParams, pCandidate, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "return;\n");
}

static void __xlCgenAsyncLinearCatchValueDeclFromCurrent(XL_CgenContext* pCtx, const XL_MirNode* pCatchName)
{
	const char* sTypeName;
	const char* sVarName;

	if ( pCtx == NULL ) {
		return;
	}

	sTypeName = ((pCatchName != NULL) && (pCatchName->sTypeName != NULL) && (pCatchName->sTypeName[0] != '\0')) ? pCatchName->sTypeName : "error";
	sVarName = ((pCatchName != NULL) && (pCatchName->sText != NULL) && (pCatchName->sText[0] != '\0')) ? pCatchName->sText : "xl_async_catch_error";
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = xl_generated_capture_exception_error();\n", __xlCTypeName(sTypeName), sVarName);
}

static void __xlCgenAsyncLinearCatchValueDeclFromResult(XL_CgenContext* pCtx, const XL_MirNode* pCatchName, const char* sResultName)
{
	const char* sTypeName;
	const char* sVarName;
	const char* sName;

	if ( pCtx == NULL ) {
		return;
	}

	sTypeName = ((pCatchName != NULL) && (pCatchName->sTypeName != NULL) && (pCatchName->sTypeName[0] != '\0')) ? pCatchName->sTypeName : "error";
	sVarName = ((pCatchName != NULL) && (pCatchName->sText != NULL) && (pCatchName->sText[0] != '\0')) ? pCatchName->sText : "xl_async_catch_error";
	sName = (sResultName != NULL) ? sResultName : "NULL";
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"%s %s = xl_generated_error_from_text(((%s != NULL) && (%s->sError != NULL) && (%s->sError[0] != '\\0')) ? (const char*)%s->sError : \"await failed\");\n",
		__xlCTypeName(sTypeName),
		sVarName,
		sName,
		sName,
		sName,
		sName
	);
}

static void __xlCgenAsyncLinearRestorePendingFromResult(XL_CgenContext* pCtx, const char* sResultName)
{
	const char* sName;

	if ( pCtx == NULL ) {
		return;
	}

	sName = (sResultName != NULL) ? sResultName : "NULL";
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"xl_generated_raise_error(xl_generated_error_from_text(((%s != NULL) && (%s->sError != NULL) && (%s->sError[0] != '\\0')) ? (const char*)%s->sError : \"await failed\"));\n",
		sName,
		sName,
		sName,
		sName
	);
}

static void __xlCgenAsyncLinearHandleFailureFromResultForStmt(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const char* sResultName
)
{
	const XL_MirNode* pBranchBlock;
	const XL_MirNode* pInnerStmt;
	size_t iInnerStmtIndex;
	size_t i;

	if ( (pCtx == NULL) || (pStmt == NULL) ) {
		return;
	}
	if ( pStmt->eKind == XL_MIR_TRY ) {
		__xlCgenAsyncLinearTryHandleFailureFromResult(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, sResultName);
		return;
	}
	if ( pStmt->eKind == XL_MIR_BRANCH ) {
		pBranchBlock = (pAwaitNodeAlt != NULL)
			? __xlSelectAsyncLinearBranchDualAwaitBlock(pCtx, pStmt, NULL, NULL, NULL)
			: __xlSelectAsyncLinearBranchAwaitBlock(pCtx, pStmt, pAwaitNode);
		if ( pBranchBlock != NULL ) {
			__xlCgenAsyncLinearHandleFailureFromResultForStmt(pCtx, pBranchBlock, pAwaitNode, pAwaitNodeAlt, sResultName);
			return;
		}
	}
	if ( (pStmt->eKind == XL_MIR_BLOCK) &&
		__xlFindAsyncLinearBlockAwaitStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, &pInnerStmt, &iInnerStmtIndex) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearHandleFailureFromResultForStmt(pCtx, pInnerStmt, pAwaitNode, pAwaitNodeAlt, sResultName);
		if ( (iInnerStmtIndex + 1) < pStmt->iChildCount ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( !xl_generated_async_has_pending_error() )\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			for ( i = iInnerStmtIndex + 1; i < pStmt->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pStmt->ppChildren[i], NULL);
			}
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		}
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		return;
	}
	__xlCgenAsyncLinearRestorePendingFromResult(pCtx, sResultName);
}

static void __xlCgenAsyncLinearTryPrelude(
	XL_CgenContext* pCtx,
	const XL_MirNode* pTryStmt,
	const XL_MirNode* pAwaitNode,
	int bResumePath,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	size_t iStepIndex,
	const char* sStateName
)
{
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pInnerStmt;
	size_t iInnerStmtIndex;
	size_t i;

	if ( (pCtx == NULL) || (pTryStmt == NULL) || (pTryStmt->eKind != XL_MIR_TRY) ) {
		return;
	}

	pTryBlock = (pTryStmt->iChildCount != 0) ? pTryStmt->ppChildren[0] : NULL;
	if ( !__xlFindAsyncLinearTryAwaitStmt(pCtx, pTryBlock, pAwaitNode, &pInnerStmt, &iInnerStmtIndex) ) {
		return;
	}

	for ( i = 0; i < iInnerStmtIndex; ++i ) {
		__xlCgenStmt(pCtx, pTryBlock->ppChildren[i], NULL);
		if ( bResumePath ) {
			__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear try prelude failed");
		}
	}
	__xlCgenAsyncLinearStmtPrelude(pCtx, pInnerStmt, pAwaitNode, bResumePath, pParams, pCandidate, iStepIndex, sStateName);
	(void)pParams;
	(void)pCandidate;
	(void)iStepIndex;
}

static void __xlCgenAsyncLinearBlockPrelude(
	XL_CgenContext* pCtx,
	const XL_MirNode* pBlockStmt,
	const XL_MirNode* pAwaitNode,
	int bResumePath,
	const char* sStateName
)
{
	const XL_MirNode* pInnerStmt;
	size_t iInnerStmtIndex;
	size_t i;

	if ( (pCtx == NULL) || (pBlockStmt == NULL) || (pBlockStmt->eKind != XL_MIR_BLOCK) ) {
		return;
	}
	if ( !__xlFindAsyncLinearBlockAwaitStmt(pCtx, pBlockStmt, pAwaitNode, &pInnerStmt, &iInnerStmtIndex) ) {
		return;
	}

	for ( i = 0; i < iInnerStmtIndex; ++i ) {
		__xlCgenStmt(pCtx, pBlockStmt->ppChildren[i], NULL);
		if ( bResumePath ) {
			__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear block prelude failed");
		}
	}
	__xlCgenAsyncLinearStmtPrelude(pCtx, pInnerStmt, pAwaitNode, bResumePath, NULL, NULL, 0, sStateName);
}

static void __xlCgenAsyncLinearBranchSelectorFieldName(char* sOut, size_t iOutSize, size_t iStepIndex)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	snprintf(sOut, iOutSize, "xl_async_branch_sel_%zu", iStepIndex);
}

static void __xlCgenAsyncLinearBranchSelectorLocalName(char* sOut, size_t iOutSize, size_t iStepIndex)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	snprintf(sOut, iOutSize, "xl_async_branch_sel_local_%zu", iStepIndex);
}

static int __xlCgenAsyncLinearEmitDynamicBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const char* sAwaitExprType,
	const char* sFutureValueName,
	size_t iStepIndex,
	const char* sStateName,
	int bResumePath
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pAwaitBlock;
	const XL_MirNode* pSyncBlock;
	int bAwaitOnTrue;
	char sSelectorField[64];
	char sSelectorLocal[64];

	if ( (pCtx == NULL) || (pStmt == NULL) || (sAwaitExprType == NULL) || (sFutureValueName == NULL) ) {
		return 0;
	}
	if ( !__xlAnalyzeAsyncLinearDynamicBranchAwait(pCtx, pStmt, pAwaitNode, &pAwaitBlock, &pSyncBlock, &bAwaitOnTrue) ) {
		return 0;
	}

	pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	__xlCgenAsyncLinearBranchSelectorFieldName(sSelectorField, sizeof(sSelectorField), iStepIndex);
	__xlCgenAsyncLinearBranchSelectorLocalName(sSelectorLocal, sizeof(sSelectorLocal), iStepIndex);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = ", sSelectorLocal);
	__xlCgenExprAsType(pCtx, pCond, "bool");
	__xlCgenAppend(&pCtx->buf, " ? 1 : 0;\n");
	if ( bResumePath ) {
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear branch failed");
	}
	if ( sStateName != NULL ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s->%s = %s;\n", sStateName, sSelectorField, sSelectorLocal);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s;\n", __xlFutureTypeName(sAwaitExprType), sFutureValueName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s%s )\n", bAwaitOnTrue ? "" : "!", sSelectorLocal);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAsyncLinearBlockPrelude(pCtx, pAwaitBlock, pAwaitNode, bResumePath, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = ", sFutureValueName);
	__xlCgenExprAsType(pCtx, __xlAwaitInnerExpr(pAwaitNode), sAwaitExprType);
	__xlCgenAppend(&pCtx->buf, ";\n");
	if ( bResumePath ) {
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear await failed");
	}
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "else\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	if ( pSyncBlock != NULL ) {
		__xlCgenStmt(pCtx, pSyncBlock, pCtx->pCurrentFuncInfo);
	}
	if ( bResumePath ) {
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear branch failed");
	}
	__xlCgenAppendIndent(&pCtx->buf);
	if ( strcmp(sAwaitExprType, "void") == 0 ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s = %s();\n", sFutureValueName, __xlFutureReadyFunc(sAwaitExprType));
	} else {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s = %s(%s);\n",
			sFutureValueName,
			__xlFutureReadyFunc(sAwaitExprType),
			__xlCDefaultReturnValue(sAwaitExprType)
		);
	}
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	return 1;
}

static int __xlCgenAsyncLinearEmitDualBranchAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pPrimaryAwaitNode,
	const XL_MirNode* pAltAwaitNode,
	const char* sAwaitExprType,
	const char* sFutureValueName,
	size_t iStepIndex,
	const char* sStateName,
	int bResumePath
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	char sSelectorField[64];
	char sSelectorLocal[64];

	if ( (pCtx == NULL) || (pStmt == NULL) || (pPrimaryAwaitNode == NULL) || (pAltAwaitNode == NULL) ||
		(sAwaitExprType == NULL) || (sFutureValueName == NULL) ) {
		return 0;
	}
	if ( !__xlAnalyzeAsyncLinearDualBranchAwait(pCtx, pStmt, &pThenBlock, &pElseBlock, NULL, NULL) ) {
		return 0;
	}

	pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	__xlCgenAsyncLinearBranchSelectorFieldName(sSelectorField, sizeof(sSelectorField), iStepIndex);
	__xlCgenAsyncLinearBranchSelectorLocalName(sSelectorLocal, sizeof(sSelectorLocal), iStepIndex);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = ", sSelectorLocal);
	__xlCgenExprAsType(pCtx, pCond, "bool");
	__xlCgenAppend(&pCtx->buf, " ? 1 : 0;\n");
	if ( bResumePath ) {
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear branch failed");
	}
	if ( sStateName != NULL ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s->%s = %s;\n", sStateName, sSelectorField, sSelectorLocal);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s;\n", __xlFutureTypeName(sAwaitExprType), sFutureValueName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s )\n", sSelectorLocal);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAsyncLinearBlockPrelude(pCtx, pThenBlock, pPrimaryAwaitNode, bResumePath, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = ", sFutureValueName);
	__xlCgenExprAsType(pCtx, __xlAwaitInnerExpr(pPrimaryAwaitNode), sAwaitExprType);
	__xlCgenAppend(&pCtx->buf, ";\n");
	if ( bResumePath ) {
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear await failed");
	}
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "else\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAsyncLinearBlockPrelude(pCtx, pElseBlock, pAltAwaitNode, bResumePath, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = ", sFutureValueName);
	__xlCgenExprAsType(pCtx, __xlAwaitInnerExpr(pAltAwaitNode), sAwaitExprType);
	__xlCgenAppend(&pCtx->buf, ";\n");
	if ( bResumePath ) {
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear await failed");
	}
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	return 1;
}

static int __xlCgenAsyncLinearEmitDynamicAwaitStep(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const char* sAwaitExprType,
	const char* sFutureValueName,
	size_t iStepIndex,
	const char* sStateName,
	int bResumePath
)
{
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pInnerStmt;
	size_t iInnerStmtIndex;
	size_t i;

	if ( (pCtx == NULL) || (pStmt == NULL) || (pAwaitNode == NULL) || (sAwaitExprType == NULL) || (sFutureValueName == NULL) ) {
		return 0;
	}
	if ( pStmt->eKind == XL_MIR_BRANCH ) {
		const XL_MirNode* pConstDualBlock = __xlSelectAsyncLinearBranchDualAwaitBlock(pCtx, pStmt, NULL, NULL, NULL);
		const XL_MirNode* pConstAwaitBlock = __xlSelectAsyncLinearBranchAwaitBlock(pCtx, pStmt, pAwaitNode);

		if ( pConstDualBlock != NULL ) {
			return __xlCgenAsyncLinearEmitDynamicAwaitStep(
				pCtx,
				pConstDualBlock,
				pAwaitNode,
				pAwaitNodeAlt,
				sAwaitExprType,
				sFutureValueName,
				iStepIndex,
				sStateName,
				bResumePath
			);
		}
		if ( pConstAwaitBlock != NULL ) {
			return __xlCgenAsyncLinearEmitDynamicAwaitStep(
				pCtx,
				pConstAwaitBlock,
				pAwaitNode,
				pAwaitNodeAlt,
				sAwaitExprType,
				sFutureValueName,
				iStepIndex,
				sStateName,
				bResumePath
			);
		}
		if ( pAwaitNodeAlt != NULL ) {
			return __xlCgenAsyncLinearEmitDualBranchAwait(
				pCtx,
				pStmt,
				pAwaitNode,
				pAwaitNodeAlt,
				sAwaitExprType,
				sFutureValueName,
				iStepIndex,
				sStateName,
				bResumePath
			);
		}
		return __xlCgenAsyncLinearEmitDynamicBranchAwait(
			pCtx,
			pStmt,
			pAwaitNode,
			sAwaitExprType,
			sFutureValueName,
			iStepIndex,
			sStateName,
			bResumePath
		);
	}
	if ( pStmt->eKind == XL_MIR_BLOCK ) {
		if ( !__xlFindAsyncLinearBlockAwaitStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, &pInnerStmt, &iInnerStmtIndex) ) {
			return 0;
		}
		for ( i = 0; i < iInnerStmtIndex; ++i ) {
			__xlCgenStmt(pCtx, pStmt->ppChildren[i], NULL);
			if ( bResumePath ) {
				__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear block prelude failed");
			}
		}
		return __xlCgenAsyncLinearEmitDynamicAwaitStep(
			pCtx,
			pInnerStmt,
			pAwaitNode,
			pAwaitNodeAlt,
			sAwaitExprType,
			sFutureValueName,
			iStepIndex,
			sStateName,
			bResumePath
		);
	}
	if ( pStmt->eKind == XL_MIR_TRY ) {
		pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		if ( !__xlFindAsyncLinearTryAwaitStmtEx(pCtx, pTryBlock, pAwaitNode, pAwaitNodeAlt, &pInnerStmt, &iInnerStmtIndex) ) {
			return 0;
		}
		for ( i = 0; i < iInnerStmtIndex; ++i ) {
			__xlCgenStmt(pCtx, pTryBlock->ppChildren[i], NULL);
			if ( bResumePath ) {
				__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear try prelude failed");
			}
		}
		return __xlCgenAsyncLinearEmitDynamicAwaitStep(
			pCtx,
			pInnerStmt,
			pAwaitNode,
			pAwaitNodeAlt,
			sAwaitExprType,
			sFutureValueName,
			iStepIndex,
			sStateName,
			bResumePath
		);
	}
	return 0;
}

static void __xlCgenAsyncLinearStmtPrelude(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	int bResumePath,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	size_t iStepIndex,
	const char* sStateName
)
{
	const XL_MirNode* pBranchAwaitBlock;

	if ( (pCtx == NULL) || (pStmt == NULL) ) {
		return;
	}
	if ( pStmt->eKind == XL_MIR_TRY ) {
		__xlCgenAsyncLinearTryPrelude(pCtx, pStmt, pAwaitNode, bResumePath, pParams, pCandidate, iStepIndex, sStateName);
		return;
	}
	if ( pStmt->eKind == XL_MIR_BRANCH ) {
		pBranchAwaitBlock = __xlSelectAsyncLinearBranchDualAwaitBlock(pCtx, pStmt, NULL, NULL, NULL);
		if ( pBranchAwaitBlock != NULL ) {
			__xlCgenAsyncLinearBlockPrelude(pCtx, pBranchAwaitBlock, pAwaitNode, bResumePath, sStateName);
			return;
		}
		pBranchAwaitBlock = __xlSelectAsyncLinearBranchAwaitBlock(pCtx, pStmt, pAwaitNode);
		if ( pBranchAwaitBlock != NULL ) {
			__xlCgenAsyncLinearBlockPrelude(pCtx, pBranchAwaitBlock, pAwaitNode, bResumePath, sStateName);
		}
		return;
	}
	if ( pStmt->eKind == XL_MIR_BLOCK ) {
		__xlCgenAsyncLinearBlockPrelude(pCtx, pStmt, pAwaitNode, bResumePath, sStateName);
	}
}

static void __xlCgenAsyncLinearTryHandleFailureFromResult(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pAwaitNode,
	const XL_MirNode* pAwaitNodeAlt,
	const char* sResultName
)
{
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pInnerStmt;
	const XL_MirNode* pCatchName;
	const XL_MirNode* pCatchBlock;
	const XL_MirNode* pFinallyBlock;
	size_t iInnerStmtIndex;
	size_t i;
	int bHasCatch;

	if ( (pCtx == NULL) || (pStmt == NULL) || (pStmt->eKind != XL_MIR_TRY) ) {
		return;
	}

	pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
	pCatchName = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
	pCatchBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
	pFinallyBlock = (pStmt->iChildCount > 3) ? pStmt->ppChildren[3] : NULL;
	bHasCatch = ((pStmt->bFlag & 1) != 0) && (pCatchBlock != NULL);
	if ( !__xlFindAsyncLinearTryAwaitStmtEx(pCtx, pTryBlock, pAwaitNode, pAwaitNodeAlt, &pInnerStmt, &iInnerStmtIndex) ) {
		return;
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAsyncLinearHandleFailureFromResultForStmt(pCtx, pInnerStmt, pAwaitNode, pAwaitNodeAlt, sResultName);

	if ( (pTryBlock != NULL) && (pTryBlock->eKind == XL_MIR_BLOCK) && ((iInnerStmtIndex + 1) < pTryBlock->iChildCount) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( !xl_generated_async_has_pending_error() )\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		for ( i = iInnerStmtIndex + 1; i < pTryBlock->iChildCount; ++i ) {
			__xlCgenStmt(pCtx, pTryBlock->ppChildren[i], NULL);
		}
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
	}

	if ( bHasCatch ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_async_has_pending_error() ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearCatchValueDeclFromCurrent(pCtx, pCatchName);
		__xlCgenStmt(pCtx, pCatchBlock, NULL);
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
	}

	if ( pFinallyBlock != NULL ) {
		__xlCgenSetCurrentFinallyBody(pCtx, 1);
		__xlCgenStmt(pCtx, pFinallyBlock, NULL);
		__xlCgenSetCurrentFinallyBody(pCtx, 0);
	}

	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenAsyncLinearAwaitValueDecl(XL_CgenContext* pCtx, const XL_MirNode* pBind)
{
	const char* sTypeName;
	const char* sVarName;
	const char* sCTypeName;

	sTypeName = ((pBind != NULL) && (pBind->sTypeName != NULL) && (pBind->sTypeName[0] != '\0')) ? pBind->sTypeName : "int";
	sVarName = ((pBind != NULL) && (pBind->sText != NULL)) ? pBind->sText : "value";
	sCTypeName = __xlCTypeName(sTypeName);

	if ( __xlIsIntegerTypeName(sTypeName) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s = (%s)(intptr_t)((pIn != NULL) ? pIn->pValue : NULL);\n",
			sCTypeName,
			sVarName,
			sCTypeName
		);
		return;
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s = (((intptr_t)((pIn != NULL) ? pIn->pValue : NULL)) != 0) ? 1 : 0;\n",
			sCTypeName,
			sVarName
		);
		return;
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "double* xl_async_result_ptr_%s = (double*)((pIn != NULL) ? pIn->pValue : NULL);\n", sVarName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s = (xl_async_result_ptr_%s != NULL) ? (%s)(*xl_async_result_ptr_%s) : (%s)0;\n",
			sCTypeName,
			sVarName,
			sVarName,
			sCTypeName,
			sVarName,
			sCTypeName
		);
		return;
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = (const char*)(((pIn != NULL) && (pIn->pValue != NULL)) ? pIn->pValue : \"\");\n", sCTypeName, sVarName);
		return;
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = (%s)((pIn != NULL) ? pIn->pValue : NULL);\n", sCTypeName, sVarName, sCTypeName);
		return;
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = (%s)((pIn != NULL) ? pIn->pValue : NULL);\n", sCTypeName, sVarName, sCTypeName);
}

static void __xlCgenAsyncLinearAwaitValueDeclNamed(XL_CgenContext* pCtx, const char* sTypeName, const char* sVarName)
{
	const char* sCTypeName;

	sCTypeName = __xlCTypeName((sTypeName != NULL) ? sTypeName : "int");
	sVarName = (sVarName != NULL) ? sVarName : "value";
	if ( __xlIsIntegerTypeName(sTypeName) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = (%s)(intptr_t)((pIn != NULL) ? pIn->pValue : NULL);\n", sCTypeName, sVarName, sCTypeName);
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "bool") == 0) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = (((intptr_t)((pIn != NULL) ? pIn->pValue : NULL)) != 0) ? 1 : 0;\n", sCTypeName, sVarName);
		return;
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "double* xl_async_result_ptr_%s = (double*)((pIn != NULL) ? pIn->pValue : NULL);\n", sVarName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = (xl_async_result_ptr_%s != NULL) ? (%s)(*xl_async_result_ptr_%s) : (%s)0;\n", sCTypeName, sVarName, sVarName, sCTypeName, sVarName, sCTypeName);
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "string") == 0) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = (const char*)(((pIn != NULL) && (pIn->pValue != NULL)) ? pIn->pValue : \"\");\n", sCTypeName, sVarName);
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "error") == 0) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = (%s)((pIn != NULL) ? pIn->pValue : NULL);\n", sCTypeName, sVarName, sCTypeName);
		return;
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = (%s)((pIn != NULL) ? pIn->pValue : NULL);\n", sCTypeName, sVarName, sCTypeName);
}

static int __xlCgenExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);
static int __xlCgenTextExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);
static int __xlCgenExprAsTypeReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const char* sExpectedTypeName,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);
static int __xlCgenBoxExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);

static int __xlCgenIndexAssignReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pLeft,
	const XL_MirNode* pRight,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pIndex;
	const char* sBaseTypeName;
	const char* sValueTypeName;
	const char* sSetFuncName;
	const char* sIndexTypeName;
	char sBaseName[64];
	char sIndexName[64];
	char sValueName[64];
	char sKeyName[64];

	pBase = (pLeft != NULL) && (pLeft->iChildCount != 0) ? pLeft->ppChildren[0] : NULL;
	pIndex = (pLeft != NULL) && (pLeft->iChildCount > 1) ? pLeft->ppChildren[1] : NULL;
	sBaseTypeName = __xlInferExprTypeName(pCtx, pBase);
	sValueTypeName = __xlInferExprTypeName(pCtx, pLeft);

	if ( __xlMirContainsAwaitExpr(pBase) ) {
		return 0;
	}

	snprintf(sBaseName, sizeof(sBaseName), "xl_assign_base_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);
	snprintf(sIndexName, sizeof(sIndexName), "xl_assign_index_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);
	snprintf(sValueName, sizeof(sValueName), "xl_assign_value_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);
	snprintf(sKeyName, sizeof(sKeyName), "xl_assign_key_%d_%d", (pLeft != NULL) ? pLeft->iLine : 0, (pLeft != NULL) ? pLeft->iColumn : 0);

	if ( __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "({ xvalue ");
		__xlCgenAppend(&pCtx->buf, sBaseName);
		__xlCgenAppend(&pCtx->buf, " = ");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "; const char* ");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, " = ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pIndex, "string", pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, "; xvalue ");
		__xlCgenAppend(&pCtx->buf, sValueName);
		__xlCgenAppend(&pCtx->buf, " = ");
		if ( !__xlCgenBoxExprReplacingAwait(pCtx, pRight, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, "; (void)xvoTableSetValue((xvalue)(");
		__xlCgenAppend(&pCtx->buf, sBaseName);
		__xlCgenAppend(&pCtx->buf, "), (const void*)");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, ", (uint32)strlen((");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, " != NULL) ? ");
		__xlCgenAppend(&pCtx->buf, sKeyName);
		__xlCgenAppend(&pCtx->buf, " : \"\"), ");
		__xlCgenAppend(&pCtx->buf, sValueName);
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
		__xlCgenValueExprAsType(pCtx, sValueName, sValueTypeName);
		__xlCgenAppend(&pCtx->buf, "; })");
		return 1;
	}

	sSetFuncName = __xlTypeHasBaseName(sBaseTypeName, "list") ? "xvoListSetValue" : "xvoArraySetValue";
	sIndexTypeName = __xlTypeHasBaseName(sBaseTypeName, "list") ? "int64" : "uint32";
	__xlCgenAppend(&pCtx->buf, "({ xvalue ");
	__xlCgenAppend(&pCtx->buf, sBaseName);
	__xlCgenAppend(&pCtx->buf, " = ");
	__xlCgenExpr(pCtx, pBase);
	__xlCgenAppend(&pCtx->buf, "; ");
	__xlCgenAppend(&pCtx->buf, sIndexTypeName);
	__xlCgenAppend(&pCtx->buf, " ");
	__xlCgenAppend(&pCtx->buf, sIndexName);
	__xlCgenAppend(&pCtx->buf, " = ");
	if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pIndex, "int", pAwaitNode, sReplacementName) ) {
		return 0;
	}
	__xlCgenAppend(&pCtx->buf, "; xvalue ");
	__xlCgenAppend(&pCtx->buf, sValueName);
	__xlCgenAppend(&pCtx->buf, " = ");
	if ( !__xlCgenBoxExprReplacingAwait(pCtx, pRight, pAwaitNode, sReplacementName) ) {
		return 0;
	}
	__xlCgenAppend(&pCtx->buf, "; (void)");
	__xlCgenAppend(&pCtx->buf, sSetFuncName);
	__xlCgenAppend(&pCtx->buf, "((xvalue)(");
	__xlCgenAppend(&pCtx->buf, sBaseName);
	__xlCgenAppend(&pCtx->buf, "), (");
	__xlCgenAppend(&pCtx->buf, sIndexTypeName);
	__xlCgenAppend(&pCtx->buf, ")(");
	__xlCgenAppend(&pCtx->buf, sIndexName);
	__xlCgenAppend(&pCtx->buf, "), ");
	__xlCgenAppend(&pCtx->buf, sValueName);
	__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	__xlCgenValueExprAsType(pCtx, sValueName, sValueTypeName);
	__xlCgenAppend(&pCtx->buf, "; })");
	return 1;
}
static int __xlCgenExprAsTypeReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const char* sExpectedTypeName,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);
static int __xlCgenBoxExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);
static int __xlCgenStrCallReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);
static int __xlCgenContainerMethodCallReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
);

static const char* __xlCgenAsyncLinearStepResultTypeName(const XL_CgenAsyncLinearResumeStep* pStep)
{
	if ( pStep == NULL ) {
		return "int";
	}
	if ( pStep->bExprAwait ) {
		if ( (pStep->pAwaitNode != NULL) && (pStep->pAwaitNode->sTypeName != NULL) && (pStep->pAwaitNode->sTypeName[0] != '\0') ) {
			return pStep->pAwaitNode->sTypeName;
		}
		return "int";
	}
	if ( (pStep->pAwaitBind != NULL) && (pStep->pAwaitBind->sTypeName != NULL) && (pStep->pAwaitBind->sTypeName[0] != '\0') ) {
		return pStep->pAwaitBind->sTypeName;
	}
	if ( (pStep->pAwaitNode != NULL) && (pStep->pAwaitNode->sTypeName != NULL) && (pStep->pAwaitNode->sTypeName[0] != '\0') ) {
		return pStep->pAwaitNode->sTypeName;
	}
	return "int";
}

static void __xlCgenAsyncLinearStepResultName(char* sOut, size_t iOutSize, const XL_CgenAsyncLinearResumeStep* pStep)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( (pStep != NULL) && !pStep->bExprAwait &&
		(pStep->pAwaitBind != NULL) &&
		(pStep->pAwaitBind->sText != NULL) &&
		(pStep->pAwaitBind->sText[0] != '\0') ) {
		snprintf(sOut, iOutSize, "%s", pStep->pAwaitBind->sText);
		return;
	}
	snprintf(
		sOut,
		iOutSize,
		"xl_async_return_await_%d_%d",
		((pStep != NULL) && (pStep->pAwaitNode != NULL)) ? pStep->pAwaitNode->iLine : 0,
		((pStep != NULL) && (pStep->pAwaitNode != NULL)) ? pStep->pAwaitNode->iColumn : 0
	);
}

static const XL_MirNode* __xlCgenAsyncLinearStepAnchorNode(
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	const XL_CgenAsyncLinearResumeStep* pStep
)
{
	if ( (pStep != NULL) && (pStep->pAwaitBind != NULL) ) {
		return pStep->pAwaitBind;
	}
	if ( (pStep != NULL) && (pStep->pAwaitStmt != NULL) ) {
		return pStep->pAwaitStmt;
	}
	return (pCandidate != NULL) ? pCandidate->pReturn : NULL;
}

static void __xlCgenAsyncLinearStateFieldDecl(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const char* sTypeName;
	const char* sName;

	if ( (pCtx == NULL) || (pNode == NULL) ) {
		return;
	}

	sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
	sName = (pNode->sText != NULL) ? pNode->sText : "tmp";
	__xlCgenAppendFormat(&pCtx->buf, "\t%s %s;\n", __xlCTypeName(sTypeName), sName);
}

static int __xlAsyncLinearStateFieldNeedsRetain(const XL_MirNode* pNode)
{
	const char* sTypeName;

	if ( pNode == NULL ) {
		return 0;
	}
	sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
	return __xlIsValueBackedTypeName(sTypeName);
}

static int __xlAsyncLinearStateFieldNeedsCopy(const XL_MirNode* pNode)
{
	const char* sTypeName;

	if ( pNode == NULL ) {
		return 0;
	}
	sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
	return __xlTypeHasBaseName(sTypeName, "array") ||
		__xlTypeHasBaseName(sTypeName, "list") ||
		__xlTypeHasBaseName(sTypeName, "set") ||
		__xlTypeHasBaseName(sTypeName, "dict") ||
		__xlIsVarTypeName(sTypeName);
}

static void __xlCgenAsyncLinearStateFieldRelease(XL_CgenContext* pCtx, const XL_MirNode* pNode, const char* sStateName)
{
	const char* sName;

	if ( (pCtx == NULL) || (pNode == NULL) || (sStateName == NULL) || !__xlAsyncLinearStateFieldNeedsRetain(pNode) ) {
		return;
	}

	sName = (pNode->sText != NULL) ? pNode->sText : "tmp";
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s->%s != NULL ) {\n", sStateName, sName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xvoUnref(%s->%s);\n", sStateName, sName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s->%s = NULL;\n", sStateName, sName);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenAsyncLinearStateFieldRestore(XL_CgenContext* pCtx, const XL_MirNode* pNode, const char* sStateName)
{
	const char* sTypeName;
	const char* sName;

	if ( (pCtx == NULL) || (pNode == NULL) || (sStateName == NULL) ) {
		return;
	}

	sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
	sName = (pNode->sText != NULL) ? pNode->sText : "tmp";
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = %s->%s;\n", __xlCTypeName(sTypeName), sName, sStateName, sName);
}

static void __xlCgenAsyncLinearStateFieldStore(XL_CgenContext* pCtx, const XL_MirNode* pNode, const char* sStateName)
{
	const char* sTypeName;
	const char* sName;
	const char* sCTypeName;

	if ( (pCtx == NULL) || (pNode == NULL) || (sStateName == NULL) ) {
		return;
	}

	sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
	sName = (pNode->sText != NULL) ? pNode->sText : "tmp";
	if ( __xlIsValueBackedTypeName(sTypeName) ) {
		sCTypeName = __xlCTypeName(sTypeName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		if ( __xlAsyncLinearStateFieldNeedsCopy(pNode) ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s xl_async_store_%s = (%s != NULL) ? xl_value_clone_async_state(%s) : NULL;\n", sCTypeName, sName, sName, sName);
		} else {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "%s xl_async_store_%s = %s;\n", sCTypeName, sName, sName);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( xl_async_store_%s != NULL ) xvoAddRef_Inline(xl_async_store_%s);\n", sName, sName);
		}
		__xlCgenAsyncLinearStateFieldRelease(pCtx, pNode, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s->%s = xl_async_store_%s;\n", sStateName, sName, sName);
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		return;
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s->%s = %s;\n", sStateName, sName, sName);
}

static void __xlCgenAsyncLinearStateCleanup(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	const char* sStateName
)
{
	size_t i;
	size_t j;

	if ( (pCtx == NULL) || (pCandidate == NULL) || (sStateName == NULL) ) {
		return;
	}

	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		__xlCgenAsyncLinearStateFieldRelease(pCtx, pParams->ppChildren[i], sStateName);
	}
	for ( i = 0; i < pCandidate->iPreBindCount; ++i ) {
		__xlCgenAsyncLinearStateFieldRelease(pCtx, pCandidate->arrPreBinds[i], sStateName);
	}
	for ( i = 0; i < pCandidate->iStepCount; ++i ) {
		const XL_CgenAsyncLinearResumeStep* pStep = &pCandidate->arrSteps[i];

		if ( pStep->pAwaitBind != NULL ) {
			__xlCgenAsyncLinearStateFieldRelease(pCtx, pStep->pAwaitBind, sStateName);
		}
		for ( j = 0; j < pStep->iPostBindCount; ++j ) {
			__xlCgenAsyncLinearStateFieldRelease(pCtx, pStep->arrPostBinds[j], sStateName);
		}
	}
}

static void __xlCgenAsyncLinearRestoreScope(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	size_t iStepIndex,
	const char* sStateName
)
{
	size_t i;
	size_t j;

	if ( (pCtx == NULL) || (pCandidate == NULL) || (sStateName == NULL) ) {
		return;
	}

	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		__xlCgenAsyncLinearStateFieldRestore(pCtx, pParams->ppChildren[i], sStateName);
	}
	for ( i = 0; i < pCandidate->iPreBindCount; ++i ) {
		__xlCgenAsyncLinearStateFieldRestore(pCtx, pCandidate->arrPreBinds[i], sStateName);
	}
	for ( i = 0; i < iStepIndex; ++i ) {
		const XL_CgenAsyncLinearResumeStep* pStep = &pCandidate->arrSteps[i];

		if ( pStep->pAwaitBind != NULL ) {
			__xlCgenAsyncLinearStateFieldRestore(pCtx, pStep->pAwaitBind, sStateName);
		}
		for ( j = 0; j < pStep->iPostBindCount; ++j ) {
			__xlCgenAsyncLinearStateFieldRestore(pCtx, pStep->arrPostBinds[j], sStateName);
		}
	}
}

static void __xlCgenAsyncLinearStoreScope(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	size_t iStepIndex,
	const char* sStateName
)
{
	size_t i;
	size_t j;

	if ( (pCtx == NULL) || (pCandidate == NULL) || (sStateName == NULL) ) {
		return;
	}

	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		__xlCgenAsyncLinearStateFieldStore(pCtx, pParams->ppChildren[i], sStateName);
	}
	for ( i = 0; i < pCandidate->iPreBindCount; ++i ) {
		__xlCgenAsyncLinearStateFieldStore(pCtx, pCandidate->arrPreBinds[i], sStateName);
	}
	for ( i = 0; i < iStepIndex; ++i ) {
		const XL_CgenAsyncLinearResumeStep* pStep = &pCandidate->arrSteps[i];

		if ( pStep->pAwaitBind != NULL ) {
			__xlCgenAsyncLinearStateFieldStore(pCtx, pStep->pAwaitBind, sStateName);
		}
		for ( j = 0; j < pStep->iPostBindCount; ++j ) {
			__xlCgenAsyncLinearStateFieldStore(pCtx, pStep->arrPostBinds[j], sStateName);
		}
	}
}

static void __xlCgenAsyncLinearResumeCallbackName(char* sOut, size_t iOutSize, const char* sBaseName, size_t iStepIndex)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	if ( iStepIndex == 0 ) {
		snprintf(sOut, iOutSize, "%s", (sBaseName != NULL) ? sBaseName : "xl_async_linear_resume");
		return;
	}
	snprintf(sOut, iOutSize, "%s_%zu", (sBaseName != NULL) ? sBaseName : "xl_async_linear_resume", iStepIndex);
}

static void __xlCgenAsyncLinearPostBind(XL_CgenContext* pCtx, const XL_MirNode* pBind, const char* sStateName)
{
	const XL_MirNode* pInit;
	const char* sTypeName;
	const char* sVarName;

	if ( (pCtx == NULL) || (pBind == NULL) || (pBind->eKind != XL_MIR_BIND) ) {
		return;
	}

	pInit = (pBind->iChildCount != 0) ? pBind->ppChildren[0] : NULL;
	sTypeName = ((pBind->sTypeName != NULL) && (pBind->sTypeName[0] != '\0')) ? pBind->sTypeName : "int";
	sVarName = (pBind->sText != NULL) ? pBind->sText : "tmp";

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTypeName), sVarName);
	__xlCgenExprAsType(pCtx, pInit, sTypeName);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear bind failed");
}

static void __xlCgenAsyncLinearPostStmt(XL_CgenContext* pCtx, const XL_MirNode* pStmt, const XL_CgenFuncInfo* pFuncInfo)
{
	if ( (pCtx == NULL) || (pStmt == NULL) || (pFuncInfo == NULL) ) {
		return;
	}

	__xlCgenStmt(pCtx, pStmt, pFuncInfo);
}

static void __xlCgenAsyncLinearBindResolve(
	XL_CgenContext* pCtx,
	const XL_MirNode* pBind,
	const char* sStateName,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const XL_MirNode* pInit;
	const char* sTypeName;
	const char* sVarName;

	if ( (pCtx == NULL) || (pBind == NULL) || (pBind->eKind != XL_MIR_BIND) ) {
		return;
	}

	pInit = (pBind->iChildCount != 0) ? pBind->ppChildren[0] : NULL;
	sTypeName = ((pBind->sTypeName != NULL) && (pBind->sTypeName[0] != '\0')) ? pBind->sTypeName : "int";
	sVarName = (pBind->sText != NULL) ? pBind->sText : "tmp";
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sTypeName), sVarName);
	if ( !__xlCgenExprReplacingAwait(pCtx, pInit, pAwaitNode, sReplacementName) ) {
		__xlCgenExprAsType(pCtx, pInit, sTypeName);
	}
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear bind failed");
}

static int __xlCgenExprAsTypeReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const char* sExpectedTypeName,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const char* sActualTypeName;

	if ( (pCtx == NULL) || (pNode == NULL) ) {
		return 0;
	}
	if ( !__xlMirContainsAwaitExpr(pNode) ) {
		__xlCgenExprAsType(pCtx, pNode, sExpectedTypeName);
		return 1;
	}

	sActualTypeName = __xlInferExprTypeName(pCtx, pNode);
	if ( (sExpectedTypeName == NULL) || (sExpectedTypeName[0] == '\0') || (strcmp(sExpectedTypeName, sActualTypeName) == 0) ) {
		return __xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName);
	}
	if ( __xlIsVarTypeName(sExpectedTypeName) || __xlIsVarTypeName(sActualTypeName) ||
		__xlIsFunctionTypeName(sExpectedTypeName) || __xlIsFunctionTypeName(sActualTypeName) ) {
		return 0;
	}
	if ( __xlIsIntegerTypeName(sExpectedTypeName) && __xlIsIntegerTypeName(sActualTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "((%s)", __xlCTypeName(sExpectedTypeName));
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	if ( __xlIsFloatTypeName(sExpectedTypeName) && __xlIsFloatTypeName(sActualTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "((%s)", __xlCTypeName(sExpectedTypeName));
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	if ( (strcmp(sExpectedTypeName, "bool") == 0) && (strcmp(sActualTypeName, "bool") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "((");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ") ? 1 : 0)");
		return 1;
	}
	return 0;
}

static int __xlCgenCallArgsReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pCall,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pFunc;
	const XL_MirNode* pParams;
	const XL_MirNode* pParam;
	size_t i;

	pArgs = ((pCall != NULL) && (pCall->iChildCount > 1)) ? pCall->ppChildren[1] : NULL;
	pFunc = __xlResolveCallFuncNode(pCtx, pCall);
	pParams = ((pFunc != NULL) && (pFunc->iChildCount != 0)) ? pFunc->ppChildren[0] : NULL;
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		if ( i != 0 ) {
			__xlCgenAppend(&pCtx->buf, ", ");
		}
		pParam = ((pParams != NULL) && (i < pParams->iChildCount)) ? pParams->ppChildren[i] : NULL;
		if ( !__xlCgenExprAsTypeReplacingAwait(
			pCtx,
			pArgs->ppChildren[i],
			(pParam != NULL) ? pParam->sTypeName : NULL,
			pAwaitNode,
			sReplacementName
		) ) {
			return 0;
		}
	}
	return 1;
}

static int __xlCgenBoxExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const char* sTypeName;

	if ( (pCtx == NULL) || (pNode == NULL) ) {
		return 0;
	}
	if ( !__xlMirContainsAwaitExpr(pNode) ) {
		__xlCgenBoxValueExpr(pCtx, pNode);
		return 1;
	}

	sTypeName = __xlInferExprTypeName(pCtx, pNode);
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return 0;
	}
	if ( __xlIsVarTypeName(sTypeName) ) {
		return 0;
	}
	__xlCgenAppend(&pCtx->buf, "({ ");
	__xlCgenAppendFormat(&pCtx->buf, "%s xl_async_boxed_value = ", __xlCTypeName(sTypeName));
	if ( !__xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
		return 0;
	}
	__xlCgenAppend(&pCtx->buf, "; ");
	__xlCgenBoxTypedCExpr(pCtx, sTypeName, "xl_async_boxed_value");
	__xlCgenAppend(&pCtx->buf, "; })");
	return 1;
}

static int __xlCgenTextExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const char* sTypeName;

	if ( (pCtx == NULL) || (pNode == NULL) ) {
		return 0;
	}

	sTypeName = __xlInferExprTypeName(pCtx, pNode);
	if ( __xlIsVarTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_value_to_string((xvalue)(");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, "))");
		return 1;
	}
	if ( __xlIsPointTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xl_point_to_string(");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "error") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_error_text(");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	return __xlCgenExprAsTypeReplacingAwait(pCtx, pNode, "string", pAwaitNode, sReplacementName);
}

static int __xlCgenFunctionValueCallExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pCallee;
	const char* sCalleeType;
	const char* sResultType;
	size_t i;

	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sCalleeType = __xlInferExprTypeName(pCtx, pCallee);
	sResultType = __xlInferExprTypeName(pCtx, pNode);
	if ( __xlMirContainsAwaitExpr(pCallee) ) {
		return 0;
	}

	__xlCgenAppend(&pCtx->buf, "({ xvalue xl_call_args = xvoCreateArray(); ");
	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		const XL_MirNode* pArg = pArgs->ppChildren[i];

		__xlCgenAppend(&pCtx->buf, "(void)xvoArrayAppendValue(xl_call_args, ");
		if ( !__xlCgenBoxExprReplacingAwait(pCtx, pArg, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ", TRUE); ");
	}
	__xlCgenAppend(&pCtx->buf, "xvalue xl_call_result_value = ((xfunction)(");
	__xlCgenExprAsType(pCtx, pCallee, sCalleeType);
	__xlCgenAppend(&pCtx->buf, "))(NULL, xl_call_args); xvoUnref(xl_call_args); ");
	if ( (sResultType == NULL) || (sResultType[0] == '\0') || (strcmp(sResultType, "void") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "if ( xl_call_result_value != NULL ) xvoUnref(xl_call_result_value); })");
		return 1;
	}

	__xlCgenAppendFormat(&pCtx->buf, "%s xl_call_result = ", __xlCTypeName(sResultType));
	__xlCgenValueExprAsType(pCtx, "xl_call_result_value", sResultType);
	__xlCgenAppend(&pCtx->buf, "; if ( xl_call_result_value != NULL ) xvoUnref(xl_call_result_value); xl_call_result; })");
	return 1;
}

static int __xlCgenStrCallReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const XL_MirNode* pArg1;
	const XL_MirNode* pArg2;
	XL_StrHelperKind eKind;

	eKind = __xlStrHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	pArg1 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	pArg2 = ((pArgs != NULL) && (pArgs->iChildCount > 2)) ? pArgs->ppChildren[2] : NULL;

	switch ( eKind ) {
		case XL_STR_HELPER_EMPTY:
			__xlCgenAppend(&pCtx->buf, "xl_str_empty(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_BYTE_LEN:
			__xlCgenAppend(&pCtx->buf, "xl_str_byte_len(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_CONTAINS:
			__xlCgenAppend(&pCtx->buf, "xl_str_contains(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_INDEX_OF:
			__xlCgenAppend(&pCtx->buf, "xl_str_index_of(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_LAST_INDEX_OF:
			__xlCgenAppend(&pCtx->buf, "xl_str_last_index_of(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_SLICE:
			__xlCgenAppend(&pCtx->buf, "xl_str_slice(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "int", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg2, "int", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_REPEAT:
			__xlCgenAppend(&pCtx->buf, "xl_str_repeat(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "int", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_SPLIT:
			__xlCgenAppend(&pCtx->buf, "xl_str_split(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_SPLIT_LINES:
			__xlCgenAppend(&pCtx->buf, "xl_str_split_lines(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_STARTS_WITH:
			__xlCgenAppend(&pCtx->buf, "xl_str_starts_with(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_ENDS_WITH:
			__xlCgenAppend(&pCtx->buf, "xl_str_ends_with(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_TRIM:
			__xlCgenAppend(&pCtx->buf, "xl_str_trim(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_TRIM_LEFT:
			__xlCgenAppend(&pCtx->buf, "xl_str_trim_left(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_TRIM_RIGHT:
			__xlCgenAppend(&pCtx->buf, "xl_str_trim_right(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_LOWER:
			__xlCgenAppend(&pCtx->buf, "xl_str_lower(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_UPPER:
			__xlCgenAppend(&pCtx->buf, "xl_str_upper(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_REPLACE:
			__xlCgenAppend(&pCtx->buf, "xl_str_replace(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg2, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_REVERSE:
			__xlCgenAppend(&pCtx->buf, "xl_str_reverse(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_PAD_LEFT:
			__xlCgenAppend(&pCtx->buf, "xl_str_pad_left(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "int", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg2, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		case XL_STR_HELPER_PAD_RIGHT:
			__xlCgenAppend(&pCtx->buf, "xl_str_pad_right(");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg1, "int", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg2, "string", pAwaitNode, sReplacementName) ) return 0;
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		default:
			return 0;
	}
}

static int __xlCgenContainerMethodCallReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const XL_MirNode* pBase;
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	const char* sCalleeName;
	const char* sBaseTypeName;
	const char* sMethodName;

	pBase = __xlContainerMethodBase(pNode);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	sCalleeName = __xlCallCalleeName(pNode);
	if ( (sCalleeName != NULL) &&
		((strcmp(sCalleeName, "__xl_method_has") == 0) ||
		(strcmp(sCalleeName, "__xl_method_append") == 0) ||
		(strcmp(sCalleeName, "__xl_method_join") == 0) ||
		(strcmp(sCalleeName, "__xl_method_remove") == 0)) ) {
		pArg0 = ((pArgs != NULL) && (pArgs->iChildCount > 1)) ? pArgs->ppChildren[1] : NULL;
	} else {
		pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;
	}
	sBaseTypeName = __xlInferExprTypeName(pCtx, pBase);
	sMethodName = __xlContainerMethodName(pNode);

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "len") == 0) ) {
		if ( __xlTypeHasBaseName(sBaseTypeName, "array") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_array(");
		} else if ( __xlTypeHasBaseName(sBaseTypeName, "list") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_list(");
		} else if ( __xlTypeHasBaseName(sBaseTypeName, "set") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_set(");
		} else if ( __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
			__xlCgenAppend(&pCtx->buf, "xl_core_len_dict(");
		} else {
			__xlCgenAppend(&pCtx->buf, "0");
			return 1;
		}
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "has") == 0) && __xlTypeHasBaseName(sBaseTypeName, "set") ) {
		__xlCgenAppend(&pCtx->buf, "({ xvalue xl_set_item = ");
		if ( !__xlCgenBoxExprReplacingAwait(pCtx, pArg0, pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, "; int xl_set_has = xvoCollExists((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), xl_set_item) ? 1 : 0; xvoUnref(xl_set_item); xl_set_has; })");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "has") == 0) && __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, "; int xl_dict_has = xvoTableExists((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), (str)xl_dict_key, (uint32)strlen((xl_dict_key != NULL) ? xl_dict_key : \"\")) ? 1 : 0; xl_dict_has; })");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "append") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_append((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		if ( !__xlCgenBoxExprReplacingAwait(pCtx, pArg0, pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "append") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_append((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		if ( !__xlCgenBoxExprReplacingAwait(pCtx, pArg0, pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "join") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_join((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "join") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_join((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "set") ) {
		__xlCgenAppend(&pCtx->buf, "xl_set_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "clear") == 0) && __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "xl_dict_clear((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "))");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "array") ) {
		__xlCgenAppend(&pCtx->buf, "xl_array_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "int", pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "list") ) {
		__xlCgenAppend(&pCtx->buf, "xl_list_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "int", pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "set") ) {
		__xlCgenAppend(&pCtx->buf, "({ xvalue xl_set_item = ");
		if ( !__xlCgenBoxExprReplacingAwait(pCtx, pArg0, pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, "; int xl_set_removed = xl_set_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), xl_set_item); xvoUnref(xl_set_item); xl_set_removed; })");
		return 1;
	}

	if ( (sMethodName != NULL) && (strcmp(sMethodName, "remove") == 0) && __xlTypeHasBaseName(sBaseTypeName, "dict") ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_dict_key = ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pArg0, "string", pAwaitNode, sReplacementName) ) return 0;
		__xlCgenAppend(&pCtx->buf, "; xl_dict_remove((xvalue)(");
		__xlCgenExpr(pCtx, pBase);
		__xlCgenAppend(&pCtx->buf, "), xl_dict_key); })");
		return 1;
	}

	return 0;
}

static int __xlCgenExprReplacingAwait(
	XL_CgenContext* pCtx,
	const XL_MirNode* pNode,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	if ( pNode == NULL ) {
		__xlCgenAppend(&pCtx->buf, "0");
		return 1;
	}
	if ( pNode == pAwaitNode ) {
		__xlCgenAppend(&pCtx->buf, (sReplacementName != NULL) ? sReplacementName : "0");
		return 1;
	}
	if ( !__xlMirContainsAwaitExpr(pNode) ) {
		__xlCgenExpr(pCtx, pNode);
		return 1;
	}
	if ( (pNode->eKind == XL_MIR_MEMBER) && (pNode->iChildCount == 1) ) {
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[0], pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppendFormat(&pCtx->buf, ".%s", (pNode->sText != NULL) ? pNode->sText : "");
		return 1;
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 1) &&
		(((pNode->sText == NULL) || (pNode->sText[0] == '\0')) || (strcmp(pNode->sText, "await") != 0)) ) {
		if ( (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
			__xlCgenAppendFormat(&pCtx->buf, "(%s", __xlCUnaryOp(pNode->sText));
			if ( !__xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[0], pAwaitNode, sReplacementName) ) {
				return 0;
			}
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		}
		return __xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[0], pAwaitNode, sReplacementName);
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) && (strcmp(pNode->sText, "=") == 0) ) {
		const XL_MirNode* pLeft = pNode->ppChildren[0];
		const XL_MirNode* pRight = pNode->ppChildren[1];
		const char* sLeftTypeName = __xlInferExprTypeName(pCtx, pLeft);
		const char* sRightTypeName = __xlInferExprTypeName(pCtx, pRight);

		if ( (pLeft != NULL) &&
			(pLeft->iChildCount == 2) &&
			((pLeft->sText == NULL) || (pLeft->sText[0] == '\0')) ) {
			const XL_MirNode* pBase = pLeft->ppChildren[0];
			const char* sBaseTypeName = __xlInferExprTypeName(pCtx, pBase);

			if ( __xlTypeHasBaseName(sBaseTypeName, "array") ||
				__xlTypeHasBaseName(sBaseTypeName, "list") ||
				__xlTypeHasBaseName(sBaseTypeName, "dict") ) {
				return __xlCgenIndexAssignReplacingAwait(pCtx, pLeft, pRight, pAwaitNode, sReplacementName);
			}
		}
		if ( __xlMirContainsAwaitExpr(pLeft) ||
			__xlIsVarTypeName(sLeftTypeName) ||
			__xlIsVarTypeName(sRightTypeName) ||
			__xlIsFunctionTypeName(sLeftTypeName) ||
			__xlIsFunctionTypeName(sRightTypeName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, "(");
		__xlCgenExpr(pCtx, pLeft);
		__xlCgenAppend(&pCtx->buf, " = ");
		if ( !__xlCgenExprAsTypeReplacingAwait(pCtx, pRight, sLeftTypeName, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		const char* sLeftTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[0]);
		const char* sRightTypeName = __xlInferExprTypeName(pCtx, pNode->ppChildren[1]);

		if ( (strcmp(pNode->sText, "+") == 0) &&
			(__xlIsTextLikeType(sLeftTypeName) || __xlIsTextLikeType(sRightTypeName)) ) {
			__xlCgenAppend(&pCtx->buf, "xl_str_concat(");
			if ( !__xlCgenTextExprReplacingAwait(pCtx, pNode->ppChildren[0], pAwaitNode, sReplacementName) ) {
				return 0;
			}
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenTextExprReplacingAwait(pCtx, pNode->ppChildren[1], pAwaitNode, sReplacementName) ) {
				return 0;
			}
			__xlCgenAppend(&pCtx->buf, ")");
			return 1;
		}
		if ( ((strcmp(pNode->sText, "==") == 0) || (strcmp(pNode->sText, "!=") == 0)) &&
			(__xlIsTextLikeType(sLeftTypeName) || __xlIsTextLikeType(sRightTypeName)) ) {
			if ( strcmp(pNode->sText, "!=") == 0 ) {
				__xlCgenAppend(&pCtx->buf, "(!xl_generated_text_equal(");
			} else {
				__xlCgenAppend(&pCtx->buf, "xl_generated_text_equal(");
			}
			if ( !__xlCgenTextExprReplacingAwait(pCtx, pNode->ppChildren[0], pAwaitNode, sReplacementName) ) {
				return 0;
			}
			__xlCgenAppend(&pCtx->buf, ", ");
			if ( !__xlCgenTextExprReplacingAwait(pCtx, pNode->ppChildren[1], pAwaitNode, sReplacementName) ) {
				return 0;
			}
			__xlCgenAppend(&pCtx->buf, (strcmp(pNode->sText, "!=") == 0) ? "))" : ")");
			return 1;
		}
		__xlCgenAppend(&pCtx->buf, "(");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[0], pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppendFormat(&pCtx->buf, " %s ", __xlCLogicalBinaryOp(pNode->sText));
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[1], pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 3) && (pNode->sText != NULL) && (strcmp(pNode->sText, "?:") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "(");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[0], pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, " ? ");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[1], pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, " : ");
		if ( !__xlCgenExprReplacingAwait(pCtx, pNode->ppChildren[2], pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlStrHelperKind(__xlCallCalleeName(pNode)) != XL_STR_HELPER_NONE) ) {
		return __xlCgenStrCallReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName);
	}
	if ( (pNode->eKind == XL_MIR_CALL) && (__xlContainerMethodBase(pNode) != NULL) ) {
		return __xlCgenContainerMethodCallReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName);
	}
	if ( pNode->eKind == XL_MIR_CALL ) {
		const XL_MirNode* pFunc;
		const XL_MirNode* pCallee;
		const char* sCalleeType;
		const char* sFuncSymbol;

		if ( __xlCallHasExpandedMultiReturnArg(pNode) ) {
			return 0;
		}
		pFunc = __xlResolveCallFuncNode(pCtx, pNode);
		pCallee = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
		sCalleeType = __xlInferExprTypeName(pCtx, pCallee);
		sFuncSymbol = __xlMirFuncSymbolName(pFunc);
		if ( (pFunc != NULL) && (sFuncSymbol != NULL) && (sFuncSymbol[0] != '\0') ) {
			__xlCgenAppend(&pCtx->buf, sFuncSymbol);
		} else if ( __xlIsFunctionTypeName(sCalleeType) ) {
			return __xlCgenFunctionValueCallExprReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName);
		} else {
			if ( __xlMirContainsAwaitExpr(pCallee) ) {
				return 0;
			}
			__xlCgenExpr(pCtx, pCallee);
		}
		__xlCgenAppend(&pCtx->buf, "(");
		if ( !__xlCgenCallArgsReplacingAwait(pCtx, pNode, pAwaitNode, sReplacementName) ) {
			return 0;
		}
		__xlCgenAppend(&pCtx->buf, ")");
		return 1;
	}
	return 0;
}

static void __xlCgenAsyncLinearReturnResolve(
	XL_CgenContext* pCtx,
	const XL_CgenFuncInfo* pFuncInfo,
	const XL_MirNode* pReturn,
	const char* sStateName,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName
)
{
	const XL_MirNode* pExpr;
	const char* sValueTypeName;
	const char* sResolveFunc;

	pExpr = ((pReturn != NULL) && (pReturn->iChildCount != 0)) ? pReturn->ppChildren[0] : NULL;
	sValueTypeName = __xlFutureValueTypeName((pFuncInfo != NULL) ? pFuncInfo->sTypeName : NULL);
	sResolveFunc = __xlPromiseResolveFunc((pFuncInfo != NULL) ? pFuncInfo->sTypeName : NULL);

	if ( (sValueTypeName == NULL) || (strcmp(sValueTypeName, "void") == 0) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( !%s(%s->pPromise) ) {\n", sResolveFunc, sStateName);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "(void)xl_generated_promise_reject_text(%s->pPromise, XRT_NET_ERROR, \"async resolve failed\");\n", sStateName);
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		return;
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"%s xl_async_ret_%d_%d = ",
		__xlCTypeName(sValueTypeName),
		(pReturn != NULL) ? pReturn->iLine : 0,
		(pReturn != NULL) ? pReturn->iColumn : 0
	);
	if ( (pAwaitNode != NULL) && (sReplacementName != NULL) ) {
		if ( !__xlCgenExprReplacingAwait(pCtx, pExpr, pAwaitNode, sReplacementName) ) {
			__xlCgenExprAsType(pCtx, pExpr, sValueTypeName);
		}
	} else {
		__xlCgenExprAsType(pCtx, pExpr, sValueTypeName);
	}
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear return failed");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"if ( !%s(%s->pPromise, xl_async_ret_%d_%d) ) {\n",
		sResolveFunc,
		sStateName,
		(pReturn != NULL) ? pReturn->iLine : 0,
		(pReturn != NULL) ? pReturn->iColumn : 0
	);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "(void)xl_generated_promise_reject_text(%s->pPromise, XRT_NET_ERROR, \"async resolve failed\");\n", sStateName);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
}

static void __xlCgenAsyncLinearStmtResolve(
	XL_CgenContext* pCtx,
	const XL_MirNode* pStmt,
	const XL_MirNode* pParams,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	size_t iStepIndex,
	const char* sStateName,
	const XL_MirNode* pAwaitNode,
	const char* sReplacementName,
	const char* sResumeCallbackName
)
{
	const XL_MirNode* pCond;
	const XL_MirNode* pThenBlock;
	const XL_MirNode* pElseBlock;
	const XL_MirNode* pBody;
	const XL_MirNode* pTryBlock;
	const XL_MirNode* pInnerTryStmt;
	const XL_MirNode* pInnerBlockStmt;
	const XL_MirNode* pCatchName;
	const XL_MirNode* pCatchBlock;
	const XL_MirNode* pFinallyBlock;
	const XL_CgenAsyncLinearResumeStep* pCurrentStep;
	const XL_MirNode* pAwaitNodeAlt;
	size_t iInnerBlockStmtIndex;
	size_t iInnerTryStmtIndex;
	size_t i;

	if ( (pCtx == NULL) || (pStmt == NULL) ) {
		return;
	}
	pCurrentStep = ((pCandidate != NULL) && (iStepIndex < pCandidate->iStepCount)) ? &pCandidate->arrSteps[iStepIndex] : NULL;
	pAwaitNodeAlt = (pCurrentStep != NULL) ? pCurrentStep->pAwaitNodeAlt : NULL;
	if ( (pAwaitNodeAlt != NULL) &&
		!__xlCanAsyncLinearResolveStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt) ) {
		pAwaitNodeAlt = NULL;
	}

	if ( pStmt->eKind == XL_MIR_BRANCH ) {
		int bDynamicAwaitOnTrue;
		char sBranchSelectorField[64];

		pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		pThenBlock = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
		pElseBlock = (pStmt->iChildCount > 2) ? pStmt->ppChildren[2] : NULL;
		pInnerBlockStmt = __xlSelectAsyncLinearBranchDualAwaitBlock(pCtx, pStmt, NULL, NULL, NULL);
		if ( pInnerBlockStmt != NULL ) {
			__xlCgenAsyncLinearStmtResolve(
				pCtx,
				pInnerBlockStmt,
				pParams,
				pCandidate,
				iStepIndex,
				sStateName,
				pAwaitNode,
				sReplacementName,
				sResumeCallbackName
			);
			return;
		}
		if ( (pAwaitNodeAlt != NULL) &&
			__xlAnalyzeAsyncLinearDualAwaitStep(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, NULL) ) {
			__xlCgenAsyncLinearBranchSelectorFieldName(sBranchSelectorField, sizeof(sBranchSelectorField), iStepIndex);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( %s->%s == 1 )\n", sStateName, sBranchSelectorField);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAsyncLinearStmtResolve(
				pCtx,
				pThenBlock,
				pParams,
				pCandidate,
				iStepIndex,
				sStateName,
				pAwaitNode,
				sReplacementName,
				sResumeCallbackName
			);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "else\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAsyncLinearStmtResolve(
				pCtx,
				pElseBlock,
				pParams,
				pCandidate,
				iStepIndex,
				sStateName,
				pAwaitNodeAlt,
				sReplacementName,
				sResumeCallbackName
			);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			return;
		}
		pInnerBlockStmt = __xlSelectAsyncLinearBranchAwaitBlock(pCtx, pStmt, pAwaitNode);
		if ( pInnerBlockStmt != NULL ) {
			__xlCgenAsyncLinearStmtResolve(
				pCtx,
				pInnerBlockStmt,
				pParams,
				pCandidate,
				iStepIndex,
				sStateName,
				pAwaitNode,
				sReplacementName,
				sResumeCallbackName
			);
			return;
		}
		if ( __xlAnalyzeAsyncLinearDynamicBranchAwait(pCtx, pStmt, pAwaitNode, &pInnerBlockStmt, NULL, &bDynamicAwaitOnTrue) ) {
			__xlCgenAsyncLinearBranchSelectorFieldName(sBranchSelectorField, sizeof(sBranchSelectorField), iStepIndex);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( %s->%s == %d )\n", sStateName, sBranchSelectorField, bDynamicAwaitOnTrue ? 1 : 0);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAsyncLinearStmtResolve(
				pCtx,
				pInnerBlockStmt,
				pParams,
				pCandidate,
				iStepIndex,
				sStateName,
				pAwaitNode,
				sReplacementName,
				sResumeCallbackName
			);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			return;
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "int xl_if_cond_%d_%d = ", pStmt->iLine, pStmt->iColumn);
		if ( (pAwaitNode != NULL) && (sReplacementName != NULL) ) {
			if ( !__xlCgenExprReplacingAwait(pCtx, pCond, pAwaitNode, sReplacementName) ) {
				__xlCgenExpr(pCtx, pCond);
			}
		} else {
			__xlCgenExpr(pCtx, pCond);
		}
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear branch failed");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( xl_if_cond_%d_%d )\n", pStmt->iLine, pStmt->iColumn);
		__xlCgenStmt(pCtx, pThenBlock, pCtx->pCurrentFuncInfo);
		if ( pElseBlock != NULL ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "else\n");
			__xlCgenStmt(pCtx, pElseBlock, pCtx->pCurrentFuncInfo);
		}
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear branch failed");
		return;
	}
	if ( pStmt->eKind == XL_MIR_BLOCK ) {
		if ( __xlFindAsyncLinearBlockAwaitStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, &pInnerBlockStmt, &iInnerBlockStmtIndex) ) {
			__xlCgenAsyncLinearStmtResolve(
				pCtx,
				pInnerBlockStmt,
				pParams,
				pCandidate,
				iStepIndex,
				sStateName,
				pAwaitNode,
				sReplacementName,
				sResumeCallbackName
			);
			for ( i = iInnerBlockStmtIndex + 1; i < pStmt->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pStmt->ppChildren[i], NULL);
				__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear block failed");
			}
			return;
		}
	}
	if ( pStmt->eKind == XL_MIR_WHILE ) {
		const XL_CgenAsyncLinearResumeStep* pStep;
		const XL_CgenFuncInfo* pFuncInfo;
		const char* sAwaitExprType;
		const char* sAwaitTraceName;
		int iAwaitStateId;
		char sFutureValueName[64];
		char sHookName[64];
		char sBreakLabel[64];
		char sContinueLabel[64];

		pStep = ((pCandidate != NULL) && (iStepIndex < pCandidate->iStepCount)) ? &pCandidate->arrSteps[iStepIndex] : NULL;
		pFuncInfo = pCtx->pCurrentFuncInfo;
		pCond = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		pBody = (pStmt->iChildCount > 1) ? pStmt->ppChildren[1] : NULL;
		sAwaitExprType = (pStep != NULL) ? __xlInferExprTypeName(pCtx, pStep->pAwaitInner) : NULL;
		sAwaitTraceName = (pStep != NULL) ? __xlCallCalleeName(pStep->pAwaitInner) : NULL;
		iAwaitStateId = ((pFuncInfo != NULL) && (pFuncInfo->pAsyncFunc != NULL) && (pStep != NULL))
			? __xlCgenFindAsyncStateId(pFuncInfo->pAsyncFunc, __xlCgenAsyncLinearStepAnchorNode(pCandidate, pStep), "await")
			: -1;
		if ( (sAwaitTraceName == NULL) || (sAwaitTraceName[0] == '\0') ) {
			sAwaitTraceName = "await";
		}
		snprintf(sFutureValueName, sizeof(sFutureValueName), "xl_async_future_loop_%zu", (pStep != NULL) ? pStep->iAwaitStmtIndex : iStepIndex);
		snprintf(sHookName, sizeof(sHookName), "xl_async_hook_loop_%zu", (pStep != NULL) ? pStep->iAwaitStmtIndex : iStepIndex);
		snprintf(sBreakLabel, sizeof(sBreakLabel), "xl_async_while_break_%d_%d", pStmt->iLine, pStmt->iColumn);
		snprintf(sContinueLabel, sizeof(sContinueLabel), "xl_async_while_continue_%d_%d", pStmt->iLine, pStmt->iColumn);

		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "int xl_while_cond_%d_%d = ", pStmt->iLine, pStmt->iColumn);
		if ( (pAwaitNode != NULL) && (sReplacementName != NULL) ) {
			if ( !__xlCgenExprReplacingAwait(pCtx, pCond, pAwaitNode, sReplacementName) ) {
				__xlCgenExpr(pCtx, pCond);
			}
		} else {
			__xlCgenExpr(pCtx, pCond);
		}
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear while failed");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( xl_while_cond_%d_%d )\n", pStmt->iLine, pStmt->iColumn);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenPushLoopFrame(pCtx, sBreakLabel, sContinueLabel);
		if ( pBody != NULL ) {
			__xlCgenStmt(pCtx, pBody, pCtx->pCurrentFuncInfo);
		}
		__xlCgenPopLoopFrame(pCtx);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sContinueLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear while body failed");
		__xlCgenAsyncLinearStoreScope(pCtx, pParams, pCandidate, iStepIndex, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sAwaitExprType), sFutureValueName);
		__xlCgenExprAsType(pCtx, (pStep != NULL) ? pStep->pAwaitInner : __xlAwaitInnerExpr(pAwaitNode), sAwaitExprType);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear await failed");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s.handle == NULL ) {\n", sFutureValueName);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, \"async await future missing\");\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( pState->pPromise != NULL ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xPromiseDestroy(pState->pPromise);\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAsyncLinearStateCleanup(pCtx, pParams, pCandidate, "pState");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xrtFree(pState);\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return;\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		if ( iAwaitStateId >= 0 ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
			__xlCgenAppendCString(&pCtx->buf, (pFuncInfo != NULL) ? pFuncInfo->sName : "");
			__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
		}
		__xlCgenTraceEventStmt(pCtx, "await", sAwaitTraceName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xfuture* %s = xFutureFinallyEngine(%s.handle, NULL, 0u, %s, pState);\n", sHookName, sFutureValueName, (sResumeCallbackName != NULL) ? sResumeCallbackName : "NULL");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sHookName);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, \"async linear hook failed\");\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( pState->pPromise != NULL ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xPromiseDestroy(pState->pPromise);\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAsyncLinearStateCleanup(pCtx, pParams, pCandidate, "pState");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xrtFree(pState);\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return;\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sHookName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return;\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sBreakLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		return;
	}
	if ( __xlCanAsyncLinearResolveTryCatchStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, &pInnerTryStmt, &pCatchName, &pCatchBlock, &pFinallyBlock, &iInnerTryStmtIndex) ) {
		pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearStmtResolve(
			pCtx,
			pInnerTryStmt,
			pParams,
			pCandidate,
			iStepIndex,
			sStateName,
			pAwaitNode,
			sReplacementName,
			sResumeCallbackName
		);
		if ( (pTryBlock != NULL) && (pTryBlock->eKind == XL_MIR_BLOCK) && ((iInnerTryStmtIndex + 1) < pTryBlock->iChildCount) ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( !xl_generated_async_has_pending_error() )\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			for ( i = iInnerTryStmtIndex + 1; i < pTryBlock->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pTryBlock->ppChildren[i], NULL);
			}
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_async_has_pending_error() ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearCatchValueDeclFromCurrent(pCtx, pCatchName);
		__xlCgenStmt(pCtx, pCatchBlock, NULL);
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		if ( pFinallyBlock != NULL ) {
			__xlCgenSetCurrentFinallyBody(pCtx, 1);
			__xlCgenStmt(pCtx, pFinallyBlock, NULL);
			__xlCgenSetCurrentFinallyBody(pCtx, 0);
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_async_has_pending_error() ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearRejectCurrentAndCleanup(pCtx, pParams, pCandidate, sStateName, "async linear try/catch failed");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		return;
	}
	if ( __xlCanAsyncLinearResolveTryFinallyStmtEx(pCtx, pStmt, pAwaitNode, pAwaitNodeAlt, &pInnerTryStmt, &pFinallyBlock, &iInnerTryStmtIndex) ) {
		pTryBlock = (pStmt->iChildCount != 0) ? pStmt->ppChildren[0] : NULL;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearStmtResolve(
			pCtx,
			pInnerTryStmt,
			pParams,
			pCandidate,
			iStepIndex,
			sStateName,
			pAwaitNode,
			sReplacementName,
			sResumeCallbackName
		);
		if ( (pTryBlock != NULL) && (pTryBlock->eKind == XL_MIR_BLOCK) && ((iInnerTryStmtIndex + 1) < pTryBlock->iChildCount) ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( !xl_generated_async_has_pending_error() )\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			for ( i = iInnerTryStmtIndex + 1; i < pTryBlock->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pTryBlock->ppChildren[i], NULL);
			}
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		}
		__xlCgenSetCurrentFinallyBody(pCtx, 1);
			__xlCgenStmt(pCtx, pFinallyBlock, NULL);
		__xlCgenSetCurrentFinallyBody(pCtx, 0);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_async_has_pending_error() ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearRejectCurrentAndCleanup(pCtx, pParams, pCandidate, sStateName, "async linear try/finally failed");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		return;
	}

	__xlCgenAppendIndent(&pCtx->buf);
	if ( (pAwaitNode != NULL) && (sReplacementName != NULL) ) {
		if ( !__xlCgenExprReplacingAwait(pCtx, pStmt, pAwaitNode, sReplacementName) ) {
			__xlCgenExpr(pCtx, pStmt);
		}
	} else {
		__xlCgenExpr(pCtx, pStmt);
	}
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAsyncLinearPendingErrorCheck(pCtx, sStateName, "async linear stmt failed");
}

static void __xlCgenEmitAsyncLinearResumeHelper(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_CgenFuncInfo* pFuncInfo,
	const XL_CgenAsyncLinearResumeCandidate* pCandidate,
	const char* sStateTypeName,
	const char* sResumeFuncName
)
{
	size_t i;
	size_t j;
	const XL_CgenFuncInfo* pPrevFuncInfo;
	char sResumeCallbackName[320];
	char sNextResumeCallbackName[320];

	if ( (pCtx == NULL) || (pFuncInfo == NULL) || (pCandidate == NULL) ) {
		return;
	}

	__xlCgenAppendFormat(&pCtx->buf, "typedef struct %s {\n", sStateTypeName);
	__xlCgenAppend(&pCtx->buf, "\txpromise* pPromise;\n");
	__xlCgenAppend(&pCtx->buf, "\tint iStep;\n");
	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		__xlCgenAsyncLinearStateFieldDecl(pCtx, pParams->ppChildren[i]);
	}
	for ( i = 0; i < pCandidate->iPreBindCount; ++i ) {
		__xlCgenAsyncLinearStateFieldDecl(pCtx, pCandidate->arrPreBinds[i]);
	}
	for ( i = 0; i < pCandidate->iStepCount; ++i ) {
		const XL_CgenAsyncLinearResumeStep* pStep = &pCandidate->arrSteps[i];
		char sBranchSelectorField[64];
		int bStepDualBranch;

		if ( pStep->pAwaitBind != NULL ) {
			__xlCgenAsyncLinearStateFieldDecl(pCtx, pStep->pAwaitBind);
		}
		for ( j = 0; j < pStep->iPostBindCount; ++j ) {
			__xlCgenAsyncLinearStateFieldDecl(pCtx, pStep->arrPostBinds[j]);
		}
		bStepDualBranch = 0;
		if ( __xlAnalyzeAsyncLinearStepBranchSelector(pCtx, pStep, NULL, &bStepDualBranch) ) {
			__xlCgenAsyncLinearBranchSelectorFieldName(sBranchSelectorField, sizeof(sBranchSelectorField), i);
			__xlCgenAppendFormat(&pCtx->buf, "\tint %s;\n", sBranchSelectorField);
		}
	}
	__xlCgenAppendFormat(&pCtx->buf, "} %s;\n\n", sStateTypeName);
	for ( i = 0; i < pCandidate->iStepCount; ++i ) {
		__xlCgenAsyncLinearResumeCallbackName(sResumeCallbackName, sizeof(sResumeCallbackName), sResumeFuncName, i);
		__xlCgenAppendFormat(&pCtx->buf, "static void %s(const xfuture_result* pIn, ptr pArg);\n", sResumeCallbackName);
	}
	__xlCgenAppend(&pCtx->buf, "\n");

	for ( i = 0; i < pCandidate->iStepCount; ++i ) {
		const XL_CgenAsyncLinearResumeStep* pStep = &pCandidate->arrSteps[i];
		const char* sAwaitTraceName = __xlCallCalleeName(pStep->pAwaitInner);
		int iResumeStateId = (pFuncInfo->pAsyncFunc != NULL) ? __xlCgenFindAsyncStateId(pFuncInfo->pAsyncFunc, __xlCgenAsyncLinearStepAnchorNode(pCandidate, pStep), "resume") : -1;
		char sAwaitResultName[96];
		char sAfterStepLabel[96];
		const XL_MirNode* pInnerTryStmt;
		const XL_MirNode* pCatchName;
		const XL_MirNode* pCatchBlock;
		const XL_MirNode* pFinallyBlock;
		size_t iInnerTryStmtIndex;
		char sBranchSelectorField[64];
		int bDynamicBranchStep;
		int bDynamicAwaitOnTrue;
		int bDualDynamicBranchStep;
		int bTryCatchStep;
		int bTryFinallyStep;
		int bStructuredFailureStep;
		const char* sAltAwaitTraceName;

		if ( (sAwaitTraceName == NULL) || (sAwaitTraceName[0] == '\0') ) {
			sAwaitTraceName = ((pStep->pAwaitBind != NULL) && (pStep->pAwaitBind->sText != NULL)) ? pStep->pAwaitBind->sText : "await";
		}
		sAltAwaitTraceName = __xlCallCalleeName(pStep->pAwaitInnerAlt);
		if ( (sAltAwaitTraceName == NULL) || (sAltAwaitTraceName[0] == '\0') ) {
			sAltAwaitTraceName = sAwaitTraceName;
		}
		__xlCgenAsyncLinearStepResultName(sAwaitResultName, sizeof(sAwaitResultName), pStep);
		snprintf(sAfterStepLabel, sizeof(sAfterStepLabel), "xl_async_after_step_%zu", i);
		bDualDynamicBranchStep = 0;
		bDynamicBranchStep = __xlAnalyzeAsyncLinearStepBranchSelector(
			pCtx,
			pStep,
			&bDynamicAwaitOnTrue,
			&bDualDynamicBranchStep
		);
		__xlCgenAsyncLinearBranchSelectorFieldName(sBranchSelectorField, sizeof(sBranchSelectorField), i);
		bTryCatchStep = __xlCanAsyncLinearResolveTryCatchStmtEx(
			pCtx,
			pStep->pAwaitStmt,
			pStep->pAwaitNode,
			pStep->pAwaitNodeAlt,
			&pInnerTryStmt,
			&pCatchName,
			&pCatchBlock,
			&pFinallyBlock,
			&iInnerTryStmtIndex
		);
		bTryFinallyStep = __xlCanAsyncLinearResolveTryFinallyStmtEx(
			pCtx,
			pStep->pAwaitStmt,
			pStep->pAwaitNode,
			pStep->pAwaitNodeAlt,
			&pInnerTryStmt,
			&pFinallyBlock,
			&iInnerTryStmtIndex
		);
		bStructuredFailureStep = bTryCatchStep || bTryFinallyStep ||
			((pStep->pAwaitStmt != NULL) &&
			 ((pStep->pAwaitStmt->eKind == XL_MIR_BRANCH) || (pStep->pAwaitStmt->eKind == XL_MIR_BLOCK)));
		__xlCgenAsyncLinearResumeCallbackName(sResumeCallbackName, sizeof(sResumeCallbackName), sResumeFuncName, i);
		__xlCgenAppendFormat(&pCtx->buf, "static void %s(const xfuture_result* pIn, ptr pArg)\n{\n", sResumeCallbackName);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s* pState = (%s*)pArg;\n", sStateTypeName, sStateTypeName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( pState == NULL ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return;\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "pState->iStep = %d;\n", (int)i);
		if ( bDynamicBranchStep ) {
			if ( bDualDynamicBranchStep ) {
				if ( iResumeStateId >= 0 ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
					__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
					__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iResumeStateId);
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "if ( pState->%s == %d ) {\n", sBranchSelectorField, bDynamicAwaitOnTrue ? 1 : 0);
				pCtx->buf.iIndent += 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_event(\"resume\", ");
				__xlCgenAppendCString(&pCtx->buf, sAwaitTraceName);
				__xlCgenAppend(&pCtx->buf, ");\n");
				pCtx->buf.iIndent -= 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "}\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "else\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "{\n");
				pCtx->buf.iIndent += 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_event(\"resume\", ");
				__xlCgenAppendCString(&pCtx->buf, sAltAwaitTraceName);
				__xlCgenAppend(&pCtx->buf, ");\n");
				pCtx->buf.iIndent -= 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "}\n");
			} else {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "if ( pState->%s == %d ) {\n", sBranchSelectorField, bDynamicAwaitOnTrue ? 1 : 0);
				pCtx->buf.iIndent += 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_event(\"resume\", ");
				__xlCgenAppendCString(&pCtx->buf, sAwaitTraceName);
				__xlCgenAppend(&pCtx->buf, ");\n");
				if ( iResumeStateId >= 0 ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
					__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
					__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iResumeStateId);
				}
				pCtx->buf.iIndent -= 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "}\n");
			}
		} else {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_event(\"resume\", ");
			__xlCgenAppendCString(&pCtx->buf, sAwaitTraceName);
			__xlCgenAppend(&pCtx->buf, ");\n");
			if ( iResumeStateId >= 0 ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iResumeStateId);
			}
		}
		if ( bStructuredFailureStep ) {
			__xlCgenAsyncLinearRestoreScope(pCtx, pParams, pCandidate, i, "pState");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAsyncLinearHandleFailureFromResultForStmt(
				pCtx,
				pStep->pAwaitStmt,
				pStep->pAwaitNode,
				pStep->pAwaitNodeAlt,
				"pIn"
			);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( xl_generated_async_has_pending_error() ) {\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAsyncLinearRejectCurrentAndCleanup(
				pCtx,
				pParams,
				pCandidate,
				"pState",
				bTryCatchStep
					? "async linear try/catch failed"
					: (bTryFinallyStep ? "async linear try/finally failed" : "async linear stmt failed")
			);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", sAfterStepLabel);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		} else {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( (pIn == NULL) || (pIn->iStatus != XRT_NET_OK) ) {\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAsyncLinearRejectResultAndCleanup(pCtx, pParams, pCandidate, "pState", "pIn", "await failed");
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		}

		if ( !bStructuredFailureStep ) {
			__xlCgenAsyncLinearRestoreScope(pCtx, pParams, pCandidate, i, "pState");
		}
		if ( (pStep->pAwaitBind != NULL) && !pStep->bExprAwait ) {
			__xlCgenAsyncLinearAwaitValueDecl(pCtx, pStep->pAwaitBind);
		} else if ( (pStep->pAwaitBind != NULL) && pStep->bExprAwait ) {
			__xlCgenAsyncLinearAwaitValueDeclNamed(pCtx, __xlCgenAsyncLinearStepResultTypeName(pStep), sAwaitResultName);
			__xlCgenAsyncLinearBindResolve(pCtx, pStep->pAwaitBind, "pState", pStep->pAwaitNode, sAwaitResultName);
		} else if ( pStep->pAwaitStmt != NULL ) {
			__xlCgenAsyncLinearAwaitValueDeclNamed(pCtx, __xlCgenAsyncLinearStepResultTypeName(pStep), sAwaitResultName);
			__xlCgenAsyncLinearStmtResolve(pCtx, pStep->pAwaitStmt, pParams, pCandidate, i, "pState", pStep->pAwaitNode, sAwaitResultName, sResumeCallbackName);
		} else {
			__xlCgenAsyncLinearAwaitValueDeclNamed(pCtx, __xlCgenAsyncLinearStepResultTypeName(pStep), sAwaitResultName);
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sAfterStepLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");

		pPrevFuncInfo = pCtx->pCurrentFuncInfo;
		pCtx->pCurrentFuncInfo = pFuncInfo;
		for ( j = 0; j < pStep->iPostBindCount; ++j ) {
			__xlCgenAsyncLinearPostBind(pCtx, pStep->arrPostBinds[j], "pState");
		}
		for ( j = 0; j < pStep->iPostStmtCount; ++j ) {
			__xlCgenAsyncLinearPostStmt(pCtx, pStep->arrPostStmts[j], pFuncInfo);
		}
		if ( (i + 1) < pCandidate->iStepCount ) {
			const XL_CgenAsyncLinearResumeStep* pNextStep = &pCandidate->arrSteps[i + 1];
			const char* sNextAwaitExprType = __xlInferExprTypeName(pCtx, pNextStep->pAwaitInner);
			const char* sNextAwaitTraceName = __xlCallCalleeName(pNextStep->pAwaitInner);
			int iAwaitStateId = (pFuncInfo->pAsyncFunc != NULL) ? __xlCgenFindAsyncStateId(pFuncInfo->pAsyncFunc, __xlCgenAsyncLinearStepAnchorNode(pCandidate, pNextStep), "await") : -1;
			int bNextDynamicBranch;
			int bNextAwaitOnTrue;
			int bNextDualDynamicBranch;
			char sFutureValueName[64];
			char sHookName[64];
			char sBranchSelectorField[64];
			const char* sNextAltAwaitTraceName;

			if ( (sNextAwaitTraceName == NULL) || (sNextAwaitTraceName[0] == '\0') ) {
				if ( (pNextStep->pAwaitBind != NULL) && (pNextStep->pAwaitBind->sText != NULL) ) {
					sNextAwaitTraceName = pNextStep->pAwaitBind->sText;
				} else if ( (pNextStep->pAwaitStmt != NULL) && (pNextStep->pAwaitStmt->sText != NULL) && (pNextStep->pAwaitStmt->sText[0] != '\0') ) {
					sNextAwaitTraceName = pNextStep->pAwaitStmt->sText;
				} else {
					sNextAwaitTraceName = "await";
				}
			}
			sNextAltAwaitTraceName = __xlCallCalleeName(pNextStep->pAwaitInnerAlt);
			if ( (sNextAltAwaitTraceName == NULL) || (sNextAltAwaitTraceName[0] == '\0') ) {
				sNextAltAwaitTraceName = sNextAwaitTraceName;
			}
			bNextDualDynamicBranch = 0;
			bNextDynamicBranch = __xlAnalyzeAsyncLinearStepBranchSelector(
				pCtx,
				pNextStep,
				&bNextAwaitOnTrue,
				&bNextDualDynamicBranch
			);
			__xlCgenAsyncLinearBranchSelectorFieldName(sBranchSelectorField, sizeof(sBranchSelectorField), i + 1);
			if ( pNextStep->pAwaitStmt != NULL ) {
				if ( bNextDynamicBranch ) {
					snprintf(sFutureValueName, sizeof(sFutureValueName), "xl_async_future_%zu", pNextStep->iAwaitStmtIndex);
					(void)__xlCgenAsyncLinearEmitDynamicAwaitStep(
						pCtx,
						pNextStep->pAwaitStmt,
						pNextStep->pAwaitNode,
						pNextStep->pAwaitNodeAlt,
						sNextAwaitExprType,
						sFutureValueName,
						i + 1,
						"pState",
						1
					);
				} else {
					__xlCgenAsyncLinearStmtPrelude(pCtx, pNextStep->pAwaitStmt, pNextStep->pAwaitNode, 1, pParams, pCandidate, i + 1, "pState");
				}
			}
			__xlCgenAsyncLinearStoreScope(pCtx, pParams, pCandidate, i + 1, "pState");
			if ( !bNextDynamicBranch ) {
				snprintf(sFutureValueName, sizeof(sFutureValueName), "xl_async_future_%zu", pNextStep->iAwaitStmtIndex);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sNextAwaitExprType), sFutureValueName);
				__xlCgenExprAsType(pCtx, pNextStep->pAwaitInner, sNextAwaitExprType);
				__xlCgenAppend(&pCtx->buf, ";\n");
				__xlCgenAsyncLinearPendingErrorCheck(pCtx, "pState", "async linear await failed");
			}
			snprintf(sHookName, sizeof(sHookName), "xl_async_hook_%zu", pNextStep->iAwaitStmtIndex);
			__xlCgenAsyncLinearResumeCallbackName(sNextResumeCallbackName, sizeof(sNextResumeCallbackName), sResumeFuncName, i + 1);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( %s.handle == NULL ) {\n", sFutureValueName);
			pCtx->buf.iIndent += 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, \"async await future missing\");\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( pState->pPromise != NULL ) {\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xPromiseDestroy(pState->pPromise);\n");
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			__xlCgenAsyncLinearStateCleanup(pCtx, pParams, pCandidate, "pState");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xrtFree(pState);\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "return;\n");
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			if ( bNextDynamicBranch && bNextDualDynamicBranch ) {
				if ( iAwaitStateId >= 0 ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
					__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
					__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "if ( pState->%s == %d ) {\n", sBranchSelectorField, bNextAwaitOnTrue ? 1 : 0);
				pCtx->buf.iIndent += 1;
				__xlCgenTraceEventStmt(pCtx, "await", sNextAwaitTraceName);
				pCtx->buf.iIndent -= 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "}\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "else\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "{\n");
				pCtx->buf.iIndent += 1;
				__xlCgenTraceEventStmt(pCtx, "await", sNextAltAwaitTraceName);
				pCtx->buf.iIndent -= 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "}\n");
			} else if ( bNextDynamicBranch ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "if ( pState->%s == %d ) {\n", sBranchSelectorField, bNextAwaitOnTrue ? 1 : 0);
				pCtx->buf.iIndent += 1;
				if ( iAwaitStateId >= 0 ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
					__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
					__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
				}
				__xlCgenTraceEventStmt(pCtx, "await", sNextAwaitTraceName);
				pCtx->buf.iIndent -= 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "}\n");
			} else {
				if ( iAwaitStateId >= 0 ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
					__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
					__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
				}
				__xlCgenTraceEventStmt(pCtx, "await", sNextAwaitTraceName);
			}
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "xfuture* %s = xFutureFinallyEngine(%s.handle, NULL, 0u, %s, pState);\n", sHookName, sFutureValueName, sNextResumeCallbackName);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sHookName);
			pCtx->buf.iIndent += 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "(void)xl_generated_promise_reject_text(pState->pPromise, XRT_NET_ERROR, \"async linear hook failed\");\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( pState->pPromise != NULL ) {\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xPromiseDestroy(pState->pPromise);\n");
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			__xlCgenAsyncLinearStateCleanup(pCtx, pParams, pCandidate, "pState");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xrtFree(pState);\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "return;\n");
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sHookName);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "return;\n");
		} else {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_event(\"return\", ");
			__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
			__xlCgenAppend(&pCtx->buf, ");\n");
			__xlCgenAsyncLinearReturnResolve(
				pCtx,
				pFuncInfo,
				pCandidate->pReturn,
				"pState",
				pStep->bReturnExprAwait ? pStep->pAwaitNode : NULL,
				pStep->bReturnExprAwait ? sAwaitResultName : NULL
			);
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( pState->pPromise != NULL ) {\n");
			pCtx->buf.iIndent += 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xPromiseDestroy(pState->pPromise);\n");
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			__xlCgenAsyncLinearStateCleanup(pCtx, pParams, pCandidate, "pState");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xrtFree(pState);\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "return;\n");
		}
		pCtx->pCurrentFuncInfo = pPrevFuncInfo;
		pCtx->buf.iIndent -= 1;
		__xlCgenAppend(&pCtx->buf, "}\n\n");
	}
}

static int __xlCgenEmitAsyncLinearResumeFunc(
	XL_CgenContext* pCtx,
	const XL_MirNode* pParams,
	const XL_MirNode* pBody,
	const XL_CgenFuncInfo* pFuncInfo
)
{
	XL_CgenAsyncLinearResumeCandidate candidate;
	char sStateTypeName[320];
	char sResumeFuncName[320];
	char sFutureValueName[64];
	char sOutFutureName[64];
	char sPromiseName[64];
	char sStateName[64];
	char sHookName[64];
	char sFirstResumeCallbackName[320];
	char sDoWhileBreakLabel[64];
	char sDoWhileContinueLabel[64];
	char sFirstBranchSelectorField[64];
	char sFirstBranchSelectorLocal[64];
	const char* sAwaitExprType;
	const char* sAwaitTraceName;
	const char* sAwaitTraceNameAlt;
	int iAwaitStateId;
	const XL_CgenAsyncLinearResumeStep* pFirstStep;
	const XL_MirNode* pDoWhileBody;
	int bFirstDoWhile;
	int bFirstDynamicBranch;
	int bFirstAwaitOnTrue;
	int bFirstDualDynamicBranch;
	size_t i;

	if ( (pCtx == NULL) || (pFuncInfo == NULL) ) {
		return 0;
	}
	if ( !__xlCollectAsyncLinearResumeCandidate(pCtx, pBody, &candidate) ) {
		return 0;
	}
	if ( __xlCgenCurrentTryFrame(pCtx) != NULL || __xlCgenCurrentFinallyFrame(pCtx) != NULL ) {
		return 0;
	}
	if ( candidate.iStepCount == 0 ) {
		return 0;
	}
	pFirstStep = &candidate.arrSteps[0];

	snprintf(sStateTypeName, sizeof(sStateTypeName), "%s__async_linear_state", pFuncInfo->sSymbolName);
	snprintf(sResumeFuncName, sizeof(sResumeFuncName), "%s__async_linear_resume", pFuncInfo->sSymbolName);
	snprintf(sFutureValueName, sizeof(sFutureValueName), "xl_async_future_%zu", pFirstStep->iAwaitStmtIndex);
	snprintf(sOutFutureName, sizeof(sOutFutureName), "xl_async_out_%zu", pFirstStep->iAwaitStmtIndex);
	snprintf(sPromiseName, sizeof(sPromiseName), "xl_async_promise_%zu", pFirstStep->iAwaitStmtIndex);
	snprintf(sStateName, sizeof(sStateName), "xl_async_state_%zu", pFirstStep->iAwaitStmtIndex);
	snprintf(sHookName, sizeof(sHookName), "xl_async_hook_%zu", pFirstStep->iAwaitStmtIndex);
	__xlCgenAsyncLinearResumeCallbackName(sFirstResumeCallbackName, sizeof(sFirstResumeCallbackName), sResumeFuncName, 0);
	bFirstDoWhile = (pFirstStep->pAwaitStmt != NULL) &&
		(pFirstStep->pAwaitStmt->eKind == XL_MIR_WHILE) &&
		pFirstStep->pAwaitStmt->bFlag;
	bFirstDualDynamicBranch = 0;
	bFirstDynamicBranch = __xlAnalyzeAsyncLinearStepBranchSelector(
		pCtx,
		pFirstStep,
		&bFirstAwaitOnTrue,
		&bFirstDualDynamicBranch
	);
	pDoWhileBody = bFirstDoWhile && (pFirstStep->pAwaitStmt->iChildCount > 1) ? pFirstStep->pAwaitStmt->ppChildren[1] : NULL;
	snprintf(sDoWhileBreakLabel, sizeof(sDoWhileBreakLabel), "xl_async_do_while_break_%d_%d", (pFirstStep->pAwaitStmt != NULL) ? pFirstStep->pAwaitStmt->iLine : 0, (pFirstStep->pAwaitStmt != NULL) ? pFirstStep->pAwaitStmt->iColumn : 0);
	snprintf(sDoWhileContinueLabel, sizeof(sDoWhileContinueLabel), "xl_async_do_while_continue_%d_%d", (pFirstStep->pAwaitStmt != NULL) ? pFirstStep->pAwaitStmt->iLine : 0, (pFirstStep->pAwaitStmt != NULL) ? pFirstStep->pAwaitStmt->iColumn : 0);
	__xlCgenAsyncLinearBranchSelectorFieldName(sFirstBranchSelectorField, sizeof(sFirstBranchSelectorField), 0);
	__xlCgenAsyncLinearBranchSelectorLocalName(sFirstBranchSelectorLocal, sizeof(sFirstBranchSelectorLocal), 0);

	sAwaitExprType = __xlInferExprTypeName(pCtx, pFirstStep->pAwaitInner);
	sAwaitTraceName = __xlCallCalleeName(pFirstStep->pAwaitInner);
	if ( (sAwaitTraceName == NULL) || (sAwaitTraceName[0] == '\0') ) {
		if ( (pFirstStep->pAwaitBind != NULL) && (pFirstStep->pAwaitBind->sText != NULL) ) {
			sAwaitTraceName = pFirstStep->pAwaitBind->sText;
		} else if ( (pFirstStep->pAwaitStmt != NULL) && (pFirstStep->pAwaitStmt->sText != NULL) && (pFirstStep->pAwaitStmt->sText[0] != '\0') ) {
			sAwaitTraceName = pFirstStep->pAwaitStmt->sText;
		} else {
			sAwaitTraceName = "await";
		}
	}
	sAwaitTraceNameAlt = __xlCallCalleeName(pFirstStep->pAwaitInnerAlt);
	if ( (sAwaitTraceNameAlt == NULL) || (sAwaitTraceNameAlt[0] == '\0') ) {
		sAwaitTraceNameAlt = sAwaitTraceName;
	}
	iAwaitStateId = (pFuncInfo->pAsyncFunc != NULL) ? __xlCgenFindAsyncStateId(pFuncInfo->pAsyncFunc, __xlCgenAsyncLinearStepAnchorNode(&candidate, pFirstStep), "await") : -1;

	for ( i = 0; i < candidate.iPreBindCount; ++i ) {
		__xlCgenStmt(pCtx, candidate.arrPreBinds[i], pFuncInfo);
	}
	if ( bFirstDoWhile ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xfuture* %s = xFutureCreate();\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xpromise* %s;\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s* %s;\n", sStateTypeName, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xfuture* %s;\n", sHookName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sOutFutureName);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear future alloc failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "(void)xFutureSetDebugName(%s, (str)", sOutFutureName);
		__xlCgenAppendCString(&pCtx->buf, __xlFutureTypeDebugName(pFuncInfo->sTypeName));
		__xlCgenAppend(&pCtx->buf, ");\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = xPromiseCreate(%s);\n", sPromiseName, sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sPromiseName);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear promise alloc failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = (%s*)xrtMalloc(sizeof(*%s));\n", sStateName, sStateTypeName, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sStateName);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear state alloc failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "memset(%s, 0, sizeof(*%s));\n", sStateName, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s->pPromise = %s;\n", sStateName, sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s->iStep = 0;\n", sStateName);

		if ( pDoWhileBody != NULL ) {
			__xlCgenPushLoopFrame(pCtx, sDoWhileBreakLabel, sDoWhileContinueLabel);
			__xlCgenStmt(pCtx, pDoWhileBody, pFuncInfo);
			__xlCgenPopLoopFrame(pCtx);
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_had_exception ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_capture_exception_error());\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_capture_native_error() ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_capture_exception_error());\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sDoWhileContinueLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			__xlCgenAsyncLinearStateFieldStore(pCtx, pParams->ppChildren[i], sStateName);
		}
		for ( i = 0; i < candidate.iPreBindCount; ++i ) {
			__xlCgenAsyncLinearStateFieldStore(pCtx, candidate.arrPreBinds[i], sStateName);
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sAwaitExprType), sFutureValueName);
		__xlCgenExprAsType(pCtx, pFirstStep->pAwaitInner, sAwaitExprType);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_had_exception ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearStateCleanup(pCtx, pParams, &candidate, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_capture_exception_error());\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( xl_generated_capture_native_error() ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearStateCleanup(pCtx, pParams, &candidate, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_capture_exception_error());\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s.handle == NULL ) {\n", sFutureValueName);
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearStateCleanup(pCtx, pParams, &candidate, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async await future missing\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		if ( iAwaitStateId >= 0 ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
			__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
			__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
		}
		__xlCgenTraceEventStmt(pCtx, "await", sAwaitTraceName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s = xFutureFinallyEngine(%s.handle, NULL, 0u, %s, %s);\n", sHookName, sFutureValueName, sResumeFuncName, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sHookName);
		pCtx->buf.iIndent += 1;
		__xlCgenAsyncLinearStateCleanup(pCtx, pParams, &candidate, sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear hook failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sHookName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return (%s){ %s };\n", __xlFutureTypeName(pFuncInfo->sTypeName), sOutFutureName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sDoWhileBreakLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			__xlCgenAsyncLinearStateFieldStore(pCtx, pParams->ppChildren[i], sStateName);
		}
		for ( i = 0; i < candidate.iPreBindCount; ++i ) {
			__xlCgenAsyncLinearStateFieldStore(pCtx, candidate.arrPreBinds[i], sStateName);
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xfuture_result xl_async_break_result = { XRT_NET_OK, NULL, NULL, XFUTURE_RESULT_F_NONE };\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s(&xl_async_break_result, %s);\n", sFirstResumeCallbackName, sStateName);
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return (%s){ %s };\n", __xlFutureTypeName(pFuncInfo->sTypeName), sOutFutureName);
		return 1;
	}

	if ( pFirstStep->pAwaitStmt != NULL ) {
		if ( bFirstDynamicBranch ) {
			(void)__xlCgenAsyncLinearEmitDynamicAwaitStep(
				pCtx,
				pFirstStep->pAwaitStmt,
				pFirstStep->pAwaitNode,
				pFirstStep->pAwaitNodeAlt,
				sAwaitExprType,
				sFutureValueName,
				0,
				NULL,
				0
			);
		} else {
			__xlCgenAsyncLinearStmtPrelude(pCtx, pFirstStep->pAwaitStmt, pFirstStep->pAwaitNode, 0, pParams, &candidate, 0, NULL);
		}
		__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	}
	if ( !bFirstDynamicBranch ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlCTypeName(sAwaitExprType), sFutureValueName);
		__xlCgenExprAsType(pCtx, pFirstStep->pAwaitInner, sAwaitExprType);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s.handle == NULL ) {\n", sFutureValueName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async await future missing\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	if ( bFirstDynamicBranch ) {
		if ( bFirstDualDynamicBranch ) {
			if ( iAwaitStateId >= 0 ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
			}
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( %s == %d ) {\n", sFirstBranchSelectorLocal, bFirstAwaitOnTrue ? 1 : 0);
			pCtx->buf.iIndent += 1;
			__xlCgenTraceEventStmt(pCtx, "await", sAwaitTraceName);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "else\n");
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "{\n");
			pCtx->buf.iIndent += 1;
			__xlCgenTraceEventStmt(pCtx, "await", sAwaitTraceNameAlt);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		} else {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "if ( %s == %d ) {\n", sFirstBranchSelectorLocal, bFirstAwaitOnTrue ? 1 : 0);
			pCtx->buf.iIndent += 1;
			if ( iAwaitStateId >= 0 ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
			}
			__xlCgenTraceEventStmt(pCtx, "await", sAwaitTraceName);
			pCtx->buf.iIndent -= 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "}\n");
		}
	} else {
		if ( iAwaitStateId >= 0 ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "xl_generated_async_trace_state_event(");
			__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
			__xlCgenAppendFormat(&pCtx->buf, ", %d);\n", iAwaitStateId);
		}
		__xlCgenTraceEventStmt(pCtx, "await", sAwaitTraceName);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xfuture* %s = xFutureCreate();\n", sOutFutureName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xpromise* %s;\n", sPromiseName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s* %s;\n", sStateTypeName, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xfuture* %s;\n", sHookName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sOutFutureName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear future alloc failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "(void)xFutureSetDebugName(%s, (str)", sOutFutureName);
	__xlCgenAppendCString(&pCtx->buf, __xlFutureTypeDebugName(pFuncInfo->sTypeName));
	__xlCgenAppend(&pCtx->buf, ");\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = xPromiseCreate(%s);\n", sPromiseName, sOutFutureName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sPromiseName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear promise alloc failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = (%s*)xrtMalloc(sizeof(*%s));\n", sStateName, sStateTypeName, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sStateName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear state alloc failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "memset(%s, 0, sizeof(*%s));\n", sStateName, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s->pPromise = %s;\n", sStateName, sPromiseName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s->iStep = 0;\n", sStateName);
	if ( bFirstDynamicBranch ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s->%s = %s;\n", sStateName, sFirstBranchSelectorField, sFirstBranchSelectorLocal);
	}
	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		__xlCgenAsyncLinearStateFieldStore(pCtx, pParams->ppChildren[i], sStateName);
	}
	for ( i = 0; i < candidate.iPreBindCount; ++i ) {
		__xlCgenAsyncLinearStateFieldStore(pCtx, candidate.arrPreBinds[i], sStateName);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = xFutureFinallyEngine(%s.handle, NULL, 0u, %s, %s);\n", sHookName, sFutureValueName, sResumeFuncName, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s == NULL ) {\n", sHookName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAsyncLinearStateCleanup(pCtx, pParams, &candidate, sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xrtFree(%s);\n", sStateName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xPromiseDestroy(%s);\n", sPromiseName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sOutFutureName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_generated_error_make(\"Error\", \"async linear hook failed\", 0, \"\", \"\"));\n", __xlFutureErrorFunc(pFuncInfo->sTypeName));
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xFutureRelease(%s);\n", sHookName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "return (%s){ %s };\n", __xlFutureTypeName(pFuncInfo->sTypeName), sOutFutureName);
	return 1;
}

static void __xlCgenFunc(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pParams;
	const XL_MirNode* pBody;
	const XL_MirNode* pParam;
	const XL_CgenFuncInfo* pPrevFuncInfo;
	XL_CgenFuncInfo funcInfo;
	XL_CgenGeneratorField arrGeneratorFields[128];
	XL_CgenGeneratorTryFrameField arrGeneratorTryFrameFields[64];
	char sGeneratorVarName[64];
	char sGeneratorDoneLabel[64];
	char sGeneratorTypeName[256];
	char sGeneratorNextFuncName[256];
	char sEscapeKindName[64];
	char sEscapeLevelName[64];
	char sEscapeValueName[64];
	const XL_MirNode* pWrapperGeneratorFunc;
	char sWrapperGeneratorTypeName[256];
	char sWrapperReturnDefaultValue[320];
	XL_CgenAsyncLinearResumeCandidate asyncLinearCandidate;
	char sAsyncLinearStateTypeName[320];
	char sAsyncLinearResumeFuncName[320];
	int bHasAsyncLinearCandidate;
	int iGeneratorFieldCount;
	int iGeneratorTryFrameFieldCount;
	int iGeneratorYieldCount;
	int iGeneratorSuspendIndex;
	size_t i;

	memset(&funcInfo, 0, sizeof(funcInfo));
	funcInfo.sName = (pNode->sText != NULL) ? pNode->sText : "anon";
	funcInfo.sSymbolName = (__xlMirFuncSymbolName(pNode) != NULL) ? __xlMirFuncSymbolName(pNode) : funcInfo.sName;
	funcInfo.sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "void";
	if ( pNode->bFlag ) {
		funcInfo.sTypeName = __xlFutureValueTypeName(funcInfo.sTypeName);
	}
	funcInfo.sReturnCTypeName = __xlCTypeName(funcInfo.sTypeName);
	funcInfo.sReturnDefaultValue = __xlCDefaultReturnValue(funcInfo.sTypeName);
	funcInfo.bAsync = pNode->bFlag;
	funcInfo.bGenerator = __xlIsGeneratorFuncNode(pNode);
	snprintf(sGeneratorVarName, sizeof(sGeneratorVarName), "xl_gen_result_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sGeneratorDoneLabel, sizeof(sGeneratorDoneLabel), "xl_gen_done_%d_%d", pNode->iLine, pNode->iColumn);
	__xlCgenGeneratorTypeNameForFunc(sGeneratorTypeName, sizeof(sGeneratorTypeName), pNode);
	__xlCgenGeneratorNextFuncNameForFunc(sGeneratorNextFuncName, sizeof(sGeneratorNextFuncName), pNode);
	snprintf(sEscapeKindName, sizeof(sEscapeKindName), "xl_escape_kind_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sEscapeLevelName, sizeof(sEscapeLevelName), "xl_escape_level_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sEscapeValueName, sizeof(sEscapeValueName), "xl_escape_value_%d_%d", pNode->iLine, pNode->iColumn);
	iGeneratorFieldCount = __xlCgenCollectGeneratorFields(pNode, arrGeneratorFields, (int)XL_COUNTOF(arrGeneratorFields));
	iGeneratorTryFrameFieldCount = __xlCgenCollectGeneratorTryFrameFields(pNode, arrGeneratorTryFrameFields, (int)XL_COUNTOF(arrGeneratorTryFrameFields));
	iGeneratorYieldCount = __xlMirCountKind((pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, XL_MIR_YIELD);
	iGeneratorSuspendIndex = 0;
	pPrevFuncInfo = pCtx->pCurrentFuncInfo;
	funcInfo.sGeneratorTypeName = sGeneratorTypeName;
	funcInfo.sGeneratorNextFuncName = sGeneratorNextFuncName;
	funcInfo.sGeneratorSelfName = "pGen";
	funcInfo.sGeneratorValueOutName = "pValue";
	funcInfo.sGeneratorVarName = sGeneratorVarName;
	funcInfo.sGeneratorDoneLabel = sGeneratorDoneLabel;
	funcInfo.sEscapeKindName = sEscapeKindName;
	funcInfo.sEscapeLevelName = sEscapeLevelName;
	funcInfo.sEscapeValueName = sEscapeValueName;
	funcInfo.pAsyncFunc = __xlCgenFindAsyncFunc(pCtx, pNode);
	funcInfo.pGeneratorFunc = __xlCgenFindGeneratorFunc(pCtx, pNode);
	funcInfo.pHybridFunc = __xlCgenFindHybridFunc(pCtx, pNode);
	if ( (funcInfo.pHybridFunc != NULL) &&
		((__xlCgenCountHybridYieldPoints(funcInfo.pHybridFunc) != iGeneratorYieldCount) ||
		!__xlCgenIsHybridProjectionComplete(funcInfo.pHybridFunc)) ) {
		funcInfo.pHybridFunc = NULL;
	}
	funcInfo.pGeneratorSuspendIndex = &iGeneratorSuspendIndex;

	pParams = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pBody = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	memset(&asyncLinearCandidate, 0, sizeof(asyncLinearCandidate));
	sAsyncLinearStateTypeName[0] = '\0';
	sAsyncLinearResumeFuncName[0] = '\0';
	bHasAsyncLinearCandidate = funcInfo.bAsync && !funcInfo.bGenerator && __xlCollectAsyncLinearResumeCandidate(pCtx, pBody, &asyncLinearCandidate);
	if ( bHasAsyncLinearCandidate ) {
		snprintf(sAsyncLinearStateTypeName, sizeof(sAsyncLinearStateTypeName), "%s__async_linear_state", funcInfo.sSymbolName);
		snprintf(sAsyncLinearResumeFuncName, sizeof(sAsyncLinearResumeFuncName), "%s__async_linear_resume", funcInfo.sSymbolName);
		__xlCgenEmitAsyncLinearResumeHelper(pCtx, pParams, &funcInfo, &asyncLinearCandidate, sAsyncLinearStateTypeName, sAsyncLinearResumeFuncName);
	}
	pWrapperGeneratorFunc = NULL;
	sWrapperGeneratorTypeName[0] = '\0';
	sWrapperReturnDefaultValue[0] = '\0';
	if ( !funcInfo.bAsync && !funcInfo.bGenerator &&
		(funcInfo.sTypeName != NULL) &&
		(strcmp(funcInfo.sTypeName, "generator") == 0) &&
		__xlResolveGeneratorReturnSourceInfo(
			pCtx,
			pNode,
			&pWrapperGeneratorFunc,
			NULL,
			NULL,
			0
		) ) {
		__xlCgenGeneratorTypeNameForFunc(sWrapperGeneratorTypeName, sizeof(sWrapperGeneratorTypeName), pWrapperGeneratorFunc);
		funcInfo.sReturnCTypeName = sWrapperGeneratorTypeName;
		snprintf(sWrapperReturnDefaultValue, sizeof(sWrapperReturnDefaultValue), "(%s){0}", sWrapperGeneratorTypeName);
		funcInfo.sReturnDefaultValue = sWrapperReturnDefaultValue;
	}

	if ( __xlTaskHelperKind(funcInfo.sName) != XL_TASK_HELPER_NONE ) {
		return;
	}
	if ( __xlCgenEmitBuiltinHelperFunc(pCtx, pNode, &funcInfo) ) {
		return;
	}

	if ( __xlMirFuncHasExternTarget(pNode) && (pBody != NULL) && (pBody->eKind == XL_MIR_BLOCK) && (pBody->iChildCount == 0) ) {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"extern %s %s(",
			__xlCgenFuncReturnCTypeName(&funcInfo),
			pNode->sExternName
		);
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			pParam = pParams->ppChildren[i];
			if ( i != 0 ) {
				__xlCgenAppend(&pCtx->buf, ", ");
			}
			__xlCgenAppendFormat(
				&pCtx->buf,
				"%s %s",
				__xlCTypeName(((pParam->sTypeName != NULL) && (pParam->sTypeName[0] != '\0')) ? pParam->sTypeName : "int"),
				(pParam->sText != NULL) ? pParam->sText : "arg"
			);
		}
		__xlCgenAppend(&pCtx->buf, ");\n");
		__xlCgenAppendFormat(
			&pCtx->buf,
			"static %s %s(",
			__xlCgenFuncReturnCTypeName(&funcInfo),
			funcInfo.sSymbolName
		);
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			pParam = pParams->ppChildren[i];
			if ( i != 0 ) {
				__xlCgenAppend(&pCtx->buf, ", ");
			}
			__xlCgenAppendFormat(
				&pCtx->buf,
				"%s %s",
				__xlCTypeName(((pParam->sTypeName != NULL) && (pParam->sTypeName[0] != '\0')) ? pParam->sTypeName : "int"),
				(pParam->sText != NULL) ? pParam->sText : "arg"
			);
		}
		__xlCgenAppend(&pCtx->buf, ")\n{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		if ( (funcInfo.sTypeName == NULL) || (strcmp(funcInfo.sTypeName, "void") == 0) ) {
			__xlCgenAppendFormat(&pCtx->buf, "%s(", pNode->sExternName);
		} else {
			__xlCgenAppendFormat(&pCtx->buf, "return %s(", pNode->sExternName);
		}
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			pParam = pParams->ppChildren[i];
			if ( i != 0 ) {
				__xlCgenAppend(&pCtx->buf, ", ");
			}
			__xlCgenAppend(&pCtx->buf, (pParam->sText != NULL) ? pParam->sText : "arg");
		}
		__xlCgenAppend(&pCtx->buf, ");\n");
		if ( (funcInfo.sTypeName == NULL) || (strcmp(funcInfo.sTypeName, "void") == 0) ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "return;\n");
		}
		pCtx->buf.iIndent -= 1;
		__xlCgenAppend(&pCtx->buf, "}\n\n");
		return;
	}

	if ( funcInfo.bGenerator ) {
		__xlCgenAppendGeneratorField(arrGeneratorFields, (int)XL_COUNTOF(arrGeneratorFields), &iGeneratorFieldCount, sEscapeKindName, "int");
		__xlCgenAppendGeneratorField(arrGeneratorFields, (int)XL_COUNTOF(arrGeneratorFields), &iGeneratorFieldCount, sEscapeLevelName, "int");
		__xlCgenAppendFormat(&pCtx->buf, "typedef struct %s {\n", funcInfo.sGeneratorTypeName);
		__xlCgenAppend(&pCtx->buf, "\tXL_GeneratedGeneratorExecutor executor;\n");
		for ( i = 0; i < (size_t)iGeneratorFieldCount; ++i ) {
			__xlCgenAppendFormat(
				&pCtx->buf,
				"\t%s %s;\n",
				__xlCTypeName(arrGeneratorFields[i].sTypeName),
				arrGeneratorFields[i].sName
			);
		}
		for ( i = 0; i < (size_t)iGeneratorTryFrameFieldCount; ++i ) {
			__xlCgenAppendFormat(
				&pCtx->buf,
				"\tXL_GeneratedExceptionFrame %s;\n",
				arrGeneratorTryFrameFields[i].sName
			);
		}
		__xlCgenAppendFormat(&pCtx->buf, "} %s;\n\n", funcInfo.sGeneratorTypeName);

		__xlCgenAppendFormat(
			&pCtx->buf,
			"static int %s(const char* sName, %s* pGen, %s* pValue, xvalue xl_gen_resume_value)\n{\n",
			funcInfo.sGeneratorNextFuncName,
			funcInfo.sGeneratorTypeName,
			__xlGeneratorItemCType(funcInfo.sTypeName)
		);
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "(void)xl_gen_resume_value;\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( (pGen == NULL) || (pValue == NULL) ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return 0;\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "if ( !xl_generated_generator_executor_enter(&pGen->executor, (sName != NULL) ? sName : \"\") ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return 0;\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenGeneratorFieldAliasLines(pCtx, arrGeneratorFields, iGeneratorFieldCount, "pGen", 1);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "switch ( pGen->executor.state ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "case 0:\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "break;\n");
		pCtx->buf.iIndent -= 1;
		if ( (funcInfo.pHybridFunc != NULL) && (funcInfo.pHybridFunc->iPointCount != 0) ) {
			for ( i = 0; i < funcInfo.pHybridFunc->iPointCount; ++i ) {
				const XL_HybridSmPoint* pPoint = &funcInfo.pHybridFunc->pPoints[i];

				if ( !pPoint->bYield || (pPoint->iGeneratorStateId <= 0) ) {
					continue;
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"/* hybrid.group %d awaitCount=%d */\n",
					pPoint->iProjectionGroupId,
					__xlCgenCountHybridAwaitsInGroup(funcInfo.pHybridFunc, pPoint->iProjectionGroupId)
				);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "case %d:\n", pPoint->iGeneratorStateId);
				pCtx->buf.iIndent += 1;
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "goto xl_gen_resume_%d;\n", pPoint->iGeneratorStateId);
				pCtx->buf.iIndent -= 1;
			}
		} else {
			if ( (funcInfo.pGeneratorFunc != NULL) && (funcInfo.pGeneratorFunc->iStateCount != 0) ) {
				for ( i = 0; i < funcInfo.pGeneratorFunc->iStateCount; ++i ) {
					const XL_GeneratorSmState* pState = &funcInfo.pGeneratorFunc->pStates[i];

					if ( !pState->bSuspend || (pState->iStateId <= 0) ) {
						continue;
					}
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppendFormat(&pCtx->buf, "case %d:\n", pState->iStateId);
					pCtx->buf.iIndent += 1;
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppendFormat(&pCtx->buf, "goto xl_gen_resume_%d;\n", pState->iStateId);
					pCtx->buf.iIndent -= 1;
				}
			}
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "default:\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", funcInfo.sGeneratorDoneLabel);
		pCtx->buf.iIndent -= 1;
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		pCtx->pCurrentFuncInfo = &funcInfo;
		if ( (pBody != NULL) && (pBody->eKind == XL_MIR_BLOCK) ) {
			for ( i = 0; i < pBody->iChildCount; ++i ) {
				__xlCgenStmt(pCtx, pBody->ppChildren[i], &funcInfo);
			}
		} else {
			__xlCgenStmt(pCtx, pBody, &funcInfo);
		}
		pCtx->pCurrentFuncInfo = pPrevFuncInfo;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", funcInfo.sGeneratorDoneLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xl_generated_generator_executor_finish(&pGen->executor, (sName != NULL) ? sName : \"\");\n");
		__xlCgenGeneratorFieldAliasLines(pCtx, arrGeneratorFields, iGeneratorFieldCount, "pGen", 0);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return 0;\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppend(&pCtx->buf, "}\n\n");

		__xlCgenAppendFormat(
			&pCtx->buf,
			"static %s %s(",
			funcInfo.sGeneratorTypeName,
			funcInfo.sSymbolName
		);
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			pParam = pParams->ppChildren[i];
			if ( i != 0 ) {
				__xlCgenAppend(&pCtx->buf, ", ");
			}
			__xlCgenAppendFormat(
				&pCtx->buf,
				"%s %s",
				__xlCTypeName(((pParam->sTypeName != NULL) && (pParam->sTypeName[0] != '\0')) ? pParam->sTypeName : "int"),
				(pParam->sText != NULL) ? pParam->sText : "arg"
			);
		}
		__xlCgenAppend(&pCtx->buf, ")\n{\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s;\n", funcInfo.sGeneratorTypeName, funcInfo.sGeneratorVarName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "memset(&%s, 0, sizeof(%s));\n", funcInfo.sGeneratorVarName, funcInfo.sGeneratorVarName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "xl_generated_generator_executor_init(&%s.executor);\n", funcInfo.sGeneratorVarName);
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			pParam = pParams->ppChildren[i];
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(
				&pCtx->buf,
				"%s.%s = %s;\n",
				funcInfo.sGeneratorVarName,
				(pParam->sText != NULL) ? pParam->sText : "arg",
				(pParam->sText != NULL) ? pParam->sText : "arg"
			);
		}
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s;\n", funcInfo.sGeneratorVarName);
		pCtx->buf.iIndent -= 1;
		__xlCgenAppend(&pCtx->buf, "}\n\n");
		pCtx->pCurrentFuncInfo = pPrevFuncInfo;
		return;
	}

	__xlCgenAppendFormat(
		&pCtx->buf,
		"static %s %s(",
		(funcInfo.bAsync && !funcInfo.bGenerator) ? __xlFutureTypeName(funcInfo.sTypeName) : __xlCgenFuncReturnCTypeName(&funcInfo),
		funcInfo.sSymbolName
	);
	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		pParam = pParams->ppChildren[i];
		if ( i != 0 ) {
			__xlCgenAppend(&pCtx->buf, ", ");
		}
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s",
			__xlCTypeName(((pParam->sTypeName != NULL) && (pParam->sTypeName[0] != '\0')) ? pParam->sTypeName : "int"),
			(pParam->sText != NULL) ? pParam->sText : "arg"
		);
	}
	__xlCgenAppend(&pCtx->buf, ")\n");
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	pCtx->pCurrentFuncInfo = &funcInfo;

	if ( funcInfo.bAsync ) {
		__xlCgenTraceEventStmt(pCtx, "enter", funcInfo.sName);
	}
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", funcInfo.sEscapeKindName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", funcInfo.sEscapeLevelName);
	if ( (funcInfo.sTypeName != NULL) && (strcmp(funcInfo.sTypeName, "void") != 0) ) {
		const char* sEscapeDefaultValue;

		sEscapeDefaultValue = __xlCgenFuncReturnDefaultValue(&funcInfo);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s", __xlCgenFuncReturnCTypeName(&funcInfo), funcInfo.sEscapeValueName);
		if ( sEscapeDefaultValue != NULL ) {
			__xlCgenAppendFormat(&pCtx->buf, " = %s", sEscapeDefaultValue);
		}
		__xlCgenAppend(&pCtx->buf, ";\n");
	}

	if ( funcInfo.bAsync && !funcInfo.bGenerator && __xlCgenEmitAsyncLinearResumeFunc(pCtx, pParams, pBody, &funcInfo) ) {
		pCtx->pCurrentFuncInfo = pPrevFuncInfo;
		pCtx->buf.iIndent -= 1;
		__xlCgenAppend(&pCtx->buf, "}\n\n");
		return;
	}

	if ( (pBody != NULL) && (pBody->eKind == XL_MIR_BLOCK) ) {
		for ( i = 0; i < pBody->iChildCount; ++i ) {
			__xlCgenStmt(pCtx, pBody->ppChildren[i], &funcInfo);
		}
	} else {
		__xlCgenStmt(pCtx, pBody, &funcInfo);
	}

	if ( funcInfo.bAsync && !funcInfo.bGenerator ) {
		__xlCgenAsyncDefaultReturn(pCtx, &funcInfo);
	}

	pCtx->pCurrentFuncInfo = pPrevFuncInfo;
	pCtx->buf.iIndent -= 1;
	__xlCgenAppend(&pCtx->buf, "}\n\n");
}

static void __xlCgenFunctionValueThunkPrototype(XL_CgenContext* pCtx, const XL_MirNode* pFunc)
{
	char sThunkName[256];

	if ( (pCtx == NULL) || !__xlCanEmitFunctionValueThunk(pFunc) ) {
		return;
	}

	__xlCgenFunctionValueThunkName(sThunkName, sizeof(sThunkName), pFunc);
	__xlCgenAppendFormat(&pCtx->buf, "static xvalue %s(xvalue varENV, xvalue arrParam);\n", sThunkName);
}

static void __xlCgenFunctionValueThunk(XL_CgenContext* pCtx, const XL_MirNode* pFunc)
{
	const XL_MirNode* pParams;
	const XL_MirNode* pParam;
	const char* sReturnType;
	const char* sFuncSymbol;
	size_t i;
	char sThunkName[256];
	char sResultName[64];

	if ( (pCtx == NULL) || !__xlCanEmitFunctionValueThunk(pFunc) ) {
		return;
	}

	pParams = (pFunc->iChildCount != 0) ? pFunc->ppChildren[0] : NULL;
	sReturnType = ((pFunc->sTypeName != NULL) && (pFunc->sTypeName[0] != '\0')) ? pFunc->sTypeName : "void";
	sFuncSymbol = __xlMirFuncSymbolName(pFunc);
	__xlCgenFunctionValueThunkName(sThunkName, sizeof(sThunkName), pFunc);
	snprintf(sResultName, sizeof(sResultName), "xl_func_result_%d", pFunc->iFuncIndex);

	__xlCgenAppendFormat(&pCtx->buf, "static xvalue %s(xvalue varENV, xvalue arrParam)\n{\n", sThunkName);
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "(void)varENV;\n");
	for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
		char sArgExpr[96];

		pParam = pParams->ppChildren[i];
		snprintf(sArgExpr, sizeof(sArgExpr), "xvoArrayGetValue((xvalue)(arrParam), (uint32)(%llu))", (unsigned long long)i);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s = ",
			__xlCTypeName(((pParam->sTypeName != NULL) && (pParam->sTypeName[0] != '\0')) ? pParam->sTypeName : "int"),
			(pParam->sText != NULL) ? pParam->sText : "arg"
		);
		__xlCgenValueExprAsType(pCtx, sArgExpr, ((pParam->sTypeName != NULL) && (pParam->sTypeName[0] != '\0')) ? pParam->sTypeName : "int");
		__xlCgenAppend(&pCtx->buf, ";\n");
	}

	if ( strcmp(sReturnType, "void") == 0 ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s(", sFuncSymbol);
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			pParam = pParams->ppChildren[i];
			if ( i != 0 ) {
				__xlCgenAppend(&pCtx->buf, ", ");
			}
			__xlCgenAppend(&pCtx->buf, (pParam->sText != NULL) ? pParam->sText : "arg");
		}
		__xlCgenAppend(&pCtx->buf, ");\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return xvoCreateNull();\n");
	} else {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = %s(", __xlCTypeName(sReturnType), sResultName, sFuncSymbol);
		for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
			pParam = pParams->ppChildren[i];
			if ( i != 0 ) {
				__xlCgenAppend(&pCtx->buf, ", ");
			}
			__xlCgenAppend(&pCtx->buf, (pParam->sText != NULL) ? pParam->sText : "arg");
		}
		__xlCgenAppend(&pCtx->buf, ");\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return ");
		__xlCgenBoxTypedCExpr(pCtx, sReturnType, sResultName);
		__xlCgenAppend(&pCtx->buf, ";\n");
	}

	pCtx->buf.iIndent -= 1;
	__xlCgenAppend(&pCtx->buf, "}\n\n");
}

static void __xlCgenEntryWrapper(XL_CgenContext* pCtx, const XL_MirNode* pEntryFunc)
{
	const char* sTypeName;
	const char* sEntryName;
	const char* sEntrySymbol;
	int bHasModuleInit;

	sTypeName = ((pEntryFunc != NULL) && (pEntryFunc->sTypeName != NULL) && (pEntryFunc->sTypeName[0] != '\0')) ? pEntryFunc->sTypeName : "void";
	sEntryName = ((pEntryFunc != NULL) && (pEntryFunc->sText != NULL)) ? pEntryFunc->sText : "main";
	sEntrySymbol = ((pEntryFunc != NULL) && (__xlMirFuncSymbolName(pEntryFunc) != NULL)) ? __xlMirFuncSymbolName(pEntryFunc) : sEntryName;
	bHasModuleInit = __xlHasTopLevelInitNode(pCtx->pRoot);

	__xlCgenAppend(&pCtx->buf, "int xl_generated_entry(void)\n{\n");
	__xlCgenAppend(&pCtx->buf, "\tint xlrt_attached_here = xlrt_attach_generated_runtime();\n");
	__xlCgenAppend(&pCtx->buf, "\tint xlrt_result = 0;\n");
	__xlCgenAppend(&pCtx->buf, "\txl_generated_runtime_reset();\n");
	if ( bHasModuleInit ) {
		__xlCgenAppend(&pCtx->buf, "\txl_generated_module_init();\n");
		__xlCgenAppend(&pCtx->buf, "\tif ( xl_generated_had_exception )\n");
		__xlCgenAppend(&pCtx->buf, "\t{\n");
		__xlCgenAppend(&pCtx->buf, "\t\txl_generated_async_trace_flush_to_env_file();\n");
		__xlCgenAppend(&pCtx->buf, "\t\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\t\treturn 0;\n");
		__xlCgenAppend(&pCtx->buf, "\t}\n");
		__xlCgenAppend(&pCtx->buf, "\tif ( xl_generated_capture_native_error() )\n");
		__xlCgenAppend(&pCtx->buf, "\t{\n");
		__xlCgenAppend(&pCtx->buf, "\t\txl_generated_async_trace_flush_to_env_file();\n");
		__xlCgenAppend(&pCtx->buf, "\t\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\t\treturn 0;\n");
		__xlCgenAppend(&pCtx->buf, "\t}\n");
	}
	if ( (pEntryFunc != NULL) && pEntryFunc->bFlag ) {
		if ( strcmp(sTypeName, "void") == 0 ) {
			__xlCgenAppendFormat(
				&pCtx->buf,
				"\t%s(",
				__xlFutureAwaitFunc("void")
			);
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntrySymbol);
			__xlCgenAppend(&pCtx->buf, "\txl_generated_async_trace_flush_to_env_file();\n");
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
			return;
		}
		if ( __xlIsIntegerTypeName(sTypeName) || (strcmp(sTypeName, "bool") == 0) ) {
			__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = %s(", __xlFutureAwaitFunc(sTypeName));
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntrySymbol);
			__xlCgenAppend(&pCtx->buf, "\txl_generated_async_trace_flush_to_env_file();\n");
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
			return;
		}
		if ( __xlIsFloatTypeName(sTypeName) ) {
			__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = (int)%s(", __xlFutureAwaitFunc(sTypeName));
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntrySymbol);
			__xlCgenAppend(&pCtx->buf, "\txl_generated_async_trace_flush_to_env_file();\n");
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
			return;
		}

		__xlCgenAppendFormat(&pCtx->buf, "\t%s(", __xlFutureAwaitFunc(sTypeName));
		__xlCgenAppendCString(&pCtx->buf, sEntryName);
		__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntrySymbol);
		__xlCgenAppend(&pCtx->buf, "\txl_generated_async_trace_flush_to_env_file();\n");
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
		return;
	}

	if ( (pEntryFunc != NULL) && (strcmp(sTypeName, "void") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "\t%s();\n", sEntrySymbol);
		__xlCgenAppend(&pCtx->buf, "\txl_generated_async_trace_flush_to_env_file();\n");
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
		return;
	}

	if ( pEntryFunc != NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = %s();\n", sEntrySymbol);
		__xlCgenAppend(&pCtx->buf, "\txl_generated_async_trace_flush_to_env_file();\n");
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "\txl_generated_async_trace_flush_to_env_file();\n");
	__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
	__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
}

int xlCgenMir(const XL_LowerResult* pLowerResult, const XL_ModuleLinkResult* pModules, XL_DiagContext* pDiag, char** psOutCode)
{
	XL_CgenContext ctx;
	const XL_MirNode* pEntryFunc;
	const XL_MirNode* pRoot;
	size_t i;

	pRoot = (pLowerResult != NULL) ? pLowerResult->pMirRoot : NULL;
	if ( (pRoot == NULL) || (psOutCode == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.pRoot = pRoot;
	ctx.pLowerResult = pLowerResult;
	ctx.pModules = pModules;

	__xlCgenAppend(&ctx.buf, "#include <stddef.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <stdint.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <stdbool.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <stdio.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <stdlib.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <string.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <math.h>\n");
	__xlCgenAppend(&ctx.buf, "#include \"xrt.h\"\n");
	__xlCgenAppend(&ctx.buf, "#include \"xlrt_generated.h\"\n\n");
	__xlCgenRuntimeSupport(&ctx.buf);

	if ( __xlHasTopLevelBind(pRoot) ) {
		for ( i = 0; i < pRoot->iChildCount; ++i ) {
			if ( (pRoot->ppChildren[i] != NULL) && (pRoot->ppChildren[i]->eKind == XL_MIR_BIND) ) {
				__xlCgenTopLevelBindDecl(&ctx, pRoot->ppChildren[i]);
			}
		}
		__xlCgenAppend(&ctx.buf, "\n");
	}

	__xlCgenModuleInit(&ctx, pRoot);
	__xlCgenModuleAbiObjects(&ctx);
	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		if ( pRoot->ppChildren[i]->eKind == XL_MIR_FUNC ) {
			__xlCgenFunctionValueThunkPrototype(&ctx, pRoot->ppChildren[i]);
		}
	}
	__xlCgenAppend(&ctx.buf, "\n");

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		if ( pRoot->ppChildren[i]->eKind == XL_MIR_FUNC ) {
			__xlCgenFunc(&ctx, pRoot->ppChildren[i]);
			__xlCgenFunctionValueThunk(&ctx, pRoot->ppChildren[i]);
		}
	}

	pEntryFunc = __xlFindEntryFunc(pRoot);
	if ( pEntryFunc != NULL ) {
		__xlCgenEntryWrapper(&ctx, pEntryFunc);
	} else {
		XL_DIAG_WARN(pDiag, "cgen", "no zero-arg entry function found for run pipeline");
		__xlCgenEntryWrapper(&ctx, NULL);
	}

	__xlNormalizeGeneratedFutureTypeNames(ctx.buf.sData);
	*psOutCode = ctx.buf.sData;
	return XL_RC_OK;
}
