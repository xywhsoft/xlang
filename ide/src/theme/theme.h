#ifndef XLANG_IDE_THEME_H
#define XLANG_IDE_THEME_H

#include "xui.h"

enum { IDE_THEME_CLASSIC, IDE_THEME_GRAPHITE, IDE_THEME_PAPER, IDE_THEME_OCEAN, IDE_THEME_COUNT };

/* UI semantics only. Image pixels, drawing colors and explicit document/SGR
 * colors belong to their content, never to this palette. */
typedef enum ide_color_role {
	IDE_BG, IDE_PANEL, IDE_FIELD, IDE_BORDER, IDE_TEXT, IDE_MUTED, IDE_DISABLED,
	IDE_ACCENT, IDE_SELECTION, IDE_HOVER, IDE_ACTIVE, IDE_ON_ACCENT, IDE_BUTTON,
	IDE_ALT, IDE_SCROLL, IDE_SCROLL_HOVER, IDE_ERROR, IDE_SUCCESS, IDE_WARNING,
	IDE_INFO, IDE_WARNING_BG, IDE_CANVAS, IDE_CANVAS_TEXT, IDE_SHADOW,
	IDE_TRANSPARENT, IDE_KEYWORD, IDE_TYPE, IDE_NUMBER, IDE_STRING, IDE_COMMENT,
	IDE_COLOR_COUNT
} ide_color_role;

const char* ideThemeName(int theme);
const char* ideThemeLabel(int theme);
int ideThemeFind(const char* name);
uint32_t ideThemeColor(int theme, ide_color_role role);
int ideThemeInit(xui_context context, int theme);
int ideThemeApply(xui_context context, int theme);
/* Labels use a class so changing their role does not erase inline styles. */
int ideThemeSetTextRole(xui_widget widget, ide_color_role role);


#endif
