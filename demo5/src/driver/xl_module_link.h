#ifndef XL_MODULE_LINK_H
#define XL_MODULE_LINK_H

#include "../common/xl_common.h"
#include "../ast/xl_ast.h"
#include "../diag/xl_diag.h"

typedef struct XL_ModuleExportSummary {
	char* sName;
	char* sSymbol;
} XL_ModuleExportSummary;

typedef struct XL_ModuleSummary {
	char* sKey;
	char* sFilePath;
	char* sBindingName;
	char* sManifestPath;
	char* sPackageName;
	char* sPackageVersion;
	char* sPackageAbiVersion;
	char* sPackageAbiId;
	char* sArtifactKind;
	char* sArtifactPath;
	char* sArtifactFile;
	char* sConsumeMode;
	char** arrImports;
	char** arrResolvedImports;
	size_t iImportCount;
	XL_ModuleExportSummary* pExports;
	size_t iExportCount;
	int bIsRoot;
	int bIsNative;
} XL_ModuleSummary;

typedef struct XL_ModuleLinkResult {
	XL_AstNode* pLinkedRoot;
	char* sTextDump;
	char* sJsonDump;
	XL_ModuleSummary* pModules;
	size_t iModuleCount;
} XL_ModuleLinkResult;

void xlModuleLinkResultInit(XL_ModuleLinkResult* pResult);
void xlModuleLinkResultFree(XL_ModuleLinkResult* pResult);
int xlModuleLinkFromFile(const char* sInputFile, XL_DiagContext* pDiag, int bTraceParser, XL_ModuleLinkResult* pResult);

#endif
