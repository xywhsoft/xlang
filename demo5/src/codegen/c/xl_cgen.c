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

typedef struct XL_CgenContext {
	XL_CgenBuffer buf;
	const XL_MirNode* pRoot;
	XL_CgenLoopFrame arrLoopStack[64];
	int iLoopDepth;
	XL_CgenTryFrame arrTryStack[64];
	int iTryDepth;
} XL_CgenContext;

typedef struct XL_CgenFuncInfo {
	const char* sName;
	const char* sSymbolName;
	const char* sTypeName;
	int bAsync;
	int bGenerator;
	const char* sGeneratorVarName;
	const char* sGeneratorDoneLabel;
} XL_CgenFuncInfo;

typedef enum XL_TaskHelperKind {
	XL_TASK_HELPER_NONE = 0,
	XL_TASK_HELPER_SPAWN,
	XL_TASK_HELPER_ALL,
	XL_TASK_HELPER_RACE,
	XL_TASK_HELPER_CANCEL,
	XL_TASK_HELPER_DELAY
} XL_TaskHelperKind;

typedef enum XL_CorePrintHelperKind {
	XL_CORE_PRINT_NONE = 0,
	XL_CORE_PRINT,
	XL_CORE_PRINTLN
} XL_CorePrintHelperKind;

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
	XL_TIME_HELPER_PARSE
} XL_TimeHelperKind;

typedef enum XL_PathHelperKind {
	XL_PATH_HELPER_NONE = 0,
	XL_PATH_HELPER_JOIN,
	XL_PATH_HELPER_NAME_EXT,
	XL_PATH_HELPER_NAME,
	XL_PATH_HELPER_EXT,
	XL_PATH_HELPER_DIR,
	XL_PATH_HELPER_IS_ABS,
	XL_PATH_HELPER_RANDOM
} XL_PathHelperKind;

typedef enum XL_FsHelperKind {
	XL_FS_HELPER_NONE = 0,
	XL_FS_HELPER_EXISTS,
	XL_FS_HELPER_IS_FILE,
	XL_FS_HELPER_IS_DIR,
	XL_FS_HELPER_SIZE,
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
	XL_STR_HELPER_REPLACE
} XL_StrHelperKind;

typedef enum XL_RandHelperKind {
	XL_RAND_HELPER_NONE = 0,
	XL_RAND_HELPER_INT32,
	XL_RAND_HELPER_RANGE,
	XL_RAND_HELPER_TEXT
} XL_RandHelperKind;

typedef enum XL_HostHelperKind {
	XL_HOST_HELPER_NONE = 0,
	XL_HOST_HELPER_NAME,
	XL_HOST_HELPER_IP
} XL_HostHelperKind;

typedef enum XL_FmtHelperKind {
	XL_FMT_HELPER_NONE = 0,
	XL_FMT_HELPER_INT_TEXT,
	XL_FMT_HELPER_FLOAT_TEXT
} XL_FmtHelperKind;

typedef enum XL_EnvHelperKind {
	XL_ENV_HELPER_NONE = 0,
	XL_ENV_HELPER_GET,
	XL_ENV_HELPER_EXISTS,
	XL_ENV_HELPER_CWD
} XL_EnvHelperKind;

typedef enum XL_DirHelperKind {
	XL_DIR_HELPER_NONE = 0,
	XL_DIR_HELPER_LIST,
	XL_DIR_HELPER_FILES,
	XL_DIR_HELPER_DIRS,
	XL_DIR_HELPER_TREE
} XL_DirHelperKind;

typedef enum XL_ProcessHelperKind {
	XL_PROCESS_HELPER_NONE = 0,
	XL_PROCESS_HELPER_EXEC,
	XL_PROCESS_HELPER_CAPTURE,
	XL_PROCESS_HELPER_EXEC_IN,
	XL_PROCESS_HELPER_CAPTURE_IN
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

static int __xlIsValueBackedTypeName(const char* sTypeName)
{
	return __xlTypeHasBaseName(sTypeName, "array") ||
		__xlTypeHasBaseName(sTypeName, "list") ||
		__xlTypeHasBaseName(sTypeName, "set") ||
		__xlTypeHasBaseName(sTypeName, "dict") ||
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

static const char* __xlCTypeName(const char* sTypeName)
{
	if ( sTypeName == NULL ) {
		return "void";
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
	if ( __xlIsPointTypeName(sTypeName) ) {
		return "void*";
	}
	if ( __xlIsValueBackedTypeName(sTypeName) ) {
		return "xvalue";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
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

static const char* __xlFutureAwaitFunc(const char* sTypeName)
{
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

static const char* __xlFutureErrorFunc(const char* sTypeName)
{
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

static const char* __xlGeneratorTypeName(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || __xlIsIntegerTypeName(sTypeName) ) {
		return "XL_GeneratorInt";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "XL_GeneratorFloat";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "XL_GeneratorString";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "XL_GeneratorError";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "XL_GeneratorBool";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "XL_GeneratorPtr";
	}
	return "XL_GeneratorInt";
}

static const char* __xlGeneratorInitFunc(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_generator_int_init";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_generator_float_init";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_init";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_generator_error_init";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_generator_bool_init";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "xl_generator_ptr_init";
	}
	return "xl_generator_int_init";
}

static const char* __xlGeneratorYieldFunc(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_generator_int_yield";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_generator_float_yield";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_yield";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_generator_error_yield";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_generator_bool_yield";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "xl_generator_ptr_yield";
	}
	return "xl_generator_int_yield";
}

static const char* __xlGeneratorDoneFunc(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_generator_int_done";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_generator_float_done";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_done";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_generator_error_done";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_generator_bool_done";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "xl_generator_ptr_done";
	}
	return "xl_generator_int_done";
}

static const char* __xlGeneratorNextFunc(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') || __xlIsIntegerTypeName(sTypeName) ) {
		return "xl_generator_int_next";
	}
	if ( __xlIsFloatTypeName(sTypeName) ) {
		return "xl_generator_float_next";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_next";
	}
	if ( strcmp(sTypeName, "error") == 0 ) {
		return "xl_generator_error_next";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "xl_generator_bool_next";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return "xl_generator_ptr_next";
	}
	return "xl_generator_int_next";
}

static const char* __xlGeneratorItemCType(const char* sTypeName)
{
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

static const char* __xlCDefaultReturnValue(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "0";
	}
	if ( strcmp(sTypeName, "void") == 0 ) {
		return NULL;
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
	if ( __xlIsValueBackedTypeName(sTypeName) ) {
		return "NULL";
	}
	if ( (strcmp(sTypeName, "string") == 0) || (strcmp(sTypeName, "error") == 0) ) {
		return "NULL";
	}
	return "0";
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

	if ( psTypeName != NULL ) {
		*psTypeName = NULL;
	}

	pFunc = __xlResolveCallFuncNode(pCtx, pCallNode);
	if ( (pFunc == NULL) || (pFunc->bFlag == 0) ) {
		return 0;
	}

	if ( psTypeName != NULL ) {
		*psTypeName = ((pFunc->sTypeName != NULL) && (pFunc->sTypeName[0] != '\0')) ? pFunc->sTypeName : "void";
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
static void __xlCgenExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode);
static void __xlCgenExprAsType(XL_CgenContext* pCtx, const XL_MirNode* pNode, const char* sExpectedTypeName);
static void __xlCgenValueExprAsType(XL_CgenContext* pCtx, const char* sValueExpr, const char* sExpectedTypeName);
static void __xlCgenStmt(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo);
static const char* __xlCDefaultReturnValue(const char* sTypeName);
static void __xlCgenPropagatePendingException(XL_CgenContext* pCtx, const XL_CgenFuncInfo* pFuncInfo);

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

	if ( (pCtx == NULL) || (pNode == NULL) || (pNode->eKind != XL_MIR_BIND) ) {
		return;
	}

	sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int";
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
	size_t i;

	if ( (pCtx == NULL) || !__xlHasTopLevelInitNode(pRoot) ) {
		return;
	}

	memset(&initInfo, 0, sizeof(initInfo));
	initInfo.sName = "xl_generated_module_init";
	initInfo.sTypeName = "void";

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

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "xl_generated_module_init_done = 1;\n");
	pCtx->buf.iIndent -= 1;
	__xlCgenAppend(&pCtx->buf, "}\n\n");
}

static void __xlCgenFutureSupportTyped(XL_CgenBuffer* pBuf, const char* sSuffix, const char* sCType, const char* sHelperStem, const char* sDefaultValue)
{
	__xlCgenAppendFormat(
		pBuf,
		"typedef struct XL_Future%s {\n\tint status;\n\t%s value;\n\tXL_GeneratedError* error;\n} XL_Future%s;\n\n",
		sSuffix,
		sCType,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tfuture.status = 0;\n\tfuture.value = value;\n\tfuture.error = NULL;\n\treturn future;\n}\n\n",
		sSuffix,
		sHelperStem,
		sCType,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static XL_Future%s xl_future_%s_error(XL_GeneratedError* error)\n{\n\tXL_Future%s future;\n\tfuture.status = 1;\n\tfuture.value = %s;\n\tfuture.error = error;\n\treturn future;\n}\n\n",
		sSuffix,
		sHelperStem,
		sSuffix,
		sDefaultValue
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txl_generated_async_trace_event(\"await\", (sName != NULL) ? sName : \"\");\n\tif ( future.status != 0 ) {\n\t\txl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make(\"Error\", \"await failed\", 0, \"\", \"\"));\n\t\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n\t\treturn future.value;\n\t}\n\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n\treturn future.value;\n}\n\n",
		sCType,
		sHelperStem,
		sSuffix
	);
}

static void __xlCgenTaskSupportTyped(XL_CgenBuffer* pBuf, const char* sSuffix, const char* sCType, const char* sDefaultValue)
{
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_spawn_%s(const char* sName, %s value)\n{\n\txl_generated_task_trace_event(\"spawn\", (sName != NULL) ? sName : \"\");\n\treturn value;\n}\n\n",
		sCType,
		sSuffix,
		sCType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_all_%s(const char* sName, %s left, %s right)\n{\n\t(void)left;\n\txl_generated_task_trace_event(\"all\", (sName != NULL) ? sName : \"\");\n\treturn right;\n}\n\n",
		sCType,
		sSuffix,
		sCType,
		sCType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_race_%s(const char* sName, %s left, %s right)\n{\n\t(void)right;\n\txl_generated_task_trace_event(\"race\", (sName != NULL) ? sName : \"\");\n\treturn left;\n}\n\n",
		sCType,
		sSuffix,
		sCType,
		sCType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static int xl_task_cancel_%s(const char* sName, %s value)\n{\n\t(void)value;\n\txl_generated_task_trace_event(\"cancel\", (sName != NULL) ? sName : \"\");\n\treturn 1;\n}\n\n",
		sSuffix,
		sCType
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_task_default_%s(void)\n{\n\treturn %s;\n}\n\n",
		sCType,
		sSuffix,
		sDefaultValue
	);
}

static void __xlCgenTaskDelaySupport(XL_CgenBuffer* pBuf)
{
	__xlCgenAppend(
		pBuf,
		"static int xl_task_delay(const char* sName, int ms)\n{\n"
		"\t(void)ms;\n"
		"\txl_generated_task_trace_event(\"delay\", (sName != NULL) ? sName : \"\");\n"
		"\treturn ms;\n"
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
	__xlCgenAppend(pBuf, "static void xl_generated_raise_error(XL_GeneratedError* value);\n\n");
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
	__xlCgenTaskSupportTyped(pBuf, "int", "int", "0");
	__xlCgenTaskSupportTyped(pBuf, "float", "double", "0.0");
	__xlCgenTaskSupportTyped(pBuf, "string", "const char*", "NULL");
	__xlCgenTaskSupportTyped(pBuf, "error", "XL_GeneratedError*", "NULL");
	__xlCgenTaskSupportTyped(pBuf, "bool", "int", "0");
	__xlCgenTaskDelaySupport(pBuf);

	__xlCgenAppend(pBuf, "typedef struct XL_FutureVoid {\n\tint status;\n\tXL_GeneratedError* error;\n\tint unused;\n} XL_FutureVoid;\n\n");
	__xlCgenAppend(pBuf, "static XL_FutureVoid xl_future_void_ready(void)\n{\n\tXL_FutureVoid future;\n\tfuture.status = 0;\n\tfuture.error = NULL;\n\tfuture.unused = 0;\n\treturn future;\n}\n\n");
	__xlCgenAppend(pBuf, "static XL_FutureVoid xl_future_void_error(XL_GeneratedError* error)\n{\n\tXL_FutureVoid future;\n\tfuture.status = 1;\n\tfuture.error = error;\n\tfuture.unused = 0;\n\treturn future;\n}\n\n");
	__xlCgenAppend(pBuf, "static void xl_future_void_await(const char* sName, XL_FutureVoid future)\n{\n\txl_generated_async_trace_event(\"await\", (sName != NULL) ? sName : \"\");\n\tif ( future.status != 0 ) {\n\t\txl_generated_raise_error((future.error != NULL) ? future.error : xl_generated_error_make(\"Error\", \"await failed\", 0, \"\", \"\"));\n\t\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n\t\treturn;\n\t}\n\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n}\n\n");
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

	sDefaultReturn = __xlCDefaultReturnValue((pFuncInfo != NULL) ? pFuncInfo->sTypeName : "void");
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
		__xlCgenExprAsType(
			pCtx,
			pArgs->ppChildren[i],
			(pParam != NULL) ? pParam->sTypeName : NULL
		);
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
	char sEndLabel[64];
	char sFrameName[64];
	int bHasCatch;
	int bHasFinally;

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
	snprintf(sEndLabel, sizeof(sEndLabel), "xl_try_end_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sFrameName, sizeof(sFrameName), "xl_try_frame_%d_%d", pNode->iLine, pNode->iColumn);

	bHasCatch = (pNode->bFlag & 1) != 0;
	bHasFinally = (pNode->bFlag & 2) != 0;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "XL_GeneratedExceptionFrame* %s = xl_generated_exception_frame_push();\n", sFrameName);
	if ( sCatchName != NULL ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s = NULL;\n", __xlCTypeName("error"), sCatchName);
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
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sFinallyLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenStmt(pCtx, pFinallyBlock, pFuncInfo);
	}

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( %s->needs_rethrow && !xl_generated_had_exception )\n", sFrameName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "xl_generated_exception_frame_restore(%s);\n", sFrameName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "xl_generated_exception_frame_pop();\n");
	__xlCgenPropagateRaisedException(pCtx, pFuncInfo);
	pCtx->buf.iIndent -= 1;
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "}\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s:\n", sEndLabel);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "xl_generated_exception_frame_pop();\n");
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
	const XL_MirNode* pArgs;

	if ( (pCallNode == NULL) || (pCallNode->eKind != XL_MIR_CALL) || (pCallNode->iChildCount == 0) ) {
		return NULL;
	}
	pCallee = pCallNode->ppChildren[0];
	if ( (pCallee != NULL) && (pCallee->eKind == XL_MIR_MEMBER) && (pCallee->iChildCount != 0) ) {
		return pCallee->ppChildren[0];
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
	if ( strcmp(sCalleeName, "xl_mod_std_rand__range") == 0 ) {
		return XL_RAND_HELPER_RANGE;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_rand__text") == 0 ) {
		return XL_RAND_HELPER_TEXT;
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
	return XL_FMT_HELPER_NONE;
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
	if ( strcmp(sCalleeName, "xl_mod_std_env__cwd") == 0 ) {
		return XL_ENV_HELPER_CWD;
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
	if ( strcmp(sCalleeName, "xl_mod_std_dir__files") == 0 ) {
		return XL_DIR_HELPER_FILES;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__dirs") == 0 ) {
		return XL_DIR_HELPER_DIRS;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_dir__tree") == 0 ) {
		return XL_DIR_HELPER_TREE;
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
	return XL_PROCESS_HELPER_NONE;
}

static const char* __xlInferExprTypeName(const XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const char* sCalleeName;
	const char* sAsyncTypeName;
	XL_TaskHelperKind eTaskKind;

	if ( pNode == NULL ) {
		return NULL;
	}
	if ( (pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') ) {
		return pNode->sTypeName;
	}
	if ( pNode->eKind == XL_MIR_CALL ) {
		sCalleeName = __xlCallCalleeName(pNode);
		sAsyncTypeName = NULL;
		if ( __xlResolveAsyncCallType(pCtx, pNode, &sAsyncTypeName) ) {
			return sAsyncTypeName;
		}
		if ( __xlCorePrintHelperKind(sCalleeName) != XL_CORE_PRINT_NONE ) {
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
		return __xlInferExprTypeName(pCtx, pNode->ppChildren[0]);
	}
	return NULL;
}

static const char* __xlTaskTypeSuffix(const char* sTypeName)
{
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
	return (strcmp(sTypeName, "string") == 0) || (strcmp(sTypeName, "error") == 0);
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

	sTypeName = __xlInferExprTypeName(pCtx, pNode);
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
	if ( strcmp(sTypeName, "string") == 0 ) {
		__xlCgenAppend(&pCtx->buf, "({ const char* xl_text_value = ");
		__xlCgenExprAsType(pCtx, pNode, "string");
		__xlCgenAppend(&pCtx->buf, "; xvoCreateText((ptr)xl_text_value, (uint32)strlen((xl_text_value != NULL) ? xl_text_value : \"\"), FALSE); })");
		return;
	}
	if ( __xlIsPointTypeName(sTypeName) ) {
		__xlCgenAppend(&pCtx->buf, "xvoCreatePoint((ptr)(");
		__xlCgenExpr(pCtx, pNode);
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

	sActualTypeName = __xlInferExprTypeName(pCtx, pNode);
	if ( __xlIsVarTypeName(sExpectedTypeName) ) {
		__xlCgenBoxValueExpr(pCtx, pNode);
		return;
	}
	if ( __xlIsVarTypeName(sActualTypeName) ) {
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
		if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "string") == 0) ) {
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
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "string") == 0) &&
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
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "string") == 0) &&
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
	if ( __xlIsFloatTypeName(sExpectedTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "((%s)xvoGetFloat((xvalue)(%s)))", __xlCTypeName(sExpectedTypeName), (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "bool") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "(xvoGetBool((xvalue)(%s)) ? 1 : 0)", (sValueExpr != NULL) ? sValueExpr : "NULL");
		return;
	}
	if ( (sExpectedTypeName != NULL) && (strcmp(sExpectedTypeName, "string") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "xl_generated_copy_text(xl_value_to_string((xvalue)(%s)))", (sValueExpr != NULL) ? sValueExpr : "NULL");
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

static void __xlCgenTaskArgExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const char* sAsyncTypeName;
	const char* sCalleeName;

	sAsyncTypeName = NULL;
	sCalleeName = __xlCallCalleeName(pNode);
	if ( __xlResolveAsyncCallType(pCtx, pNode, &sAsyncTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitFunc(sAsyncTypeName));
		__xlCgenAppendCString(&pCtx->buf, sCalleeName);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	__xlCgenExpr(pCtx, pNode);
}

static void __xlCgenTaskCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const char* sCalleeName;
	const char* sTypeName;
	const char* sSuffix;
	const char* sTraceName;
	XL_TaskHelperKind eKind;

	sCalleeName = __xlCallCalleeName(pNode);
	eKind = __xlTaskHelperKind(sCalleeName);
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
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
			__xlCgenAppendFormat(&pCtx->buf, "xl_task_cancel_%s(", sSuffix);
			__xlCgenAppendCString(&pCtx->buf, sTraceName);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenTaskArgExpr(pCtx, (pArgs != NULL) ? pArgs->ppChildren[0] : NULL);
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
		__xlCgenTaskArgExpr(pCtx, pArg0);
	}
	__xlCgenAppend(&pCtx->buf, ")");
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
		default:
			return;
	}
}

static void __xlCgenEnvCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	XL_EnvHelperKind eKind;

	eKind = __xlEnvHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;

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
		case XL_ENV_HELPER_CWD:
			__xlCgenAppend(&pCtx->buf, "xl_env_cwd()");
			return;
		default:
			return;
	}
}

static void __xlCgenDirCall(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pArgs;
	const XL_MirNode* pArg0;
	XL_DirHelperKind eKind;

	eKind = __xlDirHelperKind(__xlCallCalleeName(pNode));
	pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pArg0 = ((pArgs != NULL) && (pArgs->iChildCount != 0)) ? pArgs->ppChildren[0] : NULL;

	switch ( eKind ) {
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
		default:
			return;
	}
}

static void __xlCgenAwaitExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pInner;
	const char* sAsyncTypeName;
	const char* sCalleeName;

	pInner = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	sAsyncTypeName = NULL;
	sCalleeName = __xlCallCalleeName(pInner);
	if ( __xlResolveAsyncCallType(pCtx, pInner, &sAsyncTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitFunc(sAsyncTypeName));
		__xlCgenAppendCString(&pCtx->buf, sCalleeName);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pInner);
		__xlCgenAppend(&pCtx->buf, ")");
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

	if ( (pNode->eKind == XL_MIR_CALL) && (__xlTaskHelperKind(__xlCallCalleeName(pNode)) != XL_TASK_HELPER_NONE) ) {
		__xlCgenTaskCall(pCtx, pNode);
		return;
	}

	if ( (pNode->eKind == XL_MIR_CALL) && (__xlCorePrintHelperKind(__xlCallCalleeName(pNode)) != XL_CORE_PRINT_NONE) ) {
		__xlCgenCorePrintCall(pCtx, pNode);
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
			(strcmp(pNode->ppChildren[0]->sTypeName, "error") == 0) ) {
			__xlCgenErrorMemberExpr(pCtx, pNode);
			return;
		}
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		__xlCgenAppendFormat(&pCtx->buf, ".%s", (pNode->sText != NULL) ? pNode->sText : "");
		return;
	}

	if ( pNode->eKind == XL_MIR_CALL ) {
		const XL_MirNode* pFunc;
		const char* sFuncSymbol;

		pFunc = __xlResolveCallFuncNode(pCtx, pNode);
		sFuncSymbol = __xlMirFuncSymbolName(pFunc);
		if ( (pFunc != NULL) && (sFuncSymbol != NULL) && (sFuncSymbol[0] != '\0') ) {
			__xlCgenAppend(&pCtx->buf, sFuncSymbol);
		} else {
			__xlCgenExpr(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL);
		}
		__xlCgenAppend(&pCtx->buf, "(");
		__xlCgenCallArgs(pCtx, pNode);
		__xlCgenAppend(&pCtx->buf, ")");
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

static int __xlIsGeneratorCall(const XL_CgenContext* pCtx, const XL_MirNode* pNode, const char** psValueTypeName, const char** psFuncName)
{
	const XL_MirNode* pFunc;

	if ( psValueTypeName != NULL ) {
		*psValueTypeName = NULL;
	}
	if ( psFuncName != NULL ) {
		*psFuncName = NULL;
	}
	if ( (pCtx == NULL) || (pNode == NULL) || (pNode->eKind != XL_MIR_CALL) ) {
		return 0;
	}

	pFunc = __xlResolveCallFuncNode(pCtx, pNode);
	if ( !__xlIsGeneratorFuncNode(pFunc) ) {
		return 0;
	}
	if ( psValueTypeName != NULL ) {
		*psValueTypeName = ((pFunc->sTypeName != NULL) && (pFunc->sTypeName[0] != '\0')) ? pFunc->sTypeName : "int";
	}
	if ( psFuncName != NULL ) {
		*psFuncName = (pFunc != NULL) ? pFunc->sText : __xlCallCalleeName(pNode);
	}
	return 1;
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
	const XL_MirNode* pTarget0;
	const XL_MirNode* pTarget1;
	const char* sTarget0;
	const char* sTarget1;
	const char* sValueTypeName;
	const char* sFuncName;
	char sGeneratorName[64];
	char sIndexName[64];
	char sCurrentValueName[64];
	char sBreakLabel[64];
	char sContinueLabel[64];

	pTargets = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pIterable = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
	pBody = (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL;
	pTarget0 = ((pTargets != NULL) && (pTargets->iChildCount != 0)) ? pTargets->ppChildren[0] : NULL;
	pTarget1 = ((pTargets != NULL) && (pTargets->iChildCount > 1)) ? pTargets->ppChildren[1] : NULL;
	sTarget0 = ((pTarget0 != NULL) && (pTarget0->sText != NULL) && (pTarget0->sText[0] != '\0')) ? pTarget0->sText : "item";
	sTarget1 = ((pTarget1 != NULL) && (pTarget1->sText != NULL) && (pTarget1->sText[0] != '\0')) ? pTarget1->sText : "value";

	if ( !__xlIsGeneratorCall(pCtx, pIterable, &sValueTypeName, &sFuncName) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "/* generator iterable is reserved for a later stage */\n");
		return;
	}

	snprintf(sGeneratorName, sizeof(sGeneratorName), "xl_gen_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sIndexName, sizeof(sIndexName), "xl_gen_index_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sCurrentValueName, sizeof(sCurrentValueName), "xl_gen_value_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sBreakLabel, sizeof(sBreakLabel), "xl_for_break_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sContinueLabel, sizeof(sContinueLabel), "xl_for_continue_%d_%d", pNode->iLine, pNode->iColumn);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", __xlGeneratorTypeName(sValueTypeName), sGeneratorName);
	__xlCgenExpr(pCtx, pIterable);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s;\n", __xlGeneratorItemCType(sValueTypeName), sCurrentValueName);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "for ( int %s = 0; ", sIndexName);
	__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlGeneratorNextFunc(sValueTypeName));
	__xlCgenAppendCString(&pCtx->buf, sFuncName);
	__xlCgenAppendFormat(&pCtx->buf, ", &%s, &%s); ++%s )\n", sGeneratorName, sCurrentValueName, sIndexName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	if ( (pTargets != NULL) && (pTargets->iChildCount > 1) ) {
		__xlCgenAppendFormat(&pCtx->buf, "int %s = %s;\n", sTarget0, sIndexName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s = %s;\n",
			__xlGeneratorItemCType(sValueTypeName),
			sTarget1,
			sCurrentValueName
		);
	} else {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s %s = %s;\n",
			__xlGeneratorItemCType(sValueTypeName),
			sTarget0,
			sCurrentValueName
		);
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
	const char* sDefaultReturn;

	__xlCgenAppendIndent(&pCtx->buf);
	if ( (pFuncInfo == NULL) || (pFuncInfo->sTypeName == NULL) || (strcmp(pFuncInfo->sTypeName, "void") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "return %s();\n", __xlFutureReadyFunc("void"));
		return;
	}

	sDefaultReturn = __xlCDefaultReturnValue(pFuncInfo->sTypeName);
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
	size_t i;

	if ( pNode == NULL ) {
		return;
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
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(
				&pCtx->buf,
				"%s %s = ",
				__xlCTypeName(((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int"),
				(pNode->sText != NULL) ? pNode->sText : "tmp"
			);
			__xlCgenExprAsType(
				pCtx,
				(pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL,
				((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int"
			);
			__xlCgenAppend(&pCtx->buf, ";\n");
			__xlCgenPropagatePendingException(pCtx, pFuncInfo);
			return;
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
			if ( __xlIsRangeCall((pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL) ) {
				__xlCgenForInRange(pCtx, pNode, pFuncInfo);
			} else if ( __xlIsGeneratorCall(pCtx, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, NULL, NULL) ) {
				__xlCgenForInGenerator(pCtx, pNode, pFuncInfo);
			} else {
				__xlCgenForInContainer(pCtx, pNode, pFuncInfo);
			}
			return;
		case XL_MIR_WHILE:
			__xlCgenWhile(pCtx, pNode, pFuncInfo);
			return;
		case XL_MIR_RETURN:
			if ( (pFuncInfo != NULL) && pFuncInfo->bGenerator ) {
				if ( pNode->iChildCount != 0 ) {
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppend(&pCtx->buf, "(void)(");
					__xlCgenExpr(pCtx, pNode->ppChildren[0]);
					__xlCgenAppend(&pCtx->buf, ");\n");
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pFuncInfo->sGeneratorDoneLabel);
				return;
			}
			if ( (pFuncInfo != NULL) && pFuncInfo->bAsync ) {
				if ( (pFuncInfo->sTypeName == NULL) || (strcmp(pFuncInfo->sTypeName, "void") == 0) ) {
					__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
					__xlCgenAppendIndent(&pCtx->buf);
					__xlCgenAppendFormat(&pCtx->buf, "return %s();\n", __xlFutureReadyFunc("void"));
				} else {
					if ( pNode->iChildCount != 0 ) {
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(
							&pCtx->buf,
							"%s xl_async_ret_%d_%d = ",
							__xlCTypeName(pFuncInfo->sTypeName),
							pNode->iLine,
							pNode->iColumn
						);
						__xlCgenExprAsType(pCtx, pNode->ppChildren[0], pFuncInfo->sTypeName);
						__xlCgenAppend(&pCtx->buf, ";\n");
						__xlCgenPropagatePendingException(pCtx, pFuncInfo);
						__xlCgenTraceEventStmt(pCtx, "return", pFuncInfo->sName);
						__xlCgenAppendIndent(&pCtx->buf);
						__xlCgenAppendFormat(&pCtx->buf, "return %s(xl_async_ret_%d_%d);\n", __xlFutureReadyFunc(pFuncInfo->sTypeName), pNode->iLine, pNode->iColumn);
					} else {
						sDefaultReturn = __xlCDefaultReturnValue(pFuncInfo->sTypeName);
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
					__xlCTypeName((pFuncInfo != NULL) ? pFuncInfo->sTypeName : pNode->ppChildren[0]->sTypeName),
					pNode->iLine,
					pNode->iColumn
				);
				__xlCgenExprAsType(
					pCtx,
					pNode->ppChildren[0],
					(pFuncInfo != NULL) ? pFuncInfo->sTypeName : pNode->ppChildren[0]->sTypeName
				);
				__xlCgenAppend(&pCtx->buf, ";\n");
				__xlCgenPropagatePendingException(pCtx, pFuncInfo);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "return xl_ret_%d_%d;\n", pNode->iLine, pNode->iColumn);
			} else {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return;\n");
			}
			return;
		case XL_MIR_BREAK:
		{
			const XL_CgenLoopFrame* pFrame;

			pFrame = __xlCgenFindLoopFrame(pCtx, (int)pNode->iIntValue);
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

			pFrame = __xlCgenFindLoopFrame(pCtx, (int)pNode->iIntValue);
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
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlGeneratorYieldFunc(pFuncInfo->sTypeName));
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppendFormat(&pCtx->buf, ", &%s, ", pFuncInfo->sGeneratorVarName);
				if ( pNode->iChildCount != 0 ) {
					__xlCgenExpr(pCtx, pNode->ppChildren[0]);
				} else {
					__xlCgenAppend(&pCtx->buf, __xlCDefaultReturnValue(pFuncInfo->sTypeName));
				}
				__xlCgenAppend(&pCtx->buf, ");\n");
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

static void __xlCgenFunc(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const XL_MirNode* pParams;
	const XL_MirNode* pBody;
	const XL_MirNode* pParam;
	XL_CgenFuncInfo funcInfo;
	char sGeneratorVarName[64];
	char sGeneratorDoneLabel[64];
	size_t i;

	memset(&funcInfo, 0, sizeof(funcInfo));
	funcInfo.sName = (pNode->sText != NULL) ? pNode->sText : "anon";
	funcInfo.sSymbolName = (__xlMirFuncSymbolName(pNode) != NULL) ? __xlMirFuncSymbolName(pNode) : funcInfo.sName;
	funcInfo.sTypeName = ((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "void";
	funcInfo.bAsync = pNode->bFlag;
	funcInfo.bGenerator = __xlIsGeneratorFuncNode(pNode);
	snprintf(sGeneratorVarName, sizeof(sGeneratorVarName), "xl_gen_result_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sGeneratorDoneLabel, sizeof(sGeneratorDoneLabel), "xl_gen_done_%d_%d", pNode->iLine, pNode->iColumn);
	funcInfo.sGeneratorVarName = sGeneratorVarName;
	funcInfo.sGeneratorDoneLabel = sGeneratorDoneLabel;
	if ( __xlTaskHelperKind(funcInfo.sName) != XL_TASK_HELPER_NONE ) {
		return;
	}
	if ( __xlCorePrintHelperKind(funcInfo.sName) != XL_CORE_PRINT_NONE ) {
		return;
	}
	if ( __xlCoreLenHelperKind(funcInfo.sName) != XL_CORE_LEN_NONE ) {
		return;
	}
	if ( __xlTimeHelperKind(funcInfo.sName) != XL_TIME_HELPER_NONE ) {
		return;
	}
	if ( __xlPathHelperKind(funcInfo.sName) != XL_PATH_HELPER_NONE ) {
		return;
	}
	if ( __xlFsHelperKind(funcInfo.sName) != XL_FS_HELPER_NONE ) {
		return;
	}
	if ( __xlStrHelperKind(funcInfo.sName) != XL_STR_HELPER_NONE ) {
		return;
	}
	if ( __xlRandHelperKind(funcInfo.sName) != XL_RAND_HELPER_NONE ) {
		return;
	}
	if ( __xlHostHelperKind(funcInfo.sName) != XL_HOST_HELPER_NONE ) {
		return;
	}
	if ( __xlFmtHelperKind(funcInfo.sName) != XL_FMT_HELPER_NONE ) {
		return;
	}
	if ( __xlEnvHelperKind(funcInfo.sName) != XL_ENV_HELPER_NONE ) {
		return;
	}
	if ( __xlDirHelperKind(funcInfo.sName) != XL_DIR_HELPER_NONE ) {
		return;
	}
	if ( __xlProcessHelperKind(funcInfo.sName) != XL_PROCESS_HELPER_NONE ) {
		return;
	}

	pParams = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pBody = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;

	if ( __xlMirFuncHasExternTarget(pNode) && (pBody != NULL) && (pBody->eKind == XL_MIR_BLOCK) && (pBody->iChildCount == 0) ) {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"extern %s %s(",
			__xlCTypeName(funcInfo.sTypeName),
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
			__xlCTypeName(funcInfo.sTypeName),
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

	__xlCgenAppendFormat(
		&pCtx->buf,
		"static %s %s(",
		funcInfo.bGenerator ? __xlGeneratorTypeName(funcInfo.sTypeName) :
			(funcInfo.bAsync ? __xlFutureTypeName(funcInfo.sTypeName) : __xlCTypeName(funcInfo.sTypeName)),
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

	if ( funcInfo.bGenerator ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s %s;\n", __xlGeneratorTypeName(funcInfo.sTypeName), funcInfo.sGeneratorVarName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s(&%s);\n", __xlGeneratorInitFunc(funcInfo.sTypeName), funcInfo.sGeneratorVarName);
	} else if ( funcInfo.bAsync ) {
		__xlCgenTraceEventStmt(pCtx, "enter", funcInfo.sName);
	}

	if ( (pBody != NULL) && (pBody->eKind == XL_MIR_BLOCK) ) {
		for ( i = 0; i < pBody->iChildCount; ++i ) {
			__xlCgenStmt(pCtx, pBody->ppChildren[i], &funcInfo);
		}
	} else {
		__xlCgenStmt(pCtx, pBody, &funcInfo);
	}

	if ( funcInfo.bGenerator ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s:\n", funcInfo.sGeneratorDoneLabel);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, ";\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlGeneratorDoneFunc(funcInfo.sTypeName));
		__xlCgenAppendCString(&pCtx->buf, funcInfo.sName);
		__xlCgenAppendFormat(&pCtx->buf, ", &%s);\n", funcInfo.sGeneratorVarName);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppendFormat(&pCtx->buf, "return %s;\n", funcInfo.sGeneratorVarName);
	} else if ( funcInfo.bAsync ) {
		__xlCgenAsyncDefaultReturn(pCtx, &funcInfo);
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
		__xlCgenAppend(&pCtx->buf, "\t\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\t\treturn 0;\n");
		__xlCgenAppend(&pCtx->buf, "\t}\n");
		__xlCgenAppend(&pCtx->buf, "\tif ( xl_generated_capture_native_error() )\n");
		__xlCgenAppend(&pCtx->buf, "\t{\n");
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
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
			return;
		}
		if ( __xlIsIntegerTypeName(sTypeName) || (strcmp(sTypeName, "bool") == 0) ) {
			__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = %s(", __xlFutureAwaitFunc(sTypeName));
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntrySymbol);
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
			return;
		}
		if ( __xlIsFloatTypeName(sTypeName) ) {
			__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = (int)%s(", __xlFutureAwaitFunc(sTypeName));
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntrySymbol);
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
			return;
		}

		__xlCgenAppendFormat(&pCtx->buf, "\t%s(", __xlFutureAwaitFunc(sTypeName));
		__xlCgenAppendCString(&pCtx->buf, sEntryName);
		__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntrySymbol);
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
		return;
	}

	if ( (pEntryFunc != NULL) && (strcmp(sTypeName, "void") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "\t%s();\n", sEntrySymbol);
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
		return;
	}

	if ( pEntryFunc != NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = %s();\n", sEntrySymbol);
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
		return;
	}

	__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
	__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
}

int xlCgenMir(const XL_MirNode* pRoot, XL_DiagContext* pDiag, char** psOutCode)
{
	XL_CgenContext ctx;
	const XL_MirNode* pEntryFunc;
	size_t i;

	if ( (pRoot == NULL) || (psOutCode == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.pRoot = pRoot;

	__xlCgenAppend(&ctx.buf, "#include <stddef.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <stdint.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <stdbool.h>\n");
	__xlCgenAppend(&ctx.buf, "#include <stdio.h>\n");
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

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		if ( pRoot->ppChildren[i]->eKind == XL_MIR_FUNC ) {
			__xlCgenFunc(&ctx, pRoot->ppChildren[i]);
		}
	}

	pEntryFunc = __xlFindEntryFunc(pRoot);
	if ( pEntryFunc != NULL ) {
		__xlCgenEntryWrapper(&ctx, pEntryFunc);
	} else {
		XL_DIAG_WARN(pDiag, "cgen", "no zero-arg entry function found for run pipeline");
		__xlCgenEntryWrapper(&ctx, NULL);
	}

	*psOutCode = ctx.buf.sData;
	return XL_RC_OK;
}
