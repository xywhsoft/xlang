"""Guarded original semantic-index allocation and type-alias scope adapters.

This admits index/type assets only; full semantic memo/IR guards remain separate.
"""
from pathlib import Path
import hashlib
from native_signature_overlay import profile as parser_profile, PROFILES as PARSER_PROFILES

NATIVE_INDEX_BYTES = 32 * 1024 * 1024

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

PROFILES = {
    "legacy": {
        "src/sem/xl6_sem_index.c": "215ffbdfe8cf2a0d54b36c76a96f5c0ff603020b7aee6868b9e819bca70b1c54",
        "src/sem/xl6_sem_index.h": "04b26faa3b6a9bc4ff1c9b986c2af3b91d9c0e9a0c9b69cec70a342e2ba0740b",
        "src/type/xl6_type_model.c": "fdf35faa5dc2366a0625317d70cc82dbe8031762fcb7c81439e99a3a8de2a051",
        "src/type/xl6_type_model.h": "a7605e810c8473c864cf408f87704b1690e78147596b87c28975b6ba928bc98c",
        "src/sem/xl6_type_registry.c": "a0c37403f28d8a29ac4589e1dd78344a0d5fa2c8051ba2ead0209b9e0a228583",
        "src/sem/xl6_type_registry.h": "f5967cce13c1d6051cf0c5c716e422e2865c69c5fd3451b39cbad95e642cd8a3",
        "src/sem/xl6_sem.c": "d4f19af32f1732126913b99717a6b1b2d38d92f343b60828786b62d2a7d8183a",
        "src/sem/xl6_sem.h": "4f42d0b56c32a0adb65eb39bca5ff09504ddc563c51c2325d87fd1c8fc39dc8f"
    },
    "current-9192a99f": {
        "src/sem/xl6_sem_index.c": "6a4c6aeecdc8fdb7fdc0b5aeb5725d85398169fa5485cf98ef7be85975e82738",
        "src/sem/xl6_sem_index.h": "04b26faa3b6a9bc4ff1c9b986c2af3b91d9c0e9a0c9b69cec70a342e2ba0740b",
        "src/type/xl6_type_model.c": "3f70de039edcef2d6d6345457e388d015610cb5dd0acde360e4e45df94db5f8a",
        "src/type/xl6_type_model.h": "4b1e9c0302a3df75fbe32956819c3e8148555bf2393276a0d5f812b3d904e612",
        "src/sem/xl6_type_registry.c": "f8be2409dc6188091d831a68df657b64b6911d07b197736796d74ce2379260ba",
        "src/sem/xl6_type_registry.h": "27f0415459b060075b8e18a677e9c05d7460412ef5d908a86dc10d519f30ccd1",
        "src/sem/xl6_sem.c": "407ca4e51cbc7889b535cbe591f7f823f59131cd179aa285073c034c6796e0b0",
        "src/sem/xl6_sem.h": "ba993a3b092f5c48bb368dc06f31eb24029563ba3bee48fffef1d05cbfa9272d",
        "src/type/xl6_constructor_flow.c": "18ff45082ca206b53334b9e8665f8a017a0210e1867f8daa81d3902f7603b761",
        "src/type/xl6_constructor_flow.h": "f25eb6e89f4d9c2d32700c818428ff4a9eb5bcbc3307d54c753de398e53a1da8",
        "src/type/xl6_native_value_profiles.inc": "1fc7d6c3a18695ab0a1fe5b14350a90f781b0c7001bf30ad1cbac3f9e5710d2b",
        "src/common/xl6_path_identity.h": "48fd7456ad348c974da20aefa87f32862daeab664a341cfa144e2817d03c2683"
    }
}

def profile(frontend: Path) -> dict[str,str]:
    audited = parser_profile(frontend)
    ast = 'src/ast/xl6_ast.h'
    cohort = next(name for name,expected in PARSER_PROFILES.items()
                  if audited[str(frontend/ast)]==expected[ast])
    expected = PROFILES.get(cohort)
    if expected and all((frontend/rel).is_file() and sha(frontend/rel)==digest for rel,digest in expected.items()):
        return {**audited, **{str(frontend/rel):digest for rel,digest in expected.items()}}
    raise ValueError('shared index/type cohort changed; audit collector/canonical/type-registry contracts')

def index(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Expose the shared collector and parameterize its limit, keeping upstream immutable."""
    original = frontend / "src/sem/xl6_sem_index.c"
    audited = profile(frontend)
    body = original.read_bytes()
    source = body.decode("utf-8").replace("\r\n", "\n")
    definition = "#define XL6_INDEX_BYTES (8u * 1024u * 1024u)"
    if source.count(definition) != 1:
        raise ValueError("shared index budget declaration changed; audit its native build overlay")
    for old, new in {
        '} XL6_IndexBuild;': '    size_t factsCapacity, sourcesCapacity;\n} XL6_IndexBuild;\n#include "analysis/frontend_index_budget.inc"',
        'if (!xl6IndexBudget(b, 1536)) return 0;': 'if (!xlsIndexRecordBudget(b)) return 0;',
        'b->count >= XL6_INDEX_NODES': 'b->count >= xlsIndexNodeLimit(b)',
        '    f = &b->facts[b->count++];': '    if (!xlsIndexReserveFact(b)) return 0;\n    f = &b->facts[b->count++];',
        '    if (b->sourceCount >= XL6_INDEX_NODES) {': '    if (!xlsIndexReserveSource(b)) {',
        '        b->error = "semantic index exceeds source identity limit";': '        if (!b->error) b->error = "semantic index exceeds source identity limit";',
    }.items():
        if source.count(old) != 1:
            raise ValueError('shared typed-index allocation seam changed: ' + old)
        source = source.replace(old, new)
    generated_root = output / "frontend"
    generated_root.mkdir(exist_ok=True)
    compiled = generated_root / "xl6_sem_index.c"
    compiled.write_bytes((source.replace(definition,
        "#ifndef XL6_INDEX_BYTES\n" + definition + "\n#endif") +
        '\n#include "analysis/frontend_index.inc"\n').encode("utf-8"))
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled),
        change="preserve shared collector; typed arenas grow within byte budget, legacy JSON retains fixed node/record limits", indexBytes=NATIVE_INDEX_BYTES,
        auditedSeams=8, nativeRecordBytes=64, legacyJsonRecordBytes=1536,
        auditedDependencies={path:digest for path,digest in audited.items() if path!=str(original)})


def types(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Filter immutable-scope alias candidates without changing alias rules."""
    original = frontend / 'src/type/xl6_type_model.c'
    audited = profile(frontend)
    body = original.read_bytes()
    source = body.decode('utf-8').replace('\r\n', '\n')
    old = 'static const XL6_AstNode* xl6TypeFindAlias(const XL6_AstNode* root, const char* name)\n{'
    if source.count(old) != 1:
        raise ValueError('shared alias lookup seam changed')
    source = source.replace(old, '#include "analysis/frontend_types.inc"\n'
        'static const XL6_AstNode* xlsTypeFindAliasOriginal(const XL6_AstNode* root, const char* name)\n{')
    compiled = output / 'frontend/xl6_type_model.c'
    compiled.write_text(source, encoding='utf-8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=hashlib.sha256(body).hexdigest(),
        compiledSource=str(compiled), compiledSha256=sha(compiled), auditedSeams=1,
        change='ordered alias candidates in explicit immutable IR scope; original alias rules retained',
        pointerBudgetBytes=8 * 1024 * 1024,
        auditedDependencies={path:digest for path,digest in audited.items() if path!=str(original)})
