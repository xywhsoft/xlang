#ifndef XIDE_PREFERENCES_H
#define XIDE_PREFERENCES_H
#include "app/xide_app.h"
int xidePreferencesStartDocument(xide_app*);
int xidePreferencesEditor(xide_app*,xui_widget,const xide_settings_options*);
int xidePreferencesApply(xide_app*,const xide_settings_draft*,xide_model_error*);
#endif
