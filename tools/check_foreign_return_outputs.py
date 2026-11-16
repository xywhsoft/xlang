"""Preserve an exact C int result together with trivial pointer outputs.

Uses a complete O2/TCC/XRT/VFS compiler, not a frontend overlay. Exercises
direct/function/bound/protocol/dynamic/field calls, rollback, exact native ABI,
standalone/shared and real embedded compilers. CPU only; no release install.
The separate source-client gate verifies sealed IR mutations.
"""
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
from full_compiler_qualification import verify_full_compiler
from compiler_response import command_with_response


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    a = parser.parse_args()
    root, compiler, build, out = (p.resolve() for p in
        (a.root, a.compiler, a.compiler_report, a.out))
    metadata = verify_full_compiler(build, compiler)
    source = Path(metadata['source'])
    win = os.name == 'nt'
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    frozen = json.loads(Path(metadata['sourceFreeze']).read_text(encoding='utf8'))
    # The tests may evolve independently after the compiler build starts;
    # every compiler implementation file must still be byte-identical.
    changes = {n: h for n, h in frozen['overrides'].items() if n.startswith('src/')}
    assert len(changes) == 14 and all(sha(root / n) == h for n, h in changes.items())
    case = root / 'test/cases/foreign_return_outputs.xl'
    native = case.with_suffix('.h')
    producer = case.with_suffix('.c')
    decl = case.with_name(case.stem + '_decl.c')
    host = root / 'test/cases/foreign_decl_module_host.c'
    tools = {n: Path(shutil.which(n)).resolve() for n in ('gcc', 'g++', 'objdump')}
    sdk = compiler.parent / 'include'
    runtime = compiler.parent / ('xlc.dll' if win else 'libxlc.so')
    stage = source / 'tools/stage_runtime_headers.py'
    files = [Path(__file__), Path(__file__).with_name('full_compiler_qualification.py'),
        Path(__file__).with_name('compiler_response.py'), case, native, producer, decl, host,
        compiler, runtime, build, Path(metadata['sourceFreeze']), stage,
        source / 'tools/gen_tcc_vfs_resources.py',
        source / 'tools' / ('tcc_vfs_manifest_win64_xlc.txt' if win else 'tcc_vfs_manifest_linux_xlc.txt'),
        *(root / n for n in changes), *tools.values(),
        *(p for p in sdk.rglob('*') if p.is_file())]
    inputs = {str(p): sha(p) for p in files}
    out.mkdir(parents=True, exist_ok=False)
    snapshots = out / 'input-snapshots'
    snapshots.mkdir()
    for p in (case, native, producer, decl, Path(__file__)):
        shutil.copy2(p, snapshots / p.name)
    env = {k: v for k, v in os.environ.items()
        if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    key = 'PATH' if win else 'LD_LIBRARY_PATH'
    clean_path = env.get(key, '')
    env[key] = str(compiler.parent) + os.pathsep + clean_path
    env.update(PYTHONUTF8='1', PYTHONDONTWRITEBYTECODE='1',
        TMP=str(out), TEMP=str(out), TMPDIR=str(out))
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs,
        compilerChanges=changes, fullCompilerBuild=True, shippingDelivery=False,
        networkTests=False, rows=[], programs=[], heapInterval=dict(
            languageCalls=1000, nativeCalls=1000, alloc=0, free=0, realloc=0,
            limitation='XRT counters only; no CPU or OS allocator equivalence claim'))

    def save():
        temporary = out / 'results.json.tmp'
        temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        os.replace(temporary, out / 'results.json')

    def run(name, command, reject=False, marker=None, standalone=False):
        command = list(map(str, command))
        actual = command_with_response(command, tools['gcc'], out / (name + '.rsp'))
        execution = dict(env)
        if standalone:
            execution[key] = clean_path
        started = time.monotonic()
        result = subprocess.run(actual, cwd=root, env=execution,
            capture_output=True, timeout=360)
        data = result.stdout + result.stderr
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command,
            exitCode=result.returncode, expectedFailure=reject,
            seconds=time.monotonic() - started, logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert bool(result.returncode) == reject, log
        if marker:
            assert marker in data, log
        return data

    def imports(name, program):
        data = run(name + '-imports', [tools['objdump'], '-p', program])
        imported = re.findall(rb'DLL Name:\s*(\S+)', data) if win else re.findall(rb'\bNEEDED\s+(\S+)', data)
        assert not {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so',
            b'libtcc.dll', b'libtcc.so'}.intersection(n.lower() for n in imported)
        report['programs'].append(dict(path=str(program), sha256=sha(program),
            bytes=program.stat().st_size, imports=[n.decode() for n in imported]))
        save()

    try:
        headers = out / 'runtime-sdk'
        run('stage-headers', [sys.executable, '-B', stage, '--root', source, '--out', headers])
        inputs.update((str(p), sha(p)) for p in headers.rglob('*') if p.is_file())
        include = ['--inc-path', root, '--inc-path', sdk]
        inc = ['-I' + str(p) for p in (root, headers / 'include', sdk)]
        generated, public, lowered = (out / n for n in ('generated.c', 'public.h', 'lowered.json'))
        run('emit', [compiler, 'cgen', case, '--emit', generated, '--emit-header', public, *include])
        run('ir', [compiler, 'ir', case, '--lowered', '--format', 'json', '--emit', lowered, *include])
        code = generated.read_text(encoding='utf-8-sig')
        assert re.search(r'\.e0\s*=\s*returnOutputQuery\(', code)
        assert 'native_callee_receiver->query' in code
        assert re.search(r'\.e0\s*=\s*native_callee\(', code)
        assert 'xl6ForeignIntegerFailureV1' in code
        for standard, tool, language in (('c11', 'gcc', 'c'),
                ('c++11', 'g++', 'c++'), ('c++17', 'g++', 'c++')):
            run('header-' + standard, [tools[tool], '-std=' + standard,
                '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', *inc,
                '-x', language, '-fsyntax-only', public])
        for opt in ('O0', 'O2'):
            program = out / ('decl-' + opt + ('.exe' if win else ''))
            run('decl-' + opt + '-build', [tools['gcc'], '-std=c11', '-' + opt,
                '-I' + str(root), '-I' + str(compiler.parent), '-I' + str(sdk), decl, '-o', program])
            run('decl-' + opt + '-run', [program], marker=b'FOREIGN_RETURN_OUTPUT_DECL_OK')
        marker = b'FOREIGN_RETURN_OUTPUTS_OK'
        run('jit', [compiler, 'run', case, *include], marker=marker)
        binary = out / ('standalone.exe' if win else 'standalone')
        run('standalone-build', [compiler, 'build', case, '-o', binary, *include])
        run('standalone-run', [binary], marker=marker, standalone=True)
        imports('standalone', binary)
        shared = out / 'shared.xl'
        shared.write_text(case.read_text(encoding='utf8').replace(
            'func main(...) as int', 'export "return_outputs_check" func check() as int'), encoding='utf8')
        library = out / ('outputs.dll' if win else 'outputs.so')
        run('shared-build', [compiler, 'build', shared, producer, '--shared', '-o', library, *include])
        client = ('import ctypes,sys; m=ctypes.CDLL(sys.argv[1]); '
            'm.return_outputs_check.restype=ctypes.c_int; assert m.return_outputs_check()==0; '
            'm.xl_script_module_unit_checked.restype=ctypes.c_int; assert m.xl_script_module_unit_checked()==1')
        run('shared-run', [sys.executable, '-B', '-c', client, library], marker=marker, standalone=True)
        imports('shared', library)
        system = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if win else [
            '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-Wl,-rpath,' + str(runtime.parent)]
        warnings = ['-Wno-' + n for n in ('unused-function', 'unused-variable', 'unused-label',
            'unused-const-variable', 'unused-but-set-variable', 'cast-function-type')]
        for opt in ('O0', 'O2'):
            program = out / (opt + ('.exe' if win else ''))
            run(opt + '-build', [tools['gcc'], '-std=c11', '-' + opt, '-Wall', '-Wextra',
                '-Werror', *warnings, '-DXL_STATIC_EMBED', *inc, generated, producer, host, runtime,
                *system, '-o', program])
            run(opt + '-run', [program], marker=marker)
        original = case.read_text(encoding='utf8')
        # Isolated mutations of the first declaration exercise semantic
        # rejection before C or header bytes can be produced.
        needle = original.splitlines()[0]
        mutations = {
            'sentinel-later': ('@c_outputs(-1,1,2)', '@c_outputs(1,-1,2)'),
            'sentinel-negative': ('@c_outputs(-1,1,2)', '@c_outputs(-2,1,2)'),
            'no-pointer-output': ('@c_outputs(-1,1,2)', '@c_outputs(-1)'),
            'duplicate-slot': ('@c_outputs(-1,1,2)', '@c_outputs(-1,1,1)'),
            'missing-result': ('as (c_int,c_double,c_int)', 'as (c_int,c_double)'),
            'regular-int': ('as (c_int,c_double,c_int)', 'as (int,c_double,c_int)'),
            'unsigned-return': ('as (c_int,c_double,c_int)', 'as (c_uint,c_double,c_int)'),
            'status-conflict': ('@c_outputs(-1,1,2)', '@c_outputs(-1,1,2) @c_status(0)'),
            'missing-no-raise': ('@no_raise ', ''),
            'borrowed-conflict': ('@c_result("integer_min",0)', '@c_result("borrowed_cstr")'),
            'unreachable-slot': ('@c_outputs(-1,1,2)', '@c_outputs(-1,1,63)'),
        }
        for name, (old, new) in mutations.items():
            replacement = needle.replace(old, new, 1)
            assert replacement != needle
            bad = out / (name + '.xl')
            target, header = out / (name + '.c'), out / (name + '.h')
            bad.write_text(original.replace(needle, replacement, 1), encoding='utf8')
            run('reject-' + name, [compiler, 'cgen', bad, '--emit', target,
                '--emit-header', header, *include], reject=True)
            assert not target.exists() and not header.exists()
        header_text = native.read_text(encoding='utf8')
        for name, old, new in (
            ('unsigned-return', 'int returnOutputQuery(', 'unsigned int returnOutputQuery('),
            ('wide-return', 'int returnOutputQuery(', 'long long returnOutputQuery('),
            ('wrong-pointer', 'int returnOutputQuery(int,double*,int*)', 'int returnOutputQuery(int,float*,int*)')):
            bad = out / ('abi-' + name)
            target = bad / 'test/cases' / native.name
            target.parent.mkdir(parents=True)
            assert old in header_text
            target.write_text(header_text.replace(old, new, 1), encoding='utf8')
            for language, tool in (('c', 'gcc'), ('c++', 'g++')):
                data = run('reject-abi-' + name + '-' + language,
                    [tools[tool], '-std=' + ('c11' if language == 'c' else 'c++17'),
                     '-I' + str(bad), *inc, '-DXL_STATIC_EMBED', '-x', language,
                     '-fsyntax-only', public], reject=True)
                assert b'ABI' in data
        # The embedded compiler is actually linked into both artifacts. Its
        # precompiled frontend must understand the new output contract, too.
        child = ('#include <xlc.h>\nint main(void){XLC_Context* c=xlcCreateContext(0);'
            'if(!c)return 1;int ok=xlcAddIncludePath(c,' + json.dumps(root.as_posix()) +
            ') && xlcAddSourceFile(c,' + json.dumps(case.as_posix()) +
            ') && xlcCompile(c) && xlcRun(c,0,0) && xlcGetLastExitCode(c)==0;'
            'xlcDestroyContext(c);return ok?0:2;}\n')
        for is_shared in (False, True):
            name = 'nested-shared' if is_shared else 'nested-parent'
            outer = out / (name + '.xl')
            body = ('export "nested_outputs" func check() as int\n return tcc.run(' +
                json.dumps(child) + ')\nend\n') if is_shared else (
                'assert tcc.run(' + json.dumps(child) + ')==0\n')
            outer.write_text('import xlang.compiler\nimport tcc\n' + body, encoding='utf8')
            program = out / (name + (('.dll' if win else '.so') if is_shared else ('.exe' if win else '')))
            run(name + '-build', [compiler, 'build', outer,
                *(['--shared'] if is_shared else []), '-o', program])
            if is_shared:
                client = ('import ctypes,sys; m=ctypes.CDLL(sys.argv[1]); '
                    'm.nested_outputs.restype=ctypes.c_int; assert m.nested_outputs()==0; '
                    'm.xl_script_module_unit_checked.restype=ctypes.c_int; assert m.xl_script_module_unit_checked()==1')
                run(name + '-run', [sys.executable, '-B', '-c', client, program],
                    marker=marker, standalone=True)
            else:
                run(name + '-run', [program], marker=marker, standalone=True)
            imports(name, program)
        verify_full_compiler(build, compiler)
        report.update(complete=True, inputsUnchanged=all(sha(p) == h for p, h in inputs.items()))
        report['passed'] = report['inputsUnchanged']
        assert report['passed']
    except BaseException as error:
        report.update(complete=True, passed=False, error=repr(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
