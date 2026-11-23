"""Reviewed XUI managed declarations which header-only inference cannot recover.

These reproduce existing qualified public methods, not native business logic.
Physical declarations and concrete records are checked before emission; no
copy of managed.xl is read to authorize or reconstruct a declaration.
"""
import re,hashlib,json
from pathlib import Path
from collections import Counter

METHODS = {'xuiResourceFind': ['xui_resource xuiResourceFind(xui_context pContext, const char* sName)',
                              'Context',
                              ['extern "xuiResourceFind" @no_raise @c_cstr(0) @c_result("owner_view",0) func findResource(string name) as Resource?']],
 'xuiResourceSetSurface': ['int xuiResourceSetSurface(xui_context pContext, const char* sName, xui_surface pSurface)',
                           'Context',
                           ['extern "xuiResourceSetSurface" @no_raise @c_cstr(0) @c_result("status",0) func setSurfaceResource(string name,Surface surface)']],
 'xuiResourceSetFont': ['int xuiResourceSetFont(xui_context pContext, const char* sName, xui_font pFont)',
                        'Context',
                        ['extern "xuiResourceSetFont" @no_raise @c_cstr(0) @c_result("status",0) func setFontResource(string name,Font font)']],
 'xuiSetTheme': ['int xuiSetTheme(xui_context pContext, const xui_theme_t* pTheme)',
                        'Context',
                        ['extern "xuiSetTheme" @no_raise @c_pack(1,"xui_theme_t",0,15) @c_pack_size(1,"iSize","uint32_t") '
                        '@c_pack_field(1,"iTextColor",0,"value") @c_pack_field(1,"iBackgroundColor",1,"value") '
                        '@c_pack_field(1,"iPanelColor",2,"value") @c_pack_field(1,"iBorderColor",3,"value") '
                        '@c_pack_field(1,"iAccentColor",4,"value") @c_pack_field(1,"iSelectionColor",5,"value") '
                        '@c_pack_field(1,"iStateNormalColor",6,"value") @c_pack_field(1,"iStateHoverColor",7,"value") '
                        '@c_pack_field(1,"iStateActiveColor",8,"value") @c_pack_field(1,"iStateFocusColor",9,"value") '
                        '@c_pack_field(1,"iStateDisabledColor",10,"value") @c_pack_field(1,"fPadding",11,"value") '
                        '@c_pack_field(1,"fSpacing",12,"value") @c_pack_field(1,"fBorderWidth",13,"value") '
                        '@c_pack_field(1,"pFont",14,"view_raw") @c_result("status",0) func setTheme(c_uint textColor,c_uint '
                        'backgroundColor,c_uint panelColor,c_uint borderColor,c_uint accentColor,c_uint selectionColor,c_uint '
                        'normalColor,c_uint hoverColor,c_uint activeColor,c_uint focusColor,c_uint disabledColor,c_float '
                        'padding,c_float spacing,c_float borderWidth,Font? font=null)']],
 'xuiGetTheme': ['int xuiGetTheme(xui_context pContext, xui_theme_t* pTheme)',
                        'Context',
                        ['extern "xuiGetTheme" @no_raise @c_output_record(1,"xui_theme_t","borrowed") @c_status(0) '
                        '@c_output_field(0,"iTextColor") @c_output_field(1,"iBackgroundColor") @c_output_field(2,"iPanelColor") '
                        '@c_output_field(3,"iBorderColor") @c_output_field(4,"iAccentColor") @c_output_field(5,"iSelectionColor") '
                        '@c_output_field(6,"iStateNormalColor") @c_output_field(7,"iStateHoverColor") '
                        '@c_output_field(8,"iStateActiveColor") @c_output_field(9,"iStateFocusColor") '
                        '@c_output_field(10,"iStateDisabledColor") @c_output_field(11,"fPadding") @c_output_field(12,"fSpacing") '
                        '@c_output_field(13,"fBorderWidth") @c_output_field(14,"pFont","retain",retainFont,"zero",rollbackFont) '
                        'func getTheme() as '
                        '(c_uint,c_uint,c_uint,c_uint,c_uint,c_uint,c_uint,c_uint,c_uint,c_uint,c_uint,c_float,c_float,c_float,Font?)']],
 'xl6Xui3CopyToClipboard': ['int xl6Xui3CopyToClipboard(xui_context,xui_data_object,const XL6_CodeOwner*);',
                            'Context',
                            ['extern "xl6Xui3CopyToClipboard" @no_raise @c_code_owner(2) @c_result("status",0) func '
                             'copyToClipboard(DataObject data)']],
 'xl6Xui3ClipboardSetItems': ['int xl6Xui3ClipboardSetItems(xui_context,const xui_clipboard_item_t*,int,const XL6_CodeOwner*);',
                             'Context',
                             ['extern "xl6Xui3ClipboardSetItems" @no_raise @c_code_owner(3) @c_array_group(1,0,2,"int",2) '
                              '@c_array_record(1,"xui_clipboard_item_t") @c_array_field(1,"sFormat",0,"c_string") '
                              '@c_array_span_field(1,"pData","iDataSize",1,"const void*","size_t") @c_result("status",0) '
                              'func setClipboardItems(array<string> formats,array<bytes> payloads,int size=-1)']],
 'xuiBuiltinAssetGetAtlas': ['int xuiBuiltinAssetGetAtlas(xui_context pContext, xui_surface* ppSurface)',
                             'Context',
                             ['extern "xuiBuiltinAssetGetAtlas" @no_raise @c_output_retain(1,retainSurface,"zero",rollbackSurface) '
                              '@c_output(1,0) func builtinAssetGetAtlas() as Surface']],
 'xuiCodeEditFindAll': ['int xuiCodeEditFindAll(xui_widget pWidget, const xui_find_options_t* pOptions, int* pResultCount)',
                        'CodeEdit',
                        ['extern "xuiCodeEditFindAll" @no_raise @c_output(2,0) @c_pack(1,"xui_find_options_t",0,6) '
                         '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sPattern",0,"c_string") @c_pack_field(1,"iFlags",1,"value") '
                         '@c_pack_field(1,"iStartOffset",2,"value") @c_pack_field(1,"iRangeStart",3,"value") '
                         '@c_pack_field(1,"iRangeEnd",4,"value") @c_pack_field(1,"iMaxResults",5,"value") func findAll(string query,c_uint '
                         'flags=0,c_int start=-1,c_int rangeStart=0,c_int rangeEnd=-1,c_int maxResults=0) as c_int']],
 'xuiCodeEditGetFindResult': ['int xuiCodeEditGetFindResult(xui_widget pWidget, int iIndex, xui_code_find_result_t* pResult)',
                             'CodeEdit',
                             ['extern "xuiCodeEditGetFindResult" @no_raise @c_output_record(2,"xui_code_find_result_t","borrowed") '
                              '@c_status(0) @c_output_field(0,"iStart") @c_output_field(1,"iEnd") @c_output_field(2,"iLine") '
                              '@c_output_field(3,"iColumn") @c_output_field(4,"sPreview","cstring") '
                              'func getFindResult(c_int index) as (c_int,c_int,c_int,c_int,string)']],
 'xuiCodeEditFindNext': ['int xuiCodeEditFindNext(xui_widget pWidget, const xui_find_options_t* pOptions)',
                         'CodeEdit',
                         ['extern "xuiCodeEditFindNext" @no_raise @c_pack(1,"xui_find_options_t",0,6) @c_pack_size(1,"iSize","uint32_t") '
                          '@c_pack_field(1,"sPattern",0,"c_string") @c_pack_field(1,"iFlags",1,"value") '
                          '@c_pack_field(1,"iStartOffset",2,"value") @c_pack_field(1,"iRangeStart",3,"value") '
                          '@c_pack_field(1,"iRangeEnd",4,"value") @c_pack_field(1,"iMaxResults",5,"value") func findNext(string '
                          'query,c_uint flags=0,c_int start=-1,c_int rangeStart=0,c_int rangeEnd=-1,c_int maxResults=0) as c_int']],
 'xuiCodeEditFindPrevious': ['int xuiCodeEditFindPrevious(xui_widget pWidget, const xui_find_options_t* pOptions)',
                             'CodeEdit',
                             ['extern "xuiCodeEditFindPrevious" @no_raise @c_pack(1,"xui_find_options_t",0,6) '
                              '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sPattern",0,"c_string") '
                              '@c_pack_field(1,"iFlags",1,"value") @c_pack_field(1,"iStartOffset",2,"value") '
                              '@c_pack_field(1,"iRangeStart",3,"value") @c_pack_field(1,"iRangeEnd",4,"value") '
                              '@c_pack_field(1,"iMaxResults",5,"value") func findPrevious(string query,c_uint flags=0,c_int start=-1,c_int '
                              'rangeStart=0,c_int rangeEnd=-1,c_int maxResults=0) as c_int']],
 'xuiCodeEditGetOffsetRect': ['int xuiCodeEditGetOffsetRect(xui_widget pWidget, int iOffset, xui_rect_t* pRect)',
                              'CodeEdit',
                              ['extern "xuiCodeEditGetOffsetRect" @no_raise @c_output(2,0) func getOffsetRect(c_int iOffset) as Rect']],
 'xuiCodeEditGetTextView': ['int xuiCodeEditGetTextView(xui_widget pWidget, xstrview* pView)',
                            'CodeEdit',
                            ['extern "xuiCodeEditGetTextView" @no_raise @c_output(1,0) @c_output_view(1,"text") func getText() as '
                             'string?']],
 'xuiCodeEditReplaceAll': ['int xuiCodeEditReplaceAll(xui_widget pWidget, const xui_find_options_t* pOptions, int* pReplaceCount)',
                           'CodeEdit',
                           ['extern "xuiCodeEditReplaceAll" @no_raise @c_output(2,0) @c_pack(1,"xui_find_options_t",0,7) '
                            '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sPattern",0,"c_string") '
                            '@c_pack_field(1,"sReplacement",1,"c_string") @c_pack_field(1,"iFlags",2,"value") '
                            '@c_pack_field(1,"iStartOffset",3,"value") @c_pack_field(1,"iRangeStart",4,"value") '
                            '@c_pack_field(1,"iRangeEnd",5,"value") @c_pack_field(1,"iMaxResults",6,"value") func replaceAll(string '
                            'query,string replacement,c_uint flags=0,c_int start=-1,c_int rangeStart=0,c_int rangeEnd=-1,c_int '
                            'maxResults=0) as c_int']],
 'xuiCodeEditReplaceAllPlain': ['int xuiCodeEditReplaceAllPlain(xui_widget pWidget, const char* sPattern, const char* sReplacement, '
                                'uint32_t iFlags, int* pReplaceCount)',
                                'CodeEdit',
                                ['extern "xuiCodeEditReplaceAllPlain" @no_raise @c_cstr(0) @c_cstr(1) @c_output(4,0) func '
                                 'replaceAllPlain(string query,string replacement,c_uint flags=0) as c_int']],
 'xuiCodeEditReplaceAllRegex': ['int xuiCodeEditReplaceAllRegex(xui_widget pWidget, const char* sPattern, const char* sReplacement, '
                                'uint32_t iFlags, int* pReplaceCount)',
                                'CodeEdit',
                                ['extern "xuiCodeEditReplaceAllRegex" @no_raise @c_cstr(0) @c_cstr(1) @c_output(4,0) func '
                                 'replaceAllRegex(string query,string replacement,c_uint flags=0) as c_int']],
 'xuiCodeEditReplaceCurrent': ['int xuiCodeEditReplaceCurrent(xui_widget pWidget, const xui_find_options_t* pOptions)',
                               'CodeEdit',
                               ['extern "xuiCodeEditReplaceCurrent" @no_raise @c_pack(1,"xui_find_options_t",0,7) '
                                '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sPattern",0,"c_string") '
                                '@c_pack_field(1,"sReplacement",1,"c_string") @c_pack_field(1,"iFlags",2,"value") '
                                '@c_pack_field(1,"iStartOffset",3,"value") @c_pack_field(1,"iRangeStart",4,"value") '
                                '@c_pack_field(1,"iRangeEnd",5,"value") @c_pack_field(1,"iMaxResults",6,"value") func '
                                'replaceCurrent(string query,string replacement,c_uint flags=0,c_int start=-1,c_int rangeStart=0,c_int '
                                'rangeEnd=-1,c_int maxResults=0) as c_int']],
 'xuiCodeEditSetMenuTitle': ['int xuiCodeEditSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)',
                             'CodeEdit',
                             ['extern "xuiCodeEditSetMenuTitle" @no_raise @c_cstr(1) @c_result("status",0) func setMenuTitle(c_int '
                              'iCommand,string sTitle)']],
 'xuiCommandDispatch': ['int xuiCommandDispatch(xui_context pContext, xui_widget pTarget, int iCommand, const char* sCommand, void* pData)',
                        'Context',
                        ['extern "xuiCommandDispatch" @no_raise @c_result("status",0) @c_cstr(2) @c_null(4,"void*") func '
                         'dispatchCommand(Widget? target,c_int command,string name)']],
 'xuiCreateLanguageOwned': ['int xuiCreateLanguageOwned(xui_context pContext, const char* sCode, const char* sName, int '
                            'iFallbackLanguageId, xui_language* pLanguage)',
                            'Context',
                            ['extern "xuiCreateLanguageOwned" @no_raise @c_cstr(0) @c_cstr(1) @c_result("owner_view",0) '
                             '@factory(4,0,rollbackLanguage) func createLanguage(string code,string name,c_int fallback=0) as Language']],
 'xuiCreateOwnedEx': ['int xuiCreateOwnedEx(xui_proxy pProxy, const xui_context_desc_t* pDesc, xui_context* ppContext)',
                      None,
                      ['export extern "xuiCreateOwnedEx" @header("xge3_runtime.h") @no_raise @c_result("owned_view") '
                       '@factory(2,0,rollback) @c_pack(1,"xui_context_desc_t",1,2) @c_pack_size(1,"iSize","uint32_t") '
                       '@c_pack_field(1,"iSystemFontPolicy",1,"value") @c_pack_field(1,"fSystemFontSize",2,"value") func create(Proxy '
                       'proxy,c_int systemFontPolicy,c_float systemFontSize=0) as Context']],
 'xuiDockPanelDockWindowToPaneSide': ['int xuiDockPanelDockWindowToPaneSide(xui_widget pWidget, int iWindow, int iPane, int iSide, float '
                                      'fRatio, int* pPane)',
                                      'DockPanel',
                                      ['extern "xuiDockPanelDockWindowToPaneSide" @no_raise @c_output(5,0) func dockWindowToPaneSide(c_int '
                                       'window,c_int pane,c_int side,c_float ratio=0) as c_int']],
 'xuiDockPanelFindDropTarget': ['int xuiDockPanelFindDropTarget(xui_widget pWidget, int iWindow, float fX, float fY, xui_dock_drop_info_t* '
                                'pInfo)',
                                'DockPanel',
                                ['extern "xuiDockPanelFindDropTarget" @no_raise @c_output(4,0) func findDropTarget(c_int window,c_float '
                                 'x,c_float y) as DockDropInfo']],
 'xuiDragBegin': ['int xuiDragBegin(xui_context pContext, xui_widget pSource, xui_data_object pData, uint32_t iAllowedEffects, uint32_t '
                  'iSuggestedEffect)',
                  'Context',
                  ['extern "xuiDragBegin" @no_raise @c_result("status",0) func startDrag(Widget source,DataObject data,c_uint '
                   'allowedEffects,c_uint suggestedEffect)']],
 'xuiDragCancel': ['int xuiDragCancel(xui_context pContext)',
                   'Context',
                   ['extern "xuiDragCancel" @no_raise @c_result("status",0) func dragCancel()']],
 'xuiEditGetTextView': ['int xuiEditGetTextView(xui_widget pWidget, xstrview* pView)',
                        'Widget',
                        ['extern "xuiEditGetTextView" @no_raise @c_output(1,0) @c_output_view(1,"text") func editGetText() as string?']],
 'xuiGetLanguagePack': ['xui_language xuiGetLanguagePack(xui_context pContext, int iLanguageId)',
                        'Context',
                        ['extern "xuiGetLanguagePack" @no_raise @c_result("owner_view",0) func languagePack(c_int id) as Language?']],
 'xuiHotKeyClearWidget': ['int xuiHotKeyClearWidget(xui_widget pWidget)',
                          'Widget',
                          ['extern "xuiHotKeyClearWidget" @no_raise @c_result("status",0) func hotKeyClearWidget()']],
 'xuiHotKeyRegisterCommand': ['int xuiHotKeyRegisterCommand(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers, int '
                              'iCommand, const char* sCommand, void* pData)',
                              'Context',
                              ['extern "xuiHotKeyRegisterCommand" @no_raise @c_result("status",0) @c_cstr(4) @c_null(6,"void*") func '
                               'hotKeyRegisterCommand(Widget target,c_int key,c_uint modifiers,c_int command,string name)']],
 'xuiHotKeyRegisterOwned': ['int xuiHotKeyRegisterOwned(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers, '
                            'xui_widget_event_proc onEvent, void* pUser, const xui_callback_owner_t* pOwner)',
                            'Context',
                            ['extern "xuiHotKeyRegisterOwned" @no_raise @c_result("status",0) '
                             '@c_owned_callback(3,0,"xui_callback_owner_t",-16) func hotKeyRegister(Widget target,c_int key,c_uint '
                             'modifiers,WidgetEventProc? callback)']],
 'xuiHotKeyUnregister': ['int xuiHotKeyUnregister(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers)',
                         'Context',
                         ['extern "xuiHotKeyUnregister" @no_raise @c_result("status",0) func hotKeyUnregister(Widget pWidget,c_int '
                          'iKey,c_uint iModifiers)']],
 'xuiIconCategoryRemove': ['int xuiIconCategoryRemove(xui_context pContext, const char* sName)',
                           'Context',
                           ['extern "xuiIconCategoryRemove" @no_raise @c_cstr(0) @c_result("status",0) func iconCategoryRemove(string '
                            'sName)']],
 'xuiIconPickerEnsureVisible': ['int xuiIconPickerEnsureVisible(xui_widget pWidget, xui_icon_id iId)',
                                'IconPicker',
                                ['extern "xuiIconPickerEnsureVisible" @no_raise @c_result("status",0) func ensureVisible(c_uint id)']],
 'xuiIconPickerGetCategory': ['xui_icon_category xuiIconPickerGetCategory(xui_widget pWidget)',
                              'IconPicker',
                              ['extern "xuiIconPickerGetCategory" @no_raise @c_result("owner_view",0) func category() as IconCategory?']],
 'xuiIconPickerGetMetrics': ['int xuiIconPickerGetMetrics(xui_widget pWidget, float* pBorderWidth, float* pScrollbarSize)',
                             'IconPicker',
                             ['extern "xuiIconPickerGetMetrics" @no_raise @c_outputs(1,2) @c_status(0) func getMetrics() as '
                              '(c_float,c_float)']],
 'xuiIconPickerGetSelectedIcon': ['xui_icon xuiIconPickerGetSelectedIcon(xui_widget pWidget)',
                                  'IconPicker',
                                  ['extern "xuiIconPickerGetSelectedIcon" @no_raise @c_result("owner_view",0) func selectedIcon() as '
                                   'Icon?']],
 'xuiIconPickerGetSelectedId': ['xui_icon_id xuiIconPickerGetSelectedId(xui_widget pWidget)',
                                'IconPicker',
                                ['extern "xuiIconPickerGetSelectedId" @no_raise func selectedId() as c_uint']],
 'xuiIconPickerSetCategory': ['int xuiIconPickerSetCategory(xui_widget pWidget, xui_icon_category pCategory)',
                              'IconPicker',
                              ['extern "xuiIconPickerSetCategory" @no_raise @c_result("status",0) func setCategory(IconCategory? '
                               'category=null)']],
 'xuiIconPickerSetFormatterOwned': ['int xuiIconPickerSetFormatterOwned(xui_widget pWidget, xui_icon_picker_text_owned_proc onFormat, '
                                    'void* pUser, const xui_callback_owner_t* pOwner)',
                                    'IconPicker',
                                    ['extern "xuiIconPickerSetFormatterOwned" @no_raise '
                                     '@c_owned_callback(0,0,"xui_callback_owner_t",0,"owned_text") @c_result("status",0) func '
                                     'setFormatter(IconPickerTextProc? onFormat)']],
 'xuiIconPickerSetSelectedId': ['int xuiIconPickerSetSelectedId(xui_widget pWidget, xui_icon_id iId)',
                                'IconPicker',
                                ['extern "xuiIconPickerSetSelectedId" @no_raise @c_result("status",0) func setSelectedId(c_uint id)']],
 'xuiInputImeCompositionEx': ['int xuiInputImeCompositionEx(xui_context pContext, const xui_ime_composition_t* pComposition)',
                              'Context',
                              ['extern "xuiInputImeCompositionEx" @no_raise @c_result("status",0) @c_span(0,"const char*","int") '
                               '@c_pack(1,"xui_ime_composition_t",0,9) @c_pack_size(1,"iSize","uint32_t") '
                               '@c_pack_field(1,"sText",0,"span_data") @c_pack_field(1,"iTextSize",0,"span_count") '
                               '@c_pack_field(1,"bActive",1,"value") @c_pack_field(1,"iCursor",2,"value") '
                               '@c_pack_field(1,"iSelectionStart",3,"value") @c_pack_field(1,"iSelectionEnd",4,"value") '
                               '@c_pack_field(1,"bReplacementRange",5,"value") @c_pack_field(1,"iReplacementStart",6,"value") '
                               '@c_pack_field(1,"iReplacementEnd",7,"value") @c_pack_field(1,"iAttribute",8,"value") func '
                               'inputImeCompositionEx(string text,c_int active=1,c_int cursor=0,c_int selectionStart=0,c_int '
                               'selectionEnd=0,c_int hasReplacement=0,c_int replacementStart=0,c_int replacementEnd=0,c_int attribute=0)']],
 'xuiInputKeyDownEx': ['int xuiInputKeyDownEx(xui_context pContext, int iKey, uint32_t iModifiers, uint32_t* pResult)',
                       'Context',
                       ['extern "xuiInputKeyDownEx" @no_raise @c_output(3,0) func inputKeyDownEx(c_int key,c_uint modifiers=0) as c_uint']],
 'xuiInputKeyUpEx': ['int xuiInputKeyUpEx(xui_context pContext, int iKey, uint32_t iModifiers, uint32_t* pResult)',
                     'Context',
                     ['extern "xuiInputKeyUpEx" @no_raise @c_output(3,0) func inputKeyUpEx(c_int key,c_uint modifiers=0) as c_uint']],
 'xuiInputRedo': ['int xuiInputRedo(xui_widget pWidget)', 'Input', ['extern "xuiInputRedo" @no_raise @c_result("status",0) func redo()']],
 'xuiInputSetMaxLength': ['int xuiInputSetMaxLength(xui_widget pWidget, int iMaxLength)',
                          'Input',
                          ['extern "xuiInputSetMaxLength" @no_raise @c_result("status",0) func setMaxLength(c_int iMaxLength)']],
 'xuiInputTextEx': ['int xuiInputTextEx(xui_context pContext, uint32_t iCodepoint, uint32_t* pResult)',
                    'Context',
                    ['extern "xuiInputTextEx" @no_raise @c_output(2,0) func inputTextEx(c_uint codepoint) as c_uint']],
 'xuiInputUndo': ['int xuiInputUndo(xui_widget pWidget)', 'Input', ['extern "xuiInputUndo" @no_raise @c_result("status",0) func undo()']],
 'xuiInventoryGridGetVisibleRange': ['int xuiInventoryGridGetVisibleRange(xui_widget pWidget, xui_inventory_visible_range_t* pRange)',
                                     'InventoryGrid',
                                     ['extern "xuiInventoryGridGetVisibleRange" @no_raise @c_output(1,0) func getVisibleRange() as '
                                      'InventoryVisibleRange']],
 'xuiInventoryGridHitTest': ['int xuiInventoryGridHitTest(xui_widget pWidget, float fX, float fY, xui_inventory_hit_t* pHit)',
                             'InventoryGrid',
                             ['extern "xuiInventoryGridHitTest" @no_raise @c_output(3,0) func hitTest(c_float worldX,c_float worldY) as '
                              'InventoryHit']],
 'xuiInventoryGridToXValue': ['int xuiInventoryGridToXValue(xui_widget pWidget, xvalue** ppValue)',
                             'InventoryGrid',
                             ['extern "xuiInventoryGridToXValue" @no_raise @c_output(1,0) @c_output_view(1,"owned_value") func '
                              'toValue() as var']],
 'xuiLanguageSetText': ['int xuiLanguageSetText(xui_context pContext, xui_language pLanguage, int iTextId, const char* sText)',
                        'Context',
                        ['extern "xuiLanguageSetText" @no_raise @c_cstr(2) @c_result("status",0) func setLanguageText(Language '
                         'language,c_int id,string text)',
                         'extern "xuiLanguageSetText" @no_raise @c_null(3,"const char*") @c_result("status",0) func '
                         'clearLanguageText(Language language,c_int id)']],
 'xuiLanguageTouch': ['void xuiLanguageTouch(xui_context pContext, xui_language pLanguage)',
                      'Context',
                      ['extern "xuiLanguageTouch" @no_raise func touchLanguage(Language language)']],
 'xuiMenuAddItem': ['int xuiMenuAddItem(xui_widget pWidget, const xui_menu_item_t* pItem)',
                    'Menu',
                    ['extern "xuiMenuAddItem" @no_raise @c_result("status",0) @c_pack(1,"xui_menu_item_t",0,7) '
                     '@c_pack_field(1,"sText",0,"c_string") @c_pack_field(1,"sShortcut",1,"c_string") @c_pack_field(1,"iType",2,"value") '
                     '@c_pack_field(1,"iState",3,"value") @c_pack_field(1,"iValue",4,"value") @c_pack_field(1,"iIcon",5,"value") '
                     '@c_pack_field(1,"pSubmenu",6,"view_raw") func addItem(string text,string? shortcut=null,c_int type=0,c_uint '
                     'state=1,c_int value=0,c_int icon=0,Menu? submenu=null)']],
 'xuiMenuSetItems': ['int xuiMenuSetItems(xui_widget pWidget, const xui_menu_item_t* pItems, int iCount)',
                     'Menu',
                     ['extern "xuiMenuSetItems" @no_raise @c_result("status",0) @c_array_group(1,0,7,"int",7) '
                      '@c_array_record(1,"xui_menu_item_t") @c_array_field(1,"sText",0,"c_string") '
                      '@c_array_field(1,"sShortcut",1,"c_string") @c_array_field(1,"iType",2,"value") @c_array_field(1,"iState",3,"value") '
                      '@c_array_field(1,"iValue",4,"value") @c_array_field(1,"iIcon",5,"value") @c_array_field(1,"pSubmenu",6,"view_raw") '
                      'func setItemsColumns(array<string?> texts,array<string?> shortcuts,array<c_int> types,array<c_uint> '
                      'states,array<c_int> values,array<c_int> icons,array<Menu?> submenus,int size=-1)']],
 'xuiMessageListAppendNodeText': ['int xuiMessageListAppendNodeText(xui_widget pWidget, const char* sId, const char* sText)',
                                  'MessageList',
                                  ['extern "xuiMessageListAppendNodeText" @no_raise @c_cstr(0) @c_cstr(1) @c_result("status",0) func '
                                   'appendNodeText(string sId,string sText)']],
 'xuiMessageListSetFont': ['int xuiMessageListSetFont(xui_widget pWidget, xui_font pFont)',
                           'MessageList',
                           ['extern "xuiMessageListSetFont" @no_raise @c_result("status",0) func setFont(Font? pFont)']],
 'xuiMessageListSetNodeCollapsed': ['int xuiMessageListSetNodeCollapsed(xui_widget pWidget, const char* sId, int bCollapsed)',
                                    'MessageList',
                                    ['extern "xuiMessageListSetNodeCollapsed" @no_raise @c_cstr(0) @c_result("status",0) func '
                                     'setNodeCollapsed(string sId,c_int bCollapsed)']],
 'xuiMessageListSetNodeTitle': ['int xuiMessageListSetNodeTitle(xui_widget pWidget, const char* sId, const char* sTitle)',
                                'MessageList',
                                ['extern "xuiMessageListSetNodeTitle" @no_raise @c_cstr(0) @c_cstr(1) @c_result("status",0) func '
                                 'setNodeTitle(string sId,string sTitle)']],
 'xuiMessageListUpdateNodeText': ['int xuiMessageListUpdateNodeText(xui_widget pWidget, const char* sId, const char* sText)',
                                  'MessageList',
                                  ['extern "xuiMessageListUpdateNodeText" @no_raise @c_cstr(0) @c_cstr(1) @c_result("status",0) func '
                                   'updateNodeText(string sId,string sText)']],
 'xuiRadioGroupAddOption': ['int xuiRadioGroupAddOption(xui_widget pGroup, xui_widget* ppRadio, const xui_radio_desc_t* pDesc)',
                            'RadioGroup',
                            ['extern "xuiRadioGroupAddOption" @no_raise @c_result("owner_view",0) @factory(1,0,rollbackRadio) '
                             '@c_pack(2,"xui_radio_desc_t",0,9) @c_pack_size(2,"iSize","uint32_t") @c_pack_field(2,"sText",0,"c_string") '
                             '@c_pack_field(2,"pFont",1,"view_raw") @c_pack_field(2,"iTextColor",2,"value") '
                             '@c_pack_field(2,"iDisabledTextColor",3,"value") @c_pack_field(2,"iTextFlags",4,"value") '
                             '@c_pack_field(2,"bChecked",5,"value") @c_pack_field(2,"fIndicatorSize",6,"value") '
                             '@c_pack_field(2,"fGap",7,"value") @c_pack_field(2,"bUseBuiltinAtlas",8,"value") func addOption(string? '
                             'text=null,Font? font=null,c_uint textColor=0,c_uint disabledTextColor=0,c_uint textFlags=0,c_int '
                             'checked=0,c_float indicatorSize=0,c_float gap=0,c_int useBuiltinAtlas=0) as Radio']],
 'xuiReportError': ['int xuiReportError(xui_context pContext, const xui_error_info_t* pError)',
                    'Context',
                    ['extern "xuiReportError" @no_raise @c_result("status",0) @c_pack(1,"xui_error_info_t",0,5) '
                     '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"iCode",0,"value") @c_pack_field(1,"iStage",1,"value") '
                     '@c_pack_field(1,"bRecoverable",2,"value") @c_pack_field(1,"sOperation",3,"c_string") '
                     '@c_pack_field(1,"sMessage",4,"c_string") func reportError(c_int code,c_int stage,c_int recoverable=1,string? '
                     'operation=null,string? message=null)',
                     'extern "xuiReportError" @no_raise @c_result("status",0) @c_pack(1,"xui_error_info_t",0,7) '
                     '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"iCode",0,"value") @c_pack_field(1,"iStage",1,"value") '
                     '@c_pack_field(1,"bRecoverable",2,"value") @c_pack_field(1,"pWidget",3,"view_raw") @c_pack_field(1,"tRect",4,"value") '
                     '@c_pack_field(1,"sOperation",5,"c_string") @c_pack_field(1,"sMessage",6,"c_string") func reportError(c_int '
                     'code,c_int stage,c_int recoverable,Widget? widget,Rect rectangle,string? operation=null,string? message=null)']],
 'xuiSetAccessibilityEventCallbackOwned': ['int xuiSetAccessibilityEventCallbackOwned(xui_context pContext, xui_accessibility_event_proc '
                                           'onEvent, void* pUser, const xui_callback_owner_t* pOwner)',
                                           'Context',
                                           ['extern "xuiSetAccessibilityEventCallbackOwned" @no_raise @c_result("status",0) '
                                            '@c_owned_callback(0,0,"xui_callback_owner_t",0) func '
                                            'setAccessibilityEventCallback(AccessibilityEventProc? callback)']],
 'xuiSetErrorCallbackOwned': ['int xuiSetErrorCallbackOwned(xui_context pContext, xui_error_proc onError, void* pUser, const '
                              'xui_callback_owner_t* pOwner)',
                              'Context',
                              ['extern "xuiSetErrorCallbackOwned" @no_raise @c_result("status",0) '
                               '@c_owned_callback(0,0,"xui_callback_owner_t",0) func setErrorCallback(ErrorProc? callback)']],
 'xuiTableViewGetCellContentRect': ['int xuiTableViewGetCellContentRect(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)',
                                    'TableView',
                                    ['extern "xuiTableViewGetCellContentRect" @no_raise @c_output(3,0) func getCellContentRect(c_int '
                                     'row,c_int column) as Rect']],
 'xuiTableViewGetCellRect': ['int xuiTableViewGetCellRect(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)',
                             'TableView',
                             ['extern "xuiTableViewGetCellRect" @no_raise @c_output(3,0) func getCellRect(c_int row,c_int column) as '
                              'Rect']],
 'xuiTableViewGetItemAt': ['int xuiTableViewGetItemAt(xui_widget pWidget, float fX, float fY, int* pRow, int* pColumn)',
                           'TableView',
                           ['extern "xuiTableViewGetItemAt" @no_raise @c_outputs(3,4) @c_status(0) func getItemAt(c_float x,c_float y) as '
                            '(c_int,c_int)']],
 'xuiTableViewGetOffset': ['int xuiTableViewGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)',
                           'TableView',
                           ['extern "xuiTableViewGetOffset" @no_raise @c_outputs(1,2) @c_status(0) func getOffset() as (c_float,c_float)']],
 'xuiTagInputSetTags': ['int xuiTagInputSetTags(xui_widget pWidget, const char* const* ppTags, int iCount)',
                        'TagInput',
                        ['extern "xuiTagInputSetTags" @no_raise @c_result("status",0) @c_array_group(1,0,1,"int",1) '
                         '@c_array_input(1,0,"const char* const*","c_string") func setTags(array<string> tags,int size=-1)']],
 'xuiTerminalGetCell': ['int xuiTerminalGetCell(xui_widget pWidget, int iColumn, int iRow, xui_terminal_cell_t* pCell)',
                        'Terminal',
                        ['extern "xuiTerminalGetCell" @no_raise @c_output(3,0) func getCell(c_int column,c_int row) as TerminalCell']],
 'xuiTerminalFindText': ['int xuiTerminalFindText(xui_widget pWidget, const char* sText, uint32_t iFlags, int* pLine, int* pColumn)',
                         'Terminal',
                         ['extern "xuiTerminalFindText" @no_raise @c_cstr(0) @c_outputs(-1,3,4) @c_result("integer_min",0) '
                          'func findText(string text,c_uint flags=0) as (c_int,c_int,c_int)']],
 'xuiTerminalFindNext': ['int xuiTerminalFindNext(xui_widget pWidget, const char* sText, uint32_t iFlags, int* pLine, int* pColumn)',
                         'Terminal',
                         ['extern "xuiTerminalFindNext" @no_raise @c_cstr(0) @c_outputs(-1,3,4) @c_result("integer_min",0) '
                          'func findNext(string text,c_uint flags=0) as (c_int,c_int,c_int)']],
 'xuiTerminalFindPrev': ['int xuiTerminalFindPrev(xui_widget pWidget, const char* sText, uint32_t iFlags, int* pLine, int* pColumn)',
                         'Terminal',
                         ['extern "xuiTerminalFindPrev" @no_raise @c_cstr(0) @c_outputs(-1,3,4) @c_result("integer_min",0) '
                          'func findPrevious(string text,c_uint flags=0) as (c_int,c_int,c_int)']],
 'xuiTerminalGetFindMatch': ['int xuiTerminalGetFindMatch(xui_widget pWidget, int* pLine, int* pColumn, int* pLength)',
                             'Terminal',
                             ['extern "xuiTerminalGetFindMatch" @no_raise @c_outputs(-1,1,2,3) @c_result("integer_min",0) '
                              'func getFindMatch() as (c_int,c_int,c_int,c_int)']],
 'xuiTerminalGetLinkAt': ['int xuiTerminalGetLinkAt(xui_widget pWidget, int iLine, int iColumn, char* sBuffer, int iCapacity, '
                         'int* pStartColumn, int* pLength)',
                         'Terminal',
                         ['extern "xuiTerminalGetLinkAt" @no_raise @c_span(2,"char*","int") @c_outputs(-1,5,6) '
                          '@c_result("integer_min",0) func linkAt(c_int line,c_int column,Buffer output) as (c_int,c_int,c_int)']],
 'xuiTextEditFindNext': ['int xuiTextEditFindNext(xui_widget pWidget, const xui_find_options_t* pOptions)',
                         'TextEdit',
                         ['extern "xuiTextEditFindNext" @no_raise @c_pack(1,"xui_find_options_t",0,6) @c_pack_size(1,"iSize","uint32_t") '
                          '@c_pack_field(1,"sPattern",0,"c_string") @c_pack_field(1,"iFlags",1,"value") '
                          '@c_pack_field(1,"iStartOffset",2,"value") @c_pack_field(1,"iRangeStart",3,"value") '
                          '@c_pack_field(1,"iRangeEnd",4,"value") @c_pack_field(1,"iMaxResults",5,"value") func findNext(string '
                          'query,c_uint flags=0,c_int start=-1,c_int rangeStart=0,c_int rangeEnd=-1,c_int maxResults=0) as c_int']],
 'xuiTextEditFindPrevious': ['int xuiTextEditFindPrevious(xui_widget pWidget, const xui_find_options_t* pOptions)',
                             'TextEdit',
                             ['extern "xuiTextEditFindPrevious" @no_raise @c_pack(1,"xui_find_options_t",0,6) '
                              '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sPattern",0,"c_string") '
                              '@c_pack_field(1,"iFlags",1,"value") @c_pack_field(1,"iStartOffset",2,"value") '
                              '@c_pack_field(1,"iRangeStart",3,"value") @c_pack_field(1,"iRangeEnd",4,"value") '
                              '@c_pack_field(1,"iMaxResults",5,"value") func findPrevious(string query,c_uint flags=0,c_int start=-1,c_int '
                              'rangeStart=0,c_int rangeEnd=-1,c_int maxResults=0) as c_int']],
 'xuiTextEditReplaceAll': ['int xuiTextEditReplaceAll(xui_widget pWidget, const xui_find_options_t* pOptions, int* pReplaceCount)',
                           'TextEdit',
                           ['extern "xuiTextEditReplaceAll" @no_raise @c_output(2,0) @c_pack(1,"xui_find_options_t",0,7) '
                            '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sPattern",0,"c_string") '
                            '@c_pack_field(1,"sReplacement",1,"c_string") @c_pack_field(1,"iFlags",2,"value") '
                            '@c_pack_field(1,"iStartOffset",3,"value") @c_pack_field(1,"iRangeStart",4,"value") '
                            '@c_pack_field(1,"iRangeEnd",5,"value") @c_pack_field(1,"iMaxResults",6,"value") func replaceAll(string '
                            'query,string replacement,c_uint flags=0,c_int start=-1,c_int rangeStart=0,c_int rangeEnd=-1,c_int '
                            'maxResults=0) as c_int']],
 'xuiTextEditReplaceCurrent': ['int xuiTextEditReplaceCurrent(xui_widget pWidget, const xui_find_options_t* pOptions)',
                               'TextEdit',
                               ['extern "xuiTextEditReplaceCurrent" @no_raise @c_pack(1,"xui_find_options_t",0,7) '
                                '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sPattern",0,"c_string") '
                                '@c_pack_field(1,"sReplacement",1,"c_string") @c_pack_field(1,"iFlags",2,"value") '
                                '@c_pack_field(1,"iStartOffset",3,"value") @c_pack_field(1,"iRangeStart",4,"value") '
                                '@c_pack_field(1,"iRangeEnd",5,"value") @c_pack_field(1,"iMaxResults",6,"value") func '
                                'replaceCurrent(string query,string replacement,c_uint flags=0,c_int start=-1,c_int rangeStart=0,c_int '
                                'rangeEnd=-1,c_int maxResults=0) as c_int']],
 'xuiTextEditSetFont': ['int xuiTextEditSetFont(xui_widget pWidget, xui_font pFont)',
                        'TextEdit',
                        ['extern "xuiTextEditSetFont" @no_raise @c_result("status",0) func setFont(Font? pFont)']],
 'xuiTextEditSetLineNumbers': ['int xuiTextEditSetLineNumbers(xui_widget pWidget, int bVisible, float fWidth)',
                               'TextEdit',
                               ['extern "xuiTextEditSetLineNumbers" @no_raise @c_result("status",0) func setLineNumbers(c_int '
                                'bVisible,c_float fWidth)']],
 'xuiTextEditSetMaxLength': ['int xuiTextEditSetMaxLength(xui_widget pWidget, int iMaxLength)',
                             'TextEdit',
                             ['extern "xuiTextEditSetMaxLength" @no_raise @c_result("status",0) func setMaxLength(c_int iMaxLength)']],
 'xuiTextEditSetSelection': ['int xuiTextEditSetSelection(xui_widget pWidget, int iStart, int iEnd)',
                             'TextEdit',
                             ['extern "xuiTextEditSetSelection" @no_raise @c_result("status",0) func setSelection(c_int iStart,c_int '
                              'iEnd)']],
 'xuiTextEditSetVisualMetrics': ['int xuiTextEditSetVisualMetrics(xui_widget pWidget, float fBorderWidth, float fLineGap)',
                                 'TextEdit',
                                 ['extern "xuiTextEditSetVisualMetrics" @no_raise @c_result("status",0) func setVisualMetrics(c_float '
                                  'fBorderWidth,c_float fLineGap)']],
 'xuiTextEditSetWordWrap': ['int xuiTextEditSetWordWrap(xui_widget pWidget, int bWordWrap)',
                            'TextEdit',
                            ['extern "xuiTextEditSetWordWrap" @no_raise @c_result("status",0) func setWordWrap(c_int bWordWrap)']],
 'xuiTimeLineViewGetOffset': ['int xuiTimeLineViewGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)',
                              'TimeLineView',
                              ['extern "xuiTimeLineViewGetOffset" @no_raise @c_outputs(1,2) @c_status(0) func getOffset() as '
                               '(c_float,c_float)']],
 'xuiTimeLineViewHitTest': ['int xuiTimeLineViewHitTest(xui_widget pWidget, float fX, float fY, xui_timeline_hit_t* pHit)',
                            'TimeLineView',
                            ['extern "xuiTimeLineViewHitTest" @no_raise @c_output(3,0) func hitTest(c_float x,c_float y) as TimelineHit']],
 'xuiWebViewClose': ['int xuiWebViewClose(xui_widget pWidget)',
                     'WebView',
                     ['extern "xuiWebViewClose" @no_raise @c_result("status",0) func closeBrowser()']],
 'xuiWidgetMeasure': ['int xuiWidgetMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pMeasuredSize)',
                      'Widget',
                      ['extern "xuiWidgetMeasure" @no_raise @c_output(2,0) func measure(Vec2 constraint) as Vec2']],
 'xuiWidgetMeasureContent': ['int xuiWidgetMeasureContent(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pContentSize)',
                             'Widget',
                             ['extern "xuiWidgetMeasureContent" @no_raise @c_output(2,0) func measureContent(Vec2 constraint) as Vec2']],
 'xuiWidgetNotifyAccessibility': ['int xuiWidgetNotifyAccessibility(xui_widget pWidget, int iEventType, uint64_t iNodeId)',
                                  'Widget',
                                  ['extern "xuiWidgetNotifyAccessibility" @no_raise @c_result("status",0) func notifyAccessibility(c_int '
                                   'iEventType,uint64 iNodeId)']]}

METHODS.update({
    'xuiMessageListSetNodeRendererOwned': [
        'int xuiMessageListSetNodeRendererOwned(xui_widget pWidget, xui_message_list_node_renderer_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'MessageList', ['extern "xuiMessageListSetNodeRendererOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) '
        '@c_result("status",0) func setNodeRenderer(MessageNodeRenderProc? renderer)']],
    'xuiMessageListSetNodes': ['int xuiMessageListSetNodes(xui_widget pWidget, const xui_message_node_t* pNodes, int iCount)',
        'MessageList', ['extern "xuiMessageListSetNodes" @no_raise @c_array_group(1,0,9,"int",9) @c_array_record(1,"xui_message_node_t","iSize","uint32_t") '
        '@c_array_field(1,"sId",0,"c_string") @c_array_field(1,"sText",1,"c_string") @c_array_field(1,"sSender",2,"c_string") '
        '@c_array_field(1,"sTime",3,"c_string") @c_array_field(1,"sParentId",4,"c_string") @c_array_field(1,"sTitle",5,"c_string") '
        '@c_array_field(1,"iType",6,"value") @c_array_field(1,"iFlags",7,"value") @c_array_field(1,"iAuxiliaryKind",8,"value") @c_result("status",0) '
        'func setNodes(array<string> ids,array<string?> texts,array<string?> senders,array<string?> times,array<string?> parentIds,array<string?> titles,array<c_int> types,array<c_int> flags,array<c_int> auxiliaryKinds,int size=-1)']],
    'xuiMessageListGetNodeDescriptor': ['int xuiMessageListGetNodeDescriptor(xui_widget pWidget, int iIndex, xui_message_node_t* pNode)',
        'MessageList', ['extern "xuiMessageListGetNodeDescriptor" @no_raise @c_output_record(2,"xui_message_node_t","borrowed") @c_status(0) '
        '@c_output_field(0,"sId","cstring") @c_output_field(1,"sText","cstring") @c_output_field(2,"sSender","cstring") '
        '@c_output_field(3,"sTime","cstring") @c_output_field(4,"sParentId","cstring") @c_output_field(5,"sTitle","cstring") '
        '@c_output_field(6,"iType") @c_output_field(7,"iFlags") @c_output_field(8,"iAuxiliaryKind") '
        'func getNode(c_int index) as (string,string,string,string,string,string,c_int,c_int,c_int)']],
    'xuiTimeLineViewGetLayerDescriptor': ['int xuiTimeLineViewGetLayerDescriptor(xui_widget pWidget, int iLayer, xui_timeline_layer_desc_t* pLayer)',
        'TimeLineView', ['extern "xuiTimeLineViewGetLayerDescriptor" @no_raise @c_output_record(2,"xui_timeline_layer_desc_t","borrowed") @c_status(0) '
        '@c_output_field(0,"iId") @c_output_field(1,"sName","cstring") @c_output_field(2,"bVisible") @c_output_field(3,"bLocked") '
        '@c_output_field(4,"bSelected") @c_output_field(5,"fHeight") @c_output_field(6,"iColor") '
        'func getLayer(c_int layer) as (c_int,string,c_int,c_int,c_int,c_float,c_uint)']],
    'xuiTimeLineViewSetFrame': ['int xuiTimeLineViewSetFrame(xui_widget pWidget, int iLayer, int iFrame, int iType, void* pUser)',
        'TimeLineView', ['extern "xuiTimeLineViewSetFrame" @no_raise @c_null(4,"void*") @c_result("status",0) func setFrame(c_int layer,c_int frame,c_int type)']],
    'xuiTimeLineViewGetFrame': ['int xuiTimeLineViewGetFrame(xui_widget pWidget, int iLayer, int iFrame, xui_timeline_frame_t* pFrame)',
        'TimeLineView', ['extern "xuiTimeLineViewGetFrame" @no_raise @c_output_record(3,"xui_timeline_frame_t","borrowed") @c_status(0) '
        '@c_output_field(0,"iLayer") @c_output_field(1,"iFrame") @c_output_field(2,"iType") func getFrame(c_int layer,c_int frame) as (c_int,c_int,c_int)']],
    'xuiTimeLineViewGetSpanDescriptor': ['int xuiTimeLineViewGetSpanDescriptor(xui_widget pWidget, int iSpanId, xui_timeline_span_desc_t* pSpan)',
        'TimeLineView', ['extern "xuiTimeLineViewGetSpanDescriptor" @no_raise @c_output_record(2,"xui_timeline_span_desc_t","borrowed") @c_status(0) '
        '@c_output_field(0,"iLayer") @c_output_field(1,"iId") @c_output_field(2,"iStartFrame") @c_output_field(3,"iEndFrame") '
        '@c_output_field(4,"iType") @c_output_field(5,"sCustomType","cstring") @c_output_field(6,"sLabel","cstring") @c_output_field(7,"iColor") '
        'func getSpan(c_int id) as (c_int,c_int,c_int,c_int,c_int,string,string,c_uint)']],
    'xuiComboBoxSetItemData': ['int xuiComboBoxSetItemData(xui_widget pWidget, const xui_combobox_item_t* pItems, int iCount)',
        'ComboBox', ['extern "xuiComboBoxSetItemData" @no_raise @c_array_group(1,0,5,"int",5) @c_array_record(1,"xui_combobox_item_t") '
        '@c_array_field(1,"sText",0,"c_string") @c_array_field(1,"iValue",1,"value") @c_array_field(1,"bEnabled",2,"value") '
        '@c_array_field(1,"bSeparator",3,"value") @c_array_field(1,"iIcon",4,"value") @c_result("status",0) '
        'func setItemData(array<string?> texts,array<c_int> values,array<c_int> enabled,array<c_int> separators,array<c_int> icons,int size=-1)']],
    'xuiMenuGetItemDescriptor': [
        'int xuiMenuGetItemDescriptor(xui_widget pWidget, int iIndex, xui_menu_item_t* pItem)', 'Menu',
        ['extern "xuiMenuGetItemDescriptor" @no_raise @c_output_record(2,"xui_menu_item_t","borrowed") @c_status(0)'
         ' @c_output_field(0,"sText","cstring") @c_output_field(1,"sShortcut","cstring") @c_output_field(2,"iType")'
         ' @c_output_field(3,"iState") @c_output_field(4,"iValue") @c_output_field(5,"iIcon")'
         ' @c_output_field(6,"pSubmenu","owner_view") func getItem(c_int index) as (string?,string?,c_int,c_uint,c_int,c_int,Menu?)'
        ]],
    'xuiMenuBarGetItemDescriptor': [
        'int xuiMenuBarGetItemDescriptor(xui_widget pWidget, int iIndex, xui_menubar_item_t* pItem)', 'MenuBar',
        ['extern "xuiMenuBarGetItemDescriptor" @no_raise @c_output_record(2,"xui_menubar_item_t","borrowed") @c_status(0)'
         ' @c_output_field(0,"sText","cstring") @c_output_field(1,"iState") @c_output_field(2,"iValue")'
         ' @c_output_field(3,"iMnemonic") @c_output_field(4,"pMenu","owner_view")'
         ' @c_output_field(5,"tRect") func getItem(c_int index) as (string?,c_int,c_int,c_int,Menu?,Rect)'
        ]],
    'xuiToolbarGetItemDescriptor': [
        'int xuiToolbarGetItemDescriptor(xui_widget pWidget, int iIndex, xui_toolbar_item_t* pItem)', 'Toolbar',
        ['extern "xuiToolbarGetItemDescriptor" @no_raise @c_output_record(2,"xui_toolbar_item_t","borrowed") @c_status(0)'
         ' @c_output_field(0,"sText","cstring") @c_output_field(1,"sTooltip","cstring") @c_output_field(2,"iType")'
         ' @c_output_field(3,"iState") @c_output_field(4,"iValue") @c_output_field(5,"iGroup")'
         ' @c_output_field(6,"pIcon","retain",retainSurface,"zero",rollbackSurface) @c_output_field(7,"tIconSrc")'
         ' @c_output_field(8,"tRect") func getItem(c_int index) as (string?,string?,c_int,c_uint,c_int,c_int,Surface?,Rect,Rect)'
        ]],
    'xuiStatusBarGetItemDescriptor': [
        'int xuiStatusBarGetItemDescriptor(xui_widget pWidget, int iIndex, xui_statusbar_item_t* pItem)', 'StatusBar',
        ['extern "xuiStatusBarGetItemDescriptor" @no_raise @c_output_record(2,"xui_statusbar_item_t","borrowed") @c_status(0)'
         ' @c_output_field(0,"sText","cstring") @c_output_field(1,"iType") @c_output_field(2,"iSection")'
         ' @c_output_field(3,"iState") @c_output_field(4,"iValue") @c_output_field(5,"fWidth") @c_output_field(6,"fFlex")'
         ' @c_output_field(7,"fMin") @c_output_field(8,"fMax") @c_output_field(9,"fValue")'
         ' @c_output_field(10,"tRect") func getItem(c_int index) as (string?,c_int,c_int,c_uint,c_int,c_float,c_float,c_float,c_float,c_float,Rect)'
        ]],
    'xuiComboBoxGetItemDescriptor': ['int xuiComboBoxGetItemDescriptor(xui_widget pWidget, int iIndex, xui_combobox_item_t* pItem)',
        'ComboBox', ['extern "xuiComboBoxGetItemDescriptor" @no_raise @c_output_record(2,"xui_combobox_item_t","borrowed") @c_status(0) '
        '@c_output_field(0,"sText","cstring") @c_output_field(1,"iValue") @c_output_field(2,"bEnabled") @c_output_field(3,"bSeparator") '
        '@c_output_field(4,"iIcon") func getItem(c_int index) as (string,c_int,c_int,c_int,c_int)']],
    'xuiCascaderGetItemDescriptor': [
        'int xuiCascaderGetItemDescriptor(xui_widget pWidget, int iIndex, xui_cascader_item_t* pItem)', 'Cascader',
        ['extern "xuiCascaderGetItemDescriptor" @no_raise @c_output_record(2,"xui_cascader_item_t","borrowed") @c_status(0) '
         '@c_output_field(0,"sText","cstring") @c_output_field(1,"iValue") @c_output_field(2,"iParent") @c_output_field(3,"iFlags") '
         'func getItem(c_int index) as (string,c_int,c_int,c_uint)']],
    'xuiCascaderSetSelectedPath': [
        'int xuiCascaderSetSelectedPath(xui_widget pWidget, const int* arrValues, int iDepth)', 'Cascader',
        ['extern "xuiCascaderSetSelectedPath" @no_raise @c_result("status",0) @c_array_group(1,0,1,"int",1) '
         '@c_array_input(1,0,"const int*","value") func setSelectedPath(array<c_int> values,int size=-1)']],
    'xuiCascaderGetSelectedPathSnapshot': [
        'int xuiCascaderGetSelectedPathSnapshot(xui_widget pWidget, xui_cascader_path_t* pPath)', 'Cascader',
        ['extern "xuiCascaderGetSelectedPathSnapshot" @no_raise @c_output(1,0) func selectedPath() as CascaderPath']],
    'xuiWorkflowWidgetGetWorkflowOwned': ['int xuiWorkflowWidgetGetWorkflowOwned(xui_widget widget,xui_workflow* output)', "WorkflowWidget", ['extern "xuiWorkflowWidgetGetWorkflowOwned" @no_raise @c_result("owned_view") @factory(1,0,rollbackWorkflow) func getWorkflow() as Workflow']],
    'xuiWorkflowWidgetSetWorkflowShared': ['int xuiWorkflowWidgetSetWorkflowShared(xui_widget widget,xui_workflow workflow)', "WorkflowWidget", ['extern "xuiWorkflowWidgetSetWorkflowShared" @no_raise @c_result("status",0) func setWorkflow(Workflow workflow)']],
    'xuiDebugEventTrace': [
        'int xuiDebugEventTrace(xui_context pContext, const xui_event_t* pEvent, char* sBuffer, int iCapacity)', 'Context',
        ['extern "xuiDebugEventTrace" @no_raise @c_span(1,"char*","int") @c_result("status",0) func debugEventTrace(CallbackConstEvent event,Buffer output)']],
    'xuiGetRenderNode': [
        'int xuiGetRenderNode(xui_context pContext, int iIndex, xui_render_node_t* pNode)', 'Context',
        ['extern "xuiGetRenderNode" @no_raise @c_output_record(2,"xui_render_node_t","borrowed") @c_output_size(2,"iSize") @c_status(0) '
         '@c_output_field(0,"tWorldRect") @c_output_field(1,"tPaintRect") @c_output_field(2,"iLayer") @c_output_field(3,"iZIndex") '
         '@c_output_field(4,"iStateId") @c_output_field(5,"iCachePolicy") @c_output_field(6,"iFlags") '
'func getRenderNode(c_int index) as (Rect,Rect,c_int,c_int,c_uint,c_int,c_uint)',
         'extern "xuiGetRenderNode" @no_raise @c_output_record(2,"xui_render_node_t","borrowed") @c_output_size(2,"iSize") @c_status(0) @c_output_field(0,"pWidget","owner_view") @c_output_field(1,"tWorldRect") @c_output_field(2,"tPaintRect") @c_output_field(3,"iLayer") @c_output_field(4,"iZIndex") @c_output_field(5,"iStateId") @c_output_field(6,"iCachePolicy") @c_output_field(7,"iFlags") func getRenderNodeWithWidget(c_int index) as (Widget,Rect,Rect,c_int,c_int,c_uint,c_int,c_uint)']],
    'xuiDebugWidgetInspectAtChecked': [
        'int xuiDebugWidgetInspectAtChecked(xui_context pContext, float fX, float fY, uint32_t iFlags, xui_debug_widget_info_t* pInfo)', 'Context',
        ['extern "xuiDebugWidgetInspectAtChecked" @no_raise @c_output_record(4,"xui_debug_widget_info_t","borrowed") @c_output_size(4,"iSize") @c_status(0) '
         '@c_output_field(0,"pWidget","owner_view") @c_output_field(1,"pParent","owner_view") @c_output_field(2,"tRect") @c_output_field(3,"tWorldRect") '
         '@c_output_field(4,"iDepth") @c_output_field(5,"iChildCount") @c_output_field(6,"bVisible") @c_output_field(7,"bEnabled") @c_output_field(8,"bFocusable") '
         '@c_output_field(9,"bTabStop") @c_output_field(10,"bFocusScope") @c_output_field(11,"bHitTestVisible") @c_output_field(12,"bDragEnabled") @c_output_field(13,"bDropEnabled") '
         '@c_output_field(14,"iImeMode") @c_output_field(15,"iLayer") @c_output_field(16,"iZIndex") @c_output_field(17,"iDirtyFlags") '
         '@c_output_field(18,"iSubtreeDirtyFlags") @c_output_field(19,"iEventMask") @c_output_field(20,"iSubtreeEventMask") '
         'func inspectWidgetAt(c_float x,c_float y,c_uint flags=0) as (Widget?,Widget?,Rect,Rect,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_uint,c_uint,uint64,uint64)']],
    'xuiDebugWidgetInspect': [
        'int xuiDebugWidgetInspect(xui_widget pWidget, xui_debug_widget_info_t* pInfo)', 'Widget',
        ['extern "xuiDebugWidgetInspect" @no_raise @c_output_record(1,"xui_debug_widget_info_t","borrowed") @c_output_size(1,"iSize") @c_status(0) '
         '@c_output_field(0,"iDepth") @c_output_field(1,"iChildCount") @c_output_field(2,"bVisible") @c_output_field(3,"bEnabled") '
         '@c_output_field(4,"bFocusable") @c_output_field(5,"bTabStop") @c_output_field(6,"bFocusScope") @c_output_field(7,"bHitTestVisible") '
         '@c_output_field(8,"bDragEnabled") @c_output_field(9,"bDropEnabled") @c_output_field(10,"iImeMode") @c_output_field(11,"iLayer") '
         '@c_output_field(12,"iZIndex") @c_output_field(13,"iDirtyFlags") @c_output_field(14,"iEventMask") @c_output_field(15,"iSubtreeEventMask") '
         '@c_output_field(16,"tRect") @c_output_field(17,"tWorldRect") @c_output_field(18,"iSubtreeDirtyFlags") '
         'func debugInfo() as (c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_uint,uint64,uint64,Rect,Rect,c_uint)']],
    'xuiDockPanelGetWindowInfo': [
        'int xuiDockPanelGetWindowInfo(xui_widget pWidget, int iWindow, xui_dock_window_info_t* pInfo)', 'DockPanel',
        ['extern "xuiDockPanelGetWindowInfo" @no_raise @c_output_record(2,"xui_dock_window_info_t","borrowed") @c_output_size(2,"iSize") @c_status(0) '
         '@c_output_field(0,"iWindow") @c_output_field(1,"iPane") @c_output_field(2,"iState") @c_output_field(3,"iRegion") '
         '@c_output_field(4,"iLastRegion") @c_output_field(5,"iLastSide") @c_output_field(6,"iLastTabIndex") @c_output_field(7,"bClosable") '
         '@c_output_field(8,"bDockable") @c_output_field(9,"bMovable") @c_output_field(10,"bResizable") @c_output_field(11,"iResizeEdges") '
         '@c_output_field(12,"fMinWidth") @c_output_field(13,"fMinHeight") @c_output_field(14,"tIconSrc") @c_output_field(15,"tRect") '
         '@c_output_field(16,"tClientRect") @c_output_field(17,"tTabRect") @c_output_field(18,"tAutoHideRect") '
'func getWindowInfo(c_int window) as (c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_int,c_uint,c_float,c_float,Rect,Rect,Rect,Rect,Rect)',
         'extern "xuiDockPanelGetWindowInfo" @no_raise @c_output_record(2,"xui_dock_window_info_t","borrowed") @c_output_size(2,"iSize") @c_status(0) @c_output_field(0,"pHostWidget","owner_view") func getWindowHost(c_int window) as Widget']],
    'xuiFlowGraphWidgetGetGraphOwned': [
        'int xuiFlowGraphWidgetGetGraphOwned(xui_widget widget,xui_flow_graph* output)',
        'FlowGraphWidget',
        ['extern "xuiFlowGraphWidgetGetGraphOwned" @no_raise @c_result("owned_view") @factory(1,0,rollbackFlowGraph) func getGraph() as FlowGraph']],
    'xuiFlowGraphWidgetSetGraphShared': [
        'int xuiFlowGraphWidgetSetGraphShared(xui_widget widget,xui_flow_graph graph)',
        'FlowGraphWidget',
        ['extern "xuiFlowGraphWidgetSetGraphShared" @no_raise @c_result("status",0) func setGraph(FlowGraph graph)']],
    'xuiFlowGraphWidgetHitTest': [
        'int xuiFlowGraphWidgetHitTest(xui_widget pWidget, float fX, float fY, xui_flow_hit_t* pHit)',
        'FlowCanvas',
        ['extern "xuiFlowGraphWidgetHitTest" @no_raise @c_output(3,0) func hitTest(c_float x,c_float y) as FlowHit']],
    'xuiFlowGraphWidgetSelectAt': [
        'int xuiFlowGraphWidgetSelectAt(xui_widget pWidget, float fX, float fY, xui_flow_hit_t* pHit)',
        'FlowCanvas',
        ['extern "xuiFlowGraphWidgetSelectAt" @no_raise @c_output(3,0) func selectAt(c_float x,c_float y) as FlowHit']],
    'xuiFlowGraphWidgetGetHoverHit': [
        'int xuiFlowGraphWidgetGetHoverHit(xui_widget pWidget, xui_flow_hit_t* pHit)',
        'FlowCanvas',
        ['extern "xuiFlowGraphWidgetGetHoverHit" @no_raise @c_output(1,0) func getHoverHit() as FlowHit']],
    'xuiWorkflowWidgetGetCanvas': [
        'xui_widget xuiWorkflowWidgetGetCanvas(xui_widget pWidget)',
        'WorkflowWidget',
        ['extern "xuiWorkflowWidgetGetCanvas" @no_raise @c_result("owner_view",0) func getCanvas() as Widget?']],
})

METHODS.update({
    'xuiWebViewSetEventOwned': [
        'int xuiWebViewSetEventOwned(xui_widget pWidget, xui_webview_event_proc onEvent, void* pUser, const xui_callback_owner_t* pOwner)',
        'WebView',
        ['extern "xuiWebViewSetEventOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0) @c_result("status",0) func setEvent(WebViewEventProc? onEvent)']],
    'xuiWebViewGetZoomFactor': [
        'int xuiWebViewGetZoomFactor(xui_widget pWidget, double* pFactor)',
        'WebView',
        ['extern "xuiWebViewGetZoomFactor" @no_raise @c_output(1,0) func getZoomFactor() as c_double']],
})

METHODS.update({
    'xuiChartGetViewRange': [
        'int xuiChartGetViewRange(xui_widget pWidget, double* pMinX, double* pMaxX, double* pMinY, double* pMaxY)',
        'Chart', ['extern "xuiChartGetViewRange" @no_raise @c_outputs(-1,1,2,3,4) @c_result("integer_min",0) func getViewRange() as (c_int,c_double,c_double,c_double,c_double)']],
    'xuiChartSetTooltipCallbackOwned': [
        'int xuiChartSetTooltipCallbackOwned(xui_widget pWidget, xui_chart_tooltip_owned_proc onTooltip, void* pUser, const xui_callback_owner_t* pOwner)',
        'Chart', ['extern "xuiChartSetTooltipCallbackOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0,"owned_text") @c_result("status",0) func setTooltipProvider(ChartTooltipProc? provider)']],
    'xuiChartSetContextMenuOwned': [
        'int xuiChartSetContextMenuOwned(xui_widget pWidget, xui_chart_context_proc onContext, void* pUser, const xui_callback_owner_t* pOwner)',
        'Chart', ['extern "xuiChartSetContextMenuOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setContextMenu(ChartContextProc? callback)']],
    'xuiChartGetBrushRange': [
        'int xuiChartGetBrushRange(xui_widget pWidget, double* pMinX, double* pMaxX, double* pMinY, double* pMaxY)',
        'Chart', ['extern "xuiChartGetBrushRange" @no_raise @c_outputs(-1,1,2,3,4) @c_result("integer_min",0) func getBrushRange() as (c_int,c_double,c_double,c_double,c_double)']],
    'xuiChartGetAnimation': [
        'int xuiChartGetAnimation(xui_widget pWidget, int* pEnabled, float* pDuration, float* pProgress)',
        'Chart', ['extern "xuiChartGetAnimation" @no_raise @c_outputs(-1,1,2,3) @c_result("integer_min",0) func getAnimation() as (c_int,c_int,c_float,c_float)']],
    'xuiChartGetSeriesAreaFill': [
        'int xuiChartGetSeriesAreaFill(xui_widget pWidget, int iSeries, uint32_t* pColor)',
        'Chart', ['extern "xuiChartGetSeriesAreaFill" @no_raise @c_outputs(-1,2) @c_result("integer_min",0) func getSeriesAreaFill(c_int iSeries) as (c_int,c_uint)']],
    'xuiChartGetSeriesValueRadius': [
        'int xuiChartGetSeriesValueRadius(xui_widget pWidget, int iSeries, float* pMinSize, float* pMaxSize)',
        'Chart', ['extern "xuiChartGetSeriesValueRadius" @no_raise @c_outputs(-1,2,3) @c_result("integer_min",0) func getSeriesValueRadius(c_int iSeries) as (c_int,c_float,c_float)']],
    'xuiChartGetSeriesValueColor': [
        'int xuiChartGetSeriesValueColor(xui_widget pWidget, int iSeries, uint32_t* pMinColor, uint32_t* pMaxColor)',
        'Chart', ['extern "xuiChartGetSeriesValueColor" @no_raise @c_outputs(-1,2,3) @c_result("integer_min",0) func getSeriesValueColor(c_int iSeries) as (c_int,c_uint,c_uint)']],
})

CALLBACKS = {'xui_accessibility_event_proc': ('typedef void (*xui_accessibility_event_proc)(xui_context_t* pContext, xui_widget_t* pWidget, const '
                                  'xui_accessibility_event_t* pEvent, void* pUser);',
                                  'export typedef AccessibilityEventProc = '
                                  'func(CallbackContext,CallbackWidget,CallbackConstAccessibilityEvent)'),
 'xui_error_proc': ('typedef void (*xui_error_proc)(xui_context pContext, const xui_error_info_t* pError, void* pUser);',
                    'export typedef ErrorProc = func(CallbackContext,CallbackConstErrorInfo)'),
 'xui_radio_group_change_proc': ('typedef void (*xui_radio_group_change_proc)(xui_widget_t* pGroup, xui_widget_t* pSelected, int '
                                 'iSelectedIndex, void* pUser);',
                                 'export typedef RadioGroupChangeProc = func(Widget,Widget?,c_int)')}

CALLBACKS.update({
    'xui_numeric_input_text_owned_proc': (
        'typedef xstrview (*xui_numeric_input_text_owned_proc)(float fValue, void* pUser);',
        'export typedef NumericInputFormatter = func(c_float) as string',
    ),
    'xui_toolbar_overflow_proc': (
        'typedef void (*xui_toolbar_overflow_proc)(xui_widget_t* pWidget, int iFirst, int iCount, void* pUser);',
        'export typedef ToolbarOverflowProc = func(CallbackWidget,c_int,c_int)',
    ),
    'xui_chart_tooltip_owned_proc': (
        'typedef xstrview (*xui_chart_tooltip_owned_proc)(xui_widget_t* pWidget, int iSeries, int iItem, void* pUser);',
        'export typedef ChartTooltipProc = func(CallbackWidget,c_int,c_int) as string',
    ),
    'xui_chart_context_proc': (
        'typedef int (*xui_chart_context_proc)(xui_widget_t* pWidget, const xui_chart_hit_t* pHit, float fX, float fY, void* pUser);',
        'export typedef ChartContextProc = func(CallbackWidget,CallbackChartHit,c_float,c_float) as c_int',
    ),
    'xui_message_list_node_renderer_proc': (
        'typedef int (*xui_message_list_node_renderer_proc)(xui_widget_t* pWidget, int iIndex, const xui_message_node_t* pNode, '
        'xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef MessageNodeRenderProc = func(CallbackWidget,c_int,CallbackConstMessageNode,CallbackDrawContext,Rect,c_int) as c_int'),
    'xui_webview_event_proc': (
        'typedef void (*xui_webview_event_proc)(xui_widget pWidget, int iEvent, int32_t iNativeError, void* pUser);',
        'export typedef WebViewEventProc = func(Widget,c_int,c_int)'),
})

VIEWS = {'CallbackConstAccessibilityEvent': 'export extern "const xui_accessibility_event_t*" class CallbackConstAccessibilityEvent '
                                    '@header("xge3_runtime.h") @lifetime("callback")\n'
                                    ' native point handle\n'
                                    ' extern "iType" @c_field get eventType() as c_int\n'
                                    ' extern "iNodeId" @c_field get nodeId() as uint64\n'
                                    ' extern "iRevision" @c_field get revision() as c_uint\n'
                                    'end',
 'CallbackConstErrorInfo': 'export extern "const xui_error_info_t*" class CallbackConstErrorInfo @header("xge3_runtime.h") '
                           '@lifetime("callback")\n'
                           ' native point handle\n'
                           ' extern "iCode" @c_field get code() as c_int\n'
                           ' extern "iStage" @c_field get stage() as c_int\n'
                           ' extern "bRecoverable" @c_field get recoverable() as c_int\n'
                           ' extern "tRect" @c_field get rectangle() as Rect\n'
                           ' extern "pWidget" @c_field @c_result("scope_view",0) get widget() as CallbackWidget?\n'
                           ' extern "sOperation" @c_field @c_result("cstring") get operation() as string?\n'
                           ' extern "sMessage" @c_field @c_result("cstring") get message() as string?\n'
                           'end',
 'CallbackConstEvent': 'export extern "const xui_event_t*" class CallbackConstEvent @header("xge3_runtime.h") @lifetime("callback")\n'
                       ' native point handle\n'
                       ' extern "xuiEventDragData" @no_raise @c_result("scope_view",0) func dragData() as CallbackDragData?\n'
                       ' extern "pTarget" @c_field @c_result("scope_view",0) get target() as CallbackWidget?\n'
                       ' extern "pCurrentTarget" @c_field @c_result("scope_view",0) get currentTarget() as CallbackWidget?\n'
                       ' extern "pRelated" @c_field @c_result("scope_view",0) get related() as CallbackWidget?\n'
                       ' extern "sCommand" @c_field @c_result("cstring") get commandName() as string?\n'
                       ' extern "iSize" @c_field get iSize() as c_uint\n'
                       ' extern "iType" @c_field get iType() as c_int\n'
                       ' extern "iPhase" @c_field get iPhase() as c_int\n'
                       ' extern "iFlags" @c_field get iFlags() as c_uint\n'
                       ' extern "fX" @c_field get fX() as c_int\n'
                       ' extern "fY" @c_field get fY() as c_int\n'
                       ' extern "fWheelX" @c_field get fWheelX() as c_float\n'
                       ' extern "fWheelY" @c_field get fWheelY() as c_float\n'
                       ' extern "iButton" @c_field get iButton() as c_int\n'
                       ' extern "iClickCount" @c_field get iClickCount() as c_int\n'
                       ' extern "iButtons" @c_field get iButtons() as c_uint\n'
                       ' extern "iKey" @c_field get iKey() as c_int\n'
                       ' extern "iCodepoint" @c_field get iCodepoint() as c_uint\n'
                       ' extern "iModifiers" @c_field get iModifiers() as c_uint\n'
                       ' extern "iCommand" @c_field get iCommand() as c_int\n'
                       ' extern "iTextSize" @c_field get iTextSize() as c_int\n'
                       ' extern "sText" @c_field @c_length("iTextSize","int") @c_result("text_view") get text() as string\n'
                       ' extern "iCompositionStart" @c_field get iCompositionStart() as c_int\n'
                       ' extern "iCompositionLength" @c_field get iCompositionLength() as c_int\n'
                       ' extern "iPointerId" @c_field get iPointerId() as uint64\n'
                       ' extern "iPointerType" @c_field get iPointerType() as c_int\n'
                       ' extern "fPressure" @c_field get fPressure() as c_float\n'
                       ' extern "fContactW" @c_field get fContactW() as c_float\n'
                       ' extern "fContactH" @c_field get fContactH() as c_float\n'
                       ' extern "bCompositionActive" @c_field get bCompositionActive() as c_int\n'
                       ' extern "iCompositionCursor" @c_field get iCompositionCursor() as c_int\n'
                       ' extern "iCompositionSelectionStart" @c_field get iCompositionSelectionStart() as c_int\n'
                       ' extern "iCompositionSelectionEnd" @c_field get iCompositionSelectionEnd() as c_int\n'
                       ' extern "bCompositionReplacementRange" @c_field get bCompositionReplacementRange() as c_int\n'
                       ' extern "iCompositionReplacementStart" @c_field get iCompositionReplacementStart() as c_int\n'
                       ' extern "iCompositionReplacementEnd" @c_field get iCompositionReplacementEnd() as c_int\n'
                       ' extern "iCompositionAttribute" @c_field get iCompositionAttribute() as c_int\n'
                       'end',
 'CallbackContext': 'export extern "xui_context_t*" class CallbackContext @header("xge3_runtime.h") @lifetime("callback")\n'
                    ' native point handle\n'
                    ' extern "xuiGetLanguage" @no_raise func getLanguage() as c_int\n'
                    ' extern "xuiRequestClose" @no_raise @c_result("status",0) func requestClose()\n'
                    'end',
 'CallbackConstMessageListEvent': 'export extern "const xui_message_list_event_t*" class CallbackConstMessageListEvent @header("xge3_runtime.h") @lifetime("callback")\n'
                                  ' native point handle\n'
                                  ' extern "iSize" @c_field get iSize() as c_uint\n'
                                  ' extern "iEvent" @c_field get iEvent() as c_int\n'
                                  ' extern "iIndex" @c_field get iIndex() as c_int\n'
                                  ' extern "iNodeType" @c_field get iNodeType() as c_int\n'
                                  ' extern "iButton" @c_field get iButton() as c_int\n'
                                  ' extern "iModifiers" @c_field get iModifiers() as c_uint\n'
                                  ' extern "fX" @c_field get fX() as c_float\n'
                                  ' extern "fY" @c_field get fY() as c_float\n'
                                  ' extern "pNode" @c_field @c_result("scope_view",0) get node() as CallbackConstMessageNode?\n'
                                  'end',
 'CallbackConstMessageNode': 'export extern "const xui_message_node_t*" class CallbackConstMessageNode '
                            '@header("xge3_runtime.h") @lifetime("callback")\n'
                            ' native point handle\n'
                            ' extern "sId" @c_field @c_result("cstring") get id() as string\n'
                            ' extern "sText" @c_field @c_result("cstring") get text() as string\n'
                            ' extern "sSender" @c_field @c_result("cstring") get sender() as string\n'
                            ' extern "sTime" @c_field @c_result("cstring") get time() as string\n'
                            ' extern "sParentId" @c_field @c_result("cstring") get parentId() as string\n'
                            ' extern "sTitle" @c_field @c_result("cstring") get title() as string\n'
                            ' extern "iType" @c_field get kind() as c_int\n'
                            ' extern "iFlags" @c_field get flags() as c_int\n'
                            ' extern "iAuxiliaryKind" @c_field get auxiliaryKind() as c_int\n'
                            'end'}

RECORDS = {'xui_accessibility_event_t': 'typedef struct xui_accessibility_event_t { uint32_t iSize; int iType; uint64_t iNodeId; uint32_t iRevision; '
                              '} xui_accessibility_event_t;',
 'xui_callback_owner_t': 'typedef struct xui_callback_owner_t { xrtownershipref tReference; bool (*onHold)(const void*); bool '
                         '(*onDrop)(const void*); } xui_callback_owner_t;',
 'xui_context_desc_t': 'typedef struct xui_context_desc_t { uint32_t iSize; int iSystemFontPolicy; float fSystemFontSize; uint32_t iFlags; '
                       '} xui_context_desc_t;',
 'xui_code_find_result_t': 'typedef struct xui_code_find_result_t xui_code_find_result_t;',
 'xui_dock_drop_info_t': 'typedef struct xui_dock_drop_info_t xui_dock_drop_info_t;',
 'xui_error_info_t': 'typedef struct xui_error_info_t { uint32_t iSize; int iCode; int iStage; int bRecoverable; xui_widget pWidget; '
                     'xui_rect_t tRect; const char* sOperation; const char* sMessage; } xui_error_info_t;',
 'xui_event_t': 'typedef struct xui_event_t xui_event_t;',
 'xui_find_options_t': 'typedef struct xui_find_options_t xui_find_options_t;',
 'xui_ime_composition_t': 'typedef struct xui_ime_composition_t { uint32_t iSize; const char* sText; int iTextSize; int bActive; int '
                          'iCursor; int iSelectionStart; int iSelectionEnd; int bReplacementRange; int iReplacementStart; int '
                          'iReplacementEnd; int iAttribute; } xui_ime_composition_t;',
 'xui_inventory_hit_t': 'typedef struct xui_inventory_hit_t { uint32_t iSize; int iPart; int iSlot; xui_rect_t tRect; float fX; float fY; '
                        '} xui_inventory_hit_t;',
 'xui_inventory_visible_range_t': 'typedef struct xui_inventory_visible_range_t { uint32_t iSize; int iFirstSlot; int iLastSlot; int '
                                  'iSlotCount; int iFirstRow; int iLastRow; int iRowCount; int iFirstColumn; int iLastColumn; int '
                                  'iColumnCount; int iTotalRows; int iTotalColumns; int iPaintSlotCount; } xui_inventory_visible_range_t;',
 'xui_menu_item_t': 'typedef struct xui_menu_item_t { const char* sText; const char* sShortcut; int iType; uint32_t iState; int iValue; '
                    'int iIcon; xui_widget_t* pSubmenu; void* pUser; } xui_menu_item_t;',
 'xui_radio_desc_t': 'typedef struct xui_radio_desc_t { uint32_t iSize; const char* sText; struct xui_font_t* pFont; uint32_t iTextColor; '
                     'uint32_t iDisabledTextColor; uint32_t iTextFlags; int bChecked; float fIndicatorSize; float fGap; int '
                     'bUseBuiltinAtlas; } xui_radio_desc_t;',
 'xui_rect_t': 'typedef struct xui_rect_t { int fX; int fY; int fW; int fH; } xui_rect_t;',
 'xui_terminal_cell_t': 'typedef struct xui_terminal_cell_t { uint32_t iSize; uint32_t iCodepoint; uint32_t iFgColor; uint32_t iBgColor; '
                        'uint32_t iFlags; uint16_t iStyle; uint8_t iWidth; uint16_t iLinkId; uint8_t iCombiningCount; uint8_t '
                        'arrReserved[3]; uint32_t arrCombining[XUI_TERMINAL_MAX_COMBINING]; } xui_terminal_cell_t;',
 'xui_timeline_hit_t': 'typedef struct xui_timeline_hit_t xui_timeline_hit_t;',
 'xui_vec2_t': 'typedef struct xui_vec2_t { float fX; float fY; } xui_vec2_t;'}

# Resolve forward typedefs too: their declaration alone omits the field layout.
TYPE_SHAPES = {'xui_accessibility_event_proc': 'e024773384cabe3acdff878404d4c09f4d472d4991a5ccadaa44e58a4cec4dbf',
 'xui_accessibility_event_t': 'f39ed9eca1aa8f1b8caa63368d57a586a8aa8f8d563d6f8c2e65247d43fdc873',
 'xui_callback_owner_t': '35c650eef37df012c6e923aadf66f21faa1ca1be3ed0d835988075edbe397a71',
 'xui_context_desc_t': '915af53e0a24c2a045f6281d2b500d0528d910a3b8f24bb8eaae7dd57666c8fa',
 'xui_code_find_result_t': '8140f41a4aacab78803fbe3a9db5279a074a49f0a2961913d364c85f82e1da84',
 'xui_dock_drop_info_t': '548739842232aca5557c245360c3280859db8a50cdc764323db41eb1580423ab',
 'xui_error_info_t': '3a3a8cbc9278b6bf896fbe5e9d470e93e7a3aaba37a25b3b32b08f2f46744812',
 'xui_error_proc': '69c9d6276e273cb8485a1fa32ec1f2a146ab575fc653287422fd4871ddf6ba4a',
 'xui_event_t': 'c7e3f00fcfcd475744515c1890fc85aac882d3021a45a42272435363edc50334',
 'xui_find_options_t': '121848d8f0fc70f200e43a7a64c9cfe960b6bbda1de2b1b919de76b7c333e3b8',
 'xui_ime_composition_t': 'f7ea0c9b43459c62eec0cf523b011cc62776d079a8538f034919d3b1dd0e347c',
 'xui_inventory_hit_t': 'c04bc1f9b5828f88b9c1e6795ce89cbd47641166cd9e1871298ea29d9701b6cc',
 'xui_inventory_visible_range_t': '212d30f5ce0770531a3337720c80d56be8966b1f6f4d495fb12001e1a12b42ca',
 'xui_menu_item_t': 'e2e8345766c9df98325902150ba1d477b2c352b2e162f9eaea41840d566f2650',
 'xui_radio_desc_t': 'c97ecf3e5afdc6690e616935c9b88393e0c90c35706741e98b211178562a3642',
 'xui_radio_group_change_proc': '59c5469e937558e0d359cd29bedc968b9815d34c7722bcce958393175499fd10',
 'xui_rect_t': '61a6d7ed54e05d63f25958d5219f5363155cf0e7950ecfe660b57924a9900032',
 'xui_terminal_cell_t': '27a463797f92b3289f54fd8c61b4bd80a31bfe5b798debdc446ed61b263a445c',
 'xui_timeline_hit_t': '85a7722fb57db5cef87d1e94fa011f96ec4bf34592116e3791fb1d99751a5df1',
 'xui_vec2_t': 'dba0d1ca7b2f5c1eb244a9eee5d2743563abeeab4f0af14162f52ab3182c4cf0'}

RECORDS.update({
    'xui_flow_graph_t': 'typedef struct xui_flow_graph_t xui_flow_graph_t;',
    'xui_flow_graph': 'typedef xui_flow_graph_t* xui_flow_graph;',
})
TYPE_SHAPES.update({
    'xui_flow_graph_t': '7c2b3b3f108a91ce4f9154f75b371307823f520c9bf35400f869572483f9a6a0',
    'xui_flow_graph': '53f509f41455dcfcdd627b8e143e9b88647ba9ad8e73369b7bff9fdb88013ee4',
})

TYPE_SHAPES.update({
    'xui_webview_event_proc': '4c8d001e4b5001ef70e4347f5955528d82a5fb5237178cdde269a1237d680db1',
})

METHODS.update({
    'xuiNumericInputSetFormatterOwned': [
        'int xuiNumericInputSetFormatterOwned(xui_widget pWidget, xui_numeric_input_text_owned_proc onFormat, '
        'void* pUser, const xui_callback_owner_t* pOwner)', 'NumericInput',
        ['extern "xuiNumericInputSetFormatterOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0,"owned_text") '
         '@c_result("status",0) func setFormatter(NumericInputFormatter? formatter)']],
    'xuiTableViewGetColumnDescriptor': ['int xuiTableViewGetColumnDescriptor(xui_widget pWidget, int iColumn, xui_table_column_desc_t* pColumn)',
        'TableView', ['extern "xuiTableViewGetColumnDescriptor" @no_raise @c_output_record(2,"xui_table_column_desc_t","borrowed") @c_status(0) @c_output_field(0,"tSettings") @c_output_field(1,"sId","cstring") @c_output_field(2,"sTitle","cstring") @c_output_field(3,"tRect") func getColumn(c_int index) as (TableColumnSettings,string?,string?,Rect)']],
    'xuiTableViewSetColumnDescriptors': ['int xuiTableViewSetColumnDescriptors(xui_widget pWidget, const xui_table_column_desc_t* pColumns, int iCount)',
        'TableView', ['extern "xuiTableViewSetColumnDescriptors" @no_raise @c_array_group(1,0,3,"int",3) @c_array_record(1,"xui_table_column_desc_t") @c_array_field(1,"tSettings",0,"value") @c_array_field(1,"sId",1,"c_string") @c_array_field(1,"sTitle",2,"c_string") @c_result("status",0) func setColumnsDetailed(array<TableColumnSettings> settings,array<string?> ids,array<string?> titles,int size=-1)']],
    'xuiTableViewSetHeaderRendererOwned': ['int xuiTableViewSetHeaderRendererOwned(xui_widget pWidget, xui_table_view_header_renderer_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'TableView', ['extern "xuiTableViewSetHeaderRendererOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setHeaderRenderer(TableViewHeaderRenderer? renderer)']],
    'xuiTableViewSetCellRendererOwned': ['int xuiTableViewSetCellRendererOwned(xui_widget pWidget, xui_table_view_cell_renderer_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'TableView', ['extern "xuiTableViewSetCellRendererOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setCellRenderer(TableViewCellRenderer? renderer)']],
    'xuiTableViewSetColumnFormatterOwned': ['int xuiTableViewSetColumnFormatterOwned(xui_widget pWidget, int iColumn, xui_table_view_format_owned_proc onFormat, void* pUser, const xui_callback_owner_t* pOwner)',
        'TableView', ['extern "xuiTableViewSetColumnFormatterOwned" @no_raise @c_owned_callback(1,0,"xui_callback_owner_t",-16) @c_result("status",0) func setColumnFormatter(c_int column,TableViewColumnFormatter? formatter)']],
    'xuiTableViewSetColumnRendererOwned': ['int xuiTableViewSetColumnRendererOwned(xui_widget pWidget, int iColumn, xui_table_view_cell_renderer_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'TableView', ['extern "xuiTableViewSetColumnRendererOwned" @no_raise @c_owned_callback(1,0,"xui_callback_owner_t",-16) @c_result("status",0) func setColumnRenderer(c_int column,TableViewCellRenderer? renderer)']],
    'xuiTableViewSetRows': ['int xuiTableViewSetRows(xui_widget pWidget, const xui_table_view_row_t* arrRows, int iCount)', 'TableView',
        ['extern "xuiTableViewSetRows" @no_raise @c_span(0,"const xui_table_view_row_t*","int") func setRows(Buffer arrRows) as c_int',
         'extern "xuiTableViewSetRows" @no_raise @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const xui_table_view_row_t*","value") func setRows(array<TableViewRow> rows,int size=-1) as c_int']],
    'xuiPropertyGridSetEditorConfig': ['int xuiPropertyGridSetEditorConfig(xui_widget pWidget, int iProperty, const xui_table_grid_editor_config_t* pConfig)',
        'PropertyGrid', ['extern "xuiPropertyGridSetEditorConfig" @no_raise @c_result("status",0) '
        '@c_pack(2,"xui_table_grid_editor_config_t",1,28) @c_pack_field(2,"fMin",1,"value") '
        '@c_pack_field(2,"fMax",2,"value") @c_pack_field(2,"fStep",3,"value") '
        '@c_pack_field(2,"iPrecision",4,"value") @c_pack_field(2,"bNullable",5,"value") '
        '@c_pack_field(2,"iEnumSelected",6,"value") @c_pack_field(2,"bEnumUseValue",7,"value") '
        '@c_pack_field(2,"iEnumSelectedValue",8,"value") @c_pack_field(2,"arrPalette",15,"span_data") '
        '@c_pack_field(2,"iPaletteCount",15,"span_count") @c_pack_field(2,"bAlphaEnabled",16,"value") '
        '@c_pack_field(2,"iDateMode",17,"value") @c_pack_field(2,"bDateModeSet",18,"value") '
        '@c_pack_field(2,"bShowSecond",19,"value") @c_pack_field(2,"sDateFormat",20,"c_string") '
        '@c_pack_field(2,"tDateMin",21,"value") @c_pack_field(2,"tDateMax",22,"value") '
        '@c_pack_field(2,"bDateHasMin",23,"value") @c_pack_field(2,"bDateHasMax",24,"value") '
        '@c_pack_field(2,"tDefaultRangeSpan",25,"value") @c_pack_field(2,"sActionText",26,"c_string") '
        '@c_pack_field(2,"sFileFilter",27,"c_string") @c_pack_field(2,"sRangeSeparator",28,"c_string") '
        '@c_span(15,"const uint32_t*","int") @c_array_group(10,9,5,"int",14) '
        '@c_array_record(10,"xui_combobox_item_t") @c_array_field(10,"sText",0,"c_string") '
        '@c_array_field(10,"iValue",1,"value") @c_array_field(10,"bEnabled",2,"value") '
        '@c_array_field(10,"bSeparator",3,"value") @c_array_field(10,"iIcon",4,"value") '
        '@c_array_pack(10,2,"arrEnumItemData","iEnumItemCount") func setEditorConfig(c_int property,c_float '
        'minimum=0,c_float maximum=0,c_float step=0,c_int precision=-1,c_int nullable=0,c_int '
        'selectedOption=-1,c_int useOptionValue=0,c_int selectedValue=0,array<string?> '
        'optionLabels=[],array<c_int> optionValues=[],array<c_int> optionEnabled=[],array<c_int> '
        'optionSeparators=[],array<c_int> optionIcons=[],int optionCount=-1,bytes palette=b"",c_int '
        'alphaEnabled=0,c_int dateMode=0,c_int useDateMode=0,c_int showSeconds=1,string? dateFormat=null,int64 '
        'dateMinimumMilliseconds=0,int64 dateMaximumMilliseconds=0,c_int useDateMinimum=0,c_int '
        'useDateMaximum=0,int64 defaultRangeMilliseconds=0,string? actionText=null,string? '
        'fileFilter=null,string? rangeSeparator=null)',
        'extern "xuiPropertyGridSetEditorConfig" @no_raise @c_result("status",0) @c_null(2,"const xui_table_grid_editor_config_t*") func clearEditorConfig(c_int property)']],
    'xuiTableGridSetRows': ['int xuiTableGridSetRows(xui_widget pWidget, const xui_table_view_row_t* arrRows, int iCount)', 'TableGrid',
        ['extern "xuiTableGridSetRows" @no_raise @c_span(0,"const xui_table_view_row_t*","int") func setRows(Buffer arrRows) as c_int',
         'extern "xuiTableGridSetRows" @no_raise @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const xui_table_view_row_t*","value") func setRows(array<TableViewRow> rows,int size=-1) as c_int']],
    'xuiTableGridGetColumnDescriptor': ['int xuiTableGridGetColumnDescriptor(xui_widget pWidget, int iColumn, xui_table_column_desc_t* pColumn)',
        'TableGrid', ['extern "xuiTableGridGetColumnDescriptor" @no_raise @c_output_record(2,"xui_table_column_desc_t","borrowed") @c_status(0) @c_output_field(0,"tSettings") @c_output_field(1,"sId","cstring") @c_output_field(2,"sTitle","cstring") @c_output_field(3,"tRect") func getColumn(c_int index) as (TableColumnSettings,string?,string?,Rect)']],
    'xuiTableGridSetColumnDescriptors': ['int xuiTableGridSetColumnDescriptors(xui_widget pWidget, const xui_table_column_desc_t* pColumns, int iCount)',
        'TableGrid', ['extern "xuiTableGridSetColumnDescriptors" @no_raise @c_array_group(1,0,3,"int",3) @c_array_record(1,"xui_table_column_desc_t") @c_array_field(1,"tSettings",0,"value") @c_array_field(1,"sId",1,"c_string") @c_array_field(1,"sTitle",2,"c_string") @c_result("status",0) func setColumnsDetailed(array<TableColumnSettings> settings,array<string?> ids,array<string?> titles,int size=-1)']],
    'xuiTableViewGetSortColumn': ['int xuiTableViewGetSortColumn(xui_widget pWidget, int* pDescending)',
        'TableView', ['extern "xuiTableViewGetSortColumn" @no_raise @c_outputs(-1,1) @c_result("integer_min",-1) func getSortColumn() as (c_int,c_int)']],
})

METHODS.update({
    'xuiListViewSetActivateOwned': ['int xuiListViewSetActivateOwned(xui_widget pWidget, xui_list_view_activate_proc onActivate, void* pUser, const xui_callback_owner_t* pOwner)',
        'ListView', ['extern "xuiListViewSetActivateOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0) @c_result("status",0) func setActivate(ListViewActivateProc? onActivate)']],
    'xuiListViewSetItemRendererOwned': ['int xuiListViewSetItemRendererOwned(xui_widget pWidget, xui_list_view_item_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'ListView', ['extern "xuiListViewSetItemRendererOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setItemRenderer(ListViewItemRenderProc? onRender)']],
    'xuiListViewSetVirtualItemsOwned': ['int xuiListViewSetVirtualItemsOwned(xui_widget pWidget, int iCount, xui_list_view_text_owned_proc onText, void* pUser, const xui_callback_owner_t* pOwner)',
        'ListView', ['extern "xuiListViewSetVirtualItemsOwned" @no_raise @c_owned_callback(1,0,"xui_callback_owner_t",0,"owned_text") @c_result("status",0) func setVirtualItems(c_int count,ListViewTextProc? onText=null)']],
    'xuiListViewGetItemTextOwned': ['int xuiListViewGetItemTextOwned(xui_widget pWidget, int iIndex, char** psText, size_t* piSize)',
        'ListView', ['extern "xuiListViewGetItemTextOwned" @no_raise @c_output_text(2,3,"char*","size_t","xrt",0) func readItemText(c_int index) as string']],
    'xuiListViewSetEnabledItems': ['int xuiListViewSetEnabledItems(xui_widget pWidget, const int* arrEnabled, int iCount)', 'ListView', [
        'extern "xuiListViewSetEnabledItems" @no_raise @c_span(0,"const int*","int") @c_result("status",0) func setEnabledItems(Buffer arrEnabled)',
        'extern "xuiListViewSetEnabledItems" @no_raise @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const int*","value") @c_result("status",0) func setEnabledItems(array<c_int> enabled,int size=-1)',
    ]],
    'xuiTreeViewSetItemRendererOwned': ['int xuiTreeViewSetItemRendererOwned(xui_widget pWidget, xui_tree_view_item_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'TreeView', ['extern "xuiTreeViewSetItemRendererOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setItemRenderer(TreeViewItemRenderProc? onRender)']],
    'xuiTreeViewSetIconRendererOwned': ['int xuiTreeViewSetIconRendererOwned(xui_widget pWidget, xui_tree_view_icon_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'TreeView', ['extern "xuiTreeViewSetIconRendererOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setIconRenderer(TreeViewIconRenderProc? onRender)']],
    'xuiTreeViewSetNodeDescriptors': ['int xuiTreeViewSetNodeDescriptors(xui_widget pWidget, const xui_tree_node_desc_t* pNodes, int iCount)',
        'TreeView', ['extern "xuiTreeViewSetNodeDescriptors" @no_raise @c_array_group(1,0,2,"int",2) @c_array_record(1,"xui_tree_node_desc_t") @c_array_field(1,"tState",0,"value") @c_array_field(1,"sText",1,"c_string") @c_result("status",0) func setNodes(array<TreeNodeState> states,array<string?> texts,int size=-1)']],
    'xuiTreeViewGetNodeDescriptor': ['int xuiTreeViewGetNodeDescriptor(xui_widget pWidget, int iIndex, xui_tree_node_desc_t* pNode)',
        'TreeView', ['extern "xuiTreeViewGetNodeDescriptor" @no_raise @c_output_record(2,"xui_tree_node_desc_t","borrowed") @c_status(0) @c_output_field(0,"tState") @c_output_field(1,"sText","cstring") func getNode(c_int index) as (TreeNodeState,string)']],
    'xuiTreeViewGetNodeDescriptorById': ['int xuiTreeViewGetNodeDescriptorById(xui_widget pWidget, int iNodeId, xui_tree_node_desc_t* pNode)',
        'TreeView', ['extern "xuiTreeViewGetNodeDescriptorById" @no_raise @c_output_record(2,"xui_tree_node_desc_t","borrowed") @c_status(0) @c_output_field(0,"tState") @c_output_field(1,"sText","cstring") func getNodeById(c_int id) as (TreeNodeState,string)']],
    'xuiTreeViewGetVisibleNodeDescriptor': ['int xuiTreeViewGetVisibleNodeDescriptor(xui_widget pWidget, int iVisible, xui_tree_node_desc_t* pNode)',
        'TreeView', ['extern "xuiTreeViewGetVisibleNodeDescriptor" @no_raise @c_output_record(2,"xui_tree_node_desc_t","borrowed") @c_status(0) @c_output_field(0,"tState") @c_output_field(1,"sText","cstring") func getVisibleNode(c_int index) as (TreeNodeState,string)']],
})

CALLBACKS.update({
    'xui_list_view_activate_proc': (
        'typedef void (*xui_list_view_activate_proc)(xui_widget_t* pWidget, int iIndex, int iButton, void* pUser);',
        'export typedef ListViewActivateProc = func(Widget,c_int,c_int)',
    ),
    'xui_list_view_item_proc': (
        'typedef int (*xui_list_view_item_proc)(xui_widget_t* pWidget, int iIndex, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef ListViewItemRenderProc = func(CallbackWidget,c_int,CallbackDrawContext,Rect,c_int) as c_int',
    ),
    'xui_list_view_text_owned_proc': (
        'typedef xstrview (*xui_list_view_text_owned_proc)(xui_widget_t* pWidget, int iIndex, void* pUser);',
        'export typedef ListViewTextProc = func(CallbackWidget,c_int) as string',
    ),
    'xui_tree_view_item_proc': (
        'typedef int (*xui_tree_view_item_proc)(xui_widget_t* pWidget, int iNodeId, int iVisible, const xui_tree_view_node_t* pNode, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef TreeViewItemRenderProc = func(CallbackWidget,c_int,c_int,CallbackTreeNode,CallbackDrawContext,Rect,c_int) as c_int',
    ),
    'xui_tree_view_icon_proc': (
        'typedef int (*xui_tree_view_icon_proc)(xui_widget pWidget, int iNodeId, xui_draw_context pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef TreeViewIconRenderProc = func(CallbackWidget,c_int,CallbackDrawContext,Rect,c_int) as c_int',
    ),
})

METHODS.update({
    'xuiCodeEditGetCompletionPosition': ['int xuiCodeEditGetCompletionPosition(xui_widget pWidget, uint32_t* pDocumentVersion, int* pOffset)', 'CodeEdit',
        ['extern "xuiCodeEditGetCompletionPosition" @no_raise @c_outputs(1,2) @c_status(0) func getCompletionPosition() as (c_uint,c_int)']],
    'xuiCodeEditApplyCompletionItems': ['int xuiCodeEditApplyCompletionItems(xui_widget pWidget, uint32_t iDocumentVersion, int iOffset, const xui_code_completion_item_t* pItems, int iItemCount)', 'CodeEdit',
        ['extern "xuiCodeEditApplyCompletionItems" @no_raise @c_array_group(3,2,11,"int",13) @c_array_record(3,"xui_code_completion_item_t","iSize","uint32_t") @c_array_field(3,"sLabel",0,"c_string") @c_array_field(3,"sInsertText",1,"c_string") @c_array_field(3,"sDetail",2,"c_string") @c_array_field(3,"sDocumentation",3,"c_string") @c_array_field(3,"iKind",4,"value") @c_array_field(3,"iSortOrder",5,"value") @c_array_field(3,"iUserData",6,"value") @c_array_field(3,"sFilterText",7,"c_string") @c_array_field(3,"sSortText",8,"c_string") @c_array_field(3,"sCommitCharacters",9,"c_string") @c_array_field(3,"iFlags",10,"value") @c_result("status",0) func applyCompletionItems(c_uint version,c_int offset,array<string> labels,array<string?> insertTexts,array<string?> details,array<string?> documentation,array<c_int> kinds,array<c_int> sortOrders,array<c_uintptr> userData,array<string?> filterTexts,array<string?> sortTexts,array<string?> commitCharacters,array<c_uint> flags,int size=-1)']],
    'xuiCodeEditShowSignatureHelp': ['int xuiCodeEditShowSignatureHelp(xui_widget pWidget, const xui_code_signature_help_t* pHelp)', 'CodeEdit',
        ['extern "xuiCodeEditShowSignatureHelp" @no_raise @c_pack(1,"xui_code_signature_help_t",0,8) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sLabel",0,"c_string") @c_pack_field(1,"sDocumentation",1,"c_string") @c_pack_field(1,"iActiveParameter",2,"value") @c_pack_field(1,"iUserData",7,"value") @c_array_group(10,3,3,"int",6) @c_array_record(10,"xui_code_signature_parameter_t","iSize","uint32_t") @c_array_field(10,"sLabel",0,"c_string") @c_array_field(10,"sDocumentation",1,"c_string") @c_array_field(10,"tLabelRange",2,"value") @c_array_pack(10,1,"pParameters","iParameterCount") @c_result("status",0) func showSignatureHelp(string label,string? documentation=null,c_int activeParameter=-1,array<string?> parameterLabels=[],array<string?> parameterDocumentation=[],array<CodeRange> parameterRanges=[],int size=-1,c_uintptr userData=0)']],
    'xuiCodeEditShowHint': ['int xuiCodeEditShowHint(xui_widget pWidget, const xui_code_hover_t* pHint)', 'CodeEdit',
        ['extern "xuiCodeEditShowHint" @no_raise @c_pack(1,"xui_code_hover_t",0,4) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"tRange",0,"value") @c_pack_field(1,"sText",1,"c_string") @c_pack_field(1,"sContentType",2,"c_string") @c_pack_field(1,"iUserData",3,"value") @c_result("status",0) func showHint(CodeRange range,string text,string? contentType=null,c_uintptr userData=0)']],
    'xuiCodeEditSetPlaceholders': ['int xuiCodeEditSetPlaceholders(xui_widget pWidget, const xui_code_placeholder_t* pPlaceholders, int iCount, int iActiveIndex)', 'CodeEdit',
        ['extern "xuiCodeEditSetPlaceholders" @no_raise @c_span(0,"const xui_code_placeholder_t*","int") func setPlaceholders(Buffer pPlaceholders,c_int iActiveIndex) as c_int',
        'extern "xuiCodeEditSetPlaceholders" @no_raise @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const xui_code_placeholder_t*","value") func setPlaceholders(array<CodePlaceholder> placeholders,int size=-1,c_int activeIndex=0) as c_int']],
    'xuiCodeEditApplyTextEdits': ['int xuiCodeEditApplyTextEdits(xui_widget pWidget, const xui_code_text_edit_t* pEdits, int iEditCount, int iSelectionAnchor, int iSelectionCaret)', 'CodeEdit',
        ['extern "xuiCodeEditApplyTextEdits" @no_raise @c_array_group(1,0,2,"int",2) @c_array_record(1,"xui_code_text_edit_t","iSize","uint32_t") @c_array_field(1,"tRange",0,"value") @c_array_field(1,"sText",1,"c_string") func applyTextEdits(array<CodeRange> ranges,array<string> replacements,int size=-1,c_int selectionAnchor=-1,c_int selectionCaret=-1) as c_int']],
})


# CodeEdit diagnostic publication and independent query snapshots.
METHODS.update({
    'xuiCodeEditGetDocumentVersion': ['int xuiCodeEditGetDocumentVersion(xui_widget widget, uint32_t* version)', 'CodeEdit',
        ['extern "xuiCodeEditGetDocumentVersion" @no_raise @c_output(1,0) func documentVersion() as c_uint']],
    'xuiCodeEditApplyDiagnostics': ['int xuiCodeEditApplyDiagnostics(xui_widget widget, uint32_t documentVersion, const xui_code_diagnostic_t* diagnostics, int count)', 'CodeEdit',
        ['extern "xuiCodeEditApplyDiagnostics" @no_raise @c_array_group(2,1,6,"int",7) @c_array_record(2,"xui_code_diagnostic_t") @c_array_field(2,"tRange",0,"value") @c_array_field(2,"iSeverity",1,"value") @c_array_field(2,"sCode",2,"c_string") @c_array_field(2,"sMessage",3,"c_string") @c_array_field(2,"sSource",4,"c_string") @c_array_field(2,"iUserData",5,"value") @c_result("status",0) func applyDiagnostics(c_uint version,array<CodeRange> ranges,array<c_int> severities,array<string?> codes,array<string?> messages,array<string?> sources,array<c_uintptr> userData,int size=-1)']],
    'xuiCodeEditGetDiagnosticCount': ['int xuiCodeEditGetDiagnosticCount(xui_widget widget)', 'CodeEdit',
        ['extern "xuiCodeEditGetDiagnosticCount" @no_raise @c_result("integer_min",0) func diagnosticCount() as c_int']],
    'xuiCodeEditGetDiagnostic': ['int xuiCodeEditGetDiagnostic(xui_widget widget, int index, xui_code_diagnostic_t* diagnostic)', 'CodeEdit',
        ['extern "xuiCodeEditGetDiagnostic" @no_raise @c_output_record(2,"xui_code_diagnostic_t","borrowed") @c_status(0) @c_output_field(0,"tRange") @c_output_field(1,"iSeverity") @c_output_field(2,"sCode","cstring") @c_output_field(3,"sMessage","cstring") @c_output_field(4,"sSource","cstring") @c_output_field(5,"iUserData") func getDiagnostic(c_int index) as (CodeRange,c_int,string?,string?,string?,c_uintptr)']],
    'xuiCodeEditSetFindScope': ['int xuiCodeEditSetFindScope(xui_widget pWidget, xui_code_find_scope pScope)', 'CodeEdit',
        ['extern "xuiCodeEditSetFindScope" @no_raise @c_result("status",0) func setFindScope(CodeFindScope? scope)']],
    'xuiContextGetCodeFindScope': ['xui_code_find_scope xuiContextGetCodeFindScope(xui_context pContext, xui_widget pWidget)', 'Context',
        ['extern "xuiContextGetCodeFindScope" @no_raise @c_result("existing_root") func getCodeFindScope(CodeEdit editor) as CodeFindScope?']],
    'xuiCodeEditFindDiagnostic': ['int xuiCodeEditFindDiagnostic(xui_widget widget, float x, float y, xui_code_diagnostic_hit_t* hit)', 'CodeEdit',
        ['extern "xuiCodeEditFindDiagnostic" @no_raise @c_output_record(3,"xui_code_diagnostic_hit_t","borrowed") @c_status(0) @c_output_field(0,"iIndex") @c_output_field(1,"tRect") @c_output_field(2,"tDiagnostic.tRange") @c_output_field(3,"tDiagnostic.iSeverity") @c_output_field(4,"tDiagnostic.sCode","cstring") @c_output_field(5,"tDiagnostic.sMessage","cstring") @c_output_field(6,"tDiagnostic.sSource","cstring") @c_output_field(7,"tDiagnostic.iUserData") func hitTestDiagnostic(c_float contextX,c_float contextY) as (c_int,Rect,CodeRange,c_int,string?,string?,string?,c_uintptr)']],
})
RECORDS.update({
    'xui_code_range_t': 'typedef struct xui_code_range_t xui_code_range_t;',
    'xui_code_diagnostic_t': 'typedef struct xui_code_diagnostic_t xui_code_diagnostic_t;',
    'xui_code_diagnostic_hit_t': 'typedef struct xui_code_diagnostic_hit_t xui_code_diagnostic_hit_t;',
})
TYPE_SHAPES.update({
    'xui_code_range_t': '756be221361dae72d21123ef3cdb63823041d3a543c64a83400cc00f00d18ef5',
    'xui_code_diagnostic_t': '2bb5b2e09ee0a377a9b02f38644d2be72d4241f990930085d154696a8f6bfc0b',
    'xui_code_diagnostic_hit_t': '96bfdb7fed7bc83de249a36f510e812d52cc571ded0ca3c1517f2cd72a387b71',
})

METHODS['xuiCodeEditSetDiagnosticHoverOwned'] = [
    'int xuiCodeEditSetDiagnosticHoverOwned(xui_widget pWidget, xui_code_diagnostic_hover_proc onHover, void* pUser, const xui_callback_owner_t* pOwner)', 'CodeEdit',
    ['extern "xuiCodeEditSetDiagnosticHoverOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0) @c_result("status",0) func setDiagnosticHover(CodeDiagnosticHoverProc? callback)'],
]
CALLBACKS['xui_code_diagnostic_hover_proc'] = (
    'typedef void (*xui_code_diagnostic_hover_proc)(xui_widget_t* pWidget, const xui_code_diagnostic_hit_t* pHit, void* pUser);',
    'export typedef CodeDiagnosticHoverProc = func(CallbackWidget,CallbackConstCodeDiagnosticHit?)',
)
VIEWS.update({
    'CallbackChartHit': """export extern "const xui_chart_hit_t*" class CallbackChartHit @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iSize" @c_field get size() as c_uint
 extern "iPart" @c_field get part() as c_int
 extern "iSeries" @c_field get series() as c_int
 extern "iItem" @c_field get item() as c_int
 extern "tRect" @c_field get rect() as Rect
 extern "fX" @c_field get x() as c_float
 extern "fY" @c_field get y() as c_float
end""",
    'CallbackConstCodeDiagnosticHit': """
export extern "const xui_code_diagnostic_hit_t*" class CallbackConstCodeDiagnosticHit @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iIndex" @c_field get index() as c_int
 extern "tRect" @c_field get rect() as Rect
 extern "xuiCodeDiagnosticHitDetails" @no_raise @c_result("scope_view",0) func details() as CallbackCodeDiagnostic
end
""".strip(),
    'CallbackCodeDiagnostic': """
export extern "const xui_code_diagnostic_t*" class CallbackCodeDiagnostic @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "tRange" @c_field get range() as CodeRange
 extern "iSeverity" @c_field get severity() as c_int
 extern "sCode" @c_field @c_result("cstring") get code() as string?
 extern "sMessage" @c_field @c_result("cstring") get message() as string?
 extern "sSource" @c_field @c_result("cstring") get source() as string?
 extern "iUserData" @c_field get userData() as c_uintptr
end
""".strip(),
})

METHODS.update({
    'xuiCodeEditSetTokenStyleResolverOwned': [
        'int xuiCodeEditSetTokenStyleResolverOwned(xui_widget pWidget, xui_code_token_style_proc onStyle, void* pUser, const xui_callback_owner_t* pOwner)',
        'CodeEdit', ['extern "xuiCodeEditSetTokenStyleResolverOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setTokenStyleResolver(CodeTokenStyleResolver? resolver)'],
    ],
    'xuiCodeEditHasTokenStyleResolver': [
        'bool xuiCodeEditHasTokenStyleResolver(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditHasTokenStyleResolver" @no_raise func hasTokenStyleResolver() as bool'],
    ],
})
CALLBACKS['xui_code_token_style_proc'] = (
    'typedef int (*xui_code_token_style_proc)(xui_code_document_t* pDocument, const xui_code_token_t* pToken, int* pStyleId, void* pUser);',
    'export typedef CodeTokenStyleResolver = func(CallbackCodeDocument,CallbackConstCodeToken,CallbackInt) as c_int',
)
VIEWS['CallbackConstCodeToken'] = """
export extern "const xui_code_token_t*" class CallbackConstCodeToken @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iStartOffset" @c_field get startOffset() as c_int
 extern "iEndOffset" @c_field get endOffset() as c_int
 extern "iStyle" @c_field get style() as c_int
 extern "iKind" @c_field get kind() as c_int
 extern "iFlags" @c_field get flags() as c_int
end
""".strip()
TYPE_SHAPES.update({
    'xui_code_token_t': 'bb082d4f8e837b8716830ea41689d774bdc090cc6d76512774a52f4cfa3bedbe',
    'xui_code_token_style_proc': 'ee01ae5abf574ec1b8beeec018dbdafeeed6c6cecff259d62df254ee1f243511',
})

METHODS.update({
    'xuiCodeEditGetDocument': ['xui_code_document xuiCodeEditGetDocument(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetDocument" @no_raise @c_result("owner_view",0) func document() as CodeEditDocument']],
    'xuiCodeEditGetSelection': ['xui_code_selection_model xuiCodeEditGetSelection(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetSelection" @no_raise @c_result("owner_view",0) func selection() as CodeEditSelection']],
    'xuiCodeEditSetDocumentChangeCallbackOwned': ['int xuiCodeEditSetDocumentChangeCallbackOwned(xui_widget pWidget, xui_code_document_change_proc onChange, void* pUser, const xui_callback_owner_t* pOwner)', 'CodeEdit', ['extern "xuiCodeEditSetDocumentChangeCallbackOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0) @c_result("status",0) func setDocumentChangeCallback(CodeDocumentChangeProc? callback)']],
})
METHODS.update({
    'xuiCodeEditGetMargins': ['xui_code_margin_model xuiCodeEditGetMargins(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetMargins" @no_raise @c_result("owner_view",0) func margins() as CodeEditMargins']],
    'xuiCodeEditGetProviders': ['xui_code_provider_set xuiCodeEditGetProviders(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetProviders" @no_raise @c_result("owner_view",0) func providers() as CodeEditProviders']],
    'xuiCodeEditSetMarginRenderCallbackOwned': ['int xuiCodeEditSetMarginRenderCallbackOwned(xui_widget pWidget, int iId, xui_code_margin_render_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)', 'CodeEdit', ['extern "xuiCodeEditSetMarginRenderCallbackOwned" @no_raise @c_owned_callback(1,0,"xui_callback_owner_t",-16) @c_result("status",0) func setMarginRenderCallback(c_int id,CodeMarginRenderProc? callback)']],
    'xuiCodeEditSetMarginEventCallbackOwned': ['int xuiCodeEditSetMarginEventCallbackOwned(xui_widget pWidget, int iId, xui_code_margin_event_proc onEvent, void* pUser, const xui_callback_owner_t* pOwner)', 'CodeEdit', ['extern "xuiCodeEditSetMarginEventCallbackOwned" @no_raise @c_owned_callback(1,0,"xui_callback_owner_t",-16) @c_result("status",0) func setMarginEventCallback(c_int id,CodeMarginEventProc? callback)']],
})
CALLBACKS['xui_code_margin_render_proc'] = (
    'typedef int (*xui_code_margin_render_proc)(xui_widget_t* pWidget, int iMarginId, int iLine, xui_draw_context_t* pDraw, xui_rect_t tRect, void* pUser);',
    'export typedef CodeMarginRenderProc = func(CallbackWidget,c_int,c_int,CallbackDrawContext,Rect) as c_int',
)
CALLBACKS['xui_code_margin_event_proc'] = (
    'typedef int (*xui_code_margin_event_proc)(xui_widget_t* pWidget, int iMarginId, int iLine, int iEvent, xui_rect_t tRect, void* pUser);',
    'export typedef CodeMarginEventProc = func(CallbackWidget,c_int,c_int,c_int,Rect) as c_int',
)
METHODS.update({
    'xuiCodeEditSetCompletionProviderOwned': ['int xuiCodeEditSetCompletionProviderOwned(xui_widget pWidget, xui_code_completion_reply_proc callback, void* user, const xui_callback_owner_t* owner)', 'CodeEdit', ['extern "xuiCodeEditSetCompletionProviderOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setCompletionProvider(CodeCompletionProvider? callback)']],
    'xuiCodeEditSetHoverProviderOwned': ['int xuiCodeEditSetHoverProviderOwned(xui_widget pWidget, xui_code_hover_reply_proc callback, void* user, const xui_callback_owner_t* owner)', 'CodeEdit', ['extern "xuiCodeEditSetHoverProviderOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setHoverProvider(CodeHoverProvider? callback)']],
    'xuiCodeEditSetSignatureProviderOwned': ['int xuiCodeEditSetSignatureProviderOwned(xui_widget pWidget, xui_code_signature_reply_proc callback, void* user, const xui_callback_owner_t* owner)', 'CodeEdit', ['extern "xuiCodeEditSetSignatureProviderOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setSignatureProvider(CodeSignatureProvider? callback)']],
    'xuiCodeEditRequestHoverHint': ['int xuiCodeEditRequestHoverHint(xui_widget pWidget, int offset)', 'CodeEdit', ['extern "xuiCodeEditRequestHoverHint" @no_raise @c_result("status",0) func requestHoverHint(c_int offset=-1)']],
    'xuiCodeEditSetHoverOptions': ['int xuiCodeEditSetHoverOptions(xui_widget pWidget, bool enabled, float delay)', 'CodeEdit', ['extern "xuiCodeEditSetHoverOptions" @no_raise @c_result("status",0) func setHoverOptions(bool enabled,c_float delay=0.35)']],
    'xuiCodeEditGetHoverOptions': ['int xuiCodeEditGetHoverOptions(xui_widget pWidget, bool* enabled, float* delay)', 'CodeEdit', ['extern "xuiCodeEditGetHoverOptions" @no_raise @c_outputs(1,2) @c_status(0) func getHoverOptions() as (bool,c_float)']],
    'xuiCodeEditHitTestText': ['int xuiCodeEditHitTestText(xui_widget pWidget, float fContextX, float fContextY, xui_code_edit_text_hit_t* pHit)', 'CodeEdit', ['extern "xuiCodeEditHitTestText" @no_raise @c_output(3,0) func hitTestText(c_float fContextX,c_float fContextY) as CodeEditTextHit']],
})
CALLBACKS['xui_code_completion_reply_proc'] = (
    'typedef int (*xui_code_completion_reply_proc)(xui_widget_t* pWidget, int iOffset, xui_code_completion_reply_t* pReply, void* pUser);',
    'export typedef CodeCompletionProvider = func(CallbackWidget,c_int,CodeCompletionReply) as c_int',
)
CALLBACKS['xui_code_hover_reply_proc'] = (
    'typedef int (*xui_code_hover_reply_proc)(xui_widget_t* pWidget, int iOffset, xui_code_hover_reply_t* pReply, void* pUser);',
    'export typedef CodeHoverProvider = func(CallbackWidget,c_int,CodeHoverReply) as c_int',
)
CALLBACKS['xui_code_signature_reply_proc'] = (
    'typedef int (*xui_code_signature_reply_proc)(xui_widget_t* pWidget, int iOffset, xui_code_signature_reply_t* pReply, void* pUser);',
    'export typedef CodeSignatureProvider = func(CallbackWidget,c_int,CodeSignatureReply) as c_int',
)
VIEWS['CodeCompletionReply'] = """
export extern "xui_code_completion_reply_t*" class CodeCompletionReply @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "xuiCodeCompletionReplyGetPrefix" @no_raise @c_const(0) @c_result("cstring") func prefix() as string
 extern "xuiCodeCompletionReplyGetRemaining" @no_raise @c_const(0) func remaining() as c_int
 extern "xuiCodeCompletionReplyAppendItem" @no_raise @c_pack(1,"xui_code_completion_item_t",0,11) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sLabel",0,"c_string") @c_pack_field(1,"sInsertText",1,"c_string") @c_pack_field(1,"sDetail",2,"c_string") @c_pack_field(1,"sDocumentation",3,"c_string") @c_pack_field(1,"iKind",4,"value") @c_pack_field(1,"iSortOrder",5,"value") @c_pack_field(1,"iUserData",6,"value") @c_pack_field(1,"sFilterText",7,"c_string") @c_pack_field(1,"sSortText",8,"c_string") @c_pack_field(1,"sCommitCharacters",9,"c_string") @c_pack_field(1,"iFlags",10,"value") @c_result("status",0) func add(string label,string? insertText=null,string? detail=null,string? documentation=null,c_int kind=0,c_int sortOrder=0,c_uintptr userData=0,string? filterText=null,string? sortText=null,string? commitCharacters=null,c_uint flags=0)
 extern "xuiCodeCompletionReplyAppendItems" @no_raise @c_array_group(1,0,11,"int",11) @c_array_record(1,"xui_code_completion_item_t","iSize","uint32_t") @c_array_field(1,"sLabel",0,"c_string") @c_array_field(1,"sInsertText",1,"c_string") @c_array_field(1,"sDetail",2,"c_string") @c_array_field(1,"sDocumentation",3,"c_string") @c_array_field(1,"iKind",4,"value") @c_array_field(1,"iSortOrder",5,"value") @c_array_field(1,"iUserData",6,"value") @c_array_field(1,"sFilterText",7,"c_string") @c_array_field(1,"sSortText",8,"c_string") @c_array_field(1,"sCommitCharacters",9,"c_string") @c_array_field(1,"iFlags",10,"value") @c_result("status",0) func appendItems(array<string> labels,array<string?> insertTexts,array<string?> details,array<string?> documentation,array<c_int> kinds,array<c_int> sortOrders,array<c_uintptr> userData,array<string?> filterTexts,array<string?> sortTexts,array<string?> commitCharacters,array<c_uint> flags,int size=-1)
end
""".strip()
VIEWS['CodeHoverReply'] = """
export extern "xui_code_hover_reply_t*" class CodeHoverReply @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "xuiCodeHoverReplySet" @no_raise @c_pack(1,"xui_code_hover_t",0,4) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"tRange",0,"value") @c_pack_field(1,"sText",1,"c_string") @c_pack_field(1,"sContentType",2,"c_string") @c_pack_field(1,"iUserData",3,"value") @c_result("status",0) func set(CodeRange range,string text,string? contentType=null,c_uintptr userData=0)
end
""".strip()
VIEWS['CodeSignatureReply'] = """
export extern "xui_code_signature_reply_t*" class CodeSignatureReply @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "xuiCodeSignatureReplySet" @no_raise @c_pack(1,"xui_code_signature_help_t",0,8) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sLabel",0,"c_string") @c_pack_field(1,"sDocumentation",1,"c_string") @c_pack_field(1,"iActiveParameter",2,"value") @c_pack_field(1,"iUserData",7,"value") @c_array_group(10,3,3,"int",6) @c_array_record(10,"xui_code_signature_parameter_t","iSize","uint32_t") @c_array_field(10,"sLabel",0,"c_string") @c_array_field(10,"sDocumentation",1,"c_string") @c_array_field(10,"tLabelRange",2,"value") @c_array_pack(10,1,"pParameters","iParameterCount") @c_result("status",0) func set(string label,string? documentation=null,c_int activeParameter=-1,array<string?> parameterLabels=[],array<string?> parameterDocumentation=[],array<CodeRange> parameterRanges=[],int size=-1,c_uintptr userData=0)
end
""".strip()
INPUT_DECORATION_FIELDS = (
    ('iKind','value','c_int kind'),
    ('sText','c_string','string? text=null'),
    ('fWidth','value','c_float width=0'),
    ('onClick','owned_callback','InputDecorationClickProc? click=null'),
    ('onPaint','owned_callback','InputDecorationPaintProc? paint=null'),
    ('iIcon','value','c_int icon=0'),
    ('pSurface','view_raw','Surface? surface=null'),
    ('iVisibleMode','value','c_int visibility=0'),
    ('fPadding','value','c_float padding=0'),
    ('iColor','value','c_uint color=0'),
    ('iHoverColor','value','c_uint hoverColor=0'),
    ('iActiveColor','value','c_uint activeColor=0'),
    ('iDisabledColor','value','c_uint disabledColor=0'),
    ('tSrc','value','Rect source=Rect()'),
)

def input_decoration_configure(add):
    pack=3 if add else 2
    native='xuiInputDecorationAddOwned' if add else 'xuiInputDecorationSetOwned'
    name='decorationAdd' if add else 'decorationSet'
    first='c_int side' if add else 'InputDecoration decoration'
    result='@factory(2,0,rollbackInputDecoration) @c_result("owner_view",0)' if add else '@c_result("status",0)'
    args=','.join((first,*(arg for _,_,arg in INPUT_DECORATION_FIELDS)))
    fields=[]
    for index,(field,mode,_) in enumerate(INPUT_DECORATION_FIELDS,1):
        fields.append(f'@c_pack_field({pack},"{field}",{index},"{mode}")')
        if mode=='owned_callback':
            failure=0 if field=='onClick' else -16
            fields.append(f'@c_owned_callback({index},-1,"xui_callback_owner_t",{failure},"last")')
    prefix=f'@c_pack({pack},"xui_input_decoration_desc_t",1,{len(INPUT_DECORATION_FIELDS)}) @c_pack_size({pack},"iSize","uint32_t") @c_pack_callback_group({pack},"pUser",{pack+1},"optional")'
    suffix=' as InputDecoration' if add else ''
    return f'extern "{native}" @no_raise {prefix} {" ".join(fields)} {result} func {name}({args}){suffix}'

INPUT_DECORATION_DESCRIBE = 'extern "xuiInputDecorationDescribe" @no_raise @c_output_record(1,"xui_input_decoration_desc_t","borrowed") @c_status(0) @c_output_field(0,"iKind") @c_output_field(1,"iVisibleMode") @c_output_field(2,"fWidth") @c_output_field(3,"fPadding") @c_output_field(4,"iIcon") @c_output_field(5,"sText","cstring") @c_output_field(6,"pSurface","retain",retainSurface,"zero",rollbackSurface) @c_output_field(7,"tSrc") @c_output_field(8,"iColor") @c_output_field(9,"iHoverColor") @c_output_field(10,"iActiveColor") @c_output_field(11,"iDisabledColor") func describe() as (c_int,c_int,c_float,c_float,c_int,string?,Surface?,Rect,c_uint,c_uint,c_uint,c_uint)'
INPUT_DECORATION_ROLLBACK = 'extern "xuiInputDecorationRemoveFromOwner" @no_raise @c_result("status",0) func rollbackInputDecoration(InputDecoration decoration)'

VIEWS['CallbackInputDecoration'] = """
export extern "xui_input_decoration" class CallbackInputDecoration @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "xuiInputDecorationGetBounds" @no_raise @c_output(1,0) func getBounds() as Rect
end
""".strip()
CALLBACKS.update({
    'xui_input_decoration_click_proc': (
        'typedef void (*xui_input_decoration_click_proc)(xui_widget_t* pWidget, xui_input_decoration_t* pDecoration, void* pUser);',
        'export typedef InputDecorationClickProc = func(CallbackWidget,CallbackInputDecoration)'),
    'xui_input_decoration_paint_proc': (
        'typedef int (*xui_input_decoration_paint_proc)(xui_widget_t* pWidget, xui_input_decoration_t* pDecoration, xui_draw_context_t* pDraw, xui_rect_t tRect, uint32_t iState, void* pUser);',
        'export typedef InputDecorationPaintProc = func(CallbackWidget,CallbackInputDecoration,CallbackDrawContext,Rect,c_uint) as c_int'),
})
VIEWS['InputDecoration'] = ("""
export extern "xui_input_decoration" class InputDecoration @header("xge3_runtime.h") @lifetime("owner") @owner(Context) @policy("xl6Xui3InputDecorationPolicy") @watch("xui_lifetime_link_t")
 native point handle
 extern "xuiInputDecorationGetBounds" @no_raise @c_output(1,0) func getBounds() as Rect
 {}
 extern "xuiInputDecorationRemoveFromOwner" @no_raise @c_result("status",0) func remove()
 extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()
end
""".strip().format(INPUT_DECORATION_DESCRIBE))
METHODS.update({
    'xuiCodeEditGetCommandMap': ['xui_code_command_map xuiCodeEditGetCommandMap(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetCommandMap" @no_raise @c_result("owner_view",0) func commandMap() as CodeEditCommandMap']],
    'xuiCodeEditGetFoldState': ['xui_code_fold_state xuiCodeEditGetFoldState(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetFoldState" @no_raise @c_result("owner_view",0) func foldState() as CodeEditFoldState']],
    'xuiCodeEditGetLanguageRegistry': ['xui_code_language_registry xuiCodeEditGetLanguageRegistry(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetLanguageRegistry" @no_raise @c_result("owner_view",0) func languageRegistry() as CodeEditLanguageRegistry']],
    'xuiCodeEditGetAnnotations': ['xui_code_annotation_store xuiCodeEditGetAnnotations(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetAnnotations" @no_raise @c_result("owner_view",0) func annotations() as CodeEditAnnotations']],
    'xuiCodeEditGetTokenBuffer': ['xui_code_token_buffer xuiCodeEditGetTokenBuffer(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetTokenBuffer" @no_raise @c_result("owner_view",0) func tokenBuffer() as CodeEditTokenBuffer']],
    'xuiCodeEditGetTheme': ['xui_code_theme xuiCodeEditGetTheme(xui_widget pWidget)', 'CodeEdit', ['extern "xuiCodeEditGetTheme" @no_raise @c_result("owner_view",0) func theme() as CodeEditTheme']],
    'xuiInputDecorationGetAt': ['xui_input_decoration xuiInputDecorationGetAt(xui_widget pWidget, int iSide, int iIndex)', 'Input', ['extern "xuiInputDecorationGetAt" @no_raise @c_result("owner_view",0) func decorationGetAt(c_int side,c_int index) as InputDecoration?']],
    'xuiInputDecorationAddOwned': ['int xuiInputDecorationAddOwned(xui_widget pWidget, int iSide, xui_input_decoration* ppDecoration, const xui_input_decoration_desc_t* pDesc, const xui_callback_owner_t* pOwner)', 'Input', [input_decoration_configure(True)]],
    'xuiInputDecorationSetOwned': ['int xuiInputDecorationSetOwned(xui_widget pWidget, xui_input_decoration pDecoration, const xui_input_decoration_desc_t* pDesc, const xui_callback_owner_t* pOwner)', 'Input', [input_decoration_configure(False)]],
    'xuiInputDecorationRemove': ['int xuiInputDecorationRemove(xui_widget pWidget, xui_input_decoration pDecoration)', 'Input', ['extern "xuiInputDecorationRemove" @no_raise @c_result("status",0) func decorationRemove(InputDecoration decoration)']],
    'xuiCodeEditMarkAllPlain': ['int xuiCodeEditMarkAllPlain(xui_widget widget, const char* pattern, uint32_t flags, int indicator, int* matches)', 'CodeEdit', ['extern "xuiCodeEditMarkAllPlain" @no_raise @c_cstr(0) @c_output(4,0) func markAllPlain(string pattern,c_uint flags=0,c_int indicator=0) as c_int']],
    'xuiCodeEditMarkAllRegex': ['int xuiCodeEditMarkAllRegex(xui_widget widget, const char* pattern, uint32_t flags, int indicator, int* matches, char* error, int errorCapacity)', 'CodeEdit', ['extern "xuiCodeEditMarkAllRegex" @no_raise @c_cstr(0) @c_output(4,0) @c_span(3,"char*","int") func markAllRegex(string pattern,c_uint flags=0,c_int indicator=0,Buffer? error=null) as c_int']],
    'xuiCodeEditQueryCommandEnabled': ['int xuiCodeEditQueryCommandEnabled(xui_widget widget, int command, bool* hasDecision, bool* enabled)', 'CodeEdit', ['extern "xuiCodeEditQueryCommandEnabled" @no_raise @c_outputs(2,3) @c_status(0) func queryCommandEnabled(c_int command) as (bool,bool)']],
    'xuiCodeEditExecuteCommand': ['int xuiCodeEditExecuteCommand(xui_widget widget, int command, bool* handled)', 'CodeEdit', ['extern "xuiCodeEditExecuteCommand" @no_raise @c_output(2,0) func executeCommand(c_int command) as bool']],
    'xuiCodeEditSetCommandProviderOwned': ['int xuiCodeEditSetCommandProviderOwned(xui_widget widget, xui_code_editor_command_proc callback, void* user, const xui_callback_owner_t* owner)', 'CodeEdit', ['extern "xuiCodeEditSetCommandProviderOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setCommandProvider(CodeEditorCommand? callback)']],
    'xuiCodeEditSetCommandEnabledProviderOwned': ['int xuiCodeEditSetCommandEnabledProviderOwned(xui_widget widget, xui_code_editor_command_proc callback, void* user, const xui_callback_owner_t* owner)', 'CodeEdit', ['extern "xuiCodeEditSetCommandEnabledProviderOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setCommandEnabledProvider(CodeEditorCommand? callback)']],
})
CALLBACKS['xui_code_editor_command_proc'] = (
    'typedef int (*xui_code_editor_command_proc)(xui_widget_t* pWidget, int iCommand, int* pResult, void* pUser);',
    'export typedef CodeEditorCommand = func(CallbackWidget,c_int,CallbackInt) as c_int',
)
CALLBACKS['xui_code_document_change_proc'] = (
    'typedef void (*xui_code_document_change_proc)(xui_code_document_t* pDocument, xui_code_range_t tRange, uint32_t iVersion, void* pUser);',
    'export typedef CodeDocumentChangeProc = func(CallbackCodeDocument,CodeRange,c_uint)',
)
VIEWS['CallbackCodeDocument'] = """
export extern "xui_code_document_t*" class CallbackCodeDocument @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "xuiCodeDocumentGetTextView" @no_raise @c_output(1,0) @c_output_view(1,"text") func getText() as string
 extern "xuiCodeDocumentGetLength" @no_raise func getLength() as c_int
 extern "xuiCodeDocumentGetLineCount" @no_raise func getLineCount() as c_int
 extern "xuiCodeDocumentGetLineRange" @no_raise @c_outputs(2,3) @c_status(0) func getLineRange(c_int line) as (c_int,c_int)
 extern "xuiCodeDocumentGetByte" @no_raise @c_output(2,0) func getByte(c_int offset) as c_char
end
""".strip()
METHODS['xuiCodeEditSetInputHandlerOwned'] = [
    'int xuiCodeEditSetInputHandlerOwned(xui_widget pWidget, xui_code_input_owned_proc onInput, void* pUser, const xui_callback_owner_t* pOwner)',
    'CodeEdit', ['extern "xuiCodeEditSetInputHandlerOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setInputHandler(CodeInputHandler? handler)'],
]
CALLBACKS['xui_code_input_owned_proc'] = (
    'typedef int (*xui_code_input_owned_proc)(xui_widget_t* pWidget, const xui_code_input_event_t* pEvent, xui_code_input_response_t* pAction, void* pUser);',
    'export typedef CodeInputHandler = func(CallbackWidget,CallbackConstCodeInputEvent,CallbackCodeInputResponse) as c_int',
)
VIEWS.update({
    'CallbackConstCodeInputEvent': """
export extern "const xui_code_input_event_t*" class CallbackConstCodeInputEvent @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iPhase" @c_field get phase() as c_int
 extern "iSource" @c_field get source() as c_int
 extern "pDocument" @c_field @c_result("scope_view",0) get document() as CallbackCodeDocument
 extern "iDocumentVersion" @c_field get documentVersion() as c_uint
 extern "sText" @c_field @c_length("iTextSize","int") @c_result("text_view") get text() as string
 extern "iTextSize" @c_field get textSize() as c_int
 extern "iCodepoint" @c_field get codepoint() as c_uint
 extern "iKey" @c_field get key() as c_int
 extern "iModifiers" @c_field get modifiers() as c_uint
 extern "tSelectionBefore" @c_field get selectionBefore() as CodeSelection
 extern "tSelectionAfter" @c_field get selectionAfter() as CodeSelection
 extern "tReplacedRange" @c_field get replacedRange() as CodeRange
 extern "tInsertedRange" @c_field get insertedRange() as CodeRange
end
""".strip(),
    'CallbackCodeInputResponse': """
export extern "xui_code_input_response_t*" class CallbackCodeInputResponse @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "xuiCodeInputResponseGetFlags" @no_raise @c_const(0) func flags() as c_uint
 extern "xuiCodeInputResponseSetFlags" @no_raise @c_result("status",0) func setFlags(c_uint flags)
 extern "xuiCodeInputResponseSetSelection" @no_raise @c_result("status",0) func setSelection(c_int anchor,c_int caret)
 extern "xuiCodeInputResponseSetEdits" @no_raise @c_array_group(1,0,2,"int",2) @c_array_record(1,"xui_code_text_edit_t","iSize","uint32_t") @c_array_field(1,"tRange",0,"value") @c_array_field(1,"sText",1,"c_string") @c_result("status",0) func setEdits(array<CodeRange> ranges,array<string> replacements,int size=-1)
end
""".strip(),
})
TYPE_SHAPES.update({
    'xui_code_text_edit_t': 'ea4a11dc194bdd6c6409e45e1b0d3f0fcd301999db1382124acac29525079ed0',
    'xui_code_input_event_t': 'dd6b743d0c04ada452b02084b1e0176670712f6ccb9d190f05faac0596bb4092',
    'xui_code_input_response_t': '9944445e0a01cb20a77fb946309e5dd4b6633837eeb756df81084afd0adbed72',
    'xui_code_input_owned_proc': 'd26b97f1de461e28f9a6bc5f51861acbb064a5d2da5b434ca5c49cdac97cc48e',
})

# Typed style rules and scoped snapshots of nested property records.
METHODS.update({
    'xuiStyleSetNamed': ['int xuiStyleSetNamed(xui_context pContext, const xui_style_desc_t* pStyle)', 'Context',
        ['extern "xuiStyleSetNamed" @no_raise @c_pack(1,"xui_style_desc_t",0,11) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sName",0,"c_string") @c_pack_field(1,"sParent",1,"c_string") @c_array_pack(20,1,"pProperties","iPropertyCount") @c_array_group(20,2,8,"int",10) @c_array_record(20,"xui_style_property_t","iSize","uint32_t") @c_array_size(20,"tValue.iSize","uint32_t") @c_array_field(20,"iPropertyId",0,"value") @c_array_field(20,"sName",1,"c_string") @c_array_field(20,"iDirtyFlags",2,"value") @c_array_field(20,"tValue.iType",3,"value") @c_array_field(20,"tValue.iInt",4,"value") @c_array_field(20,"tValue.fFloat",5,"value") @c_array_field(20,"tValue.iColor",6,"value") @c_array_field(20,"tValue.sText",7,"c_string") @c_result("status",0) func styleSetNamed(string name,string? parent,array<c_uint> ids,array<string?> names,array<c_uint> dirtyFlags,array<c_int> types,array<c_int> integers,array<c_float> numbers,array<c_uint> colors,array<string?> texts,int size=-1)']],
    'xuiStyleSetClass': ['int xuiStyleSetClass(xui_context pContext, const char* sClass, const xui_style_desc_t* pStyle)', 'Context',
        ['extern "xuiStyleSetClass" @no_raise @c_cstr(0) @c_pack(2,"xui_style_desc_t",1,10) @c_pack_size(2,"iSize","uint32_t") @c_pack_field(2,"sParent",1,"c_string") @c_array_pack(20,2,"pProperties","iPropertyCount") @c_array_group(20,2,8,"int",10) @c_array_record(20,"xui_style_property_t","iSize","uint32_t") @c_array_size(20,"tValue.iSize","uint32_t") @c_array_field(20,"iPropertyId",0,"value") @c_array_field(20,"sName",1,"c_string") @c_array_field(20,"iDirtyFlags",2,"value") @c_array_field(20,"tValue.iType",3,"value") @c_array_field(20,"tValue.iInt",4,"value") @c_array_field(20,"tValue.fFloat",5,"value") @c_array_field(20,"tValue.iColor",6,"value") @c_array_field(20,"tValue.sText",7,"c_string") @c_result("status",0) func styleSetClass(string className,string? parent,array<c_uint> ids,array<string?> names,array<c_uint> dirtyFlags,array<c_int> types,array<c_int> integers,array<c_float> numbers,array<c_uint> colors,array<string?> texts,int size=-1)']],
    'xuiStyleSetStateClass': ['int xuiStyleSetStateClass(xui_context pContext, const xui_state_style_desc_t* pStyle)', 'Context',
        ['extern "xuiStyleSetStateClass" @no_raise @c_pack(1,"xui_state_style_desc_t",0,11) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sClass",0,"c_string") @c_pack_field(1,"iStateMask",1,"value") @c_array_pack(20,1,"pProperties","iPropertyCount") @c_array_group(20,2,8,"int",10) @c_array_record(20,"xui_style_property_t","iSize","uint32_t") @c_array_size(20,"tValue.iSize","uint32_t") @c_array_field(20,"iPropertyId",0,"value") @c_array_field(20,"sName",1,"c_string") @c_array_field(20,"iDirtyFlags",2,"value") @c_array_field(20,"tValue.iType",3,"value") @c_array_field(20,"tValue.iInt",4,"value") @c_array_field(20,"tValue.fFloat",5,"value") @c_array_field(20,"tValue.iColor",6,"value") @c_array_field(20,"tValue.sText",7,"c_string") @c_result("status",0) func styleSetStateClass(string className,c_uint stateMask,array<c_uint> ids,array<string?> names,array<c_uint> dirtyFlags,array<c_int> types,array<c_int> integers,array<c_float> numbers,array<c_uint> colors,array<string?> texts,int size=-1)']],
    'xuiStyleSetDefault': ['int xuiStyleSetDefault(xui_context pContext, const xui_style_property_t* pProperties, int iPropertyCount)', 'Context',
        ['extern "xuiStyleSetDefault" @no_raise @c_array_group(1,0,8,"int",8) @c_array_record(1,"xui_style_property_t","iSize","uint32_t") @c_array_size(1,"tValue.iSize","uint32_t") @c_array_field(1,"iPropertyId",0,"value") @c_array_field(1,"sName",1,"c_string") @c_array_field(1,"iDirtyFlags",2,"value") @c_array_field(1,"tValue.iType",3,"value") @c_array_field(1,"tValue.iInt",4,"value") @c_array_field(1,"tValue.fFloat",5,"value") @c_array_field(1,"tValue.iColor",6,"value") @c_array_field(1,"tValue.sText",7,"c_string") @c_result("status",0) func styleSetDefault(array<c_uint> ids,array<string?> names,array<c_uint> dirtyFlags,array<c_int> types,array<c_int> integers,array<c_float> numbers,array<c_uint> colors,array<string?> texts,int size=-1)']],
    'xuiStyleRegisterProperty': ['int xuiStyleRegisterProperty(xui_context pContext, const xui_style_property_info_t* pInfo, uint32_t* pPropertyId)', 'Context',
        ['extern "xuiStyleRegisterProperty" @no_raise @c_pack(1,"xui_style_property_info_t",0,10) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"sName",0,"c_string") @c_pack_field(1,"iValueType",1,"value") @c_pack_field(1,"iDirtyFlags",2,"value") @c_pack_field(1,"iFlags",3,"value") @c_pack_record(1,"tDefaultValue","xui_style_value_t") @c_pack_size(1,"tDefaultValue.iSize","uint32_t","tDefaultValue") @c_pack_field(1,"tDefaultValue.iType",4,"value") @c_pack_field(1,"tDefaultValue.iInt",5,"value") @c_pack_field(1,"tDefaultValue.fFloat",6,"value") @c_pack_field(1,"tDefaultValue.iColor",7,"value") @c_pack_field(1,"tDefaultValue.sText",8,"c_string") @c_pack_field(1,"pWidgetType",9,"view_raw") @c_output(2,0) func styleRegisterProperty(string name,c_int valueType,c_uint dirtyFlags=0,c_uint flags=0,c_int defaultType=0,c_int defaultInt=0,c_float defaultFloat=0,c_uint defaultColor=0,string? defaultText=null,WidgetType? widgetType=null) as c_uint']],
    'xuiStyleGetPropertyInfo': ['int xuiStyleGetPropertyInfo(xui_context pContext, uint32_t iPropertyId, xui_style_property_info_t* pInfo)', 'Context',
        ['extern "xuiStyleGetPropertyInfo" @no_raise @c_output_record(2,"xui_style_property_info_t","borrowed") @c_output_size(2,"iSize") @c_status(0) @c_output_field(0,"iPropertyId") @c_output_field(1,"sName","cstring") @c_output_field(2,"iValueType") @c_output_field(3,"iDirtyFlags") @c_output_field(4,"iFlags") @c_output_field(5,"tDefaultValue.iType") @c_output_field(6,"tDefaultValue.iInt") @c_output_field(7,"tDefaultValue.fFloat") @c_output_field(8,"tDefaultValue.iColor") @c_output_field(9,"tDefaultValue.sText","cstring") func styleGetPropertyInfo(c_uint id) as (c_uint,string?,c_int,c_uint,c_uint,c_int,c_int,c_float,c_uint,string?)',
        'extern "xuiStyleGetPropertyInfo" @no_raise @c_output_record(2,"xui_style_property_info_t","borrowed") @c_output_size(2,"iSize") @c_status(0) @c_output_field(0,"pWidgetType","owner_view") func styleGetPropertyScope(c_uint id) as WidgetType?']],
    'xuiWidgetSetInlineStyle': ['int xuiWidgetSetInlineStyle(xui_widget pWidget, const xui_style_property_t* pProperties, int iPropertyCount)', 'Widget',
        ['extern "xuiWidgetSetInlineStyle" @no_raise @c_array_group(1,0,8,"int",8) @c_array_record(1,"xui_style_property_t","iSize","uint32_t") @c_array_size(1,"tValue.iSize","uint32_t") @c_array_field(1,"iPropertyId",0,"value") @c_array_field(1,"sName",1,"c_string") @c_array_field(1,"iDirtyFlags",2,"value") @c_array_field(1,"tValue.iType",3,"value") @c_array_field(1,"tValue.iInt",4,"value") @c_array_field(1,"tValue.fFloat",5,"value") @c_array_field(1,"tValue.iColor",6,"value") @c_array_field(1,"tValue.sText",7,"c_string") @c_result("status",0) func setInlineStyle(array<c_uint> ids,array<string?> names,array<c_uint> dirtyFlags,array<c_int> types,array<c_int> integers,array<c_float> numbers,array<c_uint> colors,array<string?> texts,int size=-1)']],
    'xuiWidgetGetInlineStyleProperty': ['int xuiWidgetGetInlineStyleProperty(xui_widget pWidget, const char* sName, xui_style_property_t* pProperty)', 'Widget',
        ['extern "xuiWidgetGetInlineStyleProperty" @no_raise @c_cstr(0) @c_output_record(2,"xui_style_property_t","borrowed") @c_output_size(2,"iSize") @c_status(0) @c_output_field(0,"iPropertyId") @c_output_field(1,"sName","cstring") @c_output_field(2,"iDirtyFlags") @c_output_field(3,"tValue.iType") @c_output_field(4,"tValue.iInt") @c_output_field(5,"tValue.fFloat") @c_output_field(6,"tValue.iColor") @c_output_field(7,"tValue.sText","cstring") func getInlineStyleProperty(string name) as (c_uint,string?,c_uint,c_int,c_int,c_float,c_uint,string?)']],
    'xuiWidgetGetResolvedStyleProperty': ['int xuiWidgetGetResolvedStyleProperty(xui_widget pWidget, const char* sName, xui_style_property_t* pProperty)', 'Widget',
        ['extern "xuiWidgetGetResolvedStyleProperty" @no_raise @c_cstr(0) @c_output_record(2,"xui_style_property_t","borrowed") @c_output_size(2,"iSize") @c_status(0) @c_output_field(0,"iPropertyId") @c_output_field(1,"sName","cstring") @c_output_field(2,"iDirtyFlags") @c_output_field(3,"tValue.iType") @c_output_field(4,"tValue.iInt") @c_output_field(5,"tValue.fFloat") @c_output_field(6,"tValue.iColor") @c_output_field(7,"tValue.sText","cstring") func getResolvedStyleProperty(string name) as (c_uint,string?,c_uint,c_int,c_int,c_float,c_uint,string?)']],
    'xuiWidgetGetResolvedStylePropertyAt': ['int xuiWidgetGetResolvedStylePropertyAt(xui_widget pWidget, int iIndex, xui_style_property_t* pProperty)', 'Widget',
        ['extern "xuiWidgetGetResolvedStylePropertyAt" @no_raise @c_output_record(2,"xui_style_property_t","borrowed") @c_output_size(2,"iSize") @c_status(0) @c_output_field(0,"iPropertyId") @c_output_field(1,"sName","cstring") @c_output_field(2,"iDirtyFlags") @c_output_field(3,"tValue.iType") @c_output_field(4,"tValue.iInt") @c_output_field(5,"tValue.fFloat") @c_output_field(6,"tValue.iColor") @c_output_field(7,"tValue.sText","cstring") func getResolvedStylePropertyAt(c_int index) as (c_uint,string?,c_uint,c_int,c_int,c_float,c_uint,string?)']],
})


VIEWS["WidgetType"] = 'export extern "xui_widget_type_t*" class WidgetType @header("xge3_runtime.h") @lifetime("owner") @owner(Context) @policy("xl6Xui3WidgetTypePolicy") @watch("xui_lifetime_link_t")\n native point handle\n extern "xuiWidgetTypeGetName" @no_raise @c_result("cstring") func getName() as string\n extern "xuiWidgetTypeGetParent" @no_raise @c_result("owner_view",0) func getParent() as WidgetType?\n extern "xuiWidgetUnregisterType" @no_raise @c_result("status",0) func unregister()\n extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()\nend'

METHODS.update({
    'xuiWidgetFindType': ['xui_widget_type xuiWidgetFindType(xui_context pContext, const char* sName)', 'Context',
        ['extern "xuiWidgetFindType" @no_raise @c_cstr(0) @c_result("owner_view",0) func findWidgetType(string name) as WidgetType?']],
    'xuiWidgetRegisterType': ['int xuiWidgetRegisterType(xui_context pContext, xui_widget_type* ppType, const xui_widget_type_desc_t* pDesc)', 'Context',
        ['extern "xuiWidgetRegisterType" @no_raise @c_pack(2,"xui_widget_type_desc_t",0,3) @c_pack_size(2,"iSize","uint32_t") @c_pack_field(2,"sName",0,"c_string") @c_pack_field(2,"pParent",1,"view_raw") @c_pack_field(2,"iFlags",2,"value") @c_result("owner_view",0) @factory(1,0,rollbackWidgetType) func registerWidgetType(string name,WidgetType? parent=null,c_uint flags=0) as WidgetType']],
    'xuiWidgetCreateTyped': ['int xuiWidgetCreateTyped(xui_context pContext, xui_widget_type pType, xui_widget* ppWidget, const void* pCreateData)', 'Context',
        ['extern "xuiWidgetCreateTyped" @no_raise @c_null(3,"const void*") @c_result("owner_view",0) @factory(2,0,rollbackWidget) func createWidget(WidgetType type) as Widget']],
    'xuiStyleSetType': ['int xuiStyleSetType(xui_context pContext, xui_widget_type pType, const xui_style_desc_t* pStyle)', 'Context',
        ['extern "xuiStyleSetType" @no_raise @c_pack(2,"xui_style_desc_t",1,10) @c_pack_size(2,"iSize","uint32_t") @c_pack_field(2,"sParent",1,"c_string") @c_array_pack(20,2,"pProperties","iPropertyCount") @c_array_group(20,2,8,"int",10) @c_array_record(20,"xui_style_property_t","iSize","uint32_t") @c_array_size(20,"tValue.iSize","uint32_t") @c_array_field(20,"iPropertyId",0,"value") @c_array_field(20,"sName",1,"c_string") @c_array_field(20,"iDirtyFlags",2,"value") @c_array_field(20,"tValue.iType",3,"value") @c_array_field(20,"tValue.iInt",4,"value") @c_array_field(20,"tValue.fFloat",5,"value") @c_array_field(20,"tValue.iColor",6,"value") @c_array_field(20,"tValue.sText",7,"c_string") @c_result("status",0) func styleSetType(WidgetType type,string? parent,array<c_uint> ids,array<string?> names,array<c_uint> dirtyFlags,array<c_int> types,array<c_int> integers,array<c_float> numbers,array<c_uint> colors,array<string?> texts,int size=-1)']],
    'xuiStyleRemoveType': ['int xuiStyleRemoveType(xui_context pContext, xui_widget_type pType)', 'Context',
        ['extern "xuiStyleRemoveType" @no_raise @c_result("status",0) func styleRemoveType(WidgetType type)']],
    'xuiWidgetGetType': ['xui_widget_type xuiWidgetGetType(xui_widget pWidget)', 'Widget',
        ['extern "xuiWidgetGetType" @no_raise @c_result("owner_view",0) func widgetType() as WidgetType']],
    'xuiWidgetIsType': ['int xuiWidgetIsType(xui_widget pWidget, xui_widget_type pType)', 'Widget',
        ['extern "xuiWidgetIsType" @no_raise func isType(WidgetType type) as c_int']],
})

for control in (
    'Label', 'Hyperlink', 'Breadcrumb', 'Image', 'QrCode', 'Panel',
    'Separator', 'Progress', 'StepBar', 'Chart', 'CodeEdit', 'Input',
    'TagInput', 'NumericInput', 'TextEdit', 'Button', 'CheckBox', 'CheckCard',
    'Radio', 'RadioGroup', 'Toggle', 'ScrollBar', 'Slider', 'RangeSlider',
    'Page', 'Carousel', 'VirtualJoystick', 'InventoryGrid', 'Terminal', 'SplitLayout',
    'Tabs', 'Accordion', 'Window', 'DockPanel', 'ScrollFrame', 'ScrollView',
    'Canvas', 'ListView', 'MessageList', 'TreeView', 'TableView', 'TableGrid',
    'TimeLineView', 'PropertyGrid', 'Popup', 'Menu', 'MenuBar', 'Toolbar',
    'StatusBar', 'ComboBox', 'Cascader', 'ColorPicker', 'IconPicker', 'DatePicker',
    'FlowGraph', 'Workflow', 'WebView',
):
    native = f"xui{control}GetType"
    method = control[0].lower() + control[1:] + "Type"
    METHODS[native] = [f"xui_widget_type {native}(xui_context pContext)", "Context",
        [f'extern "{native}" @no_raise @c_result("owner_view",0) func {method}() as WidgetType?']]

VIEWS.update({
    'CallbackWidget': 'export extern "xui_widget_t*" class CallbackWidget @header("xge3_runtime.h") @lifetime("callback")\n native point handle\n extern "xuiWidgetGetRect" @no_raise func rect() as Rect\n extern "xuiWidgetSetRect" @no_raise @c_result("status",0) func setRect(Rect value)\n extern "xuiWidgetInvalidate" @no_raise @c_result("status",0) func invalidate(c_uint dirtyFlags)\n extern "xuiWidgetSetInstanceState" @no_raise @c_result("status",0) func setInstanceState(var value)\n extern "xuiWidgetGetInstanceState" @no_raise @c_output(1,0) @c_output_view(1,"owned_value") func getInstanceState() as var\nend',
    'CallbackWidgetTypeData': 'export extern "void*" class CallbackWidgetTypeData @header("xge3_runtime.h") @lifetime("callback")\n native point handle\nend',
    'CallbackWidgetCreateData': 'export extern "const void*" class CallbackWidgetCreateData @header("xge3_runtime.h") @lifetime("callback")\n native point handle\nend',
})

WIDGET_TYPE_CALLBACKS = (
    'export typedef WidgetTypeInitProc = func(CallbackWidget,CallbackWidgetTypeData?,CallbackWidgetCreateData?) as c_int',
    'export typedef WidgetTypeDestroyProc = func(CallbackWidget,CallbackWidgetTypeData?)',
    'export typedef WidgetTypeMeasureProc = func(CallbackWidget,Vec2,CallbackVec2) as c_int',
    'export typedef WidgetTypeLayoutPrepareProc = func(CallbackWidget) as c_int',
    'export typedef WidgetTypeLayoutChildrenProc = func(CallbackWidget,Rect) as c_int',
    'export typedef WidgetTypeLayoutCompleteProc = func(CallbackWidget,Rect) as c_int',
    'export typedef WidgetTypeRenderProc = func(CallbackWidget,CallbackDrawContext,c_uint) as c_int',
    'export typedef WidgetTypeUpdateProc = func(CallbackWidget,c_float) as c_int',
    'export typedef WidgetTypeCursorProc = func(CallbackWidget,c_int,c_int) as c_int',
)

METHODS.update({
    'xuiWidgetSetInstanceState': ['int xuiWidgetSetInstanceState(xui_widget pWidget, xvalue* pValue)', 'Widget',
        ['extern "xuiWidgetSetInstanceState" @no_raise @c_result("status",0) func setInstanceState(var value)']],
    'xuiWidgetGetInstanceState': ['int xuiWidgetGetInstanceState(xui_widget pWidget, xvalue** ppValue)', 'Widget',
        ['extern "xuiWidgetGetInstanceState" @no_raise @c_output(1,0) @c_output_view(1,"owned_value") func getInstanceState() as var']],
})

METHODS["xuiWidgetRegisterTypeOwned"] = ['int xuiWidgetRegisterTypeOwned(xui_context pContext, xui_widget_type* ppType, const xui_widget_type_desc_t* pDesc, const xui_callback_owner_t* pOwner)', "Context",
    ['extern "xuiWidgetRegisterTypeOwned" @no_raise @c_pack(2,"xui_widget_type_desc_t",0,12) @c_pack_size(2,"iSize","uint32_t") @c_pack_callback_group(2,"pUser",3,"optional") @c_pack_field(2,"sName",0,"c_string") @c_pack_field(2,"pParent",1,"view_raw") @c_pack_field(2,"iFlags",2,"value") @c_pack_field(2,"onInit",3,"owned_callback") @c_owned_callback(3,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(2,"onDestroy",4,"owned_callback") @c_owned_callback(4,-1,"xui_callback_owner_t",0,"last") @c_pack_field(2,"onContentMeasure",5,"owned_callback") @c_owned_callback(5,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(2,"onLayoutPrepare",6,"owned_callback") @c_owned_callback(6,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(2,"onLayoutChildren",7,"owned_callback") @c_owned_callback(7,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(2,"onLayoutComplete",8,"owned_callback") @c_owned_callback(8,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(2,"onCacheRender",9,"owned_callback") @c_owned_callback(9,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(2,"onUpdate",10,"owned_callback") @c_owned_callback(10,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(2,"onQueryCursor",11,"owned_callback") @c_owned_callback(11,-1,"xui_callback_owner_t",-16,"last") @c_result("owner_view",0) @factory(1,0,rollbackWidgetType) func registerWidgetTypeWithCallbacks(string name,WidgetType? parent=null,c_uint flags=0,WidgetTypeInitProc? initialize=null,WidgetTypeDestroyProc? destroy=null,WidgetTypeMeasureProc? measure=null,WidgetTypeLayoutPrepareProc? prepareLayout=null,WidgetTypeLayoutChildrenProc? layoutChildren=null,WidgetTypeLayoutCompleteProc? completeLayout=null,WidgetTypeRenderProc? render=null,WidgetTypeUpdateProc? update=null,WidgetTypeCursorProc? queryCursor=null) as WidgetType']]

VIEW_FUNCTIONS = {'xuiGetLanguage': 'int xuiGetLanguage(xui_context pContext)', 'xuiRequestClose': 'int xuiRequestClose(xui_context pContext)'}
VIEW_FUNCTIONS['xuiInputDecorationGetBounds'] = 'int xuiInputDecorationGetBounds(xui_input_decoration pDecoration, xui_rect_t* pRect)'
VIEW_FUNCTIONS['xuiInputDecorationDescribe'] = 'int xuiInputDecorationDescribe(xui_input_decoration pDecoration, xui_input_decoration_desc_t* pDesc)'
VIEW_FUNCTIONS['xuiInputDecorationRemoveFromOwner'] = 'int xuiInputDecorationRemoveFromOwner(xui_input_decoration pDecoration)'

VIEW_FUNCTIONS.update({
    'xuiWidgetTypeGetName': 'const char* xuiWidgetTypeGetName(xui_widget_type pType)',
    'xuiWidgetTypeGetParent': 'xui_widget_type xuiWidgetTypeGetParent(xui_widget_type pType)',
    'xuiWidgetUnregisterType': 'int xuiWidgetUnregisterType(xui_widget_type pType)',
})

VIEW_FUNCTIONS.update({
    'xuiCodeInputResponseGetFlags': 'uint32_t xuiCodeInputResponseGetFlags(const xui_code_input_response_t* response)',
    'xuiCodeInputResponseSetFlags': 'int xuiCodeInputResponseSetFlags(xui_code_input_response_t* response, uint32_t flags)',
    'xuiCodeInputResponseSetSelection': 'int xuiCodeInputResponseSetSelection(xui_code_input_response_t* response, int anchor, int caret)',
    'xuiCodeInputResponseSetEdits': 'int xuiCodeInputResponseSetEdits(xui_code_input_response_t* response, const xui_code_text_edit_t* edits, int count)',
})

INLINE_FUNCTIONS = {'xuiEventDragData': 'f9bd130d67726da8ae4ca0fa029089b33cb1ed0de3d9918fec59bee9d0ba219c'}

INLINE_FUNCTIONS['xuiCodeDiagnosticHitDetails'] = '35a47617b74dfa170459596fcc0eed87afadf2a46645e14f72320e900755c192'

# Owned WebView Close events are qualified independently of browser startup.
# Platform/browser readiness and successful zoom remain runtime prerequisites.
DEFERRED = {}

# Sized text views preserve embedded NUL and checked read failure. The legacy
RECORDS.update({
    'xui_tooltip_placement_t': 'typedef struct xui_tooltip_placement_t { int iAnchor; int bCustomAnchorRect; xui_rect_t tAnchorRect; '
                               'float fOffsetX; float fOffsetY; float fDelay; int bFollowCursor; } xui_tooltip_placement_t;',
    'xui_tooltip_info_t': 'typedef struct xui_tooltip_info_t { uint32_t iSize; int iType; const char* sText; '
                          'xui_tooltip_placement_t tPlacement; } xui_tooltip_info_t;',
    'xui_custom_tooltip_desc_t': 'typedef struct xui_custom_tooltip_desc_t { uint32_t iSize; xui_tooltip_placement_t tPlacement; '
                                 'xui_tooltip_measure_proc onMeasure; xui_tooltip_paint_proc onPaint; void* pUser; } xui_custom_tooltip_desc_t;',
    'xui_tooltip_resolver_desc_t': 'typedef struct xui_tooltip_resolver_desc_t { uint32_t iSize; xui_tooltip_reply_proc onResolve; '
                                   'xui_tooltip_measure_proc onMeasure; xui_tooltip_paint_proc onPaint; void* pUser; } xui_tooltip_resolver_desc_t;',
})
CALLBACKS.update({
    'xui_tooltip_measure_proc': (
        'typedef xui_vec2_t (*xui_tooltip_measure_proc)(xui_context pContext, xui_widget pOwner, void* pUser);',
        'export typedef TooltipMeasureProc = func(CallbackContext,CallbackWidget) as Vec2',
    ),
    'xui_tooltip_paint_proc': (
        'typedef int (*xui_tooltip_paint_proc)(xui_context pContext, xui_widget pOwner, xui_draw_context pDraw, xui_rect_t tRect, void* pUser);',
        'export typedef TooltipPaintProc = func(CallbackContext,CallbackWidget,CallbackDrawContext,Rect) as c_int',
    ),
    'xui_tooltip_reply_proc': (
        'typedef int (*xui_tooltip_reply_proc)(xui_context pContext, xui_widget pWidget, xui_tooltip_reply_t* pReply, void* pUser);',
        'export typedef TooltipResolverProc = func(CallbackContext,CallbackWidget,TooltipReply) as c_int',
    ),
})
VIEW_FUNCTIONS.update({
    'xuiTooltipReplySetText': 'int xuiTooltipReplySetText(xui_tooltip_reply_t* pReply, xstrview tText, const xui_tooltip_placement_t* pPlacement)',
    'xuiTooltipReplySetCustom': 'int xuiTooltipReplySetCustom(xui_tooltip_reply_t* pReply, const xui_tooltip_placement_t* pPlacement)',
})
VIEWS['TooltipReply'] = """
export extern "xui_tooltip_reply_t*" class TooltipReply @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "xuiTooltipReplySetText" @no_raise @c_text_view(0) @c_ref(1) @c_result("status",0) func setText(string text,TooltipPlacement placement)
 extern "xuiTooltipReplySetCustom" @no_raise @c_ref(0) @c_result("status",0) func setCustom(TooltipPlacement placement)
end
""".strip()
TYPE_SHAPES.update({
    'xui_tooltip_placement_t': '7e392f48a4649b6903a5f310011403fcbcc6c83a0bd0a9211c0de000a51eaeec',
    'xui_tooltip_info_t': '980e18b4dfa23d765168b5b8fd4cfb4163f4b0efbbe32d9639eb6ef73fd22013',
    'xui_custom_tooltip_desc_t': '819255cc83e84c6a4207d08db88f06a453679532fee06ce97149061b640eac8e',
    'xui_tooltip_measure_proc': 'ef6ed08d8e94cee3e665bc0bd0a62d5a63013037fae4ced247fe322b159f0380',
    'xui_tooltip_paint_proc': '6dbc7199687e3bc05cca4077ba96b47b3d3065847e08e3122ed1bf8bb1585092',
    'xui_tooltip_reply_proc': '42295719473f790af1a8a2f5d073eaeaa3a3c89fdaa093839c801ddfa9f86001',
    'xui_tooltip_resolver_desc_t': 'a1b7eaa329d11595afca1001a1a9f952dc258b54c426e75496a3f5c26fa2d119',
})
METHODS.update({
    'xuiToolbarSetOverflowOwned': [
        'int xuiToolbarSetOverflowOwned(xui_widget pWidget, int bEnabled, float fButtonSize, '
        'xui_toolbar_overflow_proc onOverflow, void* pUser, const xui_callback_owner_t* pOwner)', 'Toolbar',
        ['extern "xuiToolbarSetOverflowOwned" @no_raise @c_owned_callback(2,0,"xui_callback_owner_t",0) '
         '@c_result("status",0) func setOverflow(c_int enabled,c_float buttonSize,ToolbarOverflowProc? onOverflow=null)']],
    'xuiTooltipPlacementDefault': [
        'xui_tooltip_placement_t xuiTooltipPlacementDefault(void)', None,
        ['export extern "xuiTooltipPlacementDefault" @header("xge3_runtime.h") @pure @no_raise '
         'func tooltipPlacementDefault() as TooltipPlacement']],
    'xuiWidgetSetTextTooltip': [
        'int xuiWidgetSetTextTooltip(xui_widget pWidget, const char* sText, const xui_tooltip_placement_t* pPlacement)', 'Widget',
        ['extern "xuiWidgetSetTextTooltip" @no_raise @c_cstr(0) @c_ref(1) @c_result("status",0) '
         'func setTooltipText(string text,TooltipPlacement placement)']],
    'xuiWidgetSetCustomTooltipOwned': [
        'int xuiWidgetSetCustomTooltipOwned(xui_widget pWidget, const xui_custom_tooltip_desc_t* pDesc, '
        'const xui_callback_owner_t* pOwner)', 'Widget',
        ['extern "xuiWidgetSetCustomTooltipOwned" @no_raise @c_pack(1,"xui_custom_tooltip_desc_t",0,3) '
         '@c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"tPlacement",0,"value") '
         '@c_pack_callback_group(1,"pUser",2) @c_pack_field(1,"onMeasure",1,"owned_callback") '
         '@c_owned_callback(1,-1,"xui_callback_owner_t",0,"last") @c_pack_field(1,"onPaint",2,"owned_callback") '
         '@c_owned_callback(2,-1,"xui_callback_owner_t",-16,"last") @c_result("status",0) '
         'func setCustomTooltip(TooltipPlacement placement,TooltipMeasureProc measure,TooltipPaintProc paint)']],
    'xuiWidgetSetTooltipResolverOwned': [
        'int xuiWidgetSetTooltipResolverOwned(xui_widget pWidget, const xui_tooltip_resolver_desc_t* pDesc, '
        'const xui_callback_owner_t* pOwner)', 'Widget',
        ['extern "xuiWidgetSetTooltipResolverOwned" @no_raise @c_pack(1,"xui_tooltip_resolver_desc_t",0,3) '
         '@c_pack_size(1,"iSize","uint32_t") @c_pack_callback_group(1,"pUser",2) @c_pack_field(1,"onResolve",0,"owned_callback") '
         '@c_owned_callback(0,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(1,"onMeasure",1,"owned_callback") '
         '@c_owned_callback(1,-1,"xui_callback_owner_t",0,"last") @c_pack_field(1,"onPaint",2,"owned_callback") '
         '@c_owned_callback(2,-1,"xui_callback_owner_t",-16,"last") @c_result("status",0) '
         'func setTooltipResolver(TooltipResolverProc resolver,TooltipMeasureProc? measure=null,TooltipPaintProc? paint=null)']],
    'xuiWidgetGetTooltipInfo': [
        'int xuiWidgetGetTooltipInfo(xui_widget pWidget, xui_tooltip_info_t* pInfo)', 'Widget',
        ['extern "xuiWidgetGetTooltipInfo" @no_raise @c_output_record(1,"xui_tooltip_info_t","borrowed") '
         '@c_output_size(1,"iSize") @c_status(0) @c_output_field(0,"iType") '
         '@c_output_field(1,"sText","cstring") @c_output_field(2,"tPlacement") '
         'func tooltipInfo() as (c_int,string?,TooltipPlacement)']],
})

# Provider snapshots and sets close through their ownership descriptors.
CLOSE_REPLACEMENTS = {
    'xuiDestroy': ('xui.Context.close', 'xl6Xui3ContextPolicy',
        'destroy_context', 'xuiDestroyOwnerChecked'),
    'xuiDataObjectRelease': ('xui.DataObject.close', 'xl6Xui3DataObjectPolicy',
        'destroy_xl6Xui3DataObject', 'xuiDataObjectReleaseOwnerChecked'),
    'xuiDataObjectReleaseChecked': ('xui.DataObject.close', 'xl6Xui3DataObjectPolicy',
        'destroy_xl6Xui3DataObject', 'xuiDataObjectReleaseOwnerChecked'),
    'xuiTextLayoutDestroy': ('xui.TextLayout.close', 'xl6Xui3TextLayoutPolicy',
        'destroy_text_layout', 'xuiTextLayoutDestroyOwnerChecked'),
    'xuiTextLayoutDestroyChecked': ('xui.TextLayout.close', 'xl6Xui3TextLayoutPolicy',
        'destroy_text_layout', 'xuiTextLayoutDestroyOwnerChecked'),
    'xuiCodeDocumentDestroy': ('xui.CodeDocument.close', 'xl6Xui3CodeDocumentPolicy',
        'destroy_code_document', 'xuiCodeDocumentDestroyOwnerChecked'),
    'xuiCodeDocumentDestroyChecked': ('xui.CodeDocument.close', 'xl6Xui3CodeDocumentPolicy',
        'destroy_code_document', 'xuiCodeDocumentDestroyOwnerChecked'),
    'xuiCodeFindScopeDestroy': ('xui.CodeFindScope.close', 'xl6Xui3CodeFindScopePolicy',
        'destroy_find_scope', 'xuiCodeFindScopeDestroyOwnerChecked'),
    'xuiCodeFindScopeDestroyChecked': ('xui.CodeFindScope.close', 'xl6Xui3CodeFindScopePolicy',
        'destroy_find_scope', 'xuiCodeFindScopeDestroyOwnerChecked'),
    'xuiCodeLanguageRegistryDestroy': ('xui.CodeLanguageRegistry.close', 'xl6Xui3CodeLanguageRegistryPolicy',
        'destroy_code_language_registry', 'xuiCodeLanguageRegistryDestroyOwnerChecked'),
    'xuiCodeLanguageRegistryDestroyChecked': ('xui.CodeLanguageRegistry.close', 'xl6Xui3CodeLanguageRegistryPolicy',
        'destroy_code_language_registry', 'xuiCodeLanguageRegistryDestroyOwnerChecked'),
    'xuiCodeDocumentBatchDestroy': ('xui.CodeDocumentBatch.close', 'xl6Xui3CodeDocumentBatchPolicy',
        'destroy_code_document_batch', 'xuiCodeDocumentBatchDestroy'),
    'xuiCodeDocumentBatchSnapshotDestroy': ('xui.CodeDocumentBatchSnapshot.close', 'xl6Xui3CodeDocumentBatchSnapshotPolicy',
        'destroy_code_document_batch_snapshot', 'xuiCodeDocumentBatchSnapshotDestroy'),
    'xuiCodeDocumentBatchPayloadDestroy': ('xui.CodeDocumentBatchPayload.close', 'xl6Xui3CodeDocumentBatchPayloadPolicy',
        'destroy_code_document_batch_payload', 'xuiCodeDocumentBatchPayloadDestroy'),
    'xuiCodeMarginModelDestroy': ('xui.CodeMarginModel.close', 'xl6Xui3CodeMarginPolicy', 'destroy_code_margin', 'xuiCodeMarginModelDestroyOwnerChecked'),
    'xuiCodeMarginModelDestroyChecked': ('xui.CodeMarginModel.close', 'xl6Xui3CodeMarginPolicy', 'destroy_code_margin', 'xuiCodeMarginModelDestroyOwnerChecked'),
    'xuiTableViewCellOutputReleaseChecked': ('xui.TableCellOutput.close', 'xl6Xui3TableCellOutputPolicy',
        'destroy_cell', 'xuiTableViewCellOutputReleaseOwnerChecked'),
    'xuiCodeProviderSetDestroy': ('xui.CodeProviderSet.close', 'xl6Xui3CodeProviderSetPolicy',
        'destroy_code_providers', 'xuiCodeProviderSetDestroyOwnerChecked'),
    'xuiCodeProviderSetDestroyChecked': ('xui.CodeProviderSet.close', 'xl6Xui3CodeProviderSetPolicy',
        'destroy_code_providers', 'xuiCodeProviderSetDestroyOwnerChecked'),
    'xuiCodeCompletionSnapshotDestroy': ('xui.CodeCompletionSnapshot.close', 'xl6Xui3CodeCompletionSnapshotPolicy',
        'destroy_completion_snapshot', 'xuiCodeCompletionSnapshotDestroy'),
    'xuiCodeHoverSnapshotDestroy': ('xui.CodeHoverSnapshot.close', 'xl6Xui3CodeHoverSnapshotPolicy',
        'destroy_hover_snapshot', 'xuiCodeHoverSnapshotDestroy'),
    'xuiCodeSignatureSnapshotDestroy': ('xui.CodeSignatureSnapshot.close', 'xl6Xui3CodeSignatureSnapshotPolicy',
        'destroy_signature_snapshot', 'xuiCodeSignatureSnapshotDestroy'),
}

# Reviewed native entry points with managed replacements.
REPLACED = {
    'xuiTextLayoutGetContext': ('xui.TextLayout.getContext', 'xuiTextLayoutGetScopedContext'),
    'xuiCreate': ('xui.create', 'xuiCreateOwned'),
    'xuiCreateEx': ('xui.create', 'xuiCreateOwnedEx'),
    'xuiCreateLanguage': ('xui.Context.createLanguage', 'xuiCreateLanguageOwned'),
    'xuiGetProxy': ('xui.Context.proxy', 'xuiGetProxyOwned'),
    'xuiDataObjectCreate': ('xui.createDataObject', 'xuiDataObjectCreateOwned'),
    'xuiDataObjectAddRef': ('xui.DataObject.share', 'xuiDataObjectShareOwned'),
    'xuiDataObjectSetProvider': ('xui.DataObject.setProvider', 'xuiDataObjectSetProviderOwned'),
    'xuiTextLayoutGetSize': ('xui.TextLayout.getState', 'xuiTextLayoutGetState'),
    'xuiTextLayoutGetLineCount': ('xui.TextLayout.getState', 'xuiTextLayoutGetState'),
    'xuiTextLayoutGetTruncated': ('xui.TextLayout.getState', 'xuiTextLayoutGetState'),
    'xuiTextLayoutGetText': ('xui.TextLayout.getText', 'xuiTextLayoutGetTextView'),
    'xuiCodeDocumentGetText': ('xui.CodeDocument.getText', 'xuiCodeDocumentGetTextView'),
    'xuiCodeDocumentSetText': ('xui.CodeDocument.setText', 'xuiCodeDocumentSetTextLength'),
    'xuiCodeDocumentSetChangeCallback': ('xui.CodeDocument.setChangeCallback', 'xuiCodeDocumentSetChangeCallbackOwned'),
    'xuiCodeFindScopeSetActivate': ('xui.CodeFindScope.setActivate', 'xuiCodeFindScopeSetActivateOwned'),
    'xuiCodeFoldStateBuildFromProvider': ('xui.CodeFoldState.buildFromProvider', 'xuiCodeFoldStateBuildFromCallback'),
    'xuiCodeLanguageLex': ('xui.CodeLanguageRegistry.lex', 'xuiCodeLanguageRegistryLex'),
    'xuiCodeLanguageFold': ('xui.CodeLanguageRegistry.fold', 'xuiCodeLanguageRegistryFold'),
    'xuiCodeLanguageRegistryClear': ('xui.CodeLanguageRegistry.clear', 'xuiCodeLanguageRegistryClearChecked'),
    'xuiCodeDocumentBatchDescribe': ('xui.CodeDocumentBatch.describe', 'xuiCodeDocumentBatchDescribeSnapshot'),
    'xuiCodeDocumentBatchStage': ('xui.CodeDocumentBatchSnapshot.stage', 'xuiCodeDocumentBatchSnapshotStage'),
    'xuiCodeDocumentBatchAccept': ('xui.CodeDocumentBatch.accept', 'xuiCodeDocumentBatchAcceptPayload'),
    # These native callbacks have an owned registration for the same hook.
    'xuiAccordionSetSelect': ('xui.Accordion.setSelect', 'xuiAccordionSetSelectOwned'),
    'xuiBreadcrumbSetClick': ('xui.Breadcrumb.setClick', 'xuiBreadcrumbSetClickOwned'),
    'xuiBreadcrumbSetContextMenu': ('xui.Breadcrumb.setContextMenu', 'xuiBreadcrumbSetContextMenuOwned'),
    'xuiButtonSetClick': ('xui.Button.setClick', 'xuiButtonSetClickOwned'),
    'xuiCarouselSetChange': ('xui.Carousel.setChange', 'xuiCarouselSetChangeOwned'),
    'xuiCascaderSetChange': ('xui.Cascader.setChange', 'xuiCascaderSetChangeOwned'),
    'xuiMenuGetItem': ('xui.Menu.getItem', 'xuiMenuGetItemDescriptor'),
    'xuiMenuBarGetItem': ('xui.MenuBar.getItem', 'xuiMenuBarGetItemDescriptor'),
    'xuiToolbarGetItem': ('xui.Toolbar.getItem', 'xuiToolbarGetItemDescriptor'),
    'xuiStatusBarGetItem': ('xui.StatusBar.getItem', 'xuiStatusBarGetItemDescriptor'),
    'xuiCascaderGetItem': ('xui.Cascader.getItem', 'xuiCascaderGetItemDescriptor'),
    'xuiComboBoxGetItem': ('xui.ComboBox.getItem', 'xuiComboBoxGetItemDescriptor'),
    'xuiCodeLayoutBuildVisibleLines': ('xui.captureCodeLayout', 'xuiCodeLayoutCapture'),
    'xuiCheckBoxSetChange': ('xui.CheckBox.setChange', 'xuiCheckBoxSetChangeOwned'),
    'xuiCheckCardSetChange': ('xui.CheckCard.setChange', 'xuiCheckCardSetChangeOwned'),
    'xuiColorPickerSetChange': ('xui.ColorPicker.setChange', 'xuiColorPickerSetChangeOwned'),
    'xuiComboBoxSetSelect': ('xui.ComboBox.setSelect', 'xuiComboBoxSetSelectOwned'),
    'xuiComboBoxSetTextChange': ('xui.ComboBox.setTextChange', 'xuiComboBoxSetTextChangeOwned'),
    'xuiDockPanelSetActiveChanged': ('xui.DockPanel.setActiveChanged', 'xuiDockPanelSetActiveChangedOwned'),
    'xuiDockPanelSetContextMenu': ('xui.DockPanel.setContextMenu', 'xuiDockPanelSetContextMenuOwned'),
    'xuiDockPanelSetWindowClose': ('xui.DockPanel.setWindowClose', 'xuiDockPanelSetWindowCloseOwned'),
    'xuiDockPanelSetWindowStateChanged': ('xui.DockPanel.setWindowStateChanged', 'xuiDockPanelSetWindowStateChangedOwned'),
    'xuiFileDialogSetContextMenu': ('xui.FileDialog.setContextMenu', 'xuiFileDialogSetContextMenuOwned'),
    'xuiFileDialogSetResult': ('xui.FileDialog.setResult', 'xuiFileDialogSetResultOwned'),
    'xuiFlowGraphWidgetSetContextMenu': ('xui.FlowGraphWidget.setContextMenu', 'xuiFlowGraphWidgetSetContextMenuOwned'),
    'xuiHotKeyRegister': ('xui.Context.hotKeyRegister', 'xuiHotKeyRegisterOwned'),
    'xuiHyperlinkSetClick': ('xui.Hyperlink.setClick', 'xuiHyperlinkSetClickOwned'),
    'xuiInputSetChange': ('xui.Input.setChange', 'xuiInputSetChangeOwned'),
    'xuiInventoryGridSetActivateCallback': ('xui.InventoryGrid.setActivateCallback', 'xuiInventoryGridSetActivateCallbackOwned'),
    'xuiInventoryGridSetContextCallback': ('xui.InventoryGrid.setContextCallback', 'xuiInventoryGridSetContextCallbackOwned'),
    'xuiInventoryGridSetSelectCallback': ('xui.InventoryGrid.setSelectCallback', 'xuiInventoryGridSetSelectCallbackOwned'),
    'xuiInventoryGridSetSplitCallback': ('xui.InventoryGrid.setSplitCallback', 'xuiInventoryGridSetSplitCallbackOwned'),
    'xuiListViewSetContextMenu': ('xui.ListView.setContextMenu', 'xuiListViewSetContextMenuOwned'),
    'xuiListViewSetSelect': ('xui.ListView.setSelect', 'xuiListViewSetSelectOwned'),
    'xuiMenuBarSetSelect': ('xui.MenuBar.setSelect', 'xuiMenuBarSetSelectOwned'),
    'xuiMenuSetSelect': ('xui.Menu.setSelect', 'xuiMenuSetSelectOwned'),
    'xuiMessageListSetEvent': ('xui.MessageList.setEvent', 'xuiMessageListSetEventOwned'),
    'xuiMessageListSetNodeRenderer': ('xui.MessageList.setNodeRenderer', 'xuiMessageListSetNodeRendererOwned'),
    'xuiMsgBoxSetResult': ('xui.MsgBox.setResult', 'xuiMsgBoxSetResultOwned'),
    'xuiMsgTipSetClose': ('xui.MsgTip.setDismissed', 'xuiMsgTipSetCloseOwned'),
    'xuiNumericInputSetChange': ('xui.NumericInput.setChange', 'xuiNumericInputSetChangeOwned'),
    'xuiNumericInputSetErrorChange': ('xui.NumericInput.setErrorChange', 'xuiNumericInputSetErrorChangeOwned'),
    'xuiNumericInputSetFormatter': ('xui.NumericInput.setFormatter', 'xuiNumericInputSetFormatterOwned'),
    'xuiPageSetChange': ('xui.Page.setChange', 'xuiPageSetChangeOwned'),
    'xuiPopupSetChange': ('xui.Popup.setChange', 'xuiPopupSetChangeOwned'),
    'xuiPropertyGridSetChange': ('xui.PropertyGrid.setChange', 'xuiPropertyGridSetChangeOwned'),
    'xuiPropertyGridSetContextMenu': ('xui.PropertyGrid.setContextMenu', 'xuiPropertyGridSetContextMenuOwned'),
    'xuiPropertyGridSetSelect': ('xui.PropertyGrid.setSelect', 'xuiPropertyGridSetSelectOwned'),
    'xuiRadioGroupSetChange': ('xui.RadioGroup.setChange', 'xuiRadioGroupSetChangeOwned'),
    'xuiRadioSetChange': ('xui.Radio.setChange', 'xuiRadioSetChangeOwned'),
    'xuiRangeSliderSetChange': ('xui.RangeSlider.setChange', 'xuiRangeSliderSetChangeOwned'),
    'xuiScrollBarSetChange': ('xui.ScrollBar.setChange', 'xuiScrollBarSetChangeOwned'),
    'xuiScrollFrameSetChange': ('xui.ScrollFrame.setChange', 'xuiScrollFrameSetChangeOwned'),
    'xuiScrollViewSetChange': ('xui.ScrollView.setChange', 'xuiScrollViewSetChangeOwned'),
    'xuiSetAccessibilityEventCallback': ('xui.Context.setAccessibilityEventCallback', 'xuiSetAccessibilityEventCallbackOwned'),
    'xuiSetErrorCallback': ('xui.Context.setErrorCallback', 'xuiSetErrorCallbackOwned'),
    'xuiSliderSetChange': ('xui.Slider.setChange', 'xuiSliderSetChangeOwned'),
    'xuiSplitLayoutSetChange': ('xui.SplitLayout.setChange', 'xuiSplitLayoutSetChangeOwned'),
    'xuiStatusBarSetContextMenu': ('xui.StatusBar.setContextMenu', 'xuiStatusBarSetContextMenuOwned'),
    'xuiStatusBarSetSelect': ('xui.StatusBar.setSelect', 'xuiStatusBarSetSelectOwned'),
    'xuiTabsSetContextMenu': ('xui.Tabs.setContextMenu', 'xuiTabsSetContextMenuOwned'),
    'xuiTabsSetSelect': ('xui.Tabs.setSelect', 'xuiTabsSetSelectOwned'),
    'xuiTagInputSetChange': ('xui.TagInput.setChange', 'xuiTagInputSetChangeOwned'),
    'xuiTagInputSetContextMenu': ('xui.TagInput.setContextMenu', 'xuiTagInputSetContextMenuOwned'),
    'xuiTextEditSetChange': ('xui.TextEdit.setChange', 'xuiTextEditSetChangeOwned'),
    'xuiToastSetClose': ('xui.Toast.setDismissed', 'xuiToastSetCloseOwned'),
    'xuiToastShow': ('xui.Toast.show', 'xuiToastShowOwned'),
    'xuiToggleSetChange': ('xui.Toggle.setChange', 'xuiToggleSetChangeOwned'),
    'xuiToolbarSetContextMenu': ('xui.Toolbar.setContextMenu', 'xuiToolbarSetContextMenuOwned'),
    'xuiToolbarSetOverflow': ('xui.Toolbar.setOverflow', 'xuiToolbarSetOverflowOwned'),
    'xuiToolbarSetSelect': ('xui.Toolbar.setSelect', 'xuiToolbarSetSelectOwned'),
    'xuiTreeViewSetContextMenu': ('xui.TreeView.setContextMenu', 'xuiTreeViewSetContextMenuOwned'),
    'xuiTreeViewSetSelect': ('xui.TreeView.setSelect', 'xuiTreeViewSetSelectOwned'),
    'xuiVirtualJoystickSetChange': ('xui.VirtualJoystick.setChange', 'xuiVirtualJoystickSetChangeOwned'),
    'xuiWebViewSetEvent': ('xui.WebView.setEvent', 'xuiWebViewSetEventOwned'),
    'xuiWindowSetClose': ('xui.Window.setClose', 'xuiWindowSetCloseOwned'),
    'xuiWindowSetContextMenu': ('xui.Window.setContextMenu', 'xuiWindowSetContextMenuOwned'),
    'xuiWorkflowWidgetSetContextMenu': ('xui.WorkflowWidget.setContextMenu', 'xuiWorkflowWidgetSetContextMenuOwned'),
    'xuiIconAddCustom': ('xui.IconCategory.addCustom', 'xuiIconAddCustomOwned'),
    'xuiIconAddSurface': ('xui.IconCategory.addSurface', 'xuiIconAddSurfaceShared'),
    'xuiIconAddRef': ('xui.Icon.retain', 'xuiIconRetainReference'),
    'xuiIconCategoryAddRef': ('xui.IconCategory.retain', 'xuiIconCategoryRetainReference'),
    'xuiIconPickerSetChange': ('xui.IconPicker.setChange', 'xuiIconPickerSetChangeOwned'),
    'xuiIconPickerSetFormatter': ('xui.IconPicker.setFormatter', 'xuiIconPickerSetFormatterOwned'),
    'xuiWidgetDestroy': ('xui.Widget.close', 'xuiWidgetDestroyChecked'),
    'xuiWidgetGetAccessibleNodeCount': ('xui.Widget.getAccessibleNodeCount', 'xuiWidgetGetAccessibleNodeCountChecked'),
    'xuiWidgetSetCacheRenderCallback': ('xui.Widget.setCacheRenderCallback', 'xuiWidgetSetCacheRenderCallbackOwned'),
    'xuiWidgetSetContentMeasureCallback': ('xui.Widget.setContentMeasureCallback', 'xuiWidgetSetContentMeasureCallbackOwned'),
    'xuiWidgetSetCursorQueryCallback': ('xui.Widget.setCursorQueryCallback', 'xuiWidgetSetCursorQueryCallbackOwned'),
    'xuiWidgetSetDefaultAction': ('xui.Widget.setDefaultAction', 'xuiWidgetSetDefaultActionOwned'),
    'xuiWidgetSetCancelAction': ('xui.Widget.setCancelAction', 'xuiWidgetSetCancelActionOwned'),
    'xuiWidgetSetEventCallback': ('xui.Widget.setEventCallback', 'xuiWidgetSetEventCallbackOwned'),
    'xuiWidgetSetEventHandler': ('xui.Widget.setEventHandler', 'xuiWidgetSetEventHandlerOwned'),
    'xuiWidgetSetImeCandidateRect': ('xui.Widget.setImeCandidateRect', 'xuiWidgetSetImeCandidateRectOwned'),
    'xuiWidgetSetLayoutChildrenCallback': ('xui.Widget.setLayoutChildrenCallback', 'xuiWidgetSetLayoutChildrenCallbackOwned'),
    'xuiWidgetSetLayoutCompleteCallback': ('xui.Widget.setLayoutCompleteCallback', 'xuiWidgetSetLayoutCompleteCallbackOwned'),
    'xuiWidgetSetLayoutPrepareCallback': ('xui.Widget.setLayoutPrepareCallback', 'xuiWidgetSetLayoutPrepareCallbackOwned'),
    'xuiWidgetSetUpdateCallback': ('xui.Widget.setUpdateCallback', 'xuiWidgetSetUpdateCallbackOwned'),
    'xuiWidgetSetTooltipResolver': ('xui.Widget.setTooltipResolver', 'xuiWidgetSetTooltipResolverOwned'),
    'xuiWidgetSetAccessibilityProvider': ('xui.Widget.setAccessibilityProvider', 'xuiWidgetSetAccessibilityProviderOwned'),
    'xuiTerminalCreateFakeSession': ('xui.createFakeTerminalSession', 'xuiTerminalCreateFakeSessionOwned'),
    'xuiTerminalCreateProcessSession': ('xui.createProcessTerminalSession', 'xuiTerminalCreateProcessSessionOwned'),
    'xuiTerminalCreateSshSession': ('xui.createSshTerminalSession', 'xuiTerminalCreateSshSessionOwned'),
    'xuiTerminalSessionSetResizeCallback': ('xui.TerminalSession.setResizeCallback', 'xuiTerminalSessionSetResizeCallbackOwned'),
    'xuiTerminalSetInputCallback': ('xui.Terminal.setInputCallback', 'xuiTerminalSetInputCallbackOwned'),
    'xuiTerminalSetResizeCallback': ('xui.Terminal.setResizeCallback', 'xuiTerminalSetResizeCallbackOwned'),
    'xuiTerminalSetTitleCallback': ('xui.Terminal.setTitleCallback', 'xuiTerminalSetTitleCallbackOwned'),
    'xuiTerminalSetLinkCallback': ('xui.Terminal.setLinkCallback', 'xuiTerminalSetLinkCallbackOwned'),
    'xuiTableViewCellOutputAddRef': ('xui.TableCellOutput.share','xuiTableViewCellOutputShareOwned'),
    'xuiTableViewSetHeaderRenderer': ('xui.TableView.setHeaderRenderer','xuiTableViewSetHeaderRendererOwned'),
    'xuiTableViewSetCellRenderer': ('xui.TableView.setCellRenderer','xuiTableViewSetCellRendererOwned'),
    'xuiTableViewSetColumnFormatter': ('xui.TableView.setColumnFormatter','xuiTableViewSetColumnFormatterOwned'),
    'xuiTableViewGetColumn': ('xui.TableView.getColumn','xuiTableViewGetColumnDescriptor'),
    'xuiTableViewSetAdapter': ('xui.TableView.setAdapter','xuiTableViewSetAdapterOwned'),
    'xuiTableViewSetColumnResize': ('xui.TableView.setColumnResize','xuiTableViewSetColumnResizeOwned'),
    'xuiTableViewSetContextMenu': ('xui.TableView.setContextMenu','xuiTableViewSetContextMenuOwned'),
    'xuiTableViewSetHover': ('xui.TableView.setHover','xuiTableViewSetHoverOwned'),
    'xuiTableViewSetMergeProvider': ('xui.TableView.setMergeProvider','xuiTableViewSetMergeProviderOwned'),
    'xuiTableViewSetSelect': ('xui.TableView.setSelect','xuiTableViewSetSelectOwned'),
    'xuiTableViewSetSort': ('xui.TableView.setSort','xuiTableViewSetSortOwned'),
    'xuiTableGridSetAdapter': ('xui.TableGrid.setAdapter','xuiTableGridSetAdapterOwned'),
    'xuiTableGridSetChange': ('xui.TableGrid.setChange','xuiTableGridSetChangeOwned'),
    'xuiTableGridSetContextMenu': ('xui.TableGrid.setContextMenu','xuiTableGridSetContextMenuOwned'),
    'xuiTableGridSetEditor': ('xui.TableGrid.setEditor','xuiTableGridSetEditorOwned'),
    'xuiTableGridSetEditorConfig': ('xui.TableGrid.setEditorConfig','xuiTableGridSetEditorConfigOwned'),
    'xuiTableGridSetValidate': ('xui.TableGrid.setValidate','xuiTableGridSetValidateOwned'),
    'xuiCodeProviderSetClear': ('xui.CodeProviderSet.clear', 'xuiCodeProviderSetClearChecked'),
    'xuiCodeProviderSetCompletion': ('xui.CodeProviderSet.setCompletionProvider', 'xuiCodeProviderSetCompletionReplyOwned'),
    'xuiCodeProviderSetCompletionOwned': ('xui.CodeProviderSet.setCompletionProvider', 'xuiCodeProviderSetCompletionReplyOwned'),
    'xuiCodeProviderSetHover': ('xui.CodeProviderSet.setHoverProvider', 'xuiCodeProviderSetHoverReplyOwned'),
    'xuiCodeProviderSetHoverOwned': ('xui.CodeProviderSet.setHoverProvider', 'xuiCodeProviderSetHoverReplyOwned'),
    'xuiCodeProviderSetSignature': ('xui.CodeProviderSet.setSignatureProvider', 'xuiCodeProviderSetSignatureReplyOwned'),
    'xuiCodeProviderSetSignatureOwned': ('xui.CodeProviderSet.setSignatureProvider', 'xuiCodeProviderSetSignatureReplyOwned'),
    'xuiCodeProviderSetCommandEnabled': ('xui.CodeProviderSet.setCommandEnabledProvider', 'xuiCodeProviderSetCommandEnabledOwned'),
    'xuiCodeProviderRequestCompletion': ('xui.CodeProviderSet.requestCompletion', 'xuiCodeProviderCaptureCompletion'),
    'xuiCodeProviderRequestSignature': ('xui.CodeProviderSet.requestSignature', 'xuiCodeProviderCaptureSignature'),
    'xuiCodeEditHitTestDiagnostic': ('xui.CodeEdit.hitTestDiagnostic', 'xuiCodeEditFindDiagnostic'),
    'xuiCodeEditGetFindScope': ('xui.Context.getCodeFindScope', 'xuiContextGetCodeFindScope'),
    'xuiEditGetText': ('xui.Widget.editGetText','xuiEditGetTextView'),
    'xuiCodeEditGetText': ('xui.CodeEdit.getText','xuiCodeEditGetTextView'),
    'xuiCodeEditSetDiagnosticHover': ('xui.CodeEdit.setDiagnosticHover','xuiCodeEditSetDiagnosticHoverOwned'),
    'xuiCodeEditSetTokenStyleResolver': ('xui.CodeEdit.setTokenStyleResolver','xuiCodeEditSetTokenStyleResolverOwned'),
    'xuiCodeEditSetInputHandler': ('xui.CodeEdit.setInputHandler','xuiCodeEditSetInputHandlerOwned'),
    'xuiTreeViewSetItemRenderer': ('xui.TreeView.setItemRenderer','xuiTreeViewSetItemRendererOwned'),
    'xuiTreeViewSetIconRenderer': ('xui.TreeView.setIconRenderer','xuiTreeViewSetIconRendererOwned'),
    'xuiListViewSetActivate': ('xui.ListView.setActivate','xuiListViewSetActivateOwned'),
    'xuiListViewSetItemRenderer': ('xui.ListView.setItemRenderer','xuiListViewSetItemRendererOwned'),
    'xuiListViewSetVirtualItems': ('xui.ListView.setVirtualItems','xuiListViewSetVirtualItemsOwned'),
    'xuiChartSetTooltipCallback': ('xui.Chart.setTooltipProvider','xuiChartSetTooltipCallbackOwned'),
    'xuiChartSetContextMenu': ('xui.Chart.setContextMenu','xuiChartSetContextMenuOwned'),
    'xuiCodeAnnotationGetMarkers': ('xui.CodeAnnotationStore.captureMarkers','xuiCodeAnnotationCaptureMarkers'),
    'xuiCodeAnnotationGetIndicatorsAt': ('xui.CodeAnnotationStore.captureIndicatorsAt','xuiCodeAnnotationCaptureIndicatorsAt'),
    'xuiCodeAnnotationGetDiagnosticsAt': ('xui.CodeAnnotationStore.captureDiagnosticsAt','xuiCodeAnnotationCaptureDiagnosticsAt'),
    'xuiCodeAnnotationGetDiagnosticsInRange': ('xui.CodeAnnotationStore.captureDiagnosticsInRange','xuiCodeAnnotationCaptureDiagnosticsInRange'),
    'xuiCodeEditGetTextOffsetRect': ('xui.CodeEdit.getOffsetRect','xuiCodeEditGetOffsetRect'),
    'xuiDatePickerSetChanging': ('xui.DatePicker.setChanging','xuiDatePickerSetChangingOwned'),
    'xuiDatePickerSetChange': ('xui.DatePicker.setChange','xuiDatePickerSetChangeOwned'),
    'xuiDatePickerSetCommit': ('xui.DatePicker.setCommit','xuiDatePickerSetCommitOwned'),
    'xuiDatePickerSetCancel': ('xui.DatePicker.setCancel','xuiDatePickerSetCancelOwned'),
    'xuiDatePickerSetClear': ('xui.DatePicker.setClear','xuiDatePickerSetClearOwned'),
    'xuiFlowGraphGetRevision': ('xui.FlowGraph.revision','xuiFlowGraphGetRevisionChecked'),
    'xuiFlowGraphGetCleanRevision': ('xui.FlowGraph.cleanRevision','xuiFlowGraphGetCleanRevisionChecked'),
    'xuiInputDecorationAdd': ('xui.Input.decorationAdd','xuiInputDecorationAddOwned'),
    'xuiInputDecorationSet': ('xui.Input.decorationSet','xuiInputDecorationSetOwned'),
    'xuiInputDecorationGetRect': ('xui.InputDecoration.getBounds','xuiInputDecorationGetBounds'),
}


METHODS.update({'xuiInventoryGridSetSlotContent': ['int xuiInventoryGridSetSlotContent(xui_widget pWidget, int iSlot, const xui_inventory_slot_content_t* pContent)',
                                    'InventoryGrid',
                                    ['extern "xuiInventoryGridSetSlotContent" @no_raise @c_pack(2,"xui_inventory_slot_content_t",1,3) '
                                     '@c_pack_field(2,"tSettings",1,"value") @c_pack_field(2,"sText",2,"c_string") @c_pack_field(2,"sHotkey",3,"c_string") '
                                     '@c_result("status",0) func setSlotContent(c_int index,InventorySlotSettings settings,string text="",string hotkey="")']],
 'xuiInventoryGridGetSlotContent': ['int xuiInventoryGridGetSlotContent(xui_widget pWidget, int iSlot, xui_inventory_slot_content_t* pContent)',
                                    'InventoryGrid',
                                    ['extern "xuiInventoryGridGetSlotContent" @no_raise @c_output_record(2,"xui_inventory_slot_content_t","borrowed") '
                                     '@c_status(0) @c_output_field(0,"tSettings") @c_output_field(1,"sText","cstring") @c_output_field(2,"sHotkey","cstring") '
                                     'func getSlotContent(c_int index) as (InventorySlotSettings,string,string)']],
 'xuiInventoryGridQuerySlotsOwned': ['int xuiInventoryGridQuerySlotsOwned(xui_widget pWidget, const xui_inventory_slot_query_t* pQuery, xui_inventory_indices* '
                                     'pOut)',
                                     'InventoryGrid',
                                     ['extern "xuiInventoryGridQuerySlotsOwned" @no_raise @c_result("owned_view") @factory(2,0,releaseInventoryIndices) '
                                      '@c_sync_callback(5,0,0,"last","xge3_runtime.h") @c_sync_callback(6,0,0,"last","xge3_runtime.h") '
                                      '@c_pack(1,"xui_inventory_slot_query_t",0,7) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"iFlags",0,"value") '
                                      '@c_pack_field(1,"iRequiredFlags",1,"value") @c_pack_field(1,"iRejectedFlags",2,"value") '
                                      '@c_pack_field(1,"iSortMode",3,"value") @c_pack_field(1,"sTextContains",4,"c_string") '
                                      '@c_pack_field(1,"onFilter",5,"callback") @c_pack_field(1,"pFilterUser",5,"callback_user") '
                                      '@c_pack_field(1,"onCompare",6,"callback") @c_pack_field(1,"pCompareUser",6,"callback_user") func querySlots(c_uint '
                                      'flags=0,c_uint requiredFlags=0,c_uint rejectedFlags=0,c_int sortMode=0,string? textContains=null,InventoryFilterProc? '
                                      'filter=null,InventoryCompareProc? compare=null) as InventoryIndices']],
 'xuiInventoryGridSortSlotsOwned': ['int xuiInventoryGridSortSlotsOwned(xui_widget pWidget, const int* arrSlots, int iSlotCount, int iSortMode, uint32_t '
                                    'iFlags, xui_inventory_compare_proc onCompare, void* pUser, xui_inventory_indices* pOut)',
                                    'InventoryGrid',
                                    ['extern "xuiInventoryGridSortSlotsOwned" @no_raise @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const int*","value") '
                                     '@c_sync_callback(4,0,0,"last","xge3_runtime.h") @c_result("owned_view") @factory(7,0,releaseInventoryIndices) func '
                                     'sortSlots(array<c_int> slots,int size=-1,c_int sortMode=0,c_uint flags=0,InventoryCompareProc? compare=null) as '
                                     'InventoryIndices']],
 'xuiInventoryIndicesAddRef': ['int xuiInventoryIndicesAddRef(xui_inventory_indices pIndices)',
                               None,
                               ['extern "xuiInventoryIndicesAddRef" @header("xge3_runtime.h") @no_raise func retainInventoryIndices(InventoryIndices value) as '
                                'c_int']],
 'xuiInventoryIndicesRelease': ['void xuiInventoryIndicesRelease(xui_inventory_indices pIndices)',
                                None,
                                ['extern "xuiInventoryIndicesRelease" @header("xge3_runtime.h") @no_raise func releaseInventoryIndices(InventoryIndices '
                                 'value)']]})
CALLBACKS.update({'xui_inventory_filter_proc': ('typedef int (*xui_inventory_filter_proc)(xui_widget_t* pWidget, int iSlot, const xui_inventory_slot_t* pSlot, void* pUser);',
                               'export typedef InventoryFilterProc = func(CallbackWidget,c_int,CallbackInventorySlot) as c_int'),
 'xui_inventory_compare_proc': ('typedef int (*xui_inventory_compare_proc)(xui_widget_t* pWidget, int iSlotA, const xui_inventory_slot_t* pSlotA, int iSlotB, '
                                'const xui_inventory_slot_t* pSlotB, void* pUser);',
                                'export typedef InventoryCompareProc = func(CallbackWidget,c_int,CallbackInventorySlot,c_int,CallbackInventorySlot) as c_int')})
VIEWS.update({
    'CallbackInventorySlot': """export extern "const xui_inventory_slot_t*" class CallbackInventorySlot @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iSlotId" @c_field get slotId() as c_int
 extern "iItemId" @c_field get itemId() as c_int
 extern "iCount" @c_field get count() as c_int
 extern "iMaxCount" @c_field get maxCount() as c_int
 extern "iSlotType" @c_field get slotType() as c_int
 extern "iItemType" @c_field get itemType() as c_int
 extern "iFlags" @c_field get flags() as c_uint
 extern "iQualityColor" @c_field get qualityColor() as c_uint
 extern "fCooldownRate" @c_field get cooldownRate() as c_float
 extern "fDurabilityRate" @c_field get durabilityRate() as c_float
 extern "xl6Xui3InventorySlotText" @no_raise @c_result("cstring") func text() as string
 extern "xl6Xui3InventorySlotHotkey" @no_raise @c_result("cstring") func hotkey() as string
end""",
    'InventoryIndices': """export extern "xui_inventory_indices" class InventoryIndices @header("xge3_runtime.h") @lifetime("owned") @policy("xl6Xui3InventoryIndicesPolicy")
 native point handle
 extern "xuiInventoryIndicesGet" @no_raise @c_owned_sequence(1,2,0,"size_t",0) @c_retain(retainInventoryIndices,"zero",releaseInventoryIndices) func indices() as NativeSequence<c_int>
 extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()
end""",
})
VIEW_FUNCTIONS["xuiInventoryIndicesGet"] = 'int xuiInventoryIndicesGet(xui_inventory_indices pIndices, const int** ppSlots, size_t* pCount)'
RECORDS.update({
    'xui_inventory_slot_settings_t': 'typedef struct xui_inventory_slot_settings_t { int iSlotId; int iItemId; int iCount; int iMaxCount; int iSlotType; int iItemType; uint32_t iFlags; uint32_t iQualityColor; float fCooldownRate; float fDurabilityRate; } xui_inventory_slot_settings_t;',
    'xui_inventory_slot_content_t': 'typedef struct xui_inventory_slot_content_t { xui_inventory_slot_settings_t tSettings; const char* sText; const char* sHotkey; } xui_inventory_slot_content_t;',
    'xui_inventory_indices': 'typedef struct xui_inventory_indices_t* xui_inventory_indices;',
})
REPLACED.update({
    "xuiInventoryGridQuerySlots": ("xui.InventoryGrid.querySlots", "xuiInventoryGridQuerySlotsOwned"),
    "xuiInventoryGridSortSlots": ("xui.InventoryGrid.sortSlots", "xuiInventoryGridSortSlotsOwned"),
})

METHODS.update({
    'xuiInventoryGridSetSlotIcon': (
        'int xuiInventoryGridSetSlotIcon(xui_widget pWidget, int iSlot, xui_surface pIcon, xui_rect_t tSource, uint32_t iTint)',
        'InventoryGrid', ['extern "xuiInventoryGridSetSlotIcon" @no_raise @c_result("status",0) func setSlotIcon(c_int index,Surface? icon,Rect source,c_uint tint=0)']),
    'xuiInventoryGridGetSlotIcon': (
        'int xuiInventoryGridGetSlotIcon(xui_widget pWidget, int iSlot, xui_surface* pIcon, xui_rect_t* pSource, uint32_t* pTint)',
        'InventoryGrid', ['extern "xuiInventoryGridGetSlotIcon" @no_raise @c_output_retain(2,retainSurface,"zero",rollbackSurface) @c_outputs(2,3,4) @c_status(0) func getSlotIcon(c_int index) as (Surface?,Rect,c_uint)']),
    'xuiInventoryGridClearAll': (
        'int xuiInventoryGridClearAll(xui_widget pWidget)',
        'InventoryGrid', ['extern "xuiInventoryGridClearAll" @no_raise @c_result("status",0) func clearAll()']),
    'xuiInventoryGridGetScrollSnapshot': (
        'int xuiInventoryGridGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'InventoryGrid', ['extern "xuiInventoryGridGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiCodeEditGetScrollSnapshot': (
        'int xuiCodeEditGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'CodeEdit', ['extern "xuiCodeEditGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiTextEditGetScrollSnapshot': (
        'int xuiTextEditGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'TextEdit', ['extern "xuiTextEditGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiTerminalGetScrollSnapshot': (
        'int xuiTerminalGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'Terminal', ['extern "xuiTerminalGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiTerminalSetScrollOffset': (
        'int xuiTerminalSetScrollOffset(xui_widget pWidget, float fOffsetY)',
        'Terminal', ['extern "xuiTerminalSetScrollOffset" @no_raise @c_result("status",0) func setScrollOffset(c_float y)']),
    'xuiTerminalScrollBy': (
        'int xuiTerminalScrollBy(xui_widget pWidget, float fDeltaY)',
        'Terminal', ['extern "xuiTerminalScrollBy" @no_raise @c_result("status",0) func scrollBy(c_float dy)']),
    'xuiTerminalAttachSession': (
        'int xuiTerminalAttachSession(xui_widget pWidget, xui_terminal_session_t* pSession)',
        'Terminal', ['extern "xuiTerminalAttachSession" @no_raise @c_result("status",0) func attachSession(TerminalSession session)']),
    'xuiWidgetGetAccessibleNodeCountChecked': (
        'int xuiWidgetGetAccessibleNodeCountChecked(xui_widget pWidget)',
        'Widget', ['extern "xuiWidgetGetAccessibleNodeCountChecked" @no_raise @c_result("integer_min",0) func getAccessibleNodeCount() as c_int']),
    'xuiWidgetSetAccessibilityProviderOwned': (
        'int xuiWidgetSetAccessibilityProviderOwned(xui_widget pWidget, const xui_accessible_provider_desc_t* pDesc, const xui_callback_owner_t* pOwner)',
        'Widget', ['extern "xuiWidgetSetAccessibilityProviderOwned" @no_raise @c_pack(1,"xui_accessible_provider_desc_t",0,3) @c_pack_size(1,"iSize","uint32_t") @c_pack_callback_group(1,"pUser",2,"optional") @c_pack_field(1,"onCount",0,"owned_callback") @c_owned_callback(0,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(1,"onNode",1,"owned_callback") @c_owned_callback(1,-1,"xui_callback_owner_t",-16,"last") @c_pack_field(1,"onAction",2,"owned_callback") @c_owned_callback(2,-1,"xui_callback_owner_t",-16,"last") @c_result("status",0) func setAccessibilityProvider(AccessibleCountProc? count=null,AccessibleNodeProc? node=null,AccessibleActionProc? action=null)']),
    'xuiWidgetPerformAccessibleAction': (
        'int xuiWidgetPerformAccessibleAction(xui_widget pWidget, uint64_t iNodeId, int iAction, const void* pData)',
        'Widget', ['extern "xuiWidgetPerformAccessibleAction" @no_raise @c_null(3,"const void*") @c_result("status",0) func performAccessibleAction(uint64 nodeId,c_int action)']),
    'xuiWidgetPerformAccessibleSelection': (
        'int xuiWidgetPerformAccessibleSelection(xui_widget pWidget, uint64_t iNodeId, int iAnchor, int iCaret)',
        'Widget', ['extern "xuiWidgetPerformAccessibleSelection" @no_raise @c_result("status",0) func performAccessibleSelection(uint64 nodeId,c_int anchor,c_int caret)']),
    'xuiWidgetSetAccessibleTextValue': (
        'int xuiWidgetSetAccessibleTextValue(xui_widget pWidget, uint64_t iNodeId, const char* sText)',
        'Widget', ['extern "xuiWidgetSetAccessibleTextValue" @no_raise @c_cstr(1) @c_result("status",0) func setAccessibleValue(uint64 nodeId,string value)']),
    'xuiWidgetSetAccessibleNumberValue': (
        'int xuiWidgetSetAccessibleNumberValue(xui_widget pWidget, uint64_t iNodeId, double fNumber)',
        'Widget', ['extern "xuiWidgetSetAccessibleNumberValue" @no_raise @c_result("status",0) func setAccessibleValue(uint64 nodeId,c_double value)']),
    'xuiScrollFrameGetScrollSnapshot': (
        'int xuiScrollFrameGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'ScrollFrame', ['extern "xuiScrollFrameGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiScrollViewGetScrollSnapshot': (
        'int xuiScrollViewGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'ScrollView', ['extern "xuiScrollViewGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiCanvasGetScrollSnapshot': (
        'int xuiCanvasGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'Canvas', ['extern "xuiCanvasGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiListViewGetScrollSnapshot': (
        'int xuiListViewGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'ListView', ['extern "xuiListViewGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiTreeViewGetScrollSnapshot': (
        'int xuiTreeViewGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'TreeView', ['extern "xuiTreeViewGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiTableViewGetScrollSnapshot': (
        'int xuiTableViewGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'TableView', ['extern "xuiTableViewGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiPopupGetScrollSnapshot': (
        'int xuiPopupGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'Popup', ['extern "xuiPopupGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiScrollModelWithViewport': (
        'int xuiScrollModelWithViewport(const xui_scroll_model_t* pModel, xui_rect_t tViewport, xui_scroll_model_t* pResult)',
        None, ['export extern "xuiScrollModelWithViewport" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_output(2,0) func scrollModelWithViewport(ScrollModel model,Rect viewport) as ScrollModel']),
    'xuiScrollModelWithContentSize': (
        'int xuiScrollModelWithContentSize(const xui_scroll_model_t* pModel, float fWidth, float fHeight, xui_scroll_model_t* pResult)',
        None, ['export extern "xuiScrollModelWithContentSize" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_output(3,0) func scrollModelWithContentSize(ScrollModel model,c_float width,c_float height) as ScrollModel']),
    'xuiScrollModelWithOffset': (
        'int xuiScrollModelWithOffset(const xui_scroll_model_t* pModel, float fOffsetX, float fOffsetY, xui_scroll_model_t* pResult)',
        None, ['export extern "xuiScrollModelWithOffset" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_output(3,0) func scrollModelWithOffset(ScrollModel model,c_float x,c_float y) as ScrollModel']),
    'xuiScrollModelScrolledBy': (
        'int xuiScrollModelScrolledBy(const xui_scroll_model_t* pModel, float fDeltaX, float fDeltaY, xui_scroll_model_t* pResult)',
        None, ['export extern "xuiScrollModelScrolledBy" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_output(3,0) func scrollModelScrolledBy(ScrollModel model,c_float dx,c_float dy) as ScrollModel']),
    'xuiScrollModelWithRectVisible': (
        'int xuiScrollModelWithRectVisible(const xui_scroll_model_t* pModel, xui_rect_t tContentRect, xui_scroll_model_t* pResult)',
        None, ['export extern "xuiScrollModelWithRectVisible" @header("xge3_runtime.h") @no_raise @c_ref(0) @c_output(2,0) func scrollModelWithRectVisible(ScrollModel model,Rect contentRect) as ScrollModel']),
    'xuiInventoryGridSetScrollOffset': (
        'int xuiInventoryGridSetScrollOffset(xui_widget pWidget, float fX, float fY)',
        'InventoryGrid', ['extern "xuiInventoryGridSetScrollOffset" @no_raise @c_result("status",0) func setScrollOffset(c_float x,c_float y)']),
    'xuiInventoryGridScrollBy': (
        'int xuiInventoryGridScrollBy(xui_widget pWidget, float fDeltaX, float fDeltaY)',
        'InventoryGrid', ['extern "xuiInventoryGridScrollBy" @no_raise @c_result("status",0) func scrollBy(c_float dx,c_float dy)']),
    'xuiInventoryGridEnsureContentRectVisible': (
        'int xuiInventoryGridEnsureContentRectVisible(xui_widget pWidget, xui_rect_t tContentRect)',
        'InventoryGrid', ['extern "xuiInventoryGridEnsureContentRectVisible" @no_raise @c_result("status",0) func ensureContentRectVisible(Rect contentRect)']),
    'xuiInventoryGridSetDragCallbackOwned': (
        'int xuiInventoryGridSetDragCallbackOwned(xui_widget pWidget, xui_inventory_drag_proc onDrag, void* pUser, const xui_callback_owner_t* pOwner)',
        'InventoryGrid', ['extern "xuiInventoryGridSetDragCallbackOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0) @c_result("status",0) func setDragCallback(InventoryDragProc? onDrag)']),
    'xuiInventoryGridSetDropCallbackOwned': (
        'int xuiInventoryGridSetDropCallbackOwned(xui_widget pWidget, xui_inventory_drop_proc onDrop, void* pUser, const xui_callback_owner_t* pOwner)',
        'InventoryGrid', ['extern "xuiInventoryGridSetDropCallbackOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0) @c_result("status",0) func setDropCallback(InventoryDropProc? onDrop)']),
    'xuiInventoryGridSetTooltipCallbackOwned': (
        'int xuiInventoryGridSetTooltipCallbackOwned(xui_widget pWidget, xui_inventory_tooltip_owned_proc onTooltip, void* pUser, const xui_callback_owner_t* pOwner)',
        'InventoryGrid', ['extern "xuiInventoryGridSetTooltipCallbackOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0,"owned_text") @c_result("status",0) func setTooltipProvider(InventoryTooltipProc? provider)']),
    'xuiInventoryGridSetRenderCallbackOwned': (
        'int xuiInventoryGridSetRenderCallbackOwned(xui_widget pWidget, xui_inventory_slot_render_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'InventoryGrid', ['extern "xuiInventoryGridSetRenderCallbackOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setSlotRenderer(InventorySlotRenderProc? renderer)']),
    'xuiInventoryGridSetAnimationLayerOwned': (
        'int xuiInventoryGridSetAnimationLayerOwned(xui_widget pWidget, xui_inventory_animation_layer_proc onRender, void* pUser, const xui_callback_owner_t* pOwner)',
        'InventoryGrid', ['extern "xuiInventoryGridSetAnimationLayerOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_result("status",0) func setAnimationRenderer(InventoryAnimationRenderProc? renderer)']),
    'xuiInventoryGridSetSlotAnimationStyle': (
        'int xuiInventoryGridSetSlotAnimationStyle(xui_widget pWidget, int iSlot, uint32_t iFlags, float fScale, uint32_t iTint)',
        'InventoryGrid', ['extern "xuiInventoryGridSetSlotAnimationStyle" @no_raise @c_result("status",0) func setSlotAnimationStyle(c_int index,c_uint flags,c_float scale=1.0,c_uint tint=0)']),
    'xuiInventoryGridGetSlotAnimationStyle': (
        'int xuiInventoryGridGetSlotAnimationStyle(xui_widget pWidget, int iSlot, int* pActive, uint32_t* pFlags, float* pScale, uint32_t* pTint)',
        'InventoryGrid', ['extern "xuiInventoryGridGetSlotAnimationStyle" @no_raise @c_outputs(2,3,4,5) @c_status(0) func getSlotAnimationStyle(c_int index) as (c_int,c_uint,c_float,c_uint)']),
})
CALLBACKS.update({
    'xui_inventory_drag_proc': (
        'typedef int (*xui_inventory_drag_proc)(xui_widget_t* pWidget, int iSlot, void* pUser);',
        'export typedef InventoryDragProc = func(CallbackWidget,c_int) as c_int'),
    'xui_inventory_drop_proc': (
        'typedef int (*xui_inventory_drop_proc)(xui_widget_t* pWidget, int iFromSlot, int iToSlot, int iDropMode, void* pUser);',
        'export typedef InventoryDropProc = func(CallbackWidget,c_int,c_int,c_int) as c_int'),
    'xui_inventory_tooltip_owned_proc': (
        'typedef xstrview (*xui_inventory_tooltip_owned_proc)(xui_widget_t* pWidget, int iSlot, const xui_inventory_slot_t* pSlot, void* pUser);',
        'export typedef InventoryTooltipProc = func(CallbackWidget,c_int,CallbackInventorySlot) as string'),
    'xui_inventory_slot_render_proc': (
        'typedef int (*xui_inventory_slot_render_proc)(xui_widget_t* pWidget, int iSlot, const xui_inventory_slot_t* pSlot, xui_draw_context_t* pDraw, xui_rect_t tRect, uint32_t iState, void* pUser);',
        'export typedef InventorySlotRenderProc = func(CallbackWidget,c_int,CallbackInventorySlot,CallbackDrawContext,Rect,c_uint) as c_int'),
    'xui_inventory_animation_layer_proc': (
        'typedef int (*xui_inventory_animation_layer_proc)(xui_widget_t* pWidget, int iSlot, const xui_inventory_slot_t* pSlot, xui_draw_context_t* pDraw, xui_rect_t tRect, uint32_t iState, uint32_t iFlags, uint32_t iTint, void* pUser);',
        'export typedef InventoryAnimationRenderProc = func(CallbackWidget,c_int,CallbackInventorySlot,CallbackDrawContext,Rect,c_uint,c_uint,c_uint) as c_int'),
})
REPLACED.update({
    'xuiInventoryGridGetScrollModel': ('xui.InventoryGrid.scrollSnapshot','xuiInventoryGridGetScrollSnapshot'),
    'xuiCodeEditGetScrollModel': ('xui.CodeEdit.scrollSnapshot','xuiCodeEditGetScrollSnapshot'),
    'xuiTextEditGetScrollModel': ('xui.TextEdit.scrollSnapshot','xuiTextEditGetScrollSnapshot'),
    'xuiTerminalGetScrollModel': ('xui.Terminal.scrollSnapshot','xuiTerminalGetScrollSnapshot'),
    'xuiScrollFrameGetModel': ('xui.ScrollFrame.scrollSnapshot','xuiScrollFrameGetScrollSnapshot'),
    'xuiScrollViewGetModel': ('xui.ScrollView.scrollSnapshot','xuiScrollViewGetScrollSnapshot'),
    'xuiCanvasGetModel': ('xui.Canvas.scrollSnapshot','xuiCanvasGetScrollSnapshot'),
    'xuiListViewGetModel': ('xui.ListView.scrollSnapshot','xuiListViewGetScrollSnapshot'),
    'xuiTreeViewGetModel': ('xui.TreeView.scrollSnapshot','xuiTreeViewGetScrollSnapshot'),
    'xuiTableViewGetModel': ('xui.TableView.scrollSnapshot','xuiTableViewGetScrollSnapshot'),
    'xuiPopupGetModel': ('xui.Popup.scrollSnapshot','xuiPopupGetScrollSnapshot'),
    'xuiScrollModelSetViewport': ('xui.scrollModelWithViewport','xuiScrollModelWithViewport'),
    'xuiScrollModelSetContentSize': ('xui.scrollModelWithContentSize','xuiScrollModelWithContentSize'),
    'xuiScrollModelSetOffset': ('xui.scrollModelWithOffset','xuiScrollModelWithOffset'),
    'xuiScrollModelScrollBy': ('xui.scrollModelScrolledBy','xuiScrollModelScrolledBy'),
    'xuiScrollModelEnsureRectVisible': ('xui.scrollModelWithRectVisible','xuiScrollModelWithRectVisible'),
    'xuiInventoryGridSetDragCallback': ('xui.InventoryGrid.setDragCallback','xuiInventoryGridSetDragCallbackOwned'),
    'xuiInventoryGridSetDropCallback': ('xui.InventoryGrid.setDropCallback','xuiInventoryGridSetDropCallbackOwned'),
    'xuiInventoryGridSetTooltipCallback': ('xui.InventoryGrid.setTooltipProvider','xuiInventoryGridSetTooltipCallbackOwned'),
    'xuiInventoryGridSetRenderCallback': ('xui.InventoryGrid.setSlotRenderer','xuiInventoryGridSetRenderCallbackOwned'),
    'xuiInventoryGridSetAnimationRenderCallback': ('xui.InventoryGrid.setAnimationRenderer','xuiInventoryGridSetAnimationLayerOwned'),
    'xuiTimeLineViewGetLayer': ('xui.TimeLineView.getLayer', 'xuiTimeLineViewGetLayerDescriptor'),
    'xuiTimeLineViewGetSpan': ('xui.TimeLineView.getSpan', 'xuiTimeLineViewGetSpanDescriptor'),
    'xuiTimeLineViewGetModel': ('xui.TimeLineView.scrollSnapshot', 'xuiTimeLineViewGetScrollSnapshot'),
    'xuiTimeLineViewSetRenderers': ('xui.TimeLineView.setRenderers', 'xuiTimeLineViewSetRenderersOwned'),
    'xuiTimeLineViewSetCurrentFrameCallbacks': ('xui.TimeLineView.setCurrentFrameCallbacks', 'xuiTimeLineViewSetCurrentFrameCallbacksOwned'),
    'xuiTimeLineViewSetLayerCallbacks': ('xui.TimeLineView.setLayerCallbacks', 'xuiTimeLineViewSetLayerCallbacksOwned'),
    'xuiTimeLineViewSetFrameCallbacks': ('xui.TimeLineView.setFrameCallbacks', 'xuiTimeLineViewSetFrameCallbacksOwned'),
    'xuiTimeLineViewSetSpanCallbacks': ('xui.TimeLineView.setSpanCallbacks', 'xuiTimeLineViewSetSpanCallbacksOwned'),
    'xuiTimeLineViewSetLayerSelected': ('xui.TimeLineView.setLayerSelected', 'xuiTimeLineViewSetLayerSelectedOwned'),
    'xuiTimeLineViewSetContextMenu': ('xui.TimeLineView.setContextMenu', 'xuiTimeLineViewSetContextMenuOwned'),
    'xuiTimeLineViewSetFrameClick': ('xui.TimeLineView.setFrameClick', 'xuiTimeLineViewSetFrameClickOwned'),
    'xuiTimeLineViewSetSelectionChange': ('xui.TimeLineView.setSelectionChange', 'xuiTimeLineViewSetSelectionChangeOwned'),
})

METHODS.update({
    'xuiTimeLineViewGetScrollSnapshot': (
        'int xuiTimeLineViewGetScrollSnapshot(xui_widget pWidget, xui_scroll_model_t* pSnapshot)',
        'TimeLineView', ['extern "xuiTimeLineViewGetScrollSnapshot" @no_raise @c_output(1,0) func scrollSnapshot() as ScrollModel']),
    'xuiTimeLineViewSetRenderersOwned': (
        'int xuiTimeLineViewSetRenderersOwned(xui_widget pWidget, xui_timeline_layer_renderer_proc onLayer, void* pLayerUser, const xui_callback_owner_t* pLayerOwner, '
        'xui_timeline_ruler_renderer_proc onRuler, void* pRulerUser, const xui_callback_owner_t* pRulerOwner, '
        'xui_timeline_frame_renderer_proc onFrame, void* pFrameUser, const xui_callback_owner_t* pFrameOwner, '
        'xui_timeline_span_renderer_proc onSpan, void* pSpanUser, const xui_callback_owner_t* pSpanOwner)',
        'TimeLineView', ['extern "xuiTimeLineViewSetRenderersOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",0) '
            '@c_owned_callback(1,0,"xui_callback_owner_t",0) @c_owned_callback(2,0,"xui_callback_owner_t",0) '
            '@c_owned_callback(3,0,"xui_callback_owner_t",0) @c_result("status",0) '
            'func setRenderers(TimelineLayerRenderProc? layer=null,TimelineRulerRenderProc? ruler=null,TimelineFrameRenderProc? frame=null,TimelineSpanRenderProc? span=null)']),
})

CALLBACKS.update({
    'xui_timeline_layer_renderer_proc': (
        'typedef int (*xui_timeline_layer_renderer_proc)(xui_widget_t* pWidget, int iLayer, const xui_timeline_layer_t* pLayer, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef TimelineLayerRenderProc = func(CallbackWidget,c_int,CallbackTimelineLayer,CallbackDrawContext,Rect,c_int) as c_int'),
    'xui_timeline_ruler_renderer_proc': (
        'typedef int (*xui_timeline_ruler_renderer_proc)(xui_widget_t* pWidget, int iFrame, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef TimelineRulerRenderProc = func(CallbackWidget,c_int,CallbackDrawContext,Rect,c_int) as c_int'),
    'xui_timeline_frame_renderer_proc': (
        'typedef int (*xui_timeline_frame_renderer_proc)(xui_widget_t* pWidget, int iLayer, int iFrame, const xui_timeline_frame_t* pFrame, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef TimelineFrameRenderProc = func(CallbackWidget,c_int,c_int,CallbackTimelineFrame?,CallbackDrawContext,Rect,c_int) as c_int'),
    'xui_timeline_span_renderer_proc': (
        'typedef int (*xui_timeline_span_renderer_proc)(xui_widget_t* pWidget, int iLayer, int iSpanId, const xui_timeline_span_t* pSpan, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser);',
        'export typedef TimelineSpanRenderProc = func(CallbackWidget,c_int,c_int,CallbackTimelineSpan,CallbackDrawContext,Rect,c_int) as c_int'),
})

VIEWS.update({
    'CallbackTimelineLayer': '''
export extern "const xui_timeline_layer_t*" class CallbackTimelineLayer @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iId" @c_field get id() as c_int
 extern "bVisible" @c_field get visible() as c_int
 extern "bLocked" @c_field get locked() as c_int
 extern "bSelected" @c_field get selected() as c_int
 extern "fHeight" @c_field get height() as c_float
 extern "iColor" @c_field get color() as c_uint
 extern "xl6Xui3TimelineLayerName" @no_raise @c_result("cstring") func name() as string
end''',
    'CallbackTimelineFrame': '''
export extern "const xui_timeline_frame_t*" class CallbackTimelineFrame @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iLayer" @c_field get layer() as c_int
 extern "iFrame" @c_field get frame() as c_int
 extern "iType" @c_field get kind() as c_int
end''',
    'CallbackTimelineSpan': '''
export extern "const xui_timeline_span_t*" class CallbackTimelineSpan @header("xge3_runtime.h") @lifetime("callback")
 native point handle
 extern "iLayer" @c_field get layer() as c_int
 extern "iId" @c_field get id() as c_int
 extern "iStartFrame" @c_field get startFrame() as c_int
 extern "iEndFrame" @c_field get endFrame() as c_int
 extern "iType" @c_field get kind() as c_int
 extern "iColor" @c_field get color() as c_uint
 extern "xl6Xui3TimelineSpanType" @no_raise @c_result("cstring") func customType() as string
 extern "xl6Xui3TimelineSpanLabel" @no_raise @c_result("cstring") func label() as string
end''',
})

def validate(root,header,function_sha):
    functions={f['name']:f['declaration']for f in header['functions']}
    types={t['name']:t['declaration']for t in header['types']}
    for name,(physical,_,_)in METHODS.items():
        if name in ('xl6Xui3CopyToClipboard','xl6Xui3ClipboardSetItems'):
            runtime=(root/'lib/bindings/_xge_native/include/xge3_runtime.h').read_text(encoding='utf-8-sig')
            if physical not in runtime.splitlines():raise ValueError('reviewed runtime declaration changed: '+name)
        elif functions.get(name)!=physical:raise ValueError('reviewed XUI declaration changed: '+name)
    for name,physical in VIEW_FUNCTIONS.items():
        if functions.get(name)!=physical:raise ValueError('reviewed callback view function changed: '+name)
    text=Path(header['header']).read_text(encoding='utf-8-sig')
    for name,digest in INLINE_FUNCTIONS.items():
        if function_sha(text,name)!=digest:raise ValueError('reviewed inline view helper changed: '+name)
    for name,(physical,_)in CALLBACKS.items():
        if types.get(name)!=physical:raise ValueError('reviewed callback signature changed: '+name)
    for name,digest in TYPE_SHAPES.items():
        entry=next((v['type']for v in header['types']if v['name']==name),None)
        actual=hashlib.sha256(json.dumps(entry,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        if actual!=digest:raise ValueError('reviewed XUI resolved type changed: '+name)
    for name,physical in RECORDS.items():
        if types.get(name)!=physical:raise ValueError('reviewed XUI record changed: '+name)


def extend(context,classes,free,callback_declarations,mapped):
    # The inferred declaration belongs to the standalone widget. Publish it
    # on the common canvas view so Workflow can use it as well.
    canvas=classes['FlowGraphWidget']
    canvas[:]=[row for row in canvas if 'extern "xuiFlowGraphWidgetGetHoverHit"' not in row]
    for native,(_,owner,rows)in METHODS.items():
        target=free if owner is None else context if owner=='Context'else classes[owner]
        target[:]=[row for row in target if 'extern "'+native+'"'not in row]
        target.extend((' 'if owner else'')+row for row in rows)
        method=re.search(r'\bfunc (\w+)\(',rows[0]).group(1)
        mapped[native]='xui.'+(owner+'.'if owner else'')+method
    # A dedicated view factory preserves the existing bound method signature.
    free.append('export extern "xuiWorkflowWidgetGetCanvas" @header("xge3_runtime.h") @no_raise @c_result("owner_view",0) func flowCanvas(WorkflowWidget widget) as FlowCanvas?')
    # requestClose intentionally operates on a callback-epoch view, not an
    # independently owning Context projected from an asynchronous observer.
    context[:]=[row for row in context if 'extern "xuiRequestClose"'not in row]
    for native,(_,row)in CALLBACKS.items():
        logical=re.search(r'typedef (\w+)',row).group(1)
        callback_declarations[:]=[v for v in callback_declarations if not v.startswith('export typedef '+logical+' ')]
        callback_declarations.append(row)
    callback_declarations.extend(WIDGET_TYPE_CALLBACKS)
    for name,body in VIEWS.items():
        pattern=r'^export extern "[^"\n]+" class '+re.escape(name)+r'\b'
        callback_declarations[:]=[v for v in callback_declarations if not re.match(pattern,v)]
        callback_declarations.append(body)
    free[:]=[row for row in free if 'extern "xuiWidgetUnregisterType"' not in row]
    free.append('extern "xuiWidgetUnregisterType" @no_raise @c_result("status",0) func rollbackWidgetType(WidgetType value)')
    free.append(INPUT_DECORATION_ROLLBACK)
    mapped.update(xuiWidgetTypeGetName='xui.WidgetType.getName',xuiWidgetTypeGetParent='xui.WidgetType.getParent',
                  xuiWidgetUnregisterType='xui.WidgetType.unregister')
    mapped.update(xuiCodeInputResponseGetFlags='xui.CallbackCodeInputResponse.flags',
                  xuiCodeInputResponseSetFlags='xui.CallbackCodeInputResponse.setFlags',
                  xuiCodeInputResponseSetSelection='xui.CallbackCodeInputResponse.setSelection',
                  xuiCodeInputResponseSetEdits='xui.CallbackCodeInputResponse.setEdits')
    mapped['xuiCodeDiagnosticHitDetails']='xui.CallbackConstCodeDiagnosticHit.details'
    mapped['xuiInputDecorationGetBounds']='xui.InputDecoration.getBounds'
    mapped['xuiInputDecorationDescribe']='xui.InputDecoration.describe'
    mapped['xuiInputDecorationRemoveFromOwner']='xui.InputDecoration.remove'
    mapped.update(xuiTooltipReplySetText='xui.TooltipReply.setText',xuiTooltipReplySetCustom='xui.TooltipReply.setCustom')
    mapped['xuiInventoryIndicesGet']='xui.InventoryIndices.indices'
    mapped.update(xuiEventDragData='xui.CallbackConstEvent.dragData',xuiRequestClose='xui.CallbackContext.requestClose')


def managed_units(path):
    """Compare whole declarations with their class, keeping overload multiplicity.

    Class/declaration order and comments do not change these extern-only units.
    Attributes, nullable types, defaults, parameter names, field order within a
    declaration and the class owning each method remain byte-sensitive.
    Reject bodies/unknown syntax rather than silently omitting a new construct.
    """
    owner=None;units=Counter();seen=set()
    for line in Path(path).read_text(encoding='utf-8-sig').splitlines():
        text=line.strip()
        if not text or text.startswith('//'):continue
        match=re.match(r'export extern "[^"\n]+" class (\w+)\b',text)
        if match:
            if owner or match[1]in seen:raise ValueError('nested/duplicate managed class: '+text)
            owner=match[1];seen.add(owner);units[('',text)]+=1
        elif text=='end':
            if owner is None:raise ValueError('unexpected managed end')
            owner=None
        elif owner and (text=='native point handle' or re.match(r'extern "[^"\n]+" .*\b(?:func|get|let) \w+\(',text)):
            units[(owner,text)]+=1
        elif owner is None and (re.match(r'export typedef \w+ = func\(',text)or re.match(r'(?:export )?extern "[^"\n]+" .*\bfunc \w+\(',text)):
            units[('',text)]+=1
        else:raise ValueError('unsupported managed declaration syntax: '+text)
    if owner is not None:raise ValueError('unterminated managed class: '+owner)
    return units
