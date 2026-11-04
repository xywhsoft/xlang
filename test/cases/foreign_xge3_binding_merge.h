#ifndef XL6_XGE3_BINDING_MERGE_H
#define XL6_XGE3_BINDING_MERGE_H
#include <xge3_runtime.h>
/* Test setup/observation only: these are not package API adapters. */
int binding_merge_column(xui_widget table);
int binding_merge_row_span(const xui_table_view_cell_output_t* output);
#endif
