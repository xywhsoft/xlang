#ifndef XIDE_SYMBOL_RENAME_VIEW_H
#define XIDE_SYMBOL_RENAME_VIEW_H
#include "app/xide_app.h"
enum xide_symbol_rename_control {XIDE_SYMBOL_NAME,XIDE_SYMBOL_PREVIEW,XIDE_SYMBOL_APPLY,
 XIDE_SYMBOL_CANCEL,XIDE_SYMBOL_FILES,XIDE_SYMBOL_PATH,XIDE_SYMBOL_RECOVERY,XIDE_SYMBOL_CONTROL_COUNT};
enum xide_symbol_rename_phase {XIDE_SYMBOL_CLOSED,XIDE_SYMBOL_INPUT,XIDE_SYMBOL_PLANNING,
 XIDE_SYMBOL_CONFIRM,XIDE_SYMBOL_APPLYING,XIDE_SYMBOL_CANCELLING,XIDE_SYMBOL_RESULT,XIDE_SYMBOL_UNCERTAIN};
int xideSymbolRenameCanOpen(xide_app*);
int xideSymbolRenameOpen(xide_app*);
int xideSymbolRenameIsModal(xide_app*);
int xideSymbolRenameAllowsAction(xide_app*,int);
/* 0: defer exit until asynchronous cancellation settles;1: ordinary quit;
 *2: explicit consent to discard unsaved edits and keep uncertain recovery data.
 * Does not resume a lost/uncertain reader graph or replay a journal. */
int xideSymbolRenameBeforeQuit(xide_app*);
/* Native UI regression inspection; borrowed controls/status only. */
xui_widget xideSymbolRenameWindow(xide_app*);
xui_widget xideSymbolRenameControl(xide_app*,int);
int xideSymbolRenamePhase(xide_app*);
const char* xideSymbolRenameMessage(xide_app*);
#endif
