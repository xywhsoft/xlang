"""Mutate returned-int output contracts in a genuine all-source IR client.

This gate does not qualify embedded VFS or shipping binaries. Use the companion
complete-compiler gate for those; no compiler object overrides are accepted.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time
from foreign_validation_build import load_build
from compiler_response import command_with_response


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, build, out = (v.resolve() for v in (a.root, a.build, a.out))
    b = load_build(build=build)
    assert b['fullSourceRebuild'] and not b['overlay']
    source = b['cohort']
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    freeze = source.parent / 'freeze.json'
    frozen = json.loads(freeze.read_text(encoding='utf8'))
    changes = {n: h for n, h in frozen['overrides'].items() if n.startswith('src/')}
    assert len(changes) == 14
    for name, digest in changes.items():
        assert sha(root / name) == digest == sha(source / name), name
    case = root / 'test/cases/foreign_return_outputs.xl'
    client = case.with_name(case.stem + '_ir.c')
    cc = Path(shutil.which('gcc')).resolve()
    inputs = dict(b['inputs'])
    inputs.update((str(f), sha(f)) for f in (Path(__file__),
        Path(__file__).with_name('compiler_response.py'), freeze, cc, case, client,
        *(root / n for n in changes)))
    out.mkdir(parents=True, exist_ok=False)
    win = os.name == 'nt'
    runtime = Path(b['base']['TccCore'])
    inputs[str(runtime)] = sha(runtime)
    env = dict(os.environ, PYTHONUTF8='1', PYTHONDONTWRITEBYTECODE='1')
    key = 'PATH' if win else 'LD_LIBRARY_PATH'
    env[key] = str(runtime.parent) + os.pathsep + env.get(key, '')
    system = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if win else [
        '-Wl,--export-dynamic', '-Wl,-rpath,' + str(runtime.parent), '-pthread', '-ldl', '-lm']
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
        fullSourceRebuild=True, objectOverrides=False, shippingLibraryAcceptance=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command):
        command = list(map(str, command))
        actual = command_with_response(command, cc, out / (name + '.rsp'))
        start = time.monotonic()
        r = subprocess.run(actual, cwd=source, env=env, capture_output=True, timeout=180)
        log = out / (name + '.log')
        log.write_bytes(r.stdout + r.stderr)
        report['rows'].append(dict(name=name, exitCode=r.returncode,
            command=command, seconds=time.monotonic() - start, logSha256=sha(log)))
        save()
        print(name, r.returncode, flush=True)
        assert not r.returncode, log
        return r.stdout + r.stderr

    try:
        for opt in ('O0', 'O2'):
            binary = out / (opt + ('.exe' if win else ''))
            run(opt + '-build', [cc, *b['base']['CompileFlags'], '-' + opt,
                client, *b['objects'], runtime, *system, '-o', binary])
            result = run(opt + '-run', [binary, case])
            assert b'FOREIGN_RETURN_OUTPUT_IR_OK targets=8 rows=21 mutations=135 zeroOutput=270' in result
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
