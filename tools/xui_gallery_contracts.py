"""Reviewed direct declarations needed by the complete public control gallery.

No C shims: strings/arrays/descriptors are projected by existing compiler
contracts. Array projection has O(n) temporary storage; ordinary calls do not.
"""
METHODS = {
 'Context': [
  'extern "xuiRender" @no_raise @c_span(1,"const xui_rect_i_t*","int") @c_result("status",0) func render(Surface target,bytes damage=b"")'],
 'ListView': ['extern "xuiListViewSetItems" @no_raise @c_result("status",0) @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const char**","c_string") func setItems(array<string> items,int size=-1)'],
 'ComboBox': ['extern "xuiComboBoxSetItems" @no_raise @c_result("status",0) @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const char**","c_string") func setItems(array<string> items,int size=-1)'],
 'StepBar': ['extern "xuiStepBarSetSteps" @no_raise @c_result("status",0) @c_array_group(1,0,1,"int",1) @c_array_input(1,0,"const char* const*","c_string") func setSteps(array<string> titles,int size=-1)'],
 'Chart': [
  'extern "xuiChartAddSeries" @no_raise @c_cstr(1) @c_output(3,0) func addSeries(c_int type,string name) as c_int',
  'extern "xuiChartSetSeriesData" @no_raise @c_result("status",0) @c_array_group(2,1,2,"int",3) @c_array_record(2,"xui_chart_point_t") @c_array_field(2,"x",0,"value") @c_array_field(2,"y",1,"value") func setSeriesData(c_int series,array<c_double> xs,array<c_double> ys,int size=-1)'],
 'Menu': ['extern "xuiMenuAddItem" @no_raise @c_result("status",0) @c_pack(1,"xui_menu_item_t",0,6) @c_pack_field(1,"sText",0,"c_string") @c_pack_field(1,"sShortcut",1,"c_string") @c_pack_field(1,"iType",2,"value") @c_pack_field(1,"iState",3,"value") @c_pack_field(1,"iValue",4,"value") @c_pack_field(1,"iIcon",5,"value") func addItem(string text,string? shortcut=null,c_int type=0,c_uint state=1,c_int value=0,c_int icon=0)'],
 'PropertyGrid': ['extern "xuiPropertyGridAddProperty" @no_raise @c_pack(2,"xui_property_desc_t",1,7) @c_pack_field(2,"sId",1,"c_string") @c_pack_field(2,"sName",2,"c_string") @c_pack_field(2,"sDescription",3,"c_string") @c_pack_field(2,"iType",4,"value") @c_pack_field(2,"sValue",5,"c_string") @c_pack_field(2,"sDefaultValue",6,"c_string") @c_pack_field(2,"iFlags",7,"value") func addProperty(c_int category,string id,string name,string description="",c_int type=0,string value="",string? defaultValue=null,c_int flags=0) as c_int'],
 'TimeLineView': [
  'extern "xuiTimeLineViewAddLayer" @no_raise @c_cstr(0) @c_output(2,0) func addLayer(string name) as c_int',
  'extern "xuiTimeLineViewAddSpan" @no_raise @c_cstr(4) @c_output(6,0) func addSpan(c_int layer,c_int start,c_int stop,c_int type,string label) as c_int'],
 'DockPanel': ['extern "xuiDockPanelAddWindow" @no_raise @c_cstr(0) @c_output(3,0) func addWindow(string title,Widget client) as c_int'],
 'Accordion': ['extern "xuiAccordionAddSection" @no_raise @c_cstr(0) @c_result("status",0) @c_null(4,"xui_widget*") func addSection(string title,c_int id,c_int expanded)'],
}
for owner in ('TableView','TableGrid'):
 METHODS[owner]=['extern "xui'+owner+'SetColumnsCopy" @no_raise @c_result("status",0) @c_array_group(1,0,2,"int",2) @c_array_record(1,"xui_table_view_column_t") @c_array_field(1,"sTitle",0,"c_string") @c_array_field(1,"fWidth",1,"value") func setColumns(array<string> titles,array<c_float> widths,int size=-1)']

# Explicit NULL descriptors select authoritative native defaults.
METHODS["Context"] += ['extern "xuiSliderCreate" @no_raise @c_null(2,"const xui_slider_desc_t*") @c_result("owner_view",0) @factory(1,0,rollbackSlider) func slider() as Slider', 'extern "xuiRangeSliderCreate" @no_raise @c_null(2,"const xui_range_slider_desc_t*") @c_result("owner_view",0) @factory(1,0,rollbackRangeSlider) func rangeSlider() as RangeSlider', 'extern "xuiScrollBarCreate" @no_raise @c_null(2,"const xui_scrollbar_desc_t*") @c_result("owner_view",0) @factory(1,0,rollbackScrollBar) func scrollBar() as ScrollBar', 'extern "xuiSplitLayoutCreate" @no_raise @c_null(2,"const xui_split_layout_desc_t*") @c_result("owner_view",0) @factory(1,0,rollbackSplitLayout) func splitLayout() as SplitLayout', 'extern "xuiScrollViewCreate" @no_raise @c_null(2,"const xui_scroll_view_desc_t*") @c_result("owner_view",0) @factory(1,0,rollbackScrollView) func scrollView() as ScrollView', 'extern "xuiCanvasCreate" @no_raise @c_null(2,"const xui_canvas_desc_t*") @c_result("owner_view",0) @factory(1,0,rollbackCanvas) func canvas() as Canvas', 'extern "xuiSeparatorCreate" @no_raise @c_null(2,"const xui_separator_desc_t*") @c_result("owner_view",0) @factory(1,0,rollbackSeparator) func separator() as Separator']

for native,method,kind in [('xuiRadioGroupCreate','radioGroup','RadioGroup'),('xuiScrollFrameCreate','scrollFrame','ScrollFrame')]:
 METHODS['Context'].append('extern "'+native+'" @no_raise @c_null(2,"const '+('xui_radio_group_desc_t'if kind=='RadioGroup'else'xui_scroll_frame_desc_t')+'*") @c_result("owner_view",0) @factory(1,0,rollback'+kind+') func '+method+'() as '+kind)
METHODS['Context'].append('extern "xuiWebViewCreate" @no_raise @c_pack(2,"xui_webview_desc_t",0,1) @c_pack_size(2,"iSize","uint32_t") @c_pack_field(2,"sUserDataFolder",0,"c_string") @c_result("owner_view",0) @factory(1,0,rollbackWebView) func webView(string? userDataFolder=null) as WebView')
METHODS['Cascader']=['extern "xuiCascaderSetItems" @no_raise @c_result("status",0) @c_array_group(1,0,4,"int",4) @c_array_record(1,"xui_cascader_item_t") @c_array_field(1,"sText",0,"c_string") @c_array_field(1,"iValue",1,"value") @c_array_field(1,"iParent",2,"value") @c_array_field(1,"iFlags",3,"value") func setItems(array<string> titles,array<c_int> values,array<c_int> parents,array<c_uint> flags,int size=-1)']
METHODS['DockPanel'].append('extern "xuiDockPanelDockWindow" @no_raise @c_output(5,0) func dockWindow(c_int window,c_int region,c_int side,c_float ratio=0) as c_int')

def extend(context,classes,mapped):
 for owner,rows in METHODS.items():
  target=context if owner=='Context'else classes[owner]
  for row in rows:
   native=row.split('"')[1]
   # Default overloads supplement the full typed descriptor constructors.
   # Preserve those existing declarations when regenerating the binding.
   if native=='xuiWebViewCreate':
    # A packed safe constructor replaces the opaque caller descriptor entry.
    target[:]=[value for value in target if 'extern "'+native+'"'not in value]
   elif native.endswith('Create'):
    target[:]=[value for value in target if value.strip()!=row]
   else:
    target[:]=[value for value in target if 'extern "'+native+'"'not in value]
   target.append(' '+row)
   mapped[native]='xui.'+owner+'.'+row.split('func ',1)[1].split('(')[0]
