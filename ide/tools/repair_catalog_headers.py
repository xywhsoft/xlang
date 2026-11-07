"""Re-extract extension headers from a catalog's captured, hash-verified sources.

This repairs presentation data without upgrading installed packages or claiming
that an old compiler binding has been revalidated against the current workspace.
The exporter and its runtime must match the original extraction cohort exactly.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import shutil

from catalog_extensions import extract
from catalog_sources import sha, verify


def repair(pair, exporter, output):
    pair, exporter, output = Path(pair).resolve(), Path(exporter).resolve(), Path(output).resolve()
    if output.exists():
        raise ValueError('Use a fresh output directory')
    verified = verify(pair/'catalog-sources', pair/'commands.json')
    if verified['unavailable']:
        raise ValueError('Captured sources must be complete')
    before = json.loads((pair/'commands.json').read_text(encoding='utf-8-sig'))
    metadata = before['extensionCatalog']
    runtime = exporter.parent / ('xlc.dll' if exporter.suffix == '.exe' else 'libxlc.so')
    if sha(exporter) != metadata['exporterSha256'] or sha(runtime) != metadata['runtimeSha256']:
        raise ValueError('Exporter/runtime do not match the catalog extraction cohort')
    output.mkdir(parents=True)
    stage = output/'captured'
    index = json.loads((pair/'catalog-sources/index.json').read_text(encoding='utf-8'))
    captured = {}
    for row in index['entries']:
        key = row['path']
        if key not in metadata['inputs']:
            continue
        target = stage/key
        if not target.resolve().is_relative_to(stage.resolve()):
            raise ValueError('Captured extension path escapes snapshot')
        payload = pair/'catalog-sources'/(row['sha256']+'.source')
        if sha(payload) != metadata['inputs'][key]:
            raise ValueError('Captured extension source differs from original inputs: '+key)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(payload, target)
        captured[key] = target.read_bytes()

    def capture(path):
        key = path.relative_to(stage).as_posix()
        return key, captured[key]

    # The source paths, not the current installation layout, define this root.
    roots = {str(Path(key).parent.parent) for key in captured if key.endswith('/xlang.package.json')}
    if len(roots) != 1:
        raise ValueError('Captured packages have no single extension root')
    entries, documents, coverage = extract(stage/roots.pop(), exporter, output/'asts', capture)
    original_entries = [row for row in before['entries'] if row['category'] == 'extension']
    original_docs = [row for row in before['documents'] if row['id'].startswith('package:')]
    if len(entries) != len(original_entries) or documents != original_docs:
        raise ValueError('Header repair unexpectedly changes entry count or documents')
    if set(coverage['packages']) != set(metadata['packages']):
        raise ValueError('Header repair unexpectedly changes public package coverage')
    if len({row['id'] for row in entries}) != len(entries):
        raise ValueError('Re-extracted command identities are not unique')
    changes = []
    for old, new in zip(original_entries, entries):
        if old == new:
            continue
        fields = sorted(key for key in old.keys() | new.keys() if old.get(key) != new.get(key))
        if (set(fields) - {'id', 'name', 'signature', 'insert', 'sourceLine'} or
                any(old[key] != new[key] for key in ['source', 'kind', 'module'])):
            raise ValueError('Change outside declaration header repair: '+old['name'])
        changes.append(dict(before=old['name'], after=new['name'], fields=fields,
                            beforeSignature=old['signature'], afterSignature=new['signature']))
    types = [row for row in entries if row['kind'] == 'type']
    if any('\t' in row['signature'] or '\n' in row['signature'] or '\r' in row['signature'] for row in types):
        raise ValueError('Type header still contains body indentation/newlines')

    catalog = dict(before)
    iterator = iter(entries)
    catalog['entries'] = [next(iterator) if row['category'] == 'extension' else row for row in before['entries']]
    catalog['extensionCatalog'] = dict(metadata)
    catalog['extensionCatalog']['headerRepair'] = dict(
        format='xlang-catalog-header-repair-1', baseCatalogSha256=sha(pair/'commands.json'),
        extractorSha256=sha(Path(__file__).with_name('catalog_extensions.py')),
        driverSha256=sha(Path(__file__)), exporterSha256=sha(exporter), runtimeSha256=sha(runtime),
        capturedSourcesReused=True, currentWorkspaceCompilerBindingRevalidated=False,
        changedEntries=len(changes))
    catalog['coverage'] = dict(before['coverage'], categories=dict(Counter(row['category'] for row in catalog['entries'])))
    target = output/'pair'
    target.mkdir()
    (target/'commands.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8', newline='\n')
    shutil.copytree(pair/'catalog-sources', target/'catalog-sources')
    index['catalogSha256'] = sha(target/'commands.json')
    (target/'catalog-sources/index.json').write_text(json.dumps(index, ensure_ascii=False, indent=2)+'\n', encoding='utf-8', newline='\n')
    verified = verify(target/'catalog-sources', target/'commands.json')
    if sha(pair/'commands.json') != catalog['extensionCatalog']['headerRepair']['baseCatalogSha256']:
        raise ValueError('Original catalog changed during extraction')
    report = dict(status='passed', entries=len(catalog['entries']), extensionEntries=len(entries),
                  auditedTypeHeaders=len(types), changedEntries=len(changes), changes=changes,
                  availableSources=verified['available'], sourcePayloadsUnchanged=True,
                  nonExtensionEntriesUnchanged=True, documentsUnchanged=True,
                  originalCompilerBindingRetained=True,
                  catalogSha256=sha(target/'commands.json'), sourceIndexSha256=sha(target/'catalog-sources/index.json'))
    (output/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pair', type=Path, required=True)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = repair(args.pair, args.exporter, args.output)
    print(json.dumps({key: value for key, value in report.items() if key != 'changes'}, ensure_ascii=False))


if __name__ == '__main__':
    main()
