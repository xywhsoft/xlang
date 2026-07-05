#include "views/xide_views.h"
#include "i18n/xide_i18n.h"

#include <string.h>

typedef struct xide_task_row_t {
	int stateId;
	const char* command;
	const char* target;
	int messageId;
} xide_task_row_t;

static const xide_task_row_t g_rows[] = {
	{XIDE_TR_TASK_IDLE, "xl run", "main.xl", XIDE_TR_TASK_WAITING},
	{XIDE_TR_TASK_IDLE, "xl build", "project", XIDE_TR_TASK_WAITING},
	{XIDE_TR_TASK_IDLE, "dap", "main.xl", XIDE_TR_TASK_NOT_CONNECTED}
};

static int xideTaskCount(xui_widget pWidget, void* pUser)
{
	(void)pWidget;
	(void)pUser;
	return (int)(sizeof(g_rows) / sizeof(g_rows[0]));
}

static int xideTaskCell(xui_widget pWidget, int iRow, int iColumn, xui_table_view_cell_t* pCell, void* pUser)
{
	const xide_task_row_t* row;
	xide_app_t* pApp = (xide_app_t*)pUser;
	(void)pWidget;
	if ( pCell == NULL || iRow < 0 || iRow >= (int)(sizeof(g_rows) / sizeof(g_rows[0])) ) return XUI_ERROR_INVALID_ARGUMENT;
	row = &g_rows[iRow];
	memset(pCell, 0, sizeof(*pCell));
	if ( iColumn == 0 ) pCell->sText = xideTr(pApp->pContext, row->stateId);
	else if ( iColumn == 1 ) pCell->sText = row->command;
	else if ( iColumn == 2 ) pCell->sText = row->target;
	else pCell->sText = xideTr(pApp->pContext, row->messageId);
	return XUI_OK;
}

int xideCreateTaskView(xide_app_t* pApp, xui_widget* ppView)
{
	xui_table_view_desc_t desc;
	xui_table_view_column_t cols[4];
	int ret;
	memset(cols, 0, sizeof(cols));
	cols[0] = (xui_table_view_column_t){1, "state", xideTr(pApp->pContext, XIDE_TR_TASK_STATE), 70.0f, 50.0f, 160.0f, 1, 1, 1, 1, XUI_TEXT_ALIGN_LEFT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0}};
	cols[1] = (xui_table_view_column_t){2, "command", xideTr(pApp->pContext, XIDE_TR_TASK_COMMAND), 100.0f, 70.0f, 240.0f, 1, 1, 1, 1, XUI_TEXT_ALIGN_LEFT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0}};
	cols[2] = (xui_table_view_column_t){3, "target", xideTr(pApp->pContext, XIDE_TR_TASK_TARGET), 130.0f, 80.0f, 300.0f, 1, 1, 1, 1, XUI_TEXT_ALIGN_LEFT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0}};
	cols[3] = (xui_table_view_column_t){4, "message", xideTr(pApp->pContext, XIDE_TR_TASK_MESSAGE), 220.0f, 120.0f, 500.0f, 1, 1, 1, 1, XUI_TEXT_ALIGN_LEFT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, {0, 0, 0, 0}};
	memset(&desc, 0, sizeof(desc));
	desc.iSize = sizeof(desc);
	desc.arrColumns = cols;
	desc.iColumnCount = 4;
	desc.onCount = xideTaskCount;
	desc.onCell = xideTaskCell;
	desc.pAdapterUser = pApp;
	desc.pFont = pApp->pUiFont;
	desc.fDefaultRowHeight = 26.0f;
	desc.fHeaderHeight = 28.0f;
	desc.iSelectionMode = XUI_TABLE_VIEW_SELECTION_ROW;
	desc.iScrollbarMode = XUI_SCROLLBAR_POLICY_AUTO;
	ret = xuiTableViewCreate(pApp->pContext, &pApp->pTaskTable, &desc);
	if ( ret != XUI_OK ) return ret;
	*ppView = pApp->pTaskTable;
	return XUI_OK;
}
