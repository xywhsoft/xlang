"""Actual source XLIR unified value domains, Set key ABI and native target factory.

Uses all freshly built compiler/runtime objects on BOTH platforms.
Set language producer/consumer cutover is NOT activated by this target gate.
Shared List recipes keep their source authority; unsupported domains remain
closed. Native targets use actual XRT typed Set, not a wire retagged owner.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, public_dir, out = args.root.resolve(), args.public_directory.resolve(), args.out.resolve()
    assert not out.exists()
    manifest = public_dir / 'build.json'
    public = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert public['SourceXrt'] and not public['RuntimeOnly'] and public['EnableExeCompiler'] == 1
    if os.name != 'nt':
        assert public['Complete'] and public['InputsUnchanged']
    cc = shutil.which('gcc')
    assert cc
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), manifest}
    for row in public['Inputs'] + public['Headers'] + public['Objects']:
        assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']
        files.add(Path(row['Path']))
    driver = Path(public['Driver'])
    assert sha(driver).lower() == public['DriverSha256'].lower()
    product = root / 'release/xl.dll' if os.name == 'nt' else Path(public['NativeRoot']) / 'release/libxl.so'
    fixture = root / 'test/cases/stdlib_set_recipes.xl'
    nested_fixture = root / 'test/cases/stdlib_set_schema_source.xl'
    contract = root / 'test/cases/xlir_set_recipe_contract.c'
    host = root / 'test/cases/stdlib_set_recipe_host.c'
    language_host = root / 'test/cases/xlir_module_product_oom_host.c'
    memory = root / 'test/support/module_product_memory.c'
    files.update((driver, product, fixture, nested_fixture, contract, host, language_host, memory,
                  root / 'test/support/module_product_memory.h'))
    inputs = [{'Path': str(f), 'Sha256': sha(f)} for f in sorted(files)]
    out.mkdir(parents=True)
    if os.name == 'nt':
        shutil.copy2(product, out / 'xl.dll')
    environment = os.environ.copy()
    if os.name != 'nt':
        environment['XL6_SOURCE_NATIVE_ROOT'] = public['NativeRoot']
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I' + str(root), '-I' + str(root / 'release'), '-I' + str(root / 'release/include'),
             '-I' + str(root / 'lib/tcc'), '-I' + str(public_dir / 'include'),
             '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
             '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
             '-Wno-missing-field-initializers', '-Wno-pointer-sign']
    objects = [r['Path'] for r in public['Objects'] if Path(r['Path']).name not in
               ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
    if os.name == 'nt':
        links = [str(product), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        flags += ['-D_GNU_SOURCE']
        links = [str(product), '-Wl,-rpath,' + str(product.parent), '-pthread', '-ldl', '-lm']
    rows = []
    report = {'Complete': False, 'InputsUnchanged': False, 'Scope': __doc__, 'Platform': sys.platform,
              'Inputs': inputs, 'Rows': rows, 'LanguageSetMigrationComplete': False,
              'UnifiedValueCollectionRecipe': True, 'LanguageFactoryCutover': False,
              'Products': [], 'Objects': [],
              'Full53Complete': False, 'ShippingProductAcceptance': False, 'Sanitized': False,
              'LinkedSourceCompilerOptimization': 'O0', 'ClosedPrefixSweeps': [], 'ScopeEntrySweeps': []}
    suffix = '.exe' if os.name == 'nt' else ''

    def run(name, command):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=root, env=environment, capture_output=True, timeout=240)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode,
                     'Seconds': time.monotonic() - start})
        print(name + ': ' + str(result.returncode), flush=True)
        if result.returncode:
            raise RuntimeError(name + ': ' + data[-6000:].decode(errors='replace'))
        return data.decode(errors='replace')

    try:
        binary = out / ('contract' + suffix)
        run('contract.build', [cc, *flags, '-O0', contract,
            *(o for o in objects if Path(o).name != 'xl6_ir_cgen.o'), *links, '-o', binary])
        generated = out / 'targets.c'
        log = run('contract.verify', [binary, fixture, generated])
        match = re.search(r'Set targets: (\d+) complete, (\d+) rejected mutations, (\d+) pending, '
                          r'(\d+) descendants, (\d+) same-slot, (\d+) erased, layouts=(\d+)', log)
        assert match, log
        recipes, mutations, pending, descendants, same_slot, erased, mask = map(int, match.groups())
        assert recipes >= 40 and mutations == recipes * 21 and pending >= 3
        assert descendants and same_slot and erased
        assert {2, 3, 4, 7, 8, 9, 10, 11, 12, 14, 20, 21} <= {i for i in range(32) if mask & (1 << i)}, mask
        report['Proof'] = dict(Recipes=recipes, Mutations=mutations, Pending=pending,
                              DescendantMutations=descendants, SameSlot=same_slot,
                              Erased=erased, LayoutMask=mask)
        text = generated.read_text(encoding='utf-8')
        probe = text.split('/* CLOSED_SET_TARGET_NATIVE_PROOF */', 1)[1]
        factory_match = re.search(r'factories=(\d+)', log); assert factory_match
        factories = int(factory_match[1]); assert factories >= recipes
        report['Proof']['ExpectedFactories'] = factories
        assert probe.count('xl6SetCreateTakeV1(') == factories
        assert probe.count('xl6SetProjectionTakeV1(') == factories
        assert 'abort(' not in probe and 'for (' not in probe and 'while (' not in probe
        assert not any(name in probe for name in ('xrtValueSet(', 'xrtTypedSet', 'xrtValueArray('))
        for level in (0, 2):
            lane = f'O{level}'
            memory_object = out / (lane + '.memory.o')
            obj = out / (lane + '.targets.o')
            native = out / (lane + '.targets' + suffix)
            run(lane + '.memory', [cc, *flags, f'-O{level}', '-c', memory, '-o', memory_object])
            run(lane + '.compile', [cc, *flags, f'-O{level}',
                '-Dxl6ErrorScopeBegin=xl6_set_recipe_scope_begin', '-c', generated, '-o', obj])
            run(lane + '.build', [cc, *flags, f'-O{level}', host, obj, memory_object,
                                  *objects, *links, '-o', native])
            assert 'balanced' in run(lane + '.vectors', [native, 'vectors'])
            log = run(lane + '.prefixes', [native, 'oom'])
            prefix = re.search(r'closed-batch runs=(\d+) first-no-hit=(\d+)', log)
            assert prefix and int(prefix[1]) == int(prefix[2]) + 1 and int(prefix[2]) > 0
            points = re.findall(r'Set target allocation point=(\d+) hit=(\d) balanced', log)
            assert points == [(str(i), str(int(i < int(prefix[2])))) for i in range(int(prefix[1]))]
            report['ClosedPrefixSweeps'].append(dict(Optimization=lane, Runs=int(prefix[1]),
                                                      FirstNoHit=int(prefix[2])))
            log = run(lane + '.scope-entries', [native, 'scopes'])
            prefix = re.search(r'closed-batch runs=(\d+) first-no-hit=(\d+)', log)
            assert prefix and int(prefix[1]) == int(prefix[2]) + 1 and int(prefix[2]) > recipes
            points = re.findall(r'Set target scope point=(\d+) hit=(\d) balanced', log)
            assert points == [(str(i), str(int(i < int(prefix[2])))) for i in range(int(prefix[1]))]
            report['ScopeEntrySweeps'].append(dict(Optimization=lane, Runs=int(prefix[1]),
                                                  FirstNoHit=int(prefix[2])))
        # A source pool containing Set schemas must not, by itself, activate
        # language native Set or pull a target factory into an unused product.
        source_c, repeat = out / 'language.c', out / 'language.repeat.c'
        run('language.cgen', [driver, fixture, 'cgen-default', source_c])
        run('language.repeat', [driver, fixture, 'cgen-default', repeat])
        assert sha(source_c) == sha(repeat)
        assert 'xl6SetCreateTakeV1(' not in source_c.read_text(encoding='utf-8-sig')
        for level in (0, 2):
            lane = f'language.O{level}'
            native = out / (lane + suffix)
            run(lane + '.compile', [cc, *flags, f'-O{level}', '-DXL6_PRODUCT_USER_ENTRY',
                '-include', source_c, language_host, out / f'O{level}.memory.o',
                *objects, *links, '-o', native])
            assert 'balanced' in run(lane + '.run', [native])
        run('language.jit', [driver, fixture, 'run', out / 'language.jit.result'])
        # Actual Array/Stack producers can use a complete Set reference schema.
        # Set factories below create consumed metadata witnesses, not source Set
        # instances. The fixture never relies on legacy wire-Set admission.
        nested_c = out / 'nested.language.c'
        run('nested.language.cgen', [driver, nested_fixture, 'cgen-default', nested_c])
        nested_text = nested_c.read_text(encoding='utf-8-sig')
        assert 'xl6SequenceSetCreateTakeV1(' in nested_text and 'xl6SetCreateTakeV1(' in nested_text
        assert 'xlir_nominal_set_' in nested_text
        for level in (0, 2):
            lane = f'nested.language.O{level}'
            native = out / (lane + suffix)
            run(lane + '.compile', [cc, *flags, f'-O{level}', '-DXL6_PRODUCT_USER_ENTRY',
                '-include', nested_c, language_host, out / f'O{level}.memory.o', *objects, *links, '-o', native])
            assert 'balanced' in run(lane + '.run', [native])
        run('nested.language.jit', [driver, nested_fixture, 'run', out / 'nested.language.jit.result'])
        report['Objects'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.glob('*.o'))]
        report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(out.iterdir())
                              if p.is_file() and p.suffix not in ('.o', '.log', '.json')]
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        print(error, file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Set target gate: {len(rows)} phases, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
