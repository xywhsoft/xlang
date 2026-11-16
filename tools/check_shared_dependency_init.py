"""Shared output accepts dependency initialization, but rejects program entries.

Uses the actual full compiler/TCC/runtime, a shared transitive dependency and
two calls into the resulting DLL/so followed by checked module retirement.
No UI, GPU or network operation executes.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time
from full_compiler_qualification import verify_full_compiler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, compiler, build, out = [x.resolve() for x in
                                 (a.root, a.compiler, a.compiler_report, a.out)]
    verify_full_compiler(build, compiler)
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    report = dict(complete=False, passed=False, scope=__doc__, fullCompilerBuild=True,
                  shippingDelivery=False, networkTests=False, rows=[], inputs={})
    packages = out / 'packages'
    for name in ('dep', 'left', 'right'):
        folder = packages / name
        folder.mkdir(parents=True)
        # Identical statements in one file both execute; identical statements
        # at the same offsets in distinct files are also independent.
        source = ('int initialized=0\ninitialized+=1\ninitialized+=1\nexport func count() as int\n return initialized\nend\n'
                  if name == 'dep' else
                  'import dep\nint own=0\nown+=1\nint seen=dep.count()\nexport func count() as int\n return own+seen\nend\n')
        (folder / 'package.xl').write_text(source, encoding='utf-8')
        dependencies = [] if name == 'dep' else [dict(name='dep', version='1.0.0', abiVersion='1')]
        manifest = dict(format='xlang-package-1', name=name, version='1.0.0', abiVersion='1',
                        kind='test', artifact=dict(kind='source', cSources=[], xlSources=['package.xl']),
                        links=[], loadlibs=[], dependencies=dependencies, native=dict(singleton=False))
        (folder / 'xlang.package.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(XLANG_PACKAGE_PATH=str(packages), TMP=str(out), TEMP=str(out))
    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    def run(name, command, reject=False):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=300)
        data = result.stdout + result.stderr
        log = out / (name + '.log'); log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
                                   seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        save(); print(name, result.returncode, flush=True)
        if reject:
            assert result.returncode != 0 and b'shared build cannot contain' in data, log
        else:
            assert result.returncode == 0, log
        return data
    try:
        good = out / 'shared.xl'
        good.write_text('import left\nimport right\nfrom dep import count as directCount\nint rootValue=42\nexport "shared_check" func check() as int\n return rootValue+left.count()+right.count()+directCount()\nend\n', encoding='utf-8')
        library = out / ('shared.dll' if os.name == 'nt' else 'shared.so')
        run('build', [compiler, 'build', good, '--shared', '-o', library])
        host = out / 'host.py'
        host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\n'
                        'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\n'
                        'lib.xl_script_module_init.restype=ctypes.c_int\n'
                        'assert lib.xl_script_module_init(None)==1\n'
                        'assert lib.xl_script_module_init(None)==1\n'
                        'lib.shared_check.restype=ctypes.c_int\n'
                        'assert lib.shared_check()==50\nassert lib.shared_check()==50\n'
                        'lib.xl_script_module_unit_checked.restype=ctypes.c_int\n'
                        'assert lib.xl_script_module_unit_checked()==1\n', encoding='utf-8')
        run('execute-twice-and-retire', [sys.executable, '-B', host, library])
        program = out / 'program.xl'
        program.write_text('import left\nimport right\nfrom dep import count as directCount\nfunc main(...) as int\n assert left.count()==3 and right.count()==3 and directCount()==2\n println("DEPENDENCY_INIT_ONCE_OK")\n return 0\nend\n', encoding='utf-8')
        run('normal-jit', [compiler, 'run', program])
        instance_cases = []
        targets = {'forward': (packages / 'dep/package.xl').as_posix(),
                   'native': str(packages / 'dep/package.xl')}
        for spelling, direct in targets.items():
            instances = out / ('distinct-instances-' + spelling + '.xl')
            instances.write_text('import dep\nimport ' + json.dumps(direct) + ' as standalone\n'
                                 'func main(...) as int\n'
                                 ' assert dep.count()==2 and standalone.count()==2\n'
                                 ' return 0\nend\n', encoding='utf-8')
            run('distinct-import-instances-' + spelling, [compiler, 'run', instances])
            instance_cases.append(instances)
        bad = packages / 'bad'
        bad.mkdir()
        (bad / 'package.xl').write_text('assert false\nexport func value() as int\n return 1\nend\n', encoding='utf-8')
        manifest['name'] = 'bad'; manifest['dependencies'] = []
        (bad / 'xlang.package.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        bad_source = out / 'bad-init.xl'
        bad_source.write_text('import bad\nexport "bad_check" func check() as int\n return bad.value()\nend\n', encoding='utf-8')
        bad_library = out / ('bad-init.dll' if os.name == 'nt' else 'bad-init.so')
        run('bad-init-build', [compiler, 'build', bad_source, '--shared', '-o', bad_library])
        bad_host = out / 'bad-host.py'
        bad_host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\n'
                            'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\n'
                            'lib.xl_script_module_init.restype=ctypes.c_int\n'
                            'assert lib.xl_script_module_init(None)==0\n'
                            'lib.xl_script_print_uncaught()\n'
                            'lib.xl_script_clear_error()\n'
                            'assert lib.xl_script_module_unit_checked()==1\n', encoding='utf-8')
        assert b'AssertionError' in run('bad-init-checked-failure', [sys.executable, '-B', bad_host, bad_library])
        for name, body in (('main', 'func main(...) as int\n return 0\nend\n'),
                           ('root-call', 'println("program entry")\n'),
                           ('root-assignment', 'int value=0\nvalue+=1\n')):
            case = out / (name + '.xl')
            case.write_text('import left\n' + body, encoding='utf-8')
            destination = out / (name + ('.dll' if os.name == 'nt' else '.so'))
            run('reject-' + name, [compiler, 'build', case, '--shared', '-o', destination], True)
            assert not destination.exists()
        gcc = Path(shutil.which('gcc')).resolve()
        runtime = compiler.parent / ('xlc.dll' if os.name == 'nt' else 'libxlc.so')
        if os.name == 'nt':
            shutil.copy2(runtime, out / runtime.name)
        for optimization in ('O0', 'O2'):
            binary = out / ('identity-' + optimization + ('.exe' if os.name == 'nt' else '.out'))
            flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-' + optimization,
                     '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                     '-I' + str(root), '-I' + str(compiler.parent)]
            if os.name != 'nt':
                flags.append('-Wl,-rpath,' + str(compiler.parent))
            run('identity-' + optimization + '-build', [gcc, *flags, root / 'src/ast/xl6_ast.c',
                root / 'test/cases/ast_source_import_identity_test.c', runtime, '-o', binary])
            run('identity-' + optimization + '-run', [binary])
        files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'), compiler, build]
        files += [gcc, runtime, root / 'src/ast/xl6_ast.c', root / 'src/ast/xl6_ast.h',
                  root / 'test/cases/ast_source_import_identity_test.c']
        files.extend(x for x in packages.rglob('*') if x.is_file())
        files += [good, host, program, *instance_cases, bad_source, bad_host,
                  *(out / (n + '.xl') for n in ('main', 'root-call', 'root-assignment'))]
        report.update(complete=True, passed=True, inputs={str(f): sha(f) for f in files},
                      moduleSha256=sha(library))
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
