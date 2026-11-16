"""Fail closed when final-node snapshots acquire unowned native pointers.

This inspects the selected compiler headers, not a parallel IR declaration.
Only simple named fields/enums and the two audited pointer-free value records
are admitted. C compilation still supplies the actual sizes and alignment.
"""
from pathlib import Path
import hashlib
import re

OWNED = {'text': 'char', 'literalData': 'unsigned char', 'compiledPackageLink': 'char'}
SCALARS = {'char', 'unsigned char', 'int', 'unsigned int', 'size_t',
           'uint8', 'uint16', 'uint32', 'uint64', 'int8', 'int16', 'int32', 'int64',
           'XL6_IrTypeId', 'XL6_IrNodeId', 'XL6_SymbolId', 'XL6_AstKind',
           'XL6_SemCallKind', 'XL6_SemLvalueKind', 'XL6_SemExitKind'}
RECORDS = {'XL6_IrSpan', 'XL6_ForeignCallFacts'}


def clean(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)


def record(text, name):
    hits = list(re.finditer(r'\btypedef\s+struct\s+' + re.escape(name) + r'\s*\{', text))
    if len(hits) != 1:
        raise ValueError('Expected one complete native value record: ' + name)
    start = hits[0].end()
    end = text.find('}', start)
    if end < 0 or '{' in text[start:end] or not re.match(r'\s*' + re.escape(name) + r'\s*;', text[end+1:]):
        raise ValueError('Unsupported native value record declarator: ' + name)
    rows = []
    for field in text[start:end].split(';'):
        field = field.strip()
        if not field:
            continue
        tokens = re.findall(r'[A-Za-z_]\w*|\d+|\*|\[|\]', field)
        if re.sub(r'[A-Za-z_]\w*|\d+|\*|\[|\]|\s+', '', field):
            raise ValueError('Unsupported native node field: ' + field)
        extent = None
        if '[' in tokens:
            if len(tokens) < 5 or tokens[-3] != '[' or tokens[-1] != ']' or not tokens[-2].isdigit():
                raise ValueError('Unsupported native node array: ' + field)
            extent = int(tokens[-2])
            tokens = tokens[:-3]
            if not extent:
                raise ValueError('Zero native value array: ' + field)
        if len(tokens) < 2 or not re.fullmatch(r'[A-Za-z_]\w*', tokens[-1]):
            raise ValueError('Unsupported native node field: ' + field)
        name_ = tokens[-1]
        pointer = tokens[:-1].count('*')
        base = ' '.join(t for t in tokens[:-1] if t != '*')
        if not base or any(r['name'] == name_ for r in rows):
            raise ValueError('Ambiguous native node field: ' + field)
        rows.append(dict(name=name_, type=base, pointers=pointer, extent=extent))
    return rows


def validate_sources(headers):
    """headers maps actual dependency paths to UTF-8 source strings."""
    texts = {str(path): clean(text) for path, text in headers.items()}
    selected = [text for text in texts.values() if re.search(r'typedef\s+struct\s+XL6_IrNode\s*\{', text)]
    if len(selected) != 1:
        raise ValueError('Final-node graph requires one selected native IR header')
    node = record(selected[0], 'XL6_IrNode')
    enums = set()
    for text in texts.values():
        enums.update(re.findall(r'typedef\s+enum(?:\s+\w+)?\s*\{[^{}]*\}\s*(\w+)\s*;', text, flags=re.S))
    found = {}
    nested = {}
    for row in node:
        if row['pointers']:
            if row['pointers'] != 1 or row['extent'] is not None or OWNED.get(row['name']) != row['type']:
                raise ValueError('Unowned native node pointer: ' + row['name'])
            found[row['name']] = row['type']
        elif row['type'] in RECORDS:
            definitions = [text for text in texts.values() if re.search(r'typedef\s+struct\s+' + row['type'] + r'\s*\{', text)]
            if len(definitions) != 1:
                raise ValueError('Missing selected native value record: ' + row['type'])
            members = record(definitions[0], row['type'])
            if any(m['pointers'] or m['type'] not in SCALARS | enums for m in members):
                raise ValueError('Native value record contains an unaudited reference: ' + row['type'])
            nested[row['type']] = members
        elif row['type'] not in SCALARS | enums:
            raise ValueError('Unaudited native node value type: ' + row['type'])
    if not {'text', 'literalData'} <= found.keys():
        raise ValueError('Native node payload ownership changed')
    return dict(schema=1, fields=node, ownedPointers=found, valueRecords=nested,
                normalizedUtf8Headers={str(p): hashlib.sha256(text.encode('utf8')).hexdigest() for p, text in headers.items()},
                scope='Complete native node values; explicit owned payloads; non-node tables and integer-ID freshness remain caller-owned')


def validate_dependencies(paths):
    paths = [Path(p) for p in paths if Path(p).name in ('xl6_ir.h', 'xl6_foreign_call_contract.h')]
    return validate_sources({str(p): p.read_text(encoding='utf8') for p in paths})
