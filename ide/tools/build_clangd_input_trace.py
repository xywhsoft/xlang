"""Reproduce the isolated experimental provider; does not replace IDE releases."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, sys

COMMIT = 'ea7d852a70e8bdfaf601d6626a760f9771b2c4b4'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--stage', type=Path, required=True, help='Owned isolated provider stage containing source/.git.')
parser.add_argument('--jobs', type=int, default=6)
parser.add_argument('--prepare-only', action='store_true')
parser.add_argument('--build-alias', type=Path, help='Existing owned junction to stage/compile; avoids Windows path length limits.')
args = parser.parse_args()
stage = args.stage.resolve()
source, compile_root = stage / 'source', stage / 'compile'
patch_root = Path(__file__).resolve().parent / 'clangd_input_trace'
if not 1 <= args.jobs <= 12: parser.error('--jobs must be between 1 and 12')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def save(name, value): (stage / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf8')
def git(*options): return subprocess.check_output(['git', '-C', str(source), *options], text=True, encoding='utf8').strip()
def run(name, command):
    save(name + '-command.json', command)
    print('Running', name, flush=True)
    with (stage / (name + '.log')).open('wb') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    (stage / (name + '.exit.txt')).write_text(str(result.returncode), encoding='utf8')
    if result.returncode:
        print((stage / (name + '.log')).read_text(encoding='utf8', errors='replace')[-8000:])
        raise SystemExit(result.returncode)
assert git('rev-parse', 'HEAD') == COMMIT, 'Provider source must be the exact verified upstream commit.'
receipt = stage / 'input-trace-prepared.json'
if not receipt.exists():
    assert not git('status', '--porcelain', '--untracked-files=all'), 'Provider source must be clean before applying the adaptation.'
    upstream = source / 'clang-tools-extra/clangd'
    changes = {
        'support/ThreadsafeFS.cpp': [('#include "support/ThreadsafeFS.h"', '#include "support/ThreadsafeFS.h"\n#include "InputTrace.h"'),
            ('return new VolatileFileSystem(llvm::vfs::createPhysicalFileSystem());', 'return wrapInputTraceFS(new VolatileFileSystem(llvm::vfs::createPhysicalFileSystem()));')],
        'support/CMakeLists.txt': [('  ThreadsafeFS.cpp', '  ThreadsafeFS.cpp\n  InputTrace.cpp\n  NativeInput.c')],
        'CMakeLists.txt': [('  JSONTransport.cpp', '  JSONTransport.cpp\n  InputTraceTransport.cpp')],
        'JSONTransport.cpp': [('return std::make_unique<JSONTransport>(In, Out, InMirror, Pretty, Style);',
            'return newInputTraceTransport(std::make_unique<JSONTransport>(In, Out, InMirror, Pretty, Style));')],
        'Transport.h': [('std::unique_ptr<Transport>\nnewJSONTransport(', 'std::unique_ptr<Transport> newInputTraceTransport(std::unique_ptr<Transport> Inner);\nstd::unique_ptr<Transport>\nnewJSONTransport(')],
        'GlobalCompilationDatabase.cpp': [('#include "GlobalCompilationDatabase.h"', '#include "GlobalCompilationDatabase.h"\n#include "support/InputTrace.h"'),
            ('auto FS = llvm::vfs::getRealFileSystem();', 'auto FS = wrapInputTraceFS(llvm::vfs::getRealFileSystem());')],
    }
    original = {}
    for name, replacements in changes.items():
        p = upstream / name
        original[name] = sha(p)
        data = p.read_text(encoding='utf8')
        for old, new in replacements:
            assert data.count(old) == (2 if name == 'GlobalCompilationDatabase.cpp' and old.startswith('auto FS') else 1), (name, old)
            data = data.replace(old, new)
        p.write_text(data, encoding='utf8', newline='\n')
    for name in ('InputTrace.h', 'InputTrace.cpp', 'NativeInput.h', 'NativeInput.c'):
        shutil.copyfile(patch_root / name, upstream / 'support' / name)
    shutil.copyfile(patch_root / 'InputTraceTransport.cpp', upstream / 'InputTraceTransport.cpp')
    patch = subprocess.check_output(['git', '-C', str(source), 'diff', '--binary', '--no-ext-diff'])
    (stage / 'upstream-adaptation.patch').write_bytes(patch)
    adapted = {n: sha(upstream / n) for n in changes}
    added = {('support/' + n): sha(upstream / 'support' / n) for n in ('InputTrace.h', 'InputTrace.cpp', 'NativeInput.h', 'NativeInput.c')}
    added['InputTraceTransport.cpp'] = sha(upstream / 'InputTraceTransport.cpp')
    save(receipt.name, {'commit': COMMIT, 'tree': git('rev-parse', 'HEAD^{tree}'), 'original': original,
         'adapted': adapted, 'added': added, 'patchSha256': sha(stage / 'upstream-adaptation.patch')})
else:
    previous = json.loads(receipt.read_text(encoding='utf8'))
    assert previous['commit'] == COMMIT and previous['tree'] == git('rev-parse', 'HEAD^{tree}'), 'Prepared source identity changed.'
    for name, digest in {**previous['adapted'], **previous['added']}.items():
        assert sha(source / 'clang-tools-extra/clangd' / name) == digest, ('Changed prepared provider input', name)
    actual_patch = subprocess.check_output(['git', '-C', str(source), 'diff', '--binary', '--no-ext-diff'])
    assert hashlib.sha256(actual_patch).hexdigest() == previous['patchSha256'] == sha(stage / 'upstream-adaptation.patch'), 'Unrecorded tracked provider changes.'
    prefix = 'clang-tools-extra/clangd/'
    expected_status = {(' M ' + prefix + name) for name in previous['adapted']} | {('?? ' + prefix + name) for name in previous['added']}
    status = subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain', '--untracked-files=all'], text=True, encoding='utf8')
    assert set(status.splitlines()) == expected_status, 'Unrecorded provider source files.'
run('configure', [shutil.which('cmake'), '-G', 'Ninja', '-S', str(source / 'llvm'), '-B', str(compile_root),
    '-DCMAKE_BUILD_TYPE=Release', '-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra',
    '-DCMAKE_C_COMPILER=' + shutil.which('gcc'), '-DCMAKE_CXX_COMPILER=' + shutil.which('g++'),
    '-DCMAKE_EXE_LINKER_FLAGS=-static-libgcc -static-libstdc++',
    '-DLLVM_TARGETS_TO_BUILD=X86', '-DLLVM_ENABLE_ASSERTIONS=OFF', '-DLLVM_ENABLE_LTO=OFF',
    '-DLLVM_ENABLE_ZLIB=OFF', '-DLLVM_ENABLE_ZSTD=OFF', '-DLLVM_ENABLE_LIBXML2=OFF', '-DLLVM_ENABLE_CURL=OFF',
    '-DLLVM_INCLUDE_TESTS=OFF', '-DLLVM_INCLUDE_EXAMPLES=OFF', '-DLLVM_INCLUDE_BENCHMARKS=OFF',
    '-DLLVM_ENABLE_BINDINGS=OFF', '-DCLANG_ENABLE_STATIC_ANALYZER=OFF', '-DCLANG_ENABLE_ARCMT=OFF',
    '-DCLANGD_TIDY_CHECKS=OFF', '-DCLANGD_BUILD_XPC=OFF', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
if args.prepare_only:
    print('Provider adaptation prepared:', receipt, flush=True)
    sys.exit(0)
build_root = args.build_alias.absolute() if args.build_alias else compile_root
if args.build_alias:
    assert build_root.resolve() == compile_root.resolve() and build_root.parent.resolve().is_relative_to(stage.parent.parent.resolve()), 'Build alias must point to this owned compile directory.'
run('build', [shutil.which('cmake'), '--build', str(build_root), '--target', 'clangd', '--parallel', str(args.jobs)])
exe = compile_root / 'bin/clangd.exe'
assert exe.is_file()
save('input-trace-built.json', {'commit': COMMIT, 'preparedSha256': sha(receipt), 'executable': str(exe),
    'sha256': sha(exe), 'version': subprocess.check_output([str(exe), '--version'], text=True, encoding='utf8')})
print('Experimental provider built:', exe, flush=True)
