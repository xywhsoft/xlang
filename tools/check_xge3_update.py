"""Accept the matched XGE update through real declarations and initialized UI.

Windows UI runs on an owned private desktop without switching the user's desktop.
On Linux, run this tool under a test display (for example xvfb-run).
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'production', 'packages', 'static-packages', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--require-call", action="append", default=[])
    args = parser.parse_args()
    root, production, packages, static, out = [p.resolve() for p in
        (args.root, args.production, args.packages, args.static_packages, args.out)]
    out.mkdir(parents=True, exist_ok=False)
    windows = os.name == 'nt'
    cli = production / ('xl.exe' if windows else 'xl')
    fixture = (args.fixture or root / 'test/cases/foreign_xge3_update_20261108.xl').resolve()
    inputs = {str(p): sha(p) for p in [Path(__file__).resolve(), fixture, cli,
        *[p for folder in (packages, static) for p in folder.rglob('*') if p.is_file()]]}
    environment = {k: v for k, v in os.environ.items() if not k.startswith('XL6_')
        and k not in ('XLANG_PACKAGE_PATH', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    environment['XLANG_PACKAGE_PATH'] = str(packages)
    report = dict(complete=False, passed=False, inputs=inputs, rows=[],
        isolation='owned private Windows desktop' if windows else 'caller supplied Linux test display')

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, env=environment, ui=False, cwd=root):
        start = time.monotonic()
        log = out / (name + '.log')
        with log.open('wb') as stream:
            if windows and ui:
                sys.path.insert(0, str(root / 'ide/tools'))
                from native_desktop import NativeDesktopProcess
                user = ctypes.WinDLL('user32', use_last_error=True)
                user.CreateDesktopW.restype = wintypes.HANDLE
                user.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR,
                    ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
                user.CloseDesktop.argtypes = [wintypes.HANDLE]
                desktop_name = 'XlangXgeAcceptance-' + uuid.uuid4().hex
                desktop = user.CreateDesktopW(desktop_name, None, None, 0, 0x10000000, None)
                if not desktop:
                    raise ctypes.WinError(ctypes.get_last_error())
                process = None
                try:
                    process = NativeDesktopProcess(list(map(str, command)), stdout=stream,
                        stderr=stream, startupinfo=desktop_name, cwd=str(cwd), env=env)
                    try:
                        code = process.wait(180)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(10)
                        raise
                finally:
                    if process is not None:
                        process.close()
                    user.CloseDesktop(desktop)
            else:
                code = subprocess.run(list(map(str, command)), stdout=stream, stderr=stream,
                    cwd=cwd, env=env, timeout=300).returncode
        report['rows'].append(dict(name=name, command=list(map(str, command)), exitCode=code,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, code, flush=True)
        if code:
            raise RuntimeError(str(log))
        return log.read_text(encoding='utf8', errors='replace')

    generated = out / 'update.c'
    run('cgen', [cli, 'cgen', fixture, '--emit', generated, '--native-linkage', 'dynamic'])
    text = generated.read_text(encoding='utf8')
    symbols = ['xgeFontLoadSystem', 'xuiCreateOwnedEx', 'xuiContextDescDefault',
        'xuiEnsureDefaultFont', 'xuiCodeFindScopeSetActiveEditor', 'xuiCodeFindScopeRefresh',
        'xuiCodeFindScopeGetActiveEditor', 'xuiCodeFindScopeGetFindWindow',
        'xuiMsgBoxShow', 'xuiFileDialogShow', *args.require_call]
    assert all(re.search(r'\b' + name + r'\s*\(', text) for name in symbols)
    assert re.search(r'->fontLoadSystem\s*:\s*NULL', text)
    assert re.search(r'\bnative_callee\s*\(', text)
    report['directNativeCalls'] = symbols + ['Proxy.fontLoadSystem']
    run('jit', [cli, 'run', fixture], ui=True)
    for package, example in [('xge', 'system_font'), ('xui', 'shared_find')]:
        run('example-' + example, [cli, 'run', packages / package / 'examples' / (example + '.xl')])
    static_env = dict(environment, XLANG_PACKAGE_PATH=str(static))
    binary = out / ('update.exe' if windows else 'update')
    run('static-build', [cli, 'build', fixture, '-o', binary, '--native-linkage', 'static'], static_env)
    if windows:
        headers = run('static-imports', [shutil.which('objdump'), '-p', binary])
        imports = re.findall(r'DLL Name:\s*(\S+)', headers)
        assert imports and not any(re.match(r'^(?:lib)?(?:xge.*|xlc?|tcc.*)\.dll$', n, re.I)
            for n in imports), imports
    else:
        headers = run('static-imports', ['readelf', '-d', binary])
        imports = re.findall(r'Shared library: \[([^\]]+)\]', headers)
        assert not any(re.match(r'lib(?:xge|xl|tcc)', n) for n in imports), imports
    report['staticImports'] = imports
    relocated = out / 'standalone'
    relocated.mkdir()
    target = relocated / binary.name
    shutil.copy2(binary, target)
    standalone = {k: v for k, v in environment.items() if k != 'XLANG_PACKAGE_PATH'}
    if windows:
        standalone['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32') + ';' + os.environ['SystemRoot']
    run('static-relocated', [target], standalone, ui=True, cwd=relocated)
    report['inputsUnchanged'] = all(sha(p) == digest for p, digest in inputs.items())
    assert report['inputsUnchanged']
    report.update(complete=True, passed=True, standaloneBinary=dict(sha256=sha(target), bytes=target.stat().st_size))
    save()


if __name__ == '__main__':
    main()
