"""Internal container dependencies through actual source-free compiler products.
Real JIT/module/AOT execution and saved production native inputs, strict O0/O2
native objects and symbol inspection distinguish typed storage from legacy raw
stacks. No duplicate language classes, source/provider fallback or runtime patch.
Object-symbol checks do not claim a complete symbol table in stripped TCC AOT.
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

CASES = (
    ('literal', 'xlir_artifact_linkage_literal.xl', None),
    ('array', 'stdlib_array_create_language.xl', 'TYPED_ARRAY'),
    ('list', 'stdlib_list_methods.xl', 'TYPED_LIST'),
    ('tree', 'stdlib_tree_owned_queries.xl', 'TYPED_TREE'),
    ('stack', 'stdlib_stack_batch_public.xl', 'TYPED_STACK'),
    ('fixed-stack', 'stdlib_fixed_stack_only.xl', 'TYPED_STACK'),
)
INTERNAL = ('PTR_ARRAY', 'INT_MAP', 'AVL', 'AVL_TREE')
FORBIDDEN = {'STACK': 'xrtStack', 'PTR_STACK': 'xrtPtrStack',
             'BLOCK_STACK': 'xrtBlockStack', 'FIXED_STACK': 'xrtFixedStack',
             'PTR_FIXED_STACK': 'xrtPtrFixedStack'}
WITNESSES = {'PTR_ARRAY': 'xrtPtrArrayInit', 'INT_MAP': 'xrtIntMapInit',
             'AVL': 'xrtAVLInit', 'AVL_TREE': 'xrtAVLTreeInit'}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build-report', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists()
    windows = os.name == 'nt'
    cc, nm, inspector = [shutil.which(n) for n in ('gcc', 'nm', 'objdump' if windows else 'readelf')]
    assert cc and nm and inspector
    task = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(root / 'tools'))
    from gen_tcc_vfs_resources import load_resources
    build = json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit'] == 0
    products = [root / 'release' / n for n in
                (('xl.dll', 'xlc.dll') if windows else ('libxl.so', 'libxlc.so'))]
    recorded = {Path(r['Path']).resolve(): r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p) == sha(p) for p in products)
    host = task / 'test/cases/stdlib_internal_container_product_host.c'
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), Path(nm), Path(inspector),
             args.build_report.resolve(), host, *[root / 'test/cases' / c[1] for c in CASES]}
    for row in build['Inputs'] + build['Products']:
        assert sha(row['Path']) == row['Sha256'], row['Path']
        files.add(Path(row['Path']))
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(parents=True)
    isolated, sdk, temporary, relocated = [out / n for n in ('host', 'sdk', 'tmp', 'relocated')]
    release = isolated / 'release'
    release.mkdir(parents=True)
    for p in (sdk, temporary, relocated): p.mkdir()
    for p in products:
        shutil.copy2(p, release / p.name)
        if windows: shutil.copy2(p, isolated / p.name)
    for p in (root / 'release/include').glob('*.h'): shutil.copy2(p, sdk / p.name)
    build_dir = root / ('build/tmp' if windows else 'build/linux')
    manifest = root / 'tools' / ('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    dist = root / 'res/tcc/win64/x86_64' if windows else build_dir / 'tcc-dist'
    for resource in load_resources(manifest, root, dist, build_dir / 'xrt.o', build_dir):
        if resource.virtual.startswith('include/__xl_runtime/'):
            target = sdk / Path(resource.virtual).relative_to('include')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(resource.source, target)
    shutil.copy2(root / 'lib/xruntime/xruntime.h', sdk / 'xruntime.h')
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
           ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}
    env.update(TMPDIR=str(temporary), TEMP=str(temporary), TMP=str(temporary))
    rows, proofs, controls, failures = [], [], [], []
    report = dict(Complete=False, InputsUnchanged=False, Scope=__doc__, Inputs=inputs, Rows=rows,
                  Proofs=proofs, Counterexamples=controls, Failures=failures,
                  ProductionCodeChanged=False, RuntimeInstrumented=False, SourceProviderFallback=False,
                  NativeObjectsRebuiltForSymbolInspection=True, NativeInternalClosureAcceptance=False,
                  EntireAotSymbolTableAcceptance=False, Full53Acceptance=False)

    def run(tag, argv):
        argv = list(map(str, argv)); start = time.monotonic()
        try:
            result = subprocess.run(argv, cwd=isolated, env=env, capture_output=True, timeout=180)
            code, data = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as error:
            code, data = 'timeout', (error.stdout or b'') + (error.stderr or b'')
        log = out / (tag + '.log'); log.write_bytes(data)
        rows.append(dict(Name=tag, Command=argv, Exit=code, Seconds=time.monotonic() - start,
                         Log=str(log), LogSha256=sha(log)))
        assert code == 0, (tag, data[-6000:])
        return data.decode(errors='replace')

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I' + str(sdk)]
    if not windows: flags += ['-D_GNU_SOURCE']
    compat = ['-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
              '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
              '-Wno-missing-field-initializers', '-Wno-pointer-sign']

    def features(tag, source):
        text = run(tag + '.features', [cc, *flags, '-E', '-dM', source])
        return set(re.findall(r'^#define ((?:XRT|XRUNTIME)_FEATURE_[A-Z0-9_]+)\b', text, re.M))

    def unwanted(selected, symbols):
        return [n for n, prefix in FORBIDDEN.items() if 'XRT_FEATURE_' + n in selected or
                any(s.startswith(prefix) for s in symbols)]

    try:
        compiler = release / products[1].name
        binary = isolated / ('observer' + ('.exe' if windows else ''))
        links = [compiler] if windows else ['-L' + str(release), '-lxlc', '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']
        run('host.build', [cc, *flags, '-O2', host, *links, '-o', binary])
        assert not any((isolated / n).exists() for n in ('src', 'lib', 'res', 'include'))
        for label, filename, typed in CASES:
            try:
                source = isolated / filename; shutil.copy2(root / 'test/cases' / filename, source)
                image = isolated / (label + ('.exe' if windows else ''))
                for mode in ('run', 'module', 'aot'):
                    work = temporary / (label + '-' + mode); work.mkdir()
                    assert 'internal-container ' + mode + ' complete' in run(label + '.' + mode, [
                        binary, compiler, source, mode, work, image])
                native, generated = work / 'xl6_xrt_impl.c', work / 'module.c'
                assert native.is_file() and generated.is_file()
                assert not re.search(r'^#define (?:XRT|XRUNTIME)_MODULE_ALL\b', native.read_text(), re.M)
                selected = features(label, native)
                if typed: assert 'XRUNTIME_FEATURE_' + typed in selected, (label, typed)
                actual = [n for n in INTERNAL if 'XRT_FEATURE_' + n in selected]
                assert not unwanted(selected, set()), label
                symbol_counts = {}
                for opt in ('O0', 'O2'):
                    obj = out / (label + '.' + opt + '.o')
                    run(label + '.' + opt + '.native', [cc, *flags, *compat, '-' + opt, '-c', native, '-o', obj])
                    names = set(line.split()[-1] for line in run(label + '.' + opt + '.symbols', [nm, '--defined-only', obj]).splitlines() if line.split())
                    assert not unwanted(selected, names), label
                    for module in actual: assert WITNESSES[module] in names, (module, WITNESSES[module])
                    symbol_counts[opt] = len(names)
                    run(label + '.' + opt + '.generated', [cc, *flags, *compat, '-' + opt, '-c', generated,
                                                          '-o', out / (label + '.' + opt + '.generated.o')])
                moved = relocated / image.name; shutil.move(image, moved)
                imports = run(label + '.imports', [inspector, '-p' if windows else '-d', moved])
                dependencies = re.findall(r'DLL Name:\s*(\S+)', imports) if windows else re.findall(r'\(NEEDED\).*?\[(.*?)\]', imports)
                assert not any(n.lower() in ('xl.dll', 'xlc.dll') or 'libxl' in n.lower() for n in dependencies)
                assert windows or not re.search(r'\((?:RPATH|RUNPATH)\)', imports)
                run(label + '.relocated', [moved])
                proofs.append(dict(Name=label, Fixture=filename, Jit=True, Module=True, RelocatedAot=True,
                                   InternalModules=actual, TypedFeature=typed, ObjectSymbolCounts=symbol_counts,
                                   LegacyRawStacksExcluded=True, NativeSource=dict(Path=str(native), Sha256=sha(native))))
                print(label + ': passed', flush=True)
            except Exception as error: failures.append(dict(Name=label, Error=repr(error)))
        # Explicit retained-code controls, not compiler failures. The same
        # feature/symbol predicate must detect each unwanted legacy family.
        seed = Path(proofs[-1]['NativeSource']['Path']) if proofs else None
        if seed:
            for module in ('PTR_STACK', 'BLOCK_STACK', 'PTR_FIXED_STACK'):
                try:
                    tag = 'retained-' + module.lower(); changed = out / (tag + '.c')
                    changed.write_text('#define XRT_MODULE_' + module + '\n' + seed.read_text())
                    selected = features(tag, changed); obj = out / (tag + '.o')
                    run(tag + '.compile', [cc, *flags, *compat, '-O0', '-c', changed, '-o', obj])
                    names = set(line.split()[-1] for line in run(tag + '.symbols', [nm, '--defined-only', obj]).splitlines() if line.split())
                    assert any(s.startswith(FORBIDDEN[module]) for s in names)
                    assert module in unwanted(selected, names)
                    controls.append(dict(Module=module, Compiled=True, UnwantedSymbolsRejected=True))
                except Exception as error: failures.append(dict(Name=module, Error=repr(error)))
        covered = set().union(*(set(p['InternalModules']) for p in proofs))
        assert covered == set(INTERNAL), covered
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        report['NativeInternalClosureAcceptance'] = len(proofs) == len(CASES) and len(controls) == 3 and not failures
        report['Complete'] = report['InputsUnchanged'] and report['NativeInternalClosureAcceptance']
    except Exception as error: failures.append(dict(Name='gate', Error=repr(error)))
    report['PhaseCount'] = len(rows)
    report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    (out / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'], Proofs=len(proofs), Failures=failures)), flush=True)
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
