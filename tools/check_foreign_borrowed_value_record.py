"""Qualify borrowed xvalue fields in native output records with a full compiler."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

from foreign_compiler_cohort import load_compiler_cohort
from stage_runtime_headers import stage as stage_runtime_headers


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, compiler, build, out = (
        item.resolve() for item in (args.root, args.compiler, args.compiler_report, args.out))
    sdk, additional = load_compiler_cohort(root, compiler, build)
    windows = os.name == 'nt'
    runtime = compiler.parent / ('xl.dll' if windows else 'libxl.so')
    fixture = root / 'test/cases/foreign_borrowed_value_record.xl'
    header, native = fixture.with_suffix('.h'), fixture.with_suffix('.c')
    host = root / 'test/cases/foreign_decl_module_host.c'
    metadata = json.loads(build.read_text('utf8'))
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    tools = {name: Path(shutil.which(name)).resolve() for name in ('gcc', 'g++', 'objdump')}
    out.mkdir(parents=True)
    scratch = out / 'tmp'
    scratch.mkdir()
    fresh = out / 'runtime-sdk'
    frozen = Path(metadata['source'])
    stage_runtime_headers(frozen, fresh)
    inputs = {str(path): sha(path) for path in [
        fixture, header, native, host, build, compiler, runtime, Path(metadata['sourceFreeze']),
        Path(__file__), *additional, *tools.values(),
        *(p for tree in (sdk, fresh) for p in tree.rglob('*') if p.is_file())
    ]}
    env = {key: value for key, value in os.environ.items()
           if not key.startswith('XL6_') and key != 'XLANG_PACKAGE_PATH'}
    env.update(TMP=str(scratch), TEMP=str(scratch), TMPDIR=str(scratch))
    key = 'PATH' if windows else 'LD_LIBRARY_PATH'
    env[key] = str(compiler.parent) + os.pathsep + env.get(key, '')
    report = dict(complete=False, passed=False, shippingDelivery=False, fullCompilerBuild=True,
                  inputs=inputs, rows=[], programs=[], budgets=[])

    def save():
        target = out / 'results.json'
        pending = target.with_suffix('.tmp')
        pending.write_text(json.dumps(report, indent=2) + '\n', 'utf8')
        os.replace(pending, target)

    def run(name, command, success=True):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=300)
        data = result.stdout + result.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        passed = (result.returncode == 0) == success
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
            expectedSuccess=success, passed=passed, seconds=time.monotonic()-start, logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert passed, log
        return data

    def check(name, data):
        prefixes = re.findall(rb'BORROWED_VALUE_RECORD_OK\s+(\d+)\s+(\d+)', data)
        assert prefixes == [(b'2', b'6')], data[-2000:]
        report['budgets'].append(dict(name=name, shared=2, snapshot=6, repeats=4))

    def check_imports(name, path):
        data = run(name, [tools['objdump'], '-p', path])
        imports = re.findall(rb'DLL Name:\s*(\S+)' if windows else rb'\bNEEDED\s+(\S+)', data)
        assert not {item.lower() for item in imports}.intersection(
            {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so', b'libtcc.dll', b'libtcc.so'})
        report['programs'].append(dict(path=str(path), sha256=sha(path),
                                       imports=[item.decode() for item in imports]))

    includes = ['--inc-path', root, '--inc-path', fresh / 'include', '--inc-path', sdk]
    c_includes = ['-I' + str(path) for path in (fresh / 'include', root, sdk, compiler.parent)]
    original = fixture.read_text('utf8')
    try:
        generated, public = out / 'generated.c', out / 'public.h'
        run('emit', [compiler, 'cgen', fixture, '--emit', generated, '--emit-header', public, *includes])
        code = generated.read_text('utf-8-sig')
        assert 'xrtValueRetain(borrowed_output_value)' in code
        assert 'borrowedRecordReadBound(' in code and 'xl6PersistenceSnapshotNativeV1(' in code
        for std, compiler_name, lang in [('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++'), ('c++17', 'g++', 'c++')]:
            run('header-' + std, [tools[compiler_name], '-std=' + std, '-Wall', '-Wextra',
                '-Werror', *c_includes, '-x', lang, '-fsyntax-only', public])
        check('jit', run('jit', [compiler, 'run', fixture, *includes]))
        exe = out / ('record.exe' if windows else 'record')
        run('aot-build', [compiler, 'build', fixture, '-o', exe, *includes])
        check('aot', run('aot-run', [exe]))
        check_imports('aot-imports', exe)
        systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if windows else [
            '-pthread', '-ldl', '-lm', '-Wl,-rpath,' + str(runtime.parent)]
        for opt in ('O0', 'O2'):
            executable = out / (opt + ('.exe' if windows else ''))
            run(opt + '-build', [tools['gcc'], '-std=c11', '-' + opt, *c_includes,
                generated, native, host, runtime, *systems, '-o', executable])
            check(opt, run(opt + '-run', [executable]))
        shared = out / 'record-shared.xl'
        shared.write_text(original.replace('func main(...) as int', 'export "record_check" func check() as int'), 'utf8')
        module = out / ('record.dll' if windows else 'record.so')
        run('shared-build', [compiler, 'build', shared, native, '--shared', '-o', module, *includes])
        driver = out / 'shared-host.py'
        driver.write_text('import _ctypes, ctypes, os, sys\n'
            'm = ctypes.CDLL(sys.argv[1], **({"winmode": 0} if os.name == "nt" else {}))\n'
            'm.xl_script_module_init.argtypes = [ctypes.c_void_p]\n'
            'assert m.xl_script_module_init(None) == 1\n'
            'm.record_check.restype = ctypes.c_int64\n'
            'assert m.record_check() == 0\n'
            'assert m.xl_script_module_unit_checked() == 1\n'
            'if os.name == "nt":\n'
            '    assert m.xl_script_runtime_retire_checked() == 1\n'
            '    _ctypes.FreeLibrary(m._handle)\n'
            'else:\n'
            '    _ctypes.dlclose(m._handle)\n'
            'print("BORROWED_RECORD_SHARED_RETIRED")\n', 'utf8')
        data = run('shared-run', [sys.executable, '-B', driver, module])
        check('shared', data)
        assert b'BORROWED_RECORD_SHARED_RETIRED' in data
        check_imports('shared-imports', module)

        declaration = next(line for line in original.splitlines() if 'func read(' in line)
        tail = '\nfunc main(...) as int\n return 0\nend\n'
        invalid = {
            'transfer': declaration.replace('"borrowed_value"', '"owned_value"'),
            'nullable-var': declaration.replace('string?,var,string?', 'string?,var?,string?'),
            'pure': declaration.replace('@no_raise', '@no_raise @pure'),
            'missing-effects': declaration.replace(' @no_raise', ''),
            'snapshot-on-string': declaration.replace('@c_data_snapshot_result(1)', '@c_data_snapshot_result(0)'),
            'wrong-type': declaration.replace('string?,var,string?', 'string?,bytes,string?'),
            'ordinary-output': declaration.split(' @c_output_record')[0] +
                ' @c_output(1,0) @c_output_view(1,"borrowed_value") func other(c_int mode) as var',
        }
        for name, text in invalid.items():
            bad = out / ('bad-' + name + '.xl')
            bad.write_text(text + tail, 'utf8')
            target = bad.with_suffix('.c')
            data = run('reject-' + name, [compiler, 'cgen', bad, '--emit', target, *includes], False)
            assert not target.exists() and (b'[sem]' in data or b'[ir]' in data), data
        raw_header = header.read_text('utf8')
        for name, field in [('foreign-pointer', 'void* data'), ('pointer-indirection', 'xvalue** data')]:
            poisoned = out / name
            destination = poisoned / 'test/cases' / header.name
            destination.parent.mkdir(parents=True)
            destination.write_text(raw_header.replace('xvalue* data', field), 'utf8')
            for std, cc, lang in [('c11', 'gcc', 'c'), ('c++17', 'g++', 'c++')]:
                data = run('reject-header-' + name + '-' + std, [tools[cc], '-std=' + std,
                    '-I' + str(poisoned), *c_includes, '-x', lang, '-fsyntax-only', public], False)
                assert b'borrowed output record field type' in data, data
        report.update(complete=True, inputsUnchanged=all(sha(path) == digest for path, digest in inputs.items()))
        report['passed'] = report['inputsUnchanged']
        assert report['passed']
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
