#include "workspace/xide_workspace.h"

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
	xideWorkspaceCopy(pProject->sName, (int)sizeof(pProject->sName), sName);
	xideWorkspaceCopy(pProject->sFile, (int)sizeof(pProject->sFile), sFile);
	xideWorkspaceCopy(pProject->sPath, (int)sizeof(pProject->sPath), sPath);
	if ( pProject->sName[0] == '\0' ) xideWorkspaceCopy(pProject->sName, (int)sizeof(pProject->sName), "Project");
	if ( pProject->sBuildFile[0] == '\0' ) {
		char sBuild[XIDE_WORKSPACE_NAME_MAX];
		snprintf(sBuild, sizeof(sBuild), "%s.exe", pProject->sName);
		xideWorkspaceCopy(pProject->sBuildFile, (int)sizeof(pProject->sBuildFile), sBuild);
	}
	return pProject;
}

xide_workspace_item_t* xideWorkspaceProjectAddFile(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath, int bOpen)
{
	xide_workspace_item_t* pItem;
	if ( pWorkspace == NULL || pProject == NULL || pProject->iFileCount >= XIDE_WORKSPACE_FILE_MAX ) return NULL;
	pItem = &pProject->arrFiles[pProject->iFileCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pItem->iKind = XIDE_WS_NODE_FILE;
	pItem->bOpen = bOpen ? 1 : 0;
	xideWorkspaceCopy(pItem->sName, (int)sizeof(pItem->sName), sName);
	xideWorkspaceCopy(pItem->sPath, (int)sizeof(pItem->sPath), sPath);
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
	xideWorkspaceCopy(pItem->sName, (int)sizeof(pItem->sName), sName);
	xideWorkspaceCopy(pItem->sPath, (int)sizeof(pItem->sPath), sPath);
	return pItem;
}

xide_workspace_item_t* xideWorkspaceProjectAddResource(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath)
{
	xide_workspace_item_t* pItem;
	if ( pWorkspace == NULL || pProject == NULL || pProject->iResourceCount >= XIDE_WORKSPACE_RESOURCE_MAX ) return NULL;
	pItem = &pProject->arrResources[pProject->iResourceCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iNodeId = xideWorkspaceNextNodeId(pWorkspace);
	pItem->iKind = XIDE_WS_NODE_RESOURCE;
	xideWorkspaceCopy(pItem->sName, (int)sizeof(pItem->sName), sName);
	xideWorkspaceCopy(pItem->sPath, (int)sizeof(pItem->sPath), sPath);
	return pItem;
}

int xideWorkspaceCreateDefault(xide_workspace_t* pWorkspace)
{
	xide_workspace_project_t* pProject;
	if ( pWorkspace == NULL ) return 0;
	xideWorkspaceClear(pWorkspace);
	xideWorkspaceCopy(pWorkspace->sName, (int)sizeof(pWorkspace->sName), "Workspace1");
	pProject = xideWorkspaceAddProject(pWorkspace, "Project1", "Project1\\Project1.xlp", "Project1\\");
	if ( pProject == NULL ) return 0;
	(void)xideWorkspaceProjectAddFile(pWorkspace, pProject, "main.xl", "src\\main.xl", 1);
	(void)xideWorkspaceProjectAddResource(pWorkspace, pProject, "app.ico", "res\\app.ico");
	return 1;
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
