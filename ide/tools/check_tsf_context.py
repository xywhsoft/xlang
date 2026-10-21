"""Exercise Windows TSF against the IDE objects from an unchanged strict build."""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

from native_desktop import NativeDesktopProcess


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='A new, empty destination; previous evidence is never overwritten.')
    parser.add_argument('--gcc', type=Path, default=Path(r'E:\software\w64devkit\bin\gcc.exe'))
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This test requires Windows TSF and an owned Win32 desktop.')
    output = args.output.resolve()
    require(not output.exists(), 'Output already exists; choose a new evidence directory.')
    output.mkdir(parents=True)
    record: dict = {
        'status': 'failed', 'utc': datetime.now(timezone.utc).isoformat(),
        'output': str(output), 'physicalImeVerified': False,
        'candidatePagingVerified': False, 'mixedMonitorDpiVerified': False,
        'syntheticInputPosting': False, 'inputProfileActivationPerformed': False,
        'neverSwitchedDesktop': True, 'protected': {}, 'commands': [],
    }
    protected = record['protected']
    process = None
    desktop = None
    user = None
    code = 1
    try:
        script = Path(__file__).resolve()
        report_path = args.build_report.resolve()
        require(report_path.is_file(), 'Strict build report is missing.')
        report = json.loads(report_path.read_text(encoding='utf8'))
        require(report.get('exitCode') == 0 and report.get('configuration') == 'Release', 'An actual successful Release build is required.')
        require(not report.get('currentSourceChangesDuringBuild'), 'Strict build had changing source inputs.')
        source = Path(report['sourceRoot']).resolve()
        build = Path(report['root']).resolve()
        sdk = source / 'third_party/xge_sdk'
        fixture = script.parent.parent / 'tests/tsf_context_test.c'
        for path in (script, script.parent/'native_desktop.py', report_path, fixture, args.gcc.resolve()):
            protected[str(path)] = sha(path)
        inputs = report['inputs']
        require(bool(inputs) and bool(report.get('objects')), 'Strict source/object inventory is missing.')
        for row in inputs:
            path = (source/row['path']).resolve()
            require(path.is_relative_to(source), 'Source inventory path escapes its root.')
            require(path.is_file() and sha(path) == row['sha256'], 'Strict build source has changed: '+str(path))
            protected[str(path)] = row['sha256']
            copied = (build/row['path']).resolve()
            require(copied.is_relative_to(build) and copied.is_file() and sha(copied) == row['sha256'], 'Strict source snapshot has changed: '+str(copied))
            protected[str(copied)] = row['sha256']
        objects = []
        for relative, digest in report['objects'].items():
            path = (build/relative).resolve()
            require(path.is_relative_to(build) and path.name.endswith('.c.o'), 'Invalid strict object path.')
            require(path.is_file() and sha(path) == digest, 'Strict build object has changed: '+str(path))
            protected[str(path)] = digest
            objects.append(path)
        for relative, digest in report['outputs'].items():
            path = (build/relative).resolve()
            require(path.is_relative_to(build), 'Strict output path escapes its root.')
            require(path.is_file() and sha(path) == digest, 'Strict build output has changed: '+str(path))
            protected[str(path)] = digest
        require(sha(sdk/'sdk_manifest.json') == report['sdkManifestSha256'], 'SDK manifest differs from the strict build.')
        sdk_files = sorted(path for path in sdk.rglob('*') if path.is_file())
        for path in sdk_files:
            protected[str(path)] = sha(path)
        require(sha(build/'release/xge.dll') == sha(sdk/'xge.dll'), 'Strict build DLL differs from the SDK.')
        shutil.copytree(build/'release/data', output/'data')
        shutil.copy2(sdk/'xge.dll', output/'xge.dll')
        shutil.copy2(fixture, output/'tsf_context_test.c')
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
            '-DXGE_DLL', '-DXUI_DLL', '-DXGE_XRT_PROFILE_IDE', '-DXGE_DEBUGMODE=0',
            '-DXIDE_STREAM_LAYOUT_ENABLED=1', '-DXIDE_CURSOR_LAYOUT_ENABLED=1',
            '-DXIDE_PAINT_CURSOR_ENABLED=1', '-finput-charset=UTF-8', '-fexec-charset=UTF-8', '-O2',
            '-I'+str(build/'src'), '-I'+str(sdk), '-I'+str(sdk/'lib/xrt')]
        libraries = ['-lm', '-lws2_32', '-lwinhttp', '-liphlpapi', '-lgdi32', '-luser32',
            '-lshell32', '-lopengl32', '-lole32', '-lwinmm', '-lavrt', '-lcomdlg32',
            '-lversion', '-lbcrypt', '-ladvapi32', '-luuid']
        exe = output/'tsf_context_test.exe'
        arguments = [str(args.gcc.resolve()), *flags, str(output/'tsf_context_test.c'),
            *[str(path) for path in objects], str(sdk/'xge.lib'), *libraries, '-o', str(exe)]
        began = time.monotonic()
        with (output/'compile.stdout').open('xb') as out, (output/'compile.stderr').open('xb') as err:
            result = subprocess.run(arguments, stdout=out, stderr=err, timeout=60)
        record['commands'].append(dict(kind='compile', arguments=arguments, exitCode=result.returncode,
            elapsedSeconds=round(time.monotonic()-began,3), stdout=str(output/'compile.stdout'),
            stdoutSha256=sha(output/'compile.stdout'), stderr=str(output/'compile.stderr'), stderrSha256=sha(output/'compile.stderr')))
        require(result.returncode == 0, 'Strict compilation failed; see compile.stderr.')
        user = ctypes.WinDLL('user32', use_last_error=True)
        user.CreateDesktopW.restype = wintypes.HANDLE
        user.CreateDesktopW.argtypes = [wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p]
        user.CloseDesktop.argtypes = [wintypes.HANDLE]
        name = 'XlangTsfContext-'+uuid.uuid4().hex
        desktop = user.CreateDesktopW(name, None, None, 0, 0x10000000, None)
        require(bool(desktop), 'Could not create the owned private desktop.')
        began = time.monotonic()
        with (output/'run.log').open('xb') as stream:
            process = NativeDesktopProcess([str(exe)], stream, stream, name, cwd=str(output))
            timed_out = False
            while process.poll() is None:
                if time.monotonic()-began > 45:
                    timed_out = True
                    process.kill()
                time.sleep(.1)
            native_code = process.wait()
        record['commands'].append(dict(kind='native-tsf', arguments=[str(exe)], exitCode=native_code,
            elapsedSeconds=round(time.monotonic()-began,3), pid=process.pid, timedOut=timed_out,
            log=str(output/'run.log'), logSha256=sha(output/'run.log')))
        record.update(strictInputs=len(inputs), strictObjects=len(objects), strictOutputs=len(report['outputs']),
            sdkFiles=len(sdk_files), exeSha256=sha(exe), fixtureSha256=sha(output/'tsf_context_test.c'))
        require(native_code == 0 and not timed_out, 'The actual TSF test failed; see run.log.')
        record['status'] = 'passed'
        code = 0
    except Exception as error:
        record['error'] = str(error)
    finally:
        if process:
            process.close()
        if desktop and user and not user.CloseDesktop(desktop):
            record['desktopCloseError'] = ctypes.get_last_error()
            code = 1
        changes = [path for path, digest in protected.items() if not Path(path).is_file() or sha(Path(path)) != digest]
        record['protectedUnchanged'] = not changes
        if changes:
            record['changedInputs'] = changes
            code = 1
        if code:
            record['status'] = 'failed'
        record['finishedUtc'] = datetime.now(timezone.utc).isoformat()
        record['exitCode'] = code
        (output/'evidence.json').write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n',encoding='utf8')
    print(json.dumps({key:value for key,value in record.items() if key not in ('commands','protected')},ensure_ascii=False))
    return code


if __name__ == '__main__':
    raise SystemExit(main())
