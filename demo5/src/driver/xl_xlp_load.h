#ifndef XL_XLP_LOAD_H
#define XL_XLP_LOAD_H

#include "../common/xl_common.h"

typedef struct XL_XlpResolveInfo {
	char sManifestPath[XL_PATH_MAX_LEN];
	char sModuleFile[XL_PATH_MAX_LEN];
	char sBindingName[XL_PATH_MAX_LEN];
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
	char sType[64];
	int bHasDefault;
} XL_XlpParamInfo;

typedef struct XL_XlpFunctionInfo {
	char sModule[XL_PATH_MAX_LEN];
	char sModuleAbiId[XL_PATH_MAX_LEN];
	char sExportName[128];
	char sSymbol[128];
	char sOverloadGroup[256];
	char sSignatureId[256];
	char sDispatchKey[256];
	char sAbiSymbol[256];
	char sReturnType[64];
	int bAsync;
	int bGenerator;
	XL_XlpParamInfo* pParams;
	size_t iParamCount;
} XL_XlpFunctionInfo;

void xlXlpResolveInfoInit(XL_XlpResolveInfo* pInfo);

int xlXlpFindModuleSource(
	const char* sCurrentFile,
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

#endif
