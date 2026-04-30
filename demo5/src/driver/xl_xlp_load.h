#ifndef XL_XLP_LOAD_H
#define XL_XLP_LOAD_H

#include "../common/xl_common.h"

typedef struct XL_XlpResolveInfo {
	char sManifestPath[XL_PATH_MAX_LEN];
	char sModuleFile[XL_PATH_MAX_LEN];
	char sBindingName[XL_PATH_MAX_LEN];
	char sInitSymbol[XL_PATH_MAX_LEN];
	char sExportsSymbol[XL_PATH_MAX_LEN];
	char sDescriptorSymbol[XL_PATH_MAX_LEN];
	char sPackageName[XL_PATH_MAX_LEN];
	char sPackageVersion[64];
	char sPackageAbiVersion[32];
	char sPackageAbiId[XL_PATH_MAX_LEN];
	char sArtifactKind[32];
	char sArtifactPath[XL_PATH_MAX_LEN];
	char sArtifactFile[XL_PATH_MAX_LEN];
	char sConsumeMode[32];
	int bIsNative;
} XL_XlpResolveInfo;

typedef struct XL_XlpDependencyInfo {
	char sName[XL_PATH_MAX_LEN];
	char sKind[32];
	char sVersion[64];
	char sAbiVersion[32];
	char sAbiId[XL_PATH_MAX_LEN];
} XL_XlpDependencyInfo;

typedef struct XL_XlpParamInfo {
	char sName[128];
	char sLabel[128];
	char sType[XL_PATH_MAX_LEN];
	char* sDefaultValueText;
	char* sDefaultValueJson;
	int bHasDefault;
} XL_XlpParamInfo;

typedef struct XL_XlpReturnInfo {
	char sType[XL_PATH_MAX_LEN];
} XL_XlpReturnInfo;

typedef struct XL_XlpTypeParamInfo {
	char sName[XL_PATH_MAX_LEN];
} XL_XlpTypeParamInfo;

typedef struct XL_XlpFunctionInfo {
	char sModule[XL_PATH_MAX_LEN];
	char sModuleAbiId[XL_PATH_MAX_LEN];
	char sExportName[128];
	char sReceiverType[XL_PATH_MAX_LEN];
	char sMethodName[XL_PATH_MAX_LEN];
	char sSymbol[128];
	char sOverloadGroup[XL_PATH_MAX_LEN];
	char sSignatureId[XL_PATH_MAX_LEN];
	char sDispatchKey[XL_PATH_MAX_LEN];
	char sLanguageAbi[64];
	char sNativeAbi[64];
	char sLanguageAbiId[XL_PATH_MAX_LEN];
	char sNativeAbiId[XL_PATH_MAX_LEN];
	char sAbiSymbol[XL_PATH_MAX_LEN];
	char sReturnType[XL_PATH_MAX_LEN];
	char sStorageReturnType[XL_PATH_MAX_LEN];
	int bAsync;
	int bGenerator;
	int iVarArgKind;
	XL_XlpParamInfo* pParams;
	size_t iParamCount;
	XL_XlpReturnInfo* pReturns;
	size_t iReturnCount;
	XL_XlpTypeParamInfo* pTypeParams;
	size_t iTypeParamCount;
} XL_XlpFunctionInfo;

typedef struct XL_XlpExportInfo {
	char sModule[XL_PATH_MAX_LEN];
	char sModuleAbiId[XL_PATH_MAX_LEN];
	char sName[128];
	char sSymbol[128];
	char sAbiId[XL_PATH_MAX_LEN];
	char sVersionedSymbol[XL_PATH_MAX_LEN];
	char sKind[32];
	int iOverloadCount;
} XL_XlpExportInfo;

void xlXlpResolveInfoInit(XL_XlpResolveInfo* pInfo);

int xlXlpFindModuleSource(
	const char* sCurrentFile,
	const char* sCurrentManifestPath,
	const char* sImportKey,
	XL_XlpResolveInfo* pInfo
);
int xlXlpFindModuleSourceInRoots(
	const char** ppSearchRoots,
	size_t iSearchRootCount,
	const char* sCurrentManifestPath,
	const char* sImportKey,
	XL_XlpResolveInfo* pInfo
);

int xlXlpFindDependencyVersion(
	const char* sManifestPath,
	const char* sImportKey,
	char* sOutVersion,
	size_t iOutVersionSize
);

void xlXlpDependencyInfoFree(XL_XlpDependencyInfo* pDeps);
int xlXlpReadDependencies(
	const char* sManifestPath,
	XL_XlpDependencyInfo** ppDeps,
	size_t* piDepCount
);
void xlXlpFunctionInfoFree(XL_XlpFunctionInfo* pFuncs, size_t iFuncCount);
int xlXlpReadFunctions(
	const char* sManifestPath,
	const char* sModuleKey,
	XL_XlpFunctionInfo** ppFuncs,
	size_t* piFuncCount
);
void xlXlpExportInfoFree(XL_XlpExportInfo* pExports);
int xlXlpReadExports(
	const char* sManifestPath,
	const char* sModuleKey,
	XL_XlpExportInfo** ppExports,
	size_t* piExportCount
);

#endif
