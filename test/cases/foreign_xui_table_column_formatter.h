#ifndef XL6_XUI_TABLE_COLUMN_FORMATTER_H
#define XL6_XUI_TABLE_COLUMN_FORMATTER_H

#include <xge3_runtime.h>

void fixtureTableCheckGridRows(xui_widget grid, int count, int firstHeight);
void fixtureTableUseRawFormatter(xui_widget table);
void fixtureTableCheckFormatterOwners(xui_widget table);
void fixtureTableMeasureFormatter(xui_widget table, int native);
void fixtureTableMeasureColumnRenderer(xui_widget table, xui_surface target, int native);

#endif
