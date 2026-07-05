#ifndef XIDE_WORKSPACE_H
#define XIDE_WORKSPACE_H

#define XIDE_WORKSPACE_NAME_MAX 128
#define XIDE_WORKSPACE_PATH_MAX 520
#define XIDE_WORKSPACE_PROJECT_MAX 32
#define XIDE_WORKSPACE_FILE_MAX 256
#define XIDE_WORKSPACE_LIBRARY_MAX 128
#define XIDE_WORKSPACE_RESOURCE_MAX 128

typedef enum xide_workspace_node_kind_t {
	XIDE_WS_NODE_WORKSPACE = 1,
	XIDE_WS_NODE_PROJECT,
	XIDE_WS_NODE_SOURCE_GROUP,
	XIDE_WS_NODE_LIBRARY_GROUP,
	XIDE_WS_NODE_RESOURCE_GROUP,
	XIDE_WS_NODE_FILE,
	XIDE_WS_NODE_LIBRARY,
	XIDE_WS_NODE_RESOURCE
} xide_workspace_node_kind_t;

typedef struct xide_workspace_item_t {
	int iNodeId;
	int iKind;
	int bOpen;
	char sName[XIDE_WORKSPACE_NAME_MAX];
	char sPath[XIDE_WORKSPACE_PATH_MAX];
} xide_workspace_item_t;

typedef struct xide_workspace_project_t {
	int iNodeId;
	int iSourceNodeId;
	int iLibraryNodeId;
	int iResourceNodeId;
	int bChanged;
	int bExpanded;
	int bSourceExpanded;
	int bLibraryExpanded;
	int bResourceExpanded;
	char sName[XIDE_WORKSPACE_NAME_MAX];
	char sFile[XIDE_WORKSPACE_PATH_MAX];
	char sPath[XIDE_WORKSPACE_PATH_MAX];
	char sBuildFile[XIDE_WORKSPACE_NAME_MAX];
	xide_workspace_item_t arrFiles[XIDE_WORKSPACE_FILE_MAX];
	int iFileCount;
	xide_workspace_item_t arrLibraries[XIDE_WORKSPACE_LIBRARY_MAX];
	int iLibraryCount;
	xide_workspace_item_t arrResources[XIDE_WORKSPACE_RESOURCE_MAX];
	int iResourceCount;
} xide_workspace_project_t;

typedef struct xide_workspace_t {
	int iNodeId;
	int iNextNodeId;
	int bTemp;
	int bChanged;
	int bExpanded;
	char sName[XIDE_WORKSPACE_NAME_MAX];
	char sFile[XIDE_WORKSPACE_PATH_MAX];
	char sPath[XIDE_WORKSPACE_PATH_MAX];
	xide_workspace_project_t arrProjects[XIDE_WORKSPACE_PROJECT_MAX];
	int iProjectCount;
	xide_workspace_item_t arrLooseFiles[XIDE_WORKSPACE_FILE_MAX];
	int iLooseFileCount;
} xide_workspace_t;

void xideWorkspaceInit(xide_workspace_t* pWorkspace);
void xideWorkspaceClear(xide_workspace_t* pWorkspace);
int xideWorkspaceCreateDefault(xide_workspace_t* pWorkspace);
xide_workspace_project_t* xideWorkspaceAddProject(xide_workspace_t* pWorkspace, const char* sName, const char* sFile, const char* sPath);
xide_workspace_item_t* xideWorkspaceProjectAddFile(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath, int bOpen);
xide_workspace_item_t* xideWorkspaceProjectAddLibrary(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath);
xide_workspace_item_t* xideWorkspaceProjectAddResource(xide_workspace_t* pWorkspace, xide_workspace_project_t* pProject, const char* sName, const char* sPath);
int xideWorkspaceGetNodeCount(const xide_workspace_t* pWorkspace);
const xide_workspace_item_t* xideWorkspaceFindItemByNodeId(const xide_workspace_t* pWorkspace, int iNodeId);
const xide_workspace_project_t* xideWorkspaceFindProjectByNodeId(const xide_workspace_t* pWorkspace, int iNodeId);

#endif
