#!/usr/bin/env python3
"""Real language resident class V3: canonical IR/checked CFG, C/JIT/AOT and init/fini."""
import argparse
import hashlib
import json
import os
from pathlib import Path
from compiler_response import command_with_response
import shutil
import subprocess
import sys
from xlir_public_build_inputs import verify_public_build


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--public', '--public-directory', dest='public', type=Path, required=True)
    parser.add_argument('--root', type=Path)
    args = parser.parse_args()
    root, out = (args.root.resolve() if args.root else Path(__file__).resolve().parents[1]), args.out.resolve()
    cc = shutil.which('gcc')
    if not cc or out.exists(): parser.error('Require gcc and a fresh evidence root')
    cases = ['stdlib_class_identity_inherited', 'cgen_derive_string_hash_equals',
             'stdlib_native_sequence_identity', 'stdlib_class_identity_checked']
    public=args.public.resolve(); build,pinned,compared=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cli=Path(build['Driver'])
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    files = {Path(__file__), Path(cc), Path(sys.executable), runtime}
    for folder in ['src', 'lib/xrt', 'lib/xruntime', 'release/include']:
        files.update(p for p in (root / folder).rglob('*') if p.suffix in ['.h', '.c', '.inc'])
    files.update(root / ('test/cases/' + case + '.xl') for case in cases)
    files.update(root / p for p in ['test/cases/xlir_class_identity_plan_contract.c',
        'test/cases/xlir_module_product_oom_host.c', 'test/support/module_product_memory.c',
        'test/support/module_product_memory.h'])
    files.update(pinned)
    files.add(Path(__file__).with_name('compiler_response.py'))
    inputs = [dict(Path=str(p.resolve()), Sha256=digest(p)) for p in sorted(files)]
    out.mkdir(parents=True)
    rows = []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows,
        Scope=__doc__, CheckedNativeIdentity=True, Platform=sys.platform,SourceInputsMatch=True,
        ProductionFiles=len(compared),WaitQueueComplete=False,Full53Acceptance=False,ShippingArtifact=False)
    env = dict(os.environ)
    if os.name!='nt':
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    include = out / 'include/__xl_runtime'; include.mkdir(parents=True)
    for folder in ['src/runtime', 'release/include']:
        for header in (root / folder).glob('*.h'): shutil.copy2(header, include / header.name)
    if os.name == 'nt':
        shutil.copy2(runtime, out / 'xl.dll')
        links = [str(runtime)]
        contract_links = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        links = [str(runtime), '-Wl,-rpath,' + str(runtime.parent), '-pthread', '-lm', '-ldl']
        contract_links = []
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
        '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-unused-label',
        '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers',
        '-I.', '-Irelease', '-Irelease/include', '-I' + str(out / 'include'), '-I'+str(public/'include')]
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']

    def run(name, command, timeout=600):
        result = subprocess.run(command_with_response(command,cc,out/(name+'.rsp')), cwd=root, env=env,
            capture_output=True, timeout=timeout)
        data = result.stdout + result.stderr; (out / (name + '.log')).write_bytes(data)
        rows.append(dict(Name=name, Command=[str(x) for x in command], Exit=result.returncode))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode == 0, name + ': ' + data[-6000:].decode(errors='replace')
        return data

    def emit(source, mode, target):
        return [cli, source, mode, target]

    try:
        contract = out / ('contract.exe' if os.name == 'nt' else 'contract')
        run('contract.build', [cc, *flags, '-Wno-pointer-sign', '-Wno-format-truncation', '-Ilib/tcc',
            'test/cases/xlir_class_identity_plan_contract.c', *objects, *links, *contract_links, '-o', contract])
        # Preinclude belongs only to the generated host TU, never the separate
        # allocator probe (which would define the module a second time).
        for level in [0, 2]:
            run(f'memory.O{level}.build', [cc, *flags, f'-O{level}', '-c',
                'test/support/module_product_memory.c', '-o', out / f'memory.O{level}.o'])
        for case in cases:
            source = 'test/cases/' + case + '.xl'
            paths = [out / (case + '.contract' + suffix) for suffix in ['.c', '.h', '.json']]
            assert b'rejected C/header mutations' in run(case + '.contract', [contract, source, *paths])
            repeated = [p.with_name(p.stem + '.repeat' + p.suffix) for p in paths]
            run(case + '.contract.repeat', [contract, source, *repeated])
            assert all(p.read_bytes() == q.read_bytes() for p, q in zip(paths, repeated))
            for mode, suffix in [('ir-typed', '.typed.json'), ('ir-lowered', '.json'), ('cgen-default', '.c')]:
                target = out / (case + suffix)
                run(case + '.' + mode, emit(source, mode, target))
                repeat = target.with_name(target.stem + '.repeat' + target.suffix)
                run(case + '.' + mode + '.repeat', emit(source, mode, repeat))
                assert target.read_bytes() == repeat.read_bytes()
            ir = json.loads((out / (case + '.json')).read_text())
            typed=json.loads((out/(case+'.typed.json')).read_text())
            assert ir['classIdentityAbi'] == 1
            assert json.loads((out / (case + '.typed.json')).read_text())['classIdentityAbi'] == 0
            assert len(ir['classIdentityPlans']) == len(ir['classLayouts'])
            if case=='cgen_derive_string_hash_equals':
                by_name={t['name']:t['id'] for t in typed['types']}
                record=by_name['DeriveRecord']; reference=by_name['ReferenceRecord']
                derived=[row for row in typed['classLayouts'] if row['type']==record]
                plain=[row for row in typed['classLayouts'] if row['type']==reference]
                assert len(derived)==len(plain)==1
                assert derived[0]['hashAbiName']==derived[0]['equalityAbiName']=='runtime-derived'
                assert plain[0]['hashAbiName']==plain[0]['equalityAbiName']=='runtime-reference'
                assert not any(row[name] for row in derived+plain for name in ('hashTarget','equalityTarget'))
                fields=typed['classFields'][derived[0]['firstField']:derived[0]['firstField']+derived[0]['fieldCount']]
                assert [row['compareDeriveIncluded'] for row in fields]==[1,1,0]
                binds=[row for row in ir['instructions'] if row['runtimeOpName']=='xvalue-class-identity-bind']
                equal=[row for row in ir['instructions'] if row['runtimeOpName']=='class-derived-equal']
                assert sum(row['payloadType']==record for row in binds)==3
                assert sum(row['payloadType']==reference for row in binds)==2
                assert all(row['payloadType']==row['type'] for row in binds)
                assert len(equal)==3 and all(row['payloadType']==record for row in equal)
            if case == 'stdlib_class_identity_inherited':
                by_name = {t['name']: t['id'] for t in ir['types']}
                expected = {'IdentityChild': ['baseNumber', 'baseText', 'childNumber'],
                    'UnconstructedIdentityChild': ['inherited', 'own']}
                for name, names in expected.items():
                    layout_index = next(i for i, v in enumerate(ir['classLayouts']) if v['type'] == by_name[name])
                    plan = ir['classIdentityPlans'][layout_index]; assert plan['residentKeyedPair'] == 1
                    fields = ir['classIdentityFields'][plan['firstField']:plan['firstField'] + plan['fieldCount']]
                    # Exact names/order are asserted against the real arena by
                    # the independent C contract (dump intentionally omits names).
                    assert len(fields) == len(names)
            target = out / (case + '.c')
            generated = target.read_text(encoding='utf-8')
            if case=='cgen_derive_string_hash_equals':
                for required in ('XL6_CLASS_IDENTITY_DERIVED_V3','XL6_CLASS_IDENTITY_REFERENCE_V3',
                    '"id", 2','"name", 4'):
                    assert required in generated,required
                marker=f'static const XL6_ClassIdentityFieldV3 xlir_class_identity_fields_t{record}[]'
                start=generated.index(marker); end=generated.index('};',start)
                assert '"transient"' not in generated[start:end]
            for required in ('XL6_ClassIdentitySchemaV3', 'xl6ClassRuntimeV3Get()->Create',
                             'xl6ClassRuntimeV3Get()->BindIdentityTake', 'xl6ClassRuntimeV3Get()->Equal'):
                assert required in generated, required
            for forbidden in ('xlir_class_derive_hash_t', 'xlir_class_derive_equal_t',
                              'xlir_class_reference_hash_t', 'xlir_class_reference_equal_t', 'xlir_class_identity_bind_t',
                              'xl6ClassRuntimeV2Get()->BindIdentity'):
                assert forbidden not in generated, forbidden
            for level in [0, 2]:
                binary = out / (case + f'.O{level}' + ('.exe' if os.name == 'nt' else ''))
                run(case + f'.O{level}.build', [cc, *flags, f'-O{level}', '-include', target,
                    'test/cases/xlir_module_product_oom_host.c', out / f'memory.O{level}.o',
                    *objects, *links, *(contract_links if args.public else []), '-o', binary])
                assert b'balanced' in run(case + f'.O{level}.vectors', [binary])
            binary = out / (case + '.product' + ('.exe' if os.name == 'nt' else ''))
            run(case + '.JIT', [cli, source, 'run', out / (case + '.jit')])
            run(case + '.module', [cli, source, 'module', out / (case + '.module')])
            run(case + '.AOT.build', [cli, source, 'exe', binary.with_suffix('') if os.name=='nt' else binary])
            run(case + '.AOT.run', [binary])
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    except Exception as failure:
        report['Failure'] = repr(failure); print(report['Failure'], file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Identity plan: {len(rows)} checks, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__': raise SystemExit(main())
