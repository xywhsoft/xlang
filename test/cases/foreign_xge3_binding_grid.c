#include "test/cases/foreign_xge3_binding_grid.h"
int binding_grid_prepare(xui_widget grid)
{
    xui_table_view_column_t column={0};column.sId="test";column.sTitle="Custom";
    column.fWidth=100.0f;column.iType=XUI_TABLE_CELL_TYPE_CUSTOM;
    int status=xuiTableGridSetColumns(grid,&column,1);
    return status==XUI_OK ? xuiTableGridSetRows(grid,NULL,2):status;
}
