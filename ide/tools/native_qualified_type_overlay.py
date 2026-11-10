"""Retain the original Lemon qualified type name span at its producer.

The existing reduction stack supplies the complete import_name range before
that AST is freed. Join it with the already-created prefix token; leave full
node ranges, grammar actions/tables, bindings, allocations and AST ABI intact.
"""
from pathlib import Path
import hashlib

_START='static XL6_AstNode* xl6PQualifiedType(\n'
_END='\nstatic XL6_AstNode* xl6PNullableType('
_SEAM='        node = xl6PDottedNameAppendRange(ctx, node, rest->text, strlen(rest->text), first);\n'
_ADDITION='        if (node != NULL) xl6AstRangeInclude(&node->tokenRange, &rest->range);\n'

def _body(text):
    if text.count(_START)!=1:raise ValueError('Unknown native qualified type producer')
    begin=text.index(_START);end=text.index(_END,begin)
    return begin,end,text[begin:end]

def derive(source: Path, grammar: Path, output: Path):
    source,grammar,output=Path(source),Path(grammar),Path(output)
    text=source.read_text(encoding='utf8');rules=grammar.read_text(encoding='utf8')
    begin,end,body=_body(text);_,_,original=_body(rules)
    if body!=original or body.count(_SEAM)!=1:
        raise ValueError('Qualified type producer differs from its original grammar')
    if 'XL6_AstRange sourceRange; /* XL6 source-range stack v1 */'not in text or '->range = yySourceRange;'not in text:
        raise ValueError('Native reduction ranges are not available')
    fixed=body.replace(_SEAM,_SEAM+_ADDITION,1)
    if fixed.replace(_ADDITION,'',1)!=body:raise ValueError('Qualified type change exceeds its range metadata seam')
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(text[:begin]+fixed+text[end:],encoding='utf8',newline='\n')
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    return dict(source=str(source),sourceSha256=sha(source),grammarSource=str(grammar),grammarSha256=sha(grammar),
                compiledSource=str(output),compiledSha256=sha(output),auditedSeams=1,
                extraAllocations=0,extraSourceScans=0,extraTypeLookups=0,extraAstWalks=0,astAbiChanged=False,
                change='Join original prefix token and original import_name reduction range before freeing rest')
