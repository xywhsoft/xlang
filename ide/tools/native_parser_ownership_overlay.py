"""Release original Lemon AST stack values during error/final stack unwind.

The pinned grammar gives every AST-valued nonterminal an explicit LHS alias.
Start/separator controls have no AST value; tokens and scalar overrides use
different union fields. No grammar action, parse table or diagnostic changes.
"""
from pathlib import Path
import hashlib
import re


def derive(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    original = frontend / 'src/parser/xl6_parser.c'
    grammar = frontend / 'src/parser/xl6_parser.y'
    expected = '3fdd30d906a26f885777a9ab05cd08df7736a52b2eef2148c6ead75950e5651e'
    grammar_sha = '639ba6823c4dc9dffb6d9bf6a8e14fded358613a57c071ad3ddcd968eff628b7'
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    if sha(original) != expected or sha(grammar) != grammar_sha:
        raise ValueError('shared Lemon grammar/parser changed; audit AST stack ownership and union tags')
    source = original.read_text(encoding='utf8')
    rules = grammar.read_text(encoding='utf8')
    assert '%default_type { XL6_AstNode* }' in rules
    assert 'XL6_AstNode* yy270;' in source
    overrides = dict(re.findall(r'%type\s+(\w+)\s*\{([^}]+)\}', rules))
    aliases = {}
    for name, alias in re.findall(r'(?m)^(\w+)(\(\w+\))?\s*::=', rules):
        aliases.setdefault(name, []).append(bool(alias))
    controls = {name for name, values in aliases.items() if not any(values)}
    assert controls == {'program', 'opt_seps', 'need_sep', 'if_sep'}
    assert all(all(values) or not any(values) for values in aliases.values())
    owned = {name for name in aliases if name not in controls and
             ('AstNode*' in overrides[name] if name in overrides else True)}
    table = source.split('static const char *const yyTokenName[] = {', 1)[1].split('};', 1)[0]
    symbols = {name: int(number) for number, name in re.findall(r'/\*\s*(\d+)\s*\*/\s*"([^"]+)"', table)}
    assert aliases.keys() <= symbols.keys() and all(symbols[name] >= 127 for name in owned)
    begin = '/********* Begin destructor definitions ***************************************/'
    end = '/********* End destructor definitions *****************************************/'
    assert source.count(begin) == source.count(end) == 1
    assert not source.split(begin, 1)[1].split(end, 1)[0].strip()
    cases = '\n'.join(f'    case {symbols[name]}: /* {name} */' for name in sorted(owned, key=symbols.get))
    destructor = ('\n    /* Original reductions transfer AST ownership. Only discarded stack\n'
                  '     * values reach this destructor;start/separator/scalar values excluded. */\n' +
                  cases + '\n      xl6AstFree(yypminor->yy270);\n      yypminor->yy270 = NULL;\n      break;\n')
    source = source.replace(begin + '\n' + end, begin + destructor + end)
    compiled = output / 'frontend/xl6_parser.c'
    compiled.parent.mkdir(parents=True, exist_ok=True)
    compiled.write_text(source, encoding='utf8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=expected,
        grammarSource=str(grammar), grammarSha256=grammar_sha, compiledSource=str(compiled),
        compiledSha256=sha(compiled), ownedStackSymbols=len(owned),
        excludedControls=sorted(controls), ownedSymbols=sorted(owned),
        change='AST-valued discarded Lemon stack cleanup;original grammar/actions/tables/diagnostics retained')
