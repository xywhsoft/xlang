#ifndef XL_TEST_XUI_TABLE_HEADER_OWNED_H
#define XL_TEST_XUI_TABLE_HEADER_OWNED_H

#include <xge3_runtime.h>

void fixtureTableShareMeasureNative(xui_table_view_cell_output_t *cell);
void fixtureTableShareBegin(void);
void fixtureTableShareEnd(int native);
void fixtureTableShareSameCell(xui_table_view_cell_output_t *a,xui_table_view_cell_output_t *b);
void fixtureTableShareDropLast(void);
void fixtureTableHeaderFailAllocation(int allowance);

#endif
