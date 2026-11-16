"""Verify actual private VFS staging uses the active platform's SDK bytes."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--require-negative-control', action='store_true')
    a = parser.parse_args()
    root, out = a.root.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(root / 'tools'))
    spec = importlib.util.spec_from_file_location('candidate_runtime_headers', root / 'tools/stage_runtime_headers.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    platform = 'win_x64' if os.name == 'nt' else 'linux_x64'
    other = 'linux_x64' if os.name == 'nt' else 'win_x64'
    sdk = root / 'release' / platform
    metadata = root / 'lib/xrt/xrt.snapshot.json'
    expected = json.loads(metadata.read_text())['declarations']['sha256'].lower()
    assert sha(sdk / 'include/xrt_decl.h') == expected
    inputs = {str(p): sha(p) for p in (Path(__file__), metadata,
        root / 'tools/stage_runtime_headers.py', root / 'tools/gen_tcc_vfs_resources.py',
        root / 'tools/runtime_layout.py', root / 'tools/tcc_vfs_manifest_win64_xlc.txt',
        root / 'tools/tcc_vfs_manifest_linux_xlc.txt', root / 'tools/tcc_vfs_manifest_linux_runtime.txt')}
    # Exercise the real default selection, not a test-only replacement loader.
    rows = module.stage(root, out / 'active')
    release_rows = [r for r in rows if Path(r['Source']).is_relative_to(root / 'release')]
    assert release_rows and all(Path(r['Source']).is_relative_to(sdk) for r in release_rows)
    assert sha(out / 'active/include/__xl_runtime/xrt_decl.h') == expected
    inputs.update((r['Source'], r['Sha256']) for r in rows)
    inactive = root / 'release' / other / 'include/xrt_decl.h'
    negative = dict(exercised=False)
    if inactive.is_file() and sha(inactive) != expected:
        # The inherited cohort deliberately still has an older SDK for the
        # other target. Explicitly choosing it must fail the same byte gate.
        wrong = 'tcc_vfs_manifest_linux_xlc.txt' if os.name == 'nt' else 'tcc_vfs_manifest_win64_xlc.txt'
        wrong_rows = module.stage(root, out / 'inactive', root / 'tools' / wrong)
        actual = sha(out / 'inactive/include/__xl_runtime/xrt_decl.h')
        assert actual == sha(inactive) and actual != expected
        inputs.update((r['Source'], r['Sha256']) for r in wrong_rows)
        negative = dict(exercised=True, incorrectSha256=actual, requiredSha256=expected,
            rejected=True, manifest=wrong)
    elif not inactive.exists():
        wrong = 'tcc_vfs_manifest_linux_xlc.txt' if os.name == 'nt' else 'tcc_vfs_manifest_win64_xlc.txt'
        try:
            module.stage(root, out / 'inactive', root / 'tools' / wrong)
        except RuntimeError as error:
            assert 'missing VFS resource' in str(error) and other in str(error)
            negative = dict(exercised=True, missingInactiveSdk=True, rejected=True, manifest=wrong)
        else:
            raise AssertionError('Incorrect-target SDK selection unexpectedly succeeded')
    if a.require_negative_control:
        assert negative['exercised']
    assert all(sha(p) == value for p, value in inputs.items())
    report = dict(complete=True, passed=True, platform=platform, privateHeaders=len(rows),
        sdkSources=len(release_rows), xrtDeclarationSha256=expected, negativeControl=negative,
        inputs=inputs, inputsUnchanged=True, shippingDelivery=False, networkTests=False)
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print('ACTIVE_RUNTIME_SDK_RESOURCES_OK', platform, len(rows), 'negative', negative['exercised'])


if __name__ == '__main__':
    main()
