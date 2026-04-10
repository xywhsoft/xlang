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

typedef struct XL_CgenContext {
	XL_CgenBuffer buf;
	const XL_MirNode* pRoot;
	XL_CgenLoopFrame arrLoopStack[64];
	int iLoopDepth;
} XL_CgenContext;

typedef struct XL_CgenFuncInfo {
	const char* sName;
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
	XL_TASK_HELPER_CANCEL
} XL_TaskHelperKind;

typedef enum XL_CorePrintHelperKind {
	XL_CORE_PRINT_NONE = 0,
	XL_CORE_PRINT,
	XL_CORE_PRINTLN
} XL_CorePrintHelperKind;

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
	XL_STR_HELPER_STARTS_WITH,
	XL_STR_HELPER_ENDS_WITH,
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

static const char* __xlCTypeName(const char* sTypeName)
{
	if ( sTypeName == NULL ) {
		return "void";
	}
	if ( strcmp(sTypeName, "int") == 0 ) {
		return "int";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "double";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "int";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "const char*";
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
	if ( (strcmp(sTypeName, "int") == 0) || (strcmp(sTypeName, "bool") == 0) ) {
		return "XL_FutureInt";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "XL_FutureFloat";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "XL_FutureString";
	}
	return "XL_FuturePtr";
}

static const char* __xlFutureReadyFunc(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_future_void_ready";
	}
	if ( (strcmp(sTypeName, "int") == 0) || (strcmp(sTypeName, "bool") == 0) ) {
		return "xl_future_int_ready";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "xl_future_float_ready";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_future_string_ready";
	}
	return "xl_future_ptr_ready";
}

static const char* __xlFutureAwaitFunc(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (strcmp(sTypeName, "void") == 0) ) {
		return "xl_future_void_await";
	}
	if ( (strcmp(sTypeName, "int") == 0) || (strcmp(sTypeName, "bool") == 0) ) {
		return "xl_future_int_await";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "xl_future_float_await";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_future_string_await";
	}
	return "xl_future_ptr_await";
}

static const char* __xlGeneratorTypeName(const char* sTypeName)
{
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "XL_GeneratorInt";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "XL_GeneratorFloat";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "XL_GeneratorString";
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
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "xl_generator_int_init";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "xl_generator_float_init";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_init";
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
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "xl_generator_int_yield";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "xl_generator_float_yield";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_yield";
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
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "xl_generator_int_done";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "xl_generator_float_done";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_done";
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
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "xl_generator_int_next";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "xl_generator_float_next";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "xl_generator_string_next";
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
	if ( (sTypeName == NULL) || (sTypeName[0] == '\0') ) {
		return "int";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "double";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "const char*";
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
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "0.0";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
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

static int __xlFindAsyncFuncType(const XL_CgenContext* pCtx, const char* sName, const char** psTypeName)
{
	const XL_MirNode* pFunc;

	if ( psTypeName != NULL ) {
		*psTypeName = NULL;
	}
	if ( (pCtx == NULL) || (sName == NULL) ) {
		return 0;
	}

	pFunc = __xlFindFuncNode(pCtx->pRoot, sName);
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

static int __xlFindGeneratorFuncType(const XL_CgenContext* pCtx, const char* sName, const char** psTypeName)
{
	const XL_MirNode* pFunc;

	if ( psTypeName != NULL ) {
		*psTypeName = NULL;
	}
	if ( (pCtx == NULL) || (sName == NULL) ) {
		return 0;
	}

	pFunc = __xlFindFuncNode(pCtx->pRoot, sName);
	if ( !__xlIsGeneratorFuncNode(pFunc) ) {
		return 0;
	}

	if ( psTypeName != NULL ) {
		*psTypeName = ((pFunc->sTypeName != NULL) && (pFunc->sTypeName[0] != '\0')) ? pFunc->sTypeName : "int";
	}
	return 1;
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

static void __xlCgenExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode);
static void __xlCgenStmt(XL_CgenContext* pCtx, const XL_MirNode* pNode, const XL_CgenFuncInfo* pFuncInfo);

static void __xlCgenFutureSupportTyped(XL_CgenBuffer* pBuf, const char* sSuffix, const char* sCType, const char* sHelperStem)
{
	__xlCgenAppendFormat(
		pBuf,
		"typedef struct XL_Future%s {\n\t%s value;\n} XL_Future%s;\n\n",
		sSuffix,
		sCType,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static XL_Future%s xl_future_%s_ready(%s value)\n{\n\tXL_Future%s future;\n\tfuture.value = value;\n\treturn future;\n}\n\n",
		sSuffix,
		sHelperStem,
		sCType,
		sSuffix
	);
	__xlCgenAppendFormat(
		pBuf,
		"static %s xl_future_%s_await(const char* sName, XL_Future%s future)\n{\n\txl_generated_async_trace_event(\"await\", (sName != NULL) ? sName : \"\");\n\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n\treturn future.value;\n}\n\n",
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
	__xlCgenAppend(pBuf, "int xl_generated_had_exception = 0;\n");
	__xlCgenAppend(pBuf, "int xl_generated_exception_kind = 0;\n");
	__xlCgenAppend(pBuf, "const char* xl_generated_exception_text = NULL;\n");
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
	__xlCgenAppend(pBuf, "\txl_generated_exception_int = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_float = 0.0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace_len = 0;\n");
	__xlCgenAppend(pBuf, "\txl_generated_async_trace[0] = '\\0';\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_int(long long value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_int = value;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = NULL;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_float(double value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 2;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_float = value;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = NULL;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_string(const char* value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 3;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = value;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenAppend(pBuf, "static void xl_generated_raise_text(const char* value)\n{\n");
	__xlCgenAppend(pBuf, "\txl_generated_had_exception = 1;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_kind = 3;\n");
	__xlCgenAppend(pBuf, "\txl_generated_exception_text = value;\n");
	__xlCgenAppend(pBuf, "}\n\n");

	__xlCgenFutureSupportTyped(pBuf, "Int", "int", "int");
	__xlCgenFutureSupportTyped(pBuf, "Float", "double", "float");
	__xlCgenFutureSupportTyped(pBuf, "String", "const char*", "string");
	__xlCgenFutureSupportTyped(pBuf, "Ptr", "void*", "ptr");
	__xlCgenGeneratorSupportTyped(pBuf, "Int", "int", "int");
	__xlCgenGeneratorSupportTyped(pBuf, "Float", "double", "float");
	__xlCgenGeneratorSupportTyped(pBuf, "String", "const char*", "string");
	__xlCgenGeneratorSupportTyped(pBuf, "Bool", "int", "bool");
	__xlCgenGeneratorSupportTyped(pBuf, "Ptr", "void*", "ptr");
	__xlCgenTaskSupportTyped(pBuf, "int", "int", "0");
	__xlCgenTaskSupportTyped(pBuf, "float", "double", "0.0");
	__xlCgenTaskSupportTyped(pBuf, "string", "const char*", "NULL");
	__xlCgenTaskSupportTyped(pBuf, "bool", "int", "0");

	__xlCgenAppend(pBuf, "typedef struct XL_FutureVoid {\n\tint unused;\n} XL_FutureVoid;\n\n");
	__xlCgenAppend(pBuf, "static XL_FutureVoid xl_future_void_ready(void)\n{\n\tXL_FutureVoid future;\n\tfuture.unused = 0;\n\treturn future;\n}\n\n");
	__xlCgenAppend(pBuf, "static void xl_future_void_await(const char* sName, XL_FutureVoid future)\n{\n\t(void)future;\n\txl_generated_async_trace_event(\"await\", (sName != NULL) ? sName : \"\");\n\txl_generated_async_trace_event(\"resume\", (sName != NULL) ? sName : \"\");\n}\n\n");
}

static void __xlCgenThrowRaise(XL_CgenContext* pCtx, const XL_MirNode* pValue)
{
	const char* sTypeName;

	sTypeName = (pValue != NULL) ? pValue->sTypeName : NULL;
	__xlCgenAppendIndent(&pCtx->buf);
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "int") == 0) ) {
		__xlCgenAppend(&pCtx->buf, "xl_generated_raise_int((long long)(");
		__xlCgenExpr(pCtx, pValue);
		__xlCgenAppend(&pCtx->buf, "));\n");
		return;
	}
	if ( (sTypeName != NULL) && (strcmp(sTypeName, "float") == 0) ) {
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

static void __xlCgenArgs(XL_CgenContext* pCtx, const XL_MirNode* pArgs)
{
	size_t i;

	for ( i = 0; (pArgs != NULL) && (i < pArgs->iChildCount); ++i ) {
		if ( i != 0 ) {
			__xlCgenAppend(&pCtx->buf, ", ");
		}
		__xlCgenExpr(pCtx, pArgs->ppChildren[i]);
	}
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
	if ( strcmp(sCalleeName, "xl_mod_std_str__starts_with") == 0 ) {
		return XL_STR_HELPER_STARTS_WITH;
	}
	if ( strcmp(sCalleeName, "xl_mod_std_str__ends_with") == 0 ) {
		return XL_STR_HELPER_ENDS_WITH;
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

	if ( pNode == NULL ) {
		return NULL;
	}
	if ( (pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') ) {
		return pNode->sTypeName;
	}
	if ( pNode->eKind == XL_MIR_CALL ) {
		sCalleeName = __xlCallCalleeName(pNode);
		sAsyncTypeName = NULL;
		if ( (sCalleeName != NULL) && __xlFindAsyncFuncType(pCtx, sCalleeName, &sAsyncTypeName) ) {
			return sAsyncTypeName;
		}
		if ( __xlCorePrintHelperKind(sCalleeName) != XL_CORE_PRINT_NONE ) {
			return "void";
		}
		if ( __xlTimeHelperKind(sCalleeName) == XL_TIME_HELPER_NOW ) {
			return "time";
		}
		if ( __xlTimeHelperKind(sCalleeName) == XL_TIME_HELPER_PARSE ) {
			return "time";
		}
		if ( (__xlTimeHelperKind(sCalleeName) == XL_TIME_HELPER_TO_STRING) ||
			(__xlTimeHelperKind(sCalleeName) == XL_TIME_HELPER_NOW_TEXT) ||
			(__xlTimeHelperKind(sCalleeName) == XL_TIME_HELPER_DATE_TEXT) ||
			(__xlTimeHelperKind(sCalleeName) == XL_TIME_HELPER_TIME_TEXT) ||
			(__xlTimeHelperKind(sCalleeName) == XL_TIME_HELPER_RELATIVE) ) {
			return "string";
		}
		if ( __xlPathHelperKind(sCalleeName) == XL_PATH_HELPER_IS_ABS ) {
			return "bool";
		}
		if ( __xlPathHelperKind(sCalleeName) != XL_PATH_HELPER_NONE ) {
			return "string";
		}
		if ( (__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_EXISTS) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_IS_FILE) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_IS_DIR) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_WRITE_ALL) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_APPEND_ALL) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_MKDIR_ALL) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_COPY) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_MOVE) ||
			(__xlFsHelperKind(sCalleeName) == XL_FS_HELPER_DELETE) ) {
			return "bool";
		}
		if ( __xlFsHelperKind(sCalleeName) == XL_FS_HELPER_SIZE ) {
			return "int";
		}
		if ( __xlFsHelperKind(sCalleeName) == XL_FS_HELPER_READ_ALL ) {
			return "string";
		}
		if ( (__xlStrHelperKind(sCalleeName) == XL_STR_HELPER_CONTAINS) ||
			(__xlStrHelperKind(sCalleeName) == XL_STR_HELPER_STARTS_WITH) ||
			(__xlStrHelperKind(sCalleeName) == XL_STR_HELPER_ENDS_WITH) ) {
			return "bool";
		}
		if ( (__xlStrHelperKind(sCalleeName) == XL_STR_HELPER_LOWER) ||
			(__xlStrHelperKind(sCalleeName) == XL_STR_HELPER_UPPER) ||
			(__xlStrHelperKind(sCalleeName) == XL_STR_HELPER_REPLACE) ) {
			return "string";
		}
		if ( (__xlRandHelperKind(sCalleeName) == XL_RAND_HELPER_INT32) ||
			(__xlRandHelperKind(sCalleeName) == XL_RAND_HELPER_RANGE) ) {
			return "int";
		}
		if ( __xlRandHelperKind(sCalleeName) == XL_RAND_HELPER_TEXT ) {
			return "string";
		}
		if ( __xlHostHelperKind(sCalleeName) != XL_HOST_HELPER_NONE ) {
			return "string";
		}
		if ( __xlFmtHelperKind(sCalleeName) != XL_FMT_HELPER_NONE ) {
			return "string";
		}
		if ( __xlEnvHelperKind(sCalleeName) == XL_ENV_HELPER_EXISTS ) {
			return "bool";
		}
		if ( __xlEnvHelperKind(sCalleeName) != XL_ENV_HELPER_NONE ) {
			return "string";
		}
		if ( __xlDirHelperKind(sCalleeName) != XL_DIR_HELPER_NONE ) {
			return "string";
		}
		if ( __xlProcessHelperKind(sCalleeName) == XL_PROCESS_HELPER_EXEC ) {
			return "int";
		}
		if ( (__xlProcessHelperKind(sCalleeName) == XL_PROCESS_HELPER_CAPTURE) ||
			(__xlProcessHelperKind(sCalleeName) == XL_PROCESS_HELPER_CAPTURE_IN) ) {
			return "string";
		}
		if ( __xlProcessHelperKind(sCalleeName) == XL_PROCESS_HELPER_EXEC_IN ) {
			return "int";
		}
		if ( __xlTaskHelperKind(sCalleeName) == XL_TASK_HELPER_CANCEL ) {
			return "int";
		}
		pArgs = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
		if ( (pArgs != NULL) && (pArgs->iChildCount != 0) ) {
			return __xlInferExprTypeName(pCtx, pArgs->ppChildren[0]);
		}
	}
	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) ) {
		const char* sLeftType;
		const char* sRightType;

		sLeftType = __xlInferExprTypeName(pCtx, pNode->ppChildren[0]);
		sRightType = __xlInferExprTypeName(pCtx, pNode->ppChildren[1]);
		if ( (strcmp(pNode->sText, "==") == 0) || (strcmp(pNode->sText, "!=") == 0) ||
			(strcmp(pNode->sText, ">") == 0) || (strcmp(pNode->sText, ">=") == 0) ||
			(strcmp(pNode->sText, "<") == 0) || (strcmp(pNode->sText, "<=") == 0) ) {
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
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "float";
	}
	if ( strcmp(sTypeName, "string") == 0 ) {
		return "string";
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
	if ( strcmp(sTypeName, "int") == 0 ) {
		return "int";
	}
	if ( strcmp(sTypeName, "float") == 0 ) {
		return "float";
	}
	if ( strcmp(sTypeName, "bool") == 0 ) {
		return "bool";
	}
	return "string";
}

static void __xlCgenTaskArgExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	const char* sAsyncTypeName;
	const char* sCalleeName;

	sAsyncTypeName = NULL;
	sCalleeName = __xlCallCalleeName(pNode);
	if ( (sCalleeName != NULL) && __xlFindAsyncFuncType(pCtx, sCalleeName, &sAsyncTypeName) ) {
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
	__xlCgenTaskArgExpr(pCtx, pArg0);
	__xlCgenAppend(&pCtx->buf, ")");
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
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_EXT:
			__xlCgenAppend(&pCtx->buf, "xl_path_ext(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_DIR:
			__xlCgenAppend(&pCtx->buf, "xl_path_dir(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_IS_ABS:
			__xlCgenAppend(&pCtx->buf, "xl_path_is_abs(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_PATH_HELPER_RANDOM:
			__xlCgenAppend(&pCtx->buf, "xl_path_random(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
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
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_IS_FILE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_is_file(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_IS_DIR:
			__xlCgenAppend(&pCtx->buf, "xl_fs_is_dir(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_SIZE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_size(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_READ_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_read_all(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_WRITE_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_write_all(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_APPEND_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_append_all(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_MKDIR_ALL:
			__xlCgenAppend(&pCtx->buf, "xl_fs_mkdir_all(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_COPY:
			__xlCgenAppend(&pCtx->buf, "xl_fs_copy(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_MOVE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_move(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_FS_HELPER_DELETE:
			__xlCgenAppend(&pCtx->buf, "xl_fs_delete(");
			__xlCgenExpr(pCtx, pArg0);
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
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_STARTS_WITH:
			__xlCgenAppend(&pCtx->buf, "xl_str_starts_with(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_ENDS_WITH:
			__xlCgenAppend(&pCtx->buf, "xl_str_ends_with(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_LOWER:
			__xlCgenAppend(&pCtx->buf, "xl_str_lower(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_UPPER:
			__xlCgenAppend(&pCtx->buf, "xl_str_upper(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ")");
			return;
		case XL_STR_HELPER_REPLACE:
			__xlCgenAppend(&pCtx->buf, "xl_str_replace(");
			__xlCgenExpr(pCtx, pArg0);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg1);
			__xlCgenAppend(&pCtx->buf, ", ");
			__xlCgenExpr(pCtx, pArg2);
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
	if ( (sCalleeName != NULL) && __xlFindAsyncFuncType(pCtx, sCalleeName, &sAsyncTypeName) ) {
		__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitFunc(sAsyncTypeName));
		__xlCgenAppendCString(&pCtx->buf, sCalleeName);
		__xlCgenAppend(&pCtx->buf, ", ");
		__xlCgenExpr(pCtx, pInner);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	__xlCgenExpr(pCtx, pInner);
}

static void __xlCgenExpr(XL_CgenContext* pCtx, const XL_MirNode* pNode)
{
	if ( pNode == NULL ) {
		__xlCgenAppend(&pCtx->buf, "0");
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

	if ( pNode->eKind == XL_MIR_CALL ) {
		__xlCgenExpr(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL);
		__xlCgenAppend(&pCtx->buf, "(");
		__xlCgenArgs(pCtx, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL);
		__xlCgenAppend(&pCtx->buf, ")");
		return;
	}

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 2) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		__xlCgenAppend(&pCtx->buf, "(");
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		__xlCgenAppendFormat(&pCtx->buf, " %s ", pNode->sText);
		__xlCgenExpr(pCtx, pNode->ppChildren[1]);
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

	if ( (pNode->eKind == XL_MIR_VALUE) && (pNode->iChildCount == 1) && (pNode->sText != NULL) && (pNode->sText[0] != '\0') ) {
		__xlCgenAppendFormat(&pCtx->buf, "(%s", pNode->sText);
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
	const char* sCalleeName;

	if ( psValueTypeName != NULL ) {
		*psValueTypeName = NULL;
	}
	if ( psFuncName != NULL ) {
		*psFuncName = NULL;
	}
	if ( (pCtx == NULL) || (pNode == NULL) || (pNode->eKind != XL_MIR_CALL) ) {
		return 0;
	}

	sCalleeName = __xlCallCalleeName(pNode);
	if ( sCalleeName == NULL ) {
		return 0;
	}
	if ( !__xlFindGeneratorFuncType(pCtx, sCalleeName, psValueTypeName) ) {
		return 0;
	}
	if ( psFuncName != NULL ) {
		*psFuncName = sCalleeName;
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
			__xlCgenExpr(pCtx, (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL);
			__xlCgenAppend(&pCtx->buf, ";\n");
			return;
		case XL_MIR_BRANCH:
			pCond = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
			pThenBlock = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
			pElseBlock = (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "if ( ");
			__xlCgenExpr(pCtx, pCond);
			__xlCgenAppend(&pCtx->buf, " )\n");
			__xlCgenStmt(pCtx, pThenBlock, pFuncInfo);
			if ( pElseBlock != NULL ) {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "else\n");
				__xlCgenStmt(pCtx, pElseBlock, pFuncInfo);
			}
			return;
		case XL_MIR_FORIN:
			if ( __xlIsRangeCall((pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL) ) {
				__xlCgenForInRange(pCtx, pNode, pFuncInfo);
			} else if ( __xlIsGeneratorCall(pCtx, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, NULL, NULL) ) {
				__xlCgenForInGenerator(pCtx, pNode, pFuncInfo);
			} else {
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "/* for-in iterable is reserved for a later stage */\n");
			}
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
						__xlCgenExpr(pCtx, pNode->ppChildren[0]);
						__xlCgenAppend(&pCtx->buf, ";\n");
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

			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppend(&pCtx->buf, "return");
			if ( pNode->iChildCount != 0 ) {
				__xlCgenAppend(&pCtx->buf, " ");
				__xlCgenExpr(pCtx, pNode->ppChildren[0]);
			}
			__xlCgenAppend(&pCtx->buf, ";\n");
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
			__xlCgenAppendIndent(&pCtx->buf);
			if ( (pFuncInfo != NULL) && pFuncInfo->bGenerator ) {
				__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", pFuncInfo->sGeneratorDoneLabel);
				return;
			}
			if ( (pFuncInfo != NULL) && pFuncInfo->bAsync ) {
				if ( (pFuncInfo->sTypeName == NULL) || (strcmp(pFuncInfo->sTypeName, "void") == 0) ) {
					__xlCgenAppendFormat(&pCtx->buf, "return %s();\n", __xlFutureReadyFunc("void"));
				} else {
					sDefaultReturn = __xlCDefaultReturnValue(pFuncInfo->sTypeName);
					__xlCgenAppendFormat(&pCtx->buf, "return %s(", __xlFutureReadyFunc(pFuncInfo->sTypeName));
					if ( sDefaultReturn != NULL ) {
						__xlCgenAppend(&pCtx->buf, sDefaultReturn);
					}
					__xlCgenAppend(&pCtx->buf, ");\n");
				}
				return;
			}

			sDefaultReturn = __xlCDefaultReturnValue((pFuncInfo != NULL) ? pFuncInfo->sTypeName : "void");
			if ( sDefaultReturn == NULL ) {
				__xlCgenAppend(&pCtx->buf, "return;\n");
			} else {
				__xlCgenAppendFormat(&pCtx->buf, "return %s;\n", sDefaultReturn);
			}
			return;
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

	__xlCgenAppendFormat(
		&pCtx->buf,
		"static %s %s(",
		funcInfo.bGenerator ? __xlGeneratorTypeName(funcInfo.sTypeName) :
			(funcInfo.bAsync ? __xlFutureTypeName(funcInfo.sTypeName) : __xlCTypeName(funcInfo.sTypeName)),
		funcInfo.sName
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

	sTypeName = ((pEntryFunc != NULL) && (pEntryFunc->sTypeName != NULL) && (pEntryFunc->sTypeName[0] != '\0')) ? pEntryFunc->sTypeName : "void";
	sEntryName = ((pEntryFunc != NULL) && (pEntryFunc->sText != NULL)) ? pEntryFunc->sText : "main";

	__xlCgenAppend(&pCtx->buf, "int xl_generated_entry(void)\n{\n");
	__xlCgenAppend(&pCtx->buf, "\tint xlrt_attached_here = xlrt_attach_generated_runtime();\n");
	__xlCgenAppend(&pCtx->buf, "\tint xlrt_result = 0;\n");
	__xlCgenAppend(&pCtx->buf, "\txl_generated_runtime_reset();\n");
	if ( (pEntryFunc != NULL) && pEntryFunc->bFlag ) {
		if ( strcmp(sTypeName, "void") == 0 ) {
			__xlCgenAppendFormat(
				&pCtx->buf,
				"\t%s(",
				__xlFutureAwaitFunc("void")
			);
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntryName);
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
			return;
		}
		if ( (strcmp(sTypeName, "int") == 0) || (strcmp(sTypeName, "bool") == 0) ) {
			__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = %s(", __xlFutureAwaitFunc(sTypeName));
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntryName);
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
			return;
		}
		if ( strcmp(sTypeName, "float") == 0 ) {
			__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = (int)%s(", __xlFutureAwaitFunc(sTypeName));
			__xlCgenAppendCString(&pCtx->buf, sEntryName);
			__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntryName);
			__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
			__xlCgenAppend(&pCtx->buf, "\treturn xlrt_result;\n}\n");
			return;
		}

		__xlCgenAppendFormat(&pCtx->buf, "\t%s(", __xlFutureAwaitFunc(sTypeName));
		__xlCgenAppendCString(&pCtx->buf, sEntryName);
		__xlCgenAppendFormat(&pCtx->buf, ", %s());\n", sEntryName);
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
		return;
	}

	if ( (pEntryFunc != NULL) && (strcmp(sTypeName, "void") == 0) ) {
		__xlCgenAppendFormat(&pCtx->buf, "\t%s();\n", sEntryName);
		__xlCgenAppend(&pCtx->buf, "\txlrt_detach_generated_runtime(xlrt_attached_here);\n");
		__xlCgenAppend(&pCtx->buf, "\treturn 0;\n}\n");
		return;
	}

	if ( pEntryFunc != NULL ) {
		__xlCgenAppendFormat(&pCtx->buf, "\txlrt_result = %s();\n", sEntryName);
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
	__xlCgenAppend(&ctx.buf, "#include \"xrt.h\"\n");
	__xlCgenAppend(&ctx.buf, "#include \"xlrt_generated.h\"\n\n");
	__xlCgenRuntimeSupport(&ctx.buf);

	for ( i = 0; i < pRoot->iChildCount; ++i ) {
		if ( pRoot->ppChildren[i]->eKind == XL_MIR_FUNC ) {
			__xlCgenFunc(&ctx, pRoot->ppChildren[i]);
		} else {
			__xlCgenStmt(&ctx, pRoot->ppChildren[i], NULL);
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
