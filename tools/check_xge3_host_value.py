"""Qualify the pointer-free HostSessionDesc value against real native headers.

Complete compiler, C11/C++11/C++17 headers, O0/O2 C++ archives, and independent
JIT/dynamic/static programs. No renderer, OS context, window or network starts.
Enabled XRT heap accounting covers 1000 direct calls, not CPU equivalence.
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
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'dynamic-packages', 'static-packages', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, compiler, build, dynamic, static, out = (v.resolve() for v in
        (a.root, a.compiler, a.compiler_report, a.dynamic_packages, a.static_packages, a.out))
    verify_full_compiler(build, compiler)
    win = os.name == 'nt'
    sdk = compiler.parent / 'include'
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    tools = {n: Path(shutil.which(n)).resolve() for n in ('gcc', 'g++', 'ar', 'objdump')}
    case = root / 'test/cases/foreign_xge3_host_value.xl'
    native, header = case.with_suffix('.c'), case.with_suffix('.h')
    cpp = case.with_name(case.stem + '_oracle.cpp')
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
        compiler, build, case, native, header, cpp, *tools.values(),
        *(compiler.parent / n for n in (('xl.dll', 'xlc.dll') if win else ('libxl.so', 'libxlc.so')))]
    for folder in (sdk, dynamic, static):
        files.extend(f for f in folder.rglob('*') if f.is_file())
    inputs = {str(f): sha(f) for f in files}
    out.mkdir(parents=True, exist_ok=False)
    report = dict(complete=False, passed=False, shippingDelivery=False, scope=__doc__,
        inputs=inputs, rows=[], programs=[], fullCompilerBuild=True, networkTests=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, packages=dynamic):
        env = {k: v for k, v in os.environ.items()
            if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
        env.update(XLANG_PACKAGE_PATH=str(packages), TEMP=str(out), TMP=str(out), TMPDIR=str(out))
        key = 'PATH' if win else 'LD_LIBRARY_PATH'
        env[key] = str(packages / '_xge_native/native') + os.pathsep + str(compiler.parent) + os.pathsep + env.get(key, '')
        command = list(map(str, command))
        start = time.monotonic()
        z = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=240)
        data = z.stdout + z.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=z.returncode,
            seconds=time.monotonic() - start, logSha256=sha(log)))
        save()
        print(name, z.returncode, flush=True)
        assert not z.returncode, log
        return data

    try:
        includes = ['--inc-path', root, '--inc-path', sdk]
        ci = ['-I' + str(f) for f in (root, sdk, *sorted(dynamic.glob('*/include')))]
        generated, public = out / 'value.c', out / 'value.h'
        run('emit', [compiler, 'cgen', case, '--emit', generated, '--emit-header', public, *includes])
        for standard, tool, language in (('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++'), ('c++17', 'g++', 'c++')):
            run('header-' + standard, [tools[tool], '-std=' + standard,
                '-Wall', '-Wextra', '-Werror', *ci, '-x', language, '-fsyntax-only', public])
        for opt in ('O0', 'O2'):
            obj, archive = out / (opt + '.o'), out / (opt + '.a')
            run('cpp-' + opt, [tools['g++'], '-std=c++17', '-' + opt,
                '-Wall', '-Wextra', '-Werror', *( [] if win else ['-fPIC']),
                '-fno-exceptions', '-fno-rtti', *ci, '-c', cpp, '-o', obj])
            run('archive-' + opt, [tools['ar'], 'rcs', archive, obj])
            for mode, packages in (('jit', dynamic), ('dynamic', dynamic), ('static', static)):
                name = opt + '-' + mode
                if mode == 'jit':
                    data = run(name, [compiler, 'run', case, '--link', archive, *includes], packages)
                else:
                    binary = out / (name + ('.exe' if win else ''))
                    run(name + '-build', [compiler, 'build', case, '--link', archive,
                        *includes, '-o', binary], packages)
                    data = run(name + '-run', [binary], packages)
                    table = run(name + '-imports', [tools['objdump'], '-p', binary], packages)
                    imported = re.findall(rb'DLL Name:\s*(\S+)', table) if win else re.findall(rb'\bNEEDED\s+(\S+)', table)
                    names = {n.lower() for n in imported}
                    assert not {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so', b'libtcc.dll', b'libtcc.so'}.intersection(names)
                    assert any(b'xge3_host' in n for n in names) == (mode == 'dynamic')
                    report['programs'].append(dict(path=str(binary), sha256=sha(binary),
                        imports=[n.decode() for n in imported]))
                assert b'HOST_SESSION_VALUE_OK: 1000 language/native calls; enabled XRT alloc/free/realloc=0' in data
        verify_full_compiler(build, compiler)
        report.update(complete=True, inputsUnchanged=all(sha(f) == h for f, h in inputs.items()))
        report['passed'] = report['inputsUnchanged']
        assert report['passed']
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
