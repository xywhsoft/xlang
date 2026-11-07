#ifndef XIDE_SETTINGS_VIEW_H
#define XIDE_SETTINGS_VIEW_H
#include "app/xide_app.h"
enum { XIDE_SETTINGS_EDITOR,XIDE_SETTINGS_DOCUMENTS,XIDE_SETTINGS_COMPILER,XIDE_SETTINGS_APPEARANCE,XIDE_SETTINGS_LANGUAGE,XIDE_SETTINGS_PAGE_COUNT };
enum { XIDE_PREF_CODE_PATH,XIDE_PREF_CODE_SIZE,XIDE_PREF_TAB,XIDE_PREF_INDENT,XIDE_PREF_EXPAND,XIDE_PREF_LINES,XIDE_PREF_FOLD,XIDE_PREF_MINIMAP,XIDE_PREF_WRAP,
 XIDE_PREF_DOC_PATH,XIDE_PREF_DOC_SIZE,XIDE_PREF_DOC_GAP,XIDE_PREF_DOC_WRAP,XIDE_PREF_COMPILER,XIDE_PREF_UI_PATH,XIDE_PREF_UI_SIZE,XIDE_PREF_THEME,
 XIDE_PREF_AUTO,XIDE_PREF_DELAY,XIDE_PREF_COMPLETION,XIDE_PREF_SIGNATURE,XIDE_PREF_HELP_FOLLOW,XIDE_PREF_COUNT,
 XIDE_PREF_SEARCH=XIDE_PREF_COUNT,XIDE_PREF_APPLY,XIDE_PREF_OK,XIDE_PREF_CANCEL,XIDE_PREF_DEFAULTS,XIDE_PREF_PREVIEW,XIDE_PREF_QUERY };
int xideSettingsViewCreate(xide_app*);
void xideSettingsViewDestroy(xide_app*);
void xideSettingsViewStop(xide_app*);
int xideSettingsViewOpen(xide_app*,int);
int xideSettingsViewIsOpen(xide_app*);
int xideSettingsViewApply(xide_app*);
int xideSettingsViewAction(xide_app*,int);
void xideSettingsViewLayout(xide_app*);
void xideSettingsViewPoll(xide_app*);
xui_widget xideSettingsViewWindow(xide_app*);
xui_widget xideSettingsViewControl(xide_app*,int);
const char* xideSettingsViewMessage(xide_app*);
#endif
