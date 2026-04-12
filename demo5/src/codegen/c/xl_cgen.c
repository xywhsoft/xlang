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
	int* pGeneratorResumeState;
} XL_CgenFuncInfo;

typedef struct XL_CgenGeneratorField {
	const char* sName;
	const char* sTypeName;
} XL_CgenGeneratorField;

typedef struct XL_CgenGeneratorTryFrameField {
	char sName[64];
} XL_CgenGeneratorTryFrameField;

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
	return (pFunc != NULL) &&
		(pFunc->eKind == XL_MIR_FUNC) &&
		!__xlIsGeneratorFuncNode(pFunc) &&
		((pFunc->sTypeName == NULL) || (strcmp(pFunc->sTypeName, "generator") != 0)) &&
		!pFunc->bFlag;
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
		"static %s xl_task_all_%s(const char* sName, %s left, %s right)\n{\n\t%s future;\n\txfuture* arrFuture[2];\n\txfuture* pAll;\n\txl_generated_task_trace_event(\"all\", (sName != NULL) ? sName : \"\");\n\tif ( (left.handle == NULL) || (right.handle == NULL) ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.all received an empty future\", 0, \"\", \"\"));\n\t}\n\tarrFuture[0] = left.handle;\n\tarrFuture[1] = right.handle;\n\tpAll = xFutureWhenAll(arrFuture, 2);\n\tif ( pAll == NULL ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.all could not create group future\", 0, \"\", \"\"));\n\t}\n\tfuture.handle = xFutureThenInline(pAll, xl_generated_future_all_pick_last, NULL);\n\tif ( future.handle == NULL ) {\n\t\treturn %s(xl_generated_error_make(\"Error\", \"task.all could not map group future\", 0, \"\", \"\"));\n\t}\n\treturn future;\n}\n\n",
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
	bFound = 0;
	if ( !__xlCollectGeneratorReturnSourceInfo(pCtx, pBody, ppFuncNode, psValueTypeName, psFuncName, &bFound, iDepth + 1) ) {
		return 0;
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
		pDecl = __xlFindMirSymbolDeclNode(pCtx->pRoot, pNode->iSymbolIndex);
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

	iResumeState = 1;
	bExternalResumeState = 0;
	if ( (pCtx != NULL) && (pCtx->iGeneratorExprResumeState > 0) ) {
		iResumeState = pCtx->iGeneratorExprResumeState;
		bExternalResumeState = 1;
	}
	if ( !bExternalResumeState && (pFuncInfo->pGeneratorResumeState != NULL) ) {
		iResumeState = *pFuncInfo->pGeneratorResumeState;
		*pFuncInfo->pGeneratorResumeState += 1;
	}
	snprintf(sResumeResultName, sizeof(sResumeResultName), "xl_gen_resume_result_%d_%d", (pNode != NULL) ? pNode->iLine : 0, (pNode != NULL) ? pNode->iColumn : 0);

	if ( bExternalResumeState ) {
		__xlCgenAppend(&pCtx->buf, "({ ");
		__xlCgenAppendFormat(&pCtx->buf, "xvalue %s = NULL; ", sResumeResultName);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"if ( %s->state == %d ) { ",
			(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
			iResumeState
		);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s->state = 0; ",
			(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen"
		);
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s = ((xl_gen_resume_value != NULL) ? xvoCopy((xvalue)(xl_gen_resume_value)) : xvoCreateNull()); ",
			sResumeResultName
		);
		__xlCgenAppend(&pCtx->buf, "} else { ");
		__xlCgenAppendFormat(
			&pCtx->buf,
			"%s->state = %d; *(%s) = ",
			(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
			iResumeState,
			(pFuncInfo->sGeneratorValueOutName != NULL) ? pFuncInfo->sGeneratorValueOutName : "pValue"
		);
		if ( (pNode != NULL) && (pNode->iChildCount != 0) ) {
			__xlCgenExpr(pCtx, pNode->ppChildren[0]);
		} else {
			__xlCgenAppend(&pCtx->buf, __xlCDefaultReturnValue(pFuncInfo->sTypeName));
		}
		__xlCgenAppend(&pCtx->buf, "; ");
		__xlCgenPropagatePendingException(pCtx, pFuncInfo);
		__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"yield\", ");
		__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
		__xlCgenAppend(&pCtx->buf, "); ");
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
		"%s->state = %d; *(%s) = ",
		(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
		iResumeState,
		(pFuncInfo->sGeneratorValueOutName != NULL) ? pFuncInfo->sGeneratorValueOutName : "pValue"
	);
	if ( (pNode != NULL) && (pNode->iChildCount != 0) ) {
		__xlCgenExpr(pCtx, pNode->ppChildren[0]);
	} else {
		__xlCgenAppend(&pCtx->buf, __xlCDefaultReturnValue(pFuncInfo->sTypeName));
	}
	__xlCgenAppend(&pCtx->buf, "; ");
	__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"yield\", ");
	__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
	__xlCgenAppend(&pCtx->buf, "); ");
	__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"next\", ");
	__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
	__xlCgenAppend(&pCtx->buf, "); ");
	__xlCgenAppend(&pCtx->buf, "return 1; ");
	__xlCgenAppendFormat(&pCtx->buf, "xl_gen_resume_%d: ", iResumeState);
	__xlCgenAppendFormat(
		&pCtx->buf,
		"%s->state = 0; ",
		(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen"
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

	__xlCgenAppendFormat(&pCtx->buf, "%s %s = %s; ", __xlGeneratorItemCType(sValueTypeName), sValueName, __xlCDefaultReturnValue(sValueTypeName));
	if ( sGeneratorPtrExpr != NULL ) {
		__xlCgenAppendFormat(
			&pCtx->buf,
			"int %s = %s(",
			sOkName,
			sGeneratorNextFuncName
		);
		__xlCgenAppendCString(&pCtx->buf, sFuncName);
		__xlCgenAppendFormat(&pCtx->buf, ", %s, &%s, %s); ", sGeneratorPtrExpr, sValueName, sResumeArgName);
	} else {
		__xlCgenAppendFormat(&pCtx->buf, "int %s = 0; ", sOkName);
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
		__xlCgenAppend(&pCtx->buf, "xvoCreateNull()");
	}
	__xlCgenAppend(&pCtx->buf, ", TRUE); ");
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
	XL_CoreLenHelperKind eCoreLenKind;
	XL_TimeHelperKind eTimeKind;
	XL_PathHelperKind ePathKind;
	XL_FsHelperKind eFsKind;
	XL_StrHelperKind eStrKind;
	XL_RandHelperKind eRandKind;
	XL_HostHelperKind eHostKind;
	XL_FmtHelperKind eFmtKind;
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
			case XL_RAND_HELPER_RANGE: __xlCgenAppendFormat(&pCtx->buf, "return xl_rand_range(%s, %s);\n", sParam0, sParam1); break;
			case XL_RAND_HELPER_TEXT: __xlCgenAppendFormat(&pCtx->buf, "return xl_rand_text(%s);\n", sParam0); break;
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
			default: break;
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
			case XL_ENV_HELPER_CWD: __xlCgenAppend(&pCtx->buf, "return xl_env_cwd();\n"); break;
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
			case XL_DIR_HELPER_LIST: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_list(%s);\n", sParam0); break;
			case XL_DIR_HELPER_FILES: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_files(%s);\n", sParam0); break;
			case XL_DIR_HELPER_DIRS: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_dirs(%s);\n", sParam0); break;
			case XL_DIR_HELPER_TREE: __xlCgenAppendFormat(&pCtx->buf, "return xl_dir_tree(%s);\n", sParam0); break;
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
	const char* sAsyncTypeName;
	const char* sCalleeName;
	const char* sInnerTypeName;
	const char* sTraceName;
	char sFutureValueType[128];

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

	sInnerTypeName = __xlInferExprTypeName(pCtx, pInner);
	if ( __xlExtractFutureValueType(sInnerTypeName, sFutureValueType, sizeof(sFutureValueType)) ) {
		sTraceName = "";
		if ( sCalleeName != NULL ) {
			sTraceName = sCalleeName;
		} else if ( (pInner != NULL) && (pInner->sText != NULL) ) {
			sTraceName = pInner->sText;
		}
		__xlCgenAppendFormat(&pCtx->buf, "%s(", __xlFutureAwaitFunc(sFutureValueType));
		__xlCgenAppendCString(&pCtx->buf, sTraceName);
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
	const char* sValueTypeName;
	const char* sFuncName;
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
	pGeneratorFunc = NULL;

	if ( !__xlIsGeneratorCall(pCtx, pIterable, &pGeneratorFunc, &sValueTypeName, &sFuncName) ) {
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "/* generator iterable is reserved for a later stage */\n");
		return;
	}

	snprintf(sGeneratorName, sizeof(sGeneratorName), "xl_gen_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sNextFlagName, sizeof(sNextFlagName), "xl_gen_has_next_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sIndexName, sizeof(sIndexName), "xl_gen_index_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sCurrentValueName, sizeof(sCurrentValueName), "xl_gen_value_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sBreakLabel, sizeof(sBreakLabel), "xl_for_break_%d_%d", pNode->iLine, pNode->iColumn);
	snprintf(sContinueLabel, sizeof(sContinueLabel), "xl_for_continue_%d_%d", pNode->iLine, pNode->iColumn);
	__xlCgenGeneratorTypeNameForFunc(sGeneratorTypeName, sizeof(sGeneratorTypeName), pGeneratorFunc);
	__xlCgenGeneratorNextFuncNameForFunc(sGeneratorNextFuncName, sizeof(sGeneratorNextFuncName), pGeneratorFunc);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s = ", sGeneratorTypeName, sGeneratorName);
	__xlCgenExpr(pCtx, pIterable);
	__xlCgenAppend(&pCtx->buf, ";\n");
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s %s;\n", __xlGeneratorItemCType(sValueTypeName), sCurrentValueName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "int %s = 0;\n", sNextFlagName);

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "for ( int %s = 0; ; ++%s )\n", sIndexName, sIndexName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppend(&pCtx->buf, "{\n");
	pCtx->buf.iIndent += 1;

	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "%s = %s(", sNextFlagName, sGeneratorNextFuncName);
	__xlCgenAppendCString(&pCtx->buf, sFuncName);
	__xlCgenAppendFormat(&pCtx->buf, ", &%s, &%s, NULL);\n", sGeneratorName, sCurrentValueName);
	__xlCgenPropagatePendingException(pCtx, pFuncInfo);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "if ( !%s )\n", sNextFlagName);
	__xlCgenAppendIndent(&pCtx->buf);
	__xlCgenAppendFormat(&pCtx->buf, "goto %s;\n", sBreakLabel);

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
		if ( (iNestedYieldCount == 1) && (pFuncInfo->pGeneratorResumeState != NULL) ) {
			pCtx->iGeneratorExprResumeState = *pFuncInfo->pGeneratorResumeState;
			*pFuncInfo->pGeneratorResumeState += 1;
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
				}
			} else {
				__xlCgenAppendFormat(
					&pCtx->buf,
					"%s %s = ",
					__xlCTypeName(((pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0')) ? pNode->sTypeName : "int"),
					(pNode->sText != NULL) ? pNode->sText : "tmp"
				);
			}
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
			} else if ( __xlIsGeneratorCall(pCtx, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL, NULL, NULL, NULL) ) {
				__xlCgenForInGenerator(pCtx, pNode, pFuncInfo);
			} else {
				__xlCgenForInContainer(pCtx, pNode, pFuncInfo);
			}
			return;
		case XL_MIR_WHILE:
			__xlCgenWhile(pCtx, pNode, pFuncInfo);
			return;
		case XL_MIR_RETURN:
		{
			const XL_CgenFinallyFrame* pFinallyFrame;

			pFinallyFrame = __xlCgenCurrentFinallyFrame(pCtx);
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
				__xlCgenExprAsType(
					pCtx,
					pNode->ppChildren[0],
					(pFuncInfo != NULL) ? pFuncInfo->sTypeName : pNode->ppChildren[0]->sTypeName
				);
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

				iResumeState = 1;
				if ( pFuncInfo->pGeneratorResumeState != NULL ) {
					iResumeState = *pFuncInfo->pGeneratorResumeState;
					*pFuncInfo->pGeneratorResumeState += 1;
				}
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"%s->state = %d;\n",
					(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen",
					iResumeState
				);
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
				__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"yield\", ");
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppend(&pCtx->buf, ");\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"next\", ");
				__xlCgenAppendCString(&pCtx->buf, pFuncInfo->sName);
				__xlCgenAppend(&pCtx->buf, ");\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppend(&pCtx->buf, "return 1;\n");
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(&pCtx->buf, "xl_gen_resume_%d:\n", iResumeState);
				__xlCgenAppendIndent(&pCtx->buf);
				__xlCgenAppendFormat(
					&pCtx->buf,
					"%s->state = 0;\n",
					(pFuncInfo->sGeneratorSelfName != NULL) ? pFuncInfo->sGeneratorSelfName : "pGen"
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
	int iGeneratorFieldCount;
	int iGeneratorTryFrameFieldCount;
	int iGeneratorYieldCount;
	int iGeneratorResumeState;
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
	iGeneratorResumeState = 1;
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
	funcInfo.pGeneratorResumeState = &iGeneratorResumeState;

	pParams = (pNode->iChildCount != 0) ? pNode->ppChildren[0] : NULL;
	pBody = (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL;
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
		__xlCgenAppend(&pCtx->buf, "\tint state;\n");
		__xlCgenAppend(&pCtx->buf, "\tint done;\n");
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
		__xlCgenAppend(&pCtx->buf, "if ( pGen->done ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"eof\", (sName != NULL) ? sName : \"\");\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "return 0;\n");
		pCtx->buf.iIndent -= 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "}\n");
		__xlCgenGeneratorFieldAliasLines(pCtx, arrGeneratorFields, iGeneratorFieldCount, "pGen", 1);
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "switch ( pGen->state ) {\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "case 0:\n");
		pCtx->buf.iIndent += 1;
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "break;\n");
		pCtx->buf.iIndent -= 1;
		for ( i = 1; i <= (size_t)iGeneratorYieldCount; ++i ) {
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "case %zu:\n", i);
			pCtx->buf.iIndent += 1;
			__xlCgenAppendIndent(&pCtx->buf);
			__xlCgenAppendFormat(&pCtx->buf, "goto xl_gen_resume_%zu;\n", i);
			pCtx->buf.iIndent -= 1;
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
		__xlCgenAppend(&pCtx->buf, "pGen->done = 1;\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "pGen->state = -1;\n");
		__xlCgenAppendIndent(&pCtx->buf);
		__xlCgenAppend(&pCtx->buf, "xl_generated_generator_trace_event(\"done\", (sName != NULL) ? sName : \"\");\n");
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

int xlCgenMir(const XL_MirNode* pRoot, const XL_ModuleLinkResult* pModules, XL_DiagContext* pDiag, char** psOutCode)
{
	XL_CgenContext ctx;
	const XL_MirNode* pEntryFunc;
	size_t i;

	if ( (pRoot == NULL) || (psOutCode == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&ctx, 0, sizeof(ctx));
	ctx.pRoot = pRoot;
	ctx.pModules = pModules;

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
