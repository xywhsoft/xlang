"""Compile public XGE/XUI examples through normal package imports; never run them.

Uses a verified complete source compiler and an explicit immutable package/SDK
candidate. Cgen, lowered IR and strict native-header ABI compilation do not
certify GUI behavior, native initialization, OOM/lifetime execution, a final
TCC/VFS provider or shipping delivery. Legacy release examples are audited
read-only and are never included in the passing count.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build


EXAMPLES = (
    ('xge', 'headless_frame', 'headless', ('xgeSetFrameCallbackOwned', 'xgeFrameCallbackDispatchChecked')),
    ('xge', 'headless_shape', 'headless', ('xgeShapeExCreate', 'xgeShapeExClone')),
    ('xge', 'gui_shapes', 'gui', ('xgeRunOwned', 'xgeShapeRectFill', 'xgeShapeCircleFill')),
    ('xui', 'headless_path', 'headless', ('xuiPathCreate', 'xuiPathLineTo')),
    ('xui', 'headless_cell', 'headless', ('xuiTableViewCellOutputCreate', 'xuiTableViewCellOutputSetValue')),
    ('xui', 'context_graph', 'headless', ('xuiCreateOwned', 'xuiButtonSetClickOwned', 'xuiWidgetDestroyChecked')),
    ('xui', 'manager_lifetime', 'headless', ('xuiCreateOwned', 'xuiMsgBoxCreate', 'xuiMsgTipSetCloseOwned')),
    ('xui', 'gui_surface_draw', 'gui', ('xgeRunOwned', 'native_callee_receiver->surfaceCreateRGBA',
                                     'native_callee_receiver->drawBegin', 'native_callee_receiver->drawRectFill')),
)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'cohort-root', 'public-directory', 'packages', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, cohort, public, packages, out = [p.resolve() for p in
        (args.root, args.cohort_root, args.public_directory, args.packages, args.out)]
    build, pinned, compared = verify_public_build(cohort, public)
    driver = Path(build['Driver'])
    sdk = packages / '_xge_native' / 'include'
    assert (sdk / 'xge3_runtime.h').is_file()
    cc, cxx = shutil.which('gcc'), shutil.which('g++')
    assert cc and cxx
    # Public package sources are an independently frozen candidate. Refuse a
    # silent fallback to the compiler cohort's older package snapshots.
    for package in ('xge', 'xui'):
        manifest = json.loads((packages / package / 'xlang.package.json').read_text())
        assert manifest['version'] == '3.0.0' and manifest['abiVersion'] == '2'
        for relative in manifest['artifact']['xlSources']:
            assert (packages / package / relative).is_file()
    pinned.update([Path(__file__).resolve(), Path(cc), Path(cxx)])
    pinned.update(p for p in packages.rglob('*') if p.is_file())
    examples = [(p, name, category, targets, root / 'lib/bindings' / p / 'examples' / (name + '.xl'))
                for p, name, category, targets in EXAMPLES]
    for package, _, _, _, source in examples:
        assert source.read_bytes() == (packages / package / 'examples' / source.name).read_bytes(), \
            'The package candidate must contain the exact example being compiled'
    pinned.update(source for _, _, _, _, source in examples)
    legacy = []
    for package in ('xge', 'xui'):
        directory = root / 'release/win_x64/examples' / package
        for source in sorted(directory.glob('*.xl')):
            text = source.read_text(encoding='utf-8-sig')
            markers = [marker for marker in ('XgeApp', 'XuiApp', 'xge.Shape.', 'xge.Color.',
                        '.rootHandle()', 'point ctx', 'point font') if marker in text]
            legacy.append(dict(path=str(source), sha256=sha(source), oldApiMarkers=markers,
                               compiled=False, executed=False))
            pinned.add(source)
    inputs = {str(p): sha(p) for p in pinned}
    out.mkdir(parents=True, exist_ok=False)
    (out / 'tmp').mkdir()
    env = dict(os.environ, XLANG_PACKAGE_PATH=str(packages),
               XL6_SOURCE_NATIVE_ROOT=str(cohort if os.name == 'nt' else Path(build['NativeRoot'])),
               TEMP=str(out / 'tmp'), TMP=str(out / 'tmp'), TMPDIR=str(out / 'tmp'))
    report = dict(complete=False, passed=False, scope=__doc__, compilerBuild=str(public / 'build.json'),
                  compilerBuildSha256=sha(public / 'build.json'), fullSourceObjects=len(build['Objects']),
                  sourceFilesCompared=len(compared), execution=False, gpuTests=False, windowTests=False,
                  browserTests=False, nativeRuntimeAccepted=False, providerAccepted=False,
                  shippingDelivery=False, packages=str(packages), inputs=inputs, rows=[],
                  examples=[], legacyReleaseExamples=legacy)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command):
        command = command_with_response(list(map(str, command)), cc, out / (name + '.rsp'))
        start = time.monotonic()
        result = subprocess.run(command, cwd=cohort, env=env, capture_output=True, timeout=240)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
                                  seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert result.returncode == 0, str(log)

    base = ['-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED']
    if os.name == 'nt':
        base += ['-DXGE_DLL', '-DXUI_DLL']
    compiler_sdk = Path(build.get('RuntimeDirectory', build.get('SdkDirectory', cohort / 'release/win_x64')))
    base += ['-I' + str(p) for p in (sdk, cohort, public / 'include', compiler_sdk / 'include')]
    header = out / 'sdk_header.c'
    header.write_text('#include <xge3_runtime.h>\n', encoding='utf-8')
    run('header-c11', [cc, '-std=c11', *base, '-fsyntax-only', header])
    for standard in ('c++11', 'c++17'):
        run('header-' + standard, [cxx, '-x', 'c++', '-std=' + standard, *base, '-fsyntax-only', header])
    # Generated C contains deliberate reusable runtime helpers; suppression is
    # limited to unused items and GCC's known callback-signature thunk warning.
    generated_flags = [*base, *['-Wno-' + name for name in ('unused-function', 'unused-variable',
                       'unused-label', 'unused-const-variable', 'unused-but-set-variable', 'cast-function-type')]]
    for package, name, category, targets, source in examples:
        tag = package + '-' + name
        generated, ir = out / (tag + '.c'), out / (tag + '.ir.json')
        run(tag + '-cgen', [driver, source, 'cgen-default', generated])
        run(tag + '-ir', [driver, source, 'ir-lowered', ir])
        text = generated.read_text(encoding='utf-8-sig')
        assert 'xl6Xge3RuntimeBindV1(' in text, 'Normal imports must execute the common native initializer'
        for target in targets:
            assert target in text, (tag, target)
        assert not re.search(r'\b(?:XuiApp|XgeApp)\b', text)
        for optimization in ('O0', 'O2'):
            run(tag + '-' + optimization + '-abi', [cc, '-std=c11', '-' + optimization,
                *generated_flags, '-fsyntax-only', generated])
        report['examples'].append(dict(package=package, name=name, category=category,
            sourceSha256=sha(source), generatedSha256=sha(generated), irSha256=sha(ir),
            normalPackageImports=True, directNativeTargets=list(targets), cgen=True, abiCompiled=True,
            executed=False))
        save()
    report['inputsUnchanged'] = all(sha(path) == digest for path, digest in inputs.items())
    assert report['inputsUnchanged']
    report.update(complete=True, passed=True)
    save()


if __name__ == '__main__':
    main()
