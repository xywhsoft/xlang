"""Scope canonical initializer lookup to audited immutable IR phases."""
def derive(source: str) -> str:
    changes={
        'static int xl6IrValueInitializerMayRaise(\n    const XL6_IrModule* module, XL6_IrNodeId declaration)\n{':
            'static int xlsIrValueInitializerMayRaiseOriginal(\n    const XL6_IrModule* module, XL6_IrNodeId declaration)\n{',
        'const XL6_IrValueInitializer* xl6IrValueInitializerGet(\n    const XL6_IrModule* module, XL6_IrNodeId declaration)\n{':
            'static const XL6_IrValueInitializer* xlsIrValueInitializerGetOriginal(\n    const XL6_IrModule* module, XL6_IrNodeId declaration)\n{',
    }
    for old,new in changes.items():
        if source.count(old)!=1:raise ValueError('initializer lookup seam changed; audit original first-match rules')
        source=source.replace(old,new)
    start=source.index('static int xlsIrBuildModuleOriginal(')
    end=source.index('\nconst XL6_IrNode* xl6IrNodeGet',start)
    body=source[start:end]
    loop='    for ( i = 0; i < module->nodeCount; ++i ) {'
    cancel='if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,0)){xl6IrModuleFree(module);return 0;}'
    verify='    if ( xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1) || !xl6IrVerifyModule(module, diags) )'
    if any(body.count(v)!=1 for v in [loop,cancel,verify]):raise ValueError('IR statistics scope changed; audit read-only table and cancellation cleanup')
    body=body.replace(loop,'    xls_ir_initializer_scope initializers;xlsFrontendIrInitializerBegin(&initializers,module);\n'+loop)
    body=body.replace(cancel,'if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,0)){xlsFrontendIrInitializerEnd(&initializers);xl6IrModuleFree(module);return 0;}')
    body=body.replace(verify,'    xlsFrontendIrInitializerEnd(&initializers);\n'+verify)
    source=source[:start]+body+source[end:]
    gate='''                    "XLIR value declaration has duplicate initializer contracts");
    }
    for ( i = 0; i < module->nodeCount; ++i ) {'''
    if source.count(gate)!=1:raise ValueError('initializer verification gate changed; audit complete original arena validation before any index proof')
    return source.replace(gate,gate.replace('    for ( i = 0;', '    xlsIrInitializerValidated(module);\n    for ( i = 0;'))
