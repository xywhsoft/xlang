"""Prepare the ten IDE source replacements for the private checkpoint SDK."""
import argparse
import json
from pathlib import Path
import re
import shutil

from prepare_large_source_sdk import ancestors, name, plain, read, sha

IDE = Path(__file__).resolve().parents[1]
BUNDLE = IDE / 'third_party/sdk_patches/large-source-experiment-v17'

def prepare(source_root, output):
    source_root = source_root.absolute()
    output = output.absolute()
    ancestors(source_root)
    ancestors(BUNDLE)
    ancestors(output.parent)
    if output.exists() or output.is_relative_to(source_root/'src') or output.is_relative_to(BUNDLE):
        raise ValueError('Use a new output directory outside src and the patch tree')
    manifest = read(BUNDLE / 'patch-manifest.json')
    descriptor = BUNDLE / 'app-source-materials-before.json'
    if not manifest['privateOnly'] or sha(descriptor) != manifest['appBeforeDescriptorSha256']:
        raise ValueError('Unexpected private application patch')
    before = read(descriptor)
    expected = {row['path']: row['sha256'] for row in before['files']}
    changes = manifest['appChanges']
    if len(expected) != 10 or len(changes) != 10 or {r['path'] for r in changes} != set(expected):
        raise ValueError('Unexpected application source inventory')
    names = set()
    for row in changes:
        relative = name(row['path'])
        if relative.casefold() in names:
            raise ValueError('Duplicate application source')
        names.add(relative.casefold())
        original = source_root / relative
        ancestors(original.parent)
        plain(original)
        payload = BUNDLE / name(row['material'])
        ancestors(payload.parent)
        if expected[relative] != row['beforeSha256'] or sha(original) != row['beforeSha256']:
            raise ValueError('Application source changed: ' + relative)
        if plain(payload).st_size != row['size'] or sha(payload) != row['afterSha256']:
            raise ValueError('Application payload changed: ' + relative)
    # Detect newly added call sites before producing an incomplete overlay.
    constructors = []
    for original in (source_root / 'src').rglob('*.c'):
        ancestors(original.parent)
        plain(original)
        if re.search(r'\bxuiCodeEditCreate\s*\(', original.read_text(encoding='utf-8-sig')):
            constructors.append(original.relative_to(source_root).as_posix())
    if sorted(constructors) != sorted(before['constructorSourceFiles']):
        raise ValueError('IDE editor constructor inventory changed')
    output.mkdir()
    for row in changes:
        target = output / row['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(BUNDLE / row['material'], target)
    for row in changes:
        if sha(source_root / row['path']) != row['beforeSha256'] or sha(output / row['path']) != row['afterSha256']:
            raise ValueError('Application source changed while copying')
    report = dict(status='private-layout-stream-application-overlay-prepared', sourceRoot=str(source_root),
                  output=str(output), patchManifestSha256=sha(BUNDLE/'patch-manifest.json'),
                  files=[dict(path=r['path'], sha256=r['afterSha256']) for r in changes],
                  constructorSourceFiles=constructors, overlayOnly=True, inputUnchanged=True,
                  built=False, appInstalled=False, productionChanged=False)
    (output / 'app-preparation-report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    return report

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=IDE)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    result = prepare(options.source_root, options.output)
    print(json.dumps(dict(status=result['status'], files=len(result['files']), output=result['output'])))
