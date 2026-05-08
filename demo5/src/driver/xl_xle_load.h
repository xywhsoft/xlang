#ifndef XL_XLE_LOAD_H
#define XL_XLE_LOAD_H

#include "../common/xl_common.h"

#define XL_XLE_LINK_ITEM_MAX 16
#define XL_XLE_CSOURCE_ITEM_MAX 16

typedef struct XL_XleResolveInfo {
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
	char arrLinkLibs[XL_XLE_LINK_ITEM_MAX][XL_PATH_MAX_LEN];
	size_t iLinkLibCount;
	char arrLibPaths[XL_XLE_LINK_ITEM_MAX][XL_PATH_MAX_LEN];
	size_t iLibPathCount;
	char arrIncPaths[XL_XLE_LINK_ITEM_MAX][XL_PATH_MAX_LEN];
	size_t iIncPathCount;
	char arrCSourcePaths[XL_XLE_CSOURCE_ITEM_MAX][XL_PATH_MAX_LEN];
	char arrCSourceFiles[XL_XLE_CSOURCE_ITEM_MAX][XL_PATH_MAX_LEN];
	size_t iCSourceCount;
	int bIsNative;
} XL_XleResolveInfo;

typedef struct XL_XleDependencyInfo {
	char sName[XL_PATH_MAX_LEN];
	char sKind[32];
	char sVersion[64];
	char sAbiVersion[32];
	char sAbiId[XL_PATH_MAX_LEN];
} XL_XleDependencyInfo;

typedef struct XL_XleParamInfo {
	char sName[128];
	char sLabel[128];
	char sType[XL_PATH_MAX_LEN];
	char* sDefaultValueText;
	char* sDefaultValueJson;
	int bHasDefault;
} XL_XleParamInfo;

typedef struct XL_XleReturnInfo {
	char sType[XL_PATH_MAX_LEN];
} XL_XleReturnInfo;

typedef struct XL_XleTypeParamInfo {
	char sName[XL_PATH_MAX_LEN];
} XL_XleTypeParamInfo;

typedef struct XL_XleFunctionInfo {
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
	XL_XleParamInfo* pParams;
	size_t iParamCount;
	XL_XleReturnInfo* pReturns;
	size_t iReturnCount;
	XL_XleTypeParamInfo* pTypeParams;
	size_t iTypeParamCount;
} XL_XleFunctionInfo;

typedef struct XL_XleExportInfo {
	char sModule[XL_PATH_MAX_LEN];
	char sModuleAbiId[XL_PATH_MAX_LEN];
	char sName[128];
	char sSymbol[128];
	char sAbiId[XL_PATH_MAX_LEN];
	char sVersionedSymbol[XL_PATH_MAX_LEN];
	char sKind[32];
	int iOverloadCount;
} XL_XleExportInfo;

void xlXleResolveInfoInit(XL_XleResolveInfo* pInfo);

int xlXleFindModuleSource(
	const char* sCurrentFile,
	const char* sCurrentManifestPath,
	const char* sImportKey,
	XL_XleResolveInfo* pInfo
);
int xlXleFindModuleSourceInRoots(
	const char** ppSearchRoots,
	size_t iSearchRootCount,
	const char* sCurrentManifestPath,
	const char* sImportKey,
	XL_XleResolveInfo* pInfo
);

int xlXleFindDependencyVersion(
	const char* sManifestPath,
	const char* sImportKey,
	char* sOutVersion,
	size_t iOutVersionSize
);

void xlXleDependencyInfoFree(XL_XleDependencyInfo* pDeps);
int xlXleReadDependencies(
	const char* sManifestPath,
	XL_XleDependencyInfo** ppDeps,
	size_t* piDepCount
);
void xlXleFunctionInfoFree(XL_XleFunctionInfo* pFuncs, size_t iFuncCount);
int xlXleReadFunctions(
	const char* sManifestPath,
	const char* sModuleKey,
	XL_XleFunctionInfo** ppFuncs,
	size_t* piFuncCount
);
void xlXleExportInfoFree(XL_XleExportInfo* pExports);
int xlXleReadExports(
	const char* sManifestPath,
	const char* sModuleKey,
	XL_XleExportInfo** ppExports,
	size_t* piExportCount
);

#endif
