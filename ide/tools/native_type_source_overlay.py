"""Retain explicit origins in original synthetic type-at-node constructors.

The caller already supplies the exact AST origin and copies its line/column.
Use the existing source/range ownership helper at those audited sites. Never
infer an origin from matching coordinates or change type/overload predicates.
"""
from __future__ import annotations
import re

CONSTRUCTORS = (
    ('xl6SemNewNullableType', 'typeNode'),
    ('xl6SemTupleTypeFromReturns', 'tupleType'),
    ('xl6SemNewGenericUnaryType', 'typeNode'),
    ('xl6SemFunctionTypeFromCallableParts', 'typeNode'),
    ('xl6SemNewFutureCallbackType', 'typeNode'),
    ('xl6SemNewGenericContainerType', 'typeNode'),
    ('xl6SemOwnedSimpleType', 'typeNode'),
    ('xl6SemOwnedCurrentClassType', 'typeNode'),
)


def derive(source: str) -> str:
    for name, node in CONSTRUCTORS:
        pattern = r'static XL6_AstNode\* ' + re.escape(name) + r'\([^;{}]*\)\n\{'
        matches = list(re.finditer(pattern, source))
        if len(matches) != 1:
            raise ValueError('synthetic type origin constructor changed; audit: ' + name)
        start = matches[0].start()
        end = source.index('\n}\n', matches[0].end()) + 2
        body = source[start:end]
        old = f'    if ( at != NULL ) {{\n        {node}->line = at->line;'
        if body.count(old) != 1:
            raise ValueError('synthetic type origin assignment changed; audit: ' + name)
        body = body.replace(old, f'    if ( at != NULL ) {{\n        xl6AstCopySource({node}, at);\n        {node}->line = at->line;')
        source = source[:start] + body + source[end:]
    return source
