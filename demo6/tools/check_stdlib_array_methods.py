#!/usr/bin/env python3
"""Resident Array methods: actual nine-layout native/wire consumers and fault prefixes."""
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


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--shared', action='store_true')
    parser.add_argument('--defaults', action='store_true', help='Native resize: sealed defaults, 12 layouts, aliases/snapshots and atomic OOM')
    parser.add_argument('--extract', action='store_true', help='Checked native/wire extraction: nine projection layouts and atomic allocation failure')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve()
    cc, nm = shutil.which('gcc'), shutil.which('nm')
    if not cc or not nm or out.exists() or (args.sanitize and os.name == 'nt') or (args.defaults and args.extract):
        parser.error('Require gcc/nm, fresh evidence, Linux for sanitizers')
    profiles = [
        ('reference', ['src/runtime/xl6_value_admission.c'], 'xl6SequenceReferenceCreateV1'),
        ('nominal', ['src/runtime/xl6_nominal_admission.c'], 'xl6SequenceNominalCreateV1'),
        ('primitive', ['src/runtime/xl6_value_admission.c'], 'xl6SequencePrimitiveCreateV1'),
        ('c_scalar', [], 'xl6SequenceCScalarCreateV1'),
        ('sequence', ['src/runtime/xl6_array_primitive_admission.c', 'src/runtime/xl6_value_admission.c'], 'xl6SequenceNestedCreateV1'),
        ('nullable_c_scalar', ['src/runtime/xl6_nullable_native_u16.c', 'src/runtime/xl6_nullable_native_u16_value.c'], 'xl6SequenceNullableCScalarCreateV1'),
        ('numeric_enum', [], 'xl6SequenceNumericEnumCreateV1'),
        ('callable', [], 'xl6SequenceCallableCreateV1'),
        ('future', [], 'xl6SequenceFutureCreateV1'),
    ]
    common = ['src/runtime/xl6_sequence.c', 'src/runtime/xl6_array_methods.c',
              'src/runtime/xl6_array_method_insert.c', 'src/runtime/xl6_array_method_find.c',
              'src/runtime/xl6_array_erased.c', 'src/runtime/xl6_array_reference_value.c']
    host = 'test/cases/stdlib_array_methods_native_host.c'
    header = 'test/cases/stdlib_array_methods_header.c'
    if args.defaults:
        profiles += [
            ('managed_string', ['src/runtime/xl6_string_type.c', 'src/runtime/xl6_string_value.c', 'src/runtime/xl6_value_admission.c'], 'xl6SequencePrimitiveCreateV1'),
            ('bytes', ['src/runtime/xl6_value_admission.c'], 'xl6SequenceReferenceCreateV1'),
            ('nullable_string', ['src/runtime/xl6_nullable_string.c', 'src/runtime/xl6_value_admission.c'], 'xl6SequencePrimitiveCreateV1'),
        ]
        common = ['src/runtime/xl6_sequence.c', 'src/runtime/xl6_array_default.c',
                  'src/runtime/xl6_array_reference_value.c']
        host = 'test/cases/stdlib_array_resident_default_host.c'
        header = 'test/cases/stdlib_array_default_header.c'
    if args.extract:
        common = ['src/runtime/xl6_sequence.c', 'src/runtime/xl6_array_extract_value.c',
                  'src/runtime/xl6_array_erased.c', 'src/runtime/xl6_array_reference_value.c']
        host = 'test/cases/stdlib_array_extract_native_host.c'
        header = 'test/cases/stdlib_array_extract_header.c'
    def provider(profile):
        return 'src/runtime/xl6_array_' + {'managed_string': 'primitive', 'bytes': 'reference',
            'nullable_string': 'primitive'}.get(profile, profile) + '_admission.c'
    support = 'test/support/array_import_native.c'
    core_sources = ['src/runtime/xl6_array_extract_scalar.c', 'src/runtime/xl6_array_extract_string.c',
        'src/runtime/xl6_array_extract_f32.c', 'src/runtime/xl6_array_extract_char.c',
        'src/runtime/xl6_collection_extract_value.c', 'src/runtime/xl6_string_type.c',
        'src/runtime/xl6_string_value.c', 'src/runtime/xl6_nullable_string.c',
        'src/runtime/xl6_char_type.c', 'src/runtime/xl6_char_value.c'] if args.extract else []
    files = {Path(__file__), Path(cc), Path(nm), Path(sys.executable), root / host, root / support,
             root / 'test/cases/stdlib_sequence_factory_only_host.c',
             root / header, root / 'lib/xrt/xrt.h',
             root / 'lib/xruntime/xruntime.h'}
    files.update(root / name for name in common)
    files.update(root / name for name in core_sources)
    if args.extract: files.add(root / 'test/cases/stdlib_array_extract_core_host.c')
    for profile, extras, _ in profiles:
        files.update(root / name for name in [provider(profile), *extras])
    for directory in ('src/runtime', 'release/include'):
        files.update((root / directory).rglob('*.h'))
    product = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    if args.shared:
        files.add(product)
    inputs = [{'Path': str(path.resolve()), 'Sha256': digest(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    rows, sweeps = [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs,
              'Rows': rows, 'Sweeps': sweeps, 'Profiles': len(profiles),
              'Scope': 'Checked Array extraction: nine native/wire projection layouts, 12 basic/presence profiles, COW Array/List/Dict atomic OOM and native admission' if args.extract else
                  'Native Array resize/defaults: 12 layouts, resident policies, aliases/snapshots, failure atomicity' if args.defaults else __doc__, 'NativeDefaults': args.defaults,
              'SharedRuntime': args.shared, 'Sanitized': args.sanitize,
              'CheckedExtraction': args.extract,
              'CoreProfiles': 12 if args.extract else 0, 'CollectionCowProfiles': 2 if args.extract else 0,
              'RuntimeInstrumented': args.sanitize and not args.shared}
    env = dict(os.environ)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I.', '-Irelease', '-Irelease/include']
    links = [] if os.name == 'nt' else ['-pthread', '-lm']
    if os.name != 'nt':
        flags += ['-D_GNU_SOURCE']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if args.shared and os.name == 'nt':
        shutil.copy2(product, out / 'xl.dll')

    def run(name, command):
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=180)
        data = result.stdout + result.stderr
        (out / (name + '.log')).write_bytes(data)
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode,
                     'Complete': result.returncode == 0})
        if result.returncode:
            raise RuntimeError(name + ': ' + data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')

    try:
        run('header', [cc, *flags, '-c', header, '-o', str(out / 'header.o')])
        for level in (0, 2):
            objects, support_objects = {}, {}
            if not args.shared:
                sources = set(common + core_sources)
                for profile, extras, _ in profiles:
                    sources.update([provider(profile), *extras])
                for source in sorted(sources):
                    obj = out / f'O{level}.{Path(source).stem}.o'
                    run(f'O{level}.compile.{Path(source).stem}',
                        [cc, *flags, f'-O{level}', '-c', source, '-o', str(obj)])
                    objects[source] = str(obj)
                for variant in (0, 7, 8):
                    obj = out / f'O{level}.xruntime{variant}.o'
                    run(f'O{level}.native{variant}', [cc, *flags, f'-O{level}',
                        f'-DXL6_FACTORY_PROFILE={variant}', *(['-DXRT_MODULE_VALUE_COLLECTION'] if args.defaults or args.extract else []),
                        '-c', support, '-o', str(obj)])
                    support_objects[variant] = str(obj)
            if args.extract:
                lane=f'core.O{level}'
                binary=out / (lane + ('.exe' if os.name=='nt' else ''))
                selected=[] if args.shared else [objects[name] for name in
                    [*common,*core_sources,'src/runtime/xl6_array_primitive_admission.c',
                     'src/runtime/xl6_array_reference_admission.c','src/runtime/xl6_value_admission.c']]
                if not args.shared: selected.append(support_objects[0])
                linked=links if not args.shared else [str(product)] if os.name=='nt' else [
                    '-Lrelease','-lxl','-Wl,-rpath,'+str(root / 'release'),*links]
                run(lane+'.build',[cc,*flags,f'-O{level}','test/cases/stdlib_array_extract_core_host.c',
                    *selected,*linked,'-o',str(binary)])
                assert '12 basic/presence profiles' in run(lane+'.vectors',[str(binary)])
                for profile in range(14):
                    for carrier in (('wire',) if profile>=12 else ('wire','native')):
                        for operation in (('take',) if profile>=12 else ('pop','take')):
                            for point in range(513):
                                log=run(f'{lane}.{profile}.{carrier}.{operation}.{point}',
                                    [str(binary),str(profile),carrier,operation,str(point)])
                                hits=re.findall(r'hit=([01]) balanced',log); assert hits
                                if all(hit=='0' for hit in hits):
                                    sweeps.append({'CoreProfile':profile,'Lane':level,'Carrier':carrier,
                                        'Operation':operation,'FirstNoHit':point}); break
                            else: raise RuntimeError('Incomplete basic extraction allocation prefix')
            for index, (profile, extras, factory) in enumerate(profiles):
                lane = f'{profile}.O{level}'
                binary = out / (lane + ('.exe' if os.name == 'nt' else ''))
                selected = [] if args.shared else [objects[name] for name in
                    [*common, provider(profile), *extras]]
                if not args.shared:
                    selected.append(support_objects[index if index in (7, 8) else 0])
                linked = links if not args.shared else [str(product)] if os.name == 'nt' else [
                    '-Lrelease', '-lxl', '-Wl,-rpath,' + str(root / 'release'), *links]
                run(lane + '.build', [cc, *flags, f'-O{level}', f'-DXL6_FACTORY_PROFILE={index}',
                    host, *selected, *linked, '-o', str(binary)])
                if not args.shared:
                    names = {line.split()[-1].lstrip('_') for line in
                        run(lane + '.symbols', [nm, '--defined-only', str(binary)]).splitlines() if line.split()}
                    methods = ({'xl6ArrayExtractValueV1'} if args.extract else
                               {'xl6ArrayResizeDefaultV1', 'xl6SequenceResizeStorageV1',
                                'xrtTypedArrayResizeWithInitializer'} if args.defaults else
                               {'xl6ArrayMethod' + suffix + 'V1' for suffix in
                                ('Clear', 'Capacity', 'Reserve', 'Trim', 'Swap', 'Reverse', 'Insert', 'IndexOf', 'RemoveValue')})
                    assert {factory, *methods} <= names
                    allowed = {factory, 'xl6SequencePrimitiveCreateV1'} if index == 4 else {factory}
                    assert not ({row[2] for row in profiles} - allowed) & names
                    assert not {'xl6ArrayImportTakeV1', 'xl6ArrayInputAcquireV1',
                                'xl6ArrayFromValueV1', 'xl6ArrayToValueV1', 'xl6StackCreateV1'} & names
                    assert not any(re.match(r'xl6(?:Log|Process|Regex)[A-Z]|xrtTypedStack', name) for name in names)
                    if args.defaults:
                        assert not any(name.startswith('xl6ArrayMethod') for name in names)
                    if args.extract:
                        assert not any(name.startswith(('xl6ArrayMethod', 'xl6ArrayExtractString',
                            'xl6ArrayExtractBool', 'xl6ArrayExtractF32', 'xl6ArrayExtractChar',
                            'xl6CollectionExtract')) for name in names)
                if args.defaults:
                    assert 'native defaults: 100 alias/snapshot/policy cycles balanced' in run(lane + '.vectors', [str(binary)])
                    nullable = index in (1, 4, 7, 8)
                    for spare in (0, 1):
                        for point in range(513):
                            log = run(f'{lane}.nullable{int(nullable)}.spare{spare}.{point}',
                                [str(binary), str(int(nullable)), str(spare), str(point)])
                            hits = re.findall(r'hit=([01]) balanced', log); assert hits
                            if all(hit == '0' for hit in hits):
                                sweeps.append({'Profile': index, 'Lane': level, 'Nullable': nullable,
                                    'Spare': spare, 'FirstNoHit': point})
                                break
                        else: raise RuntimeError('Incomplete native default allocation prefix')
                    continue
                label = 'extraction' if args.extract else 'methods'
                assert f'Array {label} profile={index}: 100 native/wire cycles balanced' in run(
                    lane + '.vectors', [str(binary)])
                for carrier in ('wire', 'native'):
                    for operation in (('pop', 'take') if args.extract else
                        ('reserve', 'insert', 'find', 'remove', 'swap', 'reverse', 'trim')):
                        for point in range(513):
                            log = run(f'{lane}.{carrier}.{operation}.{point}',
                                      [str(binary), carrier, operation, str(point)])
                            hits = re.findall(r'hit=([01]) balanced', log)
                            assert hits
                            if all(hit == '0' for hit in hits):
                                sweeps.append({'Profile': index, 'Lane': level, 'Carrier': carrier,
                                               'Operation': operation, 'FirstNoHit': point})
                                break
                        else:
                            raise RuntimeError('Incomplete allocation prefix')
        report['InputsUnchanged'] = all(digest(Path(row['Path'])) == row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        print(error, file=sys.stderr)
    finally:
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Array methods: {len(rows)} checks, {len(sweeps)} complete prefixes, complete={report["Complete"]}')
    return 0 if report['Complete'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
