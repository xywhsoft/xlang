"""Derive the private native AST owner ABI from reviewed complete asset sets.

The current compiled-package schema is a representation substrate only; parser,
semantic and IR guards must be rebased separately before a current server build.
Every consumer of a selected layout must be rebuilt with its generated header.
"""
from pathlib import Path
import hashlib


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


PROFILES = {
    ('adf07bc08b6e6949fe1bb6095e373d130df205535e9e2fa1662d2cc7b4c65035',
     '9416c3e3e37bcf244f604738f5065369967e53203b6d9663d2190a1730c3945b'):
        dict(runtime='1fc1fc8aa38437bddcf0d909bcfcccdd32a14b9f2e52fbd9661d97c172bf2a31', packageFields=False),
    # 9192a99f9 current committed AST. Pool structs and all 65 pool functions
    # match the legacy audited runtime. New owned fields are copied/accounted
    # by the private adapters; this is not semantic/IR admission.
    ('b8bbafaed7b469232b73f6256282607f4644eb605fb1aa7a636bf123143cf026',
     '5ebb5cc003e51d954d02ad229307406a4990142d7813f755e0df3bbc9838fbb1'):
        dict(runtime='d8c4d3cb7da7d91c35b38cdb22be516425d3cbd2ed69ddb90b59ec5dfb5714cc', packageFields=True),
}


def derive(frontend: Path, output: Path) -> tuple[Path, Path, dict]:
    """Private node-owner ABI; every native AST consumer must be rebuilt."""
    original = frontend / 'src/ast/xl6_ast.c'
    header = frontend / 'src/ast/xl6_ast.h'
    runtime = frontend / 'lib/xrt/xrt.h'
    profile = PROFILES.get((sha(original), sha(header)))
    if profile is None:
        raise ValueError('shared AST changed; audit creation/copy/mutation/destruction and private owner layout')
    if sha(runtime) != profile['runtime']:
        raise ValueError('shared XRT changed; audit pool layout/page/index capacity charging and free lifetimes')
    source = original.read_text(encoding='utf8')
    old = '    xl6AstReleaseSource(node->source);\n    xrtFree(node);'
    if source.count(old) != 1:
        raise ValueError('shared AST free seam changed')
    source = source.replace('#include "src/ast/xl6_ast.h"', '#include "src/ast/xl6_ast.h"\n#include "analysis/ast_storage.h"')
    old_free = 'void xl6AstFree(XL6_AstNode* node)\n{\n    size_t i;\n\n    if ( node == NULL ) {\n        return;\n    }'
    if source.count(old_free) != 1:
        raise ValueError('shared AST early leaf-release seam changed')
    source = source.replace(old_free, old_free+'\n    if (xlsAstSharedLeafRelease(node)) return;')
    source = source.replace(old, '    xl6AstReleaseSource(node->source);\n'
        '    if (node->xlsStorage) xlsAstStorageFreeNode(node); else xrtFree(node);')
    text = header.read_text(encoding='utf8')
    old = '    size_t childCapacity;\n} XL6_AstNode;'
    if text.count(old) != 1:
        raise ValueError('shared AST private ownership seam changed')
    text = text.replace('#define XL6_AST_H', '#define XL6_AST_H\n#define XLS_NATIVE_AST_STORAGE 1\n#define XLS_NATIVE_AST_LEAF_REFS 1')
    text = text.replace(old, '    size_t childCapacity;\n    struct xls_ast_storage* xlsStorage;\n} XL6_AstNode;')
    if profile['packageFields']:
        text = text.replace('#define XLS_NATIVE_AST_STORAGE 1', '#define XLS_NATIVE_AST_STORAGE 1\n#define XLS_NATIVE_AST_PACKAGE_FIELDS 1')
    identity = '    uint32 semanticTypeId;\n    uint64 semanticTypeAbiId;'
    if text.count(identity) != 1:
        raise ValueError('shared AST private leaf-reference seam changed')
    # Occupy the existing uint32-to-uint64 alignment hole on 64-bit targets.
    # Native consumers still rebuild; private refs never come from old objects.
    text = text.replace(identity, '    uint32 semanticTypeId;\n    uint32_t xlsLeafReferences;\n    uint64 semanticTypeAbiId;')
    from native_raw_body_overlay import sized_ast
    source = sized_ast(source)
    compiled = output / 'frontend/xl6_ast.c'
    compiled_header = output / 'frontend/src/ast/xl6_ast.h'
    compiled_header.parent.mkdir(parents=True, exist_ok=True)
    compiled.write_text(source, encoding='utf8', newline='\n')
    compiled_header.write_text(text, encoding='utf8', newline='\n')
    return original, compiled, dict(source=str(original), sourceSha256=sha(original),
        compiledSource=str(compiled), compiledSha256=sha(compiled), headerSource=str(header),
        headerSourceSha256=sha(header), compiledHeader=str(compiled_header), compiledHeaderSha256=sha(compiled_header),
        runtimeSource=str(runtime), runtimeSourceSha256=sha(runtime), auditedSeams=6,
        astPackageFields=profile['packageFields'],
        change='private storage owner plus within-forest bare fact-type leaf edge references;mechanically derived counted source-binding helper with complete generic fallback;original source functions unchanged;all actual native consumers closure-verified')

