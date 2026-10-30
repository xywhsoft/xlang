"""Audited cooperative abort points; original compiler assets stay immutable.

Called only after build_lsp_core's exact compiler-source digest checks. Abort
uses the original return/cleanup paths, never asynchronous jumps or signals.
Uncancelled input runs every original rule and produces the same artifacts.
"""
from __future__ import annotations


def once(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise ValueError('shared cancellation seam changed; audit: ' + old)
    return source.replace(old, new)


def semantic_cancellation(source: str) -> str:
    for name, kind in [('xl6SemCheckExpr', 'EXPRESSION'), ('xl6SemCheckStmt', 'STATEMENT')]:
        seam = f'static int {name}(XL6_SemContext* ctx, XL6_AstNode* node)\n{{'
        source = once(source, seam, seam + f'\n    if(xlsFrontendCancelPoint(XLS_CANCEL_SEM_{kind},0))return 0;')
    start = source.index('int xl6SemCheckModuleWithQuery(')
    end = source.index('\nint xl6SemCheckModuleWithIndex(', start)
    body = source[start:end]
    body = once(body, '    XL6_SemContext ctx;',
                '    XL6_SemContext ctx;\n    xls_sem_call_scope callScope;\n    int xlsCallsActive=0;')
    body = once(body, '    xls_sem_call_scope callScope;\n    xlsSemCallsBegin(&callScope,&ctx);',
                '    xlsSemCallsBegin(&callScope,&ctx);xlsCallsActive=1;')
    loop = '    for ( i = 0; i < root->childCount; ++i ) {'
    if body.count(loop) != 4:
        raise ValueError('shared module declaration/body loops changed; audit cancellation cleanup')
    body = body.replace(loop, loop + '\n        if(xlsFrontendCancelPoint(XLS_CANCEL_SEM_MODULE,1)){ok=0;goto xls_sem_cancel_cleanup;}')
    body = once(body, '    xlsSemCallsEnd(&callScope);',
                '    xlsSemCallsEnd(&callScope);xlsCallsActive=0;\n'
                '    if(xlsFrontendCancelPoint(XLS_CANCEL_SEM_MODULE,1)){ok=0;goto xls_sem_cancel_cleanup;}')
    body = once(body, '    xl6SemPopScope(&ctx, &globalScope);',
                'xls_sem_cancel_cleanup:\n    if(xlsCallsActive)xlsSemCallsEnd(&callScope);\n'
                '    xl6SemPopScope(&ctx, &globalScope);')
    return source[:start] + body + source[end:]


def ir_cancellation(source: str, *, current=False) -> str:
    start = source.index('static XL6_IrNodeId xl6IrBuildAstNode(')
    end = source.index('\nstatic XL6_IrTypeId xl6IrClassTypeByDeclaration(', start)
    body = source[start:end]
    body = once(body, '    XL6_IrNodeId id;',
                '    if(xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,0)){*ok=0;return XL6_IR_NODE_INVALID;}\n'
                '    XL6_IrNodeId id;')
    body = once(body, '    for ( i = 0; children != NULL && i < source->childCount; ++i ) {',
                '    for ( i = 0; children != NULL && i < source->childCount; ++i ) {\n'
                '        if(xlsFrontendCancelObserved()){*ok=0;break;}')
    seam = '    node = &module->nodes[(size_t)id - 1];\n    /* ' + ('`auto` declarations' if current else 'An indexed accessor')
    body = once(body, seam,
                '    if(xlsFrontendCancelObserved()){xrtFree(children);*ok=0;return XL6_IR_NODE_INVALID;}\n' + seam)
    source = source[:start] + body + source[end:]
    start = source.index('static int xlsIrBuildModuleOriginal(XL6_IrModule* module, const XL6_AstNode* root, XL6_DiagList* diags)\n{')
    end = source.index('\nconst XL6_IrNode* xl6IrNodeGet(', start)
    body = source[start:end]
    body = once(body, '    xl6IrModuleFree(module); xl6IrModuleInit(module);',
                '    xl6IrModuleFree(module); xl6IrModuleInit(module);\n'
                '    if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;')
    # This exact sequential chain owns no temporary heap allocations between
    # calls. A cancelled helper uses the existing module-free failure branch.
    a = body.index('    if ( !xl6IrInternAsyncInvocationTypes(')
    b = body.index('    if ( buildFailure != NULL )', a)
    phases = body[a:b]
    if phases.count('if ( !') != (28 if current else 26):
        raise ValueError('shared IR build phase chain changed; audit cancellation')
    phases = phases.replace('if ( !', 'if ( xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1) || !')
    phases = once(phases, 'else if ( (buildFailure = xl6IrBuildClassDispatchTables(',
                  'else if ( xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1) || (buildFailure = xl6IrBuildClassDispatchTables(')
    # The dispatch-table assignment can produce NULL on the cancelled arm.
    phases = once(phases, '!= NULL ) { }', '!= NULL ) { if(xlsFrontendCancelObserved())buildFailure="cancelled XLIR class dispatch build"; }')
    body = body[:a] + phases + body[b:]
    loop = '    for ( i = 0; i < module->nodeCount; ++i ) {'
    body = once(body, loop, loop + '\n        if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,0)){xl6IrModuleFree(module);return 0;}')
    body = once(body, '    if ( !xl6IrVerifyModule(module, diags) )',
                '    if ( xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1) || !xl6IrVerifyModule(module, diags) )')
    source = source[:start] + body + source[end:]
    start = source.index('static int xlsIrVerifyModuleOriginal(const XL6_IrModule* module, XL6_DiagList* diags)\n{')
    end = source.index('\nstatic ', start)
    body = source[start:end]
    body = once(body, '    size_t i;', '    if(xlsFrontendCancelPoint(XLS_CANCEL_IR_VERIFY,1))return 0;\n    size_t i;')
    a = body.index('    incoming = (uint32*)xrtCalloc(')
    b = body.index('    xrtFree(incoming);\n    for ( i = 0; i < module->nodeCount;', a) + len('    xrtFree(incoming);')
    parts = [body[:a], body[a:b], body[b:]]
    loop = 'for ( i = 0; i < module->nodeCount; ++i ) {'
    if [part.count(loop) for part in parts] != [4, 2, 1]:
        raise ValueError('shared verifier node loops/owned incoming arena changed; audit cleanup')
    for index, part in enumerate(parts):
        cleanup = 'xrtFree(incoming);' if index == 1 else ''
        parts[index] = part.replace(loop, loop + '\n        if(xlsFrontendCancelPoint(XLS_CANCEL_IR_VERIFY,0)){' + cleanup + 'return 0;}')
    return source[:start] + ''.join(parts) + source[end:]
