"""Rebuild only the CLI shell over a verified complete compiler runtime.

Prove the previous failed-entry busy-owner hang, bounded failed exits, preserved
normal/nonzero exits and unchanged successful wait selection. Does not rebuild
compiler/runtime/VFS or install a release; no graphics or network tests.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from full_compiler_qualification import verify_full_compiler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for n in ('root', 'compiler', 'compiler-report', 'out'):
        p.add_argument('--' + n, type=Path, required=True)
    a = p.parse_args()
    root, compiler, build, out = [v.resolve() for v in (a.root, a.compiler, a.compiler_report, a.out)]
    metadata = verify_full_compiler(build, compiler)
    snapshot = Path(metadata['source'])
    windows = os.name == 'nt'
    sha = lambda v: hashlib.sha256(Path(v).read_bytes()).hexdigest()
    cc = Path(shutil.which('gcc')).resolve()
    files = [Path(__file__).resolve(), Path(__file__).with_name('full_compiler_qualification.py'),
        compiler, build, cc, snapshot / 'tools/stage_runtime_headers.py',
        snapshot / 'src/common/xl6_console.c', root / 'src/driver/xl6_xlc_shell.c']
    files += [root / 'test/cases' / n for n in
        ('cli_busy_error.xl', 'cli_busy_error.h', 'cli_busy_error_native.c', 'cli_retire_policy.c')]
    products = ('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so')
    files += [compiler.parent / n for n in products]
    inputs = {str(v): sha(v) for v in files}
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(TMP=str(out), TEMP=str(out), TMPDIR=str(out), PYTHONUTF8='1', PYTHONDONTWRITEBYTECODE='1')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
        compilerRuntimeRebuilt=False, shippingDelivery=False, networkTests=False, graphicsTests=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command, expected=0, marker=None, timeout=120, should_timeout=False, cwd=None, environment=None):
        command = list(map(str, command))
        start = time.monotonic()
        expired = False
        try:
            result = subprocess.run(command, cwd=cwd or out, env=environment or env,
                capture_output=True, timeout=timeout)
            code = result.returncode
            data = result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            expired = True
            code = None
            data = (error.stdout or b'') + (error.stderr or b'')
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=code, timedOut=expired,
            expectedTimeout=should_timeout, expectedExit=expected, seconds=time.monotonic() - start,
            logSha256=sha(log)))
        save()
        print(name, 'timeout' if expired else code, flush=True)
        assert expired == should_timeout, log
        if not expired:
            assert code == expected, log
        if marker:
            assert marker in data, log
        return data

    sdk = out / 'sdk'
    run('stage-private-headers', [sys.executable, '-B', snapshot / 'tools/stage_runtime_headers.py',
        '--root', snapshot, '--out', sdk])
    includes = out / 'include'
    shutil.copytree(compiler.parent / 'include', includes)
    shutil.copytree(sdk / 'include', includes, dirs_exist_ok=True)
    inputs.update((str(v), sha(v)) for v in includes.rglob('*') if v.is_file())
    cli_source = out / 'xl6_xlc_shell.c'
    shutil.copy2(root / 'src/driver/xl6_xlc_shell.c', cli_source)
    inputs[str(cli_source)] = sha(cli_source)
    source = root / 'test/cases/cli_busy_error.xl'
    native = root / 'test/cases/cli_busy_error_native.c'
    inc = ['--inc-path', root, '--inc-path', includes]
    controls = {}
    for name, body in (
        ('success', 'println("CLI_SUCCESS_OK")\n return 0'),
        ('nonzero', 'println("CLI_NONZERO_OK")\n return 7'),
        ('ordinary-error', 'throw ValueError "ordinary CLI failure"\n return 0')):
        path = out / (name + '.xl')
        path.write_text('func main(...) as int\n ' + body + '\nend\n')
        controls[name] = path
        inputs[str(path)] = sha(path)
    # The immutable baseline remains unmodified; it reads the exact same test
    # headers/native owner as the patched shell. A timeout is the expected red.
    run('baseline-busy-error', [compiler, 'run', source, native, *inc],
        timeout=3, should_timeout=True)
    for opt in ('O0', 'O2'):
        runtime = out / opt
        runtime.mkdir()
        for n in products[1:]:
            shutil.copy2(compiler.parent / n, runtime / n)
        # Literal include/... paths in the shell need the SDK parent too.
        flags = ['-std=c11', '-Wall', '-' + opt, '-ffunction-sections', '-fdata-sections',
            '-Wl,--gc-sections', '-I' + str(snapshot), '-I' + str(out), '-I' + str(includes)]
        systems = ['-lShell32'] if windows else ['-D_GNU_SOURCE', '-pthread', '-ldl', '-lm', '-Wl,-rpath,$ORIGIN']
        library = runtime / products[-1]
        cli = runtime / products[0]
        run(opt + '-shell-build', [cc, *flags, cli_source, snapshot / 'src/common/xl6_console.c',
            library, *systems, '-o', cli])
        candidate_env = dict(env)
        key = 'PATH' if windows else 'LD_LIBRARY_PATH'
        candidate_env[key] = str(runtime) + os.pathsep + candidate_env.get(key, '')
        test = runtime / ('policy.exe' if windows else 'policy')
        run(opt + '-policy-build', [cc, '-I' + str(root), *flags, '-DXL_STATIC_EMBED',
            root / 'test/cases/cli_retire_policy.c', snapshot / 'src/common/xl6_console.c',
            library, *systems, '-o', test])
        run(opt + '-policy', [test], marker=b'CLI_RETIRE_POLICY_OK', environment=candidate_env)
        for turn in range(2):
            data = run(opt + '-busy-error-' + str(turn), [cli, 'run', source, native, *inc],
                expected=1, marker=b'CLI primary failure', timeout=10, environment=candidate_env)
            assert b'runtime retirement incomplete or failed' in data
        for name, code, marker in (('success', 0, b'CLI_SUCCESS_OK'),
            ('nonzero', 7, b'CLI_NONZERO_OK'), ('ordinary-error', 1, b'ordinary CLI failure')):
            data = run(opt + '-' + name, [cli, 'run', controls[name], *inc], expected=code,
                marker=marker, timeout=10, environment=candidate_env)
            assert b'runtime retirement incomplete or failed' not in data
        assert all(sha(runtime / n) == metadata['products'][n]['sha256'] for n in products[1:])
    verify_full_compiler(build, compiler)
    report.update(complete=True, passed=True, inputsUnchanged=all(sha(v) == h for v, h in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
