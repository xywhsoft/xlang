"""Qualify all eleven direct affine-matrix declarations, without a GPU.

Run with a complete compiler build and matching dynamic/static SDKs. This gate
checks JIT, dynamically linked executables, static executables, generated public
headers, value/failure behavior and XRT heap counters. Shared-library module
initialization and whole-engine performance remain separate gates.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'dynamic-packages',
                 'static-packages', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, compiler, cohort, dynamic, static, out = [path.resolve() for path in
        (args.root, args.compiler, args.compiler_report, args.dynamic_packages,
         args.static_packages, args.out)]
    sdk, extra = load_compiler_cohort(root, compiler, cohort)
    windows = os.name == 'nt'
    tools = {name: Path(shutil.which(name)).resolve() for name in ('gcc', 'g++', 'objdump')}
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    case = root / 'test/cases/foreign_xge3_shape_matrix.xl'
    example = root / 'lib/bindings/xge/examples/shape_matrix.xl'
    sources = [case, case.with_suffix('.c'), case.with_suffix('.h'), example]
    files = [Path(__file__).resolve(), Path(__file__).with_name('foreign_compiler_cohort.py'),
             Path(__file__).with_name('full_compiler_qualification.py'), compiler, cohort,
             *tools.values(), *extra, *sources]
    files.extend(compiler.parent / name for name in
                 (('xl.dll', 'xlc.dll') if windows else ('libxl.so', 'libxlc.so')))
    for folder in (sdk, dynamic, static):
        files.extend(path for path in folder.rglob('*') if path.is_file())
    inputs = {str(path): sha(path) for path in files}
    out.mkdir(parents=True, exist_ok=False)
    report = dict(passed=False, scope=__doc__, inputs=inputs, rows=[],
                  fullCompilerBuild=True, shippingDelivery=False, graphicsTests=False,
                  sharedLibraryAcceptance=False, cpuPerformanceClaim=False)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command, packages=None):
        env = {key: value for key, value in os.environ.items() if not key.startswith('XL6_')}
        env.pop('XLANG_PACKAGE_PATH', None)
        if packages:
            env['XLANG_PACKAGE_PATH'] = str(packages)
            native = str(packages / '_xge_native/native')
            variable = 'PATH' if windows else 'LD_LIBRARY_PATH'
            env[variable] = native + os.pathsep + env.get(variable, '')
        command = list(map(str, command))
        try:
            result = subprocess.run(command, cwd=compiler.parent, env=env,
                                    capture_output=True, timeout=180)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out / (name + '.log')
        log.write_bytes(data)
        report['rows'].append(dict(name=name, command=command,
            packages=str(packages) if packages else None, exitCode=code, logSha256=sha(log)))
        save()
        print(name, code, flush=True)
        assert code == 0, log
        return data

    includes = ['--inc-path', root, '--inc-path', sdk]
    native_includes = ['-I' + str(path) for path in sorted(dynamic.glob('*/include')) if path.is_dir()]
    for source in (case, example):
        name = source.stem
        generated, header = out / (name + '.c'), out / (name + '.h')
        run(name + '-emit', [compiler, 'cgen', source, '--emit', generated,
                            '--emit-header', header, *includes], dynamic)
        for kind, packages in (('jit', dynamic), ('dynamic', dynamic), ('static', static)):
            if kind == 'jit':
                command = [compiler, 'run', source, *includes]
            else:
                binary = out / (name + '-' + kind + ('.exe' if windows else ''))
                run(name + '-' + kind + '-build', [compiler, 'build', source, '-o', binary,
                                                  *includes], packages)
                command = [binary]
            output = run(name + '-' + kind, command, packages)
            if source == case:
                assert b'Shape matrix hot calls: zero XRT heap operations' in output
                assert b'value isolation and checked inverse passed' in output
            else:
                assert b'World:' in output and b'Local:' in output
            if kind != 'jit':
                inspected = run(name + '-' + kind + '-imports',
                                [tools['objdump'], '-p', binary])
                if windows:
                    imports = re.findall(rb'DLL Name:\s*(\S+)', inspected)
                    assert imports
                    lowered = {value.lower() for value in imports}
                    assert not {b'xl.dll', b'xlc.dll', b'libtcc.dll'}.intersection(lowered)
                    assert (b'xge3_host.dll' in lowered) == (kind == 'dynamic')
                else:
                    imports = re.findall(rb'\bNEEDED\s+(\S+)', inspected)
                    assert imports
                    assert not {b'libxl.so', b'libxlc.so'}.intersection(imports)
                    assert any(b'xge3_host' in value for value in imports) == (kind == 'dynamic')
                report.setdefault('binaries', []).append(dict(path=str(binary), sha256=sha(binary),
                    bytes=binary.stat().st_size, linkage=kind, imports=[value.decode() for value in imports]))
        if source == case:
            text = generated.read_text(encoding='utf-8')
            symbols = ['xgeShapeExMatrix' + tail for tail in ('Identity', 'Translate', 'Scale',
                'Rotate', 'Skew', 'Multiply', 'Invert', 'Point', 'Vector', 'RectBounds', 'StrokeScale')]
            assert all(re.search(r'\b' + symbol + r'\(', text) for symbol in symbols)
            declarations = (dynamic / 'xge/src/shape_matrix.xl').read_text(encoding='utf-8')
            assert len(re.findall(r'^export extern ', declarations, re.M)) == len(symbols)
            assert not re.search(r'^\s*end\s*$', declarations, re.M), 'Declarations must not grow wrapper bodies'
            for standard, tool, language in [('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++'), ('c++17', 'g++', 'c++')]:
                run('header-' + standard, [tools[tool], '-x', language, '-std=' + standard,
                    '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I' + str(root),
                    '-I' + str(sdk), *native_includes, '-fsyntax-only', header])
            assert 'xl6_export_matrixFixturePoint' in header.read_text(encoding='utf-8')
            report['directNativeSymbols'] = symbols
    report.update(passed=True, inputsUnchanged=all(sha(path) == digest for path, digest in inputs.items()),
                  heapClaim='1000 warmed multiply/first-class point calls: no XRT alloc/free/realloc operations',
                  remainingSharedLibraryGate='Package initializer declaration is still a separate design decision')
    assert report['inputsUnchanged']
    save()


if __name__ == '__main__':
    main()
