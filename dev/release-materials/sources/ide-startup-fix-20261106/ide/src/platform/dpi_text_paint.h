#ifndef XLANG_IDE_DPI_TEXT_PAINT_H
#define XLANG_IDE_DPI_TEXT_PAINT_H
#include "xui.h"

/* Move the two retained paint owners into a DPI carrier. The caller keeps
 * the logical cluster/caret arrays; physical is cleared on success. Failure
 * leaves both input owners unchanged. No native callback sees the carrier. */
int xideDpiTextPaintWrap(xui_text_shape_t* logical,xui_text_shape_t* physical,float scale);
void xideDpiTextPaintLogical(const xui_text_shape_t* shape,xui_text_shape_t* output);
int xideDpiTextPaintPhysical(const xui_text_shape_t* shape,float scale,xui_text_shape_t* output);
#endif
