"""Specialize the audited checker's exact NULL-token type constructor.

Keep the native copy body, ownership, allocation order and failure behavior.
The independent test oracle is extracted from the original compiler asset.
"""
from pathlib import Path
import hashlib


def clone(source):
    start = source.index('static XL6_AstNode* xl6SemCloneTypeNode(const XL6_AstNode* node)\n{')
    end = source.index('\nstatic int xl6SemIsContainerBaseTypeName(', start)
    return source[start:end]


def derive(frontend: Path, output: Path, source: str, current: bool):
    original = clone((frontend/'src/sem/xl6_sem.c').read_text('utf8'))
    actual = clone(source)
    if actual != original:
        raise ValueError('native type-copy body changed before constructor specialization')
    oracle = output/'frontend/type_constructor_reference.inc'
    oracle.write_text(original.replace('xl6SemCloneTypeNode', 'xlsTypeCloneReference'),
                      encoding='utf8', newline='\n')
    old = '''    if ( node->text != NULL ) {
        copy = xl6AstNewText(node->kind, node->text, strlen(node->text), NULL);
    } else {
        copy = xl6AstNew(node->kind, NULL);
    }
'''
    new = '''    /* Exact native NULL-token construction: both ranges and all other
     * fields are already zero. Retain the original allocation/text-failure
     * behavior; the remaining native copy body is unchanged. */
    copy = xrtCalloc(1, sizeof(*copy));
    if ( copy != NULL ) {
        copy->kind = node->kind;
        if ( node->text != NULL ) {
            copy->text = xrtStrDupN(node->text, strlen(node->text));
        }
    }
'''
    if actual.count(old) != 1:
        raise ValueError('native NULL-token type constructor seam changed; audit')
    if current:
        source = source.replace(actual, actual.replace(old, new, 1), 1)
    return source, dict(enabled=current, originalBodySha256=hashlib.sha256(original.encode()).hexdigest(),
        oracle=str(oracle), oracleSha256=hashlib.sha256(oracle.read_bytes()).hexdigest(),
        contract='same XRT calloc and text allocation order; ordinary independent heap nodes; original copy/source/children and failure behavior; no pooling,sharing or memo admission changes')
