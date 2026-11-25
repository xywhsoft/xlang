"""Audit the entire XGE/XUI public surface, including separate Document headers.

This is a declaration inventory, not ABI, lifetime or runtime certification.
Disabled features and native lifecycle/support APIs remain in the population.
The tool only writes development reports to the requested output directory.
"""
from collections import Counter
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

from check_xge3_bindings import inventory, annotate_existing_bindings


PACK_RECORD = re.compile(r'@c_pack\(\s*(\d+)\s*,\s*"(\w+)"\s*,\s*\d+\s*,\s*\d+\s*\)')
PACK_CALLBACK = re.compile(r'@c_pack_field\(\s*(\d+)\s*,\s*"(\w+)"\s*,\s*(\d+)\s*,\s*"(callback|owned_callback)"\s*\)')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def native_snapshot_identity(native_root, path):
    """A frozen source export need not pretend to be a Git working tree."""
    path = path.resolve()
    record = json.loads(path.read_text(encoding='utf-8-sig'))
    if not record.get('complete') or not record.get('passed') or Path(record['source']).resolve() != native_root:
        raise ValueError('Native source record does not qualify this export')
    revision = record.get('engineRevision', '')
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('Native source record needs an explicit engine revision')
    pins = record.get('sourceInputs', {})
    required = {p.relative_to(native_root).as_posix() for p in (native_root / 'include').rglob('*.h')}
    required |= {'extlibs/xrt/xrt.h', 'extlibs/xrt/xrt_decl.h', 'extlibs/xrt/upstream.json'}
    if not required <= pins.keys():
        raise ValueError('Native source record omits public header/dependency inputs')
    for name, expected in pins.items():
        source = (native_root / name).resolve()
        if not source.is_relative_to(native_root) or not source.is_file() or digest(source) != expected:
            raise ValueError('Native snapshot input changed: ' + name)
    return dict(revision=revision, trackedTreeDirty=None, sourceRecord=str(path),
        sourceRecordSha256=digest(path), xrtRevision=record.get('xrtRevision'),
        overlay=record.get('overlay', {}))


def source_bindings(root):
    """Read exactly the files selected by each source package manifest."""
    functions = {}
    fields = {}
    types = {}
    constants = {}
    inputs = {}
    for package in ('xge', 'xui'):
        for manifest in sorted((root / 'lib/bindings' / package).rglob('xlang.package.json')):
            data = json.loads(manifest.read_text(encoding='utf-8-sig'))
            inputs[manifest.relative_to(root).as_posix()] = digest(manifest)
            for relative in data['artifact']['xlSources']:
                path = manifest.parent / relative
                source = path.relative_to(root).as_posix()
                inputs[source] = digest(path)
                owner = None
                native_owner = None
                for line_number, line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
                    line = line.strip()
                    where = dict(package=data['name'], source=source, line=line_number)
                    match = re.match(r'export extern "([^"]+)" (class|struct) (\w+)', line)
                    if match:
                        native = re.sub(r'\bconst\b|\*', '', match[1]).strip()
                        types.setdefault(native, []).append(dict(language=data['name'] + '.' + match[3], **where))
                        if match[2] == 'class':
                            owner, native_owner = match[3], native
                        continue
                    if line == 'end':
                        owner = native_owner = None
                        continue
                    constant = re.match(r'export const \w+ (\w+)\s*=', line)
                    if constant:
                        constants.setdefault(constant[1], []).append(dict(language=data['name'] + '.' + constant[1], **where))
                    method = re.match(r'(?:export )?extern "([^"]+)".*\b(func|get|let) (\w+)\(', line)
                    if not method or not (owner or line.startswith('export extern')):
                        continue
                    binding = dict(language=data['name'] + '.' + (owner + '.' if owner else '') + method[3], **where)
                    if '@c_field_call' in line:
                        fields.setdefault((native_owner, method[1]), []).append(binding)
                    elif not re.search(r'@c_(?:field|deref|control)\b', line):
                        functions.setdefault(method[1], []).append(binding)
                    if '@c_pack_field' in line:
                        records = dict(PACK_RECORD.findall(line))
                        for parameter, member, argument, projection in PACK_CALLBACK.findall(line):
                            if parameter not in records:
                                raise ValueError(f'Packed callback lacks its C record in {source}:{line_number}')
                            fields.setdefault((records[parameter], member), []).append(dict(binding,
                                projection=projection, cParameter=int(parameter), argument=int(argument)))
    return functions, fields, types, constants, inputs


def build_report(root, native_root, data, native_identity=None):
    functions, fields, types, constants, inputs = source_bindings(root)
    headers = data['headers'] + data['supplementaryHeaders'] + data['dependencyHeaders']
    items = []
    header_rows = []
    families = {
        'xge.h': 'XGE', 'xui.h': 'XUI', 'xui_document.h': 'Document',
        'xui_document_ui.h': 'Document UI', 'xnative_transfer.h': 'Portable transfer',
    }
    for header in headers:
        name = Path(header['header']).name
        if header['parseErrors']:
            raise ValueError('Public header parse errors: ' + name)
        counts = Counter()
        declared = Counter()
        for kind in ('functions', 'types', 'constants', 'enums', 'enumConstants', 'functionMacros', 'callableFields'):
            for entry in header[kind]:
                if kind == 'callableFields':
                    symbol = entry['record'] + '.' + entry['member']
                else:
                    symbol = entry.get('name')
                    if not symbol:
                        identity = json.dumps(entry['type'], sort_keys=True).encode()
                        symbol = '<anonymous-' + hashlib.sha256(identity).hexdigest()[:16] + '>'
                bindings = []
                if kind == 'functions':
                    bindings = functions.get(symbol, [])
                elif kind == 'types':
                    bindings = types.get(symbol, [])
                elif kind in ('constants', 'enumConstants'):
                    bindings = constants.get(symbol, [])
                elif kind == 'callableFields':
                    bindings = fields.get((entry['record'], entry['member']), [])
                item = dict(id=name + ':' + kind + ':' + symbol, family=families[name],
                            kind=kind, symbol=symbol, conditions=entry.get('conditions', []),
                            declarationPresent=bool(bindings), bindings=bindings,
                            contractReview='pending', runtimeQualification='not-certified-by-this-audit')
                if 'line' in entry:
                    item['nativeLine'] = entry['line']
                # Multiple conditional macro definitions may share a name. Keep
                # every definition, rather than overwriting a dict entry by ID.
                if 'expression' in entry:
                    item['expression'] = entry['expression']
                if 'type' in entry:
                    item['nativeTypeKind'] = entry['type']['kind']
                items.append(item)
                counts[kind] += 1
                declared[kind] += bool(bindings)
        header_rows.append(dict(header=name, family=families[name], sha256=header['sha256'],
                                population=dict(counts), declarationPresent=dict(declared)))
    public_headers = {p.name: digest(p) for p in sorted((native_root / 'include').glob('*.h'))}
    known = set(families) | {'xge_config.h', 'xge_host_xrt.h', 'xui_config.h'}
    unexpected = sorted(set(public_headers) - known)
    # A newly introduced public header cannot silently disappear from this audit.
    if unexpected:
        raise ValueError('Review newly introduced public headers: ' + ', '.join(unexpected))
    if any(digest(root / path) != expected for path, expected in inputs.items()):
        raise ValueError('Binding sources changed during the audit')
    if any(digest(native_root / 'include' / name) != row['sha256']
           for row in header_rows for name in [row['header']]):
        raise ValueError('Public headers changed during the audit')
    if native_identity is None:
        revision = subprocess.check_output(['git', '-C', str(native_root), 'rev-parse', 'HEAD'], text=True).strip()
        dirty = bool(subprocess.check_output(['git', '-C', str(native_root), 'status', '--porcelain', '--untracked-files=no'], text=True))
    else:
        revision, dirty = native_identity['revision'], native_identity['trackedTreeDirty']
    population = Counter(item['kind'] for item in items)
    present = Counter(item['kind'] for item in items if item['declarationPresent'])
    return dict(schema=1, scope=__doc__, nativeRevision=revision, nativeTrackedTreeDirty=dirty,
                nativeSourceSnapshot=native_identity,
                publicHeaderSha256=public_headers, bindingInputs=inputs, inputsUnchanged=True,
                headerRows=header_rows, population=dict(population), declarationPresent=dict(present),
                limitations=[
                    'Declaration presence does not prove ABI, ownership, errors, thread safety or execution.',
                    'An unbound native symbol can have a reviewed lifecycle/compiler replacement; this tool does not infer it.',
                    'Matching constant names do not prove their values; macros and named enums still require review.',
                    'Conditional and disabled-feature APIs remain in scope; installed SDK features are not inferred.',
                    'Types include their native record fields in native-inventory.json; no field layout acceptance is inferred.',
                ], items=items)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--native-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--native-source-record', type=Path,
                        help='Explicit engine revision and exact source/dependency hashes for a frozen non-Git export')
    parser.add_argument('--require-declarations', nargs='+', default=[], metavar='NATIVE_FUNCTION',
                        help='Fail if any named function lacks a public declaration; not a runtime completion gate')
    args = parser.parse_args()
    root, native, out = (p.resolve() for p in (args.root, args.native_root, args.out))
    identity = native_snapshot_identity(native, args.native_source_record) if args.native_source_record else None
    if out.is_relative_to(root / 'release') or out.is_relative_to(root / 'lib') or out.is_relative_to(native):
        parser.error('Reports must not overwrite runtime, binding or upstream source trees')
    out.mkdir(parents=True, exist_ok=False)
    data = inventory(native, out / 'native-inventory.json')
    annotate_existing_bindings(root, data)
    report = build_report(root, native, data, identity)
    if identity is not None and native_snapshot_identity(native, args.native_source_record) != identity:
        raise ValueError('Native source record changed during the audit')
    (out / 'surface.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    selected = {item['symbol'] for item in report['items']
                if item['kind'] == 'functions' and item['declarationPresent']}
    missing = sorted(set(args.require_declarations) - selected)
    print(json.dumps(dict(nativeRevision=report['nativeRevision'], population=report['population'],
                          declarationPresent=report['declarationPresent'], missingRequiredDeclarations=missing,
                          report=str(out / 'surface.json')), ensure_ascii=False))
    return 1 if missing else 0


if __name__ == '__main__':
    raise SystemExit(main())
