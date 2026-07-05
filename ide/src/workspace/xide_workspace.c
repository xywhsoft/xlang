#include "workspace/xide_workspace.h"
#include "lib/xrt/xrt.h"

#include <stdio.h>
#include <string.h>

static void xideWorkspaceCopy(char* sDst, int iCapacity, const char* sSrc)
{
	int len;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	if ( sSrc == NULL ) sSrc = "";
	len = (int)strlen(sSrc);
	if ( len >= iCapacity ) len = iCapacity - 1;
	if ( len > 0 ) memcpy(sDst, sSrc, (size_t)len);
	sDst[len] = '\0';
}

static int xideWorkspaceNextNodeId(xide_workspace_t* pWorkspace)
{
	if ( pWorkspace == NULL ) return 0;
	return pWorkspace->iNextNodeId++;
}

static const char* xideWorkspaceJsonText(xvalue pObj, const char* sKey, const char* sDefault)
{
	xvalue pVal;
	if ( pObj == NULL || sKey == NULL || !xvoIsTable(pObj) ) return sDefault != NULL ? sDefault : "";
	pVal = xvoTableGetValue(pObj, sKey, 0);
	return xvoIsText(pVal) ? (const char*)xvoGetText(pVal) : (sDefault != NULL ? sDefault : "");
}

static int xideWorkspaceJsonBool(xvalue pObj, const char* sKey, int iDefault)
{
	xvalue pVal;
	if ( pObj == NULL || sKey == NULL || !xvoIsTable(pObj) ) return iDefault;
	pVal = xvoTableGetValue(pObj, sKey, 0);
	if ( xvoIsBool(pVal) || xvoIsInt(pVal) ) return xvoGetBool(pVal) ? 1 : 0;
	return iDefault;
}

static int xideWorkspaceJsonInt(xvalue pObj, const char* sKey, int iDefault)
{
	xvalue pVal;
	if ( pObj == NULL || sKey == NULL || !xvoIsTable(pObj) ) return iDefault;
	pVal = xvoTableGetValue(pObj, sKey, 0);
	return xvoIsInt(pVal) ? (int)xvoGetInt(pVal) : iDefault;
}

static xvalue xideWorkspaceJsonTable(xvalue pObj, const char* sKey)
{
	xvalue pVal;
	if ( pObj == NULL || sKey == NULL || !xvoIsTable(pObj) ) return NULL;
	pVal = xvoTableGetValue(pObj, sKey, 0);
	return xvoIsTable(pVal) ? pVal : NULL;
}

static xvalue xideWorkspaceJsonArray(xvalue pObj, const char* sKey)
{
	xvalue pVal;
	if ( pObj == NULL || sKey == NULL || !xvoIsTable(pObj) ) return NULL;
	pVal = xvoTableGetValue(pObj, sKey, 0);
	return xvoIsArray(pVal) ? pVal : NULL;
}

static void xideWorkspacePathName(char* sDst, int iCapacity, const char* sPath)
{
	str sName;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	sDst[0] = '\0';
	if ( sPath == NULL || sPath[0] == '\0' ) return;
	sName = xrtPathGetNameExt((str)sPath, 0);
	if ( sName != NULL && sName[0] != '\0' ) xideWorkspaceCopy(sDst, iCapacity, (const char*)sName);
	if ( sName != NULL ) xrtFree(sName);
}

static void xideWorkspacePathNameNoExt(char* sDst, int iCapacity, const char* sPath)
{
	str sName;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	sDst[0] = '\0';
	if ( sPath == NULL || sPath[0] == '\0' ) return;
	sName = xrtPathGetName((str)sPath, 0);
	if ( sName != NULL && sName[0] != '\0' ) xideWorkspaceCopy(sDst, iCapacity, (const char*)sName);
	if ( sName != NULL ) xrtFree(sName);
}

static void xideWorkspacePathDir(char* sDst, int iCapacity, const char* sPath)
{
	str sDir;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	sDst[0] = '\0';
	if ( sPath == NULL || sPath[0] == '\0' ) return;
	sDir = xrtPathGetDir((str)sPath, 0);
	if ( sDir != NULL && sDir[0] != '\0' ) xideWorkspaceCopy(sDst, iCapacity, (const char*)sDir);
	if ( sDir != NULL ) xrtFree(sDir);
}

static void xideWorkspacePathAbs(char* sDst, int iCapacity, const char* sBase, const char* sPath)
{
	str sJoined = NULL;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	sDst[0] = '\0';
	if ( sPath == NULL || sPath[0] == '\0' ) return;
	if ( xrtPathIsAbs((str)sPath, 0) ) {
		xideWorkspaceCopy(sDst, iCapacity, sPath);
	} else if ( sBase != NULL && sBase[0] != '\0' ) {
		sJoined = xrtPathJoin(2, (str)sBase, (str)sPath);
		xideWorkspaceCopy(sDst, iCapacity, (const char*)sJoined);
	} else {
		xideWorkspaceCopy(sDst, iCapacity, sPath);
	}
	if ( sJoined != NULL ) xrtFree(sJoined);
}

static void xideWorkspacePathRel(char* sDst, int iCapacity, const char* sBase, const char* sPath)
{
	int baseLen;
	if ( sDst == NULL || iCapacity <= 0 ) return;
	sDst[0] = '\0';
	if ( sPath == NULL || sPath[0] == '\0' ) return;
	if ( sBase == NULL || sBase[0] == '\0' || !xrtPathIsAbs((str)sPath, 0) ) {
		xideWorkspaceCopy(sDst, iCapacity, sPath);
		return;
	}
	baseLen = (int)strlen(sBase);
	if ( baseLen > 0 &&
	     _strnicmp(sBase, sPath, (size_t)baseLen) == 0 &&
	     (sPath[baseLen] == '\\' || sPath[baseLen] == '/') ) {
		xideWorkspaceCopy(sDst, iCapacity, sPath + baseLen + 1);
		return;
	}
	xideWorkspaceCopy(sDst, iCapacity, sPath);
}

static int xideWorkspaceEnsureParentDir(const char* sFile)
{
	char sDir[XIDE_WORKSPACE_PATH_MAX];
	if ( sFile == NULL || sFile[0] == '\0' ) return 0;
	xideWorkspacePathDir(sDir, (int)sizeof(sDir), sFile);
	if ( sDir[0] == '\0' ) return 1;
	return xrtDirCreateAll((str)sDir) ? 1 : 0;
}

static xvalue xideWorkspaceCreateFileJson(const xide_workspace_item_t* pItem)
{
	xvalue pObj = xvoCreateTable();
	if ( pObj == NULL ) return NULL;
	(void)xvoTableSetText(pObj, "Path", 0, (str)pItem->sPath, 0, FALSE);
	(void)xvoTableSetBool(pObj, "Open", 0, pItem->bOpen ? TRUE : FALSE);
	return pObj;
}

static void xideWorkspaceFillProjectDefaults(xide_workspace_project_t* pProject)
{
	if ( pProject == NULL ) return;
	if ( pProject->sName[0] == '\0' ) xideWorkspaceCopy(pProject->sName, (int)sizeof(pProject->sName), "Project");
	if ( pProject->sAppPath[0] == '\0' ) xideWorkspaceCopy(pProject->sAppPath, (int)sizeof(pProject->sAppPath), "bin");
	if ( pProject->sBuildFile[0] == '\0' ) {
		char sBuild[XIDE_WORKSPACE_NAME_MAX];
		xideWorkspaceCopy(sBuild, (int)sizeof(sBuild), pProject->sName);
		if ( strlen(sBuild) + 4u < sizeof(sBuild) ) strcat(sBuild, ".exe");
		xideWorkspaceCopy(pProject->sBuildFile, (int)sizeof(pProject->sBuildFile), sBuild);
	}
	if ( pProject->sBuildProcessName[0] == '\0' ) xideWorkspaceCopy(pProject->sBuildProcessName, (int)sizeof(pProject->sBuildProcessName), pProject->sName);
	if ( pProject->sBuildFileDescription[0] == '\0' ) xideWorkspaceCopy(pProject->sBuildFileDescription, (int)sizeof(pProject->sBuildFileDescription), pProject->sName);
	if ( pProject->sBuildProductName[0] == '\0' ) xideWorkspaceCopy(pProject->sBuildProductName, (int)sizeof(pProject->sBuildProductName), pProject->sName);
	if ( pProject->sBuildProductVersion[0] == '\0' ) xideWorkspaceCopy(pProject->sBuildProductVersion, (int)sizeof(pProject->sBuildProductVersion), "1.0.0.0");
	if ( pProject->sBuildOriginalFilename[0] == '\0' ) xideWorkspaceCopy(pProject->sBuildOriginalFilename, (int)sizeof(pProject->sBuildOriginalFilename), pProject->sBuildFile);
	if ( pProject->sBuildCharset[0] == '\0' ) xideWorkspaceCopy(pProject->sBuildCharset, (int)sizeof(pProject->sBuildCharset), "UTF-8");
	if ( pProject->iBuildFileVersion1 <= 0 ) pProject->iBuildFileVersion1 = 1;
	if ( pProject->bBuildUseFileVersion != 0 && pProject->bBuildUseFileVersion != 1 ) pProject->bBuildUseFileVersion = 1;
}

void xideWorkspaceInit(xide_workspace_t* pWorkspace)
{
	if ( pWorkspace == NULL ) return;
	memset(pWorkspace, 0, sizeof(*pWorkspace));
	xideWorkspaceClear(pWorkspace);
}

void xideWorkspaceClear(xide_workspace_t* pWorkspace)
{
	if ( pWorkspace == NULL ) return;
	memset(pWorkspace, 0, sizeof(*pWorkspace));
	pWorkspace->iNextNodeId = 1;
	pWorkspace->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pWorkspace->bTemp = 1;
	pWorkspace->bExpanded = 1;
	xideWorkspaceCopy(pWorkspace->sName, (int)sizeof(pWorkspace->sName), "Workspace");
}

xide_workspace_project_t* xideWorkspaceAddProject(xide_workspace_t* pWorkspace, const char* sName, const char* sFile, const char* sPath)
{
	xide_workspace_project_t* pProject;
	if ( pWorkspace == NULL || pWorkspace->iProjectCount >= XIDE_WORKSPACE_PROJECT_MAX ) return NULL;
	pProject = &pWorkspace->arrProjects[pWorkspace->iProjectCount++];
	memset(pProject, 0, sizeof(*pProject));
	pProject->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pProject->iSourceNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pProject->iLibraryNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pProject->iResourceNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pProject->bExpanded = 1;
	pProject->bSourceExpanded = 1;
	pProject->bLibraryExpanded = 1;
	pProject->bResourceExpanded = 1;
	pProject->bBuildUseFileVersion = 1;
	xideWorkspaceCopy(pProject->sName, (int)sizeof(pProject->sName), sName);
	xideWorkspaceCopy(pProject->sFile, (int)sizeof(pProject->sFile), sFile);
	xideWorkspaceCopy(pProject->sPath, (int)sizeof(pProject->sPath), sPath);
	xideWorkspaceFillProjectDefaults(pProject);
	return pProject;
}

xide_workspace_item_t* xideWorkspaceProjectAddFile(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath, int bOpen)
{
	xide_workspace_item_t* pItem;
	char sAutoName[XIDE_WORKSPACE_NAME_MAX];
	if ( pWorkspace == NULL || pProject == NULL || pProject->iFileCount >= XIDE_WORKSPACE_FILE_MAX ) return NULL;
	pItem = &pProject->arrFiles[pProject->iFileCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pItem->iKind = XIDE_WS_NODE_FILE;
	pItem->bOpen = bOpen ? 1 : 0;
	xideWorkspaceCopy(pItem->sPath, (int)sizeof(pItem->sPath), sPath);
	xideWorkspacePathName(sAutoName, (int)sizeof(sAutoName), sPath);
	xideWorkspaceCopy(pItem->sName, (int)sizeof(pItem->sName), (sName != NULL && sName[0] != '\0') ? sName : sAutoName);
	return pItem;
}

xide_workspace_item_t* xideWorkspaceProjectAddLibrary(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath)
{
	xide_workspace_item_t* pItem;
	if ( pWorkspace == NULL || pProject == NULL || pProject->iLibraryCount >= XIDE_WORKSPACE_LIBRARY_MAX ) return NULL;
	pItem = &pProject->arrLibraries[pProject->iLibraryCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pItem->iKind = XIDE_WS_NODE_LIBRARY;
	xideWorkspaceCopy(pItem->sName, (int)sizeof(pItem->sName), sName != NULL && sName[0] != '\0' ? sName : sPath);
	xideWorkspaceCopy(pItem->sPath, (int)sizeof(pItem->sPath), sPath);
	return pItem;
}

xide_workspace_item_t* xideWorkspaceProjectAddResource(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath)
{
	xide_workspace_item_t* pItem;
	char sAutoName[XIDE_WORKSPACE_NAME_MAX];
	if ( pWorkspace == NULL || pProject == NULL || pProject->iResourceCount >= XIDE_WORKSPACE_RESOURCE_MAX ) return NULL;
	pItem = &pProject->arrResources[pProject->iResourceCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pItem->iKind = XIDE_WS_NODE_RESOURCE;
	xideWorkspaceCopy(pItem->sPath, (int)sizeof(pItem->sPath), sPath);
	xideWorkspacePathName(sAutoName, (int)sizeof(sAutoName), sPath);
	xideWorkspaceCopy(pItem->sName, (int)sizeof(pItem->sName), (sName != NULL && sName[0] != '\0') ? sName : sAutoName);
	return pItem;
}

int xideWorkspaceAddLooseFile(xide_workspace_t* pWorkspace, const char* sName, const char* sPath, int bOpen)
{
	xide_workspace_item_t* pItem;
	char sAutoName[XIDE_WORKSPACE_NAME_MAX];
	if ( pWorkspace == NULL || pWorkspace->iLooseFileCount >= XIDE_WORKSPACE_FILE_MAX ) return 0;
	pItem = &pWorkspace->arrLooseFiles[pWorkspace->iLooseFileCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pItem->iKind = XIDE_WS_NODE_FILE;
	pItem->bOpen = bOpen ? 1 : 0;
	xideWorkspaceCopy(pItem->sPath, (int)sizeof(pItem->sPath), sPath);
	xideWorkspacePathName(sAutoName, (int)sizeof(sAutoName), sPath);
	xideWorkspaceCopy(pItem->sName, (int)sizeof(pItem->sName), (sName != NULL && sName[0] != '\0') ? sName : sAutoName);
	return 1;
}

int xideWorkspaceCreateDefault(xide_workspace_t* pWorkspace)
{
	xide_workspace_project_t* pProject;
	if ( pWorkspace == NULL ) return 0;
	xideWorkspaceClear(pWorkspace);
	xideWorkspaceCopy(pWorkspace->sName, (int)sizeof(pWorkspace->sName), "Workspace1");
	pProject = xideWorkspaceAddProject(pWorkspace, "Project1", "Project1\\Project1.xlp", "Project1\\");
	if ( pProject == NULL ) return 0;
	xideWorkspaceCopy(pProject->sBuildIcon, (int)sizeof(pProject->sBuildIcon), "res\\app.ico");
	(void)xideWorkspaceProjectAddFile(pWorkspace, pProject, "main.xl", "src\\main.xl", 1);
	(void)xideWorkspaceProjectAddResource(pWorkspace, pProject, "app.ico", "res\\app.ico");
	return 1;
}

static int xideWorkspaceLoadProjectJson(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, xvalue pRoot)
{
	xvalue pObj;
	xvalue pArr;
	uint32 i;
	uint32 count;
	if ( pWorkspace == NULL || pProject == NULL || pRoot == NULL || !xvoIsTable(pRoot) ) return 0;

	pObj = xideWorkspaceJsonTable(pRoot, "Project");
	if ( pObj != NULL ) {
		xideWorkspaceCopy(pProject->sName, (int)sizeof(pProject->sName), xideWorkspaceJsonText(pObj, "Description", pProject->sName));
		xideWorkspaceCopy(pProject->sAppPath, (int)sizeof(pProject->sAppPath), xideWorkspaceJsonText(pObj, "AppPath", pProject->sAppPath));
		xideWorkspaceCopy(pProject->sIcon, (int)sizeof(pProject->sIcon), xideWorkspaceJsonText(pObj, "Icon", pProject->sIcon));
	}

	pObj = xideWorkspaceJsonTable(pRoot, "Status");
	if ( pObj != NULL ) {
		pProject->bExpanded = xideWorkspaceJsonBool(pObj, "ProjExpand", 1);
		pProject->bSourceExpanded = xideWorkspaceJsonBool(pObj, "FileExpand", 1);
		pProject->bLibraryExpanded = xideWorkspaceJsonBool(pObj, "LibsExpand", 1);
		pProject->bResourceExpanded = xideWorkspaceJsonBool(pObj, "ResourceExpand", 1);
	}

	pObj = xideWorkspaceJsonTable(pRoot, "Build");
	if ( pObj != NULL ) {
		xideWorkspaceCopy(pProject->sBuildFile, (int)sizeof(pProject->sBuildFile), xideWorkspaceJsonText(pObj, "File", pProject->sBuildFile));
		xideWorkspaceCopy(pProject->sBuildIcon, (int)sizeof(pProject->sBuildIcon), xideWorkspaceJsonText(pObj, "Icon", pProject->sBuildIcon));
		xideWorkspaceCopy(pProject->sBuildProcessName, (int)sizeof(pProject->sBuildProcessName), xideWorkspaceJsonText(pObj, "ProcessName", pProject->sBuildProcessName));
		xideWorkspaceCopy(pProject->sBuildComments, (int)sizeof(pProject->sBuildComments), xideWorkspaceJsonText(pObj, "Comments", pProject->sBuildComments));
		xideWorkspaceCopy(pProject->sBuildCompanyName, (int)sizeof(pProject->sBuildCompanyName), xideWorkspaceJsonText(pObj, "CompanyName", pProject->sBuildCompanyName));
		xideWorkspaceCopy(pProject->sBuildFileDescription, (int)sizeof(pProject->sBuildFileDescription), xideWorkspaceJsonText(pObj, "FileDescription", pProject->sBuildFileDescription));
		xideWorkspaceCopy(pProject->sBuildLegalCopyright, (int)sizeof(pProject->sBuildLegalCopyright), xideWorkspaceJsonText(pObj, "LegalCopyright", pProject->sBuildLegalCopyright));
		xideWorkspaceCopy(pProject->sBuildLegalTrademarks, (int)sizeof(pProject->sBuildLegalTrademarks), xideWorkspaceJsonText(pObj, "LegalTrademarks", pProject->sBuildLegalTrademarks));
		xideWorkspaceCopy(pProject->sBuildProductName, (int)sizeof(pProject->sBuildProductName), xideWorkspaceJsonText(pObj, "ProductName", pProject->sBuildProductName));
		xideWorkspaceCopy(pProject->sBuildProductVersion, (int)sizeof(pProject->sBuildProductVersion), xideWorkspaceJsonText(pObj, "ProductVersion", pProject->sBuildProductVersion));
		xideWorkspaceCopy(pProject->sBuildOriginalFilename, (int)sizeof(pProject->sBuildOriginalFilename), xideWorkspaceJsonText(pObj, "OriginalFilename", pProject->sBuildOriginalFilename));
		xideWorkspaceCopy(pProject->sBuildCharset, (int)sizeof(pProject->sBuildCharset), xideWorkspaceJsonText(pObj, "Charset", pProject->sBuildCharset));
		pProject->bBuildUseFileVersion = xideWorkspaceJsonBool(pObj, "UseFileVersion", 1);
		pProject->iBuildFileVersion1 = xideWorkspaceJsonInt(pObj, "FileVersion1", 1);
		pProject->iBuildFileVersion2 = xideWorkspaceJsonInt(pObj, "FileVersion2", 0);
		pProject->iBuildFileVersion3 = xideWorkspaceJsonInt(pObj, "FileVersion3", 0);
		pProject->iBuildFileVersion4 = xideWorkspaceJsonInt(pObj, "FileVersion4", 0);
		pProject->iBuildFileVersionAutoAdd = xideWorkspaceJsonInt(pObj, "FileVersionAutoAdd", 0);
	}

	pArr = xideWorkspaceJsonArray(pRoot, "Library");
	if ( pArr != NULL ) {
		count = xvoArrayItemCount(pArr);
		for ( i = 0; i < count; i++ ) {
			xvalue pVal = xvoArrayGetValue(pArr, i);
			if ( xvoIsText(pVal) ) (void)xideWorkspaceProjectAddLibrary(pWorkspace, pProject, (const char*)xvoGetText(pVal), (const char*)xvoGetText(pVal));
		}
	}

	pArr = xideWorkspaceJsonArray(pRoot, "File");
	if ( pArr != NULL ) {
		count = xvoArrayItemCount(pArr);
		for ( i = 0; i < count; i++ ) {
			xvalue pFile = xvoArrayGetValue(pArr, i);
			const char* sPath;
			int bOpen;
			if ( !xvoIsTable(pFile) ) continue;
			sPath = xideWorkspaceJsonText(pFile, "Path", "");
			if ( sPath[0] == '\0' ) continue;
			bOpen = xideWorkspaceJsonBool(pFile, "Open", 0);
			(void)xideWorkspaceProjectAddFile(pWorkspace, pProject, NULL, sPath, bOpen);
		}
	}

	pArr = xideWorkspaceJsonArray(pRoot, "Resource");
	if ( pArr != NULL ) {
		count = xvoArrayItemCount(pArr);
		for ( i = 0; i < count; i++ ) {
			xvalue pFile = xvoArrayGetValue(pArr, i);
			const char* sPath;
			if ( !xvoIsTable(pFile) ) continue;
			sPath = xideWorkspaceJsonText(pFile, "Path", "");
			if ( sPath[0] == '\0' ) continue;
			(void)xideWorkspaceProjectAddResource(pWorkspace, pProject, NULL, sPath);
		}
	}

	xideWorkspaceCopy(pProject->sBootCode, (int)sizeof(pProject->sBootCode), xideWorkspaceJsonText(pRoot, "BootCode", ""));
	xideWorkspaceFillProjectDefaults(pProject);
	return 1;
}

int xideWorkspaceLoadXlp(xide_workspace_t* pWorkspace, const char* sFile, xide_workspace_project_t** ppProject)
{
	char sAbs[XIDE_WORKSPACE_PATH_MAX];
	char sDir[XIDE_WORKSPACE_PATH_MAX];
	char sName[XIDE_WORKSPACE_NAME_MAX];
	xide_workspace_project_t* pProject;
	xvalue pRoot;
	int iOldProjectCount;
	int iOldNextNodeId;
	if ( ppProject != NULL ) *ppProject = NULL;
	if ( pWorkspace == NULL || sFile == NULL || sFile[0] == '\0' ) return 0;
	xideWorkspacePathAbs(sAbs, (int)sizeof(sAbs), NULL, sFile);
	pRoot = xrtParseJSON_File((str)sAbs);
	if ( pRoot == NULL || !xvoIsTable(pRoot) ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return 0;
	}
	xideWorkspacePathDir(sDir, (int)sizeof(sDir), sAbs);
	xideWorkspacePathNameNoExt(sName, (int)sizeof(sName), sAbs);
	iOldProjectCount = pWorkspace->iProjectCount;
	iOldNextNodeId = pWorkspace->iNextNodeId;
	pProject = xideWorkspaceAddProject(pWorkspace, sName, sAbs, sDir);
	if ( pProject == NULL ) {
		xvoUnref(pRoot);
		return 0;
	}
	if ( !xideWorkspaceLoadProjectJson(pWorkspace, pProject, pRoot) ) {
		pWorkspace->iProjectCount = iOldProjectCount;
		pWorkspace->iNextNodeId = iOldNextNodeId;
		xvoUnref(pRoot);
		return 0;
	}
	xvoUnref(pRoot);
	if ( ppProject != NULL ) *ppProject = pProject;
	return 1;
}

static xvalue xideWorkspaceCreateProjectJson(const xide_workspace_project_t* pProject)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pObj;
	xvalue pArr;
	int i;
	if ( pRoot == NULL || pProject == NULL ) return pRoot;

	(void)xvoTableSetInt(pRoot, "Version", 0, 1);
	pObj = xvoCreateTable();
	(void)xvoTableSetText(pObj, "Description", 0, (str)pProject->sName, 0, FALSE);
	(void)xvoTableSetText(pObj, "AppPath", 0, (str)pProject->sAppPath, 0, FALSE);
	(void)xvoTableSetText(pObj, "Icon", 0, (str)pProject->sIcon, 0, FALSE);
	(void)xvoTableSetValue(pRoot, "Project", 0, pObj, TRUE);

	pObj = xvoCreateTable();
	(void)xvoTableSetBool(pObj, "ProjExpand", 0, pProject->bExpanded ? TRUE : FALSE);
	(void)xvoTableSetBool(pObj, "FileExpand", 0, pProject->bSourceExpanded ? TRUE : FALSE);
	(void)xvoTableSetBool(pObj, "LibsExpand", 0, pProject->bLibraryExpanded ? TRUE : FALSE);
	(void)xvoTableSetBool(pObj, "ResourceExpand", 0, pProject->bResourceExpanded ? TRUE : FALSE);
	(void)xvoTableSetValue(pRoot, "Status", 0, pObj, TRUE);

	pObj = xvoCreateTable();
	(void)xvoTableSetText(pObj, "File", 0, (str)pProject->sBuildFile, 0, FALSE);
	(void)xvoTableSetText(pObj, "Icon", 0, (str)pProject->sBuildIcon, 0, FALSE);
	(void)xvoTableSetText(pObj, "ProcessName", 0, (str)pProject->sBuildProcessName, 0, FALSE);
	(void)xvoTableSetText(pObj, "Comments", 0, (str)pProject->sBuildComments, 0, FALSE);
	(void)xvoTableSetText(pObj, "CompanyName", 0, (str)pProject->sBuildCompanyName, 0, FALSE);
	(void)xvoTableSetText(pObj, "FileDescription", 0, (str)pProject->sBuildFileDescription, 0, FALSE);
	(void)xvoTableSetText(pObj, "LegalCopyright", 0, (str)pProject->sBuildLegalCopyright, 0, FALSE);
	(void)xvoTableSetText(pObj, "LegalTrademarks", 0, (str)pProject->sBuildLegalTrademarks, 0, FALSE);
	(void)xvoTableSetText(pObj, "ProductName", 0, (str)pProject->sBuildProductName, 0, FALSE);
	(void)xvoTableSetText(pObj, "ProductVersion", 0, (str)pProject->sBuildProductVersion, 0, FALSE);
	(void)xvoTableSetBool(pObj, "UseFileVersion", 0, pProject->bBuildUseFileVersion ? TRUE : FALSE);
	(void)xvoTableSetText(pObj, "OriginalFilename", 0, (str)pProject->sBuildOriginalFilename, 0, FALSE);
	(void)xvoTableSetInt(pObj, "FileVersion1", 0, pProject->iBuildFileVersion1);
	(void)xvoTableSetInt(pObj, "FileVersion2", 0, pProject->iBuildFileVersion2);
	(void)xvoTableSetInt(pObj, "FileVersion3", 0, pProject->iBuildFileVersion3);
	(void)xvoTableSetInt(pObj, "FileVersion4", 0, pProject->iBuildFileVersion4);
	(void)xvoTableSetInt(pObj, "FileVersionAutoAdd", 0, pProject->iBuildFileVersionAutoAdd);
	(void)xvoTableSetText(pObj, "Charset", 0, (str)pProject->sBuildCharset, 0, FALSE);
	(void)xvoTableSetValue(pRoot, "Build", 0, pObj, TRUE);

	pArr = xvoCreateArray();
	for ( i = 0; i < pProject->iLibraryCount; i++ ) {
		(void)xvoArrayAppendText(pArr, (str)pProject->arrLibraries[i].sName, 0, FALSE);
	}
	(void)xvoTableSetValue(pRoot, "Library", 0, pArr, TRUE);

	pArr = xvoCreateArray();
	for ( i = 0; i < pProject->iFileCount; i++ ) {
		xvalue pFile = xideWorkspaceCreateFileJson(&pProject->arrFiles[i]);
		if ( pFile != NULL ) (void)xvoArrayAppendValue(pArr, pFile, TRUE);
	}
	(void)xvoTableSetValue(pRoot, "File", 0, pArr, TRUE);

	pArr = xvoCreateArray();
	for ( i = 0; i < pProject->iResourceCount; i++ ) {
		xvalue pFile = xideWorkspaceCreateFileJson(&pProject->arrResources[i]);
		if ( pFile != NULL ) (void)xvoArrayAppendValue(pArr, pFile, TRUE);
	}
	(void)xvoTableSetValue(pRoot, "Resource", 0, pArr, TRUE);

	(void)xvoTableSetText(pRoot, "BootCode", 0, (str)pProject->sBootCode, 0, FALSE);
	return pRoot;
}

int xideWorkspaceSaveXlpAs(const xide_workspace_project_t* pProject, const char* sFile)
{
	xvalue pRoot;
	int ret;
	if ( pProject == NULL || sFile == NULL || sFile[0] == '\0' ) return 0;
	if ( !xideWorkspaceEnsureParentDir(sFile) ) return 0;
	pRoot = xideWorkspaceCreateProjectJson(pProject);
	if ( pRoot == NULL ) return 0;
	ret = xrtStringifyJSON_File((str)sFile, pRoot, TRUE);
	xvoUnref(pRoot);
	return ret ? 1 : 0;
}

int xideWorkspaceSaveXlp(const xide_workspace_project_t* pProject)
{
	if ( pProject == NULL || pProject->sFile[0] == '\0' ) return 0;
	return xideWorkspaceSaveXlpAs(pProject, pProject->sFile);
}

int xideWorkspaceLoadXlw(xide_workspace_t* pWorkspace, const char* sFile)
{
	char sAbs[XIDE_WORKSPACE_PATH_MAX];
	char sDir[XIDE_WORKSPACE_PATH_MAX];
	char sName[XIDE_WORKSPACE_NAME_MAX];
	xvalue pRoot;
	xvalue pArr;
	uint32 i;
	uint32 count;
	if ( pWorkspace == NULL || sFile == NULL || sFile[0] == '\0' ) return 0;
	xideWorkspacePathAbs(sAbs, (int)sizeof(sAbs), NULL, sFile);
	pRoot = xrtParseJSON_File((str)sAbs);
	if ( pRoot == NULL || !xvoIsTable(pRoot) ) {
		if ( pRoot != NULL ) xvoUnref(pRoot);
		return 0;
	}
	xideWorkspaceClear(pWorkspace);
	xideWorkspaceCopy(pWorkspace->sFile, (int)sizeof(pWorkspace->sFile), sAbs);
	xideWorkspacePathDir(sDir, (int)sizeof(sDir), sAbs);
	xideWorkspaceCopy(pWorkspace->sPath, (int)sizeof(pWorkspace->sPath), sDir);
	xideWorkspacePathNameNoExt(sName, (int)sizeof(sName), sAbs);
	xideWorkspaceCopy(pWorkspace->sName, (int)sizeof(pWorkspace->sName), xideWorkspaceJsonText(pRoot, "Name", sName));
	pWorkspace->bTemp = 0;
	pWorkspace->bExpanded = 1;
	if ( xideWorkspaceJsonTable(pRoot, "Status") != NULL ) {
		pWorkspace->bExpanded = xideWorkspaceJsonBool(xideWorkspaceJsonTable(pRoot, "Status"), "IsExpand", 1);
	}

	pArr = xideWorkspaceJsonArray(pRoot, "Projects");
	if ( pArr != NULL ) {
		count = xvoArrayItemCount(pArr);
		for ( i = 0; i < count; i++ ) {
			xvalue pItem = xvoArrayGetValue(pArr, i);
			const char* sProjPath;
			char sProjFile[XIDE_WORKSPACE_PATH_MAX];
			if ( !xvoIsTable(pItem) ) continue;
			sProjPath = xideWorkspaceJsonText(pItem, "Path", "");
			if ( sProjPath[0] == '\0' ) continue;
			xideWorkspacePathAbs(sProjFile, (int)sizeof(sProjFile), pWorkspace->sPath, sProjPath);
			if ( !xideWorkspaceLoadXlp(pWorkspace, sProjFile, NULL) ) {
				xvoUnref(pRoot);
				return 0;
			}
		}
	}

	pArr = xideWorkspaceJsonArray(pRoot, "Files");
	if ( pArr != NULL ) {
		count = xvoArrayItemCount(pArr);
		for ( i = 0; i < count; i++ ) {
			xvalue pItem = xvoArrayGetValue(pArr, i);
			const char* sLoosePath;
			char sLooseFile[XIDE_WORKSPACE_PATH_MAX];
			if ( !xvoIsTable(pItem) ) continue;
			sLoosePath = xideWorkspaceJsonText(pItem, "Path", "");
			if ( sLoosePath[0] == '\0' ) continue;
			xideWorkspacePathAbs(sLooseFile, (int)sizeof(sLooseFile), pWorkspace->sPath, sLoosePath);
			(void)xideWorkspaceAddLooseFile(pWorkspace, NULL, sLooseFile, xideWorkspaceJsonBool(pItem, "Open", 0));
		}
	}
	xvoUnref(pRoot);
	pWorkspace->bChanged = 0;
	return 1;
}

static xvalue xideWorkspaceCreateWorkspaceJson(const xide_workspace_t* pWorkspace, const char* sSaveFile)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pStatus;
	xvalue pProjects;
	xvalue pFiles;
	char sBase[XIDE_WORKSPACE_PATH_MAX];
	int i;
	if ( pRoot == NULL || pWorkspace == NULL ) return pRoot;
	xideWorkspacePathDir(sBase, (int)sizeof(sBase), sSaveFile);
	(void)xvoTableSetInt(pRoot, "Version", 0, 1);
	(void)xvoTableSetText(pRoot, "Name", 0, (str)pWorkspace->sName, 0, FALSE);
	(void)xvoTableSetInt(pRoot, "NewFileIndex", 0, 1);
	pStatus = xvoCreateTable();
	(void)xvoTableSetBool(pStatus, "IsExpand", 0, pWorkspace->bExpanded ? TRUE : FALSE);
	(void)xvoTableSetValue(pRoot, "Status", 0, pStatus, TRUE);

	pProjects = xvoCreateArray();
	for ( i = 0; i < pWorkspace->iProjectCount; i++ ) {
		xvalue pObj = xvoCreateTable();
		char sRel[XIDE_WORKSPACE_PATH_MAX];
		xideWorkspacePathRel(sRel, (int)sizeof(sRel), sBase, pWorkspace->arrProjects[i].sFile);
		(void)xvoTableSetText(pObj, "Path", 0, (str)sRel, 0, FALSE);
		(void)xvoTableSetBool(pObj, "Open", 0, TRUE);
		(void)xvoArrayAppendValue(pProjects, pObj, TRUE);
	}
	(void)xvoTableSetValue(pRoot, "Projects", 0, pProjects, TRUE);

	pFiles = xvoCreateArray();
	for ( i = 0; i < pWorkspace->iLooseFileCount; i++ ) {
		xvalue pObj = xvoCreateTable();
		char sRel[XIDE_WORKSPACE_PATH_MAX];
		xideWorkspacePathRel(sRel, (int)sizeof(sRel), sBase, pWorkspace->arrLooseFiles[i].sPath);
		(void)xvoTableSetText(pObj, "Path", 0, (str)sRel, 0, FALSE);
		(void)xvoTableSetBool(pObj, "Open", 0, pWorkspace->arrLooseFiles[i].bOpen ? TRUE : FALSE);
		(void)xvoArrayAppendValue(pFiles, pObj, TRUE);
	}
	(void)xvoTableSetValue(pRoot, "Files", 0, pFiles, TRUE);
	return pRoot;
}

int xideWorkspaceSaveXlwAs(const xide_workspace_t* pWorkspace, const char* sFile)
{
	xvalue pRoot;
	int ret;
	if ( pWorkspace == NULL || sFile == NULL || sFile[0] == '\0' ) return 0;
	if ( !xideWorkspaceEnsureParentDir(sFile) ) return 0;
	pRoot = xideWorkspaceCreateWorkspaceJson(pWorkspace, sFile);
	if ( pRoot == NULL ) return 0;
	ret = xrtStringifyJSON_File((str)sFile, pRoot, TRUE);
	xvoUnref(pRoot);
	return ret ? 1 : 0;
}

int xideWorkspaceSaveXlw(const xide_workspace_t* pWorkspace)
{
	if ( pWorkspace == NULL || pWorkspace->sFile[0] == '\0' ) return 0;
	return xideWorkspaceSaveXlwAs(pWorkspace, pWorkspace->sFile);
}

static int xideWorkspaceResolveItemPath(const char* sBase, const xide_workspace_item_t* pItem, char* sPath, int iCapacity)
{
	if ( sPath == NULL || iCapacity <= 0 ) return 0;
	sPath[0] = '\0';
	if ( pItem == NULL || pItem->sPath[0] == '\0' ) return 0;
	xideWorkspacePathAbs(sPath, iCapacity, sBase, pItem->sPath);
	return sPath[0] != '\0';
}

int xideWorkspaceResolveNodePath(const xide_workspace_t* pWorkspace, int iNodeId, char* sPath, int iCapacity)
{
	const xide_workspace_project_t* pProject;
	int i;
	if ( sPath == NULL || iCapacity <= 0 ) return 0;
	sPath[0] = '\0';
	if ( pWorkspace == NULL || iNodeId <= 0 ) return 0;
	for ( i = 0; i < pWorkspace->iLooseFileCount; i++ ) {
		if ( pWorkspace->arrLooseFiles[i].iNodeId == iNodeId ) {
			return xideWorkspaceResolveItemPath(pWorkspace->sPath, &pWorkspace->arrLooseFiles[i], sPath, iCapacity);
		}
	}
	for ( i = 0; i < pWorkspace->iProjectCount; i++ ) {
		int j;
		pProject = &pWorkspace->arrProjects[i];
		if ( pProject->iNodeId == iNodeId ) {
			xideWorkspacePathAbs(sPath, iCapacity, pWorkspace->sPath, pProject->sFile);
			return sPath[0] != '\0';
		}
		for ( j = 0; j < pProject->iFileCount; j++ ) {
			if ( pProject->arrFiles[j].iNodeId == iNodeId ) {
				return xideWorkspaceResolveItemPath(pProject->sPath, &pProject->arrFiles[j], sPath, iCapacity);
			}
		}
		for ( j = 0; j < pProject->iLibraryCount; j++ ) {
			if ( pProject->arrLibraries[j].iNodeId == iNodeId ) {
				return xideWorkspaceResolveItemPath(pProject->sPath, &pProject->arrLibraries[j], sPath, iCapacity);
			}
		}
		for ( j = 0; j < pProject->iResourceCount; j++ ) {
			if ( pProject->arrResources[j].iNodeId == iNodeId ) {
				return xideWorkspaceResolveItemPath(pProject->sPath, &pProject->arrResources[j], sPath, iCapacity);
			}
		}
	}
	return 0;
}

int xideWorkspaceGetNodeCount(const xide_workspace_t* pWorkspace)
{
	const xide_workspace_project_t* pProject;
	int i;
	int count;
	if ( pWorkspace == NULL ) return 0;
	count = 1 + pWorkspace->iLooseFileCount;
	for ( i = 0; i < pWorkspace->iProjectCount; i++ ) {
		pProject = &pWorkspace->arrProjects[i];
		count += 4;
		count += pProject->iFileCount;
		count += pProject->iLibraryCount;
		count += pProject->iResourceCount;
	}
	return count;
}

const xide_workspace_project_t* xideWorkspaceFindProjectByNodeId(const xide_workspace_t* pWorkspace, int iNodeId)
{
	int i;
	if ( pWorkspace == NULL ) return NULL;
	for ( i = 0; i < pWorkspace->iProjectCount; i++ ) {
		if ( pWorkspace->arrProjects[i].iNodeId == iNodeId ) return &pWorkspace->arrProjects[i];
	}
	return NULL;
}

static const xide_workspace_item_t* xideWorkspaceFindInItems(const xide_workspace_item_t* pItems, int iCount, int iNodeId)
{
	int i;
	if ( pItems == NULL ) return NULL;
	for ( i = 0; i < iCount; i++ ) {
		if ( pItems[i].iNodeId == iNodeId ) return &pItems[i];
	}
	return NULL;
}

const xide_workspace_item_t* xideWorkspaceFindItemByNodeId(const xide_workspace_t* pWorkspace, int iNodeId)
{
	const xide_workspace_project_t* pProject;
	const xide_workspace_item_t* pItem;
	int i;
	if ( pWorkspace == NULL ) return NULL;
	pItem = xideWorkspaceFindInItems(pWorkspace->arrLooseFiles, pWorkspace->iLooseFileCount, iNodeId);
	if ( pItem != NULL ) return pItem;
	for ( i = 0; i < pWorkspace->iProjectCount; i++ ) {
		pProject = &pWorkspace->arrProjects[i];
		pItem = xideWorkspaceFindInItems(pProject->arrFiles, pProject->iFileCount, iNodeId);
		if ( pItem != NULL ) return pItem;
		pItem = xideWorkspaceFindInItems(pProject->arrLibraries, pProject->iLibraryCount, iNodeId);
		if ( pItem != NULL ) return pItem;
		pItem = xideWorkspaceFindInItems(pProject->arrResources, pProject->iResourceCount, iNodeId);
		if ( pItem != NULL ) return pItem;
	}
	return NULL;
}
