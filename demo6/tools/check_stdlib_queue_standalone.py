"""Independent SPSC/MPSC/MPMC and plain-SPSC standalone products.

Real public TCC output is executed. The exact saved module/XRT source and
source-derived executable plan are independently linked with GCC at O0/O2,
without compiler objects or shared xlang libraries, then executed and inspected
for exact family symbols/dependencies. TCC outputs have no symbol table: this
does not claim TCC symbol-pruning or rebuilt source/shipping/full-53 acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'schema-root', 'compiler', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, schema, compiler, out = [getattr(args, name).resolve()
        for name in ('root', 'schema_root', 'compiler', 'out')]
    assert not out.exists()
    windows = os.name == 'nt'
    cc, nm, inspect = [shutil.which(name) for name in
        ('gcc', 'nm', 'objdump' if windows else 'readelf')]
    assert cc and nm and inspect
    manifest = compiler / ('overlay.json' if windows else 'overlay.linux.json')
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and not build['SourceProductAcceptance']
    project = Path(__file__).resolve().parents[1]
    plan_host = project / 'test/cases/xlir_standalone_runtime_plan.c'
    public_host = project / 'test/cases/stdlib_standalone_public_driver.c'
    fixtures = [(family, project / 'test/cases' /
        ('stdlib_tree_' + family.lower() + '_values.xl'), True)
        for family in ('Spsc', 'Mpsc', 'Mpmc')]
    fixtures.append(('Spsc', root / 'test/cases/stdlib_concurrent_queue_spsc_only.xl', False))
    runtime = root / 'release' / ('xl.dll' if windows else 'libxl.so')
    powershell = (shutil.which('pwsh') or shutil.which('powershell')) if windows else None
    assert not windows or powershell
    files = {Path(__file__).resolve(), Path(cc), Path(nm), Path(inspect), manifest,
        plan_host, public_host, runtime, *[fixture for _, fixture, _ in fixtures]}
    if windows:
        files.add(Path(powershell))
    if not windows:
        files.add(root / 'release/libxlc.so')
    for row in build['Inputs'] + build['Objects']:
        assert sha(row['Path']) == row['Sha256'].lower()
        files.add(Path(row['Path']))
    for folder in (schema / 'src', schema / 'lib', schema / 'release/include', compiler / 'include'):
        files.update(path for path in folder.rglob('*') if path.is_file())
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    package = out / 'package'
    package.mkdir()
    for name, source in [('src', schema / 'src'), ('lib', schema / 'lib'),
            ('release', root / 'release'), ('res', root / 'res'), ('include', compiler / 'include')]:
        if source.exists():
            if windows:
                # Directory junctions need no Windows symlink privilege. These
                # fresh links are read-only inputs; never delete/move targets.
                quote = lambda value: "'" + str(value).replace("'", "''") + "'"
                subprocess.run([powershell, '-NoProfile', '-NonInteractive', '-Command',
                    'New-Item -ItemType Junction -Path ' + quote(package / name) +
                    ' -Target ' + quote(source) + ' | Out-Null'], check=True)
            else:
                (package / name).symlink_to(source, target_is_directory=True)
    tmp = out / 'tmp'
    tmp.mkdir()
    env = dict(os.environ, TMPDIR=str(tmp), TEMP=str(tmp), TMP=str(tmp))
    rows, artifacts = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__,
        'SourceProductAcceptance': False, 'ShippingLibraryAcceptance': False,
        'Full53Acceptance': False, 'StandaloneGccLinkPruningAcceptance': False,
        'TccSymbolPruningAcceptance': False, 'TccStandaloneRunAcceptance': False,
        'Inputs': inputs, 'Rows': rows, 'Artifacts': artifacts}

    def save():
        (out / 'queue.standalone.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(name, command, cwd=package):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, timeout=240)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode,
            'Seconds': time.monotonic() - start})
        save()
        assert not result.returncode, (name, data[-7000:].decode(errors='replace'))
        return data.decode(errors='replace')

    compat = ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
        '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-Wno-pointer-sign', '-Wno-format-truncation']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
        '-I' + str(package), '-I' + str(package / 'src'), '-I' + str(package / 'release'),
        '-I' + str(package / 'release/include'), '-I' + str(package / 'include'),
        '-I' + str(package / 'lib/xruntime'), '-I' + str(package / 'lib/xrt')]
    if not windows:
        flags.append('-D_GNU_SOURCE')
    suffix = '.exe' if windows else ''
    try:
        objects = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
            ('xlir_public_source_driver.o', 'xl6_xlc_shell.o', 'xl_api_path.o')]
        path_object = out / 'xl_api_path.o'
        run('path.build', [cc, *flags, *compat, '-O0', '-DXL_ENABLE_EXE_COMPILER=1',
            '-DXL_STATIC_TCC', '-DTCC_DISABLE_BUILTIN_VFS', '-c',
            schema / 'src/api/xl_api_path.c', '-o', path_object])
        objects.append(str(path_object))
        if windows:
            shutil.copy2(runtime, package / 'xl.dll')
            libraries = [str(runtime), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
            standalone_libraries = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32', '-lm']
        else:
            libraries = ['-L' + str(root / 'release'), '-lxlc', '-lxl', '-Wl,-rpath,' + str(root / 'release'),
                '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
            standalone_libraries = ['-pthread', '-ldl', '-lm']
        observer = package / ('plan' + suffix)
        driver = package / ('public' + suffix)
        run('plan.build', [cc, *flags, *compat, '-O0', plan_host, *objects, *libraries, '-o', observer])
        run('public.build', [cc, *flags, '-O0', public_host, *objects, *libraries, '-o', driver])
        report['Observer'] = {'Path': str(observer), 'Sha256': sha(observer)}
        report['PublicDriver'] = {'Path': str(driver), 'Sha256': sha(driver)}
        families = ('Spsc', 'Mpsc', 'Mpmc')
        cached = {}
        for family, fixture, recursive in fixtures:
            name = fixture.stem
            native_tmp = out / (name + '.temps')
            native_tmp.mkdir()
            tcc_binary = out / (name + '.tcc' + suffix)
            text = run(name + '.plan', [observer, fixture])
            sources = [line.removeprefix('SOURCE:') for line in text.splitlines() if line.startswith('SOURCE:')]
            modules = [line.removeprefix('NATIVE:') for line in text.splitlines() if line.startswith('NATIVE:')]
            assert sources and len(sources) == len(set(sources)) and modules
            assert ('src/runtime/xl6_queue_' + family.lower() + '.c') in sources
            assert ('src/runtime/xl6_queue_' + family.lower() + '_admission.c' in sources) == recursive
            for other in families:
                assert ('XRUNTIME_MODULE_TYPED_QUEUE_' + other.upper() in modules) == (other == family)
            run(name + '.public.emit', [driver, package, native_tmp, runtime, fixture, tcc_binary, package / 'include'])
            run(name + '.tcc.execute', [tcc_binary])
            generated, implementation = native_tmp / 'module.c', native_tmp / 'xl6_xrt_impl.c'
            assert generated.exists() and implementation.exists()
            actual_modules = re.findall(r'^#define ((?:XRT|XRUNTIME)_MODULE_\w+)\s*$', implementation.read_text(), re.M)
            assert set(actual_modules) == set(modules)
            record = {'Fixture': str(fixture), 'Family': family, 'RecursiveAdmission': recursive,
                'TccPath': str(tcc_binary), 'TccSha256': sha(tcc_binary), 'TccBytes': tcc_binary.stat().st_size,
                'GeneratedC': {'Path': str(generated), 'Sha256': sha(generated)},
                'SelectedXrtC': {'Path': str(implementation), 'Sha256': sha(implementation)},
                'ProviderSources': sources, 'NativeModules': modules, 'GccProducts': []}
            artifacts.append(record)
            for level in ('O0', 'O2'):
                native_objects = []
                for source in sources:
                    key = (source, level)
                    if key not in cached:
                        obj = out / (Path(source).stem + '.' + level + '.o')
                        run(Path(source).stem + '.' + level + '.build', [cc, *flags, '-' + level,
                            '-c', package / source, '-o', obj])
                        cached[key] = obj
                    native_objects.append(cached[key])
                module_object = out / (name + '.' + level + '.module.o')
                xrt_object = out / (name + '.' + level + '.xrt.o')
                run(name + '.' + level + '.module.build', [cc, *flags, *compat, '-Wshadow', '-' + level,
                    '-c', generated, '-o', module_object])
                run(name + '.' + level + '.xrt.build', [cc, *flags, *compat, '-' + level,
                    '-c', implementation, '-o', xrt_object])
                binary = out / (name + '.' + level + suffix)
                run(name + '.' + level + '.link', [cc, module_object, xrt_object, *native_objects,
                    *standalone_libraries, '-o', binary])
                run(name + '.' + level + '.execute', [binary])
                symbols_text = run(name + '.' + level + '.symbols', [nm, '-g', '--defined-only', binary])
                symbols = {line.split()[-1] for line in symbols_text.splitlines() if line.strip()}
                assert 'main' in symbols and 'xl6' + family + 'QueueCreateTakeV1' in symbols
                assert ('xl6Sequence' + family + 'QueueCreateTakeV1' in symbols) == recursive
                selected = sorted(symbol for symbol in symbols if symbol.startswith(('xl6', 'xrt')) and
                    any(token.lower() in symbol.lower() for token in families))
                assert any(symbol.startswith('xrt') and family.lower() in symbol.lower() for symbol in selected)
                # Typed MPSC uses a real raw MPMC ring for free-slot recycling:
                # multiple producers take/return slots, including rollback.
                # This is required kernel code, not another language/typed
                # queue family. Do not remove that genuine dependency merely
                # to make an all-symbol substring assertion pass.
                facade_symbols = [symbol for symbol in selected if symbol.startswith(('xl6', 'xrtTyped'))]
                assert not any(other.lower() in symbol.lower() for symbol in facade_symbols
                    for other in families if other != family)
                expected_kernels = {'Spsc': {'Spsc'}, 'Mpsc': {'Mpsc', 'Mpmc'}, 'Mpmc': {'Mpmc'}}[family]
                kernel_families = {token for token in families if
                    any(symbol.startswith('xrt' + token.upper() + 'Queue') for symbol in selected)}
                assert kernel_families == expected_kernels
                assert not any(symbol.startswith(('xlc', 'xl6Ir', 'xl6Parse')) for symbol in symbols)
                dynamic = run(name + '.' + level + '.dependencies',
                    [inspect, '-p' if windows else '-d', binary])
                dependencies = re.findall(r'DLL Name: (\S+)' if windows else r'Shared library: \[([^\]]+)\]', dynamic)
                assert not any(re.match(r'(?:lib)?xl(?:c)?\.(?:so|dll)', name, re.I) for name in dependencies)
                assert not re.search(r'\bTEXTREL\b', dynamic)
                record['GccProducts'].append({'Level': level, 'Path': str(binary), 'Sha256': sha(binary),
                    'Bytes': binary.stat().st_size, 'QueueDefinedSymbols': selected, 'NeededLibraries': dependencies,
                    'RequiredRawQueueKernels': sorted(kernel_families),
                    'ProviderObjects': [{'Path': str(obj), 'Sha256': sha(obj)} for obj in native_objects]})
                save()
            print(name + ': public TCC execution and O0/O2 standalone exact-family symbols verified', flush=True)
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['StandaloneGccLinkPruningAcceptance'] = True
        report['TccStandaloneRunAcceptance'] = True
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        raise
    finally:
        save()
    print('Queue standalone: ' + str(len(rows)) + ' phases; four TCC and eight independent GCC products passed', flush=True)


if __name__ == '__main__':
    main()
