"""Run the repaired examples through the installed JIT and standalone compiler.

Clipboard checks use a private Windows window station. Redis has its own
disposable-server runner; this gate never contacts a user's Redis instance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import time
from runtime_layout import runtime_directory


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=runtime_directory(root))
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    windows = os.name == 'nt'
    xl = runtime / ('xl.exe' if windows else 'xl')
    env = dict(os.environ)
    if not windows:
        env['LD_LIBRARY_PATH'] = str(runtime) + ':' + env.get('LD_LIBRARY_PATH', '')
    report = dict(passed=False, runtime=str(runtime), inputs={}, rows=[], skipped=[])

    def sha(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    for name in (('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so')):
        path = runtime / name
        report['inputs'][str(path)] = sha(path)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command, timeout=120):
        start = time.monotonic()
        try:
            result = subprocess.run(list(map(str, command)), cwd=root, env=env,
                                    capture_output=True, timeout=timeout)
            output = result.stdout + result.stderr
            code = result.returncode
        except subprocess.TimeoutExpired as error:
            output = (error.stdout or b'') + (error.stderr or b'') + b'\nTIMEOUT\n'
            code = 124
        log = out / (name + '.log')
        log.write_bytes(output)
        report['rows'].append(dict(name=name, exitCode=code, passed=code == 0,
                                   seconds=round(time.monotonic() - start, 3),
                                   command=list(map(str, command)), logSha256=sha(log)))
        save()
        print(name, code, flush=True)
        return code == 0

    cases = [(name, root / 'test/cases' / (name + '.xl'), []) for name in (
        'cgen_class_projection_derived', 'cgen_destroy_field_assignment',
        'stdlib_channel_default_initialized', 'stdlib_object_index_context',
        'cgen_foreign_read_views')]
    cases += [(name, runtime / 'examples' / path, []) for name, path in (
        ('cli', 'extlibs/cli.xl'), ('serial', 'extlibs/serial.xl'),
        ('copy_clone', 'copy_clone.xl'), ('class_c_export', 'class_c_export.xl'),
        ('fs', 'stdlib_fs_advanced.xl'), ('concurrency', 'feature_suite/11_concurrency.xl'))]
    cases += [(name, runtime / 'projects/dev' / name / (name + '.xl'),
               ['test'] if name == 'xneural' else (['run', '3', '2', '10'] if name == 'prodcon' else []))
              for name in ('shapes', 'xneural', 'dynobj', 'deepclone', 'prodcon')]
    if windows:
        cases.append(('com', runtime / 'examples/extlibs/com.xl', []))
    else:
        report['skipped'].append(dict(name='com', reason='Windows COM API'))
    for name, source, argv in cases:
        report['inputs'][str(source)] = sha(source)
        run(name + '-jit', [xl, 'run', source, *(['--', *argv] if argv else [])])
        binary = out / (name + ('.exe' if windows else ''))
        if run(name + '-build', [xl, 'build', source, '-o', binary]):
            run(name + '-aot', [binary, *argv])
    if windows:
        runner_source = runtime / 'extlibs/clipboard/test/isolated_runner.c'
        source = runtime / 'examples/extlibs/clipboard.xl'
        report['inputs'][str(runner_source)] = sha(runner_source)
        report['inputs'][str(source)] = sha(source)
        runner = out / 'clipboard-isolated.exe'
        if run('clipboard-runner-build', ['gcc', '-O2', runner_source, '-luser32', '-o', runner]):
            run('clipboard-jit', [runner, xl, source, root, '--restored'])
            binary = out / 'clipboard.exe'
            if run('clipboard-build', [xl, 'build', source, '-o', binary]):
                run('clipboard-aot', [runner, binary, source, root, '--binary', '--restored'])
    else:
        report['skipped'].append(dict(name='clipboard', reason='Windows clipboard API'))
    report['passed'] = all(row['passed'] for row in report['rows'])
    save()
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
