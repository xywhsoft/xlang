from native_final_c_interop_profile import COHORT as FINAL_COHORT, PINS as FINAL_PINS
from native_c_interop_profile import pins_for as final_active_pins
"""Release original Lemon AST stack values during error/final stack unwind.

The pinned grammar gives every AST-valued nonterminal an explicit LHS alias.
Start/separator controls have no AST value; tokens and scalar overrides use
different union fields. No grammar action, parse table or diagnostic changes.
"""
from pathlib import Path
import hashlib
import re
from native_c_interop_profile import PINS as C_INTEROP_PINS, selected as c_interop_selected


def derive(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    from native_signature_overlay import profile
    audited = profile(frontend)
    original = frontend / 'src/parser/xl6_parser.c'
    grammar = frontend / 'src/parser/xl6_parser.y'
    profiles = {
        '3fdd30d906a26f885777a9ab05cd08df7736a52b2eef2148c6ead75950e5651e':
            '639ba6823c4dc9dffb6d9bf6a8e14fded358613a57c071ad3ddcd968eff628b7',
        # 9192a99f: same AST union tag/ownership controls; one c_vararg rule.
        'e2afa53003d34fd77de0f8fd44ec65499f8dab5ff1f971f653a5f7c32fa88f95':
            '9d82450476c4a1784f220177bfda545c34bb85878bf9203558aea23a31baefe0',
    }
    profiles[final_active_pins(frontend)['src/parser/xl6_parser.c']]=final_active_pins(frontend)['src/parser/xl6_parser.y']
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    expected = sha(original)
    if expected not in profiles or sha(grammar) != profiles[expected]:
        raise ValueError('shared Lemon grammar/parser changed; audit AST stack ownership and union tags')
    grammar_sha = profiles[expected]
    source = original.read_text(encoding='utf8')
    rules = grammar.read_text(encoding='utf8')
    assert '%default_type { XL6_AstNode* }' in rules
    c_interop=c_interop_selected(frontend)
    from native_final_c_interop_profile import selected as final_selected
    final=final_selected(frontend)
    ast_tag='yy454' if final else 'yy230' if c_interop else 'yy270'
    assert 'XL6_AstNode* '+ast_tag+';' in source
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
    existing=source.split(begin,1)[1].split(end,1)[0]
    if c_interop:
        # The new by-value contract has a separate union member/destructor.
        # Keep it byte-for-byte; never pass the composite record to AstFree.
        assert overrides['extern_func_contract'].strip()=='XL6_PExternContract'
        assert 'extern_func_contract' not in owned
        assert re.findall(r'case\s+(\d+):',existing)==[str(symbols['extern_func_contract'])]
        contract_tag='yy424' if final else 'yy166'
        assert 'xl6AstFree((yypminor->'+contract_tag+').attributes);' in existing
        # Lemon's inline finalizer skips symbols below this bound. The new
        # explicit contract destructor changed it from0 to138; newly added
        # AST destructors include import_name/target132/133. Update the range
        # too,otherwise incomplete imports leak despite correct switch cases.
        lo=min(symbols[name] for name in owned | {'extern_func_contract'})
        hi=max(symbols[name] for name in owned | {'extern_func_contract'})
        for name,value in (('YY_MIN_DSTRCTR',lo),('YY_MAX_DSTRCTR',hi)):
            old='#define '+name+'       '+('139' if final else '138')
            assert source.count(old)==1
            source=source.replace(old,'#define '+name+'       '+str(value),1)
    else:
        assert not existing.strip()
    cases = '\n'.join(f'    case {symbols[name]}: /* {name} */' for name in sorted(owned, key=symbols.get))
    destructor = ('\n    /* Original reductions transfer AST ownership. Only discarded stack\n'
                  '     * values reach this destructor;start/separator/scalar values excluded. */\n' +
                  cases + '\n      xl6AstFree(yypminor->'+ast_tag+');\n      yypminor->'+ast_tag+' = NULL;\n      break;\n')
    source = source.replace(begin + existing + end, begin + existing.rstrip('\n') + destructor + end)
    compiled = output / 'frontend/xl6_parser.c'
    compiled.parent.mkdir(parents=True, exist_ok=True)
    compiled.write_text(source, encoding='utf8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=expected,
        grammarSource=str(grammar), grammarSha256=grammar_sha, compiledSource=str(compiled),
        auditedDependencies=audited,
        compiledSha256=sha(compiled), ownedStackSymbols=len(owned),
        excludedControls=sorted(controls), ownedSymbols=sorted(owned),
        change='AST-valued discarded Lemon stack cleanup;original grammar/actions/tables/diagnostics retained')
