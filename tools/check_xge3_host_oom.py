"""Owned GL resolver factory failure prefixes and a precisely bounded heap probe.

Use a previously qualified real OS-only host fixture and matched complete O2
compiler/SDK candidates. No network, hardware timing or shipping installation.
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
from full_compiler_qualification import verify_full_compiler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'dynamic-packages',
                 'static-packages', 'dynamic-report', 'static-report', 'public-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    a = parser.parse_args()
    root, compiler, build, dynamic, static, dynamic_report, static_report, public, out = (
        p.resolve() for p in (a.root, a.compiler, a.compiler_report, a.dynamic_packages,
            a.static_packages, a.dynamic_report, a.static_report, a.public_report, a.out))
    verify_full_compiler(build, compiler)
    win = os.name == 'nt'
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    cc = Path(shutil.which('gcc')).resolve()
    objdump = Path(shutil.which('objdump')).resolve()
    parent = json.loads(public.read_text(encoding='utf8'))
    assert parent['complete'] and parent['passed'] and parent['inputsUnchanged']
    assert all(sha(p) == h for p, h in parent['inputs'].items())
    fixture = public.parent / ('probe.dll' if win else 'probe.so')
    archive = public.parent / 'probe.a'
    identity = next(p for p in parent['programs'] if p['fixture'])
    assert sha(fixture) == identity['sha256']
    source = root / 'test/cases/foreign_xge3_host_oom.xl'
    header = root / 'test/cases/foreign_xge3_host_memory.h'
    sdk = public.parent / 'include'
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
        cc, objdump, source, header, root / 'test/cases/foreign_xge3_host_probe.h',
        public, fixture, archive, build, dynamic_report, static_report]
    for name in (('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')):
        files.append(compiler.parent / name)
    for folder, receipt in ((dynamic, dynamic_report), (static, static_report)):
        stage = json.loads(receipt.read_text(encoding='utf8'))
        assert stage['complete'] and not stage['shippingDelivery']
        assert all(sha(p) == h for p, h in stage['inputs'].items())
        assert all(sha(folder / n) == h for n, h in stage['outputs'].items())
        files.extend(p for p in folder.rglob('*') if p.is_file())
    files.extend(p for p in sdk.rglob('*') if p.is_file())
    inputs = {str(p): sha(p) for p in files}
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(PYTHONUTF8='1', PYTHONDONTWRITEBYTECODE='1', LIBGL_ALWAYS_SOFTWARE='1',
        TMP=str(out), TEMP=str(out), TMPDIR=str(out))
    key = 'PATH' if win else 'LD_LIBRARY_PATH'
    env[key] = str(compiler.parent) + os.pathsep + str(public.parent) + os.pathsep + str(dynamic / '_xge_native/native') + \
        os.pathsep + env.get(key, '')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
        programs=[], failureSweeps=[], networkTests=False, shippingDelivery=False,
        actualGraphics=True, heapProbe='after native graphics-init to successful language factory return')

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, packages=None, sweep=False):
        execution = dict(env)
        if packages:
            execution['XLANG_PACKAGE_PATH'] = str(packages)
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, env=execution, cwd=out, capture_output=True, timeout=180)
        data = result.stdout + result.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert not result.returncode, log
        if sweep:
            match = re.search(rb'PUBLIC_HOST_RESOLVER_OOM_OK prefixes=\s*(\d+)', data)
            assert match and 0 < int(match[1]) < 512, log
            report['failureSweeps'].append(dict(name=name, failedPrefixes=int(match[1]),
                successfulSentinel=True, exactLiveHeapBalance=True, resolverHeapOperations=0))
            save()
        return data

    inc = ['--inc-path', root, '--inc-path', sdk]
    try:
        for backend in (('gl',) if win else ('gl', 'gles')):
            case = out / (backend + '.xl')
            text = source.read_text(encoding='utf8')
            if backend == 'gles':
                text = text.replace('c_int backend=xge.XGE_GPU_BACKEND_OPENGL33',
                    'c_int backend=xge.XGE_GPU_BACKEND_GLES30')
            case.write_text(text, encoding='utf8')
            inputs[str(case)] = sha(case)
            if backend == 'gl':
                generated = out / 'emitted.c'
                run('cgen', [compiler, 'cgen', case, '--emit', generated, *inc], dynamic)
                for opt in ('-O0', '-O2'):
                    run('abi' + opt, [cc, '-std=c11', opt, '-fsyntax-only', '-Werror',
                        '-I' + str(root), '-I' + str(sdk), '-I' + str(dynamic / 'xge/include'),
                        '-I' + str(dynamic / '_xge_native/include'), generated])
            run(backend + '-jit', [compiler, 'run', case, *inc, '--loadlib', fixture], dynamic, True)
            for label, selected in (('dynamic', dynamic), ('static', static)):
                binary = out / (backend + '-' + label + ('.exe' if win else ''))
                flags = ['--loadlib', fixture] if label == 'dynamic' else [
                    '--link', archive, '--link', 'opengl32' if win else 'EGL']
                run(backend + '-' + label + '-build', [compiler, 'build', case, *inc,
                    '--native-linkage', label, *flags, '-o', binary], selected)
                run(backend + '-' + label + '-run', [binary], sweep=True)
                raw = run(backend + '-' + label + '-imports', [objdump, '-p', binary])
                imports = re.findall(rb'DLL Name:\s*(\S+)', raw) if win else re.findall(rb'NEEDED\s+(\S+)', raw)
                imports = [n.decode().lower() for n in imports]
                assert not set(imports) & {'xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so', 'libtcc.dll', 'libtcc.so'}
                if label == 'static':
                    assert not any('xge' in n for n in imports)
                report['programs'].append(dict(path=str(binary), sha256=sha(binary), imports=imports))
        verify_full_compiler(build, compiler)
        report.update(complete=True, passed=True, inputsUnchanged=all(sha(p) == h for p, h in inputs.items()))
        assert report['inputsUnchanged']
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
