#ifndef XL6_TEST_TABLE_CELL_OPTIONAL_TEXT_H
#define XL6_TEST_TABLE_CELL_OPTIONAL_TEXT_H
#include <xge3_runtime.h>

static int fixtureCellDuringCall(xui_table_view_cell_output_t* cell,
    int (*visit)(void*), void* user)
{
    xui_table_cell_scope_t scope = {0};
    int status = xuiTableViewCellOutputCallScopeBegin(cell, &scope);
    if (status != XUI_OK)
        return status;
    status = visit(user);
    int end = xuiTableViewCellOutputCallScopeEnd(&scope);
    return status == XUI_OK ? end : status;
}
#endif
