#include "xl_module_link.h"
#include "../common/xl_app_path.h"
#include "xl_xlp_load.h"

#include "../common/xl_path.h"
#include "../lexer/xl_lexer_api.h"
#include "../parser/xl_parser_api.h"
#include "../runtime/xl_native_registry.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

#if !defined(_WIN32) && !defined(_WIN64)
	#define strtok_s strtok_r
#endif

#define XL_PROJECT_PATH_MAX 32
#define XL_BINDING_TYPE_MAX 96
#define XL_BINDING_CONST_MAX 192
#define XL_BINDING_MACRO_MAX 192
#define XL_BINDING_DECL_MACRO_MAX 64
#define XL_BINDING_COND_MAX 32
#define XL_BINDING_OPAQUE_ALIAS_MAX 64

typedef struct XL_ModuleTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_ModuleTextBuffer;

typedef struct XL_BindingTypeInfo {
	char sCName[96];
	char sXlName[96];
	int bStruct;
} XL_BindingTypeInfo;

typedef struct XL_BindingConstInfo {
	char sName[96];
	long long iValue;
} XL_BindingConstInfo;

typedef struct XL_BindingOpaqueAliasInfo {
	char sKind[16];
	char sTag[96];
	char sAlias[96];
} XL_BindingOpaqueAliasInfo;

typedef struct XL_BindingContext {
	XL_BindingTypeInfo arrTypes[XL_BINDING_TYPE_MAX];
	size_t iTypeCount;
	XL_BindingConstInfo arrConsts[XL_BINDING_CONST_MAX];
	size_t iConstCount;
	char arrStringConsts[XL_BINDING_CONST_MAX][96];
	size_t iStringConstCount;
	char arrFloatConsts[XL_BINDING_CONST_MAX][96];
	size_t iFloatConstCount;
	XL_BindingOpaqueAliasInfo arrOpaqueAliases[XL_BINDING_OPAQUE_ALIAS_MAX];
	size_t iOpaqueAliasCount;
	char arrMacros[XL_BINDING_MACRO_MAX][96];
	size_t iMacroCount;
	char arrDeclMacros[XL_BINDING_DECL_MACRO_MAX][96];
	size_t iDeclMacroCount;
	char arrIncludedHeaders[64][XL_PATH_MAX_LEN];
	size_t iIncludedHeaderCount;
	XL_DiagContext* pDiag;
	const char* sHeaderPath;
	size_t iFunctionCount;
	size_t iVarCount;
	size_t iTypeDeclCount;
	size_t iDefineCount;
	size_t iEnumCount;
	size_t iSkipCount;
	int bTrace;
} XL_BindingContext;

typedef struct XL_BindingCondFrame {
	int bParentActive;
	int bBranchActive;
	int bAnyTaken;
	int bElseSeen;
} XL_BindingCondFrame;

typedef struct XL_ModuleNameMap {
	char* sOriginalName;
	char* sMangledName;
} XL_ModuleNameMap;

typedef struct XL_ModuleExportInfo {
	char* sName;
	char* sMangledName;
} XL_ModuleExportInfo;

typedef struct XL_ModuleInfo {
	char* sModuleKey;
	char* sFilePath;
	char* sLeafAlias;
	char* sBindingName;
	char* sInitSymbol;
	char* sExportsSymbol;
	char* sDescriptorSymbol;
	char* sManifestPath;
	char* sPackageName;
	char* sPackageVersion;
	char* sPackageAbiVersion;
	char* sPackageAbiId;
	char* sArtifactKind;
	char* sArtifactPath;
	char* sArtifactFile;
	char* sConsumeMode;
	XL_AstNode* pRoot;
	char** arrImportKeys;
	char** arrImportAliases;
	char** arrResolvedImportFiles;
	size_t iImportCount;
	size_t iImportCap;
	char** arrLinkLibs;
	size_t iLinkLibCount;
	size_t iLinkLibCap;
	char** arrLibPaths;
	size_t iLibPathCount;
	size_t iLibPathCap;
	char** arrIncPaths;
	size_t iIncPathCount;
	size_t iIncPathCap;
	char** arrLoadLibs;
	size_t iLoadLibCount;
	size_t iLoadLibCap;
	XL_ModuleNameMap* arrNames;
	size_t iNameCount;
	size_t iNameCap;
	XL_ModuleExportInfo* arrExports;
	size_t iExportCount;
	size_t iExportCap;
	int bIsRoot;
	int bIsNative;
	int iState;
} XL_ModuleInfo;

typedef struct XL_ModuleScope {
	char** arrNames;
	size_t iCount;
	size_t iCap;
	struct XL_ModuleScope* pPrev;
} XL_ModuleScope;

typedef struct XL_ModuleLinkContext {
	XL_DiagContext* pDiag;
	int bTraceParser;
	int bHasProject;
	char sProjectFile[XL_PATH_MAX_LEN];
	char sProjectRoot[XL_PATH_MAX_LEN];
	char sProjectMain[XL_PATH_MAX_LEN];
	char sProjectType[32];
	char arrSourceRoots[XL_PROJECT_PATH_MAX][XL_PATH_MAX_LEN];
	size_t iSourceRootCount;
	char arrPackageRoots[XL_PROJECT_PATH_MAX][XL_PATH_MAX_LEN];
	size_t iPackageRootCount;
	char arrProjectLinks[XL_PROJECT_PATH_MAX][128];
	size_t iProjectLinkCount;
	char arrProjectExports[XL_PROJECT_PATH_MAX][XL_PATH_MAX_LEN];
	size_t iProjectExportCount;
	char arrProjectLibPaths[XL_PROJECT_PATH_MAX][XL_PATH_MAX_LEN];
	size_t iProjectLibPathCount;
	char arrProjectIncPaths[XL_PROJECT_PATH_MAX][XL_PATH_MAX_LEN];
	size_t iProjectIncPathCount;
	char sLocalPackageName[XL_PATH_MAX_LEN];
	char sLocalPackageVersion[32];
	char sLocalPackageAbiVersion[32];
	char sLocalPackageAbiId[XL_PATH_MAX_LEN];
	XL_ModuleInfo* pModules;
	size_t iModuleCount;
	size_t iModuleCap;
	size_t* pOrderIndices;
	size_t iOrderCount;
	size_t iOrderCap;
} XL_ModuleLinkContext;

static void __xlModuleNormalizePath(char* sOut, size_t iOutSize, const char* sPath);
static void __xlModuleStripFileName(char* sPath);

static int __xlModuleBufGrow(XL_ModuleTextBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pBuf->iCap == 0) ? 256 : pBuf->iCap;
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

static void __xlModuleBufAppend(XL_ModuleTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlModuleBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static int __xlModuleBufAppendN(XL_ModuleTextBuffer* pBuf, const char* sText, size_t iLen)
{
	if ( (pBuf == NULL) || ((sText == NULL) && (iLen != 0)) ) {
		return XL_RC_BAD_ARG;
	}

	if ( __xlModuleBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	if ( iLen != 0 ) {
		memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
		pBuf->iLen += iLen;
	}
	pBuf->sData[pBuf->iLen] = '\0';
	return XL_RC_OK;
}

static void __xlModuleBufAppendFormat(XL_ModuleTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlModuleBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlModuleBufAppendEscaped(XL_ModuleTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlModuleBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlModuleBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlModuleBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlModuleBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlModuleBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlModuleBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];

					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlModuleBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static char* __xlModuleDupText(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	sCopy = (char*)malloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}

	memcpy(sCopy, sText, iLen + 1);
	return sCopy;
}

static int __xlIsAsciiSpace(int ch)
{
	return (ch == ' ') || (ch == '\t') || (ch == '\v') || (ch == '\f');
}

static int __xlIsLinkIdentStart(int ch)
{
	return ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z')) || (ch == '_');
}

static int __xlIsLinkIdentCont(int ch)
{
	return __xlIsLinkIdentStart(ch) || ((ch >= '0') && (ch <= '9'));
}

static int __xlIsLinkLibNameCont(int ch)
{
	return __xlIsLinkIdentCont(ch) || (ch == '.') || (ch == '-');
}

static void __xlBindingTrim(char* sText);
static void __xlBindingCollapseSpaces(char* sText);
static int __xlBindingIsIdentText(const char* sText);
static void __xlBindingTrace(XL_BindingContext* pCtx, const char* sFormat, ...);

static int __xlBindingParamTextIsNamelessType(const char* sType)
{
	static const char* arrBuiltinTypes[] = {
		"signed char", "unsigned char",
		"signed short", "signed short int", "unsigned short", "unsigned short int",
		"signed int", "unsigned int",
		"signed long", "signed long int", "unsigned long", "unsigned long int",
		"long long", "long long int", "signed long long", "signed long long int",
		"unsigned long long", "unsigned long long int"
	};
	char sWork[160];
	char* p;
	size_t i;

	if ( (sType == NULL) || (sType[0] == '\0') ) {
		return 0;
	}
	snprintf(sWork, sizeof(sWork), "%s", sType);
	__xlBindingTrim(sWork);
	for ( p = sWork; *p != '\0'; ++p ) {
		if ( (*p >= 'A') && (*p <= 'Z') ) {
			*p = (char)(*p - 'A' + 'a');
		}
	}
	__xlBindingCollapseSpaces(sWork);
	while ( strncmp(sWork, "const ", 6) == 0 ) {
		memmove(sWork, sWork + 6, strlen(sWork + 6) + 1);
		__xlBindingTrim(sWork);
	}
	while ( strncmp(sWork, "volatile ", 9) == 0 ) {
		memmove(sWork, sWork + 9, strlen(sWork + 9) + 1);
		__xlBindingTrim(sWork);
	}
	for ( i = 0; i < sizeof(arrBuiltinTypes) / sizeof(arrBuiltinTypes[0]); ++i ) {
		if ( strcmp(sWork, arrBuiltinTypes[i]) == 0 ) {
			return 1;
		}
	}
	if ( (strncmp(sWork, "struct ", 7) == 0) || (strncmp(sWork, "union ", 6) == 0) || (strncmp(sWork, "enum ", 5) == 0) ) {
		const char* pName = sWork + ((sWork[0] == 's') ? 7 : ((sWork[0] == 'u') ? 6 : 5));
		return __xlBindingIsIdentText(pName);
	}
	return 0;
}

static int __xlModulePathIsAbsolute(const char* sPath)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		return 0;
	}
	if ( xlPathIsSep((unsigned char)sPath[0]) ) {
		return 1;
	}
	if ( ((sPath[0] >= 'A') && (sPath[0] <= 'Z')) || ((sPath[0] >= 'a') && (sPath[0] <= 'z')) ) {
		return (sPath[1] == ':') ? 1 : 0;
	}
	return 0;
}

static int __xlModuleInfoAddLinkLib(XL_ModuleInfo* pModule, const char* sLibName)
{
	char** arrNew;
	size_t i;
	size_t iNewCap;

	if ( (pModule == NULL) || (sLibName == NULL) || (sLibName[0] == '\0') ) {
		return XL_RC_OK;
	}

	for ( i = 0; i < pModule->iLinkLibCount; ++i ) {
		if ( strcmp(pModule->arrLinkLibs[i], sLibName) == 0 ) {
			return XL_RC_OK;
		}
	}

	if ( pModule->iLinkLibCount >= pModule->iLinkLibCap ) {
		iNewCap = (pModule->iLinkLibCap == 0) ? 4 : (pModule->iLinkLibCap * 2);
		arrNew = (char**)realloc(pModule->arrLinkLibs, sizeof(char*) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pModule->arrLinkLibs = arrNew;
		pModule->iLinkLibCap = iNewCap;
	}

	pModule->arrLinkLibs[pModule->iLinkLibCount] = __xlModuleDupText(sLibName);
	if ( pModule->arrLinkLibs[pModule->iLinkLibCount] == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pModule->iLinkLibCount += 1;
	return XL_RC_OK;
}

static int __xlModuleInfoAddTextItem(char*** pArr, size_t* piCount, size_t* piCap, const char* sText)
{
	char** arrNew;
	size_t i;
	size_t iNewCap;

	if ( (pArr == NULL) || (piCount == NULL) || (piCap == NULL) || (sText == NULL) || (sText[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < *piCount; ++i ) {
		if ( strcmp((*pArr)[i], sText) == 0 ) {
			return XL_RC_OK;
		}
	}
	if ( *piCount >= *piCap ) {
		iNewCap = (*piCap == 0) ? 4 : (*piCap * 2);
		arrNew = (char**)realloc(*pArr, sizeof(char*) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		*pArr = arrNew;
		*piCap = iNewCap;
	}
	(*pArr)[*piCount] = __xlModuleDupText(sText);
	if ( (*pArr)[*piCount] == NULL ) {
		return XL_RC_IO_ERROR;
	}
	*piCount += 1;
	return XL_RC_OK;
}

static int __xlModuleInfoAddLibPath(XL_ModuleInfo* pModule, const char* sPath)
{
	if ( pModule == NULL ) {
		return XL_RC_BAD_ARG;
	}
	return __xlModuleInfoAddTextItem(&pModule->arrLibPaths, &pModule->iLibPathCount, &pModule->iLibPathCap, sPath);
}

static int __xlModuleInfoAddIncPath(XL_ModuleInfo* pModule, const char* sPath)
{
	if ( pModule == NULL ) {
		return XL_RC_BAD_ARG;
	}
	return __xlModuleInfoAddTextItem(&pModule->arrIncPaths, &pModule->iIncPathCount, &pModule->iIncPathCap, sPath);
}

static int __xlModuleInfoAddLoadLib(XL_ModuleInfo* pModule, const char* sFile)
{
	if ( pModule == NULL ) {
		return XL_RC_BAD_ARG;
	}
	return __xlModuleInfoAddTextItem(&pModule->arrLoadLibs, &pModule->iLoadLibCount, &pModule->iLoadLibCap, sFile);
}

static int __xlExtractQuotedDirectiveValue(
	const char* sText,
	size_t iStart,
	size_t iEnd,
	char* sOut,
	size_t iOutSize
)
{
	size_t iPos;
	size_t iOut;
	char chQuote;

	if ( (sText == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return XL_RC_BAD_ARG;
	}
	iPos = iStart;
	while ( (iPos < iEnd) && __xlIsAsciiSpace((unsigned char)sText[iPos]) ) {
		++iPos;
	}
	if ( (iPos >= iEnd) || ((sText[iPos] != '"') && (sText[iPos] != '\'')) ) {
		return XL_RC_FAIL;
	}
	chQuote = sText[iPos++];
	iOut = 0;
	while ( iPos < iEnd ) {
		char ch;

		ch = sText[iPos++];
		if ( ch == chQuote ) {
			while ( (iPos < iEnd) && __xlIsAsciiSpace((unsigned char)sText[iPos]) ) {
				++iPos;
			}
			if ( (iPos != iEnd) &&
				!((sText[iPos] == '/') && ((iPos + 1) < iEnd) && (sText[iPos + 1] == '/')) ) {
				return XL_RC_FAIL;
			}
			sOut[iOut] = '\0';
			return XL_RC_OK;
		}
		if ( ch == '\\' ) {
			if ( iPos >= iEnd ) {
				return XL_RC_FAIL;
			}
			ch = sText[iPos++];
		}
		if ( (iOut + 1) >= iOutSize ) {
			return XL_RC_FAIL;
		}
		sOut[iOut++] = ch;
	}
	return XL_RC_FAIL;
}

static int __xlNormalizeDirectivePath(const char* sModulePath, const char* sValue, char* sOut, size_t iOutSize)
{
	char sBaseDir[XL_PATH_MAX_LEN];
	char sJoined[XL_PATH_MAX_LEN];

	if ( (sValue == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return XL_RC_BAD_ARG;
	}
	if ( sValue[0] == '\0' ) {
		sOut[0] = '\0';
		return XL_RC_OK;
	}
	if ( __xlModulePathIsAbsolute(sValue) ) {
		__xlModuleNormalizePath(sOut, iOutSize, sValue);
		return XL_RC_OK;
	}
	snprintf(sBaseDir, sizeof(sBaseDir), "%s", (sModulePath != NULL) ? sModulePath : "");
	__xlModuleStripFileName(sBaseDir);
	xlPathJoin(sJoined, sizeof(sJoined), sBaseDir, sValue);
	__xlModuleNormalizePath(sOut, iOutSize, sJoined);
	return XL_RC_OK;
}

static int __xlDirectiveNeedsRelativePathResolution(const char* sValue)
{
	const char* pScan;

	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return 0;
	}
	if ( __xlModulePathIsAbsolute(sValue) ) {
		return 1;
	}
	if ( sValue[0] == '.' ) {
		return 1;
	}
	for ( pScan = sValue; *pScan != '\0'; ++pScan ) {
		if ( xlPathIsSep((unsigned char)(*pScan)) ) {
			return 1;
		}
	}
	return 0;
}

static int __xlEnvTruthy(const char* sName)
{
	const char* sValue;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}
	sValue = getenv(sName);
	return (sValue != NULL) && (sValue[0] != '\0') &&
		(strcmp(sValue, "0") != 0) &&
		(strcmp(sValue, "false") != 0) &&
		(strcmp(sValue, "FALSE") != 0) &&
		(strcmp(sValue, "off") != 0) &&
		(strcmp(sValue, "OFF") != 0);
}

static int __xlBindingFileExists(const char* sFilePath)
{
	FILE* pFile;

	if ( (sFilePath == NULL) || (sFilePath[0] == '\0') ) {
		return 0;
	}
	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return 0;
	}
	fclose(pFile);
	return 1;
}

static int __xlBindingReadFileText(const char* sFilePath, char** psText)
{
	FILE* pFile;
	long iSize;
	size_t iRead;
	char* sText;

	if ( (sFilePath == NULL) || (psText == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	*psText = NULL;
	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}
	if ( (fseek(pFile, 0, SEEK_END) != 0) || ((iSize = ftell(pFile)) < 0) || (fseek(pFile, 0, SEEK_SET) != 0) ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	sText = (char*)malloc((size_t)iSize + 1);
	if ( sText == NULL ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	iRead = fread(sText, 1, (size_t)iSize, pFile);
	fclose(pFile);
	if ( iRead != (size_t)iSize ) {
		free(sText);
		return XL_RC_IO_ERROR;
	}
	sText[iRead] = '\0';
	*psText = sText;
	return XL_RC_OK;
}

static void __xlBindingTrim(char* sText)
{
	size_t iStart;
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}
	iStart = 0;
	while ( __xlIsAsciiSpace((unsigned char)sText[iStart]) ) {
		++iStart;
	}
	if ( iStart != 0 ) {
		memmove(sText, sText + iStart, strlen(sText + iStart) + 1);
	}
	iLen = strlen(sText);
	while ( (iLen != 0) &&
		(__xlIsAsciiSpace((unsigned char)sText[iLen - 1]) || (sText[iLen - 1] == '\r') || (sText[iLen - 1] == '\n')) ) {
		sText[--iLen] = '\0';
	}
}

static void __xlBindingCollapseSpaces(char* sText)
{
	size_t iRead;
	size_t iWrite;
	int bSpace;

	if ( sText == NULL ) {
		return;
	}
	iRead = 0;
	iWrite = 0;
	bSpace = 0;
	while ( sText[iRead] != '\0' ) {
		if ( __xlIsAsciiSpace((unsigned char)sText[iRead]) ) {
			bSpace = 1;
		} else {
			if ( bSpace && (iWrite != 0) && (sText[iRead] != '*') && (sText[iRead] != ',') && (sText[iRead] != ')') ) {
				sText[iWrite++] = ' ';
			}
			sText[iWrite++] = sText[iRead];
			bSpace = 0;
		}
		++iRead;
	}
	sText[iWrite] = '\0';
}

static int __xlBindingDecayArrayParam(char* sType)
{
	char* pOpen;
	int bHadArray;

	if ( sType == NULL ) {
		return 0;
	}
	bHadArray = 0;
	while ( (pOpen = strchr(sType, '[')) != NULL ) {
		char* pClose = strchr(pOpen, ']');

		if ( pClose == NULL ) {
			return 0;
		}
		memmove(pOpen, pClose + 1, strlen(pClose + 1) + 1);
		bHadArray = 1;
	}
	if ( bHadArray ) {
		__xlBindingTrim(sType);
		if ( strchr(sType, '*') == NULL ) {
			strncat(sType, " *", 159 - strlen(sType));
		}
		__xlBindingCollapseSpaces(sType);
		__xlBindingTrim(sType);
	}
	return bHadArray;
}

static void __xlBindingRemoveTypeQualifiers(char* sType)
{
	static const char* arrQualifiers[] = { "const", "volatile" };
	size_t i;
	int bChanged;

	if ( sType == NULL ) {
		return;
	}
	do {
		bChanged = 0;
		for ( i = 0; i < sizeof(arrQualifiers) / sizeof(arrQualifiers[0]); ++i ) {
			size_t iLen = strlen(arrQualifiers[i]);
			char* pHit = sType;

			while ( (pHit = strstr(pHit, arrQualifiers[i])) != NULL ) {
				char cBefore = (pHit == sType) ? '\0' : pHit[-1];
				char cAfter = pHit[iLen];
				int bBeforeOk = (pHit == sType) || !__xlIsLinkIdentCont((unsigned char)cBefore);
				int bAfterOk = (cAfter == '\0') || !__xlIsLinkIdentCont((unsigned char)cAfter);

				if ( bBeforeOk && bAfterOk ) {
					memmove(pHit, pHit + iLen, strlen(pHit + iLen) + 1);
					bChanged = 1;
					break;
				}
				pHit += iLen;
			}
			if ( bChanged ) {
				break;
			}
		}
		if ( bChanged ) {
			__xlBindingCollapseSpaces(sType);
			__xlBindingTrim(sType);
		}
	} while ( bChanged );
}

static void __xlBindingRemoveParamName(char* sType)
{
	size_t iLen;
	size_t iEnd;
	size_t iStart;

	if ( sType == NULL ) {
		return;
	}
	__xlBindingTrim(sType);
	iLen = strlen(sType);
	if ( iLen == 0 ) {
		return;
	}
	iEnd = iLen;
	while ( (iEnd != 0) && __xlIsAsciiSpace((unsigned char)sType[iEnd - 1]) ) {
		--iEnd;
	}
	iStart = iEnd;
	while ( (iStart != 0) && __xlIsLinkIdentCont((unsigned char)sType[iStart - 1]) ) {
		--iStart;
	}
	if ( __xlBindingParamTextIsNamelessType(sType) ) {
		return;
	}
	if ( (iStart != iEnd) && (iStart != 0) ) {
		sType[iStart] = '\0';
	}
	__xlBindingTrim(sType);
}

static int __xlBindingRemoveBalancedCallLike(char* sType, const char* sPrefix)
{
	char* pHit;
	char* pScan;
	int iDepth;
	size_t iPrefixLen;

	if ( (sType == NULL) || (sPrefix == NULL) || (sPrefix[0] == '\0') ) {
		return 0;
	}
	pHit = strstr(sType, sPrefix);
	if ( pHit == NULL ) {
		return 0;
	}
	iPrefixLen = strlen(sPrefix);
	pScan = pHit + iPrefixLen;
	iDepth = 1;
	while ( *pScan != '\0' ) {
		if ( *pScan == '(' ) {
			++iDepth;
		} else if ( *pScan == ')' ) {
			--iDepth;
			if ( iDepth == 0 ) {
				++pScan;
				memmove(pHit, pScan, strlen(pScan) + 1);
				return 1;
			}
		}
		++pScan;
	}
	return 0;
}

static int __xlBindingIdentStartsWith(const char* sText, const char* sPrefix)
{
	size_t iLen;

	if ( (sText == NULL) || (sPrefix == NULL) ) {
		return 0;
	}
	iLen = strlen(sPrefix);
	return strncmp(sText, sPrefix, iLen) == 0;
}

static int __xlBindingRemoveSalAnnotation(char* sType)
{
	static const char* arrPrefixes[] = {
		"_In", "_Out", "_Inout", "_Outptr", "_Ret", "_Pre", "_Post",
		"_Field", "_Frees_ptr", "_Must_inspect_result", "_Success"
	};
	char* pHit;
	size_t i;

	if ( sType == NULL ) {
		return 0;
	}
	for ( pHit = sType; *pHit != '\0'; ++pHit ) {
		char* pScan;
		char* pRemoveEnd;
		int bKnown;

		if ( *pHit != '_' ) {
			continue;
		}
		bKnown = 0;
		for ( i = 0; i < sizeof(arrPrefixes) / sizeof(arrPrefixes[0]); ++i ) {
			if ( __xlBindingIdentStartsWith(pHit, arrPrefixes[i]) ) {
				bKnown = 1;
				break;
			}
		}
		if ( !bKnown ) {
			continue;
		}
		pScan = pHit;
		while ( __xlIsLinkIdentCont((unsigned char)*pScan) ) {
			++pScan;
		}
		if ( pScan == pHit ) {
			continue;
		}
		pRemoveEnd = pScan;
		while ( (*pRemoveEnd == ' ') || (*pRemoveEnd == '\t') ) {
			++pRemoveEnd;
		}
		if ( *pRemoveEnd == '(' ) {
			int iDepth = 1;

			++pRemoveEnd;
			while ( *pRemoveEnd != '\0' ) {
				if ( *pRemoveEnd == '(' ) {
					++iDepth;
				} else if ( *pRemoveEnd == ')' ) {
					--iDepth;
					if ( iDepth == 0 ) {
						++pRemoveEnd;
						break;
					}
				}
				++pRemoveEnd;
			}
			if ( iDepth != 0 ) {
				return 0;
			}
		} else {
			pRemoveEnd = pScan;
		}
		memmove(pHit, pRemoveEnd, strlen(pRemoveEnd) + 1);
		return 1;
	}
	return 0;
}

static void __xlBindingRemoveStorageWords(char* sType)
{
	static const char* arrWords[] = {
		"extern", "static", "inline", "__inline", "__inline__",
		"restrict", "__restrict", "__restrict__",
		"consteval", "constexpr",
		"__cdecl", "__stdcall", "__fastcall", "__thiscall", "__vectorcall",
		"__declspec(dllexport)", "__declspec(dllimport)",
		"WINAPI", "CALLBACK", "APIENTRY", "PASCAL", "NTAPI",
		"DECLSPEC_IMPORT", "DECLSPEC_EXPORT", "EXTERN_C",
		"_In_", "_In_opt_", "_Out_", "_Out_opt_", "_Inout_", "_Inout_opt_"
	};
	size_t i;
	int bChanged;

	if ( sType == NULL ) {
		return;
	}
	do {
		bChanged = 0;
		__xlBindingTrim(sType);
		if ( __xlBindingRemoveBalancedCallLike(sType, "__declspec(") ||
			__xlBindingRemoveBalancedCallLike(sType, "__attribute__(") ) {
			bChanged = 1;
			continue;
		}
		if ( __xlBindingRemoveSalAnnotation(sType) ) {
			bChanged = 1;
			continue;
		}
		for ( i = 0; i < sizeof(arrWords) / sizeof(arrWords[0]); ++i ) {
			size_t iLen = strlen(arrWords[i]);
			char* pHit = sType;

			while ( (pHit = strstr(pHit, arrWords[i])) != NULL ) {
				char cBefore = (pHit == sType) ? '\0' : pHit[-1];
				char cAfter = pHit[iLen];
				int bBeforeOk = (pHit == sType) || !__xlIsLinkIdentCont((unsigned char)cBefore);
				int bAfterOk = (cAfter == '\0') || !__xlIsLinkIdentCont((unsigned char)cAfter);

				if ( bBeforeOk && bAfterOk ) {
					memmove(pHit, pHit + iLen, strlen(pHit + iLen) + 1);
					bChanged = 1;
					break;
				}
				pHit += iLen;
			}
			if ( bChanged ) {
				break;
			}
		}
	} while ( bChanged );
	__xlBindingCollapseSpaces(sType);
	__xlBindingTrim(sType);
}

static int __xlBindingAddType(XL_BindingContext* pCtx, const char* sCName, const char* sXlName, int bStruct)
{
	char sNormCName[96];
	size_t i;
	char* p;

	if ( (pCtx == NULL) || (sCName == NULL) || (sCName[0] == '\0') || (sXlName == NULL) || (sXlName[0] == '\0') ) {
		return XL_RC_OK;
	}
	snprintf(sNormCName, sizeof(sNormCName), "%s", sCName);
	__xlBindingTrim(sNormCName);
	for ( p = sNormCName; *p != '\0'; ++p ) {
		if ( (*p >= 'A') && (*p <= 'Z') ) {
			*p = (char)(*p - 'A' + 'a');
		}
	}
	for ( i = 0; i < pCtx->iTypeCount; ++i ) {
		if ( strcmp(pCtx->arrTypes[i].sCName, sNormCName) == 0 ) {
			snprintf(pCtx->arrTypes[i].sXlName, sizeof(pCtx->arrTypes[i].sXlName), "%s", sXlName);
			pCtx->arrTypes[i].bStruct = bStruct;
			return XL_RC_OK;
		}
	}
	if ( pCtx->iTypeCount >= XL_BINDING_TYPE_MAX ) {
		return XL_RC_OK;
	}
	snprintf(pCtx->arrTypes[pCtx->iTypeCount].sCName, sizeof(pCtx->arrTypes[pCtx->iTypeCount].sCName), "%s", sNormCName);
	snprintf(pCtx->arrTypes[pCtx->iTypeCount].sXlName, sizeof(pCtx->arrTypes[pCtx->iTypeCount].sXlName), "%s", sXlName);
	pCtx->arrTypes[pCtx->iTypeCount].bStruct = bStruct;
	pCtx->iTypeCount += 1;
	return XL_RC_OK;
}

static const char* __xlBindingFindType(const XL_BindingContext* pCtx, const char* sCName)
{
	size_t i;

	if ( (pCtx == NULL) || (sCName == NULL) || (sCName[0] == '\0') ) {
		return NULL;
	}
	for ( i = 0; i < pCtx->iTypeCount; ++i ) {
		if ( strcmp(pCtx->arrTypes[i].sCName, sCName) == 0 ) {
			return pCtx->arrTypes[i].sXlName;
		}
	}
	return NULL;
}

static void __xlBindingNormalizeTypeKey(char* sText)
{
	char* p;

	if ( sText == NULL ) {
		return;
	}
	__xlBindingTrim(sText);
	for ( p = sText; *p != '\0'; ++p ) {
		if ( (*p >= 'A') && (*p <= 'Z') ) {
			*p = (char)(*p - 'A' + 'a');
		}
	}
}

static void __xlBindingAddOpaqueAlias(XL_BindingContext* pCtx, const char* sKind, const char* sTag, const char* sAlias)
{
	char sNormKind[16];
	char sNormTag[96];
	char sNormAlias[96];
	size_t i;

	if ( (pCtx == NULL) || (sKind == NULL) || (sTag == NULL) || (sAlias == NULL) ||
		(sKind[0] == '\0') || (sTag[0] == '\0') || (sAlias[0] == '\0') ) {
		return;
	}
	snprintf(sNormKind, sizeof(sNormKind), "%s", sKind);
	snprintf(sNormTag, sizeof(sNormTag), "%s", sTag);
	snprintf(sNormAlias, sizeof(sNormAlias), "%s", sAlias);
	__xlBindingNormalizeTypeKey(sNormKind);
	__xlBindingNormalizeTypeKey(sNormTag);
	__xlBindingNormalizeTypeKey(sNormAlias);
	for ( i = 0; i < pCtx->iOpaqueAliasCount; ++i ) {
		if ( (strcmp(pCtx->arrOpaqueAliases[i].sKind, sNormKind) == 0) &&
			(strcmp(pCtx->arrOpaqueAliases[i].sTag, sNormTag) == 0) &&
			(strcmp(pCtx->arrOpaqueAliases[i].sAlias, sNormAlias) == 0) ) {
			return;
		}
	}
	if ( pCtx->iOpaqueAliasCount >= XL_BINDING_OPAQUE_ALIAS_MAX ) {
		return;
	}
	snprintf(pCtx->arrOpaqueAliases[pCtx->iOpaqueAliasCount].sKind, sizeof(pCtx->arrOpaqueAliases[pCtx->iOpaqueAliasCount].sKind), "%s", sNormKind);
	snprintf(pCtx->arrOpaqueAliases[pCtx->iOpaqueAliasCount].sTag, sizeof(pCtx->arrOpaqueAliases[pCtx->iOpaqueAliasCount].sTag), "%s", sNormTag);
	snprintf(pCtx->arrOpaqueAliases[pCtx->iOpaqueAliasCount].sAlias, sizeof(pCtx->arrOpaqueAliases[pCtx->iOpaqueAliasCount].sAlias), "%s", sAlias);
	__xlBindingTrim(pCtx->arrOpaqueAliases[pCtx->iOpaqueAliasCount].sAlias);
	pCtx->iOpaqueAliasCount += 1;
}

static void __xlBindingResolveOpaqueAliases(XL_BindingContext* pCtx, XL_ModuleTextBuffer* pOut, const char* sKind, const char* sTag, const char* sXlName)
{
	char sNormKind[16];
	char sNormTag[96];
	size_t i;

	if ( (pCtx == NULL) || (sKind == NULL) || (sTag == NULL) || (sXlName == NULL) ||
		(sKind[0] == '\0') || (sTag[0] == '\0') || (sXlName[0] == '\0') ) {
		return;
	}
	snprintf(sNormKind, sizeof(sNormKind), "%s", sKind);
	snprintf(sNormTag, sizeof(sNormTag), "%s", sTag);
	__xlBindingNormalizeTypeKey(sNormKind);
	__xlBindingNormalizeTypeKey(sNormTag);
	for ( i = 0; i < pCtx->iOpaqueAliasCount; ++i ) {
		if ( (strcmp(pCtx->arrOpaqueAliases[i].sKind, sNormKind) == 0) &&
			(strcmp(pCtx->arrOpaqueAliases[i].sTag, sNormTag) == 0) ) {
			(void)__xlBindingAddType(pCtx, pCtx->arrOpaqueAliases[i].sAlias, sXlName, 1);
			if ( (pOut != NULL) && (strcmp(pCtx->arrOpaqueAliases[i].sAlias, sXlName) != 0) ) {
				__xlModuleBufAppendFormat(pOut, "typedef %s = %s\n", pCtx->arrOpaqueAliases[i].sAlias, sXlName);
			}
			__xlBindingTrace(pCtx, "opaque_%s_alias_resolved alias='%s' type='%s'", sNormKind, pCtx->arrOpaqueAliases[i].sAlias, sXlName);
		}
	}
}

static int __xlBindingMapCType(const XL_BindingContext* pCtx, const char* sCType, char* sOut, size_t iOutSize)
{
	char sType[160];
	char* p;
	const char* sMapped;
	size_t iPointerDepth;

	if ( (sCType == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return 0;
	}
	snprintf(sType, sizeof(sType), "%s", sCType);
	__xlBindingRemoveStorageWords(sType);
	__xlBindingCollapseSpaces(sType);
	__xlBindingTrim(sType);
	while ( strncmp(sType, "const ", 6) == 0 ) {
		memmove(sType, sType + 6, strlen(sType + 6) + 1);
		__xlBindingTrim(sType);
	}
	while ( strncmp(sType, "volatile ", 9) == 0 ) {
		memmove(sType, sType + 9, strlen(sType + 9) + 1);
		__xlBindingTrim(sType);
	}
	for ( p = sType; *p != '\0'; ++p ) {
		if ( (*p >= 'A') && (*p <= 'Z') ) {
			*p = (char)(*p - 'A' + 'a');
		}
	}
	__xlBindingRemoveTypeQualifiers(sType);
	iPointerDepth = 0;
	for ( p = sType; *p != '\0'; ++p ) {
		if ( *p == '*' ) {
			iPointerDepth += 1;
		}
	}
	if ( iPointerDepth != 0 ) {
		if ( (iPointerDepth == 1) && (strstr(sType, "char") != NULL) ) {
			snprintf(sOut, iOutSize, "string");
			return 1;
		}
		snprintf(sOut, iOutSize, "point");
		return 1;
	}
	sMapped = __xlBindingFindType(pCtx, sType);
	if ( sMapped != NULL ) {
		snprintf(sOut, iOutSize, "%s", sMapped);
		return 1;
	}
	if ( (strcmp(sType, "void") == 0) ) snprintf(sOut, iOutSize, "void");
	else if ( (strcmp(sType, "bool") == 0) || (strcmp(sType, "_bool") == 0) ) snprintf(sOut, iOutSize, "bool");
	else if ( (strcmp(sType, "bool32") == 0) || (strcmp(sType, "boolean") == 0) ) snprintf(sOut, iOutSize, "bool");
	else if ( (strcmp(sType, "bool8") == 0) || (strcmp(sType, "byte") == 0) ) snprintf(sOut, iOutSize, "uint8");
	else if ( (strcmp(sType, "char") == 0) || (strcmp(sType, "signed char") == 0) || (strcmp(sType, "int8_t") == 0) ) snprintf(sOut, iOutSize, "int8");
	else if ( (strcmp(sType, "unsigned char") == 0) || (strcmp(sType, "uint8_t") == 0) ) snprintf(sOut, iOutSize, "uint8");
	else if ( (strcmp(sType, "short") == 0) || (strcmp(sType, "short int") == 0) || (strcmp(sType, "int16_t") == 0) ) snprintf(sOut, iOutSize, "int16");
	else if ( (strcmp(sType, "unsigned short") == 0) || (strcmp(sType, "unsigned short int") == 0) || (strcmp(sType, "uint16_t") == 0) || (strcmp(sType, "word") == 0) ) snprintf(sOut, iOutSize, "uint16");
	else if ( (strcmp(sType, "int") == 0) || (strcmp(sType, "signed int") == 0) || (strcmp(sType, "int32_t") == 0) || (strcmp(sType, "long32") == 0) || (strcmp(sType, "bool") == 0) ) snprintf(sOut, iOutSize, "int");
	else if ( (strcmp(sType, "unsigned int") == 0) || (strcmp(sType, "uint32_t") == 0) || (strcmp(sType, "uint") == 0) || (strcmp(sType, "dword") == 0) || (strcmp(sType, "ulong32") == 0) ) snprintf(sOut, iOutSize, "uint32");
	else if ( (strcmp(sType, "long") == 0) || (strcmp(sType, "long int") == 0) ) snprintf(sOut, iOutSize, 
#if defined(_WIN32) || defined(_WIN64)
		"int32"
#else
		"int64"
#endif
	);
	else if ( (strcmp(sType, "unsigned long") == 0) || (strcmp(sType, "unsigned long int") == 0) ) snprintf(sOut, iOutSize,
#if defined(_WIN32) || defined(_WIN64)
		"uint32"
#else
		"uint64"
#endif
	);
	else if ( (strcmp(sType, "long long") == 0) || (strcmp(sType, "long long int") == 0) || (strcmp(sType, "int64_t") == 0) ||
		(strcmp(sType, "intptr_t") == 0) || (strcmp(sType, "ssize_t") == 0) || (strcmp(sType, "ptrdiff_t") == 0) ||
		(strcmp(sType, "int_ptr") == 0) || (strcmp(sType, "long_ptr") == 0) || (strcmp(sType, "lresult") == 0) || (strcmp(sType, "ssize_t") == 0) ) snprintf(sOut, iOutSize, "int64");
	else if ( (strcmp(sType, "unsigned long long") == 0) || (strcmp(sType, "unsigned long long int") == 0) || (strcmp(sType, "uint64_t") == 0) || (strcmp(sType, "size_t") == 0) ||
		(strcmp(sType, "uintptr_t") == 0) || (strcmp(sType, "uintptr") == 0) || (strcmp(sType, "uint_ptr") == 0) ||
		(strcmp(sType, "ulong_ptr") == 0) || (strcmp(sType, "dword_ptr") == 0) || (strcmp(sType, "size_t") == 0) ) snprintf(sOut, iOutSize, "uint64");
	else if ( strcmp(sType, "float") == 0 ) snprintf(sOut, iOutSize, "float32");
	else if ( strcmp(sType, "double") == 0 ) snprintf(sOut, iOutSize, "float64");
	else if ( (strcmp(sType, "handle") == 0) || (strcmp(sType, "hwnd") == 0) || (strcmp(sType, "hmodule") == 0) || (strcmp(sType, "hinstance") == 0) ||
		(strcmp(sType, "pvoid") == 0) || (strcmp(sType, "lpvoid") == 0) || (strcmp(sType, "pcvoid") == 0) || (strcmp(sType, "lpcvoid") == 0) ) snprintf(sOut, iOutSize, "point");
	else if ( (strcmp(sType, "pcstr") == 0) || (strcmp(sType, "pstr") == 0) || (strcmp(sType, "lpstr") == 0) || (strcmp(sType, "lpcstr") == 0) ) snprintf(sOut, iOutSize, "string");
	else return 0;
	return 1;
}

static int __xlBindingSizeOfXlType(const char* sXlType, long long* pOut)
{
	if ( (sXlType == NULL) || (pOut == NULL) ) {
		return 0;
	}
	if ( (strcmp(sXlType, "int8") == 0) || (strcmp(sXlType, "uint8") == 0) ) {
		*pOut = 1;
		return 1;
	}
	if ( (strcmp(sXlType, "int16") == 0) || (strcmp(sXlType, "uint16") == 0) ) {
		*pOut = 2;
		return 1;
	}
	if ( (strcmp(sXlType, "int") == 0) || (strcmp(sXlType, "int32") == 0) ||
		(strcmp(sXlType, "uint32") == 0) || (strcmp(sXlType, "float32") == 0) ) {
		*pOut = 4;
		return 1;
	}
	if ( (strcmp(sXlType, "int64") == 0) || (strcmp(sXlType, "uint64") == 0) ||
		(strcmp(sXlType, "float64") == 0) ) {
		*pOut = 8;
		return 1;
	}
	if ( (strcmp(sXlType, "point") == 0) || (strcmp(sXlType, "string") == 0) ||
		(strncmp(sXlType, "function<", 9) == 0) ) {
		*pOut = (long long)sizeof(void*);
		return 1;
	}
	return 0;
}

static int __xlBindingSizeOfCType(const XL_BindingContext* pCtx, const char* sCType, long long* pOut)
{
	char sType[160];
	char sXlType[96];
	char* p;

	if ( (sCType == NULL) || (pOut == NULL) ) {
		return 0;
	}
	snprintf(sType, sizeof(sType), "%s", sCType);
	__xlBindingRemoveStorageWords(sType);
	__xlBindingCollapseSpaces(sType);
	__xlBindingTrim(sType);
	for ( p = sType; *p != '\0'; ++p ) {
		if ( (*p >= 'A') && (*p <= 'Z') ) {
			*p = (char)(*p - 'A' + 'a');
		}
	}
	__xlBindingRemoveTypeQualifiers(sType);
	__xlBindingTrim(sType);
	if ( strchr(sType, '*') != NULL ) {
		*pOut = (long long)sizeof(void*);
		return 1;
	}
	if ( (strcmp(sType, "char") == 0) || (strcmp(sType, "signed char") == 0) ||
		(strcmp(sType, "unsigned char") == 0) || (strcmp(sType, "int8_t") == 0) ||
		(strcmp(sType, "uint8_t") == 0) || (strcmp(sType, "bool") == 0) ||
		(strcmp(sType, "_bool") == 0) || (strcmp(sType, "bool8") == 0) ||
		(strcmp(sType, "byte") == 0) ) {
		*pOut = 1;
		return 1;
	}
	if ( (strcmp(sType, "short") == 0) || (strcmp(sType, "short int") == 0) ||
		(strcmp(sType, "unsigned short") == 0) || (strcmp(sType, "unsigned short int") == 0) ||
		(strcmp(sType, "int16_t") == 0) || (strcmp(sType, "uint16_t") == 0) ||
		(strcmp(sType, "word") == 0) ) {
		*pOut = 2;
		return 1;
	}
	if ( (strcmp(sType, "int") == 0) || (strcmp(sType, "signed int") == 0) ||
		(strcmp(sType, "unsigned int") == 0) || (strcmp(sType, "int32_t") == 0) ||
		(strcmp(sType, "uint32_t") == 0) || (strcmp(sType, "uint") == 0) ||
		(strcmp(sType, "dword") == 0) || (strcmp(sType, "long32") == 0) ||
		(strcmp(sType, "ulong32") == 0) || (strcmp(sType, "bool32") == 0) ||
		(strcmp(sType, "boolean") == 0) || (strcmp(sType, "float") == 0) ) {
		*pOut = 4;
		return 1;
	}
	if ( (strcmp(sType, "long") == 0) || (strcmp(sType, "long int") == 0) ||
		(strcmp(sType, "unsigned long") == 0) || (strcmp(sType, "unsigned long int") == 0) ) {
		*pOut = (long long)sizeof(long);
		return 1;
	}
	if ( (strcmp(sType, "long long") == 0) || (strcmp(sType, "long long int") == 0) ||
		(strcmp(sType, "unsigned long long") == 0) || (strcmp(sType, "unsigned long long int") == 0) ||
		(strcmp(sType, "int64_t") == 0) || (strcmp(sType, "uint64_t") == 0) ||
		(strcmp(sType, "double") == 0) ) {
		*pOut = 8;
		return 1;
	}
	if ( (strcmp(sType, "size_t") == 0) || (strcmp(sType, "intptr_t") == 0) ||
		(strcmp(sType, "uintptr_t") == 0) || (strcmp(sType, "ptrdiff_t") == 0) ||
		(strcmp(sType, "ssize_t") == 0) || (strcmp(sType, "int_ptr") == 0) ||
		(strcmp(sType, "uint_ptr") == 0) || (strcmp(sType, "long_ptr") == 0) ||
		(strcmp(sType, "ulong_ptr") == 0) || (strcmp(sType, "dword_ptr") == 0) ||
		(strcmp(sType, "handle") == 0) || (strcmp(sType, "hwnd") == 0) ||
		(strcmp(sType, "hmodule") == 0) || (strcmp(sType, "hinstance") == 0) ) {
		*pOut = (long long)sizeof(void*);
		return 1;
	}
	if ( __xlBindingMapCType(pCtx, sType, sXlType, sizeof(sXlType)) ) {
		return __xlBindingSizeOfXlType(sXlType, pOut);
	}
	return 0;
}

static void __xlBindingTrace(XL_BindingContext* pCtx, const char* sFormat, ...)
{
	va_list args;
	char sText[XL_DIAG_TEXT_MAX];

	if ( (pCtx == NULL) || !pCtx->bTrace || (pCtx->pDiag == NULL) || (sFormat == NULL) ) {
		return;
	}
	va_start(args, sFormat);
	vsnprintf(sText, sizeof(sText), sFormat, args);
	va_end(args);
	xlDiagEmit(pCtx->pDiag, XL_DIAG_LEVEL_INFO, "binding", __FILE__, __LINE__, "%s", sText);
}

static void __xlBindingSkip(XL_BindingContext* pCtx, const char* sReason, const char* sDecl)
{
	if ( pCtx != NULL ) {
		pCtx->iSkipCount += 1;
	}
	__xlBindingTrace(pCtx, "skip reason='%s' decl='%s'", (sReason != NULL) ? sReason : "unknown", (sDecl != NULL) ? sDecl : "");
}

static int __xlBindingIsIdentText(const char* sText)
{
	size_t i;

	if ( (sText == NULL) || (sText[0] == '\0') || !__xlIsLinkIdentStart((unsigned char)sText[0]) ) {
		return 0;
	}
	for ( i = 1; sText[i] != '\0'; ++i ) {
		if ( !__xlIsLinkIdentCont((unsigned char)sText[i]) ) {
			return 0;
		}
	}
	return 1;
}

static void __xlBindingStripOuterParens(char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}
	__xlBindingTrim(sText);
	iLen = strlen(sText);
	while ( (iLen >= 2) && (sText[0] == '(') && (sText[iLen - 1] == ')') ) {
		size_t i;
		int iDepth;
		int bWrapsAll;

		iDepth = 0;
		bWrapsAll = 1;
		for ( i = 0; i < iLen; ++i ) {
			if ( sText[i] == '(' ) {
				++iDepth;
			} else if ( sText[i] == ')' ) {
				--iDepth;
				if ( (iDepth == 0) && (i + 1 < iLen) ) {
					bWrapsAll = 0;
					break;
				}
			}
		}
		if ( !bWrapsAll ) {
			break;
		}
		memmove(sText, sText + 1, iLen - 2);
		sText[iLen - 2] = '\0';
		__xlBindingTrim(sText);
		iLen = strlen(sText);
	}
}

static void __xlBindingStripCComments(char* sLine, int* pbInBlockComment)
{
	size_t iRead;
	size_t iWrite;
	int bInString;
	int bInChar;
	int bEscape;

	if ( (sLine == NULL) || (pbInBlockComment == NULL) ) {
		return;
	}
	iRead = 0;
	iWrite = 0;
	bInString = 0;
	bInChar = 0;
	bEscape = 0;
	while ( sLine[iRead] != '\0' ) {
		char c = sLine[iRead];
		char cNext = sLine[iRead + 1];

		if ( *pbInBlockComment ) {
			if ( (c == '*') && (cNext == '/') ) {
				*pbInBlockComment = 0;
				iRead += 2;
				if ( (iWrite != 0) && (sLine[iWrite - 1] != ' ') ) {
					sLine[iWrite++] = ' ';
				}
			} else {
				++iRead;
			}
			continue;
		}
		if ( bInString || bInChar ) {
			sLine[iWrite++] = c;
			if ( bEscape ) {
				bEscape = 0;
			} else if ( c == '\\' ) {
				bEscape = 1;
			} else if ( bInString && (c == '"') ) {
				bInString = 0;
			} else if ( bInChar && (c == '\'') ) {
				bInChar = 0;
			}
			++iRead;
			continue;
		}
		if ( c == '"' ) {
			bInString = 1;
			sLine[iWrite++] = c;
			++iRead;
			continue;
		}
		if ( c == '\'' ) {
			bInChar = 1;
			sLine[iWrite++] = c;
			++iRead;
			continue;
		}
		if ( (c == '/') && (cNext == '/') ) {
			break;
		}
		if ( (c == '/') && (cNext == '*') ) {
			*pbInBlockComment = 1;
			iRead += 2;
			if ( (iWrite != 0) && (sLine[iWrite - 1] != ' ') ) {
				sLine[iWrite++] = ' ';
			}
			continue;
		}
		sLine[iWrite++] = c;
		++iRead;
	}
	sLine[iWrite] = '\0';
}

static int __xlBindingNormalizeNumber(const char* sValue, char* sOut, size_t iOutSize, int* pbFloat, int* pbUnsigned)
{
	char sWork[160];
	char sClean[160];
	char* pEnd;
	size_t iLen;
	size_t iRead;
	size_t iWrite;
	int bFloat;
	int bUnsigned;
	int bHex;
	int bBinary;
	int bOctal;

	if ( (sValue == NULL) || (sOut == NULL) || (iOutSize == 0) || (pbFloat == NULL) || (pbUnsigned == NULL) ) {
		return 0;
	}
	snprintf(sWork, sizeof(sWork), "%s", sValue);
	__xlBindingTrim(sWork);
	__xlBindingStripOuterParens(sWork);
	__xlBindingTrim(sWork);
	iLen = strlen(sWork);
	if ( iLen == 0 ) {
		return 0;
	}

	bFloat = 0;
	bUnsigned = 0;
	while ( iLen != 0 ) {
		char c = sWork[iLen - 1];
		if ( (c == 'u') || (c == 'U') ) {
			bUnsigned = 1;
			sWork[--iLen] = '\0';
		} else if ( (c == 'l') || (c == 'L') ) {
			sWork[--iLen] = '\0';
		} else if ( (c == 'f') || (c == 'F') ) {
			bFloat = 1;
			sWork[--iLen] = '\0';
		} else {
			break;
		}
	}
	__xlBindingTrim(sWork);
	if ( sWork[0] == '\0' ) {
		return 0;
	}
	iRead = 0;
	iWrite = 0;
	while ( sWork[iRead] != '\0' ) {
		if ( sWork[iRead] != '\'' ) {
			if ( (iWrite + 1) >= sizeof(sClean) ) {
				return 0;
			}
			sClean[iWrite++] = sWork[iRead];
		}
		++iRead;
	}
	sClean[iWrite] = '\0';
	snprintf(sWork, sizeof(sWork), "%s", sClean);
	if ( (strchr(sWork, '.') != NULL) || (strchr(sWork, 'e') != NULL) || (strchr(sWork, 'E') != NULL) ) {
		bFloat = 1;
	}

	errno = 0;
	if ( bFloat ) {
		(void)strtod(sWork, &pEnd);
		if ( (errno != 0) || (pEnd == sWork) || (*pEnd != '\0') ) {
			return 0;
		}
		snprintf(sOut, iOutSize, "%s", sWork);
		*pbFloat = 1;
		*pbUnsigned = 0;
		return 1;
	}

	bHex = 0;
	bBinary = 0;
	bOctal = 0;
	if ( (sWork[0] == '0') && ((sWork[1] == 'x') || (sWork[1] == 'X')) ) {
		bHex = 1;
	} else if ( (sWork[0] == '-') && (sWork[1] == '0') && ((sWork[2] == 'x') || (sWork[2] == 'X')) ) {
		bHex = 1;
	} else if ( (sWork[0] == '0') && ((sWork[1] == 'b') || (sWork[1] == 'B')) ) {
		bBinary = 1;
	} else if ( (sWork[0] == '-') && (sWork[1] == '0') && ((sWork[2] == 'b') || (sWork[2] == 'B')) ) {
		bBinary = 1;
	} else if ( (sWork[0] == '0') && (sWork[1] >= '0') && (sWork[1] <= '9') ) {
		bOctal = 1;
	} else if ( (sWork[0] == '-') && (sWork[1] == '0') && (sWork[2] >= '0') && (sWork[2] <= '9') ) {
		bOctal = 1;
	}
	if ( bBinary ) {
		const char* pScan;
		unsigned long long uValue;
		int bNegative;

		bNegative = (sWork[0] == '-');
		pScan = sWork + (bNegative ? 3 : 2);
		if ( *pScan == '\0' ) {
			return 0;
		}
		uValue = 0;
		while ( *pScan != '\0' ) {
			if ( (*pScan != '0') && (*pScan != '1') ) {
				return 0;
			}
			uValue = (uValue << 1) | (unsigned long long)(*pScan - '0');
			++pScan;
		}
		if ( bNegative && !bUnsigned ) {
			long long iValue = -(long long)uValue;
			snprintf(sOut, iOutSize, "%lld", iValue);
			*pbFloat = 0;
			*pbUnsigned = 0;
			return 1;
		}
		snprintf(sOut, iOutSize, "%llu", uValue);
		*pbFloat = 0;
		*pbUnsigned = bUnsigned || (uValue > 2147483647ULL);
		return 1;
	}

	if ( (sWork[0] == '-') && !bUnsigned ) {
		long long iValue;

		iValue = strtoll(sWork, &pEnd, (bHex || bOctal) ? 0 : 10);
		if ( (errno != 0) || (pEnd == sWork) || (*pEnd != '\0') ) {
			return 0;
		}
		snprintf(sOut, iOutSize, "%lld", iValue);
		*pbFloat = 0;
		*pbUnsigned = 0;
		return 1;
	}
	{
		unsigned long long uValue;

		uValue = strtoull(sWork, &pEnd, (bHex || bOctal) ? 0 : 10);
		if ( (errno != 0) || (pEnd == sWork) || (*pEnd != '\0') ) {
			return 0;
		}
		snprintf(sOut, iOutSize, "%llu", uValue);
		*pbFloat = 0;
		*pbUnsigned = bUnsigned || (uValue > 2147483647ULL);
		return 1;
	}
}

static int __xlBindingParseCharLiteral(const char* sValue, int* pOut)
{
	size_t iLen;
	const char* p;
	const char* pEnd;
	size_t iPrefixLen;

	if ( (sValue == NULL) || (pOut == NULL) ) {
		return 0;
	}
	iLen = strlen(sValue);
	iPrefixLen = 0;
	if ( (sValue[0] == 'u') && (sValue[1] == '8') && (sValue[2] == '\'') ) {
		iPrefixLen = 2;
	} else if ( ((sValue[0] == 'L') || (sValue[0] == 'u') || (sValue[0] == 'U')) && (sValue[1] == '\'') ) {
		iPrefixLen = 1;
	}
	if ( (iLen < (3 + iPrefixLen)) || (sValue[iPrefixLen] != '\'') || (sValue[iLen - 1] != '\'') ) {
		return 0;
	}
	p = sValue + iPrefixLen + 1;
	pEnd = sValue + iLen - 1;
	if ( *p == '\\' ) {
		int iValue;

		++p;
		if ( p >= pEnd ) {
			return 0;
		}
		if ( *p == 'x' ) {
			++p;
			if ( p >= pEnd ) {
				return 0;
			}
			iValue = 0;
			while ( p < pEnd ) {
				int iDigit;

				if ( (*p >= '0') && (*p <= '9') ) {
					iDigit = *p - '0';
				} else if ( (*p >= 'a') && (*p <= 'f') ) {
					iDigit = *p - 'a' + 10;
				} else if ( (*p >= 'A') && (*p <= 'F') ) {
					iDigit = *p - 'A' + 10;
				} else {
					return 0;
				}
				iValue = (iValue << 4) | iDigit;
				++p;
			}
			*pOut = iValue & 0xff;
			return 1;
		}
		if ( (*p >= '0') && (*p <= '7') ) {
			size_t iCount;

			iValue = 0;
			iCount = 0;
			while ( (p < pEnd) && (*p >= '0') && (*p <= '7') && (iCount < 3) ) {
				iValue = (iValue << 3) | (*p - '0');
				++p;
				++iCount;
			}
			if ( p != pEnd ) {
				return 0;
			}
			*pOut = iValue & 0xff;
			return 1;
		}
		if ( p[1] != '\'' ) {
			return 0;
		}
		switch ( *p ) {
		case 'a': *pOut = '\a'; return 1;
		case 'b': *pOut = '\b'; return 1;
		case 'f': *pOut = '\f'; return 1;
		case 'n': *pOut = '\n'; return 1;
		case 'r': *pOut = '\r'; return 1;
		case 't': *pOut = '\t'; return 1;
		case 'v': *pOut = '\v'; return 1;
		case '\\': *pOut = '\\'; return 1;
		case '\'': *pOut = '\''; return 1;
		case '"': *pOut = '"'; return 1;
		default: return 0;
		}
	}
	if ( p[1] != '\'' ) {
		return 0;
	}
	*pOut = (unsigned char)*p;
	return 1;
}

static int __xlBindingAddConst(XL_BindingContext* pCtx, const char* sName, long long iValue)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') || !__xlBindingIsIdentText(sName) ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iConstCount; ++i ) {
		if ( strcmp(pCtx->arrConsts[i].sName, sName) == 0 ) {
			pCtx->arrConsts[i].iValue = iValue;
			return 1;
		}
	}
	if ( pCtx->iConstCount >= XL_BINDING_CONST_MAX ) {
		return 0;
	}
	snprintf(pCtx->arrConsts[pCtx->iConstCount].sName, sizeof(pCtx->arrConsts[pCtx->iConstCount].sName), "%s", sName);
	pCtx->arrConsts[pCtx->iConstCount].iValue = iValue;
	pCtx->iConstCount += 1;
	return 1;
}

static int __xlBindingFindConst(const XL_BindingContext* pCtx, const char* sName, long long* pOut)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (pOut == NULL) ) {
		return 0;
	}
	if ( strcmp(sName, "NULL") == 0 ) {
		*pOut = 0;
		return 1;
	}
	if ( strcmp(sName, "TRUE") == 0 ) {
		*pOut = 1;
		return 1;
	}
	if ( strcmp(sName, "FALSE") == 0 ) {
		*pOut = 0;
		return 1;
	}
	for ( i = 0; i < pCtx->iConstCount; ++i ) {
		if ( strcmp(pCtx->arrConsts[i].sName, sName) == 0 ) {
			*pOut = pCtx->arrConsts[i].iValue;
			return 1;
		}
	}
	return 0;
}

static int __xlBindingAddStringConst(XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') || !__xlBindingIsIdentText(sName) ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iStringConstCount; ++i ) {
		if ( strcmp(pCtx->arrStringConsts[i], sName) == 0 ) {
			return 1;
		}
	}
	if ( pCtx->iStringConstCount >= XL_BINDING_CONST_MAX ) {
		return 0;
	}
	snprintf(pCtx->arrStringConsts[pCtx->iStringConstCount], sizeof(pCtx->arrStringConsts[pCtx->iStringConstCount]), "%s", sName);
	pCtx->iStringConstCount += 1;
	return 1;
}

static int __xlBindingFindStringConst(const XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iStringConstCount; ++i ) {
		if ( strcmp(pCtx->arrStringConsts[i], sName) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static int __xlBindingAddFloatConst(XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') || !__xlBindingIsIdentText(sName) ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iFloatConstCount; ++i ) {
		if ( strcmp(pCtx->arrFloatConsts[i], sName) == 0 ) {
			return 1;
		}
	}
	if ( pCtx->iFloatConstCount >= XL_BINDING_CONST_MAX ) {
		return 0;
	}
	snprintf(pCtx->arrFloatConsts[pCtx->iFloatConstCount], sizeof(pCtx->arrFloatConsts[pCtx->iFloatConstCount]), "%s", sName);
	pCtx->iFloatConstCount += 1;
	return 1;
}

static int __xlBindingFindFloatConst(const XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iFloatConstCount; ++i ) {
		if ( strcmp(pCtx->arrFloatConsts[i], sName) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static int __xlBindingCharLiteralStarts(const char* p)
{
	if ( p == NULL ) {
		return 0;
	}
	if ( p[0] == '\'' ) {
		return 1;
	}
	if ( (p[0] == 'u') && (p[1] == '8') && (p[2] == '\'') ) {
		return 1;
	}
	if ( ((p[0] == 'L') || (p[0] == 'u') || (p[0] == 'U')) && (p[1] == '\'') ) {
		return 1;
	}
	return 0;
}

static int __xlBindingScanCharLiteral(const char* p, size_t* piLen)
{
	const char* pStart;
	int bEscape;

	if ( (p == NULL) || (piLen == NULL) || !__xlBindingCharLiteralStarts(p) ) {
		return 0;
	}
	pStart = p;
	if ( p[0] == '\'' ) {
		++p;
	} else if ( (p[0] == 'u') && (p[1] == '8') && (p[2] == '\'') ) {
		p += 3;
	} else {
		p += 2;
	}
	bEscape = 0;
	while ( *p != '\0' ) {
		if ( bEscape ) {
			bEscape = 0;
			++p;
			continue;
		}
		if ( *p == '\\' ) {
			bEscape = 1;
			++p;
			continue;
		}
		if ( *p == '\'' ) {
			*piLen = (size_t)(p - pStart + 1);
			return 1;
		}
		++p;
	}
	return 0;
}

static int __xlBindingAddMacro(XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') || !__xlBindingIsIdentText(sName) ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iMacroCount; ++i ) {
		if ( strcmp(pCtx->arrMacros[i], sName) == 0 ) {
			return 1;
		}
	}
	if ( pCtx->iMacroCount >= XL_BINDING_MACRO_MAX ) {
		return 0;
	}
	snprintf(pCtx->arrMacros[pCtx->iMacroCount], sizeof(pCtx->arrMacros[pCtx->iMacroCount]), "%s", sName);
	pCtx->iMacroCount += 1;
	return 1;
}

static int __xlBindingRemoveMacro(XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iMacroCount; ++i ) {
		if ( strcmp(pCtx->arrMacros[i], sName) == 0 ) {
			size_t j;

			for ( j = i + 1; j < pCtx->iMacroCount; ++j ) {
				snprintf(pCtx->arrMacros[j - 1], sizeof(pCtx->arrMacros[j - 1]), "%s", pCtx->arrMacros[j]);
			}
			pCtx->iMacroCount -= 1;
			if ( pCtx->iMacroCount < XL_BINDING_MACRO_MAX ) {
				pCtx->arrMacros[pCtx->iMacroCount][0] = '\0';
			}
			return 1;
		}
	}
	return 0;
}

static int __xlBindingFindMacro(const XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}
	if ( (strcmp(sName, "NULL") == 0) || (strcmp(sName, "TRUE") == 0) || (strcmp(sName, "FALSE") == 0) ) {
		return 1;
	}
	for ( i = 0; i < pCtx->iMacroCount; ++i ) {
		if ( strcmp(pCtx->arrMacros[i], sName) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static int __xlBindingAddDeclMacro(XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') || !__xlBindingIsIdentText(sName) ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iDeclMacroCount; ++i ) {
		if ( strcmp(pCtx->arrDeclMacros[i], sName) == 0 ) {
			return 1;
		}
	}
	if ( pCtx->iDeclMacroCount >= XL_BINDING_DECL_MACRO_MAX ) {
		return 0;
	}
	snprintf(pCtx->arrDeclMacros[pCtx->iDeclMacroCount], sizeof(pCtx->arrDeclMacros[pCtx->iDeclMacroCount]), "%s", sName);
	pCtx->iDeclMacroCount += 1;
	return 1;
}

static void __xlBindingRemoveDeclMacro(XL_BindingContext* pCtx, const char* sName)
{
	size_t i;
	size_t j;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return;
	}
	for ( i = 0; i < pCtx->iDeclMacroCount; ++i ) {
		if ( strcmp(pCtx->arrDeclMacros[i], sName) == 0 ) {
			for ( j = i + 1; j < pCtx->iDeclMacroCount; ++j ) {
				snprintf(pCtx->arrDeclMacros[j - 1], sizeof(pCtx->arrDeclMacros[j - 1]), "%s", pCtx->arrDeclMacros[j]);
			}
			pCtx->iDeclMacroCount -= 1;
			if ( pCtx->iDeclMacroCount < XL_BINDING_DECL_MACRO_MAX ) {
				pCtx->arrDeclMacros[pCtx->iDeclMacroCount][0] = '\0';
			}
			return;
		}
	}
}

static int __xlBindingFindDeclMacro(const XL_BindingContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}
	for ( i = 0; i < pCtx->iDeclMacroCount; ++i ) {
		if ( strcmp(pCtx->arrDeclMacros[i], sName) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static int __xlBindingDefineLooksDeclMacro(const char* sValue)
{
	char sWork[512];

	if ( sValue == NULL ) {
		return 0;
	}
	snprintf(sWork, sizeof(sWork), "%s", sValue);
	__xlBindingTrim(sWork);
	if ( sWork[0] == '\0' ) {
		return 1;
	}
	return (strstr(sWork, "__declspec") != NULL) ||
		(strstr(sWork, "__attribute__") != NULL) ||
		(strcmp(sWork, "extern") == 0) ||
		(strcmp(sWork, "extern \"C\"") == 0) ||
		(strcmp(sWork, "static") == 0) ||
		(strcmp(sWork, "inline") == 0) ||
		(strcmp(sWork, "__inline") == 0) ||
		(strcmp(sWork, "__inline__") == 0) ||
		(strcmp(sWork, "__cdecl") == 0) ||
		(strcmp(sWork, "__stdcall") == 0) ||
		(strcmp(sWork, "__fastcall") == 0) ||
		(strcmp(sWork, "__vectorcall") == 0) ||
		(strcmp(sWork, "WINAPI") == 0) ||
		(strcmp(sWork, "CALLBACK") == 0) ||
		(strcmp(sWork, "APIENTRY") == 0) ||
		(strcmp(sWork, "PASCAL") == 0) ||
		(strcmp(sWork, "NTAPI") == 0) ||
		(strcmp(sWork, "DECLSPEC_IMPORT") == 0) ||
		(strcmp(sWork, "DECLSPEC_EXPORT") == 0);
}

static void __xlBindingRemoveDeclMacros(XL_BindingContext* pCtx, char* sText)
{
	size_t i;

	if ( (pCtx == NULL) || (sText == NULL) || (sText[0] == '\0') ) {
		return;
	}
	for ( i = 0; i < pCtx->iDeclMacroCount; ++i ) {
		const char* sName = pCtx->arrDeclMacros[i];
		size_t iLen = strlen(sName);
		char* pHit = sText;

		if ( iLen == 0 ) {
			continue;
		}
		while ( (pHit = strstr(pHit, sName)) != NULL ) {
			unsigned char cBefore = (pHit == sText) ? 0 : (unsigned char)pHit[-1];
			unsigned char cAfter = (unsigned char)pHit[iLen];
			if ( ((pHit == sText) || !__xlIsLinkIdentCont(cBefore)) && !__xlIsLinkIdentCont(cAfter) ) {
				memmove(pHit, pHit + iLen, strlen(pHit + iLen) + 1);
			} else {
				pHit += iLen;
			}
		}
	}
	__xlBindingCollapseSpaces(sText);
	__xlBindingTrim(sText);
}

typedef struct XL_BindingConstExprParser {
	const char* p;
	const XL_BindingContext* pCtx;
	int bOk;
	int bUnknownIdentZero;
} XL_BindingConstExprParser;

static void __xlBindingConstExprSkipWs(XL_BindingConstExprParser* pParser)
{
	while ( (*pParser->p == ' ') || (*pParser->p == '\t') || (*pParser->p == '\r') || (*pParser->p == '\n') ) {
		++pParser->p;
	}
}

static long long __xlBindingConstExprLogicalOr(XL_BindingConstExprParser* pParser);

static int __xlBindingConstExprTryCast(XL_BindingConstExprParser* pParser)
{
	const char* pSave;
	const char* pStart;
	char sType[160];
	char sXlType[96];
	size_t iLen;

	__xlBindingConstExprSkipWs(pParser);
	if ( *pParser->p != '(' ) {
		return 0;
	}
	pSave = pParser->p;
	pStart = pParser->p + 1;
	while ( (*pParser->p != '\0') && (*pParser->p != ')') ) {
		++pParser->p;
	}
	if ( *pParser->p != ')' ) {
		pParser->p = pSave;
		return 0;
	}
	iLen = (size_t)(pParser->p - pStart);
	if ( (iLen == 0) || (iLen >= sizeof(sType)) ) {
		pParser->p = pSave;
		return 0;
	}
	memcpy(sType, pStart, iLen);
	sType[iLen] = '\0';
	__xlBindingTrim(sType);
	if ( !__xlBindingMapCType(pParser->pCtx, sType, sXlType, sizeof(sXlType)) || (strcmp(sXlType, "void") == 0) ) {
		pParser->p = pSave;
		return 0;
	}
	++pParser->p;
	return 1;
}

static long long __xlBindingConstExprPrimary(XL_BindingConstExprParser* pParser)
{
	char sText[160];
	size_t iLen;
	long long iValue;

	__xlBindingConstExprSkipWs(pParser);
	if ( *pParser->p == '(' ) {
		++pParser->p;
		iValue = __xlBindingConstExprLogicalOr(pParser);
		__xlBindingConstExprSkipWs(pParser);
		if ( *pParser->p != ')' ) {
			pParser->bOk = 0;
			return 0;
		}
		++pParser->p;
		return iValue;
	}
	if ( (strncmp(pParser->p, "sizeof", 6) == 0) && !__xlIsLinkIdentCont((unsigned char)pParser->p[6]) ) {
		const char* pStart;
		int iDepth;

		pParser->p += 6;
		__xlBindingConstExprSkipWs(pParser);
		if ( *pParser->p != '(' ) {
			pParser->bOk = 0;
			return 0;
		}
		++pParser->p;
		pStart = pParser->p;
		iDepth = 1;
		while ( (*pParser->p != '\0') && (iDepth > 0) ) {
			if ( *pParser->p == '(' ) {
				++iDepth;
			} else if ( *pParser->p == ')' ) {
				--iDepth;
				if ( iDepth == 0 ) {
					break;
				}
			}
			++pParser->p;
		}
		if ( *pParser->p != ')' ) {
			pParser->bOk = 0;
			return 0;
		}
		iLen = (size_t)(pParser->p - pStart);
		if ( (iLen == 0) || (iLen >= sizeof(sText)) ) {
			pParser->bOk = 0;
			return 0;
		}
		memcpy(sText, pStart, iLen);
		sText[iLen] = '\0';
		__xlBindingTrim(sText);
		if ( !__xlBindingSizeOfCType(pParser->pCtx, sText, &iValue) ) {
			pParser->bOk = 0;
			return 0;
		}
		++pParser->p;
		return iValue;
	}
	if ( __xlBindingCharLiteralStarts(pParser->p) ) {
		const char* pStart = pParser->p;
		int iChar;

		if ( !__xlBindingScanCharLiteral(pParser->p, &iLen) || (iLen >= sizeof(sText)) ) {
			pParser->bOk = 0;
			return 0;
		}
		pParser->p += iLen;
		memcpy(sText, pStart, iLen);
		sText[iLen] = '\0';
		if ( !__xlBindingParseCharLiteral(sText, &iChar) ) {
			pParser->bOk = 0;
			return 0;
		}
		return (long long)iChar;
	}
	if ( ((*pParser->p >= '0') && (*pParser->p <= '9')) ) {
		const char* pStart = pParser->p;
		char sNumber[160];
		int bFloat;
		int bUnsigned;

		while ( ((*pParser->p >= '0') && (*pParser->p <= '9')) ||
			((*pParser->p >= 'a') && (*pParser->p <= 'z')) ||
			((*pParser->p >= 'A') && (*pParser->p <= 'Z')) ||
			(*pParser->p == '_') || (*pParser->p == '.') || (*pParser->p == '\'') ) {
			++pParser->p;
		}
		iLen = (size_t)(pParser->p - pStart);
		if ( (iLen == 0) || (iLen >= sizeof(sText)) ) {
			pParser->bOk = 0;
			return 0;
		}
		memcpy(sText, pStart, iLen);
		sText[iLen] = '\0';
		if ( !__xlBindingNormalizeNumber(sText, sNumber, sizeof(sNumber), &bFloat, &bUnsigned) || bFloat ) {
			pParser->bOk = 0;
			return 0;
		}
		errno = 0;
		iValue = strtoll(sNumber, NULL, 10);
		if ( errno != 0 ) {
			pParser->bOk = 0;
			return 0;
		}
		return iValue;
	}
	if ( ((*pParser->p >= 'A') && (*pParser->p <= 'Z')) ||
		((*pParser->p >= 'a') && (*pParser->p <= 'z')) ||
		(*pParser->p == '_') ) {
		const char* pStart = pParser->p;

		while ( __xlIsLinkIdentCont((unsigned char)*pParser->p) ) {
			++pParser->p;
		}
		iLen = (size_t)(pParser->p - pStart);
		if ( (iLen == 0) || (iLen >= sizeof(sText)) ) {
			pParser->bOk = 0;
			return 0;
		}
		memcpy(sText, pStart, iLen);
		sText[iLen] = '\0';
		if ( !__xlBindingFindConst(pParser->pCtx, sText, &iValue) ) {
			if ( pParser->bUnknownIdentZero ) {
				return 0;
			}
			pParser->bOk = 0;
			return 0;
		}
		return iValue;
	}
	pParser->bOk = 0;
	return 0;
}

static long long __xlBindingConstExprUnary(XL_BindingConstExprParser* pParser)
{
	__xlBindingConstExprSkipWs(pParser);
	if ( __xlBindingConstExprTryCast(pParser) ) {
		return __xlBindingConstExprUnary(pParser);
	}
	if ( *pParser->p == '+' ) {
		++pParser->p;
		return __xlBindingConstExprUnary(pParser);
	}
	if ( *pParser->p == '-' ) {
		++pParser->p;
		return -__xlBindingConstExprUnary(pParser);
	}
	if ( *pParser->p == '~' ) {
		++pParser->p;
		return ~__xlBindingConstExprUnary(pParser);
	}
	if ( *pParser->p == '!' ) {
		++pParser->p;
		return __xlBindingConstExprUnary(pParser) ? 0 : 1;
	}
	return __xlBindingConstExprPrimary(pParser);
}

static long long __xlBindingConstExprMul(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprUnary(pParser);

	while ( pParser->bOk ) {
		long long iRight;
		__xlBindingConstExprSkipWs(pParser);
		if ( (*pParser->p != '*') && (*pParser->p != '/') && (*pParser->p != '%') ) {
			break;
		}
		{
			char cOp = *pParser->p++;
			iRight = __xlBindingConstExprUnary(pParser);
			if ( !pParser->bOk ) {
				return 0;
			}
			if ( cOp == '*' ) {
				iValue *= iRight;
			} else {
				if ( iRight == 0 ) {
					pParser->bOk = 0;
					return 0;
				}
				iValue = (cOp == '/') ? (iValue / iRight) : (iValue % iRight);
			}
		}
	}
	return iValue;
}

static long long __xlBindingConstExprAdd(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprMul(pParser);

	while ( pParser->bOk ) {
		long long iRight;
		__xlBindingConstExprSkipWs(pParser);
		if ( (*pParser->p != '+') && (*pParser->p != '-') ) {
			break;
		}
		{
			char cOp = *pParser->p++;
			iRight = __xlBindingConstExprMul(pParser);
			if ( !pParser->bOk ) {
				return 0;
			}
			iValue = (cOp == '+') ? (iValue + iRight) : (iValue - iRight);
		}
	}
	return iValue;
}

static long long __xlBindingConstExprShift(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprAdd(pParser);

	while ( pParser->bOk ) {
		long long iRight;
		__xlBindingConstExprSkipWs(pParser);
		if ( strncmp(pParser->p, "<<", 2) == 0 ) {
			pParser->p += 2;
			iRight = __xlBindingConstExprAdd(pParser);
			if ( (iRight < 0) || (iRight >= 63) ) {
				pParser->bOk = 0;
				return 0;
			}
			iValue <<= iRight;
		} else if ( strncmp(pParser->p, ">>", 2) == 0 ) {
			pParser->p += 2;
			iRight = __xlBindingConstExprAdd(pParser);
			if ( (iRight < 0) || (iRight >= 63) ) {
				pParser->bOk = 0;
				return 0;
			}
			iValue >>= iRight;
		} else {
			break;
		}
	}
	return iValue;
}

static long long __xlBindingConstExprAnd(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprShift(pParser);

	while ( pParser->bOk ) {
		__xlBindingConstExprSkipWs(pParser);
		if ( (*pParser->p != '&') || (pParser->p[1] == '&') ) {
			break;
		}
		++pParser->p;
		iValue &= __xlBindingConstExprShift(pParser);
	}
	return iValue;
}

static long long __xlBindingConstExprXor(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprAnd(pParser);

	while ( pParser->bOk ) {
		__xlBindingConstExprSkipWs(pParser);
		if ( *pParser->p != '^' ) {
			break;
		}
		++pParser->p;
		iValue ^= __xlBindingConstExprAnd(pParser);
	}
	return iValue;
}

static long long __xlBindingConstExprOr(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprXor(pParser);

	while ( pParser->bOk ) {
		__xlBindingConstExprSkipWs(pParser);
		if ( (*pParser->p != '|') || (pParser->p[1] == '|') ) {
			break;
		}
		++pParser->p;
		iValue |= __xlBindingConstExprXor(pParser);
	}
	return iValue;
}

static long long __xlBindingConstExprRel(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprOr(pParser);

	while ( pParser->bOk ) {
		long long iRight;
		__xlBindingConstExprSkipWs(pParser);
		if ( strncmp(pParser->p, "<=", 2) == 0 ) {
			pParser->p += 2;
			iRight = __xlBindingConstExprOr(pParser);
			iValue = (iValue <= iRight) ? 1 : 0;
		} else if ( strncmp(pParser->p, ">=", 2) == 0 ) {
			pParser->p += 2;
			iRight = __xlBindingConstExprOr(pParser);
			iValue = (iValue >= iRight) ? 1 : 0;
		} else if ( (*pParser->p == '<') && (pParser->p[1] != '<') ) {
			++pParser->p;
			iRight = __xlBindingConstExprOr(pParser);
			iValue = (iValue < iRight) ? 1 : 0;
		} else if ( (*pParser->p == '>') && (pParser->p[1] != '>') ) {
			++pParser->p;
			iRight = __xlBindingConstExprOr(pParser);
			iValue = (iValue > iRight) ? 1 : 0;
		} else {
			break;
		}
	}
	return iValue;
}

static long long __xlBindingConstExprEq(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprRel(pParser);

	while ( pParser->bOk ) {
		long long iRight;
		__xlBindingConstExprSkipWs(pParser);
		if ( strncmp(pParser->p, "==", 2) == 0 ) {
			pParser->p += 2;
			iRight = __xlBindingConstExprRel(pParser);
			iValue = (iValue == iRight) ? 1 : 0;
		} else if ( strncmp(pParser->p, "!=", 2) == 0 ) {
			pParser->p += 2;
			iRight = __xlBindingConstExprRel(pParser);
			iValue = (iValue != iRight) ? 1 : 0;
		} else {
			break;
		}
	}
	return iValue;
}

static long long __xlBindingConstExprLogicalAnd(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprEq(pParser);

	while ( pParser->bOk ) {
		long long iRight;
		__xlBindingConstExprSkipWs(pParser);
		if ( strncmp(pParser->p, "&&", 2) != 0 ) {
			break;
		}
		pParser->p += 2;
		iRight = __xlBindingConstExprEq(pParser);
		iValue = (iValue && iRight) ? 1 : 0;
	}
	return iValue;
}

static long long __xlBindingConstExprLogicalOr(XL_BindingConstExprParser* pParser)
{
	long long iValue = __xlBindingConstExprLogicalAnd(pParser);

	while ( pParser->bOk ) {
		long long iRight;
		__xlBindingConstExprSkipWs(pParser);
		if ( strncmp(pParser->p, "||", 2) != 0 ) {
			break;
		}
		pParser->p += 2;
		iRight = __xlBindingConstExprLogicalAnd(pParser);
		iValue = (iValue || iRight) ? 1 : 0;
	}
	return iValue;
}

static long long __xlBindingConstExprConditional(XL_BindingConstExprParser* pParser)
{
	long long iValue;
	long long iTrueValue;
	long long iFalseValue;

	iValue = __xlBindingConstExprLogicalOr(pParser);
	if ( !pParser->bOk ) {
		return 0;
	}
	__xlBindingConstExprSkipWs(pParser);
	if ( *pParser->p != '?' ) {
		return iValue;
	}
	++pParser->p;
	iTrueValue = __xlBindingConstExprConditional(pParser);
	if ( !pParser->bOk ) {
		return 0;
	}
	__xlBindingConstExprSkipWs(pParser);
	if ( *pParser->p != ':' ) {
		pParser->bOk = 0;
		return 0;
	}
	++pParser->p;
	iFalseValue = __xlBindingConstExprConditional(pParser);
	if ( !pParser->bOk ) {
		return 0;
	}
	return (iValue != 0) ? iTrueValue : iFalseValue;
}

static int __xlBindingEvalIntConstExprEx(const XL_BindingContext* pCtx, const char* sValue, long long* pOut, int bUnknownIdentZero)
{
	XL_BindingConstExprParser parser;
	long long iValue;

	if ( (sValue == NULL) || (pOut == NULL) ) {
		return 0;
	}
	parser.p = sValue;
	parser.pCtx = pCtx;
	parser.bOk = 1;
	parser.bUnknownIdentZero = bUnknownIdentZero;
	iValue = __xlBindingConstExprConditional(&parser);
	__xlBindingConstExprSkipWs(&parser);
	if ( !parser.bOk || (*parser.p != '\0') ) {
		return 0;
	}
	*pOut = iValue;
	return 1;
}

static int __xlBindingEvalIntConstExpr(const XL_BindingContext* pCtx, const char* sValue, long long* pOut)
{
	return __xlBindingEvalIntConstExprEx(pCtx, sValue, pOut, 0);
}

static int __xlBindingNormalizePointNullDefine(const XL_BindingContext* pCtx, const char* sValue)
{
	char sWork[512];
	char sType[160];
	char sExpr[256];
	char sXlType[96];
	const char* pStart;
	const char* pClose;
	const char* pExpr;
	size_t iLen;
	long long iValue;

	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return 0;
	}
	snprintf(sWork, sizeof(sWork), "%s", sValue);
	__xlBindingTrim(sWork);
	__xlBindingStripOuterParens(sWork);
	__xlBindingTrim(sWork);
	if ( sWork[0] != '(' ) {
		return 0;
	}
	pStart = sWork + 1;
	pClose = strchr(pStart, ')');
	if ( pClose == NULL ) {
		return 0;
	}
	iLen = (size_t)(pClose - pStart);
	if ( (iLen == 0) || (iLen >= sizeof(sType)) ) {
		return 0;
	}
	memcpy(sType, pStart, iLen);
	sType[iLen] = '\0';
	__xlBindingTrim(sType);
	if ( !__xlBindingMapCType(pCtx, sType, sXlType, sizeof(sXlType)) || (strcmp(sXlType, "point") != 0) ) {
		return 0;
	}
	pExpr = pClose + 1;
	while ( (*pExpr == ' ') || (*pExpr == '\t') ) {
		++pExpr;
	}
	if ( *pExpr == '\0' ) {
		return 0;
	}
	snprintf(sExpr, sizeof(sExpr), "%s", pExpr);
	__xlBindingTrim(sExpr);
	__xlBindingStripOuterParens(sExpr);
	__xlBindingTrim(sExpr);
	if ( strcmp(sExpr, "NULL") == 0 ) {
		return 1;
	}
	if ( !__xlBindingEvalIntConstExpr(pCtx, sExpr, &iValue) ) {
		return 0;
	}
	return iValue == 0;
}

static void __xlBindingStripSimpleCCasts(char* sExpr)
{
	static const char* arrCasts[] = {
		"(int)", "(signed int)", "(unsigned int)",
		"(long)", "(long int)", "(unsigned long)", "(unsigned long int)",
		"(long long)", "(long long int)", "(unsigned long long)", "(unsigned long long int)",
		"(short)", "(unsigned short)", "(char)", "(signed char)", "(unsigned char)",
		"(int8_t)", "(uint8_t)", "(int16_t)", "(uint16_t)", "(int32_t)", "(uint32_t)", "(int64_t)", "(uint64_t)",
		"(size_t)", "(DWORD)", "(UINT)", "(WORD)", "(BYTE)"
	};
	size_t i;

	if ( sExpr == NULL ) {
		return;
	}
	for ( i = 0; i < sizeof(arrCasts) / sizeof(arrCasts[0]); ++i ) {
		char* pHit;
		size_t iLen = strlen(arrCasts[i]);

		while ( (pHit = strstr(sExpr, arrCasts[i])) != NULL ) {
			memmove(pHit, pHit + iLen, strlen(pHit + iLen) + 1);
		}
	}
	__xlBindingTrim(sExpr);
}

static int __xlBindingMacroCastTypeIsIntegral(const char* sXlType)
{
	if ( sXlType == NULL ) {
		return 0;
	}
	return (strcmp(sXlType, "bool") == 0) ||
		(strcmp(sXlType, "int") == 0) ||
		(strcmp(sXlType, "int8") == 0) ||
		(strcmp(sXlType, "uint8") == 0) ||
		(strcmp(sXlType, "int16") == 0) ||
		(strcmp(sXlType, "uint16") == 0) ||
		(strcmp(sXlType, "int32") == 0) ||
		(strcmp(sXlType, "uint32") == 0) ||
		(strcmp(sXlType, "int64") == 0) ||
		(strcmp(sXlType, "uint64") == 0);
}

static void __xlBindingStripMappedIntegralCCasts(const XL_BindingContext* pCtx, char* sExpr)
{
	size_t i;
	int bChanged;

	if ( sExpr == NULL ) {
		return;
	}
	do {
		bChanged = 0;
		for ( i = 0; sExpr[i] != '\0'; ++i ) {
			size_t j;
			char sType[160];
			char sXlType[96];
			size_t iTypeLen;

			if ( sExpr[i] != '(' ) {
				continue;
			}
			j = i + 1;
			while ( (sExpr[j] != '\0') && (sExpr[j] != ')') ) {
				if ( (sExpr[j] == '(') || (sExpr[j] == ',') || (sExpr[j] == '?') || (sExpr[j] == ':') ||
					(sExpr[j] == '+') || (sExpr[j] == '-') || (sExpr[j] == '/') || (sExpr[j] == '%') ||
					(sExpr[j] == '&') || (sExpr[j] == '|') || (sExpr[j] == '^') || (sExpr[j] == '~') ||
					(sExpr[j] == '!') || (sExpr[j] == '=') || (sExpr[j] == '<') || (sExpr[j] == '>') ) {
					break;
				}
				++j;
			}
			if ( sExpr[j] != ')' ) {
				continue;
			}
			iTypeLen = j - (i + 1);
			if ( (iTypeLen == 0) || (iTypeLen >= sizeof(sType)) ) {
				continue;
			}
			memcpy(sType, sExpr + i + 1, iTypeLen);
			sType[iTypeLen] = '\0';
			__xlBindingTrim(sType);
			if ( !__xlBindingMapCType(pCtx, sType, sXlType, sizeof(sXlType)) ||
				!__xlBindingMacroCastTypeIsIntegral(sXlType) ) {
				continue;
			}
			memmove(sExpr + i, sExpr + j + 1, strlen(sExpr + j + 1) + 1);
			bChanged = 1;
			break;
		}
	} while ( bChanged );
	__xlBindingTrim(sExpr);
}

static void __xlBindingReplaceBuiltinConstTokens(char* sExpr)
{
	char sOut[512];
	size_t iRead;
	size_t iWrite;

	if ( sExpr == NULL ) {
		return;
	}
	iRead = 0;
	iWrite = 0;
	while ( sExpr[iRead] != '\0' ) {
		if ( __xlIsLinkIdentStart((unsigned char)sExpr[iRead]) ) {
			char sIdent[96];
			size_t iStart = iRead;
			size_t iLen;
			const char* sValue = NULL;

			while ( __xlIsLinkIdentCont((unsigned char)sExpr[iRead]) ) {
				++iRead;
			}
			iLen = iRead - iStart;
			if ( iLen >= sizeof(sIdent) ) {
				iLen = sizeof(sIdent) - 1;
			}
			memcpy(sIdent, sExpr + iStart, iLen);
			sIdent[iLen] = '\0';
			if ( strcmp(sIdent, "NULL") == 0 ) {
				sValue = "0";
			} else if ( strcmp(sIdent, "TRUE") == 0 ) {
				sValue = "1";
			} else if ( strcmp(sIdent, "FALSE") == 0 ) {
				sValue = "0";
			}
			if ( sValue != NULL ) {
				size_t iValueLen = strlen(sValue);
				if ( (iWrite + iValueLen) < sizeof(sOut) ) {
					memcpy(sOut + iWrite, sValue, iValueLen);
					iWrite += iValueLen;
				}
			} else if ( (iWrite + (iRead - iStart)) < sizeof(sOut) ) {
				memcpy(sOut + iWrite, sExpr + iStart, iRead - iStart);
				iWrite += iRead - iStart;
			}
			continue;
		}
		if ( (iWrite + 1) < sizeof(sOut) ) {
			sOut[iWrite++] = sExpr[iRead];
		}
		++iRead;
	}
	sOut[iWrite] = '\0';
	snprintf(sExpr, 512, "%s", sOut);
}

static int __xlBindingMacroExprFoldSizeof(const XL_BindingContext* pCtx, char* sExpr)
{
	char sOut[512];
	size_t iRead;
	size_t iWrite;
	int bChanged;

	if ( sExpr == NULL ) {
		return 0;
	}
	iRead = 0;
	iWrite = 0;
	bChanged = 0;
	while ( (sExpr[iRead] != '\0') && ((iWrite + 1) < sizeof(sOut)) ) {
		if ( (strncmp(sExpr + iRead, "sizeof", 6) == 0) && !__xlIsLinkIdentCont((unsigned char)sExpr[iRead + 6]) &&
			((iRead == 0) || !__xlIsLinkIdentCont((unsigned char)sExpr[iRead - 1])) ) {
			size_t iPos;
			size_t iTypeStart;
			size_t iTypeLen;
			int iDepth;
			char sType[160];
			char sValue[64];
			long long iSize;
			int iValueLen;

			iPos = iRead + 6;
			while ( (sExpr[iPos] == ' ') || (sExpr[iPos] == '\t') ) {
				++iPos;
			}
			if ( sExpr[iPos] != '(' ) {
				return 0;
			}
			++iPos;
			iTypeStart = iPos;
			iDepth = 1;
			while ( (sExpr[iPos] != '\0') && (iDepth > 0) ) {
				if ( sExpr[iPos] == '(' ) {
					++iDepth;
				} else if ( sExpr[iPos] == ')' ) {
					--iDepth;
					if ( iDepth == 0 ) {
						break;
					}
				}
				++iPos;
			}
			if ( sExpr[iPos] != ')' ) {
				return 0;
			}
			iTypeLen = iPos - iTypeStart;
			if ( (iTypeLen == 0) || (iTypeLen >= sizeof(sType)) ) {
				return 0;
			}
			memcpy(sType, sExpr + iTypeStart, iTypeLen);
			sType[iTypeLen] = '\0';
			__xlBindingTrim(sType);
			if ( !__xlBindingSizeOfCType(pCtx, sType, &iSize) ) {
				return 0;
			}
			iValueLen = snprintf(sValue, sizeof(sValue), "%lld", iSize);
			if ( (iValueLen < 0) || ((iWrite + (size_t)iValueLen) >= sizeof(sOut)) ) {
				return 0;
			}
			memcpy(sOut + iWrite, sValue, (size_t)iValueLen);
			iWrite += (size_t)iValueLen;
			iRead = iPos + 1;
			bChanged = 1;
			continue;
		}
		sOut[iWrite++] = sExpr[iRead++];
	}
	sOut[iWrite] = '\0';
	snprintf(sExpr, 512, "%s", sOut);
	return bChanged;
}

static int __xlBindingMacroExprLooksSimple(const char* sExpr)
{
	const char* p;
	int iDepth;
	int iTernaryDepth;
	int iQuestionCount;
	int iColonCount;

	if ( (sExpr == NULL) || (sExpr[0] == '\0') ) {
		return 0;
	}
	if ( (strstr(sExpr, "##") != NULL) || (strchr(sExpr, '#') != NULL) ||
		(strstr(sExpr, "sizeof") != NULL) || (strstr(sExpr, "do") != NULL) ||
		(strstr(sExpr, "while") != NULL) || (strstr(sExpr, "return") != NULL) ) {
		return 0;
	}
	iDepth = 0;
	iTernaryDepth = -1;
	iQuestionCount = 0;
	iColonCount = 0;
	for ( p = sExpr; *p != '\0'; ++p ) {
		unsigned char ch = (unsigned char)(*p);

		if ( (ch == '"') || (ch == '\'') || (ch == ';') || (ch == '{') || (ch == '}') ||
			(ch == '[') || (ch == ']') ) {
			return 0;
		}
		if ( ch == '(' ) {
			++iDepth;
		} else if ( ch == ')' ) {
			--iDepth;
			if ( iDepth < 0 ) {
				return 0;
			}
		} else if ( ch == '?' ) {
			if ( iQuestionCount != 0 ) {
				return 0;
			}
			iQuestionCount += 1;
			iTernaryDepth = iDepth;
		} else if ( ch == ':' ) {
			if ( (iQuestionCount == 0) || (iColonCount != 0) || (iDepth != iTernaryDepth) ) {
				return 0;
			}
			iColonCount += 1;
		} else if ( !(isalnum(ch) || (ch == '_') || __xlIsAsciiSpace(ch) ||
			(ch == '+') || (ch == '-') || (ch == '*') || (ch == '/') || (ch == '%') ||
			(ch == '<') || (ch == '>') || (ch == '&') || (ch == '|') || (ch == '^') ||
			(ch == '~') || (ch == '!') || (ch == '=') || (ch == '.')) ) {
			return 0;
		}
	}
	if ( iQuestionCount != iColonCount ) {
		return 0;
	}
	return iDepth == 0;
}

static int __xlBindingMacroExprRewriteLogical(char* sExpr)
{
	char sOut[512];
	size_t iRead;
	size_t iWrite;
	int bChanged;

	if ( sExpr == NULL ) {
		return 0;
	}
	iRead = 0;
	iWrite = 0;
	bChanged = 0;
	while ( (sExpr[iRead] != '\0') && ((iWrite + 1) < sizeof(sOut)) ) {
		if ( (sExpr[iRead] == '&') && (sExpr[iRead + 1] == '&') ) {
			const char* sWord = " and ";
			size_t iLen = strlen(sWord);
			if ( (iWrite + iLen) < sizeof(sOut) ) {
				memcpy(sOut + iWrite, sWord, iLen);
				iWrite += iLen;
			}
			iRead += 2;
			bChanged = 1;
			continue;
		}
		if ( (sExpr[iRead] == '|') && (sExpr[iRead + 1] == '|') ) {
			const char* sWord = " or ";
			size_t iLen = strlen(sWord);
			if ( (iWrite + iLen) < sizeof(sOut) ) {
				memcpy(sOut + iWrite, sWord, iLen);
				iWrite += iLen;
			}
			iRead += 2;
			bChanged = 1;
			continue;
		}
		if ( (sExpr[iRead] == '!') && (sExpr[iRead + 1] != '=') ) {
			size_t iOperandStart;
			size_t iOperandEnd;
			size_t iLen;

			++iRead;
			while ( (sExpr[iRead] == ' ') || (sExpr[iRead] == '\t') ) {
				++iRead;
			}
			iOperandStart = iRead;
			if ( sExpr[iRead] == '(' ) {
				int iDepth = 1;
				++iRead;
				while ( (sExpr[iRead] != '\0') && (iDepth > 0) ) {
					if ( sExpr[iRead] == '(' ) {
						++iDepth;
					} else if ( sExpr[iRead] == ')' ) {
						--iDepth;
					}
					++iRead;
				}
				iOperandEnd = iRead;
			} else {
				while ( __xlIsLinkIdentCont((unsigned char)sExpr[iRead]) ||
					((sExpr[iRead] >= '0') && (sExpr[iRead] <= '9')) ) {
					++iRead;
				}
				iOperandEnd = iRead;
			}
			if ( iOperandEnd <= iOperandStart ) {
				sOut[iWrite++] = '!';
				bChanged = 1;
				continue;
			}
			iLen = iOperandEnd - iOperandStart;
			if ( (iWrite + iLen + 7) < sizeof(sOut) ) {
				sOut[iWrite++] = '(';
				memcpy(sOut + iWrite, sExpr + iOperandStart, iLen);
				iWrite += iLen;
				memcpy(sOut + iWrite, " == 0)", 6);
				iWrite += 6;
			}
			bChanged = 1;
			continue;
		}
		sOut[iWrite++] = sExpr[iRead++];
	}
	sOut[iWrite] = '\0';
	snprintf(sExpr, 512, "%s", sOut);
	__xlBindingTrim(sExpr);
	return bChanged;
}

static int __xlBindingMacroExprIsBoolish(const char* sExpr)
{
	if ( sExpr == NULL ) {
		return 0;
	}
	if ( strchr(sExpr, '?') != NULL ) {
		return 0;
	}
	return (strstr(sExpr, "==") != NULL) ||
		(strstr(sExpr, "!=") != NULL) ||
		(strstr(sExpr, "<=") != NULL) ||
		((strchr(sExpr, '<') != NULL) && (strstr(sExpr, "<<") == NULL)) ||
		(strstr(sExpr, ">=") != NULL) ||
		((strchr(sExpr, '>') != NULL) && (strstr(sExpr, ">>") == NULL)) ||
		(strstr(sExpr, " and ") != NULL) ||
		(strstr(sExpr, " or ") != NULL) ||
		(strstr(sExpr, "not ") != NULL);
}

static int __xlBindingMacroExprSegmentIsBoolish(const char* sExpr, size_t iLen)
{
	char sWork[256];

	if ( (sExpr == NULL) || (iLen == 0) ) {
		return 0;
	}
	while ( (iLen != 0) && __xlIsAsciiSpace((unsigned char)*sExpr) ) {
		++sExpr;
		--iLen;
	}
	while ( (iLen != 0) && __xlIsAsciiSpace((unsigned char)sExpr[iLen - 1]) ) {
		--iLen;
	}
	if ( (iLen == 0) || (iLen >= sizeof(sWork)) ) {
		return 0;
	}
	memcpy(sWork, sExpr, iLen);
	sWork[iLen] = '\0';
	__xlBindingTrim(sWork);
	__xlBindingStripOuterParens(sWork);
	__xlBindingTrim(sWork);
	return __xlBindingMacroExprIsBoolish(sWork);
}

static int __xlBindingMacroExprRewriteTernaryIntCondition(char* sExpr)
{
	char sOut[640];
	size_t i;
	size_t iQuestion;
	size_t iColon;
	size_t iCondStart;
	size_t iCondEnd;
	size_t iCondLen;
	int iDepth;

	if ( sExpr == NULL ) {
		return 0;
	}
	iQuestion = (size_t)-1;
	iColon = (size_t)-1;
	iDepth = 0;
	for ( i = 0; sExpr[i] != '\0'; ++i ) {
		if ( sExpr[i] == '(' ) {
			++iDepth;
		} else if ( sExpr[i] == ')' ) {
			--iDepth;
			if ( iDepth < 0 ) {
				return 0;
			}
		} else if ( (sExpr[i] == '?') && (iQuestion == (size_t)-1) ) {
			iQuestion = i;
		} else if ( (sExpr[i] == ':') && (iQuestion != (size_t)-1) && (iDepth == 0) ) {
			iColon = i;
			break;
		}
	}
	if ( (iQuestion == (size_t)-1) || (iColon == (size_t)-1) ) {
		return 0;
	}
	iCondStart = 0;
	iCondEnd = iQuestion;
	while ( (iCondStart < iCondEnd) && __xlIsAsciiSpace((unsigned char)sExpr[iCondStart]) ) {
		++iCondStart;
	}
	while ( (iCondEnd > iCondStart) && __xlIsAsciiSpace((unsigned char)sExpr[iCondEnd - 1]) ) {
		--iCondEnd;
	}
	if ( iCondEnd <= iCondStart ) {
		return 0;
	}
	if ( __xlBindingMacroExprSegmentIsBoolish(sExpr + iCondStart, iCondEnd - iCondStart) ) {
		return 0;
	}
	iCondLen = iCondEnd - iCondStart;
	if ( snprintf(
		sOut,
		sizeof(sOut),
		"(%.*s != 0) %s",
		(int)iCondLen,
		sExpr + iCondStart,
		sExpr + iQuestion
	) >= (int)sizeof(sOut) ) {
		return 0;
	}
	snprintf(sExpr, 512, "%s", sOut);
	return 1;
}

static int __xlBindingEmitFunctionLikeMacro(
	const char* sName,
	const char* pAfterName,
	const char* sLine,
	XL_ModuleTextBuffer* pOut,
	XL_BindingContext* pBindingCtx
)
{
	char sParams[256];
	char sExpr[512];
	char sReturnExpr[640];
	char sParamList[384];
	const char* pClose;
	const char* pValue;
	char* pCtx;
	char* pParam;
	size_t iParamLen;
	size_t iExprLen;
	size_t iParamCount;

	if ( (sName == NULL) || (pAfterName == NULL) || (pOut == NULL) || (*pAfterName != '(') ) {
		return 0;
	}
	pClose = strchr(pAfterName, ')');
	if ( pClose == NULL ) {
		__xlBindingSkip(pBindingCtx, "invalid function-like macro params", sLine);
		return 1;
	}
	iParamLen = (size_t)(pClose - (pAfterName + 1));
	if ( iParamLen >= sizeof(sParams) ) {
		__xlBindingSkip(pBindingCtx, "function-like macro params too long", sLine);
		return 1;
	}
	memcpy(sParams, pAfterName + 1, iParamLen);
	sParams[iParamLen] = '\0';
	pValue = pClose + 1;
	while ( (*pValue == ' ') || (*pValue == '\t') ) {
		++pValue;
	}
	if ( *pValue == '\0' ) {
		__xlBindingSkip(pBindingCtx, "empty function-like macro value", sLine);
		return 1;
	}
	snprintf(sExpr, sizeof(sExpr), "%s", pValue);
	__xlBindingTrim(sExpr);
	if ( strstr(sExpr, "sizeof") != NULL ) {
		if ( !__xlBindingMacroExprFoldSizeof(pBindingCtx, sExpr) ) {
			__xlBindingSkip(pBindingCtx, "unsupported function-like macro sizeof", sLine);
			return 1;
		}
	}
	__xlBindingStripSimpleCCasts(sExpr);
	__xlBindingStripMappedIntegralCCasts(pBindingCtx, sExpr);
	__xlBindingReplaceBuiltinConstTokens(sExpr);
	__xlBindingStripOuterParens(sExpr);
	__xlBindingStripSimpleCCasts(sExpr);
	__xlBindingStripMappedIntegralCCasts(pBindingCtx, sExpr);
	__xlBindingReplaceBuiltinConstTokens(sExpr);
	if ( strstr(sExpr, "sizeof") != NULL ) {
		if ( !__xlBindingMacroExprFoldSizeof(pBindingCtx, sExpr) ) {
			__xlBindingSkip(pBindingCtx, "unsupported function-like macro sizeof", sLine);
			return 1;
		}
	}
	if ( !__xlBindingMacroExprLooksSimple(sExpr) ) {
		__xlBindingSkip(pBindingCtx, "unsupported function-like macro expression", sLine);
		return 1;
	}
	(void)__xlBindingMacroExprRewriteLogical(sExpr);
	(void)__xlBindingMacroExprRewriteTernaryIntCondition(sExpr);

	sParamList[0] = '\0';
	iParamCount = 0;
	pParam = strtok_s(sParams, ",", &pCtx);
	while ( pParam != NULL ) {
		__xlBindingTrim(pParam);
		if ( (strcmp(pParam, "...") == 0) || !__xlBindingIsIdentText(pParam) ) {
			__xlBindingSkip(pBindingCtx, "unsupported function-like macro parameter", sLine);
			return 1;
		}
		if ( iParamCount != 0 ) {
			strncat(sParamList, ", ", sizeof(sParamList) - strlen(sParamList) - 1);
		}
		strncat(sParamList, "int ", sizeof(sParamList) - strlen(sParamList) - 1);
		strncat(sParamList, pParam, sizeof(sParamList) - strlen(sParamList) - 1);
		iParamCount += 1;
		pParam = strtok_s(NULL, ",", &pCtx);
	}
	iExprLen = strlen(sExpr);
	if ( iExprLen == 0 ) {
		__xlBindingSkip(pBindingCtx, "unsupported empty function-like macro", sLine);
		return 1;
	}
	if ( __xlBindingMacroExprIsBoolish(sExpr) ) {
		snprintf(sReturnExpr, sizeof(sReturnExpr), "int(%s)", sExpr);
	} else {
		snprintf(sReturnExpr, sizeof(sReturnExpr), "%s", sExpr);
	}
	__xlModuleBufAppendFormat(pOut, "func %s(%s) as int\n\treturn %s\nend\n", sName, sParamList, sReturnExpr);
	if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
	__xlBindingTrace(pBindingCtx, "define function_macro='%s' params=%zu", sName, iParamCount);
	return 1;
}

static int __xlBindingParseStringLiteralPrefix(const char* p, size_t* piPrefixLen)
{
	if ( (p == NULL) || (piPrefixLen == NULL) ) {
		return 0;
	}
	if ( p[0] == '"' ) {
		*piPrefixLen = 0;
		return 1;
	}
	if ( (p[0] == 'u') && (p[1] == '8') && (p[2] == '"') ) {
		*piPrefixLen = 2;
		return 1;
	}
	if ( ((p[0] == 'L') || (p[0] == 'u') || (p[0] == 'U')) && (p[1] == '"') ) {
		*piPrefixLen = 1;
		return 1;
	}
	return 0;
}

static int __xlBindingNormalizeStringDefine(const char* sValue, char* sOut, size_t iOutSize)
{
	const char* p;
	size_t iWrite;
	int bSawLiteral;

	if ( (sValue == NULL) || (sOut == NULL) || (iOutSize < 3) ) {
		return 0;
	}
	p = sValue;
	iWrite = 0;
	bSawLiteral = 0;
	sOut[iWrite++] = '"';
	while ( *p != '\0' ) {
		size_t iPrefixLen;
		int bEscape;

		while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) {
			++p;
		}
		if ( *p == '\0' ) {
			break;
		}
		if ( !__xlBindingParseStringLiteralPrefix(p, &iPrefixLen) ) {
			return 0;
		}
		p += iPrefixLen;
		if ( *p != '"' ) {
			return 0;
		}
		++p;
		bEscape = 0;
		while ( *p != '\0' ) {
			char c = *p++;

			if ( !bEscape && (c == '"') ) {
				break;
			}
			if ( (iWrite + 2) >= iOutSize ) {
				return 0;
			}
			sOut[iWrite++] = c;
			if ( bEscape ) {
				bEscape = 0;
			} else if ( c == '\\' ) {
				bEscape = 1;
			}
		}
		if ( bEscape || (p[-1] != '"') ) {
			return 0;
		}
		bSawLiteral = 1;
	}
	if ( !bSawLiteral || ((iWrite + 2) > iOutSize) ) {
		return 0;
	}
	sOut[iWrite++] = '"';
	sOut[iWrite] = '\0';
	return 1;
}

static int __xlBindingEmitDefine(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	const char* p;
	size_t iNameLen;
	char sName[128];
	char sValue[512];
	char sString[512];
	char sNumber[160];
	int bFloat;
	int bUnsigned;
	int iChar;
	long long iExprValue;

	if ( (sLine == NULL) || (pOut == NULL) || (strncmp(sLine, "#define", 7) != 0) ) {
		return XL_RC_OK;
	}
	p = sLine + 7;
	if ( (*p != ' ') && (*p != '\t') ) {
		__xlBindingSkip(pBindingCtx, "unsupported define directive", sLine);
		return XL_RC_OK;
	}
	while ( (*p == ' ') || (*p == '\t') ) {
		++p;
	}
	iNameLen = 0;
	while ( __xlIsLinkIdentCont((unsigned char)p[iNameLen]) ) {
		++iNameLen;
	}
	if ( (iNameLen == 0) || (iNameLen >= sizeof(sName)) ) {
		__xlBindingSkip(pBindingCtx, "invalid define name", sLine);
		return XL_RC_OK;
	}
	memcpy(sName, p, iNameLen);
	sName[iNameLen] = '\0';
	if ( !__xlBindingIsIdentText(sName) ) {
		__xlBindingSkip(pBindingCtx, "invalid define identifier", sLine);
		return XL_RC_OK;
	}
	(void)__xlBindingAddMacro(pBindingCtx, sName);
	p += iNameLen;
	if ( *p == '(' ) {
		if ( __xlBindingEmitFunctionLikeMacro(sName, p, sLine, pOut, pBindingCtx) ) {
			return XL_RC_OK;
		}
		__xlBindingSkip(pBindingCtx, "function-like macro", sLine);
		return XL_RC_OK;
	}
	while ( (*p == ' ') || (*p == '\t') ) {
		++p;
	}
	if ( *p == '\0' ) {
		(void)__xlBindingAddDeclMacro(pBindingCtx, sName);
		__xlBindingTrace(pBindingCtx, "define macro='%s'", sName);
		return XL_RC_OK;
	}
	snprintf(sValue, sizeof(sValue), "%s", p);
	__xlBindingTrim(sValue);
	__xlBindingStripOuterParens(sValue);
	if ( __xlBindingDefineLooksDeclMacro(sValue) ) {
		(void)__xlBindingAddDeclMacro(pBindingCtx, sName);
		__xlBindingTrace(pBindingCtx, "define decl_macro='%s'", sName);
		return XL_RC_OK;
	}
	if ( __xlBindingIsIdentText(sValue) && __xlBindingFindDeclMacro(pBindingCtx, sValue) ) {
		(void)__xlBindingAddDeclMacro(pBindingCtx, sName);
		__xlBindingTrace(pBindingCtx, "define decl_macro='%s' alias='%s'", sName, sValue);
		return XL_RC_OK;
	}
	if ( __xlBindingNormalizeStringDefine(sValue, sString, sizeof(sString)) ) {
		__xlModuleBufAppendFormat(pOut, "const string %s = %s\n", sName, sString);
		(void)__xlBindingAddStringConst(pBindingCtx, sName);
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='string'", sName);
		return XL_RC_OK;
	}
	if ( __xlBindingIsIdentText(sValue) && __xlBindingFindStringConst(pBindingCtx, sValue) ) {
		__xlModuleBufAppendFormat(pOut, "const string %s = %s\n", sName, sValue);
		(void)__xlBindingAddStringConst(pBindingCtx, sName);
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='string' alias='%s'", sName, sValue);
		return XL_RC_OK;
	}
	if ( __xlBindingIsIdentText(sValue) && __xlBindingFindFloatConst(pBindingCtx, sValue) ) {
		__xlModuleBufAppendFormat(pOut, "const float64 %s = %s\n", sName, sValue);
		(void)__xlBindingAddFloatConst(pBindingCtx, sName);
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='float64' alias='%s'", sName, sValue);
		return XL_RC_OK;
	}
	if ( __xlBindingIsIdentText(sValue) &&
		(strcmp(sValue, "NULL") != 0) && (strcmp(sValue, "TRUE") != 0) && (strcmp(sValue, "FALSE") != 0) &&
		__xlBindingFindConst(pBindingCtx, sValue, &iExprValue) ) {
		const char* sType = ((iExprValue < 0) || (iExprValue <= 2147483647LL)) ? "int" : "uint64";

		__xlModuleBufAppendFormat(pOut, "const %s %s = %s\n", sType, sName, sValue);
		(void)__xlBindingAddConst(pBindingCtx, sName, iExprValue);
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='%s' alias='%s'", sName, sType, sValue);
		return XL_RC_OK;
	}
	if ( __xlBindingNormalizePointNullDefine(pBindingCtx, sValue) ) {
		__xlModuleBufAppendFormat(pOut, "const point %s = point(null)\n", sName);
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='point' null='1'", sName);
		return XL_RC_OK;
	}
	if ( __xlBindingParseCharLiteral(sValue, &iChar) ) {
		__xlModuleBufAppendFormat(pOut, "const int %s = %d\n", sName, iChar);
		(void)__xlBindingAddConst(pBindingCtx, sName, (long long)iChar);
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='int'", sName);
		return XL_RC_OK;
	}
	if ( __xlBindingNormalizeNumber(sValue, sNumber, sizeof(sNumber), &bFloat, &bUnsigned) ) {
		const char* sType = bFloat ? "float64" : (bUnsigned ? "uint64" : "int");

		__xlModuleBufAppendFormat(pOut, "const %s %s = %s\n", sType, sName, sNumber);
		if ( !bFloat ) {
			errno = 0;
			iExprValue = strtoll(sNumber, NULL, 10);
			if ( errno == 0 ) {
				(void)__xlBindingAddConst(pBindingCtx, sName, iExprValue);
			}
		} else {
			(void)__xlBindingAddFloatConst(pBindingCtx, sName);
		}
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='%s'", sName, sType);
		return XL_RC_OK;
	}
	if ( __xlBindingEvalIntConstExpr(pBindingCtx, sValue, &iExprValue) ) {
		const char* sType = ((iExprValue < 0) || (iExprValue <= 2147483647LL)) ? "int" : "uint64";

		__xlModuleBufAppendFormat(pOut, "const %s %s = %lld\n", sType, sName, iExprValue);
		(void)__xlBindingAddConst(pBindingCtx, sName, iExprValue);
		if ( pBindingCtx != NULL ) pBindingCtx->iDefineCount += 1;
		__xlBindingTrace(pBindingCtx, "define name='%s' type='%s' expr='%lld'", sName, sType, iExprValue);
		return XL_RC_OK;
	}
	__xlBindingSkip(pBindingCtx, "unsupported define value", sLine);
	return XL_RC_OK;
}

static int __xlBindingEmitUndef(const char* sLine, XL_BindingContext* pBindingCtx)
{
	const char* p;
	size_t iNameLen;
	char sName[128];

	if ( (sLine == NULL) || (strncmp(sLine, "#undef", 6) != 0) ) {
		return XL_RC_OK;
	}
	p = sLine + 6;
	while ( (*p == ' ') || (*p == '\t') ) {
		++p;
	}
	iNameLen = 0;
	while ( __xlIsLinkIdentCont((unsigned char)p[iNameLen]) ) {
		++iNameLen;
	}
	if ( (iNameLen == 0) || (iNameLen >= sizeof(sName)) ) {
		__xlBindingSkip(pBindingCtx, "invalid undef name", sLine);
		return XL_RC_OK;
	}
	memcpy(sName, p, iNameLen);
	sName[iNameLen] = '\0';
	if ( !__xlBindingIsIdentText(sName) ) {
		__xlBindingSkip(pBindingCtx, "invalid undef identifier", sLine);
		return XL_RC_OK;
	}
	(void)__xlBindingRemoveMacro(pBindingCtx, sName);
	__xlBindingRemoveDeclMacro(pBindingCtx, sName);
	__xlBindingTrace(pBindingCtx, "undef macro='%s'", sName);
	return XL_RC_OK;
}

static int __xlBindingEmitFunction(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	char sWork[1024];
	char sLeft[512];
	char sParams[512];
	char sParamOut[512];
	char sName[128];
	char sReturnType[256];
	char sReturnXl[64];
	char* pOpen;
	char* pClose;
	size_t iNameEnd;
	size_t iNameStart;
	size_t i;

	snprintf(sWork, sizeof(sWork), "%s", sLine);
	__xlBindingRemoveStorageWords(sWork);
	pOpen = strchr(sWork, '(');
	pClose = strrchr(sWork, ')');
	if ( (pOpen == NULL) || (pClose == NULL) || (pClose < pOpen) ) {
		__xlBindingSkip(pBindingCtx, "invalid function prototype", sLine);
		return XL_RC_OK;
	}
	*pOpen = '\0';
	*pClose = '\0';
	snprintf(sLeft, sizeof(sLeft), "%s", sWork);
	snprintf(sParams, sizeof(sParams), "%s", pOpen + 1);
	__xlBindingTrim(sLeft);
	iNameEnd = strlen(sLeft);
	while ( (iNameEnd != 0) && __xlIsAsciiSpace((unsigned char)sLeft[iNameEnd - 1]) ) {
		--iNameEnd;
	}
	iNameStart = iNameEnd;
	while ( (iNameStart != 0) && __xlIsLinkIdentCont((unsigned char)sLeft[iNameStart - 1]) ) {
		--iNameStart;
	}
	if ( (iNameStart == iNameEnd) || (iNameStart == 0) ) {
		__xlBindingSkip(pBindingCtx, "invalid function name", sLine);
		return XL_RC_OK;
	}
	i = iNameEnd - iNameStart;
	if ( i >= sizeof(sName) ) i = sizeof(sName) - 1;
	memcpy(sName, sLeft + iNameStart, i);
	sName[i] = '\0';
	sLeft[iNameStart] = '\0';
	snprintf(sReturnType, sizeof(sReturnType), "%s", sLeft);
	__xlBindingTrim(sReturnType);
	if ( !__xlBindingMapCType(pBindingCtx, sReturnType, sReturnXl, sizeof(sReturnXl)) ) {
		__xlBindingSkip(pBindingCtx, "unsupported function return type", sLine);
		return XL_RC_OK;
	}
	sParamOut[0] = '\0';
	__xlBindingTrim(sParams);
	if ( (sParams[0] != '\0') && (strcmp(sParams, "void") != 0) ) {
		char* pTok;
		char* pCtx;
		int bFirst;
		int iParamIndex;

		bFirst = 1;
		iParamIndex = 0;
		pTok = strtok_s(sParams, ",", &pCtx);
		while ( pTok != NULL ) {
			char sParamType[160];
			char sParamXl[64];
			char sParamName[32];

			snprintf(sParamType, sizeof(sParamType), "%s", pTok);
			(void)__xlBindingDecayArrayParam(sParamType);
			__xlBindingRemoveParamName(sParamType);
			if ( __xlBindingMapCType(pBindingCtx, sParamType, sParamXl, sizeof(sParamXl)) && (strcmp(sParamXl, "void") != 0) ) {
				if ( !bFirst ) {
					strncat(sParamOut, ", ", sizeof(sParamOut) - strlen(sParamOut) - 1);
				}
				strncat(sParamOut, sParamXl, sizeof(sParamOut) - strlen(sParamOut) - 1);
				snprintf(sParamName, sizeof(sParamName), " p%d", iParamIndex++);
				strncat(sParamOut, sParamName, sizeof(sParamOut) - strlen(sParamOut) - 1);
				bFirst = 0;
			} else {
				__xlBindingSkip(pBindingCtx, "unsupported function parameter type", sLine);
				return XL_RC_OK;
			}
			pTok = strtok_s(NULL, ",", &pCtx);
		}
	}
	__xlModuleBufAppendFormat(pOut, "extern \"%s\" func %s(", sName, sName);
	__xlModuleBufAppend(pOut, sParamOut);
	__xlModuleBufAppendFormat(pOut, ") as %s\n", sReturnXl);
	if ( pBindingCtx != NULL ) pBindingCtx->iFunctionCount += 1;
	__xlBindingTrace(pBindingCtx, "function name='%s' return='%s'", sName, sReturnXl);
	return XL_RC_OK;
}

static int __xlBindingEmitVar(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	char sWork[512];
	char sName[128];
	char sType[256];
	char sXlType[64];
	size_t iEnd;
	size_t iStart;
	size_t iLen;

	snprintf(sWork, sizeof(sWork), "%s", sLine);
	__xlBindingRemoveStorageWords(sWork);
	__xlBindingTrim(sWork);
	iEnd = strlen(sWork);
	while ( (iEnd != 0) && __xlIsAsciiSpace((unsigned char)sWork[iEnd - 1]) ) {
		--iEnd;
	}
	iStart = iEnd;
	while ( (iStart != 0) && __xlIsLinkIdentCont((unsigned char)sWork[iStart - 1]) ) {
		--iStart;
	}
	if ( (iStart == iEnd) || (iStart == 0) ) {
		__xlBindingSkip(pBindingCtx, "invalid extern var name", sLine);
		return XL_RC_OK;
	}
	iLen = iEnd - iStart;
	if ( iLen >= sizeof(sName) ) iLen = sizeof(sName) - 1;
	memcpy(sName, sWork + iStart, iLen);
	sName[iLen] = '\0';
	sWork[iStart] = '\0';
	snprintf(sType, sizeof(sType), "%s", sWork);
	__xlBindingTrim(sType);
	if ( !__xlBindingMapCType(pBindingCtx, sType, sXlType, sizeof(sXlType)) || (strcmp(sXlType, "void") == 0) ) {
		__xlBindingSkip(pBindingCtx, "unsupported extern var type", sLine);
		return XL_RC_OK;
	}
	__xlModuleBufAppendFormat(pOut, "extern \"%s\" var %s as %s\n", sName, sName, sXlType);
	if ( pBindingCtx != NULL ) pBindingCtx->iVarCount += 1;
	__xlBindingTrace(pBindingCtx, "var name='%s' type='%s'", sName, sXlType);
	return XL_RC_OK;
}

static int __xlBindingSplitTrailingName(char* sText, char* sNameOut, size_t iNameOutSize)
{
	size_t iEnd;
	size_t iStart;
	size_t iLen;

	if ( (sText == NULL) || (sNameOut == NULL) || (iNameOutSize == 0) ) {
		return 0;
	}
	__xlBindingTrim(sText);
	iEnd = strlen(sText);
	while ( (iEnd != 0) && __xlIsAsciiSpace((unsigned char)sText[iEnd - 1]) ) {
		--iEnd;
	}
	iStart = iEnd;
	while ( (iStart != 0) && __xlIsLinkIdentCont((unsigned char)sText[iStart - 1]) ) {
		--iStart;
	}
	if ( (iStart == iEnd) || (iStart == 0) ) {
		return 0;
	}
	iLen = iEnd - iStart;
	if ( iLen >= iNameOutSize ) iLen = iNameOutSize - 1;
	memcpy(sNameOut, sText + iStart, iLen);
	sNameOut[iLen] = '\0';
	sText[iStart] = '\0';
	__xlBindingTrim(sText);
	return sText[0] != '\0';
}

static int __xlBindingBuildFunctionType(
	const XL_BindingContext* pBindingCtx,
	const char* sReturnType,
	const char* sParams,
	char* sOut,
	size_t iOutSize)
{
	char sReturnXl[96];
	char sParamWork[512];

	if ( (pBindingCtx == NULL) || (sReturnType == NULL) || (sParams == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return 0;
	}
	if ( !__xlBindingMapCType(pBindingCtx, sReturnType, sReturnXl, sizeof(sReturnXl)) ) {
		return 0;
	}
	snprintf(sOut, iOutSize, "function<%s(", sReturnXl);
	snprintf(sParamWork, sizeof(sParamWork), "%s", sParams);
	__xlBindingTrim(sParamWork);
	if ( (sParamWork[0] != '\0') && (strcmp(sParamWork, "void") != 0) ) {
		char* pTok;
		char* pCtx;
		int bFirst = 1;

		pTok = strtok_s(sParamWork, ",", &pCtx);
		while ( pTok != NULL ) {
			char sParamType[160];
			char sParamXl[96];

			snprintf(sParamType, sizeof(sParamType), "%s", pTok);
			(void)__xlBindingDecayArrayParam(sParamType);
			__xlBindingRemoveParamName(sParamType);
			if ( !__xlBindingMapCType(pBindingCtx, sParamType, sParamXl, sizeof(sParamXl)) || (strcmp(sParamXl, "void") == 0) ) {
				return 0;
			}
			if ( !bFirst ) {
				strncat(sOut, ",", iOutSize - strlen(sOut) - 1);
			}
			strncat(sOut, sParamXl, iOutSize - strlen(sOut) - 1);
			bFirst = 0;
			pTok = strtok_s(NULL, ",", &pCtx);
		}
	}
	strncat(sOut, ")>", iOutSize - strlen(sOut) - 1);
	return 1;
}

static int __xlBindingEmitTypedefFunctionPointer(const char* sLine, XL_BindingContext* pBindingCtx)
{
	char sWork[768];
	char sReturnType[256];
	char sParams[512];
	char sAlias[96];
	char sFunctionType[256];
	char* pStar;
	char* pOpen;
	char* pClose;
	char* pParamsOpen;
	char* pParamsClose;
	char* pAliasStart;
	size_t iAliasLen;
	size_t i;

	if ( (sLine == NULL) || (pBindingCtx == NULL) || (strncmp(sLine, "typedef ", 8) != 0) ) {
		return XL_RC_OK;
	}
	snprintf(sWork, sizeof(sWork), "%s", sLine + 8);
	pStar = strchr(sWork, '*');
	if ( pStar == NULL ) {
		return XL_RC_OK;
	}
	pOpen = NULL;
	for ( i = 0; sWork + i < pStar; ++i ) {
		if ( sWork[i] == '(' ) {
			pOpen = sWork + i;
		}
	}
	if ( pOpen == NULL ) {
		return XL_RC_OK;
	}
	pAliasStart = pStar + 1;
	while ( (*pAliasStart == ' ') || (*pAliasStart == '\t') ) {
		++pAliasStart;
	}
	iAliasLen = 0;
	while ( __xlIsLinkIdentCont((unsigned char)pAliasStart[iAliasLen]) ) {
		++iAliasLen;
	}
	if ( (iAliasLen == 0) || (iAliasLen >= sizeof(sAlias)) ) {
		__xlBindingSkip(pBindingCtx, "invalid typedef function pointer name", sLine);
		return XL_RC_OK;
	}
	memcpy(sAlias, pAliasStart, iAliasLen);
	sAlias[iAliasLen] = '\0';
	if ( !__xlBindingIsIdentText(sAlias) ) {
		__xlBindingSkip(pBindingCtx, "invalid typedef function pointer identifier", sLine);
		return XL_RC_OK;
	}
	pClose = strchr(pAliasStart + iAliasLen, ')');
	if ( pClose == NULL ) {
		__xlBindingSkip(pBindingCtx, "invalid typedef function pointer declarator", sLine);
		return XL_RC_OK;
	}
	pParamsOpen = pClose + 1;
	while ( (*pParamsOpen == ' ') || (*pParamsOpen == '\t') ) {
		++pParamsOpen;
	}
	if ( *pParamsOpen != '(' ) {
		__xlBindingSkip(pBindingCtx, "missing typedef function pointer params", sLine);
		return XL_RC_OK;
	}
	pParamsClose = strrchr(pParamsOpen, ')');
	if ( (pParamsClose == NULL) || (pParamsClose <= pParamsOpen) ) {
		__xlBindingSkip(pBindingCtx, "invalid typedef function pointer params", sLine);
		return XL_RC_OK;
	}
	*pOpen = '\0';
	*pStar = '\0';
	*pParamsClose = '\0';
	snprintf(sReturnType, sizeof(sReturnType), "%s %s", sWork, pOpen + 1);
	__xlBindingRemoveStorageWords(sReturnType);
	__xlBindingTrim(sReturnType);
	snprintf(sParams, sizeof(sParams), "%s", pParamsOpen + 1);
	if ( !__xlBindingBuildFunctionType(pBindingCtx, sReturnType, sParams, sFunctionType, sizeof(sFunctionType)) ) {
		__xlBindingSkip(pBindingCtx, "unsupported typedef function pointer signature", sLine);
		return XL_RC_OK;
	}
	(void)__xlBindingAddType(pBindingCtx, sAlias, sFunctionType, 0);
	pBindingCtx->iTypeDeclCount += 1;
	__xlBindingTrace(pBindingCtx, "typedef function_pointer='%s' type='%s'", sAlias, sFunctionType);
	return XL_RC_OK;
}

static void __xlBindingStripTrailingDeclaratorPointers(char* sType);

static int __xlBindingEmitTypedefAlias(const char* sLine, XL_BindingContext* pBindingCtx)
{
	char sWork[512];
	char sBaseType[256];
	char* pTok;
	char* pCtx;
	int iRc;
	size_t iTypeDeclCountBefore;
	int bFirst;
	int bEmitted;

	if ( (sLine == NULL) || (pBindingCtx == NULL) || (strncmp(sLine, "typedef ", 8) != 0) || (strstr(sLine, "{") != NULL) ) {
		return XL_RC_OK;
	}
	if ( strchr(sLine, '*') != NULL && strchr(sLine, '(') != NULL && strchr(sLine, ')') != NULL ) {
		iTypeDeclCountBefore = pBindingCtx->iTypeDeclCount;
		iRc = __xlBindingEmitTypedefFunctionPointer(sLine, pBindingCtx);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		if ( pBindingCtx->iTypeDeclCount != iTypeDeclCountBefore ) {
			return XL_RC_OK;
		}
	}
	snprintf(sWork, sizeof(sWork), "%s", sLine + 8);
	sBaseType[0] = '\0';
	bFirst = 1;
	bEmitted = 0;
	pTok = strtok_s(sWork, ",", &pCtx);
	while ( pTok != NULL ) {
		char sDecl[256];
		char sAlias[96];
		char sAliasType[256];
		char sXlType[96];
		char sStructName[128];
		const char* sOpaqueKind;
		const char* pOpaqueName;
		char* pAliasStart;

		snprintf(sDecl, sizeof(sDecl), "%s", pTok);
		__xlBindingTrim(sDecl);
		if ( sDecl[0] != '\0' ) {
			if ( bFirst ) {
				snprintf(sAliasType, sizeof(sAliasType), "%s", sDecl);
				if ( !__xlBindingSplitTrailingName(sAliasType, sAlias, sizeof(sAlias)) ) {
					__xlBindingSkip(pBindingCtx, "invalid typedef alias", sLine);
					return XL_RC_OK;
				}
				snprintf(sBaseType, sizeof(sBaseType), "%s", sAliasType);
				__xlBindingStripTrailingDeclaratorPointers(sBaseType);
			} else {
				pAliasStart = sDecl;
				while ( (*pAliasStart == ' ') || (*pAliasStart == '\t') ) {
					++pAliasStart;
				}
				snprintf(sAliasType, sizeof(sAliasType), "%s", sBaseType);
				while ( *pAliasStart == '*' ) {
					strncat(sAliasType, " *", sizeof(sAliasType) - strlen(sAliasType) - 1);
					++pAliasStart;
					while ( (*pAliasStart == ' ') || (*pAliasStart == '\t') ) {
						++pAliasStart;
					}
				}
				snprintf(sAlias, sizeof(sAlias), "%s", pAliasStart);
				__xlBindingTrim(sAlias);
				if ( !__xlBindingIsIdentText(sAlias) ) {
					__xlBindingSkip(pBindingCtx, "invalid typedef alias", sLine);
					return XL_RC_OK;
				}
			}
			if ( __xlBindingMapCType(pBindingCtx, sAliasType, sXlType, sizeof(sXlType)) ) {
				pBindingCtx->iTypeDeclCount += 1;
				bEmitted = 1;
				__xlBindingTrace(pBindingCtx, "typedef alias='%s' type='%s'", sAlias, sXlType);
				(void)__xlBindingAddType(pBindingCtx, sAlias, sXlType, 0);
			} else if ( (strncmp(sAliasType, "struct ", 7) == 0) || (strncmp(sAliasType, "union ", 6) == 0) ) {
				if ( strncmp(sAliasType, "struct ", 7) == 0 ) {
					sOpaqueKind = "struct";
					pOpaqueName = sAliasType + 7;
				} else {
					sOpaqueKind = "union";
					pOpaqueName = sAliasType + 6;
				}
				snprintf(sStructName, sizeof(sStructName), "%s", pOpaqueName);
				__xlBindingTrim(sStructName);
				if ( __xlBindingIsIdentText(sStructName) && __xlBindingIsIdentText(sAlias) ) {
					(void)__xlBindingAddType(pBindingCtx, sStructName, "point", 0);
					(void)__xlBindingAddType(pBindingCtx, sAliasType, "point", 0);
					(void)__xlBindingAddType(pBindingCtx, sAlias, "point", 0);
					__xlBindingAddOpaqueAlias(pBindingCtx, sOpaqueKind, sStructName, sAlias);
					pBindingCtx->iTypeDeclCount += 1;
					bEmitted = 1;
					__xlBindingTrace(pBindingCtx, "typedef opaque_%s='%s' alias='%s' type='point'", sOpaqueKind, sStructName, sAlias);
				} else {
					__xlBindingSkip(pBindingCtx, "unsupported typedef target type", sLine);
					return XL_RC_OK;
				}
			} else {
				__xlBindingSkip(pBindingCtx, "unsupported typedef target type", sLine);
				return XL_RC_OK;
			}
		}
		bFirst = 0;
		pTok = strtok_s(NULL, ",", &pCtx);
	}
	if ( !bEmitted ) {
		__xlBindingSkip(pBindingCtx, "unsupported typedef target type", sLine);
	}
	return XL_RC_OK;
}

static int __xlBindingEmitStructFunctionPointerField(
	const char* sField,
	XL_ModuleTextBuffer* pOut,
	XL_BindingContext* pBindingCtx,
	const char* sSkipReason)
{
	char sWork[512];
	char sReturnType[256];
	char sParams[256];
	char sFieldName[96];
	char sFunctionType[256];
	char* pStar;
	char* pOpen;
	char* pClose;
	char* pParamsOpen;
	char* pParamsClose;
	char* pNameStart;
	size_t iNameLen;
	size_t i;

	if ( (sField == NULL) || (pOut == NULL) || (pBindingCtx == NULL) ) {
		return 0;
	}
	snprintf(sWork, sizeof(sWork), "%s", sField);
	__xlBindingRemoveStorageWords(sWork);
	__xlBindingTrim(sWork);
	pStar = strchr(sWork, '*');
	if ( pStar == NULL ) {
		return 0;
	}
	pOpen = NULL;
	for ( i = 0; sWork + i < pStar; ++i ) {
		if ( sWork[i] == '(' ) {
			pOpen = sWork + i;
		}
	}
	if ( pOpen == NULL ) {
		return 0;
	}
	pNameStart = pStar + 1;
	while ( (*pNameStart == ' ') || (*pNameStart == '\t') ) {
		++pNameStart;
	}
	iNameLen = 0;
	while ( __xlIsLinkIdentCont((unsigned char)pNameStart[iNameLen]) ) {
		++iNameLen;
	}
	if ( (iNameLen == 0) || (iNameLen >= sizeof(sFieldName)) ) {
		__xlBindingSkip(pBindingCtx, sSkipReason, sField);
		return 0;
	}
	memcpy(sFieldName, pNameStart, iNameLen);
	sFieldName[iNameLen] = '\0';
	if ( !__xlBindingIsIdentText(sFieldName) ) {
		__xlBindingSkip(pBindingCtx, sSkipReason, sField);
		return 0;
	}
	pClose = strchr(pNameStart + iNameLen, ')');
	if ( pClose == NULL ) {
		__xlBindingSkip(pBindingCtx, sSkipReason, sField);
		return 0;
	}
	pParamsOpen = pClose + 1;
	while ( (*pParamsOpen == ' ') || (*pParamsOpen == '\t') ) {
		++pParamsOpen;
	}
	if ( *pParamsOpen != '(' ) {
		return 0;
	}
	pParamsClose = strrchr(pParamsOpen, ')');
	if ( (pParamsClose == NULL) || (pParamsClose <= pParamsOpen) ) {
		__xlBindingSkip(pBindingCtx, sSkipReason, sField);
		return 0;
	}
	*pOpen = '\0';
	*pStar = '\0';
	*pParamsClose = '\0';
	snprintf(sReturnType, sizeof(sReturnType), "%s %s", sWork, pOpen + 1);
	__xlBindingRemoveStorageWords(sReturnType);
	__xlBindingTrim(sReturnType);
	snprintf(sParams, sizeof(sParams), "%s", pParamsOpen + 1);
	if ( !__xlBindingBuildFunctionType(pBindingCtx, sReturnType, sParams, sFunctionType, sizeof(sFunctionType)) ) {
		__xlBindingSkip(pBindingCtx, sSkipReason, sField);
		return 0;
	}
	__xlModuleBufAppendFormat(pOut, "\tvar %s as %s\n", sFieldName, sFunctionType);
	__xlBindingTrace(pBindingCtx, "struct field function_pointer='%s' type='%s'", sFieldName, sFunctionType);
	return 1;
}

static void __xlBindingStripTrailingDeclaratorPointers(char* sType)
{
	size_t iLen;

	if ( sType == NULL ) {
		return;
	}
	__xlBindingTrim(sType);
	iLen = strlen(sType);
	while ( iLen != 0 ) {
		while ( (iLen != 0) && __xlIsAsciiSpace((unsigned char)sType[iLen - 1]) ) {
			sType[--iLen] = '\0';
		}
		if ( (iLen == 0) || (sType[iLen - 1] != '*') ) {
			break;
		}
		sType[--iLen] = '\0';
	}
	__xlBindingTrim(sType);
}

static int __xlBindingEmitStructFieldDecls(
	const char* sField,
	XL_ModuleTextBuffer* pOut,
	XL_BindingContext* pBindingCtx,
	const char* sSkipReason)
{
	char sWork[512];
	char sBaseType[256];
	char* pTok;
	char* pCtx;
	int bFirst;

	if ( (sField == NULL) || (pOut == NULL) || (pBindingCtx == NULL) ) {
		return 0;
	}
	snprintf(sWork, sizeof(sWork), "%s", sField);
	__xlBindingTrim(sWork);
	if ( sWork[0] == '\0' ) {
		return 1;
	}
	if ( __xlBindingEmitStructFunctionPointerField(sWork, pOut, pBindingCtx, sSkipReason) ) {
		return 1;
	}
	if ( strchr(sWork, ':') != NULL ) {
		char* pColon = strchr(sWork, ':');
		*pColon = '\0';
		__xlBindingTrim(sWork);
		__xlBindingTrace(pBindingCtx, "struct field bitfield='%s'", sWork);
	}
	sBaseType[0] = '\0';
	bFirst = 1;
	pTok = strtok_s(sWork, ",", &pCtx);
	while ( pTok != NULL ) {
		char sDecl[256];
		char sFieldName[96];
		char sFieldType[256];
		char sFieldXlType[96];
		char* pNameStart;

		snprintf(sDecl, sizeof(sDecl), "%s", pTok);
		__xlBindingTrim(sDecl);
		if ( sDecl[0] != '\0' ) {
			if ( bFirst ) {
				snprintf(sFieldType, sizeof(sFieldType), "%s", sDecl);
				if ( strchr(sFieldType, '[') != NULL ) {
					char* pBracket = strchr(sFieldType, '[');
					char* pNameStart;
					*pBracket = '\0';
					__xlBindingTrim(sFieldType);
					pNameStart = sFieldType + strlen(sFieldType);
					while ( (pNameStart > sFieldType) && __xlIsAsciiSpace((unsigned char)pNameStart[-1]) ) {
						*--pNameStart = '\0';
					}
					while ( (pNameStart > sFieldType) && __xlIsLinkIdentCont((unsigned char)pNameStart[-1]) ) {
						--pNameStart;
					}
					snprintf(sFieldName, sizeof(sFieldName), "%s[]", pNameStart);
					*pNameStart = '\0';
					__xlBindingTrim(sFieldType);
					if ( (sFieldType[0] == '\0') || !__xlBindingIsIdentText(sFieldName) ) {
						if ( strlen(sFieldName) >= 2 ) {
							sFieldName[strlen(sFieldName) - 2] = '\0';
						}
						if ( (sFieldType[0] == '\0') || !__xlBindingIsIdentText(sFieldName) ) {
							__xlBindingSkip(pBindingCtx, sSkipReason, sField);
							return 0;
						}
						strncat(sFieldName, "[]", sizeof(sFieldName) - strlen(sFieldName) - 1);
					}
				} else if ( !__xlBindingSplitTrailingName(sFieldType, sFieldName, sizeof(sFieldName)) ) {
					__xlBindingSkip(pBindingCtx, sSkipReason, sField);
					return 0;
				}
				snprintf(sBaseType, sizeof(sBaseType), "%s", sFieldType);
				__xlBindingStripTrailingDeclaratorPointers(sBaseType);
			} else {
				pNameStart = sDecl;
				while ( (*pNameStart == ' ') || (*pNameStart == '\t') ) {
					++pNameStart;
				}
				snprintf(sFieldType, sizeof(sFieldType), "%s", sBaseType);
				while ( *pNameStart == '*' ) {
					strncat(sFieldType, " *", sizeof(sFieldType) - strlen(sFieldType) - 1);
					++pNameStart;
					while ( (*pNameStart == ' ') || (*pNameStart == '\t') ) {
						++pNameStart;
					}
				}
				snprintf(sFieldName, sizeof(sFieldName), "%s", pNameStart);
				__xlBindingTrim(sFieldName);
				if ( !__xlBindingIsIdentText(sFieldName) ) {
					__xlBindingSkip(pBindingCtx, sSkipReason, sField);
					return 0;
				}
			}
			if ( strchr(sFieldName, '[') != NULL ) {
				char* pBracket = strchr(sFieldName, '[');
				*pBracket = '\0';
				__xlBindingTrim(sFieldName);
				if ( !__xlBindingIsIdentText(sFieldName) ) {
					__xlBindingSkip(pBindingCtx, sSkipReason, sField);
					return 0;
				}
				if ( __xlBindingMapCType(pBindingCtx, sFieldType, sFieldXlType, sizeof(sFieldXlType)) &&
					((strcmp(sFieldXlType, "int8") == 0) || (strcmp(sFieldXlType, "uint8") == 0)) ) {
					__xlModuleBufAppendFormat(pOut, "\tvar %s as string\n", sFieldName);
					__xlBindingTrace(pBindingCtx, "struct field inline_array='%s' type='string'", sFieldName);
				} else {
					__xlModuleBufAppendFormat(pOut, "\tvar %s as point\n", sFieldName);
					__xlBindingTrace(pBindingCtx, "struct field inline_array='%s' type='point'", sFieldName);
				}
				bFirst = 0;
				pTok = strtok_s(NULL, ",", &pCtx);
				continue;
			}
			if ( strchr(sFieldType, '[') != NULL ) {
				__xlBindingSkip(pBindingCtx, sSkipReason, sField);
				return 0;
			}
			if ( __xlBindingMapCType(pBindingCtx, sFieldType, sFieldXlType, sizeof(sFieldXlType)) &&
				(strcmp(sFieldXlType, "void") != 0) ) {
				__xlModuleBufAppendFormat(pOut, "\tvar %s as %s\n", sFieldName, sFieldXlType);
			} else {
				__xlBindingSkip(pBindingCtx, sSkipReason, sField);
				return 0;
			}
		}
		bFirst = 0;
		pTok = strtok_s(NULL, ",", &pCtx);
	}
	return 1;
}

static int __xlBindingEmitTypedefStruct(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	char sWork[1024];
	char sFields[768];
	char sName[96];
	char sTag[96];
	char sAliases[256];
	char arrValueAliases[8][96];
	char arrPtrAliases[8][96];
	XL_ModuleTextBuffer fieldBuf;
	char* pOpen;
	char* pClose;
	char* pAfter;
	char* pTok;
	char* pCtx;
	size_t iValueAliasCount;
	size_t iPtrAliasCount;
	int bFieldsOk;

	if ( (sLine == NULL) || (pOut == NULL) || (pBindingCtx == NULL) || (strncmp(sLine, "typedef struct", 14) != 0) ) {
		return XL_RC_OK;
	}
	snprintf(sWork, sizeof(sWork), "%s", sLine);
	pOpen = strchr(sWork, '{');
	pClose = strrchr(sWork, '}');
	if ( (pOpen == NULL) || (pClose == NULL) || (pClose < pOpen) ) {
		__xlBindingSkip(pBindingCtx, "invalid typedef struct", sLine);
		return XL_RC_OK;
	}
	*pOpen = '\0';
	*pClose = '\0';
	snprintf(sTag, sizeof(sTag), "%s", sWork + 14);
	__xlBindingTrim(sTag);
	pAfter = pClose + 1;
	__xlBindingTrim(pAfter);
	if ( pAfter[0] == '\0' ) {
		__xlBindingSkip(pBindingCtx, "missing typedef struct alias", sLine);
		return XL_RC_OK;
	}
	sName[0] = '\0';
	memset(arrValueAliases, 0, sizeof(arrValueAliases));
	memset(arrPtrAliases, 0, sizeof(arrPtrAliases));
	iValueAliasCount = 0;
	iPtrAliasCount = 0;
	snprintf(sAliases, sizeof(sAliases), "%s", pAfter);
	pTok = strtok_s(sAliases, ",", &pCtx);
	while ( pTok != NULL ) {
		char sAliasDecl[128];
		char* pAlias;
		int bPointerAlias;

		snprintf(sAliasDecl, sizeof(sAliasDecl), "%s", pTok);
		__xlBindingTrim(sAliasDecl);
		pAlias = sAliasDecl;
		bPointerAlias = 0;
		while ( *pAlias == '*' ) {
			bPointerAlias = 1;
			++pAlias;
			while ( (*pAlias == ' ') || (*pAlias == '\t') ) {
				++pAlias;
			}
		}
		__xlBindingTrim(pAlias);
		if ( !__xlBindingIsIdentText(pAlias) ) {
			__xlBindingSkip(pBindingCtx, "invalid typedef struct alias", sLine);
			return XL_RC_OK;
		}
		if ( bPointerAlias ) {
			if ( iPtrAliasCount < XL_COUNTOF(arrPtrAliases) ) {
				snprintf(arrPtrAliases[iPtrAliasCount], sizeof(arrPtrAliases[iPtrAliasCount]), "%s", pAlias);
				iPtrAliasCount += 1;
			}
		} else if ( sName[0] == '\0' ) {
			snprintf(sName, sizeof(sName), "%s", pAlias);
		} else if ( iValueAliasCount < XL_COUNTOF(arrValueAliases) ) {
			snprintf(arrValueAliases[iValueAliasCount], sizeof(arrValueAliases[iValueAliasCount]), "%s", pAlias);
			iValueAliasCount += 1;
		}
		pTok = strtok_s(NULL, ",", &pCtx);
	}
	if ( sName[0] == '\0' ) {
		__xlBindingSkip(pBindingCtx, "missing typedef struct value alias", sLine);
		return XL_RC_OK;
	}
	memset(&fieldBuf, 0, sizeof(fieldBuf));
	bFieldsOk = 1;
	snprintf(sFields, sizeof(sFields), "%s", pOpen + 1);
	pTok = strtok_s(sFields, ";", &pCtx);
	while ( pTok != NULL ) {
		char sField[256];

		snprintf(sField, sizeof(sField), "%s", pTok);
		__xlBindingTrim(sField);
		if ( sField[0] != '\0' ) {
			if ( !__xlBindingEmitStructFieldDecls(sField, &fieldBuf, pBindingCtx, "unsupported typedef struct field") ) {
				bFieldsOk = 0;
				break;
			}
		}
		pTok = strtok_s(NULL, ";", &pCtx);
	}
	if ( bFieldsOk ) {
		__xlModuleBufAppendFormat(pOut, "class %s extends object\n", sName);
		__xlModuleBufAppend(pOut, (fieldBuf.sData != NULL) ? fieldBuf.sData : "");
		__xlModuleBufAppend(pOut, "end\n\n");
		if ( __xlBindingAddType(pBindingCtx, sName, sName, 1) != XL_RC_OK ) {
			free(fieldBuf.sData);
			return XL_RC_IO_ERROR;
		}
	} else {
		__xlModuleBufAppendFormat(pOut, "typedef %s = point\n", sName);
		(void)__xlBindingAddType(pBindingCtx, sName, "point", 0);
		__xlBindingTrace(pBindingCtx, "struct alias='%s' type='point' reason='unsupported field layout'", sName);
	}
	for ( iValueAliasCount = 0; iValueAliasCount < XL_COUNTOF(arrValueAliases); ++iValueAliasCount ) {
		if ( arrValueAliases[iValueAliasCount][0] == '\0' ) {
			break;
		}
		__xlModuleBufAppendFormat(pOut, "typedef %s = %s\n", arrValueAliases[iValueAliasCount], bFieldsOk ? sName : "point");
		(void)__xlBindingAddType(pBindingCtx, arrValueAliases[iValueAliasCount], bFieldsOk ? sName : "point", bFieldsOk ? 1 : 0);
		__xlBindingTrace(pBindingCtx, "typedef struct_value_alias='%s' type='%s'", arrValueAliases[iValueAliasCount], bFieldsOk ? sName : "point");
	}
	if ( sTag[0] != '\0' ) {
		char sStructName[128];

		(void)__xlBindingAddType(pBindingCtx, sTag, bFieldsOk ? sName : "point", bFieldsOk ? 1 : 0);
		snprintf(sStructName, sizeof(sStructName), "struct %s", sTag);
		(void)__xlBindingAddType(pBindingCtx, sStructName, bFieldsOk ? sName : "point", bFieldsOk ? 1 : 0);
		if ( bFieldsOk ) {
			__xlBindingResolveOpaqueAliases(pBindingCtx, pOut, "struct", sTag, sName);
		}
	}
	for ( iPtrAliasCount = 0; iPtrAliasCount < XL_COUNTOF(arrPtrAliases); ++iPtrAliasCount ) {
		if ( arrPtrAliases[iPtrAliasCount][0] == '\0' ) {
			break;
		}
		__xlModuleBufAppendFormat(pOut, "typedef %s = point\n", arrPtrAliases[iPtrAliasCount]);
		(void)__xlBindingAddType(pBindingCtx, arrPtrAliases[iPtrAliasCount], "point", 0);
		__xlBindingTrace(pBindingCtx, "typedef struct_pointer_alias='%s' type='point'", arrPtrAliases[iPtrAliasCount]);
	}
	if ( pBindingCtx != NULL ) pBindingCtx->iTypeDeclCount += 1;
	if ( bFieldsOk ) {
		__xlBindingTrace(pBindingCtx, "struct alias='%s'", sName);
	}
	free(fieldBuf.sData);
	return XL_RC_OK;
}

static int __xlBindingEmitTypedefUnionOpaque(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	char sWork[1024];
	char sTag[96];
	char sAliases[256];
	char sName[96];
	char arrValueAliases[8][96];
	char arrPtrAliases[8][96];
	char* pOpen;
	char* pClose;
	char* pAfter;
	char* pTok;
	char* pCtx;
	size_t i;
	int bEmitted;

	if ( (sLine == NULL) || (pOut == NULL) || (pBindingCtx == NULL) || (strncmp(sLine, "typedef union", 13) != 0) ) {
		return XL_RC_OK;
	}
	snprintf(sWork, sizeof(sWork), "%s", sLine);
	pOpen = strchr(sWork, '{');
	pClose = strrchr(sWork, '}');
	if ( (pOpen == NULL) || (pClose == NULL) || (pClose < pOpen) ) {
		__xlBindingSkip(pBindingCtx, "invalid typedef union", sLine);
		return XL_RC_OK;
	}
	*pOpen = '\0';
	*pClose = '\0';
	snprintf(sTag, sizeof(sTag), "%s", sWork + 13);
	__xlBindingTrim(sTag);
	pAfter = pClose + 1;
	__xlBindingTrim(pAfter);
	if ( pAfter[0] == '\0' ) {
		__xlBindingSkip(pBindingCtx, "missing typedef union alias", sLine);
		return XL_RC_OK;
	}
	sName[0] = '\0';
	memset(arrValueAliases, 0, sizeof(arrValueAliases));
	memset(arrPtrAliases, 0, sizeof(arrPtrAliases));
	snprintf(sAliases, sizeof(sAliases), "%s", pAfter);
	pTok = strtok_s(sAliases, ",", &pCtx);
	while ( pTok != NULL ) {
		char sAliasDecl[128];
		char* pAlias;
		int bPointerAlias;

		snprintf(sAliasDecl, sizeof(sAliasDecl), "%s", pTok);
		__xlBindingTrim(sAliasDecl);
		pAlias = sAliasDecl;
		bPointerAlias = 0;
		while ( *pAlias == '*' ) {
			bPointerAlias = 1;
			++pAlias;
			while ( (*pAlias == ' ') || (*pAlias == '\t') ) {
				++pAlias;
			}
		}
		__xlBindingTrim(pAlias);
		if ( !__xlBindingIsIdentText(pAlias) ) {
			__xlBindingSkip(pBindingCtx, "invalid typedef union alias", sLine);
			return XL_RC_OK;
		}
		if ( bPointerAlias ) {
			for ( i = 0; i < XL_COUNTOF(arrPtrAliases); ++i ) {
				if ( arrPtrAliases[i][0] == '\0' ) {
					snprintf(arrPtrAliases[i], sizeof(arrPtrAliases[i]), "%s", pAlias);
					break;
				}
			}
		} else if ( sName[0] == '\0' ) {
			snprintf(sName, sizeof(sName), "%s", pAlias);
		} else {
			for ( i = 0; i < XL_COUNTOF(arrValueAliases); ++i ) {
				if ( arrValueAliases[i][0] == '\0' ) {
					snprintf(arrValueAliases[i], sizeof(arrValueAliases[i]), "%s", pAlias);
					break;
				}
			}
		}
		pTok = strtok_s(NULL, ",", &pCtx);
	}
	bEmitted = 0;
	if ( sName[0] != '\0' ) {
		__xlModuleBufAppendFormat(pOut, "typedef %s = point\n", sName);
		(void)__xlBindingAddType(pBindingCtx, sName, "point", 0);
		__xlBindingTrace(pBindingCtx, "typedef union_opaque='%s' type='point'", sName);
		bEmitted = 1;
	}
	for ( i = 0; i < XL_COUNTOF(arrValueAliases); ++i ) {
		if ( arrValueAliases[i][0] == '\0' ) {
			break;
		}
		__xlModuleBufAppendFormat(pOut, "typedef %s = point\n", arrValueAliases[i]);
		(void)__xlBindingAddType(pBindingCtx, arrValueAliases[i], "point", 0);
		__xlBindingTrace(pBindingCtx, "typedef union_value_alias='%s' type='point'", arrValueAliases[i]);
		bEmitted = 1;
	}
	for ( i = 0; i < XL_COUNTOF(arrPtrAliases); ++i ) {
		if ( arrPtrAliases[i][0] == '\0' ) {
			break;
		}
		__xlModuleBufAppendFormat(pOut, "typedef %s = point\n", arrPtrAliases[i]);
		(void)__xlBindingAddType(pBindingCtx, arrPtrAliases[i], "point", 0);
		__xlBindingTrace(pBindingCtx, "typedef union_pointer_alias='%s' type='point'", arrPtrAliases[i]);
		bEmitted = 1;
	}
	if ( sTag[0] != '\0' ) {
		char sUnionName[128];

		(void)__xlBindingAddType(pBindingCtx, sTag, "point", 0);
		snprintf(sUnionName, sizeof(sUnionName), "union %s", sTag);
		(void)__xlBindingAddType(pBindingCtx, sUnionName, "point", 0);
	}
	if ( bEmitted ) {
		pBindingCtx->iTypeDeclCount += 1;
	} else {
		__xlBindingSkip(pBindingCtx, "missing typedef union usable alias", sLine);
	}
	return XL_RC_OK;
}

static int __xlBindingEmitUnionOpaqueDecl(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	char sWork[1024];
	char sName[96];
	char sUnionName[128];
	char* pOpen;
	char* pClose;

	if ( (sLine == NULL) || (pOut == NULL) || (pBindingCtx == NULL) || (strncmp(sLine, "union ", 6) != 0) ) {
		return XL_RC_OK;
	}
	snprintf(sWork, sizeof(sWork), "%s", sLine);
	pOpen = strchr(sWork, '{');
	pClose = strrchr(sWork, '}');
	if ( (pOpen == NULL) || (pClose == NULL) || (pClose < pOpen) ) {
		__xlBindingSkip(pBindingCtx, "invalid union declaration", sLine);
		return XL_RC_OK;
	}
	*pOpen = '\0';
	snprintf(sName, sizeof(sName), "%s", sWork + 6);
	__xlBindingTrim(sName);
	if ( !__xlBindingIsIdentText(sName) ) {
		__xlBindingSkip(pBindingCtx, "invalid union name", sLine);
		return XL_RC_OK;
	}
	__xlModuleBufAppendFormat(pOut, "typedef %s = point\n", sName);
	(void)__xlBindingAddType(pBindingCtx, sName, "point", 0);
	snprintf(sUnionName, sizeof(sUnionName), "union %s", sName);
	(void)__xlBindingAddType(pBindingCtx, sUnionName, "point", 0);
	pBindingCtx->iTypeDeclCount += 1;
	__xlBindingTrace(pBindingCtx, "union_opaque name='%s' type='point'", sName);
	return XL_RC_OK;
}

static int __xlBindingEmitStructDecl(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	char sWork[1024];
	char sFields[768];
	char sName[96];
	char sStructName[128];
	XL_ModuleTextBuffer fieldBuf;
	char* pOpen;
	char* pClose;
	char* pTok;
	char* pCtx;
	int bFieldsOk;

	if ( (sLine == NULL) || (pOut == NULL) || (pBindingCtx == NULL) || (strncmp(sLine, "struct ", 7) != 0) ) {
		return XL_RC_OK;
	}
	snprintf(sWork, sizeof(sWork), "%s", sLine);
	pOpen = strchr(sWork, '{');
	pClose = strrchr(sWork, '}');
	if ( (pOpen == NULL) || (pClose == NULL) || (pClose < pOpen) ) {
		__xlBindingSkip(pBindingCtx, "invalid struct declaration", sLine);
		return XL_RC_OK;
	}
	*pOpen = '\0';
	*pClose = '\0';
	snprintf(sName, sizeof(sName), "%s", sWork + 7);
	__xlBindingTrim(sName);
	if ( !__xlBindingIsIdentText(sName) ) {
		__xlBindingSkip(pBindingCtx, "invalid struct name", sLine);
		return XL_RC_OK;
	}
	memset(&fieldBuf, 0, sizeof(fieldBuf));
	bFieldsOk = 1;
	snprintf(sFields, sizeof(sFields), "%s", pOpen + 1);
	pTok = strtok_s(sFields, ";", &pCtx);
	while ( pTok != NULL ) {
		char sField[256];

		snprintf(sField, sizeof(sField), "%s", pTok);
		__xlBindingTrim(sField);
		if ( sField[0] != '\0' ) {
			if ( !__xlBindingEmitStructFieldDecls(sField, &fieldBuf, pBindingCtx, "unsupported struct field") ) {
				bFieldsOk = 0;
				break;
			}
		}
		pTok = strtok_s(NULL, ";", &pCtx);
	}
	snprintf(sStructName, sizeof(sStructName), "struct %s", sName);
	if ( bFieldsOk ) {
		__xlModuleBufAppendFormat(pOut, "class %s extends object\n", sName);
		__xlModuleBufAppend(pOut, (fieldBuf.sData != NULL) ? fieldBuf.sData : "");
		__xlModuleBufAppend(pOut, "end\n\n");
		if ( __xlBindingAddType(pBindingCtx, sName, sName, 1) != XL_RC_OK ) {
			free(fieldBuf.sData);
			return XL_RC_IO_ERROR;
		}
		(void)__xlBindingAddType(pBindingCtx, sStructName, sName, 1);
		__xlBindingResolveOpaqueAliases(pBindingCtx, pOut, "struct", sName, sName);
	} else {
		__xlModuleBufAppendFormat(pOut, "typedef %s = point\n", sName);
		(void)__xlBindingAddType(pBindingCtx, sName, "point", 0);
		(void)__xlBindingAddType(pBindingCtx, sStructName, "point", 0);
		__xlBindingTrace(pBindingCtx, "struct name='%s' type='point' reason='unsupported field layout'", sName);
	}
	if ( pBindingCtx != NULL ) pBindingCtx->iTypeDeclCount += 1;
	if ( bFieldsOk ) {
		__xlBindingTrace(pBindingCtx, "struct name='%s'", sName);
	}
	free(fieldBuf.sData);
	return XL_RC_OK;
}

static int __xlBindingEmitForwardDecl(const char* sLine, XL_BindingContext* pBindingCtx)
{
	char sName[128];
	char sTaggedName[160];
	const char* pName;
	const char* sKind;
	const char* sXlType;

	if ( (sLine == NULL) || (pBindingCtx == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( strncmp(sLine, "struct ", 7) == 0 ) {
		pName = sLine + 7;
		sKind = "struct";
		sXlType = "point";
	} else if ( strncmp(sLine, "union ", 6) == 0 ) {
		pName = sLine + 6;
		sKind = "union";
		sXlType = "point";
	} else if ( strncmp(sLine, "enum ", 5) == 0 ) {
		pName = sLine + 5;
		sKind = "enum";
		sXlType = "int";
	} else {
		return XL_RC_OK;
	}
	snprintf(sName, sizeof(sName), "%s", pName);
	__xlBindingTrim(sName);
	if ( !__xlBindingIsIdentText(sName) ) {
		__xlBindingSkip(pBindingCtx, "invalid forward declaration", sLine);
		return XL_RC_OK;
	}
	snprintf(sTaggedName, sizeof(sTaggedName), "%s %s", sKind, sName);
	(void)__xlBindingAddType(pBindingCtx, sName, sXlType, 0);
	(void)__xlBindingAddType(pBindingCtx, sTaggedName, sXlType, 0);
	pBindingCtx->iTypeDeclCount += 1;
	__xlBindingTrace(pBindingCtx, "forward %s='%s' type='%s'", sKind, sName, sXlType);
	return XL_RC_OK;
}

static int __xlBindingEmitEnum(const char* sLine, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	char sWork[1024];
	char sHead[256];
	char sItems[768];
	char sEnumName[96];
	char arrEnumAliases[8][96];
	char arrEnumPtrAliases[8][96];
	char* pOpen;
	char* pClose;
	char* pAfter;
	char* pTok;
	char* pCtx;
	long long iCurrent;
	size_t iAlias;
	size_t iPtrAlias;
	int bHasCurrent;
	int bTypedef;

	if ( (sLine == NULL) || (pOut == NULL) || (pBindingCtx == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	bTypedef = (strncmp(sLine, "typedef enum", 12) == 0);
	if ( !bTypedef && (strncmp(sLine, "enum ", 5) != 0) ) {
		return XL_RC_OK;
	}
	snprintf(sWork, sizeof(sWork), "%s", sLine);
	pOpen = strchr(sWork, '{');
	pClose = strrchr(sWork, '}');
	if ( (pOpen == NULL) || (pClose == NULL) || (pClose < pOpen) ) {
		__xlBindingSkip(pBindingCtx, "invalid enum declaration", sLine);
		return XL_RC_OK;
	}
	*pOpen = '\0';
	*pClose = '\0';
	snprintf(sHead, sizeof(sHead), "%s", sWork + (bTypedef ? 12 : 5));
	__xlBindingTrim(sHead);
	pAfter = pClose + 1;
	__xlBindingTrim(pAfter);
	sEnumName[0] = '\0';
	memset(arrEnumAliases, 0, sizeof(arrEnumAliases));
	memset(arrEnumPtrAliases, 0, sizeof(arrEnumPtrAliases));
	iAlias = 0;
	iPtrAlias = 0;
	if ( bTypedef ) {
		if ( pAfter[0] != '\0' ) {
			char sAliasList[256];
			char* pAliasTok;
			char* pAliasCtx;

			snprintf(sAliasList, sizeof(sAliasList), "%s", pAfter);
			pAliasTok = strtok_s(sAliasList, ",", &pAliasCtx);
			while ( pAliasTok != NULL ) {
				char sAliasDecl[128];
				char sAliasName[96];
				char* pAlias;
				int bPointerAlias;

				snprintf(sAliasDecl, sizeof(sAliasDecl), "%s", pAliasTok);
				__xlBindingTrim(sAliasDecl);
				pAlias = sAliasDecl;
				bPointerAlias = 0;
				while ( *pAlias == '*' ) {
					bPointerAlias = 1;
					++pAlias;
					while ( (*pAlias == ' ') || (*pAlias == '\t') ) {
						++pAlias;
					}
				}
				__xlBindingTrim(pAlias);
				if ( !__xlBindingSplitTrailingName(pAlias, sAliasName, sizeof(sAliasName)) ) {
					snprintf(sAliasName, sizeof(sAliasName), "%s", pAlias);
					__xlBindingTrim(sAliasName);
				}
				if ( __xlBindingIsIdentText(sAliasName) ) {
					if ( bPointerAlias ) {
						if ( iPtrAlias < XL_COUNTOF(arrEnumPtrAliases) ) {
							snprintf(arrEnumPtrAliases[iPtrAlias], sizeof(arrEnumPtrAliases[iPtrAlias]), "%s", sAliasName);
							iPtrAlias += 1;
						}
					} else if ( sEnumName[0] == '\0' ) {
						snprintf(sEnumName, sizeof(sEnumName), "%s", sAliasName);
					} else if ( iAlias < XL_COUNTOF(arrEnumAliases) ) {
						snprintf(arrEnumAliases[iAlias], sizeof(arrEnumAliases[iAlias]), "%s", sAliasName);
						iAlias += 1;
					}
				}
				pAliasTok = strtok_s(NULL, ",", &pAliasCtx);
			}
		}
	} else if ( sHead[0] != '\0' ) {
		snprintf(sEnumName, sizeof(sEnumName), "%s", sHead);
	}
	if ( sEnumName[0] != '\0' ) {
		char sEnumStructName[128];

		(void)__xlBindingAddType(pBindingCtx, sEnumName, "int", 0);
		if ( sHead[0] != '\0' ) {
			(void)__xlBindingAddType(pBindingCtx, sHead, "int", 0);
			snprintf(sEnumStructName, sizeof(sEnumStructName), "enum %s", sHead);
			(void)__xlBindingAddType(pBindingCtx, sEnumStructName, "int", 0);
		}
		if ( pBindingCtx != NULL ) pBindingCtx->iTypeDeclCount += 1;
		__xlBindingTrace(pBindingCtx, "enum type='%s'", sEnumName);
	}
	for ( iAlias = 0; iAlias < XL_COUNTOF(arrEnumAliases); ++iAlias ) {
		if ( arrEnumAliases[iAlias][0] == '\0' ) {
			break;
		}
		(void)__xlBindingAddType(pBindingCtx, arrEnumAliases[iAlias], "int", 0);
		__xlBindingTrace(pBindingCtx, "typedef enum_alias='%s' type='int'", arrEnumAliases[iAlias]);
	}
	for ( iPtrAlias = 0; iPtrAlias < XL_COUNTOF(arrEnumPtrAliases); ++iPtrAlias ) {
		if ( arrEnumPtrAliases[iPtrAlias][0] == '\0' ) {
			break;
		}
		(void)__xlBindingAddType(pBindingCtx, arrEnumPtrAliases[iPtrAlias], "point", 0);
		__xlBindingTrace(pBindingCtx, "typedef enum_pointer_alias='%s' type='point'", arrEnumPtrAliases[iPtrAlias]);
	}

	snprintf(sItems, sizeof(sItems), "%s", pOpen + 1);
	iCurrent = -1;
	bHasCurrent = 0;
	pTok = strtok_s(sItems, ",", &pCtx);
	while ( pTok != NULL ) {
		char sItem[256];
		char sName[128];
		char sValue[160];
		char sNumber[160];
		char* pEq;
		int bFloat;
		int bUnsigned;

		snprintf(sItem, sizeof(sItem), "%s", pTok);
		__xlBindingTrim(sItem);
		if ( sItem[0] != '\0' ) {
			pEq = strchr(sItem, '=');
			if ( pEq != NULL ) {
				*pEq = '\0';
				snprintf(sName, sizeof(sName), "%s", sItem);
				snprintf(sValue, sizeof(sValue), "%s", pEq + 1);
				__xlBindingTrim(sName);
				__xlBindingTrim(sValue);
				if ( __xlBindingIsIdentText(sName) &&
					__xlBindingNormalizeNumber(sValue, sNumber, sizeof(sNumber), &bFloat, &bUnsigned) &&
					!bFloat ) {
					iCurrent = strtoll(sNumber, NULL, 10);
					bHasCurrent = 1;
					__xlModuleBufAppendFormat(pOut, "const int %s = %s\n", sName, sNumber);
					(void)__xlBindingAddConst(pBindingCtx, sName, iCurrent);
					pBindingCtx->iEnumCount += 1;
					__xlBindingTrace(pBindingCtx, "enum value='%s' int='%s'", sName, sNumber);
				} else if ( __xlBindingIsIdentText(sName) &&
					__xlBindingEvalIntConstExpr(pBindingCtx, sValue, &iCurrent) ) {
					bHasCurrent = 1;
					__xlModuleBufAppendFormat(pOut, "const int %s = %lld\n", sName, iCurrent);
					(void)__xlBindingAddConst(pBindingCtx, sName, iCurrent);
					pBindingCtx->iEnumCount += 1;
					__xlBindingTrace(pBindingCtx, "enum value='%s' int='%lld'", sName, iCurrent);
				} else {
					__xlBindingSkip(pBindingCtx, "unsupported enum value", pTok);
				}
			} else {
				snprintf(sName, sizeof(sName), "%s", sItem);
				__xlBindingTrim(sName);
				if ( __xlBindingIsIdentText(sName) ) {
					iCurrent = bHasCurrent ? (iCurrent + 1) : 0;
					bHasCurrent = 1;
					__xlModuleBufAppendFormat(pOut, "const int %s = %lld\n", sName, iCurrent);
					(void)__xlBindingAddConst(pBindingCtx, sName, iCurrent);
					pBindingCtx->iEnumCount += 1;
					__xlBindingTrace(pBindingCtx, "enum value='%s' int='%lld'", sName, iCurrent);
				} else {
					__xlBindingSkip(pBindingCtx, "invalid enum value name", pTok);
				}
			}
		}
		pTok = strtok_s(NULL, ",", &pCtx);
	}
	return XL_RC_OK;
}

static int __xlBindingProcessStatement(char* sStmt, XL_ModuleTextBuffer* pOut, XL_BindingContext* pBindingCtx)
{
	size_t iLen;
	int iRc;

	if ( (sStmt == NULL) || (pOut == NULL) || (pBindingCtx == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	__xlBindingTrim(sStmt);
	if ( strncmp(sStmt, "extern \"C\"", 10) == 0 ) {
		memmove(sStmt, sStmt + 10, strlen(sStmt + 10) + 1);
		__xlBindingTrim(sStmt);
	}
	__xlBindingRemoveDeclMacros(pBindingCtx, sStmt);
	iLen = strlen(sStmt);
	if ( (iLen == 0) || (sStmt[0] == '#') ) {
		return XL_RC_OK;
	}
	if ( sStmt[iLen - 1] == ';' ) {
		sStmt[iLen - 1] = '\0';
		__xlBindingTrim(sStmt);
	}
	if ( sStmt[0] == '\0' ) {
		return XL_RC_OK;
	}
	if ( strncmp(sStmt, "typedef ", 8) == 0 ) {
		if ( strncmp(sStmt, "typedef enum", 12) == 0 ) {
			iRc = __xlBindingEmitEnum(sStmt, pOut, pBindingCtx);
		} else if ( strncmp(sStmt, "typedef union", 13) == 0 && (strstr(sStmt, "{") != NULL) ) {
			iRc = __xlBindingEmitTypedefUnionOpaque(sStmt, pOut, pBindingCtx);
		} else if ( strstr(sStmt, "{") != NULL ) {
			iRc = __xlBindingEmitTypedefStruct(sStmt, pOut, pBindingCtx);
		} else {
			iRc = __xlBindingEmitTypedefAlias(sStmt, pBindingCtx);
		}
		return iRc;
	}
	if ( (strncmp(sStmt, "enum ", 5) == 0) && (strstr(sStmt, "{") != NULL) ) {
		return __xlBindingEmitEnum(sStmt, pOut, pBindingCtx);
	}
	if ( (strncmp(sStmt, "struct ", 7) == 0) && (strstr(sStmt, "{") != NULL) ) {
		return __xlBindingEmitStructDecl(sStmt, pOut, pBindingCtx);
	}
	if ( (strncmp(sStmt, "union ", 6) == 0) && (strstr(sStmt, "{") != NULL) ) {
		return __xlBindingEmitUnionOpaqueDecl(sStmt, pOut, pBindingCtx);
	}
	if ( strchr(sStmt, '(') != NULL ) {
		return __xlBindingEmitFunction(sStmt, pOut, pBindingCtx);
	}
	if ( (strncmp(sStmt, "struct ", 7) == 0) || (strncmp(sStmt, "union ", 6) == 0) || (strncmp(sStmt, "enum ", 5) == 0) ) {
		return __xlBindingEmitForwardDecl(sStmt, pBindingCtx);
	}
	if ( strncmp(sStmt, "extern ", 7) == 0 ) {
		return __xlBindingEmitVar(sStmt, pOut, pBindingCtx);
	}
	__xlBindingSkip(pBindingCtx, "unsupported declaration", sStmt);
	return XL_RC_OK;
}

static int __xlResolveBindingHeaderPath(
	const char* sModulePath,
	const XL_ModuleInfo* pModule,
	const char* sValue,
	char* sOut,
	size_t iOutSize
);
static int __xlBindingHeaderWasIncluded(XL_BindingContext* pBindingCtx, const char* sHeaderPath);
static int __xlBindingMarkHeaderIncluded(XL_BindingContext* pBindingCtx, const char* sHeaderPath);
static int __xlBindingParseIncludeDirective(const char* sLine, char* sOut, size_t iOutSize);

static int __xlBindingCondActive(const XL_BindingCondFrame* arrCond, size_t iCondDepth)
{
	size_t i;

	for ( i = 0; i < iCondDepth; ++i ) {
		if ( !arrCond[i].bBranchActive ) {
			return 0;
		}
	}
	return 1;
}

static int __xlBindingParseDirectiveIdent(const char* p, char* sOut, size_t iOutSize)
{
	size_t iLen;

	if ( (p == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return 0;
	}
	while ( (*p == ' ') || (*p == '\t') ) {
		++p;
	}
	if ( !__xlIsLinkIdentStart((unsigned char)*p) ) {
		return 0;
	}
	iLen = 0;
	while ( __xlIsLinkIdentCont((unsigned char)p[iLen]) ) {
		++iLen;
	}
	if ( (iLen == 0) || (iLen >= iOutSize) ) {
		return 0;
	}
	memcpy(sOut, p, iLen);
	sOut[iLen] = '\0';
	return 1;
}

static void __xlBindingRewriteDefinedOps(XL_BindingContext* pBindingCtx, char* sExpr)
{
	char sOut[512];
	size_t iRead;
	size_t iWrite;

	if ( sExpr == NULL ) {
		return;
	}
	iRead = 0;
	iWrite = 0;
	while ( (sExpr[iRead] != '\0') && ((iWrite + 1) < sizeof(sOut)) ) {
		if ( (strncmp(sExpr + iRead, "defined", 7) == 0) &&
			((iRead == 0) || !__xlIsLinkIdentCont((unsigned char)sExpr[iRead - 1])) &&
			!__xlIsLinkIdentCont((unsigned char)sExpr[iRead + 7]) ) {
			char sName[128];
			const char* p = sExpr + iRead + 7;
			int bHasParen;
			int bDefined;

			while ( (*p == ' ') || (*p == '\t') ) {
				++p;
			}
			bHasParen = (*p == '(');
			if ( bHasParen ) {
				++p;
			}
			if ( __xlBindingParseDirectiveIdent(p, sName, sizeof(sName)) ) {
				while ( __xlIsLinkIdentCont((unsigned char)*p) ) {
					++p;
				}
				while ( (*p == ' ') || (*p == '\t') ) {
					++p;
				}
				if ( !bHasParen || (*p == ')') ) {
					if ( bHasParen ) {
						++p;
					}
					bDefined = __xlBindingFindMacro(pBindingCtx, sName) ? 1 : 0;
					sOut[iWrite++] = bDefined ? '1' : '0';
					iRead = (size_t)(p - sExpr);
					continue;
				}
			}
		}
		sOut[iWrite++] = sExpr[iRead++];
	}
	sOut[iWrite] = '\0';
	snprintf(sExpr, 512, "%s", sOut);
}

static int __xlBindingEvalIfExpr(XL_BindingContext* pBindingCtx, const char* sExpr)
{
	char sWork[512];
	char sName[128];
	const char* p;
	long long iValue;
	int bNegate;

	if ( sExpr == NULL ) {
		return 0;
	}
	snprintf(sWork, sizeof(sWork), "%s", sExpr);
	__xlBindingTrim(sWork);
	__xlBindingRewriteDefinedOps(pBindingCtx, sWork);
	__xlBindingTrim(sWork);
	if ( sWork[0] == '\0' ) {
		return 0;
	}
	p = sWork;
	bNegate = 0;
	while ( (*p == '!') || (*p == ' ') || (*p == '\t') ) {
		if ( *p == '!' ) {
			bNegate = !bNegate;
		}
		++p;
	}
	if ( strncmp(p, "defined", 7) == 0 ) {
		p += 7;
		while ( (*p == ' ') || (*p == '\t') ) {
			++p;
		}
		if ( *p == '(' ) {
			++p;
			if ( !__xlBindingParseDirectiveIdent(p, sName, sizeof(sName)) ) {
				return 0;
			}
		} else if ( !__xlBindingParseDirectiveIdent(p, sName, sizeof(sName)) ) {
			return 0;
		}
		iValue = __xlBindingFindMacro(pBindingCtx, sName) ? 1 : 0;
		return bNegate ? !iValue : !!iValue;
	}
	if ( __xlBindingEvalIntConstExprEx(pBindingCtx, p, &iValue, 1) ) {
		return bNegate ? (iValue == 0) : (iValue != 0);
	}
	if ( __xlBindingParseDirectiveIdent(p, sName, sizeof(sName)) ) {
		iValue = __xlBindingFindMacro(pBindingCtx, sName) ? 1 : 0;
		return bNegate ? !iValue : !!iValue;
	}
	return 0;
}

static int __xlBindingHandleConditionalDirective(
	const char* sLine,
	XL_BindingContext* pBindingCtx,
	XL_BindingCondFrame* arrCond,
	size_t* piCondDepth
)
{
	char sName[128];
	const char* pExpr;
	int bCond;
	int bParentActive;
	XL_BindingCondFrame* pTop;

	if ( (sLine == NULL) || (arrCond == NULL) || (piCondDepth == NULL) || (sLine[0] != '#') ) {
		return 0;
	}
	if ( strncmp(sLine, "#ifdef", 6) == 0 ) {
		bCond = __xlBindingParseDirectiveIdent(sLine + 6, sName, sizeof(sName)) &&
			__xlBindingFindMacro(pBindingCtx, sName);
	} else if ( strncmp(sLine, "#ifndef", 7) == 0 ) {
		bCond = !(__xlBindingParseDirectiveIdent(sLine + 7, sName, sizeof(sName)) &&
			__xlBindingFindMacro(pBindingCtx, sName));
	} else if ( strncmp(sLine, "#if", 3) == 0 && ((sLine[3] == ' ') || (sLine[3] == '\t') || (sLine[3] == '\0')) ) {
		pExpr = sLine + 3;
		bCond = __xlBindingEvalIfExpr(pBindingCtx, pExpr);
	} else if ( strncmp(sLine, "#elif", 5) == 0 ) {
		if ( *piCondDepth == 0 ) {
			__xlBindingSkip(pBindingCtx, "unmatched #elif", sLine);
			return 1;
		}
		pTop = &arrCond[*piCondDepth - 1];
		if ( pTop->bElseSeen ) {
			__xlBindingSkip(pBindingCtx, "#elif after #else", sLine);
			pTop->bBranchActive = 0;
			return 1;
		}
		bCond = __xlBindingEvalIfExpr(pBindingCtx, sLine + 5);
		pTop->bBranchActive = pTop->bParentActive && !pTop->bAnyTaken && bCond;
		if ( bCond ) {
			pTop->bAnyTaken = 1;
		}
		return 1;
	} else if ( strncmp(sLine, "#else", 5) == 0 ) {
		if ( *piCondDepth == 0 ) {
			__xlBindingSkip(pBindingCtx, "unmatched #else", sLine);
			return 1;
		}
		pTop = &arrCond[*piCondDepth - 1];
		pTop->bBranchActive = pTop->bParentActive && !pTop->bAnyTaken;
		pTop->bAnyTaken = 1;
		pTop->bElseSeen = 1;
		return 1;
	} else if ( strncmp(sLine, "#endif", 6) == 0 ) {
		if ( *piCondDepth == 0 ) {
			__xlBindingSkip(pBindingCtx, "unmatched #endif", sLine);
			return 1;
		}
		*piCondDepth -= 1;
		return 1;
	} else {
		return 0;
	}
	if ( *piCondDepth >= XL_BINDING_COND_MAX ) {
		__xlBindingSkip(pBindingCtx, "conditional depth limit reached", sLine);
		return 1;
	}
	bParentActive = __xlBindingCondActive(arrCond, *piCondDepth);
	arrCond[*piCondDepth].bParentActive = bParentActive;
	arrCond[*piCondDepth].bBranchActive = bParentActive && bCond;
	arrCond[*piCondDepth].bAnyTaken = bCond;
	arrCond[*piCondDepth].bElseSeen = 0;
	*piCondDepth += 1;
	return 1;
}

static int __xlBindingEmitHeaderDeclsWithCtx(
	const char* sHeaderPath,
	const XL_ModuleInfo* pModule,
	XL_ModuleTextBuffer* pOut,
	XL_DiagContext* pDiag,
	XL_BindingContext* pBindingCtx,
	int iDepth
)
{
	char* sText;
	char* pLine;
	char* pCtx;
	XL_ModuleTextBuffer stmt;
	XL_BindingCondFrame arrCond[XL_BINDING_COND_MAX];
	int iRc;
	int iBraceDepth;
	int bInBlockComment;
	size_t iCondDepth;

	if ( (sHeaderPath == NULL) || (pOut == NULL) || (pBindingCtx == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( iDepth > 16 ) {
		__xlBindingSkip(pBindingCtx, "include depth limit reached", sHeaderPath);
		return XL_RC_OK;
	}
	if ( __xlBindingHeaderWasIncluded(pBindingCtx, sHeaderPath) ) {
		__xlBindingTrace(pBindingCtx, "include skip duplicate header='%s'", sHeaderPath);
		return XL_RC_OK;
	}
	(void)__xlBindingMarkHeaderIncluded(pBindingCtx, sHeaderPath);
	sText = NULL;
	iRc = __xlBindingReadFileText(sHeaderPath, &sText);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	memset(&stmt, 0, sizeof(stmt));
	pBindingCtx->pDiag = pDiag;
	pBindingCtx->sHeaderPath = sHeaderPath;
	iBraceDepth = 0;
	bInBlockComment = 0;
	iCondDepth = 0;
	__xlModuleBufAppendFormat(pOut, "// generated from #binding \"%s\"\n", sHeaderPath);
	__xlBindingTrace(pBindingCtx, "begin header='%s'", sHeaderPath);
	pLine = strtok_s(sText, "\n", &pCtx);
	while ( pLine != NULL ) {
		char sLine[1024];
		char sInclude[XL_PATH_MAX_LEN];
		char sIncludePath[XL_PATH_MAX_LEN];
		size_t iLen;
		size_t iScan;
		int bStatementComplete;

		snprintf(sLine, sizeof(sLine), "%s", pLine);
		__xlBindingStripCComments(sLine, &bInBlockComment);
		__xlBindingTrim(sLine);
		iLen = strlen(sLine);
		if ( iLen == 0 ) {
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && __xlBindingHandleConditionalDirective(sLine, pBindingCtx, arrCond, &iCondDepth) ) {
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && !__xlBindingCondActive(arrCond, iCondDepth) ) {
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && (strncmp(sLine, "#error", 6) == 0) &&
			((sLine[6] == '\0') || (sLine[6] == ' ') || (sLine[6] == '\t')) ) {
			const char* sMessage = sLine + 6;

			while ( (*sMessage == ' ') || (*sMessage == '\t') ) {
				++sMessage;
			}
			XL_DIAG_ERROR(pDiag, "binding", "#error in binding header '%s': %s", sHeaderPath, sMessage);
			free(stmt.sData);
			free(sText);
			return XL_RC_FAIL;
		}
		if ( (stmt.iLen == 0) && (strncmp(sLine, "#warning", 8) == 0) &&
			((sLine[8] == '\0') || (sLine[8] == ' ') || (sLine[8] == '\t')) ) {
			__xlBindingSkip(pBindingCtx, "warning directive", sLine);
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && (strncmp(sLine, "#pragma", 7) == 0) &&
			((sLine[7] == '\0') || (sLine[7] == ' ') || (sLine[7] == '\t')) ) {
			__xlBindingTrace(pBindingCtx, "pragma ignored='%s'", sLine);
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) &&
			((strcmp(sLine, "extern \"C\" {") == 0) || (strcmp(sLine, "extern \"C\"{") == 0) || (strcmp(sLine, "}") == 0)) ) {
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && (strncmp(sLine, "#define", 7) == 0) ) {
			while ( (iLen != 0) && (sLine[iLen - 1] == '\\') ) {
				char sNextLine[1024];

				sLine[iLen - 1] = '\0';
				__xlBindingTrim(sLine);
				pLine = strtok_s(NULL, "\n", &pCtx);
				if ( pLine == NULL ) {
					break;
				}
				snprintf(sNextLine, sizeof(sNextLine), "%s", pLine);
				__xlBindingStripCComments(sNextLine, &bInBlockComment);
				__xlBindingTrim(sNextLine);
				if ( (strlen(sLine) + strlen(sNextLine) + 2) >= sizeof(sLine) ) {
					__xlBindingSkip(pBindingCtx, "define continuation too long", sLine);
					break;
				}
				strcat(sLine, " ");
				strcat(sLine, sNextLine);
				iLen = strlen(sLine);
			}
		}
		if ( (stmt.iLen == 0) && __xlBindingParseIncludeDirective(sLine, sInclude, sizeof(sInclude)) ) {
			iRc = __xlResolveBindingHeaderPath(sHeaderPath, pModule, sInclude, sIncludePath, sizeof(sIncludePath));
			if ( iRc == XL_RC_OK ) {
				iRc = __xlBindingEmitHeaderDeclsWithCtx(sIncludePath, pModule, pOut, pDiag, pBindingCtx, iDepth + 1);
			} else {
				__xlBindingSkip(pBindingCtx, "include not found", sLine);
				iRc = XL_RC_OK;
			}
			if ( iRc != XL_RC_OK ) {
				free(stmt.sData);
				free(sText);
				return iRc;
			}
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && (strncmp(sLine, "#define", 7) == 0) ) {
			iRc = __xlBindingEmitDefine(sLine, pOut, pBindingCtx);
			if ( iRc != XL_RC_OK ) {
				free(stmt.sData);
				free(sText);
				return iRc;
			}
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && (strncmp(sLine, "#undef", 6) == 0) &&
			((sLine[6] == '\0') || (sLine[6] == ' ') || (sLine[6] == '\t')) ) {
			iRc = __xlBindingEmitUndef(sLine, pBindingCtx);
			if ( iRc != XL_RC_OK ) {
				free(stmt.sData);
				free(sText);
				return iRc;
			}
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( (stmt.iLen == 0) && (sLine[0] == '#') ) {
			pLine = strtok_s(NULL, "\n", &pCtx);
			continue;
		}
		if ( stmt.iLen != 0 ) {
			__xlModuleBufAppend(&stmt, " ");
		}
		__xlModuleBufAppend(&stmt, sLine);
		bStatementComplete = 0;
		for ( iScan = 0; sLine[iScan] != '\0'; ++iScan ) {
			if ( sLine[iScan] == '{' ) {
				iBraceDepth += 1;
			} else if ( sLine[iScan] == '}' ) {
				if ( iBraceDepth > 0 ) {
					iBraceDepth -= 1;
				}
			} else if ( (sLine[iScan] == ';') && (iBraceDepth == 0) ) {
				bStatementComplete = 1;
			}
		}
		if ( bStatementComplete ) {
			iRc = __xlBindingProcessStatement(stmt.sData, pOut, pBindingCtx);
			if ( iRc != XL_RC_OK ) {
				free(stmt.sData);
				free(sText);
				return iRc;
			}
			stmt.iLen = 0;
			if ( stmt.sData != NULL ) {
				stmt.sData[0] = '\0';
			}
		}
		pLine = strtok_s(NULL, "\n", &pCtx);
	}
	__xlModuleBufAppend(pOut, "\n");
	if ( stmt.iLen != 0 ) {
		__xlBindingSkip(pBindingCtx, "unterminated declaration", stmt.sData);
	}
	if ( iCondDepth != 0 ) {
		__xlBindingSkip(pBindingCtx, "unterminated conditional directive", sHeaderPath);
	}
	__xlBindingTrace(
		pBindingCtx,
		"summary header='%s' functions=%zu vars=%zu types=%zu defines=%zu enums=%zu skipped=%zu",
		sHeaderPath,
		pBindingCtx->iFunctionCount,
		pBindingCtx->iVarCount,
		pBindingCtx->iTypeDeclCount,
		pBindingCtx->iDefineCount,
		pBindingCtx->iEnumCount,
		pBindingCtx->iSkipCount
	);
	free(stmt.sData);
	free(sText);
	return XL_RC_OK;
}

static int __xlBindingEmitHeaderDecls(
	const char* sHeaderPath,
	const XL_ModuleInfo* pModule,
	XL_ModuleTextBuffer* pOut,
	XL_DiagContext* pDiag,
	int bTrace
)
{
	XL_BindingContext bindingCtx;

	memset(&bindingCtx, 0, sizeof(bindingCtx));
	bindingCtx.pDiag = pDiag;
	bindingCtx.sHeaderPath = sHeaderPath;
	bindingCtx.bTrace = bTrace || __xlEnvTruthy("XL_BINDING_TRACE");
	return __xlBindingEmitHeaderDeclsWithCtx(sHeaderPath, pModule, pOut, pDiag, &bindingCtx, 0);
}

static int __xlResolveBindingHeaderPath(
	const char* sModulePath,
	const XL_ModuleInfo* pModule,
	const char* sValue,
	char* sOut,
	size_t iOutSize
)
{
	char sCandidate[XL_PATH_MAX_LEN];
	size_t i;

	if ( (sValue == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return XL_RC_BAD_ARG;
	}
	if ( __xlDirectiveNeedsRelativePathResolution(sValue) ) {
		if ( __xlNormalizeDirectivePath(sModulePath, sValue, sOut, iOutSize) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
		return __xlBindingFileExists(sOut) ? XL_RC_OK : XL_RC_FAIL;
	}
	__xlNormalizeDirectivePath(sModulePath, sValue, sCandidate, sizeof(sCandidate));
	if ( __xlBindingFileExists(sCandidate) ) {
		snprintf(sOut, iOutSize, "%s", sCandidate);
		return XL_RC_OK;
	}
	for ( i = 0; (pModule != NULL) && (i < pModule->iIncPathCount); ++i ) {
		xlPathJoin(sCandidate, sizeof(sCandidate), pModule->arrIncPaths[i], sValue);
		__xlModuleNormalizePath(sOut, iOutSize, sCandidate);
		if ( __xlBindingFileExists(sOut) ) {
			return XL_RC_OK;
		}
	}
	return XL_RC_FAIL;
}

static int __xlBindingHeaderWasIncluded(XL_BindingContext* pBindingCtx, const char* sHeaderPath)
{
	size_t i;

	if ( (pBindingCtx == NULL) || (sHeaderPath == NULL) || (sHeaderPath[0] == '\0') ) {
		return 0;
	}
	for ( i = 0; i < pBindingCtx->iIncludedHeaderCount; ++i ) {
		if ( strcmp(pBindingCtx->arrIncludedHeaders[i], sHeaderPath) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static int __xlBindingMarkHeaderIncluded(XL_BindingContext* pBindingCtx, const char* sHeaderPath)
{
	if ( (pBindingCtx == NULL) || (sHeaderPath == NULL) || (sHeaderPath[0] == '\0') ) {
		return XL_RC_OK;
	}
	if ( __xlBindingHeaderWasIncluded(pBindingCtx, sHeaderPath) ) {
		return XL_RC_OK;
	}
	if ( pBindingCtx->iIncludedHeaderCount >= sizeof(pBindingCtx->arrIncludedHeaders) / sizeof(pBindingCtx->arrIncludedHeaders[0]) ) {
		__xlBindingSkip(pBindingCtx, "include limit reached", sHeaderPath);
		return XL_RC_OK;
	}
	snprintf(
		pBindingCtx->arrIncludedHeaders[pBindingCtx->iIncludedHeaderCount],
		sizeof(pBindingCtx->arrIncludedHeaders[pBindingCtx->iIncludedHeaderCount]),
		"%s",
		sHeaderPath
	);
	pBindingCtx->iIncludedHeaderCount += 1;
	return XL_RC_OK;
}

static int __xlBindingParseIncludeDirective(const char* sLine, char* sOut, size_t iOutSize)
{
	const char* p;
	const char* pStart;
	char chEnd;
	size_t iLen;

	if ( (sLine == NULL) || (sOut == NULL) || (iOutSize == 0) || (strncmp(sLine, "#include", 8) != 0) ) {
		return 0;
	}
	p = sLine + 8;
	if ( (*p != ' ') && (*p != '\t') ) {
		return 0;
	}
	while ( (*p == ' ') || (*p == '\t') ) {
		++p;
	}
	if ( *p == '"' ) {
		chEnd = '"';
		++p;
	} else if ( *p == '<' ) {
		chEnd = '>';
		++p;
	} else {
		return 0;
	}
	pStart = p;
	while ( (*p != '\0') && (*p != chEnd) ) {
		++p;
	}
	if ( *p != chEnd ) {
		return 0;
	}
	iLen = (size_t)(p - pStart);
	if ( (iLen == 0) || (iLen >= iOutSize) ) {
		return 0;
	}
	memcpy(sOut, pStart, iLen);
	sOut[iLen] = '\0';
	return 1;
}

static int __xlExtractDirectiveDecls(
	const char* sSourceText,
	const char* sModulePath,
	XL_ModuleInfo* pModule,
	XL_DiagContext* pDiag,
	int bTraceParser,
	char** psSanitizedText
)
{
	XL_ModuleTextBuffer out;
	size_t iPos;

	if ( (sSourceText == NULL) || (pModule == NULL) || (psSanitizedText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&out, 0, sizeof(out));
	iPos = 0;
	while ( sSourceText[iPos] != '\0' ) {
		size_t iLineStart;
		size_t iCursor;
		size_t iLineEnd;
		size_t iNewlineLen;
		size_t iLibStart;
		size_t iLibEnd;
		int bHandledDirective;

		iLineStart = iPos;
		iCursor = iPos;
		while ( __xlIsAsciiSpace((unsigned char)sSourceText[iCursor]) ) {
			++iCursor;
		}
		iLineEnd = iCursor;
		while ( (sSourceText[iLineEnd] != '\0') && (sSourceText[iLineEnd] != '\r') && (sSourceText[iLineEnd] != '\n') ) {
			++iLineEnd;
		}

		iNewlineLen = 0;
		if ( sSourceText[iLineEnd] == '\r' ) {
			iNewlineLen = 1;
			if ( sSourceText[iLineEnd + 1] == '\n' ) {
				iNewlineLen = 2;
			}
		} else if ( sSourceText[iLineEnd] == '\n' ) {
			iNewlineLen = 1;
		}

		bHandledDirective = 0;
		if ( (iCursor < iLineEnd) && (sSourceText[iCursor] == '#') ) {
			char sValue[XL_PATH_MAX_LEN];
			char sNormPath[XL_PATH_MAX_LEN];
			const char* sDirective;
			size_t iDirectiveStart;
			size_t iDirectiveLen;
			int iRc;

			iDirectiveStart = iCursor + 1;
			while ( (iDirectiveStart < iLineEnd) && __xlIsAsciiSpace((unsigned char)sSourceText[iDirectiveStart]) ) {
				++iDirectiveStart;
			}
			iDirectiveLen = 0;
			while ( ((iDirectiveStart + iDirectiveLen) < iLineEnd) &&
				__xlIsLinkIdentCont((unsigned char)sSourceText[iDirectiveStart + iDirectiveLen]) ) {
				++iDirectiveLen;
			}
			sDirective = sSourceText + iDirectiveStart;
			if ( (iDirectiveLen == 4) && (memcmp(sDirective, "link", 4) == 0) ) {
				iLibStart = iDirectiveStart + iDirectiveLen;
				while ( (iLibStart < iLineEnd) && __xlIsAsciiSpace((unsigned char)sSourceText[iLibStart]) ) {
					++iLibStart;
				}
				iLibEnd = iLibStart;
				if ( (iLibStart < iLineEnd) && __xlIsLinkIdentStart((unsigned char)sSourceText[iLibStart]) ) {
					while ( (iLibEnd < iLineEnd) && __xlIsLinkLibNameCont((unsigned char)sSourceText[iLibEnd]) ) {
						++iLibEnd;
					}
					while ( (iLibEnd < iLineEnd) && __xlIsAsciiSpace((unsigned char)sSourceText[iLibEnd]) ) {
						++iLibEnd;
					}
					if ( (iLibEnd == iLineEnd) ||
						((sSourceText[iLibEnd] == '/') && ((iLibEnd + 1) < iLineEnd) && (sSourceText[iLibEnd + 1] == '/')) ) {
						size_t iLibLen;
						char sLibName[128];

						iLibLen = iLibEnd - iLibStart;
						while ( (iLibLen > 0) && __xlIsAsciiSpace((unsigned char)sSourceText[iLibStart + iLibLen - 1]) ) {
							--iLibLen;
						}
						if ( iLibLen >= sizeof(sLibName) ) {
							iLibLen = sizeof(sLibName) - 1;
						}
						memcpy(sLibName, sSourceText + iLibStart, iLibLen);
						sLibName[iLibLen] = '\0';
						iRc = __xlModuleInfoAddLinkLib(pModule, sLibName);
						if ( iRc != XL_RC_OK ) {
							free(out.sData);
							return iRc;
						}
						bHandledDirective = 1;
					}
				}
			} else if ( (iDirectiveLen == 7) && (memcmp(sDirective, "libpath", 7) == 0) ) {
				iRc = __xlExtractQuotedDirectiveValue(sSourceText, iDirectiveStart + iDirectiveLen, iLineEnd, sValue, sizeof(sValue));
				if ( iRc == XL_RC_OK ) {
					iRc = __xlNormalizeDirectivePath(sModulePath, sValue, sNormPath, sizeof(sNormPath));
				}
				if ( iRc == XL_RC_OK ) {
					iRc = __xlModuleInfoAddLibPath(pModule, sNormPath);
				}
				if ( iRc != XL_RC_FAIL ) {
					if ( iRc != XL_RC_OK ) {
						free(out.sData);
						return iRc;
					}
					bHandledDirective = 1;
				}
			} else if ( (iDirectiveLen == 7) && (memcmp(sDirective, "incpath", 7) == 0) ) {
				iRc = __xlExtractQuotedDirectiveValue(sSourceText, iDirectiveStart + iDirectiveLen, iLineEnd, sValue, sizeof(sValue));
				if ( iRc == XL_RC_OK ) {
					iRc = __xlNormalizeDirectivePath(sModulePath, sValue, sNormPath, sizeof(sNormPath));
				}
				if ( iRc == XL_RC_OK ) {
					iRc = __xlModuleInfoAddIncPath(pModule, sNormPath);
				}
				if ( iRc != XL_RC_FAIL ) {
					if ( iRc != XL_RC_OK ) {
						free(out.sData);
						return iRc;
					}
					bHandledDirective = 1;
				}
			} else if ( (iDirectiveLen == 7) && (memcmp(sDirective, "loadlib", 7) == 0) ) {
				iRc = __xlExtractQuotedDirectiveValue(sSourceText, iDirectiveStart + iDirectiveLen, iLineEnd, sValue, sizeof(sValue));
				if ( iRc == XL_RC_OK ) {
					if ( __xlDirectiveNeedsRelativePathResolution(sValue) ) {
						iRc = __xlNormalizeDirectivePath(sModulePath, sValue, sNormPath, sizeof(sNormPath));
					} else {
						snprintf(sNormPath, sizeof(sNormPath), "%s", sValue);
					}
				}
				if ( iRc == XL_RC_OK ) {
					iRc = __xlModuleInfoAddLoadLib(pModule, sNormPath);
				}
				if ( iRc != XL_RC_FAIL ) {
					if ( iRc != XL_RC_OK ) {
						free(out.sData);
						return iRc;
					}
					bHandledDirective = 1;
				}
			} else if ( (iDirectiveLen == 7) && (memcmp(sDirective, "binding", 7) == 0) ) {
				iRc = __xlExtractQuotedDirectiveValue(sSourceText, iDirectiveStart + iDirectiveLen, iLineEnd, sValue, sizeof(sValue));
				if ( iRc != XL_RC_OK ) {
					XL_DIAG_ERROR(pDiag, "binding", "invalid #binding directive; expected #binding \"header.h\"");
					free(out.sData);
					return XL_RC_FAIL;
				}
				iRc = __xlResolveBindingHeaderPath(sModulePath, pModule, sValue, sNormPath, sizeof(sNormPath));
				if ( iRc != XL_RC_OK ) {
					XL_DIAG_ERROR(pDiag, "binding", "binding header not found: %s", sValue);
					free(out.sData);
					return XL_RC_FAIL;
				}
				iRc = __xlBindingEmitHeaderDecls(sNormPath, pModule, &out, pDiag, bTraceParser);
				if ( iRc != XL_RC_OK ) {
					free(out.sData);
					return iRc;
				}
				bHandledDirective = 1;
			}
		}

		if ( bHandledDirective ) {
			if ( __xlModuleBufAppendN(&out, sSourceText + iLineEnd, iNewlineLen) != XL_RC_OK ) {
				free(out.sData);
				return XL_RC_IO_ERROR;
			}
			iPos = iLineEnd + iNewlineLen;
			continue;
		}

		if ( __xlModuleBufAppendN(&out, sSourceText + iLineStart, (iLineEnd - iLineStart) + iNewlineLen) != XL_RC_OK ) {
			free(out.sData);
			return XL_RC_IO_ERROR;
		}
		iPos = iLineEnd + iNewlineLen;
	}

	*psSanitizedText = (out.sData != NULL) ? out.sData : __xlModuleDupText("");
	return (*psSanitizedText != NULL) ? XL_RC_OK : XL_RC_IO_ERROR;
}

static void __xlModuleTrace(XL_DiagContext* pDiag, const char* sFormat, ...)
{
	va_list args;
	char sText[XL_DIAG_TEXT_MAX];

	if ( (pDiag == NULL) || (sFormat == NULL) ) {
		return;
	}

	va_start(args, sFormat);
	vsnprintf(sText, sizeof(sText), sFormat, args);
	va_end(args);
	xlDiagEmit(pDiag, XL_DIAG_LEVEL_INFO, "module", __FILE__, __LINE__, "%s", sText);
}

static void __xlModuleScopeFree(XL_ModuleScope* pScope)
{
	size_t i;

	if ( pScope == NULL ) {
		return;
	}

	for ( i = 0; i < pScope->iCount; ++i ) {
		free(pScope->arrNames[i]);
	}
	free(pScope->arrNames);
	free(pScope);
}

static int __xlFileExists(const char* sFilePath)
{
	FILE* pFile;

	if ( sFilePath == NULL ) {
		return 0;
	}

	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return 0;
	}

	fclose(pFile);
	return 1;
}

static int __xlReadModuleFileText(const char* sFilePath, char** psText)
{
	FILE* pFile;
	long iSize;
	size_t iRead;
	char* sText;

	if ( (sFilePath == NULL) || (psText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*psText = NULL;
	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}
	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	iSize = ftell(pFile);
	if ( iSize < 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	sText = (char*)malloc((size_t)iSize + 1);
	if ( sText == NULL ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	iRead = (iSize > 0) ? fread(sText, 1u, (size_t)iSize, pFile) : 0u;
	fclose(pFile);
	if ( iRead != (size_t)iSize ) {
		free(sText);
		return XL_RC_IO_ERROR;
	}

	sText[iRead] = '\0';
	*psText = sText;
	return XL_RC_OK;
}

static XL_ModuleScope* __xlModuleScopePush(XL_ModuleScope* pPrev)
{
	XL_ModuleScope* pScope;

	pScope = (XL_ModuleScope*)calloc(1, sizeof(*pScope));
	if ( pScope == NULL ) {
		return NULL;
	}

	pScope->pPrev = pPrev;
	return pScope;
}

static int __xlModuleScopeAdd(XL_ModuleScope* pScope, const char* sName)
{
	char** arrNew;
	size_t iNewCap;

	if ( (pScope == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_OK;
	}

	if ( pScope->iCount >= pScope->iCap ) {
		iNewCap = (pScope->iCap == 0) ? 4 : (pScope->iCap * 2);
		arrNew = (char**)realloc(pScope->arrNames, sizeof(char*) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pScope->arrNames = arrNew;
		pScope->iCap = iNewCap;
	}

	pScope->arrNames[pScope->iCount] = __xlModuleDupText(sName);
	if ( pScope->arrNames[pScope->iCount] == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pScope->iCount += 1;
	return XL_RC_OK;
}

static int __xlModuleScopeHas(const XL_ModuleScope* pScope, const char* sName)
{
	size_t i;

	while ( pScope != NULL ) {
		for ( i = 0; i < pScope->iCount; ++i ) {
			if ( strcmp(pScope->arrNames[i], sName) == 0 ) {
				return 1;
			}
		}
		pScope = pScope->pPrev;
	}

	return 0;
}

static void __xlModuleNormalizePath(char* sOut, size_t iOutSize, const char* sPath)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	if ( sPath == NULL ) {
		sPath = "";
	}

#if defined(_WIN32) || defined(_WIN64)
	if ( GetFullPathNameA(sPath, (DWORD)iOutSize, sOut, NULL) == 0 ) {
		snprintf(sOut, iOutSize, "%s", sPath);
	}
#else
	snprintf(sOut, iOutSize, "%s", sPath);
#endif
}

static void __xlModuleStripFileName(char* sPath)
{
	size_t iLen;

	if ( sPath == NULL ) {
		return;
	}

	iLen = strlen(sPath);
	while ( iLen > 0 ) {
		if ( xlPathIsSep((unsigned char)sPath[iLen - 1]) ) {
			sPath[iLen - 1] = '\0';
			return;
		}
		--iLen;
	}
}

static const char* __xlModuleFileStem(const char* sInputFile, char* sOut, size_t iOutSize)
{
	const char* sName;
	const char* sScan;
	const char* sDot;
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return "";
	}

	sName = (sInputFile != NULL) ? sInputFile : "package";
	sScan = sName;
	while ( *sScan != '\0' ) {
		if ( (*sScan == '\\') || (*sScan == '/') ) {
			sName = sScan + 1;
		}
		++sScan;
	}

	sDot = strrchr(sName, '.');
	iLen = (sDot != NULL) ? (size_t)(sDot - sName) : strlen(sName);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1;
	}

	memcpy(sOut, sName, iLen);
	sOut[iLen] = '\0';
	return sOut;
}

static const XL_AstNode* __xlAstChild(const XL_AstNode* pNode, size_t iIndex)
{
	if ( (pNode == NULL) || (iIndex >= pNode->iChildCount) ) {
		return NULL;
	}

	return pNode->ppChildren[iIndex];
}

static char* __xlBuildImportKey(const XL_AstNode* pNode)
{
	char* sLeft;
	char* sText;
	size_t iLen;
	size_t iNeed;
	size_t i;
	size_t iOutLen;

	if ( pNode == NULL ) {
		return __xlModuleDupText("");
	}

	if ( pNode->eKind == XL_AST_IDENT ) {
		return __xlModuleDupText(pNode->sText);
	}
	if ( pNode->eKind == XL_AST_STRING ) {
		const char* sSource;

		sSource = (pNode->sText != NULL) ? pNode->sText : "";
		iLen = strlen(sSource);
		if ( (iLen >= 2) &&
			(((sSource[0] == '\"') && (sSource[iLen - 1] == '\"')) ||
			((sSource[0] == '\'') && (sSource[iLen - 1] == '\''))) ) {
			sSource += 1;
			iLen -= 2;
		}

		sText = (char*)malloc(iLen + 1);
		if ( sText == NULL ) {
			return NULL;
		}

		iOutLen = 0;
		for ( i = 0; i < iLen; ++i ) {
			char ch;

			ch = sSource[i];
			if ( (ch == '/') || (ch == '\\') ) {
				if ( (iOutLen == 0) || (sText[iOutLen - 1] == '.') ) {
					continue;
				}
				sText[iOutLen++] = '.';
				continue;
			}
			sText[iOutLen++] = ch;
		}
		if ( (iOutLen >= 3) && (strcmp(sText + iOutLen - 3, ".xl") == 0) ) {
			iOutLen -= 3;
		}
		sText[iOutLen] = '\0';
		return sText;
	}
	if ( (pNode->eKind != XL_AST_MEMBER) || (pNode->sText == NULL) ) {
		return __xlModuleDupText("");
	}

	sLeft = __xlBuildImportKey(__xlAstChild(pNode, 0));
	if ( sLeft == NULL ) {
		return NULL;
	}

	iNeed = strlen(sLeft) + 1 + strlen(pNode->sText) + 1;
	sText = (char*)malloc(iNeed);
	if ( sText == NULL ) {
		free(sLeft);
		return NULL;
	}

	if ( sLeft[0] == '\0' ) {
		snprintf(sText, iNeed, "%s", pNode->sText);
	} else {
		snprintf(sText, iNeed, "%s.%s", sLeft, pNode->sText);
	}
	free(sLeft);
	return sText;
}

static int __xlResolveImportFile(char* sOut, size_t iOutSize, const char* sCurrentFile, const char* sImportKey)
{
	char sBaseDir[XL_PATH_MAX_LEN];
	char sImportRel[XL_PATH_MAX_LEN];
	char sJoined[XL_PATH_MAX_LEN];
	size_t i;

	if ( (sOut == NULL) || (iOutSize == 0) || (sCurrentFile == NULL) || (sImportKey == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	snprintf(sBaseDir, sizeof(sBaseDir), "%s", sCurrentFile);
	__xlModuleStripFileName(sBaseDir);

	snprintf(sImportRel, sizeof(sImportRel), "%s", sImportKey);
	for ( i = 0; sImportRel[i] != '\0'; ++i ) {
		if ( sImportRel[i] == '.' ) {
			sImportRel[i] = '\\';
		}
	}
	if ( strlen(sImportRel) + 4 < sizeof(sImportRel) ) {
		strcat(sImportRel, ".xl");
	}

	xlPathJoin(sJoined, sizeof(sJoined), sBaseDir, sImportRel);
	__xlModuleNormalizePath(sOut, iOutSize, sJoined);
	return XL_RC_OK;
}

static void __xlBuildImportRelPath(char* sOut, size_t iOutSize, const char* sImportKey)
{
	size_t i;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	snprintf(sOut, iOutSize, "%s", (sImportKey != NULL) ? sImportKey : "");
	for ( i = 0; sOut[i] != '\0'; ++i ) {
		if ( sOut[i] == '.' ) {
			sOut[i] = '\\';
		}
	}
	if ( strlen(sOut) + 4 < iOutSize ) {
		strcat(sOut, ".xl");
	}
}

static int __xlResolveImportFileWithProject(
	const XL_ModuleLinkContext* pCtx,
	char* sOut,
	size_t iOutSize,
	const char* sCurrentFile,
	const char* sImportKey
)
{
	char sImportRel[XL_PATH_MAX_LEN];
	char sJoined[XL_PATH_MAX_LEN];
	size_t i;
	int iRc;

	iRc = __xlResolveImportFile(sOut, iOutSize, sCurrentFile, sImportKey);
	if ( (iRc != XL_RC_OK) || __xlFileExists(sOut) || (pCtx == NULL) || !pCtx->bHasProject ) {
		return iRc;
	}

	__xlBuildImportRelPath(sImportRel, sizeof(sImportRel), sImportKey);
	for ( i = 0; i < pCtx->iSourceRootCount; ++i ) {
		xlPathJoin(sJoined, sizeof(sJoined), pCtx->arrSourceRoots[i], sImportRel);
		__xlModuleNormalizePath(sOut, iOutSize, sJoined);
		if ( __xlFileExists(sOut) ) {
			return XL_RC_OK;
		}
	}
	return XL_RC_OK;
}

static int __xlModuleInfoAddImport(XL_ModuleInfo* pModule, const char* sImportKey, const char* sImportAlias)
{
	char** arrNew;
	char** arrAliasNew;
	char** arrResolvedNew;
	char* sAliasCopy;
	size_t iNewCap;

	if ( (pModule == NULL) || (sImportKey == NULL) || (sImportKey[0] == '\0') ) {
		return XL_RC_OK;
	}

	if ( pModule->iImportCount >= pModule->iImportCap ) {
		iNewCap = (pModule->iImportCap == 0) ? 4 : (pModule->iImportCap * 2);
		arrNew = (char**)realloc(pModule->arrImportKeys, sizeof(char*) * iNewCap);
		arrAliasNew = (char**)realloc(pModule->arrImportAliases, sizeof(char*) * iNewCap);
		arrResolvedNew = (char**)realloc(pModule->arrResolvedImportFiles, sizeof(char*) * iNewCap);
		if ( (arrNew == NULL) || (arrAliasNew == NULL) || (arrResolvedNew == NULL) ) {
			if ( arrNew != NULL ) {
				pModule->arrImportKeys = arrNew;
			}
			if ( arrAliasNew != NULL ) {
				pModule->arrImportAliases = arrAliasNew;
			}
			if ( arrResolvedNew != NULL ) {
				pModule->arrResolvedImportFiles = arrResolvedNew;
			}
			return XL_RC_IO_ERROR;
		}
		pModule->arrImportKeys = arrNew;
		pModule->arrImportAliases = arrAliasNew;
		pModule->arrResolvedImportFiles = arrResolvedNew;
		memset(pModule->arrImportAliases + pModule->iImportCap, 0, sizeof(char*) * (iNewCap - pModule->iImportCap));
		memset(pModule->arrResolvedImportFiles + pModule->iImportCap, 0, sizeof(char*) * (iNewCap - pModule->iImportCap));
		pModule->iImportCap = iNewCap;
	}

	pModule->arrImportKeys[pModule->iImportCount] = __xlModuleDupText(sImportKey);
	if ( pModule->arrImportKeys[pModule->iImportCount] == NULL ) {
		return XL_RC_IO_ERROR;
	}
	sAliasCopy = NULL;
	if ( (sImportAlias != NULL) && (sImportAlias[0] != '\0') ) {
		sAliasCopy = __xlModuleDupText(sImportAlias);
		if ( sAliasCopy == NULL ) {
			free(pModule->arrImportKeys[pModule->iImportCount]);
			pModule->arrImportKeys[pModule->iImportCount] = NULL;
			return XL_RC_IO_ERROR;
		}
	}
	pModule->arrImportAliases[pModule->iImportCount] = sAliasCopy;
	pModule->iImportCount += 1;
	return XL_RC_OK;
}

static int __xlModuleInfoResolveImport(XL_ModuleInfo* pModule, size_t iImportIndex, const char* sResolvedFile)
{
	char* sCopy;

	if ( (pModule == NULL) || (iImportIndex >= pModule->iImportCount) ) {
		return XL_RC_BAD_ARG;
	}

	sCopy = __xlModuleDupText(sResolvedFile);
	if ( (sResolvedFile != NULL) && (sCopy == NULL) ) {
		return XL_RC_IO_ERROR;
	}

	free(pModule->arrResolvedImportFiles[iImportIndex]);
	pModule->arrResolvedImportFiles[iImportIndex] = sCopy;
	return XL_RC_OK;
}

static int __xlModuleInfoAddName(XL_ModuleInfo* pModule, const char* sOriginalName, const char* sMangledName)
{
	XL_ModuleNameMap* arrNew;
	size_t iNewCap;

	if ( (pModule == NULL) || (sOriginalName == NULL) || (sOriginalName[0] == '\0') ) {
		return XL_RC_OK;
	}

	if ( pModule->iNameCount >= pModule->iNameCap ) {
		iNewCap = (pModule->iNameCap == 0) ? 4 : (pModule->iNameCap * 2);
		arrNew = (XL_ModuleNameMap*)realloc(pModule->arrNames, sizeof(XL_ModuleNameMap) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pModule->arrNames = arrNew;
		pModule->iNameCap = iNewCap;
	}

	pModule->arrNames[pModule->iNameCount].sOriginalName = __xlModuleDupText(sOriginalName);
	pModule->arrNames[pModule->iNameCount].sMangledName = __xlModuleDupText(sMangledName);
	if ( (pModule->arrNames[pModule->iNameCount].sOriginalName == NULL) || (pModule->arrNames[pModule->iNameCount].sMangledName == NULL) ) {
		free(pModule->arrNames[pModule->iNameCount].sOriginalName);
		free(pModule->arrNames[pModule->iNameCount].sMangledName);
		return XL_RC_IO_ERROR;
	}
	pModule->iNameCount += 1;
	return XL_RC_OK;
}

static int __xlModuleInfoAddExport(XL_ModuleInfo* pModule, const char* sName, const char* sMangledName)
{
	XL_ModuleExportInfo* arrNew;
	size_t iNewCap;

	if ( (pModule == NULL) || (sName == NULL) || (sName[0] == '\0') || (sMangledName == NULL) ) {
		return XL_RC_OK;
	}

	if ( pModule->iExportCount >= pModule->iExportCap ) {
		iNewCap = (pModule->iExportCap == 0) ? 4 : (pModule->iExportCap * 2);
		arrNew = (XL_ModuleExportInfo*)realloc(pModule->arrExports, sizeof(XL_ModuleExportInfo) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pModule->arrExports = arrNew;
		pModule->iExportCap = iNewCap;
	}

	pModule->arrExports[pModule->iExportCount].sName = __xlModuleDupText(sName);
	pModule->arrExports[pModule->iExportCount].sMangledName = __xlModuleDupText(sMangledName);
	if ( (pModule->arrExports[pModule->iExportCount].sName == NULL) || (pModule->arrExports[pModule->iExportCount].sMangledName == NULL) ) {
		free(pModule->arrExports[pModule->iExportCount].sName);
		free(pModule->arrExports[pModule->iExportCount].sMangledName);
		return XL_RC_IO_ERROR;
	}
	pModule->iExportCount += 1;
	return XL_RC_OK;
}

static void __xlModuleInfoFree(XL_ModuleInfo* pModule)
{
	size_t i;

	if ( pModule == NULL ) {
		return;
	}

	free(pModule->sModuleKey);
	free(pModule->sFilePath);
	free(pModule->sLeafAlias);
	free(pModule->sBindingName);
	free(pModule->sInitSymbol);
	free(pModule->sExportsSymbol);
	free(pModule->sDescriptorSymbol);
	free(pModule->sManifestPath);
	free(pModule->sPackageName);
	free(pModule->sPackageVersion);
	free(pModule->sPackageAbiVersion);
	free(pModule->sPackageAbiId);
	free(pModule->sArtifactKind);
	free(pModule->sArtifactPath);
	free(pModule->sArtifactFile);
	free(pModule->sConsumeMode);
	xlAstFree(pModule->pRoot);

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		free(pModule->arrImportKeys[i]);
		free(pModule->arrImportAliases[i]);
		free(pModule->arrResolvedImportFiles[i]);
	}
	free(pModule->arrImportKeys);
	free(pModule->arrImportAliases);
	free(pModule->arrResolvedImportFiles);
	for ( i = 0; i < pModule->iLinkLibCount; ++i ) {
		free(pModule->arrLinkLibs[i]);
	}
	free(pModule->arrLinkLibs);
	for ( i = 0; i < pModule->iLibPathCount; ++i ) {
		free(pModule->arrLibPaths[i]);
	}
	free(pModule->arrLibPaths);
	for ( i = 0; i < pModule->iIncPathCount; ++i ) {
		free(pModule->arrIncPaths[i]);
	}
	free(pModule->arrIncPaths);
	for ( i = 0; i < pModule->iLoadLibCount; ++i ) {
		free(pModule->arrLoadLibs[i]);
	}
	free(pModule->arrLoadLibs);

	for ( i = 0; i < pModule->iNameCount; ++i ) {
		free(pModule->arrNames[i].sOriginalName);
		free(pModule->arrNames[i].sMangledName);
	}
	free(pModule->arrNames);

	for ( i = 0; i < pModule->iExportCount; ++i ) {
		free(pModule->arrExports[i].sName);
		free(pModule->arrExports[i].sMangledName);
	}
	free(pModule->arrExports);

	memset(pModule, 0, sizeof(*pModule));
}

static XL_ModuleInfo* __xlFindModuleByFile(XL_ModuleLinkContext* pCtx, const char* sFilePath)
{
	size_t i;

	if ( (pCtx == NULL) || (sFilePath == NULL) || (sFilePath[0] == '\0') ) {
		return NULL;
	}

	for ( i = 0; i < pCtx->iModuleCount; ++i ) {
		if ( (pCtx->pModules[i].sFilePath != NULL) && (strcmp(pCtx->pModules[i].sFilePath, sFilePath) == 0) ) {
			return &pCtx->pModules[i];
		}
	}

	return NULL;
}

static XL_ModuleInfo* __xlFindModuleByKey(const XL_ModuleLinkContext* pCtx, const char* sModuleKey)
{
	size_t i;

	if ( (pCtx == NULL) || (sModuleKey == NULL) || (sModuleKey[0] == '\0') ) {
		return NULL;
	}

	for ( i = 0; i < pCtx->iModuleCount; ++i ) {
		if ( (pCtx->pModules[i].sModuleKey != NULL) && (strcmp(pCtx->pModules[i].sModuleKey, sModuleKey) == 0) ) {
			return &pCtx->pModules[i];
		}
	}

	return NULL;
}

static int __xlAppendOrder(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	size_t* pNewIndices;
	size_t iNewCap;
	size_t iModuleIndex;

	if ( (pCtx == NULL) || (pModule == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	if ( pCtx->iOrderCount >= pCtx->iOrderCap ) {
		iNewCap = (pCtx->iOrderCap == 0) ? 4 : (pCtx->iOrderCap * 2);
		pNewIndices = (size_t*)realloc(pCtx->pOrderIndices, sizeof(size_t) * iNewCap);
		if ( pNewIndices == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pCtx->pOrderIndices = pNewIndices;
		pCtx->iOrderCap = iNewCap;
	}

	iModuleIndex = (size_t)(pModule - pCtx->pModules);
	pCtx->pOrderIndices[pCtx->iOrderCount++] = iModuleIndex;
	return XL_RC_OK;
}

static XL_ModuleInfo* __xlAddModuleRecord(XL_ModuleLinkContext* pCtx)
{
	XL_ModuleInfo* pNewItems;
	size_t iNewCap;

	if ( pCtx->iModuleCount >= pCtx->iModuleCap ) {
		iNewCap = (pCtx->iModuleCap == 0) ? 4 : (pCtx->iModuleCap * 2);
		pNewItems = (XL_ModuleInfo*)realloc(pCtx->pModules, sizeof(XL_ModuleInfo) * iNewCap);
		if ( pNewItems == NULL ) {
			return NULL;
		}
		memset(pNewItems + pCtx->iModuleCap, 0, sizeof(XL_ModuleInfo) * (iNewCap - pCtx->iModuleCap));
		pCtx->pModules = pNewItems;
		pCtx->iModuleCap = iNewCap;
	}

	return &pCtx->pModules[pCtx->iModuleCount++];
}

static const char* __xlFindMangledName(const XL_ModuleInfo* pModule, const char* sOriginalName)
{
	size_t i;

	if ( (pModule == NULL) || (sOriginalName == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pModule->iNameCount; ++i ) {
		if ( strcmp(pModule->arrNames[i].sOriginalName, sOriginalName) == 0 ) {
			return pModule->arrNames[i].sMangledName;
		}
	}

	return NULL;
}

static const char* __xlFindExportName(const XL_ModuleInfo* pModule, const char* sExportName)
{
	size_t i;

	if ( (pModule == NULL) || (sExportName == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pModule->iExportCount; ++i ) {
		if ( strcmp(pModule->arrExports[i].sName, sExportName) == 0 ) {
			return pModule->arrExports[i].sMangledName;
		}
	}

	return NULL;
}

static int __xlModuleHasImportKey(const XL_ModuleInfo* pModule, const char* sModuleKey)
{
	size_t i;

	if ( (pModule == NULL) || (sModuleKey == NULL) ) {
		return 0;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( strcmp(pModule->arrImportKeys[i], sModuleKey) == 0 ) {
			return 1;
		}
	}

	return 0;
}

static const char* __xlFindImportKeyByAlias(const XL_ModuleInfo* pModule, const char* sAlias)
{
	size_t i;

	if ( (pModule == NULL) || (sAlias == NULL) || (sAlias[0] == '\0') ) {
		return NULL;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( (pModule->arrImportAliases[i] != NULL) && (strcmp(pModule->arrImportAliases[i], sAlias) == 0) ) {
			return pModule->arrImportKeys[i];
		}
	}

	return NULL;
}

static XL_ModuleInfo* __xlFindImportedModule(const XL_ModuleLinkContext* pCtx, const XL_ModuleInfo* pModule, const char* sModuleKey)
{
	size_t i;
	XL_ModuleInfo* pImported;

	if ( (pCtx == NULL) || (pModule == NULL) || (sModuleKey == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( strcmp(pModule->arrImportKeys[i], sModuleKey) != 0 ) {
			continue;
		}
		if ( pModule->arrResolvedImportFiles[i] != NULL ) {
			pImported = __xlFindModuleByFile((XL_ModuleLinkContext*)pCtx, pModule->arrResolvedImportFiles[i]);
			if ( pImported != NULL ) {
				return pImported;
			}
		}
	}

	return __xlFindModuleByKey(pCtx, sModuleKey);
}

static void __xlReplaceNodeWithIdent(XL_AstNode* pNode, const char* sName)
{
	size_t i;

	if ( (pNode == NULL) || (sName == NULL) ) {
		return;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlAstFree(pNode->ppChildren[i]);
	}
	free(pNode->ppChildren);
	pNode->ppChildren = NULL;
	pNode->iChildCount = 0;
	pNode->iChildCapacity = 0;
	free(pNode->sText2);
	pNode->sText2 = NULL;
	pNode->eKind = XL_AST_IDENT;
	pNode->iIntValue = 0;
	pNode->fFloatValue = 0.0;
	pNode->bFlag = 0;
	xlAstSetText(pNode, sName, strlen(sName));
}

static int __xlCollectImports(XL_ModuleInfo* pModule)
{
	const XL_AstNode* pStmt;
	const char* sImportAlias;
	char* sImportKey;
	size_t i;
	int iRc;

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( (pStmt == NULL) || (pStmt->eKind != XL_AST_IMPORT) ) {
			continue;
		}

		sImportKey = __xlBuildImportKey(__xlAstChild(pStmt, 0));
		if ( sImportKey == NULL ) {
			return XL_RC_IO_ERROR;
		}
		sImportAlias = ((pStmt->sText2 != NULL) && (pStmt->sText2[0] != '\0')) ? pStmt->sText2 : NULL;
		iRc = __xlModuleInfoAddImport(pModule, sImportKey, sImportAlias);
		free(sImportKey);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	return XL_RC_OK;
}

static const char* __xlModuleMangleKey(const XL_ModuleInfo* pModule, char* sBuffer, size_t iBufferSize)
{
	if ( (sBuffer == NULL) || (iBufferSize == 0) ) {
		return "";
	}

	if ( pModule == NULL ) {
		sBuffer[0] = '\0';
		return sBuffer;
	}

	if ( (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') && !pModule->bIsRoot ) {
		snprintf(sBuffer, iBufferSize, "%s__%s", pModule->sPackageAbiId, (pModule->sModuleKey != NULL) ? pModule->sModuleKey : "module");
	} else {
		snprintf(sBuffer, iBufferSize, "%s", (pModule->sModuleKey != NULL) ? pModule->sModuleKey : "module");
	}

	return sBuffer;
}

static char* __xlBuildMangledName(const char* sModuleKey, const char* sName)
{
	char sPrefix[XL_PATH_MAX_LEN];
	size_t i;
	size_t iLen;
	char* sResult;

	snprintf(sPrefix, sizeof(sPrefix), "xl_mod_%s__", (sModuleKey != NULL) ? sModuleKey : "module");
	for ( i = 0; sPrefix[i] != '\0'; ++i ) {
		if ( !( (sPrefix[i] >= '0') && (sPrefix[i] <= '9') ) &&
			!( (sPrefix[i] >= 'A') && (sPrefix[i] <= 'Z') ) &&
			!( (sPrefix[i] >= 'a') && (sPrefix[i] <= 'z') ) &&
			(sPrefix[i] != '_') ) {
			sPrefix[i] = '_';
		}
	}

	iLen = strlen(sPrefix) + strlen((sName != NULL) ? sName : "") + 1;
	sResult = (char*)malloc(iLen);
	if ( sResult == NULL ) {
		return NULL;
	}

	snprintf(sResult, iLen, "%s%s", sPrefix, (sName != NULL) ? sName : "");
	return sResult;
}

static int __xlCollectTopLevelNamesAndExports(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	XL_AstNode* pStmt;
	const XL_AstNode* pValue;
	char* sMangledName;
	char sMangleKey[XL_PATH_MAX_LEN];
	const char* sResolvedName;
	size_t i;
	size_t j;
	int iRc;

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( pStmt == NULL ) {
			continue;
		}

		if ( (pStmt->eKind == XL_AST_FUNC_DECL) || (pStmt->eKind == XL_AST_CONST_DECL) || (pStmt->eKind == XL_AST_VAR_DECL) ) {
			sMangledName = pModule->bIsRoot ? __xlModuleDupText(pStmt->sText) : __xlBuildMangledName(__xlModuleMangleKey(pModule, sMangleKey, sizeof(sMangleKey)), pStmt->sText);
			if ( sMangledName == NULL ) {
				return XL_RC_IO_ERROR;
			}
			iRc = __xlModuleInfoAddName(pModule, pStmt->sText, sMangledName);
			free(sMangledName);
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
		}
	}

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( (pStmt == NULL) || (pStmt->eKind != XL_AST_EXPORT) ) {
			continue;
		}

		pValue = __xlAstChild(pStmt, 0);
		if ( (pValue == NULL) || (pValue->eKind != XL_AST_IDENT) || (pValue->sText == NULL) ) {
			XL_DIAG_ERROR(pCtx->pDiag, "module", "export only supports top-level identifiers in demo5 stage 7");
			return XL_RC_FAIL;
		}

		sResolvedName = __xlFindMangledName(pModule, pValue->sText);
		if ( sResolvedName == NULL ) {
			XL_DIAG_ERROR(pCtx->pDiag, "module", "export target not found in module '%s': %s", pModule->sModuleKey, pValue->sText);
			return XL_RC_FAIL;
		}

		iRc = __xlModuleInfoAddExport(pModule, pValue->sText, sResolvedName);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	if ( pModule->bIsRoot ) {
		return XL_RC_OK;
	}

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( pStmt == NULL ) {
			continue;
		}
		if ( (pStmt->eKind != XL_AST_FUNC_DECL) && (pStmt->eKind != XL_AST_CONST_DECL) && (pStmt->eKind != XL_AST_VAR_DECL) ) {
			continue;
		}
		for ( j = 0; j < pModule->iNameCount; ++j ) {
			if ( strcmp(pStmt->sText, pModule->arrNames[j].sOriginalName) == 0 ) {
				xlAstSetText(pStmt, pModule->arrNames[j].sMangledName, strlen(pModule->arrNames[j].sMangledName));
				break;
			}
		}
	}

	return XL_RC_OK;
}

static void __xlRewriteModuleNode(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode, XL_ModuleScope* pScope);

static void __xlRewriteSequentialChildren(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode, XL_ModuleScope* pScope)
{
	XL_AstNode* pChild;
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		pChild = pNode->ppChildren[i];
		__xlRewriteModuleNode(pCtx, pModule, pChild, pScope);
		if ( (pScope != NULL) && (pChild != NULL) &&
			((pChild->eKind == XL_AST_CONST_DECL) || (pChild->eKind == XL_AST_VAR_DECL) || (pChild->eKind == XL_AST_FUNC_DECL)) &&
			(pChild->sText != NULL) ) {
			__xlModuleScopeAdd(pScope, pChild->sText);
		}
	}
}

static const char* __xlMemberRootName(const XL_AstNode* pNode)
{
	const XL_AstNode* pScan;

	pScan = pNode;
	while ( (pScan != NULL) && (pScan->eKind == XL_AST_MEMBER) ) {
		pScan = __xlAstChild(pScan, 0);
	}
	if ( (pScan == NULL) || (pScan->eKind != XL_AST_IDENT) ) {
		return NULL;
	}
	return pScan->sText;
}

static void __xlRewriteMemberImportRef(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode, XL_ModuleScope* pScope)
{
	char* sChain;
	char* sDot;
	char* sImportKey;
	XL_ModuleInfo* pImported;
	const char* sMangledName;
	const char* sRootName;
	size_t i;

	if ( (pModule == NULL) || (pNode == NULL) ) {
		return;
	}

	sChain = __xlBuildImportKey(pNode);
	if ( sChain == NULL ) {
		return;
	}

	sDot = strrchr(sChain, '.');
	if ( sDot == NULL ) {
		free(sChain);
		return;
	}

	*sDot = '\0';
	sImportKey = sChain;
	if ( !__xlModuleHasImportKey(pModule, sImportKey) ) {
		const char* sAliasedImportKey;

		sRootName = __xlMemberRootName(pNode);
		if ( (sRootName != NULL) && __xlModuleScopeHas(pScope, sRootName) ) {
			free(sChain);
			return;
		}

		sAliasedImportKey = __xlFindImportKeyByAlias(pModule, sImportKey);
		if ( sAliasedImportKey != NULL ) {
			sImportKey = (char*)sAliasedImportKey;
		} else {
			for ( i = 0; i < pModule->iImportCount; ++i ) {
				pImported = __xlFindImportedModule(pCtx, pModule, pModule->arrImportKeys[i]);
				if ( (pImported != NULL) && (pImported->sLeafAlias != NULL) && (strcmp(pImported->sLeafAlias, sImportKey) == 0) ) {
					sImportKey = pImported->sModuleKey;
					break;
				}
			}
		}
	}

	pImported = __xlFindImportedModule(pCtx, pModule, sImportKey);
	if ( (pImported == NULL) || !__xlModuleHasImportKey(pModule, sImportKey) ) {
		if ( strcmp(sImportKey, sChain) != 0 ) {
			/* alias path already resolved */
		} else {
			free(sChain);
			return;
		}
	}

	sMangledName = __xlFindExportName(pImported, sDot + 1);
	if ( sMangledName != NULL ) {
		__xlReplaceNodeWithIdent(pNode, sMangledName);
	}
	free(sChain);
}

static void __xlRewriteModuleNode(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode, XL_ModuleScope* pScope)
{
	XL_ModuleScope* pFuncScope;
	const XL_AstNode* pParams;
	const XL_AstNode* pParam;
	size_t i;
	const char* sMangledName;

	if ( pNode == NULL ) {
		return;
	}

	switch ( pNode->eKind ) {
		case XL_AST_MEMBER:
			__xlRewriteMemberImportRef(pCtx, pModule, pNode, pScope);
			if ( pNode->eKind == XL_AST_IDENT ) {
				return;
			}
			__xlRewriteSequentialChildren(pCtx, pModule, pNode, pScope);
			return;
		case XL_AST_IDENT:
			if ( (pNode->sText != NULL) && !__xlModuleScopeHas(pScope, pNode->sText) ) {
				sMangledName = __xlFindMangledName(pModule, pNode->sText);
				if ( (sMangledName != NULL) && (!pModule->bIsRoot) ) {
					xlAstSetText(pNode, sMangledName, strlen(sMangledName));
				}
			}
			return;
		case XL_AST_FUNC_DECL:
			pFuncScope = __xlModuleScopePush(pScope);
			if ( pFuncScope == NULL ) {
				return;
			}
			pParams = __xlAstChild(pNode, 0);
			for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
				pParam = pParams->ppChildren[i];
				if ( (pParam != NULL) && (pParam->sText != NULL) ) {
					__xlModuleScopeAdd(pFuncScope, pParam->sText);
				}
			}
			if ( __xlAstChild(pNode, 2) != NULL ) {
				__xlRewriteSequentialChildren(pCtx, pModule, pNode->ppChildren[2], pFuncScope);
			}
			__xlModuleScopeFree(pFuncScope);
			return;
		case XL_AST_BLOCK:
			pFuncScope = __xlModuleScopePush(pScope);
			if ( pFuncScope == NULL ) {
				return;
			}
			__xlRewriteSequentialChildren(pCtx, pModule, pNode, pFuncScope);
			__xlModuleScopeFree(pFuncScope);
			return;
		case XL_AST_FORIN:
			pFuncScope = __xlModuleScopePush(pScope);
			if ( pFuncScope == NULL ) {
				return;
			}
			if ( __xlAstChild(pNode, 1) != NULL ) {
				__xlRewriteModuleNode(pCtx, pModule, pNode->ppChildren[1], pScope);
			}
			if ( __xlAstChild(pNode, 0) != NULL ) {
				for ( i = 0; i < pNode->ppChildren[0]->iChildCount; ++i ) {
					pParam = pNode->ppChildren[0]->ppChildren[i];
					if ( (pParam != NULL) && (pParam->sText != NULL) ) {
						__xlModuleScopeAdd(pFuncScope, pParam->sText);
					}
				}
			}
			if ( __xlAstChild(pNode, 2) != NULL ) {
				__xlRewriteModuleNode(pCtx, pModule, pNode->ppChildren[2], pFuncScope);
			}
			__xlModuleScopeFree(pFuncScope);
			return;
		default:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				__xlRewriteModuleNode(pCtx, pModule, pNode->ppChildren[i], pScope);
			}
			return;
	}
}

static int __xlLinkAndRewriteModule(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	int iRc;

	iRc = __xlCollectTopLevelNamesAndExports(pCtx, pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlRewriteSequentialChildren(pCtx, pModule, pModule->pRoot, NULL);
	return XL_RC_OK;
}

static int __xlParseFileToAst(const char* sFilePath, XL_DiagContext* pDiag, int bTraceParser, XL_AstNode** ppRoot)
{
	XL_Lexer* pLexer;
	XL_ParseResult result;
	int iRc;

	if ( (sFilePath == NULL) || (ppRoot == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppRoot = NULL;
	pLexer = xlLexerCreateFromFile(sFilePath);
	if ( pLexer == NULL ) {
		return XL_RC_IO_ERROR;
	}

	xlParseResultInit(&result);
	iRc = xlParserParseLexer(pLexer, pDiag, bTraceParser, &result);
	xlLexerDestroy(pLexer);
	if ( (iRc != XL_RC_OK) || (result.pRoot == NULL) ) {
		xlParseResultFree(&result);
		return (iRc != XL_RC_OK) ? iRc : XL_RC_FAIL;
	}

	*ppRoot = result.pRoot;
	result.pRoot = NULL;
	xlParseResultFree(&result);
	return XL_RC_OK;
}

static int __xlParseSourceToAst(const char* sSourceText, const char* sVirtualPath, XL_DiagContext* pDiag, int bTraceParser, XL_AstNode** ppRoot)
{
	XL_Lexer* pLexer;
	XL_ParseResult result;
	int iRc;

	if ( (sSourceText == NULL) || (ppRoot == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppRoot = NULL;
	pLexer = xlLexerCreateFromString(sSourceText, strlen(sSourceText), (sVirtualPath != NULL) ? sVirtualPath : "<native>");
	if ( pLexer == NULL ) {
		return XL_RC_IO_ERROR;
	}

	xlParseResultInit(&result);
	iRc = xlParserParseLexer(pLexer, pDiag, bTraceParser, &result);
	xlLexerDestroy(pLexer);
	if ( (iRc != XL_RC_OK) || (result.pRoot == NULL) ) {
		xlParseResultFree(&result);
		return (iRc != XL_RC_OK) ? iRc : XL_RC_FAIL;
	}

	*ppRoot = result.pRoot;
	result.pRoot = NULL;
	xlParseResultFree(&result);
	return XL_RC_OK;
}

static int __xlParseModuleSourceText(
	const char* sSourceText,
	const char* sVirtualPath,
	XL_ModuleInfo* pModule,
	XL_DiagContext* pDiag,
	int bTraceParser,
	XL_AstNode** ppRoot
)
{
	char* sSanitized;
	int iRc;

	if ( (sSourceText == NULL) || (pModule == NULL) || (ppRoot == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	sSanitized = NULL;
	iRc = __xlExtractDirectiveDecls(sSourceText, sVirtualPath, pModule, pDiag, bTraceParser, &sSanitized);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlParseSourceToAst(sSanitized, sVirtualPath, pDiag, bTraceParser, ppRoot);
	free(sSanitized);
	return iRc;
}

static int __xlParseModuleFileToAst(
	const char* sFilePath,
	XL_ModuleInfo* pModule,
	XL_DiagContext* pDiag,
	int bTraceParser,
	XL_AstNode** ppRoot
)
{
	char* sSourceText;
	int iRc;

	if ( (sFilePath == NULL) || (pModule == NULL) || (ppRoot == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	sSourceText = NULL;
	iRc = __xlReadModuleFileText(sFilePath, &sSourceText);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	iRc = __xlParseModuleSourceText(sSourceText, sFilePath, pModule, pDiag, bTraceParser, ppRoot);
	free(sSourceText);
	return iRc;
}

static void __xlSanitizeIdentPart(char* sOut, size_t iOutSize, const char* sText)
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

static void __xlBuildModuleAbiSymbol(char* sOut, size_t iOutSize, const char* sModuleKey, const char* sSuffix)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];
	const char* sSourceKey;

	sSourceKey = ((sModuleKey != NULL) && (sModuleKey[0] != '\0')) ? sModuleKey : "<root>";
	__xlSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sSourceKey);
	snprintf(
		sOut,
		iOutSize,
		"xl_mod_%s__%s",
		(sModuleKeySafe[0] != '\0') ? sModuleKeySafe : "module",
		(sSuffix != NULL) ? sSuffix : ""
	);
}

static int __xlModuleKeyHasCollisionInContext(const XL_ModuleLinkContext* pCtx, const XL_ModuleInfo* pTarget)
{
	size_t i;
	size_t iMatchCount;
	const char* sTargetKey;

	if ( (pCtx == NULL) || (pCtx->pModules == NULL) || (pTarget == NULL) ) {
		return 0;
	}

	sTargetKey = ((pTarget->sModuleKey != NULL) && (pTarget->sModuleKey[0] != '\0')) ? pTarget->sModuleKey : "<root>";
	iMatchCount = 0;
	for ( i = 0; i < pCtx->iModuleCount; ++i ) {
		const XL_ModuleInfo* pModule;
		const char* sModuleKey;

		pModule = &pCtx->pModules[i];
		sModuleKey = ((pModule->sModuleKey != NULL) && (pModule->sModuleKey[0] != '\0')) ? pModule->sModuleKey : "<root>";
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

static void __xlBuildUniqueModuleAbiSymbol(const XL_ModuleInfo* pModule, const char* sSuffix, char* sOut, size_t iOutSize)
{
	char sUniqueKey[XL_PATH_MAX_LEN];
	const char* sModuleKey;

	if ( (pModule != NULL) &&
		!pModule->bIsRoot &&
		(pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
		sModuleKey = ((pModule->sModuleKey != NULL) && (pModule->sModuleKey[0] != '\0')) ? pModule->sModuleKey : "module";
		snprintf(sUniqueKey, sizeof(sUniqueKey), "%s__%s", pModule->sPackageAbiId, sModuleKey);
		__xlBuildModuleAbiSymbol(sOut, iOutSize, sUniqueKey, sSuffix);
		return;
	}

	__xlBuildModuleAbiSymbol(sOut, iOutSize, (pModule != NULL) ? pModule->sModuleKey : NULL, sSuffix);
}

static void __xlGetEffectiveModuleAbiSymbol(
	const XL_ModuleLinkContext* pCtx,
	const XL_ModuleInfo* pModule,
	const char* sExistingSymbol,
	const char* sSuffix,
	char* sOut,
	size_t iOutSize
)
{
	char sDefaultSymbol[XL_PATH_MAX_LEN];

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	__xlBuildModuleAbiSymbol(sDefaultSymbol, sizeof(sDefaultSymbol), (pModule != NULL) ? pModule->sModuleKey : NULL, sSuffix);
	if ( __xlModuleKeyHasCollisionInContext(pCtx, pModule) &&
		( ((sExistingSymbol == NULL) || (sExistingSymbol[0] == '\0')) || (strcmp(sExistingSymbol, sDefaultSymbol) == 0) ) ) {
		__xlBuildUniqueModuleAbiSymbol(pModule, sSuffix, sOut, iOutSize);
		return;
	}

	if ( (sExistingSymbol != NULL) && (sExistingSymbol[0] != '\0') ) {
		snprintf(sOut, iOutSize, "%s", sExistingSymbol);
		return;
	}

	snprintf(sOut, iOutSize, "%s", sDefaultSymbol);
}

static void __xlBuildPackageAbiId(
	const char* sPackageName,
	const char* sPackageVersion,
	const char* sPackageAbiVersion,
	char* sOut,
	size_t iOutSize
)
{
	snprintf(
		sOut,
		iOutSize,
		"%s@%s#abi%s",
		((sPackageName != NULL) && (sPackageName[0] != '\0')) ? sPackageName : "package",
		((sPackageVersion != NULL) && (sPackageVersion[0] != '\0')) ? sPackageVersion : "0.0.0",
		((sPackageAbiVersion != NULL) && (sPackageAbiVersion[0] != '\0')) ? sPackageAbiVersion : "1"
	);
}

static int __xlInitLocalSourcePackageIdentity(XL_ModuleLinkContext* pCtx, const char* sInputFile)
{
	if ( pCtx == NULL ) {
		return XL_RC_BAD_ARG;
	}

	if ( pCtx->bHasProject && (pCtx->sLocalPackageName[0] != '\0') ) {
		/* Project identity was already loaded from xlang.json. */
	} else {
		__xlModuleFileStem(sInputFile, pCtx->sLocalPackageName, sizeof(pCtx->sLocalPackageName));
	}
	if ( pCtx->sLocalPackageVersion[0] == '\0' ) {
		snprintf(pCtx->sLocalPackageVersion, sizeof(pCtx->sLocalPackageVersion), "0.0.0");
	}
	if ( pCtx->sLocalPackageAbiVersion[0] == '\0' ) {
		snprintf(pCtx->sLocalPackageAbiVersion, sizeof(pCtx->sLocalPackageAbiVersion), "1");
	}
	__xlBuildPackageAbiId(
		pCtx->sLocalPackageName,
		pCtx->sLocalPackageVersion,
		pCtx->sLocalPackageAbiVersion,
		pCtx->sLocalPackageAbiId,
		sizeof(pCtx->sLocalPackageAbiId)
	);
	return XL_RC_OK;
}

static const char* __xlJsonFindKey(const char* sJson, const char* sKey)
{
	char sPattern[128];

	if ( (sJson == NULL) || (sKey == NULL) || (strlen(sKey) + 4 >= sizeof(sPattern)) ) {
		return NULL;
	}
	snprintf(sPattern, sizeof(sPattern), "\"%s\"", sKey);
	return strstr(sJson, sPattern);
}

static const char* __xlJsonSkipWs(const char* sCur)
{
	while ( (sCur != NULL) && ((*sCur == ' ') || (*sCur == '\t') || (*sCur == '\r') || (*sCur == '\n')) ) {
		++sCur;
	}
	return sCur;
}

static int __xlJsonReadStringValue(const char* sJson, const char* sKey, char* sOut, size_t iOutSize)
{
	const char* sCur;
	size_t iOut;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return XL_RC_BAD_ARG;
	}
	sOut[0] = '\0';
	sCur = __xlJsonFindKey(sJson, sKey);
	if ( sCur == NULL ) {
		return XL_RC_FAIL;
	}
	sCur = strchr(sCur, ':');
	if ( sCur == NULL ) {
		return XL_RC_FAIL;
	}
	sCur = __xlJsonSkipWs(sCur + 1);
	if ( (sCur == NULL) || (*sCur != '"') ) {
		return XL_RC_FAIL;
	}
	++sCur;
	iOut = 0;
	while ( (*sCur != '\0') && (*sCur != '"') ) {
		char ch = *sCur++;
		if ( ch == '\\' && (*sCur != '\0') ) {
			ch = *sCur++;
		}
		if ( iOut + 1 < iOutSize ) {
			sOut[iOut++] = ch;
		}
	}
	sOut[iOut] = '\0';
	return (sOut[0] != '\0') ? XL_RC_OK : XL_RC_FAIL;
}

static int __xlProjectAddPath(char arrItems[XL_PROJECT_PATH_MAX][XL_PATH_MAX_LEN], size_t* piCount, const char* sRoot, const char* sValue)
{
	char sJoined[XL_PATH_MAX_LEN];
	char sNormalized[XL_PATH_MAX_LEN];
	size_t i;

	if ( (arrItems == NULL) || (piCount == NULL) || (sValue == NULL) || (sValue[0] == '\0') ) {
		return XL_RC_OK;
	}
	if ( *piCount >= XL_PROJECT_PATH_MAX ) {
		return XL_RC_OK;
	}
	if ( ((sValue[0] >= 'A') && (sValue[0] <= 'Z') && (sValue[1] == ':')) ||
		((sValue[0] >= 'a') && (sValue[0] <= 'z') && (sValue[1] == ':')) ||
		(sValue[0] == '/') ||
		(sValue[0] == '\\') ) {
		__xlModuleNormalizePath(sNormalized, sizeof(sNormalized), sValue);
	} else {
		xlPathJoin(sJoined, sizeof(sJoined), (sRoot != NULL) ? sRoot : "", sValue);
		__xlModuleNormalizePath(sNormalized, sizeof(sNormalized), sJoined);
	}
	for ( i = 0; i < *piCount; ++i ) {
		if ( strcmp(arrItems[i], sNormalized) == 0 ) {
			return XL_RC_OK;
		}
	}
	snprintf(arrItems[*piCount], XL_PATH_MAX_LEN, "%s", sNormalized);
	*piCount += 1;
	return XL_RC_OK;
}

static int __xlProjectAddPathList(
	char arrItems[XL_PROJECT_PATH_MAX][XL_PATH_MAX_LEN],
	size_t* piCount,
	const char* sPathList
)
{
	const char* pStart;
	const char* p;

	if ( (arrItems == NULL) || (piCount == NULL) || (sPathList == NULL) || (sPathList[0] == '\0') ) {
		return XL_RC_OK;
	}
	pStart = sPathList;
	for ( p = sPathList; ; ++p ) {
		int bEnd;
		int bSep;

		bEnd = (*p == '\0');
#if defined(_WIN32) || defined(_WIN64)
		bSep = (*p == ';');
#else
		bSep = (*p == ':');
#endif
		if ( bEnd || bSep ) {
			char sItem[XL_PATH_MAX_LEN];
			size_t iLen;

			iLen = (size_t)(p - pStart);
			while ( (iLen != 0) && ((*pStart == ' ') || (*pStart == '\t')) ) {
				++pStart;
				--iLen;
			}
			while ( (iLen != 0) && ((pStart[iLen - 1] == ' ') || (pStart[iLen - 1] == '\t')) ) {
				--iLen;
			}
			if ( iLen >= sizeof(sItem) ) {
				iLen = sizeof(sItem) - 1;
			}
			if ( iLen != 0 ) {
				memcpy(sItem, pStart, iLen);
				sItem[iLen] = '\0';
				(void)__xlProjectAddPath(arrItems, piCount, "", sItem);
			}
			if ( bEnd ) {
				break;
			}
			pStart = p + 1;
		}
	}
	return XL_RC_OK;
}

static void __xlProjectAddDefaultPackageRoots(XL_ModuleLinkContext* pCtx)
{
	const char* sEnv;
	const char* sHome;
	char sJoined[XL_PATH_MAX_LEN];

	if ( pCtx == NULL ) {
		return;
	}
	sEnv = getenv("XLANG_PACKAGE_PATH");
	(void)__xlProjectAddPathList(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, sEnv);

#if defined(_WIN32) || defined(_WIN64)
	sHome = getenv("USERPROFILE");
#else
	sHome = getenv("HOME");
#endif
	if ( (sHome != NULL) && (sHome[0] != '\0') ) {
		xlPathJoin(sJoined, sizeof(sJoined), sHome, ".xlang");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
		xlPathJoin(sJoined, sizeof(sJoined), sJoined, "libs");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
		xlPathJoin(sJoined, sizeof(sJoined), sJoined, "packages");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
		xlPathJoin(sJoined, sizeof(sJoined), sHome, ".xlang");
		xlPathJoin(sJoined, sizeof(sJoined), sJoined, "packages");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
	}

	sEnv = getenv("XLANG_HOME");
	if ( (sEnv != NULL) && (sEnv[0] != '\0') ) {
		xlPathJoin(sJoined, sizeof(sJoined), sEnv, "libs");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
		xlPathJoin(sJoined, sizeof(sJoined), sJoined, "packages");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
		xlPathJoin(sJoined, sizeof(sJoined), sEnv, "packages");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
		xlPathJoin(sJoined, sizeof(sJoined), sEnv, "lib");
		xlPathJoin(sJoined, sizeof(sJoined), sJoined, "xlang");
		xlPathJoin(sJoined, sizeof(sJoined), sJoined, "packages");
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
	}

#if defined(_WIN32) || defined(_WIN64)
	{
		char sAppRoot[XL_PATH_MAX_LEN];

		if ( xlAppPathGetDirectory(sAppRoot, sizeof(sAppRoot)) == XL_RC_OK ) {
			__xlModuleNormalizePath(sAppRoot, sizeof(sAppRoot), sAppRoot);
			xlPathJoin(sJoined, sizeof(sJoined), sAppRoot, "libs");
			(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
			xlPathJoin(sJoined, sizeof(sJoined), sJoined, "packages");
			(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
			xlPathJoin(sJoined, sizeof(sJoined), sAppRoot, "packages");
			(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
			xlPathJoin(sJoined, sizeof(sJoined), sAppRoot, "lib");
			xlPathJoin(sJoined, sizeof(sJoined), sJoined, "xlang");
			xlPathJoin(sJoined, sizeof(sJoined), sJoined, "packages");
			(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, "", sJoined);
		}
	}
#endif
}

static int __xlProjectAddText(char arrItems[XL_PROJECT_PATH_MAX][128], size_t* piCount, const char* sValue)
{
	if ( (arrItems == NULL) || (piCount == NULL) || (sValue == NULL) || (sValue[0] == '\0') || (*piCount >= XL_PROJECT_PATH_MAX) ) {
		return XL_RC_OK;
	}
	snprintf(arrItems[*piCount], 128, "%s", sValue);
	*piCount += 1;
	return XL_RC_OK;
}

static void __xlJsonReadStringArray(
	const char* sJson,
	const char* sKey,
	int (*fnAdd)(void* pCtx, const char* sValue),
	void* pAddCtx
)
{
	const char* sCur;

	if ( (sJson == NULL) || (sKey == NULL) || (fnAdd == NULL) ) {
		return;
	}
	sCur = __xlJsonFindKey(sJson, sKey);
	if ( sCur == NULL ) {
		return;
	}
	sCur = strchr(sCur, ':');
	if ( sCur == NULL ) {
		return;
	}
	sCur = __xlJsonSkipWs(sCur + 1);
	if ( (sCur == NULL) || (*sCur != '[') ) {
		return;
	}
	++sCur;
	while ( *sCur != '\0' ) {
		char sValue[XL_PATH_MAX_LEN];
		size_t iOut;

		sCur = __xlJsonSkipWs(sCur);
		if ( *sCur == ']' ) {
			return;
		}
		if ( *sCur != '"' ) {
			return;
		}
		++sCur;
		iOut = 0;
		while ( (*sCur != '\0') && (*sCur != '"') ) {
			char ch = *sCur++;
			if ( ch == '\\' && (*sCur != '\0') ) {
				ch = *sCur++;
			}
			if ( iOut + 1 < sizeof(sValue) ) {
				sValue[iOut++] = ch;
			}
		}
		sValue[iOut] = '\0';
		if ( *sCur == '"' ) {
			++sCur;
		}
		(void)fnAdd(pAddCtx, sValue);
		sCur = __xlJsonSkipWs(sCur);
		if ( *sCur == ',' ) {
			++sCur;
			continue;
		}
		if ( *sCur == ']' ) {
			return;
		}
	}
}

typedef struct XL_ProjectPathAddCtx {
	XL_ModuleLinkContext* pCtx;
	char (*arrItems)[XL_PATH_MAX_LEN];
	size_t* piCount;
} XL_ProjectPathAddCtx;

static int __xlProjectAddPathFromJson(void* pOpaque, const char* sValue)
{
	XL_ProjectPathAddCtx* pAdd = (XL_ProjectPathAddCtx*)pOpaque;

	return __xlProjectAddPath(pAdd->arrItems, pAdd->piCount, pAdd->pCtx->sProjectRoot, sValue);
}

typedef struct XL_ProjectTextAddCtx {
	char (*arrItems)[128];
	size_t* piCount;
} XL_ProjectTextAddCtx;

static int __xlProjectAddTextFromJson(void* pOpaque, const char* sValue)
{
	XL_ProjectTextAddCtx* pAdd = (XL_ProjectTextAddCtx*)pOpaque;

	return __xlProjectAddText(pAdd->arrItems, pAdd->piCount, sValue);
}

typedef struct XL_ProjectExportAddCtx {
	XL_ModuleLinkContext* pCtx;
} XL_ProjectExportAddCtx;

static int __xlProjectAddExportFromJson(void* pOpaque, const char* sValue)
{
	XL_ProjectExportAddCtx* pAdd = (XL_ProjectExportAddCtx*)pOpaque;

	if ( (pAdd == NULL) || (pAdd->pCtx == NULL) || (sValue == NULL) || (sValue[0] == '\0') ||
		(pAdd->pCtx->iProjectExportCount >= XL_PROJECT_PATH_MAX) ) {
		return XL_RC_OK;
	}
	snprintf(
		pAdd->pCtx->arrProjectExports[pAdd->pCtx->iProjectExportCount],
		XL_PATH_MAX_LEN,
		"%s",
		sValue
	);
	pAdd->pCtx->iProjectExportCount += 1;
	return XL_RC_OK;
}

static int __xlFindProjectFileForInput(const char* sInputFile, char* sOut, size_t iOutSize)
{
	char sDir[XL_PATH_MAX_LEN];

	if ( (sInputFile == NULL) || (sOut == NULL) || (iOutSize == 0) ) {
		return XL_RC_BAD_ARG;
	}
	sOut[0] = '\0';
	snprintf(sDir, sizeof(sDir), "%s", sInputFile);
	if ( strstr(sDir, "xlang.json") == (sDir + strlen(sDir) - strlen("xlang.json")) ) {
		__xlModuleNormalizePath(sOut, iOutSize, sDir);
		return __xlFileExists(sOut) ? XL_RC_OK : XL_RC_FAIL;
	}
	__xlModuleStripFileName(sDir);
	while ( sDir[0] != '\0' ) {
		char sCandidate[XL_PATH_MAX_LEN];
		char sPrev[XL_PATH_MAX_LEN];

		xlPathJoin(sCandidate, sizeof(sCandidate), sDir, "xlang.json");
		__xlModuleNormalizePath(sCandidate, sizeof(sCandidate), sCandidate);
		if ( __xlFileExists(sCandidate) ) {
			snprintf(sOut, iOutSize, "%s", sCandidate);
			return XL_RC_OK;
		}
		snprintf(sPrev, sizeof(sPrev), "%s", sDir);
		__xlModuleStripFileName(sDir);
		if ( strcmp(sPrev, sDir) == 0 ) {
			break;
		}
	}
	return XL_RC_FAIL;
}

static int __xlLoadProjectFile(XL_ModuleLinkContext* pCtx, const char* sProjectFile, char* sRootInputFile, size_t iRootInputFileSize)
{
	char* sJson;
	char sValue[XL_PATH_MAX_LEN];
	char sJoined[XL_PATH_MAX_LEN];
	XL_ProjectPathAddCtx pathAdd;
	XL_ProjectTextAddCtx textAdd;
	XL_ProjectExportAddCtx exportAdd;
	int iRc;

	if ( (pCtx == NULL) || (sProjectFile == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	sJson = NULL;
	iRc = __xlReadModuleFileText(sProjectFile, &sJson);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	pCtx->bHasProject = 1;
	__xlModuleNormalizePath(pCtx->sProjectFile, sizeof(pCtx->sProjectFile), sProjectFile);
	snprintf(pCtx->sProjectRoot, sizeof(pCtx->sProjectRoot), "%s", pCtx->sProjectFile);
	__xlModuleStripFileName(pCtx->sProjectRoot);
	if ( __xlJsonReadStringValue(sJson, "name", sValue, sizeof(sValue)) == XL_RC_OK ) {
		snprintf(pCtx->sLocalPackageName, sizeof(pCtx->sLocalPackageName), "%s", sValue);
	}
	if ( __xlJsonReadStringValue(sJson, "version", sValue, sizeof(sValue)) == XL_RC_OK ) {
		snprintf(pCtx->sLocalPackageVersion, sizeof(pCtx->sLocalPackageVersion), "%s", sValue);
	}
	if ( __xlJsonReadStringValue(sJson, "type", sValue, sizeof(sValue)) == XL_RC_OK ) {
		snprintf(pCtx->sProjectType, sizeof(pCtx->sProjectType), "%s", sValue);
	}
	if ( __xlJsonReadStringValue(sJson, "main", sValue, sizeof(sValue)) == XL_RC_OK ) {
		xlPathJoin(sJoined, sizeof(sJoined), pCtx->sProjectRoot, sValue);
		__xlModuleNormalizePath(pCtx->sProjectMain, sizeof(pCtx->sProjectMain), sJoined);
		if ( (sRootInputFile != NULL) && (iRootInputFileSize != 0) ) {
			snprintf(sRootInputFile, iRootInputFileSize, "%s", pCtx->sProjectMain);
		}
	}

	pathAdd.pCtx = pCtx;
	pathAdd.arrItems = pCtx->arrSourceRoots;
	pathAdd.piCount = &pCtx->iSourceRootCount;
	__xlJsonReadStringArray(sJson, "source", __xlProjectAddPathFromJson, &pathAdd);
	pathAdd.arrItems = pCtx->arrPackageRoots;
	pathAdd.piCount = &pCtx->iPackageRootCount;
	__xlJsonReadStringArray(sJson, "packages", __xlProjectAddPathFromJson, &pathAdd);
	pathAdd.arrItems = pCtx->arrProjectLibPaths;
	pathAdd.piCount = &pCtx->iProjectLibPathCount;
	__xlJsonReadStringArray(sJson, "libpath", __xlProjectAddPathFromJson, &pathAdd);
	pathAdd.arrItems = pCtx->arrProjectIncPaths;
	pathAdd.piCount = &pCtx->iProjectIncPathCount;
	__xlJsonReadStringArray(sJson, "incpath", __xlProjectAddPathFromJson, &pathAdd);
	textAdd.arrItems = pCtx->arrProjectLinks;
	textAdd.piCount = &pCtx->iProjectLinkCount;
	__xlJsonReadStringArray(sJson, "links", __xlProjectAddTextFromJson, &textAdd);
	exportAdd.pCtx = pCtx;
	__xlJsonReadStringArray(sJson, "exports", __xlProjectAddExportFromJson, &exportAdd);

	if ( pCtx->iSourceRootCount == 0 ) {
		(void)__xlProjectAddPath(pCtx->arrSourceRoots, &pCtx->iSourceRootCount, pCtx->sProjectRoot, "src");
	}
	if ( pCtx->iPackageRootCount == 0 ) {
		(void)__xlProjectAddPath(pCtx->arrPackageRoots, &pCtx->iPackageRootCount, pCtx->sProjectRoot, ".");
	}
	if ( (pCtx->sProjectMain[0] == '\0') &&
		(strcmp(pCtx->sProjectType, "package") == 0) &&
		(pCtx->iProjectExportCount != 0) &&
		(pCtx->iSourceRootCount != 0) ) {
		char sExportRel[XL_PATH_MAX_LEN];

		__xlBuildImportRelPath(sExportRel, sizeof(sExportRel), pCtx->arrProjectExports[0]);
		xlPathJoin(pCtx->sProjectMain, sizeof(pCtx->sProjectMain), pCtx->arrSourceRoots[0], sExportRel);
		__xlModuleNormalizePath(pCtx->sProjectMain, sizeof(pCtx->sProjectMain), pCtx->sProjectMain);
		if ( (sRootInputFile != NULL) && (iRootInputFileSize != 0) ) {
			snprintf(sRootInputFile, iRootInputFileSize, "%s", pCtx->sProjectMain);
		}
	}
	free(sJson);
	return XL_RC_OK;
}

static int __xlApplyProjectDirectivesToRoot(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	size_t i;
	int iRc;

	if ( (pCtx == NULL) || (pModule == NULL) || !pModule->bIsRoot || !pCtx->bHasProject ) {
		return XL_RC_OK;
	}
	for ( i = 0; i < pCtx->iProjectLinkCount; ++i ) {
		iRc = __xlModuleInfoAddLinkLib(pModule, pCtx->arrProjectLinks[i]);
		if ( iRc != XL_RC_OK ) return iRc;
	}
	for ( i = 0; i < pCtx->iProjectLibPathCount; ++i ) {
		iRc = __xlModuleInfoAddLibPath(pModule, pCtx->arrProjectLibPaths[i]);
		if ( iRc != XL_RC_OK ) return iRc;
	}
	for ( i = 0; i < pCtx->iProjectIncPathCount; ++i ) {
		iRc = __xlModuleInfoAddIncPath(pModule, pCtx->arrProjectIncPaths[i]);
		if ( iRc != XL_RC_OK ) return iRc;
	}
	return XL_RC_OK;
}

static int __xlAssignLocalSourcePackageIdentity(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	if ( (pCtx == NULL) || (pModule == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	pModule->sPackageName = __xlModuleDupText(pCtx->sLocalPackageName);
	pModule->sPackageVersion = __xlModuleDupText(pCtx->sLocalPackageVersion);
	pModule->sPackageAbiVersion = __xlModuleDupText(pCtx->sLocalPackageAbiVersion);
	pModule->sPackageAbiId = __xlModuleDupText(pCtx->sLocalPackageAbiId);
	if ( (pModule->sPackageName == NULL) || (pModule->sPackageVersion == NULL) ||
		(pModule->sPackageAbiVersion == NULL) || (pModule->sPackageAbiId == NULL) ) {
		return XL_RC_IO_ERROR;
	}

	return XL_RC_OK;
}

static int __xlAssignModuleAbiSymbols(XL_ModuleInfo* pModule, const XL_XlpResolveInfo* pXlpInfo)
{
	char sInitSymbol[XL_PATH_MAX_LEN];
	char sExportsSymbol[XL_PATH_MAX_LEN];
	char sDescriptorSymbol[XL_PATH_MAX_LEN];
	const char* sInitText;
	const char* sExportsText;
	const char* sDescriptorText;

	if ( pModule == NULL ) {
		return XL_RC_BAD_ARG;
	}

	__xlBuildModuleAbiSymbol(sInitSymbol, sizeof(sInitSymbol), pModule->sModuleKey, "init");
	__xlBuildModuleAbiSymbol(sExportsSymbol, sizeof(sExportsSymbol), pModule->sModuleKey, "exports");
	__xlBuildModuleAbiSymbol(sDescriptorSymbol, sizeof(sDescriptorSymbol), pModule->sModuleKey, "descriptor");
	sInitText = ((pXlpInfo != NULL) && (pXlpInfo->sInitSymbol[0] != '\0')) ? pXlpInfo->sInitSymbol : sInitSymbol;
	sExportsText = ((pXlpInfo != NULL) && (pXlpInfo->sExportsSymbol[0] != '\0')) ? pXlpInfo->sExportsSymbol : sExportsSymbol;
	sDescriptorText = ((pXlpInfo != NULL) && (pXlpInfo->sDescriptorSymbol[0] != '\0')) ? pXlpInfo->sDescriptorSymbol : sDescriptorSymbol;
	pModule->sInitSymbol = __xlModuleDupText(sInitText);
	pModule->sExportsSymbol = __xlModuleDupText(sExportsText);
	pModule->sDescriptorSymbol = __xlModuleDupText(sDescriptorText);
	if ( (pModule->sInitSymbol == NULL) || (pModule->sExportsSymbol == NULL) || (pModule->sDescriptorSymbol == NULL) ) {
		return XL_RC_IO_ERROR;
	}
	return XL_RC_OK;
}

static void __xlBuildXlpNativeVirtualPath(char* sOut, size_t iOutSize, const char* sManifestPath, const char* sModuleKey)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];

	__xlSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sModuleKey);
	snprintf(sOut, iOutSize, "<xlp-native:%s:%s>", (sManifestPath != NULL) ? sManifestPath : "", sModuleKeySafe);
}

static void __xlBuildXlpNativeHelperName(char* sOut, size_t iOutSize, const char* sModuleKey, size_t iFuncIndex)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];

	__xlSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sModuleKey);
	snprintf(sOut, iOutSize, "__xlp_extern__%s__%llu", sModuleKeySafe, (unsigned long long)iFuncIndex);
}

static char* __xlBuildXlpNativeSource(const char* sModuleKey, const XL_XlpFunctionInfo* pFuncs, size_t iFuncCount)
{
	XL_ModuleTextBuffer buf;
	size_t i;
	size_t j;
	int bHasExport;

	memset(&buf, 0, sizeof(buf));
	bHasExport = 0;
	for ( i = 0; i < iFuncCount; ++i ) {
		char sHelperName[256];
		const XL_XlpFunctionInfo* pFunc;

		pFunc = &pFuncs[i];
		if ( pFunc->bAsync || pFunc->bGenerator ) {
			continue;
		}
		if ( (pFunc->sExportName[0] == '\0') || (pFunc->sSymbol[0] == '\0') ) {
			continue;
		}

		__xlBuildXlpNativeHelperName(sHelperName, sizeof(sHelperName), sModuleKey, i);
		__xlModuleBufAppendFormat(&buf, "func %s(", sHelperName);
		for ( j = 0; j < pFunc->iParamCount; ++j ) {
			if ( j != 0 ) {
				__xlModuleBufAppend(&buf, ", ");
			}
			__xlModuleBufAppendFormat(&buf, "%s %s", (pFunc->pParams[j].sType[0] != '\0') ? pFunc->pParams[j].sType : "int",
				(pFunc->pParams[j].sName[0] != '\0') ? pFunc->pParams[j].sName : "arg");
		}
		__xlModuleBufAppend(&buf, ")");
		if ( pFunc->sReturnType[0] != '\0' ) {
			__xlModuleBufAppendFormat(&buf, " as %s", pFunc->sReturnType);
		}
		__xlModuleBufAppend(&buf, "\nend\n");

		__xlModuleBufAppendFormat(&buf, "func %s(", pFunc->sExportName);
		for ( j = 0; j < pFunc->iParamCount; ++j ) {
			if ( j != 0 ) {
				__xlModuleBufAppend(&buf, ", ");
			}
			__xlModuleBufAppendFormat(&buf, "%s %s", (pFunc->pParams[j].sType[0] != '\0') ? pFunc->pParams[j].sType : "int",
				(pFunc->pParams[j].sName[0] != '\0') ? pFunc->pParams[j].sName : "arg");
		}
		__xlModuleBufAppend(&buf, ")");
		if ( pFunc->sReturnType[0] != '\0' ) {
			__xlModuleBufAppendFormat(&buf, " as %s", pFunc->sReturnType);
		}
		__xlModuleBufAppend(&buf, "\n");
		if ( (pFunc->sReturnType[0] == '\0') || (strcmp(pFunc->sReturnType, "void") == 0) ) {
			__xlModuleBufAppend(&buf, "\t");
			__xlModuleBufAppendFormat(&buf, "%s(", sHelperName);
		} else {
			__xlModuleBufAppend(&buf, "\treturn ");
			__xlModuleBufAppendFormat(&buf, "%s(", sHelperName);
		}
		for ( j = 0; j < pFunc->iParamCount; ++j ) {
			if ( j != 0 ) {
				__xlModuleBufAppend(&buf, ", ");
			}
			__xlModuleBufAppend(&buf, (pFunc->pParams[j].sName[0] != '\0') ? pFunc->pParams[j].sName : "arg");
		}
		__xlModuleBufAppend(&buf, ")\nend\n");

		if ( (i == 0) || (strcmp(pFunc->sExportName, pFuncs[i - 1].sExportName) != 0) ) {
			__xlModuleBufAppendFormat(&buf, "export %s\n", pFunc->sExportName);
			bHasExport = 1;
		}
	}

	if ( !bHasExport ) {
		free(buf.sData);
		return NULL;
	}
	return buf.sData;
}

static void __xlAnnotateXlpNativeExterns(XL_AstNode* pRoot, const char* sModuleKey, const XL_XlpFunctionInfo* pFuncs, size_t iFuncCount)
{
	size_t i;
	size_t j;

	if ( (pRoot == NULL) || (pFuncs == NULL) ) {
		return;
	}

	for ( i = 0; i < iFuncCount; ++i ) {
		char sHelperName[256];

		if ( pFuncs[i].sSymbol[0] == '\0' ) {
			continue;
		}
		__xlBuildXlpNativeHelperName(sHelperName, sizeof(sHelperName), sModuleKey, i);
		for ( j = 0; j < pRoot->iChildCount; ++j ) {
			XL_AstNode* pStmt;

			pStmt = pRoot->ppChildren[j];
			if ( (pStmt == NULL) || (pStmt->eKind != XL_AST_FUNC_DECL) || (pStmt->sText == NULL) ) {
				continue;
			}
			if ( strcmp(pStmt->sText, sHelperName) == 0 ) {
				xlAstSetText2(pStmt, pFuncs[i].sSymbol, strlen(pFuncs[i].sSymbol));
				pStmt->iIntValue = 1;
				break;
			}
		}
	}
}

static void __xlAppendImportSearchPath(XL_ModuleTextBuffer* pBuf, const char* sPath)
{
	if ( (pBuf == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		return;
	}
	if ( pBuf->iLen != 0 ) {
		__xlModuleBufAppend(pBuf, ", ");
	}
	__xlModuleBufAppend(pBuf, sPath);
}

static void __xlAppendXlpPackageSearchRoots(XL_ModuleTextBuffer* pBuf, const char* sSearchStart)
{
	char sSearchDir[XL_PATH_MAX_LEN];

	if ( (pBuf == NULL) || (sSearchStart == NULL) || (sSearchStart[0] == '\0') ) {
		return;
	}

	snprintf(sSearchDir, sizeof(sSearchDir), "%s", sSearchStart);
	__xlModuleStripFileName(sSearchDir);
	while ( sSearchDir[0] != '\0' ) {
		char sPackageDir[XL_PATH_MAX_LEN];
		char sLibDir[XL_PATH_MAX_LEN];
		char sLibPackageDir[XL_PATH_MAX_LEN];
		char sPrevDir[XL_PATH_MAX_LEN];

		snprintf(sPrevDir, sizeof(sPrevDir), "%s", sSearchDir);
		xlPathJoin(sLibDir, sizeof(sLibDir), sSearchDir, "libs");
		__xlAppendImportSearchPath(pBuf, sLibDir);
		xlPathJoin(sLibPackageDir, sizeof(sLibPackageDir), sLibDir, "packages");
		__xlAppendImportSearchPath(pBuf, sLibPackageDir);
		xlPathJoin(sPackageDir, sizeof(sPackageDir), sSearchDir, "packages");
		__xlAppendImportSearchPath(pBuf, sPackageDir);
		__xlModuleStripFileName(sSearchDir);
		if ( strcmp(sPrevDir, sSearchDir) == 0 ) {
			break;
		}
	}
}

static void __xlEmitImportNotFoundDiag(
	XL_ModuleLinkContext* pCtx,
	const char* sImportKey,
	const char* sLocalCandidateFile,
	const char* sImporterManifestPath
)
{
	XL_ModuleTextBuffer paths;
	size_t i;

	if ( (pCtx == NULL) || (pCtx->pDiag == NULL) ) {
		return;
	}

	memset(&paths, 0, sizeof(paths));
	__xlAppendImportSearchPath(&paths, sLocalCandidateFile);
	if ( pCtx->bHasProject ) {
		for ( i = 0; i < pCtx->iSourceRootCount; ++i ) {
			__xlAppendImportSearchPath(&paths, pCtx->arrSourceRoots[i]);
		}
	}
	for ( i = 0; i < pCtx->iPackageRootCount; ++i ) {
		__xlAppendImportSearchPath(&paths, pCtx->arrPackageRoots[i]);
	}
	if ( (sImporterManifestPath != NULL) && (sImporterManifestPath[0] != '\0') ) {
		__xlAppendXlpPackageSearchRoots(&paths, sImporterManifestPath);
	}
	__xlAppendXlpPackageSearchRoots(&paths, sLocalCandidateFile);
	__xlAppendImportSearchPath(&paths, "native registry");

	XL_DIAG_ERROR(
		pCtx->pDiag,
		"module",
		"import not found: import='%s'; local candidate='%s'; importer manifest='%s'; search paths=[%s]",
		((sImportKey != NULL) && (sImportKey[0] != '\0')) ? sImportKey : "?",
		((sLocalCandidateFile != NULL) && (sLocalCandidateFile[0] != '\0')) ? sLocalCandidateFile : "?",
		((sImporterManifestPath != NULL) && (sImporterManifestPath[0] != '\0')) ? sImporterManifestPath : "none",
		(paths.sData != NULL) ? paths.sData : "none"
	);
	free(paths.sData);
}

static int __xlFindXlpModuleForImport(
	XL_ModuleLinkContext* pCtx,
	const char* sLocalCandidateFile,
	const char* sImporterManifestPath,
	const char* sImportKey,
	XL_XlpResolveInfo* pInfo
)
{
	const char* arrRoots[XL_PROJECT_PATH_MAX];
	size_t i;

	if ( (pCtx != NULL) && (pCtx->iPackageRootCount != 0) ) {
		for ( i = 0; i < pCtx->iPackageRootCount; ++i ) {
			arrRoots[i] = pCtx->arrPackageRoots[i];
		}
		if ( xlXlpFindModuleSourceInRoots(arrRoots, pCtx->iPackageRootCount, sImporterManifestPath, sImportKey, pInfo) == XL_RC_OK ) {
			return XL_RC_OK;
		}
	}
	return xlXlpFindModuleSource(sLocalCandidateFile, sImporterManifestPath, sImportKey, pInfo);
}

static int __xlIsStdModuleKey(const char* sModuleKey)
{
	return (sModuleKey != NULL) &&
		(strncmp(sModuleKey, "std.", 4) == 0) &&
		(sModuleKey[4] != '\0');
}

static int __xlLoadModule(
	XL_ModuleLinkContext* pCtx,
	const char* sFilePath,
	const char* sModuleKey,
	int bIsRoot,
	const char* sImporterManifestPath,
	XL_ModuleInfo** ppModule
)
{
	XL_ModuleInfo* pModule;
	XL_ModuleInfo* pImportedModule;
	XL_NativeModuleInfo nativeInfo;
	char sImportFile[XL_PATH_MAX_LEN];
	char sCurrentModuleFile[XL_PATH_MAX_LEN];
	char sNativeVirtualPath[XL_PATH_MAX_LEN];
	XL_XlpResolveInfo xlpInfo;
	XL_XlpFunctionInfo* pXlpFuncs;
	size_t iXlpFuncCount;
	char* sImportKey;
	size_t i;
	int iRc;

	pModule = __xlFindModuleByFile(pCtx, sFilePath);
	if ( (pModule == NULL) && !bIsRoot && (sModuleKey != NULL) && !__xlFileExists(sFilePath) ) {
		xlXlpResolveInfoInit(&xlpInfo);
		if ( __xlFindXlpModuleForImport(pCtx, sFilePath, sImporterManifestPath, sModuleKey, &xlpInfo) == XL_RC_OK ) {
			if ( xlpInfo.bIsNative ) {
				pXlpFuncs = NULL;
				iXlpFuncCount = 0;
				if ( xlXlpReadFunctions(xlpInfo.sManifestPath, sModuleKey, &pXlpFuncs, &iXlpFuncCount) == XL_RC_OK && (iXlpFuncCount != 0) ) {
					__xlBuildXlpNativeVirtualPath(sNativeVirtualPath, sizeof(sNativeVirtualPath), xlpInfo.sManifestPath, sModuleKey);
					pModule = __xlFindModuleByFile(pCtx, sNativeVirtualPath);
				} else {
					const char* sNativeKey;

					sNativeKey = (xlpInfo.sBindingName[0] != '\0') ? xlpInfo.sBindingName : sModuleKey;
					memset(&nativeInfo, 0, sizeof(nativeInfo));
					if ( xlNativeRegistryFind(sNativeKey, &nativeInfo) == XL_RC_OK ) {
						pModule = __xlFindModuleByFile(pCtx, nativeInfo.sVirtualPath);
					}
				}
				xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
			} else {
				pModule = __xlFindModuleByFile(pCtx, xlpInfo.sModuleFile);
			}
		}
		if ( pModule == NULL ) {
			memset(&nativeInfo, 0, sizeof(nativeInfo));
			if ( xlNativeRegistryFind(sModuleKey, &nativeInfo) == XL_RC_OK ) {
				pModule = __xlFindModuleByFile(pCtx, nativeInfo.sVirtualPath);
			}
		}
	}
	if ( pModule != NULL ) {
		if ( pModule->iState == 1 ) {
			XL_DIAG_ERROR(pCtx->pDiag, "module", "cyclic import detected: %s", sFilePath);
			return XL_RC_FAIL;
		}
		*ppModule = pModule;
		return XL_RC_OK;
	}

	pModule = __xlAddModuleRecord(pCtx);
	if ( pModule == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pModule->sModuleKey = __xlModuleDupText((sModuleKey != NULL) ? sModuleKey : "");
	pModule->bIsRoot = bIsRoot;
	pModule->iState = 1;
	if ( (!bIsRoot) && (sModuleKey != NULL) ) {
		const char* sDot;

		sDot = strrchr(sModuleKey, '.');
		pModule->sLeafAlias = __xlModuleDupText((sDot != NULL) ? (sDot + 1) : sModuleKey);
	}

	memset(&nativeInfo, 0, sizeof(nativeInfo));
	xlXlpResolveInfoInit(&xlpInfo);
	pXlpFuncs = NULL;
	iXlpFuncCount = 0;
	if ( (!bIsRoot) && __xlIsStdModuleKey(sModuleKey) && (xlNativeRegistryFind(sModuleKey, &nativeInfo) == XL_RC_OK) ) {
		pModule->bIsNative = 1;
		pModule->sFilePath = __xlModuleDupText(nativeInfo.sVirtualPath);
		pModule->sBindingName = __xlModuleDupText(nativeInfo.sBindingName);
		iRc = __xlParseModuleSourceText(nativeInfo.sSourceText, nativeInfo.sVirtualPath, pModule, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
	} else if ( bIsRoot || __xlFileExists(sFilePath) ) {
		pModule->sFilePath = __xlModuleDupText(sFilePath);
		iRc = __xlApplyProjectDirectivesToRoot(pCtx, pModule);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		iRc = __xlParseModuleFileToAst(sFilePath, pModule, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
		if ( iRc == XL_RC_OK ) {
			iRc = __xlAssignLocalSourcePackageIdentity(pCtx, pModule);
		}
		if ( iRc == XL_RC_OK ) {
			iRc = __xlAssignModuleAbiSymbols(pModule, NULL);
		}
	} else if ( (sModuleKey != NULL) &&
		(__xlFindXlpModuleForImport(pCtx, sFilePath, sImporterManifestPath, sModuleKey, &xlpInfo) == XL_RC_OK) ) {
		pModule->sManifestPath = __xlModuleDupText(xlpInfo.sManifestPath);
		pModule->sPackageName = __xlModuleDupText(xlpInfo.sPackageName);
		pModule->sPackageVersion = __xlModuleDupText(xlpInfo.sPackageVersion);
		pModule->sPackageAbiVersion = __xlModuleDupText(xlpInfo.sPackageAbiVersion);
		pModule->sPackageAbiId = __xlModuleDupText(xlpInfo.sPackageAbiId);
		pModule->sArtifactKind = __xlModuleDupText(xlpInfo.sArtifactKind);
		pModule->sArtifactPath = __xlModuleDupText(xlpInfo.sArtifactPath);
		pModule->sArtifactFile = __xlModuleDupText(xlpInfo.sArtifactFile);
		pModule->sConsumeMode = __xlModuleDupText(xlpInfo.sConsumeMode);
		if ( (pModule->sManifestPath == NULL) || (pModule->sPackageName == NULL) || (pModule->sPackageVersion == NULL) ||
			(pModule->sPackageAbiVersion == NULL) || (pModule->sPackageAbiId == NULL) ||
			(pModule->sArtifactKind == NULL) || (pModule->sArtifactPath == NULL) ||
			(pModule->sArtifactFile == NULL) || (pModule->sConsumeMode == NULL) ) {
			return XL_RC_IO_ERROR;
		}
		iRc = __xlAssignModuleAbiSymbols(pModule, &xlpInfo);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		if ( xlpInfo.bIsNative ) {
			if ( (strcmp(xlpInfo.sConsumeMode, "native-registry") != 0) &&
				(strcmp(xlpInfo.sConsumeMode, "shared-bridge") != 0) &&
				(strcmp(xlpInfo.sConsumeMode, "static-bridge") != 0) ) {
				XL_DIAG_ERROR(pCtx->pDiag, "module", "unsupported xlp native consume mode: %s", xlpInfo.sConsumeMode);
				return XL_RC_IO_ERROR;
			}
			pModule->bIsNative = 1;
			pModule->sBindingName = __xlModuleDupText(xlpInfo.sBindingName);
			iRc = xlXlpReadFunctions(xlpInfo.sManifestPath, sModuleKey, &pXlpFuncs, &iXlpFuncCount);
			if ( (iRc == XL_RC_OK) && (iXlpFuncCount != 0) ) {
				char* sNativeSource;

				__xlBuildXlpNativeVirtualPath(sNativeVirtualPath, sizeof(sNativeVirtualPath), xlpInfo.sManifestPath, sModuleKey);
				pModule->sFilePath = __xlModuleDupText(sNativeVirtualPath);
				sNativeSource = __xlBuildXlpNativeSource(sModuleKey, pXlpFuncs, iXlpFuncCount);
				if ( sNativeSource == NULL ) {
					xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
					return XL_RC_IO_ERROR;
				}
				iRc = __xlParseModuleSourceText(sNativeSource, sNativeVirtualPath, pModule, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
				if ( iRc == XL_RC_OK ) {
					__xlAnnotateXlpNativeExterns(pModule->pRoot, sModuleKey, pXlpFuncs, iXlpFuncCount);
				}
				free(sNativeSource);
			} else {
				const char* sNativeKey;

				sNativeKey = (xlpInfo.sBindingName[0] != '\0') ? xlpInfo.sBindingName : sModuleKey;
				if ( xlNativeRegistryFind(sNativeKey, &nativeInfo) != XL_RC_OK ) {
					xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
					XL_DIAG_ERROR(pCtx->pDiag, "module", "xlp native module not found in registry: %s", sNativeKey);
					return XL_RC_IO_ERROR;
				}
				pModule->sFilePath = __xlModuleDupText(nativeInfo.sVirtualPath);
				free(pModule->sBindingName);
				pModule->sBindingName = __xlModuleDupText(nativeInfo.sBindingName);
				iRc = __xlParseModuleSourceText(nativeInfo.sSourceText, nativeInfo.sVirtualPath, pModule, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
			}
			xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
		} else {
			pModule->sFilePath = __xlModuleDupText(xlpInfo.sModuleFile);
			pModule->sBindingName = __xlModuleDupText(xlpInfo.sBindingName);
			iRc = __xlParseModuleFileToAst(xlpInfo.sModuleFile, pModule, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
		}
	} else if ( (sModuleKey != NULL) && (xlNativeRegistryFind(sModuleKey, &nativeInfo) == XL_RC_OK) ) {
		pModule->bIsNative = 1;
		pModule->sFilePath = __xlModuleDupText(nativeInfo.sVirtualPath);
		pModule->sBindingName = __xlModuleDupText(nativeInfo.sBindingName);
		iRc = __xlParseModuleSourceText(nativeInfo.sSourceText, nativeInfo.sVirtualPath, pModule, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
	} else {
		__xlEmitImportNotFoundDiag(pCtx, sModuleKey, sFilePath, sImporterManifestPath);
		return XL_RC_IO_ERROR;
	}
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	snprintf(sCurrentModuleFile, sizeof(sCurrentModuleFile), "%s", (pModule->sFilePath != NULL) ? pModule->sFilePath : "");
	if ( pCtx->bTraceParser && (pCtx->pDiag != NULL) ) {
		__xlModuleTrace(
			pCtx->pDiag,
			"load module key='%s' file='%s' native=%d binding='%s' artifactKind='%s' consumeMode='%s'",
			pModule->sModuleKey,
			pModule->sFilePath,
			pModule->bIsNative,
			(pModule->sBindingName != NULL) ? pModule->sBindingName : "",
			(pModule->sArtifactKind != NULL) ? pModule->sArtifactKind : "",
			(pModule->sConsumeMode != NULL) ? pModule->sConsumeMode : ""
		);
	}

	iRc = __xlCollectImports(pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		sImportKey = pModule->arrImportKeys[i];
		iRc = __xlResolveImportFileWithProject(pCtx, sImportFile, sizeof(sImportFile), pModule->sFilePath, sImportKey);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		pImportedModule = NULL;
		iRc = __xlLoadModule(pCtx, sImportFile, sImportKey, 0, pModule->sManifestPath, &pImportedModule);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		pModule = __xlFindModuleByFile(pCtx, sCurrentModuleFile);
		if ( pModule == NULL ) {
			return XL_RC_FAIL;
		}
		iRc = __xlModuleInfoResolveImport(pModule, i, (pImportedModule != NULL) ? pImportedModule->sFilePath : NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	iRc = __xlLinkAndRewriteModule(pCtx, pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	pModule->iState = 2;
	iRc = __xlAppendOrder(pCtx, pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	*ppModule = pModule;
	return XL_RC_OK;
}

static void __xlDetachRootChildren(XL_AstNode* pRoot)
{
	if ( pRoot == NULL ) {
		return;
	}

	free(pRoot->ppChildren);
	pRoot->ppChildren = NULL;
	pRoot->iChildCount = 0;
	pRoot->iChildCapacity = 0;
}

static int __xlBuildLinkedRoot(XL_ModuleLinkContext* pCtx, XL_AstNode** ppRoot)
{
	XL_AstNode* pProgram;
	XL_ModuleInfo* pModule;
	XL_AstNode* pStmt;
	size_t i;
	size_t j;

	pProgram = xlAstCreateProgram();
	if ( pProgram == NULL ) {
		return XL_RC_IO_ERROR;
	}

	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		for ( j = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (j < pModule->pRoot->iChildCount); ++j ) {
			pStmt = pModule->pRoot->ppChildren[j];
			if ( (pStmt == NULL) || ((pStmt->eKind == XL_AST_IMPORT) && !pModule->bIsRoot) || (pStmt->eKind == XL_AST_EXPORT) ) {
				pModule->pRoot->ppChildren[j] = NULL;
				continue;
			}
			xlAstAppendChild(pProgram, pStmt);
			pModule->pRoot->ppChildren[j] = NULL;
		}
	}

	*ppRoot = pProgram;
	return XL_RC_OK;
}

static void __xlBuildModuleTextDump(const XL_ModuleLinkContext* pCtx, char** psText)
{
	XL_ModuleTextBuffer buf;
	XL_ModuleInfo* pModule;
	char* sNativeRegistryText;
	char sInitSymbol[XL_PATH_MAX_LEN];
	char sExportsSymbol[XL_PATH_MAX_LEN];
	char sDescriptorSymbol[XL_PATH_MAX_LEN];
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlModuleBufAppend(&buf, "ModuleLoadOrder\n");
	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		__xlModuleBufAppendFormat(
			&buf,
			"- [%zu] %s -> %s native=%d binding=%s\n",
			i,
			pModule->sModuleKey,
			pModule->sFilePath,
			pModule->bIsNative,
			(pModule->sBindingName != NULL) ? pModule->sBindingName : ""
		);
	}
	__xlModuleBufAppend(&buf, "ModuleExports\n");
	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		__xlGetEffectiveModuleAbiSymbol(pCtx, pModule, pModule->sInitSymbol, "init", sInitSymbol, sizeof(sInitSymbol));
		__xlGetEffectiveModuleAbiSymbol(pCtx, pModule, pModule->sExportsSymbol, "exports", sExportsSymbol, sizeof(sExportsSymbol));
		__xlGetEffectiveModuleAbiSymbol(pCtx, pModule, pModule->sDescriptorSymbol, "descriptor", sDescriptorSymbol, sizeof(sDescriptorSymbol));
		__xlModuleBufAppendFormat(&buf, "- module=%s\n", pModule->sModuleKey);
		__xlModuleBufAppendFormat(
			&buf,
			"  abi init=%s exports=%s descriptor=%s\n",
			sInitSymbol,
			sExportsSymbol,
			sDescriptorSymbol
		);
		for ( j = 0; j < pModule->iLinkLibCount; ++j ) {
			__xlModuleBufAppendFormat(&buf, "  link %s\n", pModule->arrLinkLibs[j]);
		}
		for ( j = 0; j < pModule->iLibPathCount; ++j ) {
			__xlModuleBufAppendFormat(&buf, "  libpath %s\n", pModule->arrLibPaths[j]);
		}
		for ( j = 0; j < pModule->iIncPathCount; ++j ) {
			__xlModuleBufAppendFormat(&buf, "  incpath %s\n", pModule->arrIncPaths[j]);
		}
		for ( j = 0; j < pModule->iLoadLibCount; ++j ) {
			__xlModuleBufAppendFormat(&buf, "  loadlib %s\n", pModule->arrLoadLibs[j]);
		}
		for ( j = 0; j < pModule->iExportCount; ++j ) {
			__xlModuleBufAppendFormat(&buf, "  export %s -> %s\n", pModule->arrExports[j].sName, pModule->arrExports[j].sMangledName);
		}
	}
	sNativeRegistryText = xlNativeRegistryDumpText();
	if ( sNativeRegistryText != NULL ) {
		__xlModuleBufAppend(&buf, sNativeRegistryText);
		free(sNativeRegistryText);
	}
	*psText = buf.sData;
}

static void __xlBuildModuleJsonDump(const XL_ModuleLinkContext* pCtx, char** psJson)
{
	XL_ModuleTextBuffer buf;
	XL_ModuleInfo* pModule;
	char* sNativeRegistryJson;
	char sInitSymbol[XL_PATH_MAX_LEN];
	char sExportsSymbol[XL_PATH_MAX_LEN];
	char sDescriptorSymbol[XL_PATH_MAX_LEN];
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlModuleBufAppend(&buf, "{\n\t\"modules\": [\n");
	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		__xlGetEffectiveModuleAbiSymbol(pCtx, pModule, pModule->sInitSymbol, "init", sInitSymbol, sizeof(sInitSymbol));
		__xlGetEffectiveModuleAbiSymbol(pCtx, pModule, pModule->sExportsSymbol, "exports", sExportsSymbol, sizeof(sExportsSymbol));
		__xlGetEffectiveModuleAbiSymbol(pCtx, pModule, pModule->sDescriptorSymbol, "descriptor", sDescriptorSymbol, sizeof(sDescriptorSymbol));
		__xlModuleBufAppend(&buf, "\t\t{\n");
		__xlModuleBufAppend(&buf, "\t\t\t\"key\": \"");
		__xlModuleBufAppendEscaped(&buf, pModule->sModuleKey);
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"file\": \"");
		__xlModuleBufAppendEscaped(&buf, pModule->sFilePath);
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"native\": ");
		__xlModuleBufAppend(&buf, pModule->bIsNative ? "1" : "0");
		__xlModuleBufAppend(&buf, ",\n\t\t\t\"binding\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sBindingName != NULL) ? pModule->sBindingName : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"artifactKind\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sArtifactKind != NULL) ? pModule->sArtifactKind : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"artifactPath\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sArtifactPath != NULL) ? pModule->sArtifactPath : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"artifactFile\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sArtifactFile != NULL) ? pModule->sArtifactFile : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"consumeMode\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sConsumeMode != NULL) ? pModule->sConsumeMode : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"initSymbol\": \"");
		__xlModuleBufAppendEscaped(&buf, sInitSymbol);
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"exportsSymbol\": \"");
		__xlModuleBufAppendEscaped(&buf, sExportsSymbol);
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"descriptorSymbol\": \"");
		__xlModuleBufAppendEscaped(&buf, sDescriptorSymbol);
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"linkLibs\": [");
		if ( pModule->iLinkLibCount != 0 ) {
			__xlModuleBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iLinkLibCount; ++j ) {
			__xlModuleBufAppend(&buf, "\t\t\t\t\"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrLinkLibs[j]);
			__xlModuleBufAppend(&buf, "\"");
			if ( (j + 1) != pModule->iLinkLibCount ) {
				__xlModuleBufAppend(&buf, ",");
			}
			__xlModuleBufAppend(&buf, "\n");
		}
		if ( pModule->iLinkLibCount != 0 ) {
			__xlModuleBufAppend(&buf, "\t\t\t");
		}
		__xlModuleBufAppend(&buf, "],\n\t\t\t\"libPaths\": [");
		if ( pModule->iLibPathCount != 0 ) {
			__xlModuleBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iLibPathCount; ++j ) {
			__xlModuleBufAppend(&buf, "\t\t\t\t\"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrLibPaths[j]);
			__xlModuleBufAppend(&buf, "\"");
			if ( (j + 1) != pModule->iLibPathCount ) {
				__xlModuleBufAppend(&buf, ",");
			}
			__xlModuleBufAppend(&buf, "\n");
		}
		if ( pModule->iLibPathCount != 0 ) {
			__xlModuleBufAppend(&buf, "\t\t\t");
		}
		__xlModuleBufAppend(&buf, "],\n\t\t\t\"incPaths\": [");
		if ( pModule->iIncPathCount != 0 ) {
			__xlModuleBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iIncPathCount; ++j ) {
			__xlModuleBufAppend(&buf, "\t\t\t\t\"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrIncPaths[j]);
			__xlModuleBufAppend(&buf, "\"");
			if ( (j + 1) != pModule->iIncPathCount ) {
				__xlModuleBufAppend(&buf, ",");
			}
			__xlModuleBufAppend(&buf, "\n");
		}
		if ( pModule->iIncPathCount != 0 ) {
			__xlModuleBufAppend(&buf, "\t\t\t");
		}
		__xlModuleBufAppend(&buf, "],\n\t\t\t\"loadLibs\": [");
		if ( pModule->iLoadLibCount != 0 ) {
			__xlModuleBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iLoadLibCount; ++j ) {
			__xlModuleBufAppend(&buf, "\t\t\t\t\"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrLoadLibs[j]);
			__xlModuleBufAppend(&buf, "\"");
			if ( (j + 1) != pModule->iLoadLibCount ) {
				__xlModuleBufAppend(&buf, ",");
			}
			__xlModuleBufAppend(&buf, "\n");
		}
		if ( pModule->iLoadLibCount != 0 ) {
			__xlModuleBufAppend(&buf, "\t\t\t");
		}
		__xlModuleBufAppend(&buf, "],\n\t\t\t\"exports\": [");
		if ( pModule->iExportCount != 0 ) {
			__xlModuleBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iExportCount; ++j ) {
			__xlModuleBufAppend(&buf, "\t\t\t\t{\n\t\t\t\t\t\"name\": \"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrExports[j].sName);
			__xlModuleBufAppend(&buf, "\",\n\t\t\t\t\t\"symbol\": \"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrExports[j].sMangledName);
			__xlModuleBufAppend(&buf, "\"\n\t\t\t\t}");
			if ( (j + 1) != pModule->iExportCount ) {
				__xlModuleBufAppend(&buf, ",");
			}
			__xlModuleBufAppend(&buf, "\n");
		}
		if ( pModule->iExportCount != 0 ) {
			__xlModuleBufAppend(&buf, "\t\t\t");
		}
		__xlModuleBufAppend(&buf, "]\n\t\t}");
		if ( (i + 1) != pCtx->iOrderCount ) {
			__xlModuleBufAppend(&buf, ",");
		}
		__xlModuleBufAppend(&buf, "\n");
	}
	__xlModuleBufAppend(&buf, "\t]");
	sNativeRegistryJson = xlNativeRegistryDumpJson();
	if ( sNativeRegistryJson != NULL ) {
		__xlModuleBufAppend(&buf, ",\n\t\"registry\": ");
		__xlModuleBufAppend(&buf, sNativeRegistryJson);
		free(sNativeRegistryJson);
		__xlModuleBufAppend(&buf, "\n");
	} else {
		__xlModuleBufAppend(&buf, "\n");
	}
	__xlModuleBufAppend(&buf, "}\n");
	*psJson = buf.sData;
}

static int __xlCopyModuleSummaries(const XL_ModuleLinkContext* pCtx, XL_ModuleLinkResult* pResult)
{
	size_t i;
	size_t j;

	if ( (pCtx == NULL) || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( pCtx->iOrderCount == 0 ) {
		return XL_RC_OK;
	}

	pResult->pModules = (XL_ModuleSummary*)calloc(pCtx->iOrderCount, sizeof(*pResult->pModules));
	if ( pResult->pModules == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pResult->iModuleCount = pCtx->iOrderCount;

	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		XL_ModuleInfo* pModule;
		XL_ModuleSummary* pSummary;

		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		pSummary = &pResult->pModules[i];
		pSummary->sKey = __xlModuleDupText((pModule != NULL) ? pModule->sModuleKey : "");
		pSummary->sFilePath = __xlModuleDupText((pModule != NULL) ? pModule->sFilePath : "");
		pSummary->sBindingName = __xlModuleDupText((pModule != NULL) ? pModule->sBindingName : "");
		pSummary->sInitSymbol = __xlModuleDupText((pModule != NULL) ? pModule->sInitSymbol : "");
		pSummary->sExportsSymbol = __xlModuleDupText((pModule != NULL) ? pModule->sExportsSymbol : "");
		pSummary->sDescriptorSymbol = __xlModuleDupText((pModule != NULL) ? pModule->sDescriptorSymbol : "");
		pSummary->sManifestPath = __xlModuleDupText((pModule != NULL) ? pModule->sManifestPath : "");
		pSummary->sPackageName = __xlModuleDupText((pModule != NULL) ? pModule->sPackageName : "");
		pSummary->sPackageVersion = __xlModuleDupText((pModule != NULL) ? pModule->sPackageVersion : "");
		pSummary->sPackageAbiVersion = __xlModuleDupText((pModule != NULL) ? pModule->sPackageAbiVersion : "");
		pSummary->sPackageAbiId = __xlModuleDupText((pModule != NULL) ? pModule->sPackageAbiId : "");
		pSummary->sArtifactKind = __xlModuleDupText((pModule != NULL) ? pModule->sArtifactKind : "");
		pSummary->sArtifactPath = __xlModuleDupText((pModule != NULL) ? pModule->sArtifactPath : "");
		pSummary->sArtifactFile = __xlModuleDupText((pModule != NULL) ? pModule->sArtifactFile : "");
		pSummary->sConsumeMode = __xlModuleDupText((pModule != NULL) ? pModule->sConsumeMode : "");
		pSummary->bIsRoot = (pModule != NULL) ? pModule->bIsRoot : 0;
		pSummary->bIsNative = (pModule != NULL) ? pModule->bIsNative : 0;
		pSummary->iImportCount = (pModule != NULL) ? pModule->iImportCount : 0;
		pSummary->iLinkLibCount = (pModule != NULL) ? pModule->iLinkLibCount : 0;
		pSummary->iLibPathCount = (pModule != NULL) ? pModule->iLibPathCount : 0;
		pSummary->iIncPathCount = (pModule != NULL) ? pModule->iIncPathCount : 0;
		pSummary->iLoadLibCount = (pModule != NULL) ? pModule->iLoadLibCount : 0;
		pSummary->iExportCount = (pModule != NULL) ? pModule->iExportCount : 0;
		if ( (pSummary->sKey == NULL) || (pSummary->sFilePath == NULL) || (pSummary->sBindingName == NULL) ||
			(pSummary->sInitSymbol == NULL) || (pSummary->sExportsSymbol == NULL) || (pSummary->sDescriptorSymbol == NULL) ||
			(pSummary->sManifestPath == NULL) || (pSummary->sPackageName == NULL) || (pSummary->sPackageVersion == NULL) ||
			(pSummary->sPackageAbiVersion == NULL) || (pSummary->sPackageAbiId == NULL) ||
			(pSummary->sArtifactKind == NULL) || (pSummary->sArtifactPath == NULL) ||
			(pSummary->sArtifactFile == NULL) || (pSummary->sConsumeMode == NULL) ) {
			return XL_RC_IO_ERROR;
		}
		if ( pSummary->iImportCount != 0 ) {
			pSummary->arrImports = (char**)calloc(pSummary->iImportCount, sizeof(*pSummary->arrImports));
			pSummary->arrImportAliases = (char**)calloc(pSummary->iImportCount, sizeof(*pSummary->arrImportAliases));
			pSummary->arrResolvedImports = (char**)calloc(pSummary->iImportCount, sizeof(*pSummary->arrResolvedImports));
			if ( (pSummary->arrImports == NULL) || (pSummary->arrImportAliases == NULL) || (pSummary->arrResolvedImports == NULL) ) {
				return XL_RC_IO_ERROR;
			}
			for ( j = 0; j < pSummary->iImportCount; ++j ) {
				pSummary->arrImports[j] = __xlModuleDupText(pModule->arrImportKeys[j]);
				pSummary->arrImportAliases[j] = __xlModuleDupText((pModule->arrImportAliases[j] != NULL) ? pModule->arrImportAliases[j] : "");
				pSummary->arrResolvedImports[j] = __xlModuleDupText(pModule->arrResolvedImportFiles[j]);
				if ( (pSummary->arrImports[j] == NULL) || (pSummary->arrImportAliases[j] == NULL) ||
					((pModule->arrResolvedImportFiles[j] != NULL) && (pSummary->arrResolvedImports[j] == NULL)) ) {
					return XL_RC_IO_ERROR;
				}
			}
		}
		if ( pSummary->iLinkLibCount != 0 ) {
			pSummary->arrLinkLibs = (char**)calloc(pSummary->iLinkLibCount, sizeof(*pSummary->arrLinkLibs));
			if ( pSummary->arrLinkLibs == NULL ) {
				return XL_RC_IO_ERROR;
			}
			for ( j = 0; j < pSummary->iLinkLibCount; ++j ) {
				pSummary->arrLinkLibs[j] = __xlModuleDupText(pModule->arrLinkLibs[j]);
				if ( pSummary->arrLinkLibs[j] == NULL ) {
					return XL_RC_IO_ERROR;
				}
			}
		}
		if ( pSummary->iLibPathCount != 0 ) {
			pSummary->arrLibPaths = (char**)calloc(pSummary->iLibPathCount, sizeof(*pSummary->arrLibPaths));
			if ( pSummary->arrLibPaths == NULL ) {
				return XL_RC_IO_ERROR;
			}
			for ( j = 0; j < pSummary->iLibPathCount; ++j ) {
				pSummary->arrLibPaths[j] = __xlModuleDupText(pModule->arrLibPaths[j]);
				if ( pSummary->arrLibPaths[j] == NULL ) {
					return XL_RC_IO_ERROR;
				}
			}
		}
		if ( pSummary->iIncPathCount != 0 ) {
			pSummary->arrIncPaths = (char**)calloc(pSummary->iIncPathCount, sizeof(*pSummary->arrIncPaths));
			if ( pSummary->arrIncPaths == NULL ) {
				return XL_RC_IO_ERROR;
			}
			for ( j = 0; j < pSummary->iIncPathCount; ++j ) {
				pSummary->arrIncPaths[j] = __xlModuleDupText(pModule->arrIncPaths[j]);
				if ( pSummary->arrIncPaths[j] == NULL ) {
					return XL_RC_IO_ERROR;
				}
			}
		}
		if ( pSummary->iLoadLibCount != 0 ) {
			pSummary->arrLoadLibs = (char**)calloc(pSummary->iLoadLibCount, sizeof(*pSummary->arrLoadLibs));
			if ( pSummary->arrLoadLibs == NULL ) {
				return XL_RC_IO_ERROR;
			}
			for ( j = 0; j < pSummary->iLoadLibCount; ++j ) {
				pSummary->arrLoadLibs[j] = __xlModuleDupText(pModule->arrLoadLibs[j]);
				if ( pSummary->arrLoadLibs[j] == NULL ) {
					return XL_RC_IO_ERROR;
				}
			}
		}
		if ( pSummary->iExportCount == 0 ) {
			continue;
		}

		pSummary->pExports = (XL_ModuleExportSummary*)calloc(pSummary->iExportCount, sizeof(*pSummary->pExports));
		if ( pSummary->pExports == NULL ) {
			return XL_RC_IO_ERROR;
		}

		for ( j = 0; j < pSummary->iExportCount; ++j ) {
			pSummary->pExports[j].sName = __xlModuleDupText(pModule->arrExports[j].sName);
			pSummary->pExports[j].sSymbol = __xlModuleDupText(pModule->arrExports[j].sMangledName);
			if ( (pSummary->pExports[j].sName == NULL) || (pSummary->pExports[j].sSymbol == NULL) ) {
				return XL_RC_IO_ERROR;
			}
		}
	}

	return XL_RC_OK;
}

void xlModuleLinkResultInit(XL_ModuleLinkResult* pResult)
{
	if ( pResult != NULL ) {
		memset(pResult, 0, sizeof(*pResult));
	}
}

void xlModuleLinkResultFree(XL_ModuleLinkResult* pResult)
{
	size_t i;
	size_t j;

	if ( pResult == NULL ) {
		return;
	}

	xlAstFree(pResult->pLinkedRoot);
	free(pResult->sTextDump);
	free(pResult->sJsonDump);
	for ( i = 0; i < pResult->iModuleCount; ++i ) {
		for ( j = 0; j < pResult->pModules[i].iImportCount; ++j ) {
			free(pResult->pModules[i].arrImports[j]);
			free(pResult->pModules[i].arrImportAliases[j]);
			free(pResult->pModules[i].arrResolvedImports[j]);
		}
		free(pResult->pModules[i].arrImports);
		free(pResult->pModules[i].arrImportAliases);
		free(pResult->pModules[i].arrResolvedImports);
		for ( j = 0; j < pResult->pModules[i].iLinkLibCount; ++j ) {
			free(pResult->pModules[i].arrLinkLibs[j]);
		}
		free(pResult->pModules[i].arrLinkLibs);
		for ( j = 0; j < pResult->pModules[i].iLibPathCount; ++j ) {
			free(pResult->pModules[i].arrLibPaths[j]);
		}
		free(pResult->pModules[i].arrLibPaths);
		for ( j = 0; j < pResult->pModules[i].iIncPathCount; ++j ) {
			free(pResult->pModules[i].arrIncPaths[j]);
		}
		free(pResult->pModules[i].arrIncPaths);
		for ( j = 0; j < pResult->pModules[i].iLoadLibCount; ++j ) {
			free(pResult->pModules[i].arrLoadLibs[j]);
		}
		free(pResult->pModules[i].arrLoadLibs);
		for ( j = 0; j < pResult->pModules[i].iExportCount; ++j ) {
			free(pResult->pModules[i].pExports[j].sName);
			free(pResult->pModules[i].pExports[j].sSymbol);
		}
		free(pResult->pModules[i].pExports);
		free(pResult->pModules[i].sKey);
		free(pResult->pModules[i].sFilePath);
		free(pResult->pModules[i].sBindingName);
		free(pResult->pModules[i].sInitSymbol);
		free(pResult->pModules[i].sExportsSymbol);
		free(pResult->pModules[i].sDescriptorSymbol);
		free(pResult->pModules[i].sManifestPath);
		free(pResult->pModules[i].sPackageName);
		free(pResult->pModules[i].sPackageVersion);
		free(pResult->pModules[i].sPackageAbiVersion);
		free(pResult->pModules[i].sPackageAbiId);
		free(pResult->pModules[i].sArtifactKind);
		free(pResult->pModules[i].sArtifactPath);
		free(pResult->pModules[i].sArtifactFile);
		free(pResult->pModules[i].sConsumeMode);
	}
	free(pResult->pModules);
	memset(pResult, 0, sizeof(*pResult));
}

int xlModuleLinkFromFile(const char* sInputFile, XL_DiagContext* pDiag, int bTraceParser, XL_ModuleLinkResult* pResult)
{
	XL_ModuleLinkContext ctx;
	XL_ModuleInfo* pRootModule;
	char sFullPath[XL_PATH_MAX_LEN];
	char sRootInputFile[XL_PATH_MAX_LEN];
	char sProjectFile[XL_PATH_MAX_LEN];
	size_t i;
	int iRc;

	if ( (sInputFile == NULL) || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlModuleLinkResultInit(pResult);
	memset(&ctx, 0, sizeof(ctx));
	ctx.pDiag = pDiag;
	ctx.bTraceParser = bTraceParser;

	__xlModuleNormalizePath(sFullPath, sizeof(sFullPath), sInputFile);
	snprintf(sRootInputFile, sizeof(sRootInputFile), "%s", sFullPath);
	if ( __xlFindProjectFileForInput(sFullPath, sProjectFile, sizeof(sProjectFile)) == XL_RC_OK ) {
		iRc = __xlLoadProjectFile(&ctx, sProjectFile, sRootInputFile, sizeof(sRootInputFile));
		if ( iRc != XL_RC_OK ) {
			goto cleanup;
		}
		if ( (strlen(sFullPath) >= strlen("xlang.json")) &&
			(strstr(sFullPath, "xlang.json") == (sFullPath + strlen(sFullPath) - strlen("xlang.json"))) &&
			(ctx.sProjectMain[0] == '\0') ) {
			XL_DIAG_ERROR(pDiag, "module", "xlang.json missing main field: %s", sProjectFile);
			iRc = XL_RC_FAIL;
			goto cleanup;
		}
	}
	__xlProjectAddDefaultPackageRoots(&ctx);
	iRc = __xlInitLocalSourcePackageIdentity(&ctx, sRootInputFile);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}
	iRc = __xlLoadModule(&ctx, sRootInputFile, "<root>", 1, NULL, &pRootModule);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = __xlBuildLinkedRoot(&ctx, &pResult->pLinkedRoot);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	__xlBuildModuleTextDump(&ctx, &pResult->sTextDump);
	__xlBuildModuleJsonDump(&ctx, &pResult->sJsonDump);
	iRc = __xlCopyModuleSummaries(&ctx, pResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}
	iRc = XL_RC_OK;

cleanup:
	for ( i = 0; i < ctx.iModuleCount; ++i ) {
		__xlDetachRootChildren(ctx.pModules[i].pRoot);
		__xlModuleInfoFree(&ctx.pModules[i]);
	}
	free(ctx.pModules);
	free(ctx.pOrderIndices);
	if ( iRc != XL_RC_OK ) {
		xlModuleLinkResultFree(pResult);
	}
	return iRc;
}
