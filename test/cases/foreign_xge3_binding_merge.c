#include "test/cases/foreign_xge3_binding_merge.h"
int binding_merge_column(xui_widget table)
{
    xui_table_view_column_t column={0};
    column.sId="test";column.sTitle="Column";column.fWidth=80.0f;
    return xuiTableViewSetColumns(table,&column,1);
}
int binding_merge_row_span(const xui_table_view_cell_output_t* output)
{return output ? output->iRowSpan:-1;}
