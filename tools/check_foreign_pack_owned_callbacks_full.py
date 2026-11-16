"""Actual complete O2 compiler/VFS qualification for owned callback-group packs.

Assemble matching private SDK headers without changing the immutable build;
exercise JIT/static/shared and a standalone nested compiler. CPU-only.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from full_compiler_qualification import verify_full_compiler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, compiler, build, out = [v.resolve() for v in (a.root, a.compiler, a.compiler_report, a.out)]
    metadata = verify_full_compiler(build, compiler)
    snapshot = Path(metadata['source'])
    sha = lambda v: hashlib.sha256(Path(v).read_bytes()).hexdigest()
    win = os.name == 'nt'
    objdump = Path(shutil.which('objdump')).resolve()
    stage = snapshot / 'tools/stage_runtime_headers.py'
    inputs = {str(v): sha(v) for v in (compiler, build, Path(__file__), objdump,
              Path(__file__).with_name('full_compiler_qualification.py'), stage)}
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(TMP=str(out), TEMP=str(out), TMPDIR=str(out), XLANG_PACK_GROUP_ROOT=str(root),
               XLANG_PACK_GROUP_TEMP=str(out / 'nested-temps'),
               PYTHONUTF8='1', PYTHONDONTWRITEBYTECODE='1')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
                  fullCompilerVfsBuild=True, shippingDelivery=False, networkTests=False, programs=[])

    def save():
        temp = out / 'results.json.tmp'
        temp.write_text(json.dumps(report, indent=2) + '\n')
        os.replace(temp, out / 'results.json')

    def run(name, command, marker=None, working=None):
        command = list(map(str, command))
        start = time.monotonic()
        r = subprocess.run(command, cwd=working or out, env=env, capture_output=True, timeout=600)
        log = out / (name + '.log')
        log.write_bytes(r.stdout + r.stderr)
        report['rows'].append(dict(name=name, command=command, exitCode=r.returncode,
                                   logSha256=sha(log), seconds=time.monotonic() - start))
        save()
        print(name, r.returncode, flush=True)
        assert not r.returncode, log
        if marker is not None:
            assert marker in r.stdout + r.stderr, log
        return r.stdout + r.stderr

    def imports(name, path):
        raw = run(name + '-imports', [objdump, '-p', path]).lower()
        names = (re.findall(rb'dll name:\s*(\S+)', raw) if win else re.findall(rb'\bneeded\s+(\S+)', raw))
        assert not {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so', b'libtcc.dll', b'libtcc.so'}.intersection(names)
        report['programs'].append(dict(path=str(path), sha256=sha(path), bytes=path.stat().st_size,
                                       imports=[v.decode() for v in names]))

    try:
        headers = out / 'sdk'
        run('stage-headers', [sys.executable, '-B', stage, '--root', snapshot, '--out', headers])
        sdk = compiler.parent / 'include'
        files = [v for v in sdk.rglob('*') if v.is_file()]
        inputs.update((str(v), sha(v)) for v in files)
        execution = out / 'runtime'
        shutil.copytree(sdk, execution / 'include')
        shutil.copytree(headers / 'include', execution / 'include', dirs_exist_ok=True)
        for name in names:
            inputs[str(compiler.parent / name)] = sha(compiler.parent / name)
            shutil.copy2(compiler.parent / name, execution / name)
        compiler = execution / compiler.name
        verify_full_compiler(build, compiler)
        inputs.update((str(v), sha(v)) for v in execution.rglob('*') if v.is_file())
        if not win:
            env['LD_LIBRARY_PATH'] = str(execution) + ':' + env.get('LD_LIBRARY_PATH', '')
        include = ['--inc-path', root, '--inc-path', execution / 'include']
        for stem, provider, marker in (
            ('foreign_pack_owned_callback', 'foreign_pack_owned_callback_native.c', b'PACK_OWNED_CALLBACK_OK'),
            ('closure_native_scalar_cells', None, b'NATIVE_SCALAR_CELLS_OK'),
            ('foreign_pack_owned_callback_nested', None, b'PACK_GROUP_NESTED_OK')):
            case = root / 'test/cases' / (stem + '.xl')
            inputs[str(case)] = sha(case)
            native = [root / 'test/cases' / provider] if provider else []
            for v in [*native, *([case.with_suffix('.h')] if case.with_suffix('.h').exists() else [])]:
                inputs[str(v)] = sha(v)
            working = root if stem.endswith('_nested') else out
            run(stem + '-jit', [compiler, 'run', case, *native, *include], marker, working=working)
            binary = out / (stem + ('.exe' if win else ''))
            run(stem + '-build', [compiler, 'build', case, *native, '-o', binary, *include])
            run(stem + '-run', [binary], marker, working=working)
            imports(stem, binary)
            if stem.endswith('_nested'):
                continue
            shared_source = out / (stem + '-shared.xl')
            shared_source.write_text(case.read_text().replace('func main(...) as int',
                                      'export "pack_check" func check() as int'))
            inputs[str(shared_source)] = sha(shared_source)
            library = out / (stem + ('.dll' if win else '.so'))
            run(stem + '-shared-build', [compiler, 'build', shared_source, *native, '--shared', '-o', library, *include])
            host = out / (stem + '-host.py')
            host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\n'
                            'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\n'
                            'assert lib.xl_script_module_init(None)==1\n'
                            'lib.pack_check.restype=ctypes.c_int64\n'
                            'assert lib.pack_check()==0 and lib.pack_check()==0\n'
                            'assert lib.xl_script_module_unit_checked()==1\n')
            inputs[str(host)] = sha(host)
            run(stem + '-shared-run', [sys.executable, '-B', host, library], marker)
            imports(stem + '-shared', library)
        verify_full_compiler(build, compiler)
        report.update(complete=True, inputsUnchanged=all(sha(v) == digest for v, digest in inputs.items()))
        report['passed'] = report['inputsUnchanged']
        assert report['passed']
    except Exception as error:
        report.update(complete=True, passed=False, error=str(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
