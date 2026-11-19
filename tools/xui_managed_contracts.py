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
 'xl6Xui3CopyToClipboard': ['int xl6Xui3CopyToClipboard(xui_context,xui_data_object,const XL6_CodeOwner*);',
                            'Context',
                            ['extern "xl6Xui3CopyToClipboard" @no_raise @c_code_owner(2) @c_result("status",0) func '
                             'copyToClipboard(DataObject data)']],
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
    'xuiComboBoxGetItemDescriptor': ['int xuiComboBoxGetItemDescriptor(xui_widget pWidget, int iIndex, xui_combobox_item_t* pItem)',
        'ComboBox', ['extern "xuiComboBoxGetItemDescriptor" @no_raise @c_output_record(2,"xui_combobox_item_t","borrowed") @c_status(0) '
        '@c_output_field(0,"sText","cstring") @c_output_field(1,"iValue") @c_output_field(2,"bEnabled") @c_output_field(3,"bSeparator") '
        '@c_output_field(4,"iIcon") func getItem(c_int index) as (string,c_int,c_int,c_int,c_int)']],
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
    'xuiTableViewGetColumnDescriptor': ['int xuiTableViewGetColumnDescriptor(xui_widget pWidget, int iColumn, xui_table_column_desc_t* pColumn)',
        'TableView', ['extern "xuiTableViewGetColumnDescriptor" @no_raise @c_output_record(2,"xui_table_column_desc_t","borrowed") @c_status(0) @c_output_field(0,"tSettings") @c_output_field(1,"sId","cstring") @c_output_field(2,"sTitle","cstring") @c_output_field(3,"tRect") func getColumn(c_int index) as (TableColumnSettings,string?,string?,Rect)']],
    'xuiTableViewSetColumnDescriptors': ['int xuiTableViewSetColumnDescriptors(xui_widget pWidget, const xui_table_column_desc_t* pColumns, int iCount)',
        'TableView', ['extern "xuiTableViewSetColumnDescriptors" @no_raise @c_array_group(1,0,3,"int",3) @c_array_record(1,"xui_table_column_desc_t") @c_array_field(1,"tSettings",0,"value") @c_array_field(1,"sId",1,"c_string") @c_array_field(1,"sTitle",2,"c_string") @c_result("status",0) func setColumnsDetailed(array<TableColumnSettings> settings,array<string?> ids,array<string?> titles,int size=-1)']],
    'xuiTableGridGetColumnDescriptor': ['int xuiTableGridGetColumnDescriptor(xui_widget pWidget, int iColumn, xui_table_column_desc_t* pColumn)',
        'TableGrid', ['extern "xuiTableGridGetColumnDescriptor" @no_raise @c_output_record(2,"xui_table_column_desc_t","borrowed") @c_status(0) @c_output_field(0,"tSettings") @c_output_field(1,"sId","cstring") @c_output_field(2,"sTitle","cstring") @c_output_field(3,"tRect") func getColumn(c_int index) as (TableColumnSettings,string?,string?,Rect)']],
    'xuiTableGridSetColumnDescriptors': ['int xuiTableGridSetColumnDescriptors(xui_widget pWidget, const xui_table_column_desc_t* pColumns, int iCount)',
        'TableGrid', ['extern "xuiTableGridSetColumnDescriptors" @no_raise @c_array_group(1,0,3,"int",3) @c_array_record(1,"xui_table_column_desc_t") @c_array_field(1,"tSettings",0,"value") @c_array_field(1,"sId",1,"c_string") @c_array_field(1,"sTitle",2,"c_string") @c_result("status",0) func setColumnsDetailed(array<TableColumnSettings> settings,array<string?> ids,array<string?> titles,int size=-1)']],
    'xuiTableViewGetSortColumn': ['int xuiTableViewGetSortColumn(xui_widget pWidget, int* pDescending)',
        'TableView', ['extern "xuiTableViewGetSortColumn" @no_raise @c_outputs(-1,1) @c_result("integer_min",-1) func getSortColumn() as (c_int,c_int)']],
})

METHODS.update({
    'xuiTreeViewSetNodeDescriptors': ['int xuiTreeViewSetNodeDescriptors(xui_widget pWidget, const xui_tree_node_desc_t* pNodes, int iCount)',
        'TreeView', ['extern "xuiTreeViewSetNodeDescriptors" @no_raise @c_array_group(1,0,2,"int",2) @c_array_record(1,"xui_tree_node_desc_t") @c_array_field(1,"tState",0,"value") @c_array_field(1,"sText",1,"c_string") @c_result("status",0) func setNodes(array<TreeNodeState> states,array<string?> texts,int size=-1)']],
    'xuiTreeViewGetNodeDescriptor': ['int xuiTreeViewGetNodeDescriptor(xui_widget pWidget, int iIndex, xui_tree_node_desc_t* pNode)',
        'TreeView', ['extern "xuiTreeViewGetNodeDescriptor" @no_raise @c_output_record(2,"xui_tree_node_desc_t","borrowed") @c_status(0) @c_output_field(0,"tState") @c_output_field(1,"sText","cstring") func getNode(c_int index) as (TreeNodeState,string)']],
    'xuiTreeViewGetNodeDescriptorById': ['int xuiTreeViewGetNodeDescriptorById(xui_widget pWidget, int iNodeId, xui_tree_node_desc_t* pNode)',
        'TreeView', ['extern "xuiTreeViewGetNodeDescriptorById" @no_raise @c_output_record(2,"xui_tree_node_desc_t","borrowed") @c_status(0) @c_output_field(0,"tState") @c_output_field(1,"sText","cstring") func getNodeById(c_int id) as (TreeNodeState,string)']],
    'xuiTreeViewGetVisibleNodeDescriptor': ['int xuiTreeViewGetVisibleNodeDescriptor(xui_widget pWidget, int iVisible, xui_tree_node_desc_t* pNode)',
        'TreeView', ['extern "xuiTreeViewGetVisibleNodeDescriptor" @no_raise @c_output_record(2,"xui_tree_node_desc_t","borrowed") @c_status(0) @c_output_field(0,"tState") @c_output_field(1,"sText","cstring") func getVisibleNode(c_int index) as (TreeNodeState,string)']],
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

VIEW_FUNCTIONS = {'xuiGetLanguage': 'int xuiGetLanguage(xui_context pContext)', 'xuiRequestClose': 'int xuiRequestClose(xui_context pContext)'}

INLINE_FUNCTIONS = {'xuiEventDragData': 'f9bd130d67726da8ae4ca0fa029089b33cb1ed0de3d9918fec59bee9d0ba219c'}

# Owned WebView Close events are qualified independently of browser startup.
# Platform/browser readiness and successful zoom remain runtime prerequisites.
DEFERRED = {}

# Sized text views preserve embedded NUL and checked read failure. The legacy
# C-string producers remain native capabilities, with explicit replacement maps.
REPLACED = {
    'xuiEditGetText': ('xui.Widget.editGetText','xuiEditGetTextView'),
    'xuiCodeEditGetText': ('xui.CodeEdit.getText','xuiCodeEditGetTextView'),
}


def validate(root,header,function_sha):
    functions={f['name']:f['declaration']for f in header['functions']}
    types={t['name']:t['declaration']for t in header['types']}
    for name,(physical,_,_)in METHODS.items():
        if name=='xl6Xui3CopyToClipboard':
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
    for name,body in VIEWS.items():
        pattern=r'^export extern "[^"\n]+" class '+re.escape(name)+r'\b'
        callback_declarations[:]=[v for v in callback_declarations if not re.match(pattern,v)]
        callback_declarations.append(body)
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
