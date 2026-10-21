"""Compare actual SDK17/SDK21 private viewport models using a matched SDK build."""
import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import re
import shutil
import subprocess
import time
import uuid

from native_desktop import NativeDesktopProcess
from prepare_large_source_sdk import ancestors, name, plain, read, sha

IDE = Path(__file__).resolve().parents[1]
BUNDLE = IDE / 'third_party/sdk_patches/large-source-experiment-v7'

def run(build_report, output, gcc):
    build_report = build_report.absolute()
    output = output.absolute()
    build = read(build_report)
    if build['status'] != 'sdk-build-finished' or build['exitCode'] != 0:
        raise ValueError('A completed private SDK build is required')
    source = Path(build['source']).absolute()
    for root in (source, BUNDLE, output.parent):
        ancestors(root)
    if output.exists() or output.is_relative_to(source) or output.is_relative_to(BUNDLE):
        raise ValueError('Use a new output directory outside both input trees')
    manifest = read(BUNDLE / 'patch-manifest.json')
    descriptor = BUNDLE / 'source-materials-before.json'
    if sha(descriptor) != manifest['beforeDescriptorSha256']:
        raise ValueError('Original source descriptor differs')
    inputs = {r['path']: r['sha256'] for r in read(descriptor)['files']}
    inputs.update({r['path']: r['afterSha256'] for r in manifest['changes']})
    tests = manifest['rangeTestMaterials']
    required = {'tests/range_models.h', 'tests/old_range_model.c', 'tests/new_range_model.c',
                'tests/range_patterns.inc', 'tests/range_pair_test.c'}
    if {r['path'] for r in tests} != required:
        raise ValueError('Unexpected viewport comparison inventory')
    def verify():
        for relative, digest in inputs.items():
            p = source / name(relative)
            plain(p)
            if sha(p) != digest:
                raise ValueError(f'Private source differs: {relative}')
        for row in tests:
            p = BUNDLE / name(row['path'])
            plain(p)
            if sha(p) != row['sha256']:
                raise ValueError(f'Test material differs: {row["path"]}')
        for row in build['outputs']:
            p = Path(row['path'])
            plain(p)
            if sha(p) != row['sha256']:
                raise ValueError(f'Private build output differs: {p}')
    verify()
    output.mkdir()
    for row in tests:
        shutil.copy2(BUNDLE / row['path'], output / Path(row['path']).name)
    sdk = source / 'build/ide'
    shutil.copy2(sdk / 'xge.dll', output / 'xge.dll')
    executable = output / 'range_pair_test.exe'
    args = [str(gcc), '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-unused-function',
            '-Wno-cast-function-type', '-DXGE_DLL', '-DXGE_DEBUGMODE=0', '-DXGE_XRT_PROFILE_IDE',
            '-I' + str(source / 'src'), '-I' + str(source), str(output / 'range_pair_test.c'),
            str(source / 'src/xui_unicode.c'), str(sdk / 'xge.lib'), '-lm', '-lws2_32', '-liphlpapi',
            '-lgdi32', '-luser32', '-lshell32', '-lopengl32', '-lole32', '-lwinmm', '-lavrt', '-o', str(executable)]
    report = {'status': 'running', 'root': str(output), 'buildReport': str(build_report),
              'buildReportSha256': sha(build_report), 'patchManifestSha256': sha(BUNDLE / 'patch-manifest.json'),
              'arguments': args, 'testMaterials': tests, 'sourceFiles': len(inputs),
              'runtimeOwner': 'matched SDK DLL; diagnostic program imports XRT',
              'sdkDllSha256': sha(output / 'xge.dll'), 'sdkInstalled': False, 'productionChanged': False}
    pointer = output / 'result.json'
    def save():
        pointer.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    save()
    u = ctypes.WinDLL('user32', use_last_error=True)
    u.CreateDesktopW.restype = wintypes.HANDLE
    u.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p,
                                wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    u.CloseDesktop.argtypes = [wintypes.HANDLE]
    desktop = None
    try:
        log = output / 'strict-link.log'
        with log.open('xb') as stream:
            linked = subprocess.run(args, stdout=stream, stderr=stream, timeout=120)
        report.update(linkExitCode=linked.returncode, linkLogSha256=sha(log))
        save()
        if linked.returncode != 0:
            raise RuntimeError('Strict comparison build failed; see strict-link.log')
        desktop_name = 'XlangViewportReview-' + uuid.uuid4().hex
        desktop = u.CreateDesktopW(desktop_name, None, None, 0, 0x10000000, None)
        if not desktop:
            raise ctypes.WinError(ctypes.get_last_error())
        report.update(desktop=desktop_name, neverSwitchedDesktop=True, executableSha256=sha(executable))
        log = output / 'actual-test.log'
        started = time.monotonic()
        with log.open('xb') as stream:
            process = NativeDesktopProcess([str(executable)], stream, stream, desktop_name, cwd=str(output))
            report['pid'] = process.pid
            save()
            try:
                code = process.wait(timeout=120)
            finally:
                process.close()
        report.update(exitCode=code, elapsedSeconds=round(time.monotonic() - started, 3), logSha256=sha(log))
        save()
        if code != 0:
            raise RuntimeError('Viewport comparison failed; see actual-test.log')
        summary = re.fullmatch(r'viewport pair comparison passed: (\d+) assertions; (\d+) old repeat grapheme calls; (\d+) new repeat grapheme calls; (\d+) additional private widget bytes; (\d+) normalized owned patterns\s*', log.read_text(encoding='utf8'))
        if not summary:
            raise RuntimeError('Incomplete fixture counter summary')
        fields = ('assertions', 'oldRepeatGraphemeCalls', 'newRepeatGraphemeCalls',
                  'additionalPrivateWidgetBytesThisCompiler', 'normalizedOwnedPatterns')
        counters = dict(zip(fields, map(int, summary.groups())))
        if counters != manifest['expectedRangeFixtureCounters']:
            raise RuntimeError('Unexpected fixture counters')
        report.update(counters, status='viewport-pair-equivalence-passed')
        print(log.read_text(encoding='utf8'), end='')
        return report
    except Exception as error:
        report.update(status='viewport-pair-equivalence-failed', error=str(error))
        raise
    finally:
        if desktop and not u.CloseDesktop(desktop):
            raise ctypes.WinError(ctypes.get_last_error())
        verify()
        report['inputsUnchanged'] = True
        save()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--gcc', type=Path, default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
    options = parser.parse_args()
    run(options.build_report, options.output, options.gcc)
