"""Audited parser/signature and scanner adapters for exact frontend profiles."""
from pathlib import Path
import hashlib


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


# Audited as a set: never combine parser, grammar, AST or XRT generations.
PROFILES = {
    "legacy": {
        "src/parser/xl6_parser_api.c": "9bef77532acc35d8b7da1c4d801bcfb81221b2d99747d29609dfb9a54f0ba6f6",
        "src/parser/xl6_parser_api.h": "3f038b9a738212f95ac6bc93bc4b4be3a49d515abe47091b2e787a2a76fa9f01",
        "src/parser/xl6_parser.c": "3fdd30d906a26f885777a9ab05cd08df7736a52b2eef2148c6ead75950e5651e",
        "src/parser/xl6_parser.y": "639ba6823c4dc9dffb6d9bf6a8e14fded358613a57c071ad3ddcd968eff628b7",
        "src/lexer/xl6_lexer_gen.c": "b6473ca60b800755e4bb3f993189040673a7c251c7af53163ad42e3d1a551fe1",
        "src/lexer/xl6_literal_span.h": "3d1b3f3a71adaeb8d76910ece765f433e899190b2f153990feb9c7c6529e0db7",
        "src/ast/xl6_ast.h": "9416c3e3e37bcf244f604738f5065369967e53203b6d9663d2190a1730c3945b",
        "lib/xrt/xrt.h": "1fc1fc8aa38437bddcf0d909bcfcccdd32a14b9f2e52fbd9661d97c172bf2a31",
        "src/sem/xl6_completion.c": "384d4cf57a648900ee1b9d93df1895ba22526639f3393702891027c94053c94c",
        "src/sem/xl6_completion.h": "a6e93a6fe2caeba0dd18e0e5e14d0088a6ca00e86001cba53251d8c1cb50febc"
    },
    "current-9192a99f": {
        "src/parser/xl6_parser_api.c": "a54d4f36062caddccab5c77070c655219e2e4fd531a5710e14ae3d524a62fd13",
        "src/parser/xl6_parser_api.h": "bb389787227c49b2d4ae8155d6cb0aa65a5f9c20e8155271989e2b8f8e01bb10",
        "src/parser/xl6_parser.c": "e2afa53003d34fd77de0f8fd44ec65499f8dab5ff1f971f653a5f7c32fa88f95",
        "src/parser/xl6_parser.y": "9d82450476c4a1784f220177bfda545c34bb85878bf9203558aea23a31baefe0",
        "src/lexer/xl6_lexer_gen.c": "da5629614caad4e7afa6af3504555679c8e81f5ea20a1990865bf9bdd6d94a7f",
        "src/lexer/xl6_literal_span.h": "4d082dc3e0672dc34e76c17db1511262d15f344f47e5356ab697e823cb2d8f49",
        "src/ast/xl6_ast.h": "5ebb5cc003e51d954d02ad229307406a4990142d7813f755e0df3bbc9838fbb1",
        "lib/xrt/xrt.h": "d8c4d3cb7da7d91c35b38cdb22be516425d3cbd2ed69ddb90b59ec5dfb5714cc",
        "src/sem/xl6_completion.c": "d56f80b8b3ca93e9ec4009a56ee9235a86b66674f2c2ea174a0f5db36fdef2bf",
        "src/sem/xl6_completion.h": "cd110fcf9e8102093d83cb5d139c262ec50a89c66684d9d046089556605d3a5d"
    }
}

# Same parser/AST/allocator contracts with the audited retirement runtime.
PROFILES['current-ed78373d'] = {**PROFILES['current-9192a99f'],
    'lib/xrt/xrt.h': '05e8c00fb0eef0215f4d0c9291d240438cb37782df5de1c0e9ba8699b0dcd5eb'}


def profile(frontend: Path) -> dict[str, str]:
    actual = {rel: sha(frontend/rel) for rel in next(iter(PROFILES.values()))}
    if actual not in PROFILES.values():
        raise ValueError("shared parser profile changed or mixed; audit parser/grammar/AST/XRT and completion contracts")
    return {str(frontend/rel): digest for rel,digest in actual.items()}


def parser(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Share token finalization with an opt-in incomplete-call projection."""
    audited = profile(frontend)
    original = frontend / 'src/parser/xl6_parser_api.c'
    profiles = {
        '9bef77532acc35d8b7da1c4d801bcfb81221b2d99747d29609dfb9a54f0ba6f6':
            '3f038b9a738212f95ac6bc93bc4b4be3a49d515abe47091b2e787a2a76fa9f01',
        # 9192a99f: c_vararg/nullable function parameters and explicit type >>.
        'a54d4f36062caddccab5c77070c655219e2e4fd531a5710e14ae3d524a62fd13':
            'bb389787227c49b2d4ae8155d6cb0aa65a5f9c20e8155271989e2b8f8e01bb10',
    }
    expected = sha(original)
    header = frontend / 'src/parser/xl6_parser_api.h'
    if expected not in profiles or sha(header) != profiles[expected]:
        raise ValueError('shared parser changed; audit signature projection, token finalization and original type matcher')
    source = original.read_text(encoding='utf8').replace('\r\n', '\n')
    start = source.index('static int xl6ParserReadTokens(')
    end = source.index('\n}\n\n/* Replace exactly one', start)
    at = source.index('    /*\n     * EOF ', start)
    tail = source[at:end]
    assert tail.endswith('    return 1;')
    helper = ('static int xlsParserFinishTokens(XL6_ParseTokenStream* stream, XL6_DiagList* diags)\n{\n'
              '    XL6_Token token=stream->count ? stream->tokens[stream->count-1] : (XL6_Token){0};\n'
              '    XL6_Token eofToken,separator;\n' + tail + '\n}\n\n')
    reader = source[start:at].replace('    XL6_Token eofToken;\n    XL6_Token separator;\n', '')
    source = source[:start] + helper + reader + '    return xlsParserFinishTokens(stream,diags);' + source[end:]
    marker = 'static int xl6ParseSourceInternal('
    assert source.count(marker) == 1
    source = source.replace(marker, '#include "analysis/frontend_signature_parser.inc"\n\n' + marker)
    for old, new in {
        'ok = xl6ParserReadTokens(&lexer, &stream, &result->diags);': 'ok = xlsSignatureReadTokens(&lexer, &stream, &result->diags,source,query);',
        'else if (ok) ok = xl6ParserCompletionToken(&stream, source, query);': 'else if (ok) ok = xlsSignatureCompletionToken(&stream, source, query);',
    }.items():
        assert source.count(old) == 1, old
        source = source.replace(old,new)
    compiled = output / 'frontend/xl6_parser_api.c'
    compiled.write_text(source+'\n#include "analysis/frontend_call_context.inc"\n',encoding='utf8',newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=expected,
        compiledSource=str(compiled), compiledSha256=sha(compiled), auditedSeams=5,
        auditedDependencies={path: digest for path,digest in audited.items() if path!=str(original)},
        change='single original token finalizer; opt-in original-source signature projection; default lexer/parser rules retained',
        originalFunctionsUnchanged=False, originalFinalizerRulesUnchanged=True,
        originalFinalizerSha256=hashlib.sha256(tail.encode('utf8')).hexdigest(),
        maximumIndexBytes=64*1024*1024, maximumGenericProofTokens=512, maximumDelimiterDepth=256)


def lexer(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    audited = profile(frontend)
    original = frontend / 'src/lexer/xl6_lexer_gen.c'
    profiles = {
        'b6473ca60b800755e4bb3f993189040673a7c251c7af53163ad42e3d1a551fe1':
            '3d1b3f3a71adaeb8d76910ece765f433e899190b2f153990feb9c7c6529e0db7',
        # Byte changes are CRLF only; scanner and literal rules are identical.
        'da5629614caad4e7afa6af3504555679c8e81f5ea20a1990865bf9bdd6d94a7f':
            '4d082dc3e0672dc34e76c17db1511262d15f344f47e5356ab697e823cb2d8f49',
    }
    expected = sha(original)
    if expected not in profiles:
        raise ValueError('shared scanner changed; audit buffer lifetime,resume and original position advance')
    literal = frontend / 'src/lexer/xl6_literal_span.h'
    literal_sha = profiles[expected]
    if sha(literal) != literal_sha:
        raise ValueError('shared literal spans changed; audit signature error/interpolation recovery')
    compiled = output / 'frontend/xl6_lexer_gen.c'
    compiled.write_bytes(original.read_bytes()+b'\n#include <limits.h>\n#include "analysis/frontend_signature_lexer.inc"\n')
    return original,compiled,dict(source=str(original),sourceSha256=expected,
        compiledSource=str(compiled),compiledSha256=sha(compiled),auditedSeams=1,
        change='append scanner resume using original native position advance;original scanner unchanged',originalFunctionsUnchanged=True,
        auditedDependencies={path: digest for path,digest in audited.items() if path!=str(original)})
