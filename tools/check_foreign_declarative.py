"""Batch the implemented declarative-interop gates against one source build.

This validates compiler capabilities; it does not certify the XGE/XUI packages,
managed native ownership, retained callbacks, or shipping runtime products.
"""
from pathlib import Path
from runtime_layout import runtime_directory
import argparse, hashlib, json, os, subprocess, sys, time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'driver', 'source-build', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.out.resolve()
    build = json.loads(args.source_build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and Path(build['Driver']).resolve() == driver
    out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root))
    if os.name != 'nt':
        env['LD_LIBRARY_PATH'] = str(runtime_directory(root))+':'+env.get('LD_LIBRARY_PATH', '')
    report = dict(complete=False, scope=__doc__, sourceBuild=str(args.source_build.resolve()),
                  sourceBuildSha256=hashlib.sha256(args.source_build.read_bytes()).hexdigest(), stages=[])

    def run(name, command):
        start = time.monotonic()
        try:
            process = subprocess.run(list(map(str, command)), cwd=root, env=env, capture_output=True, timeout=900)
            code, output = process.returncode, process.stdout+process.stderr
        except (OSError, subprocess.TimeoutExpired) as error:
            code, output = None, str(error).encode('utf-8')
        log = out/(name+'.log'); log.write_bytes(output)
        report['stages'].append(dict(name=name, exitCode=code, command=list(map(str, command)),
            seconds=round(time.monotonic()-start, 3), log=str(log), sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        print(name, code, flush=True)
        return code == 0

    for gate in ('declarations', 'records', 'arrays', 'factories', 'callbacks', 'forwarding'):
        command = [sys.executable, '-B', root/('tools/check_foreign_'+gate+'.py'),
                   '--root', root, '--driver', driver, '--out', out/gate]
        if gate in ('arrays', 'factories', 'callbacks', 'forwarding'):
            command += ['--source-build', args.source_build.resolve()]
        run(gate, command)
    run('initialization', [sys.executable, '-B', root/'tools/check_native_initialization.py',
        '--root', root, '--driver', driver, '--out', out/'initialization'])
    for mode in ('run', 'module', 'exe'):
        run('constructor-alias-'+mode, [driver, root/'test/cases/foreign_decl_constructor_alias.xl',
            mode, out/('constructor-alias-'+mode)])
    run('constructor-alias-standalone', [out/('constructor-alias-exe.exe' if os.name == 'nt' else 'constructor-alias-exe')])
    for gate in ('text_views', 'bytes_views', 'bound_views','managed_views','expanded_calls','terminated_strings'):
        run(gate, [sys.executable, '-B', root/('tools/check_foreign_'+gate+'.py'),
            '--root', root, '--public-directory', args.source_build.resolve().parent, '--out', out/gate])
    report['complete'] = True
    report['passed'] = all(row['exitCode'] == 0 for row in report['stages'])
    (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    if not report['passed']:
        raise SystemExit('Declarative interop batch failed; inspect '+str(out/'results.json'))


if __name__ == '__main__':
    main()
