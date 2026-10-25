"""Capture raw function IR before module passes,not finalized mutable XLIR.

build_lsp_core guards exact compiler C+header digests before this adapter.
Record every raw-builder symbol lookup and conditional/forced binding write;
all original type/name/layout/call/ownership rules still execute on misses.
"""
from __future__ import annotations


def derive(source: str) -> str:
    old = 'void xl6IrModuleFree(XL6_IrModule* module)'
    if source.count(old) != 1:
        raise ValueError('IR module lifetime seam changed; audit')
    source = source.replace(old, '#include "analysis/frontend_ir_owner.inc"\n' + old)
    for old, new in {
        '        xrtFree(module->nodes[i].text);': '        xlsIrOwnerFreeText(module,i,module->nodes[i].text);',
        '        xrtFree(module->nodes[i].literalData);': '        xlsIrOwnerFreeText(module,i,module->nodes[i].literalData);',
    }.items():
        if source.count(old) != 1:
            raise ValueError('IR node text ownership seam changed; audit')
        source = source.replace(old, new)
    start = source.index('void xl6IrModuleFree(')
    end = source.index('\n}\n', start)
    source = source[:end] + '\n    xlsIrOwnerReset(module);' + source[end:]
    start = source.index('static XL6_IrNodeId xl6IrBuildAstNode(')
    end = source.index('\nstatic XL6_IrTypeId xl6IrClassTypeByDeclaration(', start)
    body = source[start:end]
    changes = {
        'static XL6_IrNodeId xl6IrBuildAstNode(': 'static XL6_IrNodeId xlsIrBuildAstNodeOriginal(',
        '    if ( node->symbol != XL6_SYMBOL_ID_INVALID && module->symbolNodes[node->symbol] == XL6_IR_NODE_INVALID ) module->symbolNodes[node->symbol] = id;':
            '    xlsIrMemoInitial(module,node->symbol,id);',
        '            module->symbolNodes[binding->symbol] = id;':
            '            xlsIrMemoWrite(module,binding->symbol,id);',
        '            module->symbolNodes[binding->symbol] = children[0];':
            '            xlsIrMemoWrite(module,binding->symbol,children[0]);',
    }
    for old, new in changes.items():
        if body.count(old) != 1:
            raise ValueError('raw IR fragment seam changed; audit: ' + old)
        body = body.replace(old, new)
    old = 'xl6IrNodeGet(module, module->symbolNodes[source->semanticSymbolId])'
    if body.count(old) != 2:
        raise ValueError('raw IR symbol dependency reads changed; audit')
    body = body.replace(old, 'xlsIrMemoRead(module,source->semanticSymbolId)')
    return source[:start] + '#include "analysis/frontend_ir_memo.inc"\n' + body + source[end:]
