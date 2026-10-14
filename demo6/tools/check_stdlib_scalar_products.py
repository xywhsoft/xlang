"""Safe numeric constructors and exact projection through real shipping products.

Uses ten existing assertion fixtures via the CLI, public relocated-module API,
and independently moved AOT executables. No source/header/provider fallback.
This is the numeric subset, not full standard-library or sanitizer acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys

FIXTURES = (
    'stdlib_native_scalar_convert', 'stdlib_native_scalar_as', 'stdlib_native_scalar_errors',
    'stdlib_native_scalar_as_only', 'stdlib_native_scalar_reflection',
    'stdlib_native_scalar_generic', 'stdlib_native_scalar_fields',
    'stdlib_scalar_conversion_reflection', 'stdlib_scalar_conversion_length', 'stdlib_float32_as',
)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    windows = os.name == 'nt'
    compiler = shutil.which('gcc')
    inspector = shutil.which('objdump' if windows else 'readelf')
    assert compiler and inspector
    products = [root / 'release' / name for name in
                (('xl.exe', 'xl.dll', 'xlc.dll') if windows else ('xl', 'libxl.so', 'libxlc.so'))]
    recorded = {Path(row['Path']).resolve(): row['Sha256'] for row in build['Products']}
    assert all(sha(path) == recorded[path] for path in products)
    task = Path(__file__).resolve().parents[1]
    fixtures = [task / 'test/cases' / (name + '.xl') for name in FIXTURES]
    host = task / 'test/cases/stdlib_product_fixture_host.c'
    files = {Path(__file__).resolve(), Path(sys.executable).resolve(), Path(compiler).resolve(),
             Path(inspector).resolve(), args.build_report.resolve(), host, *fixtures}
    for row in build['Inputs'] + build['Products']:
        assert sha(row['Path']) == row['Sha256'], row['Path']
        files.add(Path(row['Path']))
    inputs = [dict(Path=str(path), Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(parents=True)
    isolated, relocated = out / 'compiler', out / 'relocated'
    (isolated / 'release').mkdir(parents=True)
    (isolated / 'inputs').mkdir()
    relocated.mkdir()
    tmp = out / 'tmp'
    tmp.mkdir()
    for product in products:
        shutil.copy2(product, isolated / 'release' / product.name)
        if windows:
            shutil.copy2(product, isolated / product.name)
    for fixture in fixtures:
        shutil.copy2(fixture, isolated / 'inputs' / fixture.name)
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith('XL6_')
           and key not in ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp), TMP=str(tmp), TEMP=str(tmp))
    rows, failures, cases = [], [], []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows, Cases=cases,
                  Failures=failures, Scope=__doc__, ExplicitSubset=True, CandidateCount=len(FIXTURES),
                  ProviderOverrides=False, EmbeddedVfsWithoutFallback=True, NumericProductAcceptance=False,
                  RuntimeInstrumented=False, Full53Acceptance=False, ShippingLibraryAcceptance=False)

    def run(name, command, directory=isolated):
        try:
            process = subprocess.run(list(map(str, command)), cwd=directory, env=env,
                                     capture_output=True, timeout=240)
            data, code = process.stdout + process.stderr, process.returncode
        except subprocess.TimeoutExpired as error:
            data, code = (error.stdout or b'') + (error.stderr or b''), 'timeout'
        log = out / (name + '.log')
        log.write_bytes(data)
        rows.append(dict(Name=name, Exit=code, Command=list(map(str, command)),
                         Directory=str(directory), Log=str(log), LogSha256=sha(log)))
        assert code == 0, (name, data[-5000:].decode(errors='replace'))
        return data

    try:
        binary = isolated / ('fixture-host' + ('.exe' if windows else ''))
        libs = [isolated / 'release/xlc.dll'] if windows else [
            '-L' + str(isolated / 'release'), '-lxlc', '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']
        run('fixture-host-build', [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
            '-DXL_STATIC_EMBED', '-I' + str(root / 'release/include'), host, *libs, '-o', binary])
        cli = isolated / 'release' / products[0].name
        for fixture in fixtures:
            name = fixture.stem
            source = isolated / 'inputs' / fixture.name
            record = dict(Name=name, Passed=False, Rows=[])
            begin = len(rows)
            try:
                run(name + '.check', [cli, 'check', source])
                run(name + '.run', [cli, 'run', source])
                run(name + '.module', [binary, isolated / 'release' / products[2].name,
                                      source, 'module', tmp / name])
                image = isolated / (name + ('.aot.exe' if windows else '.aot'))
                run(name + '.aot-build', [cli, 'build', source, '-o', image])
                independent = relocated / image.name
                shutil.copy2(image, independent)
                imports = run(name + '.aot-imports', [inspector, '-p' if windows else '-d', independent], relocated)
                dependencies = (re.findall(rb'DLL Name:\s*([^\s]+)', imports) if windows else
                                re.findall(rb'Shared library:\s*\[([^\]]+)\]', imports))
                assert dependencies and not {name.decode().lower() for name in dependencies} & {path.name.lower() for path in products}
                if not windows:
                    assert b'(RPATH)' not in imports and b'(RUNPATH)' not in imports
                record['AotDependencies'] = [name.decode() for name in dependencies]
                run(name + '.aot-relocated', [independent], relocated)
                record['Passed'] = True
            except Exception as error:
                failures.append(dict(Name=name, Error=repr(error)))
            record['Rows'] = rows[begin:]
            cases.append(record)
            print(name + ': ' + str(record['Passed']), flush=True)
        assert not any((isolated / name).exists() for name in ('src', 'lib', 'res', 'include'))
        assert all(path.is_file() and path.suffix != '.dll' for path in relocated.iterdir())
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = report['InputsUnchanged'] and not failures and len(cases) == 10 and len(rows) == 61
        report['NumericProductAcceptance'] = report['Complete']
        report['AotWithoutProductDependencies'] = report['Complete']
    except Exception as error:
        failures.append(dict(Name='preparation', Error=repr(error)))
    report['PhaseCount'] = len(rows)
    report['Products'] = [dict(Path=str(path), Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
