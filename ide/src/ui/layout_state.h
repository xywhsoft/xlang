#ifndef XLANG_IDE_LAYOUT_STATE_H
#define XLANG_IDE_LAYOUT_STATE_H
#include "app/xide_app.h"
xvalue* xideLayoutCapture(xide_app* app,xide_model_error* error);
int xideLayoutRestore(xide_app* app,const xvalue* state,xide_model_error* error);
#endif