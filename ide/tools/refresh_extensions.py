"""Refresh extension commands and source snapshots without replacing frozen stdlib data."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

from catalog_extensions import extract
from build_catalog_exporter import build as build_exporter
from catalog_sources import create, plain, sha, verify

IDE = Path(__file__).resolve().parents[1]


def input_files(root):
    files = set()
    for manifest in sorted((root / 'release/extlibs').glob('*/xlang.package.json')):
        files.add(manifest)
        data = json.loads(manifest.read_text(encoding='utf-8-sig'))
        if data.get('kind') == 'demo6-private-extlib' or data['name'].startswith('_'):
            continue
        for name in data.get('artifact', {}).get('xlSources', []):
            file = manifest.parent / name
            if not file.resolve().is_relative_to(manifest.parent.resolve()):
                raise ValueError('Extension source escapes package: ' + str(file))
            files.add(file)
        files.update(p for p in manifest.parent.iterdir() if p.name.lower() == 'readme.md')
    return sorted(files)


def fingerprint(root):
    result = {}
    for path in input_files(root):
        plain(path)
        result[path.relative_to(root).as_posix()] = sha(path)
    return result


def tools():
    return {name: sha(IDE / 'tools' / name) for name in
            ('refresh_extensions.py', 'catalog_extensions.py', 'catalog_sources.py',
             'build_catalog_exporter.py', 'dump_extension_catalog.c')}


def check(root, data):
    verify(data / 'catalog-sources', data / 'commands.json')
    catalog = json.loads((data / 'commands.json').read_text(encoding='utf-8-sig'))
    expected = catalog.get('extensionCatalog', {})
    current = fingerprint(root)
    frontend = expected.get('frontendInputs', {})
    frontend_changed = [p for p, value in frontend.items() if not (root / p).is_file() or sha(root / p) != value]
    if expected.get('inputs') != current or expected.get('tools') != tools() or not frontend or frontend_changed:
        old = expected.get('inputs', {})
        changed = sorted(k for k in old.keys() | current.keys() if old.get(k) != current.get(k))
        raise ValueError('IDE extension catalog is stale; refresh_extensions.py is required. Changed inputs: ' + ', '.join(changed + frontend_changed))
    return dict(status='current', packages=catalog['coverage']['packages'], extensionEntries=catalog['coverage']['categories']['extension'])


def generate(root, data, output, gcc=None):
    if output.exists():
        raise ValueError('Output directory already exists: ' + str(output))
    verified = verify(data / 'catalog-sources', data / 'commands.json')
    if verified['unavailable']:
        raise ValueError('Base catalog sources are incomplete')
    base_bytes = (data / 'commands.json').read_bytes()
    base = json.loads(base_bytes.decode('utf-8-sig'))
    before, tool_hashes = fingerprint(root), tools()
    output.mkdir(parents=True)
    scratch = output / 'scratch'
    scratch.mkdir()
    tool_snapshot = output / 'snapshot/ide/tools'
    tool_snapshot.mkdir(parents=True)
    for name, value in tool_hashes.items():
        raw = (IDE / 'tools' / name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != value:
            raise ValueError('Extraction tool changed before capture: ' + name)
        (tool_snapshot / name).write_bytes(raw)
    compiler, frontend = build_exporter(root, output / 'frontend', gcc)
    compiler_hash = sha(compiler)
    retained = [e for e in base['entries'] if e['category'] != 'extension']
    documents = [d for d in base['documents'] if not d['id'].startswith('package:')]
    declared = {s['path']: s for s in base['sources'] if '/release/extlibs/' not in s['path'] and not s['path'].startswith('release/extlibs/')}
    materials = {}
    for row in json.loads((data / 'catalog-sources/index.json').read_text(encoding='utf-8'))['entries']:
        if row['path'] in declared:
            materials[row['path']] = (data / 'catalog-sources' / (row['sha256'] + '.source')).read_bytes()

    def capture(path):
        key = path.relative_to(root).as_posix()
        if key not in before:
            raise ValueError('Unexpected extension input: ' + key)
        if key not in materials:
            plain(path)
            raw = path.read_bytes()
            if hashlib.sha256(raw).hexdigest() != before[key]:
                raise ValueError('Extension input changed before capture: ' + key)
            target = output / 'snapshot' / key
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(raw)
            materials[key] = raw
            declared[key] = dict(path=key, sha256=before[key])
        return key, materials[key]

    entries, extra_docs, coverage = extract(root / 'release/extlibs', compiler, scratch / 'declarations', capture)
    runtime_copy = compiler.parent / ('xlc.dll' if sys.platform == 'win32' else 'libxlc.so')
    if fingerprint(root) != before or sha(compiler) != compiler_hash or sha(runtime_copy) != frontend['runtimeSha256'] or tools() != tool_hashes or any(sha(Path(p)) != value for p, value in frontend['inputs'].items()):
        raise ValueError('Extension inputs, compiler or extraction tools changed during generation')
    if (data / 'commands.json').read_bytes() != base_bytes:
        raise ValueError('Installed catalog changed during generation')
    catalog = dict(base)
    catalog.update(entries=retained + entries, documents=documents + extra_docs,
                   sources=sorted(declared.values(), key=lambda r: r['path']))
    catalog['coverage'] = dict(base['coverage'], packages=len(coverage['packages']),
                               categories=dict(Counter(e['category'] for e in catalog['entries'])), issues=[])
    frontend_inputs = {Path(p).relative_to(root).as_posix(): value for p, value in frontend['inputs'].items() if Path(p).is_relative_to(root)}
    stable_base = dict(entries=retained, documents=documents,
                       sources=[s for s in base['sources'] if '/release/extlibs/' not in s['path'] and not s['path'].startswith('release/extlibs/')])
    catalog['extensionCatalog'] = dict(format='xlang-ide-extension-snapshot-1', exporterSha256=compiler_hash,
        baseNonExtensionSha256=hashlib.sha256(json.dumps(stable_base, ensure_ascii=False, sort_keys=True).encode()).hexdigest(), tools=tool_hashes, inputs=before,
        frontendInputs=frontend_inputs, runtimeSha256=frontend['runtimeSha256'], **coverage)
    target = output / 'commands.json'
    target.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    bundle = create(target, materials, output / 'catalog-sources')
    old_ids = {e['id'] for e in base['entries'] if e['category'] == 'extension'}
    new_ids = {e['id'] for e in entries}
    report = dict(status='passed', catalogSha256=sha(target), sourceIndexSha256=sha(output / 'catalog-sources/index.json'),
        preservedNonExtensionEntries=len(retained), publicPackages=len(coverage['packages']),
        excludedPrivate=coverage['excludedPrivate'], extensionEntries=len(entries),
        addedEntries=len(new_ids - old_ids), removedEntries=len(old_ids - new_ids),
        documents=len(catalog['documents']), availableSources=bundle['available'],
        extensionExporterSha256=compiler_hash, inputsUnchanged=True)
    (output / 'generation-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=IDE.parent)
    parser.add_argument('--data', type=Path, default=IDE / 'data')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--gcc')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root, data = args.root.resolve(), args.data.resolve()
    try:
        if args.check:
            result = check(root, data)
        else:
            if not args.output:
                parser.error('--output is required for generation')
            result = generate(root, data, args.output.resolve(), args.gcc)
    except (OSError, ValueError, KeyError, IndexError, StopIteration) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
