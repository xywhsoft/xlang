"""Strict resident semaphore owner/admission diagnostic proof.
Only new providers and hosts are instrumented. Cgen/IR integration, coherent
source/shipping products and full 53-module acceptance are NOT claimed.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'schema-root', 'compiler', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    root, schema, compiler, out = [getattr(args, name).resolve()
        for name in ('root', 'schema_root', 'compiler', 'out')]
    assert not out.exists() and (not args.sanitize or os.name != 'nt')
    project = Path(__file__).resolve().parents[1]
    cc = shutil.which('gcc'); assert cc
    manifest = compiler / ('overlay.json' if os.name == 'nt' else 'overlay.linux.json')
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']
    providers = ['xl6_semaphore', 'xl6_array_semaphore_admission']
    host = project / 'test/cases/stdlib_semaphore_native_host.c'
    files = {Path(__file__).resolve(), Path(cc), manifest, host,
        root / 'test/support/module_product_memory.c', root / 'test/support/module_product_memory.h'}
    for row in build['Inputs'] + build['Objects']:
        assert sha(row['Path']) == row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for path in (schema / 'src').rglob('*') if path.suffix in ('.h', '.c', '.inc'))
    files.update(project / 'src/runtime' / (name + suffix) for name, suffix in
        [(name, '.c') for name in providers] + [('xl6_semaphore', '.h'), ('xl6_semaphore_admission', '.h')])
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    out.mkdir(); rows = []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__,
        'SourceProductAcceptance': False, 'ShippingLibraryAcceptance': False, 'Full53Acceptance': False,
        'LanguageCgenAcceptance': False, 'ProvidersInstrumented': args.sanitize,
        'WholeRuntimeInstrumented': False, 'Inputs': inputs, 'Rows': rows}
    def save(): (out / 'semaphore.native.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(name, command):
        command = list(map(str, command)); result = subprocess.run(command, cwd=root, capture_output=True, timeout=180)
        data = result.stdout + result.stderr; (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode}); save()
        assert not result.returncode, (name, data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
        '-I' + str(schema), '-I' + str(project), '-I.', '-Irelease', '-Irelease/include']
    if args.sanitize: flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    objects = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        {'xl6_xlc_shell.o', 'xlir_public_source_driver.o', *[name + '.o' for name in providers]}]
    if os.name == 'nt':
        shutil.copy2(root / 'release/xl.dll', out / 'xl.dll'); suffix = '.exe'
        links = [*objects, str(root / 'release/xl.dll'), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        suffix = ''; links = [*objects, '-L' + str(root / 'release'), '-lxlc', '-lxl',
            '-Wl,-rpath,' + str(root / 'release'), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
    try:
        for level in ('O0', 'O2'):
            selected = []
            for name in providers:
                obj = out / (name + '.' + level + '.o'); selected.append(obj)
                run(name + '.' + level + '.build', [cc, *flags, '-' + level, '-c',
                    project / 'src/runtime' / (name + '.c'), '-o', obj])
            binary = out / ('native.' + level + suffix)
            run(level + '.build', [cc, *flags, '-' + level, host, 'test/support/module_product_memory.c',
                *selected, *links, '-o', binary])
            assert 'OOM balanced' in run(level + '.run', [binary])
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    except Exception as error: report['Error'] = repr(error); raise
    finally: save()
    print('Semaphore native: ' + str(len(rows)) + ' phases; owner/schema OOM and actual activity lifetimes balanced', flush=True)

if __name__ == '__main__': main()
