"""Compare matching native IDE observations, exports, and unchanged SDK headers."""
import argparse, hashlib, json
from datetime import datetime, timezone
from pathlib import Path
from PIL import Image
from check_sdk_public_compatibility import pe

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--observation', type=Path, required=True)
    parser.add_argument('--sdk-build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert not args.output.exists()
    baseline, observation, sdk = (read(p) for p in (args.baseline, args.observation, args.sdk_build))
    assert sdk['exitCode'] == 0 and sdk['sourceUnchanged']
    assert baseline['status'] == observation['status'] == 'current-native-editor-observations-passed'
    assert baseline['protectedInputsUnchanged'] and observation['protectedInputsUnchanged']
    assert baseline['probeSourceSha256'] == observation['probeSourceSha256']
    assert baseline['baseBuildReportSha256'] == observation['baseBuildReportSha256']
    assert baseline['dpiOverride'] == observation['dpiOverride'] and baseline['dpiCycle'] == observation['dpiCycle']
    before = {(c['language'], c['bytes']): c for c in baseline['cases']}
    after = {(c['language'], c['bytes']): c for c in observation['cases']}
    assert before.keys() == after.keys()
    images, times = [], []
    for key, first in before.items():
        second = after[key]
        assert first['exitCode'] == second['exitCode'] == 0 and first['images'].keys() == second['images'].keys()
        for name in first['images']:
            p, q = Path(first['root'])/name, Path(second['root'])/name
            assert sha(p) == first['images'][name] and sha(q) == second['images'][name]
            with Image.open(p) as original, Image.open(q) as changed:
                assert original.size == changed.size and original.convert('RGBA').tobytes() == changed.convert('RGBA').tobytes(), (key, name)
            images.append({'language': key[0], 'bytes': key[1], 'image': name, 'baselineSha256': sha(p),
                           'newSha256': sha(q), 'allPixelsEqual': True})
        for record, label in ((first, 'baseline'), (second, 'new')):
            metrics = [m for m in record['measurements'] if m.get('event') == 'metric']
            times.append({'language': key[0], 'bytes': key[1], 'version': label, 'metrics': metrics})
    base_dll = Path(baseline['root'])/'release/xge.dll'
    new_dll = next(Path(r['path']) for r in sdk['outputs'] if Path(r['path']).name == 'xge.dll')
    assert pe(base_dll)['exports'] == pe(new_dll)['exports']
    ide = Path(__file__).resolve().parents[1]
    old_source = ide/'build/editor-sdk-precise-coordinates-a37c7c302b4b/source'
    headers = []
    for p in sorted(old_source.rglob('*.h')):
        if 'src' in p.relative_to(old_source).parts:
            continue
        relative = p.relative_to(old_source)
        q = Path(sdk['source'])/relative
        assert sha(p) == sha(q), str(relative)
        headers.append({'path': relative.as_posix(), 'sha256': sha(p)})
    record = {'status': 'dpi-paint-pixels-and-exports-verified', 'utc': datetime.now(timezone.utc).isoformat(),
              'baseline': str(args.baseline.resolve()), 'baselineSha256': sha(args.baseline),
              'observation': str(args.observation.resolve()), 'observationSha256': sha(args.observation),
              'sdkBuild': str(args.sdk_build.resolve()), 'sdkBuildSha256': sha(args.sdk_build),
              'imagePairs': len(images), 'images': images, 'exports': len(pe(base_dll)['exports']),
              'unchangedHeaders': headers, 'metrics': times, 'benchmarkTiming': False,
              'goalStatus': 'active', 'formalRelease': False}
    args.output.write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'status': record['status'], 'imagePairs': len(images), 'exports': record['exports'], 'headers': len(headers)}), flush=True)

if __name__ == '__main__':
    main()
