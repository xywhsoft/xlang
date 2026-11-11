"""CPU-only borrowed native output ownership; no window or GPU tests.

Complete builds qualify the compiler/runtime/embedded-source cohort. An explicit
diagnostic report only tests a frontend overlay, never a shipping compiler.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from full_compiler_qualification import verify_full_compiler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'out', 'objdump'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--family', choices=('borrowed', 'cstring'), required=True)
    cohort = parser.add_mutually_exclusive_group(required=True)
    cohort.add_argument('--compiler-report', type=Path)
    cohort.add_argument('--diagnostic-report', type=Path)
    args = parser.parse_args()
    root, compiler, out, objdump = (p.resolve() for p in
        (args.root, args.compiler, args.out, args.objdump))
    build = (args.compiler_report or args.diagnostic_report).resolve()
    full = args.compiler_report is not None
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    if full:
        verify_full_compiler(build, compiler)
        sdk = compiler.parent / 'include'
        extra = []
    else:
        metadata = json.loads(build.read_text())
        assert metadata['passed'] and metadata['inputsUnchanged'] and not metadata['fullCompilerBuild']
        assert compiler.parent == build.parent / 'runtime'
        assert all(sha(p) == h for p, h in metadata['inputs'].items())
        parent = Path(metadata['parent'])
        assert sha(parent) == metadata['parentSha256']
        snapshot = Path(json.loads(parent.read_text())['source'])
        sdk = snapshot / 'release' / ('win_x64' if os.name == 'nt' else 'linux_x64') / 'include'
        extra = [parent, *map(Path, metadata['inputs'])]
        for name, digest in metadata['overrides'].items():
            assert sha(root / name) == digest, name
            extra.append(root / name)
    cc, cpp = (Path(shutil.which(name)).resolve() for name in ('gcc', 'g++'))
    borrowed = args.family == 'borrowed'
    fixture = root / ('test/cases/foreign_' + args.family + '_output.xl')
    provider = fixture.with_name('foreign_' + args.family + '_output_native.c')
    files = [Path(__file__).resolve(), Path(__file__).with_name('full_compiler_qualification.py'),
             compiler, build, objdump, cc, cpp, fixture, provider, fixture.with_suffix('.h'), *extra]
    files += [compiler.parent / name for name in
              (('xl.dll', 'xlc.dll') if os.name == 'nt' else ('libxl.so', 'libxlc.so'))]
    files += list(p for p in sdk.rglob('*') if p.is_file())
    # Authoritative fixture headers include compiler-private C contracts too.
    # Pin their transitive project header tree, not only the top-level fixture.
    files += list((root / 'src').rglob('*.h'))
    files += [root / 'test/cases' / name for name in
              ('foreign_managed_view.h', 'foreign_managed_view_native.c', 'foreign_expanded_calls.h')]
    regressions = {
        'foreign_managed_view': 'foreign_managed_view_native.c',
        'foreign_output_retain': 'foreign_output_retain_native.c',
        'foreign_xge3_binding_output_view': 'foreign_xge3_binding_output_view.c',
    } if borrowed else {}
    for name, native_source in regressions.items():
        files += [root / 'test/cases' / (name + suffix) for suffix in ('.xl', '.h')]
        files.append(root / 'test/cases' / native_source)
    inputs = {str(p): sha(p) for p in files}
    out.mkdir(parents=True, exist_ok=False)
    report = dict(passed=False, family=args.family, fullCompilerBuild=full, shippingDelivery=False,
                  scope=__doc__, inputs=inputs, rows=[])
    env = dict(os.environ)
    env.pop('XLANG_PACKAGE_PATH', None)
    variable = 'PATH' if os.name == 'nt' else 'LD_LIBRARY_PATH'
    env[variable] = str(compiler.parent) + os.pathsep + env.get(variable, '')

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(name, command, reject=False, working=None):
        command = list(map(str, command))
        working = working or compiler.parent
        result = subprocess.run(command, cwd=working, env=env, capture_output=True, timeout=240)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        report['rows'].append(dict(name=name, command=command, cwd=str(working),
            exitCode=result.returncode, expectedFailure=reject, logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert bool(result.returncode) == reject, log
        return result.stdout + result.stderr

    include = ['--inc-path', root, '--inc-path', sdk]
    generated, header = out / 'generated.c', out / 'public.h'
    run('emit', [compiler, 'cgen', fixture, '--emit', generated, '--emit-header', header, *include])
    text = generated.read_text()
    if borrowed:
        assert 'ManagedChild* native_raw = {0};' in text
        assert 'xl6ForeignViewBorrowChildV1' in text and 'xl6ForeignViewBorrowRootV1' in text
    else:
        assert 'const char* native_raw = {0};' in text and 'xlir_string_from_cstr(native_raw)' in text
    run('jit', [compiler, 'run', fixture, provider, *include])
    binary = out / ('standalone.exe' if os.name == 'nt' else 'standalone')
    run('static-build', [compiler, 'build', fixture, provider, '-o', binary, *include])
    run('static-run', [binary])
    imports = run('static-imports', [objdump, '-p', binary]).decode(errors='replace').lower()
    for forbidden in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so', 'libtcc.dll', 'libtcc.so'):
        assert forbidden not in imports, forbidden
    for standard in ('c11', 'c++11', 'c++17'):
        language = 'c++' if standard.startswith('c++') else 'c'
        tool = cpp if language == 'c++' else cc
        run('header-' + standard, [tool, '-std=' + standard, '-Wall', '-Wextra', '-Werror',
            '-DXL_STATIC_EMBED', '-I' + str(root), '-I' + str(sdk), '-x', language, '-fsyntax-only', header])
    library = out / ('borrowed.dll' if os.name == 'nt' else 'libborrowed.so')
    shared_source, shared_header = out / 'library.xl', out / 'shared.h'
    original = fixture.read_text()
    shared_source.write_text(original.replace('func main(...) as int', 'func outputBody() as int', 1)
                             .replace('assert main()==0', 'assert outputBody()==0', 1))
    inputs[str(shared_source)] = sha(shared_source)
    run('shared-build', [compiler, 'build', shared_source, provider, '-shared', '-o', library,
                        '--emit-header', shared_header, *include])
    host = out / 'host.cpp'
    export = 'borrowedOutputSelfTest' if borrowed else 'cstringOutputSelfTest'
    host.write_text('''#include "shared.h"
#include <cassert>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
int main(int argc,char** argv){assert(argc==2);
#if defined(_WIN32)
 auto image=LoadLibraryA(argv[1]);assert(image);
 auto address=GetProcAddress(image,"xl6_export_SELF_TEST");
#else
 auto image=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(image);
 auto address=dlsym(image,"xl6_export_SELF_TEST");
#endif
 decltype(&xl6_export_SELF_TEST) function=nullptr;
 static_assert(sizeof(function)==sizeof(address),"procedure width");
 std::memcpy(&function,&address,sizeof(function));
 assert(function && function()==73 && function()==73);
#if defined(_WIN32)
 int(*retire)(void)=nullptr;auto retireAddress=GetProcAddress(image,"xl_script_runtime_retire_checked");
 static_assert(sizeof(retire)==sizeof(retireAddress),"retirement width");
 std::memcpy(&retire,&retireAddress,sizeof(retire));
 assert(retire && retire());assert(FreeLibrary(image));
#else
 assert(dlclose(image)==0);
#endif
 return 0;}
'''.replace('SELF_TEST', export))
    inputs[str(host)] = sha(host)
    executable = out / ('shared-host.exe' if os.name == 'nt' else 'shared-host')
    run('shared-host-build', [cpp, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
        '-I' + str(root), '-I' + str(sdk), host, *([] if os.name == 'nt' else ['-ldl']), '-o', executable])
    raw = run('shared-host-run', [executable, library])
    assert raw.count(b'Borrowed owner outputs:' if borrowed else b'C-string outputs:') == 2
    declaration = ('@no_raise @c_output(2,0) @c_result("owner_view",0) func query(c_int mode=0) as Child'
                   if borrowed else '@no_raise @c_output(2,0) @c_output_view(2,"cstring") func read(c_int mode=0) as string')
    assert original.count(declaration) == 1
    negatives = {
        'raising': declaration.replace('@no_raise ', ''),
        'pure': '@pure ' + declaration,
        'source-is-scalar': declaration.replace('"owner_view",0', '"owner_view",1'),
        'source-out-of-range': declaration.replace('"owner_view",0', '"owner_view",2'),
        'unowned': declaration.replace('"owner_view",0', '"owned_view"'),
        'wrong-result': declaration.replace('as Child', 'as c_int'),
        'wrong-output': declaration.replace('@c_output(2,0)', '@c_output(9,0)'),
        'factory-without-rollback': declaration.replace('@c_output(2,0)', '@factory(2,0)'),
        'text-conversion': declaration.replace('@c_output(2,0)', '@c_output(2,0) @c_output_view(0,"text")'),
    } if borrowed else {
        'wrong-result': declaration.replace('as string', 'as bytes'),
        'unknown-conversion': declaration.replace('"cstring"', '"cstr"'),
        'wrong-output': declaration.replace('@c_output_view(2,', '@c_output_view(1,'),
        'duplicate-conversion': '@c_output_view(2,"text") ' + declaration,
        'duplicate-output': '@c_outputs(1,2) @c_status(0) ' + declaration,
        'native-value': declaration.replace('as string', 'as c_int'),
    }
    for name, replacement in negatives.items():
        source, target = out / (name + '.xl'), out / (name + '.c')
        source.write_text(original.replace(declaration, replacement, 1))
        inputs[str(source)] = sha(source)
        raw = run('reject-' + name, [compiler, 'cgen', source, '--emit', target, *include], True)
        assert (b'[sem]' in raw or b'[parse]' in raw) and not target.exists()
    native = fixture.with_suffix('.h').read_text()
    signature = ('int borrowed_status(ManagedRoot*,int,ManagedChild**);' if borrowed else
                 'int cstr_read(ManagedRoot*,int,const char**);')
    header_negatives = {
        'const-output': 'int borrowed_status(ManagedRoot*,int,const ManagedChild**);',
        'wrong-output': 'int borrowed_status(ManagedRoot*,int,ManagedRoot**);',
        'pointer-depth': 'int borrowed_status(ManagedRoot*,int,ManagedChild*);',
        'return-status': 'long borrowed_status(ManagedRoot*,int,ManagedChild**);',
    } if borrowed else {
        'mutable-output': 'int cstr_read(ManagedRoot*,int,char**);',
        'counted-output': 'int cstr_read(ManagedRoot*,int,xstrview*);',
        'pointer-depth': 'int cstr_read(ManagedRoot*,int,const char*);',
        'return-status': 'long cstr_read(ManagedRoot*,int,const char**);',
    }
    for name, replacement in header_negatives.items():
        folder = out / ('bad-header-' + name)
        path = folder / fixture.with_suffix('.h').relative_to(root)
        path.parent.mkdir(parents=True)
        assert native.count(signature) == 1
        path.write_text(native.replace(signature, replacement))
        inputs[str(path)] = sha(path)
        raw = run('reject-header-' + name, [cc, '-std=c11', '-I' + str(folder), '-I' + str(root),
            '-I' + str(sdk), '-fsyntax-only', generated], True)
        assert b'declared C function ABI' in raw
    # A frontend overlay deliberately leaves its embedded compiler unchanged.
    # Only a complete build may qualify nested JIT and standalone embedding.
    if full and borrowed:
        nested = root / 'test/cases/foreign_output_ownership_nested.xl'
        inputs[str(nested)] = sha(nested)
        inputs[str(nested.with_suffix('.h'))] = sha(nested.with_suffix('.h'))
        env['XLANG_OUTPUT_OWNERSHIP_ROOT'] = str(root)
        run('nested-jit', [compiler, 'run', nested, *include], working=root)
        binary = out / ('nested.exe' if os.name == 'nt' else 'nested')
        run('nested-build', [compiler, 'build', nested, '-o', binary, *include])
        raw = run('nested-run', [binary], working=root)
        assert b'Nested native output ownership passed' in raw
        imports = run('nested-imports', [objdump, '-p', binary]).decode(errors='replace').lower()
        for forbidden in ('xl.dll', 'xlc.dll', 'libxl.so', 'libxlc.so', 'libtcc.dll', 'libtcc.so'):
            assert forbidden not in imports, forbidden
    for name, native_source in regressions.items():
        fixture_path = root / 'test/cases' / (name + '.xl')
        native_path = root / 'test/cases' / native_source
        # A same-basename .c is already a compiler-discovered source companion.
        helper = [] if native_path == fixture_path.with_suffix('.c') else [native_path]
        run(name + '-jit', [compiler, 'run', fixture_path, *helper, *include])
        binary = out / (name + ('.exe' if os.name == 'nt' else ''))
        run(name + '-build', [compiler, 'build', fixture_path, *helper, '-o', binary, *include])
        run(name + '-run', [binary])
    report.update(passed=True, inputsUnchanged=all(sha(p) == h for p, h in inputs.items()))
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
