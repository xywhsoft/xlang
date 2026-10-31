#include "app_xrt.h"
#include "theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

typedef struct ide_palette {
	const char* name;
	const char* label;
	int dark;
	uint32_t colors[IDE_COLOR_COUNT];
} ide_palette;

static const ide_palette palettes[] = {
	{"classic-light", "经典浅色", 0, {
		0xf1f5f9ff, 0xf8fafcff, 0xffffffff, 0xcbd5e1ff, 0x263445ff, 0x596b80ff, 0x94a3b8ff,
		0x2563ebff, 0xdbeafeff, 0xe8eff8ff, 0x255dc7ff, 0xffffffff, 0xf1f5f9ff,
		0xf3f6faff, 0xb5c3d4ff, 0x899fb9ff, 0xbc3030ff, 0x16804aff, 0x98610aff,
		0x2563c5ff, 0xffedb7ff, 0xe2e8f0ff, 0x475569ff, 0x14203540,
		0x00000000, 0x7c3fb2ff, 0x126c88ff, 0xa54e16ff, 0x26753cff, 0x65748aff
	}},
	{"graphite-dark", "石墨深色", 1, {
		0x191c22ff, 0x22262eff, 0x1b1f26ff, 0x414957ff, 0xe1e6edff, 0xa2adbdff, 0x687487ff,
		0x8ab4ffff, 0x344c70ff, 0x303845ff, 0x375d91ff, 0xffffffff, 0x2d3440ff,
		0x252b34ff, 0x4e5b6fff, 0x6d7e96ff, 0xff9292ff, 0x83d4a3ff, 0xf0c57dff,
		0x8ab4ffff, 0x594b2bff, 0x14171cff, 0xacb8c9ff, 0x00000070,
		0x00000000, 0xc9a0ffff, 0x77d4ddff, 0xf5b57dff, 0xa0d69aff, 0x95a4b8ff
	}},
	{"warm-white", "暖纸白", 0, {
		0xf1ede4ff, 0xfaf7f0ff, 0xfffcf6ff, 0xd6cbbbff, 0x443c32ff, 0x796b59ff, 0xa99c8bff,
		0x956128ff, 0xf0dec0ff, 0xf0e9dcff, 0x875b2eff, 0xfffcf6ff, 0xf2ebdfff,
		0xf4efe5ff, 0xc4b59eff, 0xa39176ff, 0xae3733ff, 0x397347ff, 0x956014ff,
		0x356f91ff, 0xf8e3b2ff, 0xe9e1d2ff, 0x70604cff, 0x39291435,
		0x00000000, 0x85537eff, 0x286e78ff, 0x9b5729ff, 0x4c743dff, 0x807660ff
	}},
	{"ocean", "海洋蓝", 1, {
		0x101f2aff, 0x172d3aff, 0x102430ff, 0x355568ff, 0xd9ebf5ff, 0x98b7c9ff, 0x618397ff,
		0x6bd5e8ff, 0x244f65ff, 0x203e50ff, 0x286077ff, 0xf0fbffff, 0x203b4bff,
		0x1a3241ff, 0x3d637aff, 0x58859cff, 0xff9f96ff, 0x7bd8b2ff, 0xf1ca84ff,
		0x83cfffff, 0x554a2eff, 0x0c1922ff, 0xa1c0d1ff, 0x000b1670,
		0x00000000, 0xb9b1ffff, 0x69d6d8ff, 0xf0bd86ff, 0x9cdbadff, 0x89adbdff
	}}
};

static const char* tokens[] = {
	"ide.bg", "ide.panel", "ide.field", "ide.border", "ide.text", "ide.muted", "ide.disabled",
	"ide.accent", "ide.selection", "ide.hover", "ide.active", "ide.on-accent", "ide.button",
	"ide.alt", "ide.scroll", "ide.scroll-hover", "ide.error", "ide.success", "ide.warning",
	"ide.info", "ide.warning-bg", "ide.canvas", "ide.canvas-text", "ide.shadow",
	"ide.transparent", "ide.keyword", "ide.type", "ide.number", "ide.string", "ide.comment"
};

/* Explicit XUI contracts, grouped by semantic role. Only paint properties are
 * set here: a theme change must not modify metrics, layout or document state. */
typedef struct color_rules { ide_color_role role; const char* keys; } color_rules;
static const color_rules rules[] = {
	{IDE_BG, "panel.background.color panel.header.color menubar.background.color toolbar.background.color "
	 "statusbar.background.color dockpanel.background.color dockpanel.caption.color dockpanel.tab.color "
	 "dockpanel.splitter.color dockpanel.auto_hide.color tabs.background.color tabs.tab.color "
	 "window.titlebar.color window.titlebar.inactive_color window.titlebar.topmost_color "
	 "tableview.header.color scrollframe.corner.color scrollview.corner.color "
	 "codeedit.margin.background_color codeedit.minimap.background_color textedit.line_number.background_color "
	 "messagelist.bubble.system_color messagelist.auxiliary.header_color"},
	{IDE_PANEL, "panel.client.color dockpanel.pane.color dockpanel.client.color tabs.client.color "
	 "window.background.color window.client.color menu.panel.color popup.panel.color tooltip.background.color "
	 "msgtip.background.color toast.background.color msgbox.client.color scrollframe.background.color scrollview.background.color "
	 "listview.background.color listview.row.color tableview.background.color tableview.row.color "
	 "treeview.background.color treeview.row.color messagelist.background.color "
	 "messagelist.bubble.other_color messagelist.auxiliary.background_color "
	 "tableview.picker.background_color combobox.popup.panel_color colorpicker.popup.panel_color"},
	{IDE_FIELD, "codeedit.background.color textedit.background.color textedit.background.hover_color "
	 "codeedit.readonly.background_color textedit.background.disabled_color "
	 "terminal.background.color colorpicker.field.color tableview.check.background_color "
	 "treeview.check.background_color "},
	{IDE_BORDER, "panel.border.color button.border_color button.disabled_border_color menu.border.color "
	 "menu.separator.color menubar.border.color toolbar.border.color toolbar.separator.color "
	 "statusbar.border.color dockpanel.border.color dockpanel.auto_hide.border_color dockpanel.tab.disabled_border_color "
	 "tabs.border.color window.border.color popup.border.color tooltip.border.color msgtip.border.color toast.border.color "
	 "textedit.border.color textedit.border.disabled_color textedit.line_number.border_color "
	 "listview.border.color tableview.grid.color treeview.border.color messagelist.border.color "
	 "codeedit.divider.color codeedit.minimap.divider_color codeedit.whitespace.color "
	 "choice.border.color choice.border.disabled_color toggle.track.border_color toggle.track.disabled_border_color "
	 "toggle.thumb.border_color toggle.thumb.disabled_border_color slider.knob.border_color "
	 "separator.color splitlayout.divider.color combobox.popup.border_color combobox.popup.separator_color "
	 "colorpicker.popup.border_color colorpicker.separator.color colorpicker.field.border_color "
	 "tableview.picker.border_color "
	 "msgbox.button.border_color"},
	{IDE_TEXT, "text.color button.icon_color menu.text.color menu.text.hover_color menu.mark.color "
	 "menubar.text.color toolbar.text.color toolbar.icon.color statusbar.text.color "
	 "dockpanel.caption.text_color dockpanel.tab.text_color tabs.text.color window.title.text_color "
	 "window.close.icon_color tooltip.text.color msgtip.text.color toast.text.color msgbox.text.color "
	 "textedit.text.color codeedit.text.color codeedit.text.readonly_color "
	 "codeedit.caret.color codeedit.line_number.active_color codeedit.syntax.operator.color codeedit.syntax.brace.color "
	 "codeedit.assist.text.color terminal.foreground.color terminal.cursor.color terminal.selection.text_color "
	 "listview.text.color listview.text.selected_color tableview.text.color tableview.text.selected_color tableview.header.text_color "
	 "treeview.text.color treeview.text.selected_color treeview.expander.color treeview.expander.selected_color "
	 "treeview.icon.color choice.text.color toggle.inner_text.unchecked_color "
	 "messagelist.text.self_color messagelist.text.other_color messagelist.text.system_color "
	 "combobox.popup.text_color combobox.popup.hover_text_color colorpicker.popup.text_color "
	 "colorpicker.selection.text_color slider.knob.color progress.text_color"},
	{IDE_MUTED, "menu.shortcut.color window.title.inactive_text_color toast.text.muted_color msgbox.text.muted_color "
	 "dockpanel.button.icon_color textedit.line_number.color textedit.placeholder.color "
	 "codeedit.line_number.color codeedit.fold.color codeedit.inline_completion.color codeedit.minimap.text_color "
	 "codeedit.assist.documentation.color codeedit.diagnostic.hint.color "
	 "messagelist.text.meta_color colorpicker.popup.muted_text_color "},
	{IDE_DISABLED, "text.disabled_color menu.text.disabled_color menubar.text.disabled_color "
	 "toolbar.text.disabled_color statusbar.text.disabled_color "
	 "dockpanel.button.disabled_color dockpanel.tab.disabled_text_color tabs.text.disabled_color "
	 "tabs.close.disabled_color textedit.text.disabled_color listview.text.disabled_color tableview.text.disabled_color "
	 "treeview.text.disabled_color choice.text.disabled_color checkbox.check.disabled_color radio.dot.disabled_color "
	 "toggle.thumb.disabled_color slider.knob.disabled_color slider.disabled.color terminal.focus.disabled_color combobox.popup.disabled_text_color "
	 "hyperlink.text.disabled_color"},
	{IDE_BG, "button.disabled_color toolbar.item.disabled_color tabs.tab.disabled_color "
	 "choice.background.disabled_color toggle.track.disabled_color msgbox.button.disabled_color"},
	{IDE_ACCENT, "button.hover_border_color button.active_border_color button.checked_border_color button.focus_color "
	 "menu.focus.color menubar.focus.color toolbar.focus.color statusbar.focus.color statusbar.progress.fill_color "
	 "dockpanel.focus.color dockpanel.splitter.active_color dockpanel.float.border_color dockpanel.tab.indicator_color "
	 "dockpanel.drag.indicator_color dockpanel.drag.insert_border_color dockpanel.drag.preview_border_color "
	 "tabs.tab.focus_color window.border.active_color textedit.border.focus_color textedit.border.hover_color "
	 "textedit.cursor.color "
	 "listview.focus.color tableview.focus.color treeview.focus.color treeview.check.color treeview.check.mark_color "
	 "treeview.icon.folder_tab_color treeview.icon.file_line_color tableview.check.mark_color "
	 "choice.accent.color choice.accent.hover_color choice.accent.active_color choice.focus.color choice.border.hover_color "
	 "checkbox.check.color radio.dot.color toggle.track.active_color slider.fill.color slider.fill.hover_color "
	 "slider.fill.active_color slider.focus.color splitlayout.divider.active_color terminal.focus.color "
	 "terminal.link.hover_color colorpicker.accent.color colorpicker.selection.color "
	 "codeedit.ime.color codeedit.minimap.viewport_border_color codeedit.assist.active_color "
	 "progress.fill_color hyperlink.text.color hyperlink.text.hover_color hyperlink.text.active_color msgbox.button.focus_color "
	 "messagelist.avatar.self_color"},
	{IDE_SELECTION, "button.checked_color button.active_color msgbox.button.active_color dockpanel.button.active_color dockpanel.float.title_color dockpanel.drag.preview_color "
	 "textedit.selection.color codeedit.selection.color "
	 "terminal.selection.color listview.row.selected_color tableview.row.selected_color treeview.row.selected_color "
	 "messagelist.row.selected_color messagelist.bubble.self_color messagelist.text.selection_color "
	 "toolbar.item.checked_color toggle.track.hover_color treeview.expander.selected_background_color "
	 "treeview.expander.selected_active_background_color"},
	{IDE_HOVER, "button.hover_color menu.item.hover_color menubar.item.hover_color toolbar.item.hover_color "
	 "statusbar.item.hover_color dockpanel.tab.hover_color dockpanel.button.hover_color dockpanel.auto_hide.hover_color "
	 "tabs.tab.hover_color tabs.close.hover_background_color window.button.hover_color window.close.hover_color "
	 "listview.row.hover_color tableview.row.hover_color treeview.row.hover_color messagelist.row.hover_color "
	 "choice.background.hover_color treeview.expander.hover_background_color "
	 "combobox.popup.hover_color toast.close.hover_color msgbox.button.hover_color"},
	{IDE_ACTIVE, "menubar.item.active_color toolbar.item.active_color statusbar.item.active_color "
	 "dockpanel.caption.active_color dockpanel.tab.active_color tabs.tab.active_color "
	 "tabs.close.active_background_color window.button.active_color window.close.active_color "
	 "choice.background.active_color treeview.expander.active_background_color"},
	{IDE_ON_ACCENT, "menubar.text.active_color toolbar.text.active_color toolbar.icon.active_color statusbar.text.active_color "
	 "dockpanel.caption.active_text_color dockpanel.tab.active_text_color tabs.text.active_color "
	 "tabs.close.active_color toggle.inner_text.checked_color msgtip.icon.text.color msgbox.icon.text.color progress.fill_text_color"},
	{IDE_BUTTON, "button.normal_color window.button.color choice.background.color toggle.track.color "
	 "toggle.thumb.color slider.track.color colorpicker.track.color msgbox.button.color "
	 "statusbar.progress.track_color progress.track_color"},
	{IDE_ALT, "tableview.row.alt_color tableview.row.disabled_color codeedit.current_line.color "
	 "codeedit.line_number.active_background_color "},
	{IDE_SCROLL, "scrollbar.thumb.color scrollframe.grip.color scrollview.grip.color popup.scrollbar.thumb.color "
	 "popup.scrollbar.grip.color treeview.icon.folder_body_color treeview.icon.file_background_color messagelist.avatar.other_color"},
	{IDE_SCROLL_HOVER, "scrollbar.thumb.hover_color popup.scrollbar.hover.color dockpanel.splitter.hover_color splitlayout.divider.hover_color"},
	{IDE_ERROR, "menu.text.danger_color dockpanel.button.close_icon_color codeedit.diagnostic.error.color "
	 "codeedit.marker.color codeedit.syntax.error.color toast.error.color colorpicker.field.error_border_color tableview.cell.invalid_color"},
	{IDE_SUCCESS, "toast.success.color"},
	{IDE_WARNING, "codeedit.diagnostic.warning.color toast.warning.color tableview.cell.dirty_color"},
	{IDE_INFO, "codeedit.diagnostic.info.color toast.info.color msgtip.icon.color msgbox.icon.color tableview.cell.editing_color"},
	{IDE_WARNING_BG, "textedit.find.active_color textedit.find.result_color "
	 "codeedit.find.active_color codeedit.find.result_color terminal.search.highlight_color"},
	{IDE_SHADOW, "menu.shadow.color popup.shadow.color msgtip.shadow.color toast.shadow.color "
	 "msgbox.backdrop.color splitlayout.shadow.color combobox.popup.shadow_color colorpicker.popup.shadow_color"},
	{IDE_TRANSPARENT, "menubar.item.color toolbar.item.color statusbar.item.color statusbar.highlight.color popup.backdrop.color "
	 "toolbar.separator.highlight_color dockpanel.button.color dockpanel.drag.preview_inner_border_color "
	 "treeview.expander.background_color treeview.expander.disabled_background_color "},
	{IDE_MUTED, "tabs.close.color tabs.close.hover_color toast.close.color"},
	{IDE_KEYWORD, "codeedit.syntax.keyword.color codeedit.syntax.preprocessor.color"},
	{IDE_TYPE, "codeedit.syntax.type.color"},
	{IDE_NUMBER, "codeedit.syntax.number.color"},
	{IDE_STRING, "codeedit.syntax.string.color codeedit.syntax.char.color"},
	{IDE_COMMENT, "codeedit.syntax.comment.color codeedit.minimap.comment_color"}
};

static const color_rules input_rules[] = {
	{IDE_TEXT, "text.color cursor.color"},
	{IDE_DISABLED, "text.disabled_color"},
	{IDE_MUTED, "placeholder.color"},
	{IDE_FIELD, "background.color background.hover_color"},
	{IDE_BG, "background.disabled_color"},
	{IDE_BORDER, "border.color border.disabled_color"},
	{IDE_ACCENT, "border.hover_color border.focus_color"},
	{IDE_SELECTION, "selection.color"}, {IDE_WARNING_BG, "error.background_color"}, {IDE_ERROR, "error.border_color"}
};
static const color_rules picker_rules[] = {
	{IDE_TEXT, "text.color arrow.color"}, {IDE_DISABLED, "text.disabled_color arrow.disabled_color"},
	{IDE_FIELD, "background.color background.hover_color background.open_color"},
	{IDE_BG, "background.disabled_color"}, {IDE_BORDER, "border.color"},
	{IDE_ACCENT, "border.hover_color border.focus_color"}, {IDE_TRANSPARENT, "button.color"},
	{IDE_HOVER, "button.hover_color"}, {IDE_SELECTION, "button.open_color"}
};
static const color_rules scroll_rules[] = {
	{IDE_BG, "track_color"}, {IDE_SCROLL, "thumb_color"}, {IDE_SCROLL_HOVER, "hover_color"},
	{IDE_ACCENT, "active_color focus_color"}, {IDE_BORDER, "disabled_color"}
};
static const color_rules extra_rules[] = {
	{IDE_PANEL, "cascader.popup.panel_color file_dialog.row.color"},
	{IDE_BG, "breadcrumb.background.color file_dialog.path.background.color"},
	{IDE_BORDER, "cascader.popup.border_color cascader.popup.separator_color file_dialog.path.border.color"},
	{IDE_MUTED, "cascader.placeholder.color cascader.popup.muted_text_color breadcrumb.separator.color"},
	{IDE_TEXT, "cascader.popup.text_color cascader.popup.active_text_color breadcrumb.text.color "
	 "file_dialog.text.color file_dialog.text.selected_color"},
	{IDE_DISABLED, "cascader.popup.disabled_text_color breadcrumb.text.disabled_color file_dialog.text.disabled_color"},
	{IDE_HOVER, "cascader.popup.hover_color file_dialog.row.hover_color"},
	{IDE_SELECTION, "cascader.popup.selected_color cascader.popup.active_color file_dialog.row.selected_color"},
	{IDE_ACCENT, "breadcrumb.text.hover_color breadcrumb.text.active_color file_dialog.focus.color"},
	{IDE_SHADOW, "cascader.popup.shadow_color"},
	{IDE_TEXT, "input.decoration.color"}, {IDE_DISABLED, "input.decoration.disabled_color"},
	{IDE_ACCENT, "input.decoration.hover_color input.decoration.active_color"},
	{IDE_BUTTON, "numeric_input.spinner.color scrollbar.button.color popup.scrollbar.button.color"},
	{IDE_HOVER, "numeric_input.spinner.hover_color"}, {IDE_ACTIVE, "numeric_input.spinner.active_color"},
	{IDE_BORDER, "numeric_input.spinner.border_color scrollbar.disabled.color popup.scrollbar.disabled.color"},
	{IDE_TEXT, "numeric_input.spinner.icon_color scrollbar.button.icon_color popup.scrollbar.button.icon_color"},
	{IDE_DISABLED, "numeric_input.spinner.icon_disabled_color"},
	{IDE_BG, "scrollbar.track.color popup.scrollbar.track.color popup.scrollbar.corner.color"},
	{IDE_ACCENT, "scrollbar.thumb.active_color scrollbar.focus.color popup.scrollbar.active.color popup.scrollbar.focus.color"}
};

const char* ideThemeName(int theme) { return theme >= 0 && theme < IDE_THEME_COUNT ? palettes[theme].name : NULL; }
const char* ideThemeLabel(int theme) { return theme >= 0 && theme < IDE_THEME_COUNT ? palettes[theme].label : NULL; }
int ideThemeFind(const char* name)
{
	if (!name) return -1;
	if (!strcmp(name, "light")) return IDE_THEME_CLASSIC;
	if (!strcmp(name, "dark")) return IDE_THEME_GRAPHITE;
	for (int i = 0; i < IDE_THEME_COUNT; ++i) if (!strcmp(name, palettes[i].name)) return i;
	return -1;
}
uint32_t ideThemeColor(int theme, ide_color_role role)
{
	if (theme < 0 || theme >= IDE_THEME_COUNT) theme = IDE_THEME_CLASSIC;
	return role >= 0 && role < IDE_COLOR_COUNT ? palettes[theme].colors[role] : 0;
}

static xui_style_property_t token_property(const char* key, const char* token)
{
	xui_style_property_t p = {0};
	p.iSize = sizeof(p); p.sName = key;
	p.iDirtyFlags = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	p.tValue.iSize = sizeof(p.tValue); p.tValue.iType = XUI_STYLE_VALUE_TOKEN; p.tValue.sText = token;
	return p;
}

typedef struct style_sheet {
	xui_style_property_t props[640];
	char keys[640][96];
	int count;
} style_sheet;

/* A large default sheet is copied/resolved for every widget, including the
 * many hidden device dialogs. Scope native properties to their control type;
 * only generic text and lazy message-service properties remain global. */
static int install_sheet(xui_context context, const style_sheet* sheet)
{
	static const struct { const char* prefix; xui_widget_type (*type)(xui_context); int choice; } scopes[] = {
		{"panel.", xuiPanelGetType, 0}, {"button.", xuiButtonGetType, 0},
		{"input.", xuiInputGetType, 0}, {"numeric_input.", xuiNumericInputGetType, 0},
		{"textedit.", xuiTextEditGetType, 0},
		{"codeedit.", xuiCodeEditGetType, 0}, {"terminal.", xuiTerminalGetType, 0},
		{"checkbox.", xuiCheckBoxGetType, 1}, {"radio.", xuiRadioGetType, 1}, {"toggle.", xuiToggleGetType, 1},
		{"slider.", xuiSliderGetType, 0}, {"hyperlink.", xuiHyperlinkGetType, 0},
		{"separator.", xuiSeparatorGetType, 0}, {"progress.", xuiProgressGetType, 0},
		{"splitlayout.", xuiSplitLayoutGetType, 0}, {"tabs.", xuiTabsGetType, 0},
		{"window.", xuiWindowGetType, 0}, {"dockpanel.", xuiDockPanelGetType, 0},
		{"menubar.", xuiMenuBarGetType, 0}, {"toolbar.", xuiToolbarGetType, 0}, {"statusbar.", xuiStatusBarGetType, 0},
		{"scrollbar.", xuiScrollBarGetType, 0}, {"scrollframe.", xuiScrollFrameGetType, 0}, {"scrollview.", xuiScrollViewGetType, 0},
		{"listview.", xuiListViewGetType, 0}, {"treeview.", xuiTreeViewGetType, 0},
		{"tableview.", xuiTableViewGetType, 0}, {"messagelist.", xuiMessageListGetType, 0},
		{"menu.", xuiMenuGetType, 0}, {"popup.", xuiPopupGetType, 0},
		{"combobox.", xuiComboBoxGetType, 0}, {"colorpicker.", xuiColorPickerGetType, 0},
		{"cascader.", xuiCascaderGetType, 0}, {"breadcrumb.", xuiBreadcrumbGetType, 0}
	};
	xui_style_property_t selected[640];
	unsigned char assigned[640] = {0};
	int result = xuiStyleBeginUpdate(context);
	if (result != XUI_OK) return result;
	for (int i = 0; i < COUNT(scopes) && result == XUI_OK; ++i) {
		int count = 0;
		for (int j = 0; j < sheet->count; ++j)
			if (!strncmp(sheet->props[j].sName, scopes[i].prefix, strlen(scopes[i].prefix)) ||
			    (scopes[i].choice && !strncmp(sheet->props[j].sName, "choice.", 7))) {
				selected[count++] = sheet->props[j]; assigned[j] = 1;
			}
		xui_style_desc_t desc = {sizeof(desc), NULL, NULL, selected, count};
		result = xuiStyleSetType(context, scopes[i].type(context), &desc);
	}
	int count = 0;
	for (int j = 0; j < sheet->count; ++j) if (!assigned[j]) selected[count++] = sheet->props[j];
	if (result == XUI_OK) result = xuiStyleSetDefault(context, selected, count);
	int end = xuiStyleEndUpdate(context);
	return result != XUI_OK ? result : end;
}

static int append_rules(style_sheet* sheet, const char* prefix, const color_rules* groups, int count)
{
	for (int i = 0; i < count; ++i) {
		const char* start = groups[i].keys;
		while (*start) {
			size_t len = strcspn(start, " ");
			if (len) {
				int n = sheet->count;
				if (n >= COUNT(sheet->props)) return XUI_ERROR_OUT_OF_MEMORY;
				int written = snprintf(sheet->keys[n], sizeof(sheet->keys[n]), "%s%.*s", prefix, (int)len, start);
				if (written < 0 || written >= (int)sizeof(sheet->keys[n])) return XUI_ERROR_INVALID_ARGUMENT;
				sheet->props[n] = token_property(sheet->keys[n], tokens[groups[i].role]);
				++sheet->count;
			}
			start += len; if (*start) ++start;
		}
	}
	return XUI_OK;
}

int ideThemeInit(xui_context context, int theme)
{
	if (!context || !ideThemeName(theme)) return XUI_ERROR_INVALID_ARGUMENT;
	/* Types register their public color contracts lazily. Register every type
 * used by this host, including editors/popups created after the initial frame. */
	xui_widget_type (*types[])(xui_context) = {
		xuiLabelGetType, xuiHyperlinkGetType, xuiPanelGetType, xuiSeparatorGetType, xuiProgressGetType,
		xuiCodeEditGetType, xuiInputGetType, xuiNumericInputGetType, xuiTextEditGetType,
		xuiButtonGetType, xuiCheckBoxGetType, xuiRadioGetType, xuiToggleGetType, xuiScrollBarGetType,
		xuiSliderGetType, xuiTerminalGetType, xuiSplitLayoutGetType, xuiTabsGetType, xuiWindowGetType,
		xuiDockPanelGetType, xuiScrollFrameGetType, xuiScrollViewGetType, xuiListViewGetType,
		xuiMessageListGetType, xuiTreeViewGetType, xuiTableViewGetType, xuiPopupGetType,
		xuiMenuGetType, xuiMenuBarGetType, xuiToolbarGetType, xuiStatusBarGetType, xuiComboBoxGetType, xuiColorPickerGetType,
		xuiCascaderGetType, xuiBreadcrumbGetType
	};
	for (int i = 0; i < COUNT(types); ++i) if (!types[i](context)) return XUI_ERROR_OUT_OF_MEMORY;
	style_sheet* sheet = calloc(1, sizeof(*sheet));
	if (!sheet) return XUI_ERROR_OUT_OF_MEMORY;
	int result = append_rules(sheet, "", rules, COUNT(rules));
#define APPEND(prefix, group) do { if (result == XUI_OK) result = append_rules(sheet, prefix, group, COUNT(group)); } while (0)
	APPEND("input.", input_rules); APPEND("numeric_input.", input_rules);
	APPEND("combobox.", picker_rules); APPEND("colorpicker.", picker_rules);
	APPEND("cascader.", picker_rules);
	APPEND("listview.scrollbar.", scroll_rules); APPEND("tableview.scrollbar.", scroll_rules);
	APPEND("treeview.scrollbar.", scroll_rules); APPEND("", extra_rules);
#undef APPEND
	if (result == XUI_OK) {
		if (sheet->count >= COUNT(sheet->props)) result = XUI_ERROR_OUT_OF_MEMORY;
		else sheet->props[sheet->count++] = token_property("codeedit.minimap.viewport_color", "ide.minimap-viewport");
	}
	char ansi_tokens[16][32];
	for (int i = 0; i < 16 && result == XUI_OK; ++i) {
		int n = sheet->count++;
		if (n >= COUNT(sheet->props)) { result = XUI_ERROR_OUT_OF_MEMORY; break; }
		snprintf(ansi_tokens[i], sizeof(ansi_tokens[i]), "ide.ansi.%d", i);
		snprintf(sheet->keys[n], sizeof(sheet->keys[n]), "terminal.palette.%d", i);
		sheet->props[n] = token_property(sheet->keys[n], ansi_tokens[i]);
	}
	for (int i = 0; i < sheet->count && result == XUI_OK; ++i) {
		const char* key = sheet->props[i].sName;
		/* Services without a public type accessor register on first display.
		 * XUI keeps these documented named properties until then. */
		if (!xuiStyleFindProperty(context, key) && strncmp(key, "tooltip.", 8) &&
		    strncmp(key, "msgtip.", 7) && strncmp(key, "toast.", 6) && strncmp(key, "msgbox.", 7) && strncmp(key, "file_dialog.", 12)) {
			fprintf(stderr, "IDE theme: unknown XUI color property: %s\n", key);
			result = XUI_ERROR_INVALID_STATE;
		}
	}
	if (result == XUI_OK) result = install_sheet(context, sheet);
	free(sheet);
	for (int i = 0; i < IDE_COLOR_COUNT && result == XUI_OK; ++i) {
		xui_style_property_t prop = token_property("text.color", tokens[i]);
		xui_style_desc_t desc = {sizeof(desc), NULL, NULL, &prop, 1};
		result = xuiStyleSetClass(context, tokens[i], &desc);
	}
	return result == XUI_OK ? ideThemeApply(context, theme) : result;
}

int ideThemeApply(xui_context context, int theme)
{
	static const uint32_t ansi[2][16] = {
		{0x263445ff,0xb63333ff,0x26753cff,0x916109ff,0x235eb9ff,0x8843a3ff,0x167785ff,0xd8dee6ff,
		 0x626f80ff,0xc33b36ff,0x267540ff,0x936509ff,0x286bceff,0x9c449cff,0x12818aff,0xf8fafcff},
		{0x263445ff,0xf38b8bff,0x8bcf9eff,0xe8c27aff,0x8ab4ffff,0xc4a0f5ff,0x77cdd5ff,0xdce4eeff,
		 0x8392a7ff,0xffababff,0xa3e5b2ff,0xffd893ff,0xa8ccffff,0xe0b6ffff,0x94e5edff,0xffffffff}
	};
	if (!context || !ideThemeName(theme)) return XUI_ERROR_INVALID_ARGUMENT;
	int result = xuiStyleBeginUpdate(context);
	if (result != XUI_OK) return result;
	for (int i = 0; i < IDE_COLOR_COUNT + 16 && result == XUI_OK; ++i) {
		xui_style_value_t value = {0};
		char name[32];
		value.iSize = sizeof(value); value.iType = XUI_STYLE_VALUE_COLOR;
		value.iColor = i < IDE_COLOR_COUNT ? palettes[theme].colors[i] : ansi[palettes[theme].dark][i - IDE_COLOR_COUNT];
		if (i >= IDE_COLOR_COUNT) snprintf(name, sizeof(name), "ide.ansi.%d", i - IDE_COLOR_COUNT);
		result = xuiStyleSetToken(context, i < IDE_COLOR_COUNT ? tokens[i] : name, &value);
	}
	if (result == XUI_OK) {
		/* Keep the source strokes visible below the minimap's viewport. */
		xui_style_value_t value = {0};
		value.iSize = sizeof(value); value.iType = XUI_STYLE_VALUE_COLOR;
		value.iColor = (palettes[theme].colors[IDE_ACCENT] & 0xffffff00u) | 46u;
		result = xuiStyleSetToken(context, "ide.minimap-viewport", &value);
	}
	int end = xuiStyleEndUpdate(context);
	return result != XUI_OK ? result : end;
}

int ideThemeSetTextRole(xui_widget widget, ide_color_role role)
{
	if (!widget || role < 0 || role >= IDE_COLOR_COUNT) return XUI_ERROR_INVALID_ARGUMENT;
	for (int i = 0; i < IDE_COLOR_COUNT; ++i) if (i != (int)role) (void)xuiWidgetRemoveStyleClass(widget, tokens[i]);
	return xuiWidgetAddStyleClass(widget, tokens[role]);
}

