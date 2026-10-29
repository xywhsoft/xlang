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
    instrumentation=parser.add_mutually_exclusive_group()
    instrumentation.add_argument('--sanitize', action='store_true')
    instrumentation.add_argument('--race', action='store_true')
    parser.add_argument('--provider-root',type=Path)
    parser.add_argument('--expect-race',action='store_true',help='negative control only: require a real TSan race in the selected old provider')
    args = parser.parse_args()
    root, schema, compiler, out = [getattr(args, name).resolve()
        for name in ('root', 'schema_root', 'compiler', 'out')]
    assert not out.exists() and (not (args.sanitize or args.race) or os.name != 'nt')
    project = Path(__file__).resolve().parents[1]
    provider=(args.provider_root.resolve() if args.provider_root else project)
    assert not args.expect_race or (args.race and args.provider_root)
    cc = shutil.which('gcc'); assert cc
    manifest = compiler / ('overlay.json' if os.name == 'nt' else 'overlay.linux.json')
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']
    providers = ['xl6_semaphore', 'xl6_array_semaphore_admission']
    host = project / 'test/cases/stdlib_semaphore_native_host.c'
    files = {Path(__file__).resolve(), Path(cc), manifest, host,
        root / 'test/support/module_product_memory.c', root / 'test/support/module_product_memory.h'}
    # Native diagnostics do not link compiler/IR objects. Verify only the
    # selected neutral boundary override, genuine runtime and included headers.
    selected = [row for row in build['Objects'] if Path(row['Path']).name == 'xl_foreign_error.o']
    assert len(selected) <= 1
    for row in selected:
        assert sha(row['Path']) == row['Sha256'].lower(); files.add(Path(row['Path']))
    files.update(path for folder in ('src/runtime', 'src/api') for path in (project / folder).glob('*.h'))
    files.update(path for folder in ('lib/xrt', 'lib/xruntime') for path in (project / folder).glob('*.h'))
    files.update(path for path in (root / 'release/include').glob('*.h'))
    files.update(path for path in (root / 'release').iterdir() if path.is_file() and
        (path.name == 'xl.dll' or path.name.startswith('libxl.so')))
    files.update(provider / 'src/runtime' / (name + suffix) for name, suffix in
        [(name, '.c') for name in providers] + [('xl6_semaphore', '.h'), ('xl6_semaphore_admission', '.h')])
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    out.mkdir(); rows = []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__,
        'SourceProductAcceptance': False, 'ShippingLibraryAcceptance': False, 'Full53Acceptance': False,
        'LanguageCgenAcceptance': False, 'ProvidersInstrumented': args.sanitize or args.race,
        'ProviderInstrumentation': 'thread' if args.race else 'address,undefined' if args.sanitize else None,
        'KnownBadProviderRaceDetected':False,
        'RaceStressOnly':args.race,
        'WholeRuntimeInstrumented': False, 'Inputs': inputs, 'Rows': rows}
    def save(): (out / 'semaphore.native.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(name, command,success=True):
        command = list(map(str, command)); result = subprocess.run(command, cwd=root, capture_output=True, timeout=180)
        data = result.stdout + result.stderr; (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode}); save()
        assert (result.returncode==0)==success, (name, data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
        '-I' + str(project), '-I' + str(schema), '-I.', '-Irelease', '-Irelease/include']
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']
    if args.sanitize: flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    if args.race: flags += ['-g', '-fsanitize=thread', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
    objects = [row['Path'] for row in selected]
    if os.name == 'nt':
        shutil.copy2(root / 'release/xl.dll', out / 'xl.dll'); suffix = '.exe'
        links = [*objects, str(root / 'release/xl.dll'), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        suffix = ''; links = [*objects, '-L' + str(root / 'release'), '-lxl',
            '-Wl,-rpath,' + str(root / 'release'), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
    try:
        for level in ('O0', 'O2'):
            selected = []
            for name in providers:
                obj = out / (name + '.' + level + '.o'); selected.append(obj)
                run(name + '.' + level + '.build', [cc, *flags, '-' + level, '-c',
                    provider / 'src/runtime' / (name + '.c'), '-o', obj])
            binary = out / ('native.' + level + suffix)
            run(level + '.build', [cc, *flags, '-' + level, host, 'test/support/module_product_memory.c',
                *selected, *links, '-o', binary])
            result=run(level+'.run',[binary,*(['--race-stress-only'] if args.race else [])],not args.expect_race)
            if args.expect_race:
                assert 'WARNING: ThreadSanitizer: data race' in result and 'xl6_semaphore.c' in result
                assert 'concurrentWorker' in result and ' in count' not in result
            else: assert 'OOM balanced' in result
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
        report['KnownBadProviderRaceDetected']=args.expect_race
    except Exception as error: report['Error'] = repr(error); raise
    finally: save()
    proof = 'old provider race reproduced' if args.expect_race else 'native owner/activity diagnostics passed'
    print('Semaphore native: ' + str(len(rows)) + ' phases; ' + proof, flush=True)

if __name__ == '__main__': main()
