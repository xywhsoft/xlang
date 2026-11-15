"""Reviewed direct record snapshots; no Native forwarding implementation."""

# native: (owner, language method, native record, output slot, parameters,
#          [(member, language type), ...])
RECORD_OUTPUTS = {
    'xuiCodeAnnotationGetDiagnostic': (
        'CodeAnnotationStore', 'getDiagnostic', 'xui_code_diagnostic_t', 2,
        'c_int index', [('tRange','CodeRange'), ('iSeverity','c_int'),
                       ('sCode','string?'), ('sMessage','string?'),
                       ('sSource','string?'), ('iUserData','c_uintptr')]),
    'xuiCodeEditGetVirtualText': (
        'CodeEdit', 'getVirtualText', 'xui_code_virtual_text_t', 1, '',
        [('iKind','c_int'), ('iOffset','c_int'), ('sText','string?'),
         ('iFlags','c_uint'), ('iUserData','c_uintptr')]),
    'xuiWidgetGetAccessibleNode': (
        'Widget', 'getAccessibleNode', 'xui_accessible_node_t', 2,
        'c_int index=0', [('iId','uint64'), ('iParentId','uint64'),
                        ('iRole','c_int'), ('iState','c_uint'),
                        ('sName','string?'), ('sValue','string?'),
                        ('sDescription','string?'), ('tBounds','Rect'),
                        ('iTextStart','c_int'), ('iTextEnd','c_int'),
                        ('iLevel','c_int'), ('iRow','c_int'), ('iColumn','c_int'),
                        ('iRowCount','c_int'), ('iColumnCount','c_int'),
                        ('iActions','c_uint'), ('tRange','AccessibleRange')]),
    'xuiStyleGetToken': (
        'Context', 'styleGetToken', 'xui_style_value_t', 2,
        'string name', [('iType','c_int'), ('iInt','c_int'),
                        ('fFloat','c_float'), ('iColor','c_uint'),
                        ('sText','string?')]),
}

STYLE_TOKEN_SET = (' extern "xuiStyleSetToken" @no_raise @c_cstr(0) '
    '@c_pack(2,"xui_style_value_t",1,5) @c_pack_size(2,"iSize","uint32_t") '
    '@c_pack_field(2,"iType",1,"value") @c_pack_field(2,"iInt",2,"value") '
    '@c_pack_field(2,"fFloat",3,"value") @c_pack_field(2,"iColor",4,"value") '
    '@c_pack_field(2,"sText",5,"c_string") @c_result("status",0) '
    'func styleSetToken(string name,c_int valueType,c_int integer=0,'
    'c_float number=0,c_uint color=0,string? text=null)')

VIRTUAL_TEXT_SET = (' extern "xuiCodeEditSetVirtualText" @no_raise '
    '@c_pack(1,"xui_code_virtual_text_t",0,5) @c_pack_size(1,"iSize","uint32_t") '
    '@c_pack_field(1,"iKind",0,"value") @c_pack_field(1,"iOffset",1,"value") '
    '@c_pack_field(1,"sText",2,"c_string") @c_pack_field(1,"iFlags",3,"value") '
    '@c_pack_field(1,"iUserData",4,"value") @c_result("status",0) '
    'func setVirtualText(c_int kind,c_int offset,string text,c_uint flags=0,c_uintptr userData=0)')


def declaration(native):
    _,name,record,slot,parameters,fields=RECORD_OUTPUTS[native]
    attrs=['@no_raise',f'@c_output_record({slot},"{record}","borrowed")',
           f'@c_output_size({slot},"iSize")','@c_status(0)']
    if native=='xuiStyleGetToken':attrs.append('@c_cstr(0)')
    attrs += [f'@c_output_field({i},"{field}"'+(',"cstring"'if typ=='string?'else'')+')'
              for i,(field,typ)in enumerate(fields)]
    return (' extern "'+native+'" '+' '.join(attrs)+' func '+name+'('+parameters+
            ') as ('+','.join(typ for _,typ in fields)+')')


def extend(context,classes,mapped):
    for native,row in RECORD_OUTPUTS.items():
        owner=row[0]
        if owner=='CodeAnnotationStore':continue  # Independent source file.
        target=context if owner=='Context'else classes[owner]
        target[:] = [line for line in target if 'extern "'+native+'"'not in line]
        target.append(declaration(native));mapped[native]='xui.'+owner+'.'+row[1]
    context[:] = [line for line in context if 'extern "xuiStyleSetToken"'not in line]
    context.append(STYLE_TOKEN_SET);mapped['xuiStyleSetToken']='xui.Context.styleSetToken'
    classes['CodeEdit'][:] = [line for line in classes['CodeEdit'] if 'extern "xuiCodeEditSetVirtualText"'not in line]
    classes['CodeEdit'].append(VIRTUAL_TEXT_SET);mapped['xuiCodeEditSetVirtualText']='xui.CodeEdit.setVirtualText'
