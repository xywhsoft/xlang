"""Staged ordinary acceptance for the five-group task; no network security tests.

P0 is entirely offline: actual CLI JIT/AOT, deterministic C, strict external C,
rejected implicit narrowing and (Linux) source-derived compiler contracts.
Future stages must explicitly add their own permitted local functional cases;
this script never discovers/runs an entire network/security test directory.
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

CHANNEL_STAGES=('channel-waits','channel-select-native','channel-cancel')

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', choices=('p0', 'atomic', 'once-event', 'thread-local', 'cancellation', 'promise', *CHANNEL_STAGES), required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--native-factory-inheritance',action='store_true',
        help='Add offline inherited native resource contracts and complete language allocation prefixes')
    parser.add_argument('--channel-case-language',action='store_true',
        help='Add public Channel case/select, typed private-result contracts and allocation prefixes')
    parser.add_argument('--native-plan-report',type=Path,
        help='Windows native-only pruning: sealed Linux actual-plan receipt with identical source graph')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    task = Path(__file__).resolve().parents[1]
    cases = task/'test/cases'
    win = os.name == 'nt'
    names = ('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')
    products = [root/'release'/name for name in names]
    assert not out.exists() and all(path.is_file() for path in products)
    selected = ('xlir_narrow_operator_literals', 'type_int_narrowing_wrap',
                'xlir_scalar_conversion_abi', 'operators_int_literal_forms',
                'cgen_integer_division_errors')
    negatives = ('bad_xlir_narrow_operator_literal_range', 'bad_xlir_narrow_operator_variable',
                 'sem_numeric_narrowing_int')
    helpers = ('stdlib_product_fixture_host.c', 'stdlib_pruning_product_host.c',
               'stdlib_codec_template_language_host.c', 'xlir_narrow_operator_contract.c')
    if args.stage == 'atomic':
        selected = ('stdlib_atomics', 'stdlib_atomic_bool_only', 'stdlib_atomic_threads',
                    'stdlib_concurrent_import_only', 'stdlib_time_text_only', 'xlir_narrow_operator_literals')
        negatives = ('bad_stdlib_atomic_missing_import', 'bad_stdlib_atomic_private_handle', 'bad_xlir_atomic_product')
        helpers = (*helpers[:3], 'xlir_atomic_contract.c', 'stdlib_atomic_oom_host.c', 'stdlib53_pruning_plan.c')
    if args.stage == 'once-event':
        selected = ('stdlib_once', 'stdlib_event', 'stdlib_once_event_threads',
                    'stdlib_regex_replace_func_errors', 'stdlib_atomic_bool_only', 'stdlib_concurrent_import_only')
        negatives = ('bad_stdlib_once_missing_import', 'bad_stdlib_once_callback',
                     'bad_stdlib_event_private_handle', 'bad_xlir_once_callback')
        helpers = (*helpers[:3], 'xlir_once_event_contract.c', 'stdlib_once_event_oom_host.c',
                   'stdlib53_pruning_plan.c', 'stdlib_sync_native_host.c')
    if args.stage == 'thread-local':
        selected = ('stdlib_thread_local', 'stdlib_thread_local_threads', 'stdlib_thread_local_owned',
                    'xlir_generic_dynamic_cast', 'xlir_nullable_bytes_cast', 'stdlib_once',
                    'stdlib_event', 'stdlib_atomic_bool_only', 'stdlib_concurrent_import_only',
                    'stdlib_thread_only', 'stdlib_nullable_numeric_enum', 'stdlib_nullable_projection_f32')
        negatives = ('bad_stdlib_thread_local_type', 'bad_stdlib_thread_local_private',
                     'bad_stdlib_thread_local_missing_import')
        helpers = (*helpers[:3], 'xlir_thread_local_contract.c', 'stdlib_thread_local_native_host.c',
                   'stdlib53_pruning_plan.c', 'stdlib_sync_native_host.c')
    if args.stage == 'cancellation':
        selected = ('stdlib_cancellation', 'stdlib_cancellation_errors', 'stdlib_cancellation_threads',
                    'stdlib_cancellation_owned', 'stdlib_cancellation_source_only',
                    'xlir_class_tag_vocabulary', 'xlir_class_tag_narrow_first',
                    'stdlib_task_group_methods', 'stdlib_task_group_sync_only', 'stdlib_once',
                    'stdlib_event', 'stdlib_thread_local', 'stdlib_concurrent_import_only',
                    'xlir_narrow_operator_literals', 'stdlib_nullable_numeric_enum')
        negatives = ('bad_stdlib_cancellation_missing_import', 'bad_stdlib_cancellation_private',
                     'bad_stdlib_cancellation_authority')
        helpers = (*helpers[:3], 'xlir_cancellation_contract.c', 'stdlib_cancellation_native_host.c',
                   'stdlib53_pruning_plan.c', 'stdlib_native_future_policy_product_host.c')
    if args.stage == 'promise':
        selected = ('stdlib_promise', 'stdlib_promise_values', 'stdlib_promise_terminal',
                    'stdlib_promise_dynamic', 'stdlib_promise_owned', 'future_duplicate_cancel',
                    'stdlib_cancellation', 'stdlib_task_group_methods', 'stdlib_concurrent_import_only',
                    'stdlib_slot_map_future_keys')
        negatives = ('bad_stdlib_promise_missing_import', 'bad_stdlib_promise_private', 'bad_stdlib_promise_type',
                     'bad_stdlib_promise_default','bad_stdlib_promise_default_local','bad_stdlib_promise_default_field',
                     'bad_stdlib_promise_default_generic_field')
        helpers = (*helpers[:3], 'xlir_promise_contract.c', 'stdlib_promise_native_host.c',
                   'stdlib53_pruning_plan.c', 'stdlib_native_future_policy_product_host.c')
    if args.stage in CHANNEL_STAGES:
        selected = ('stdlib_channel_timeout', 'stdlib_channel_owned', 'stdlib_channel_owned_wait',
                    'stdlib_channel_recipes', 'stdlib_promise', 'stdlib_cancellation',
                    'stdlib_concurrent_import_only', 'stdlib_channel_pruned', 'stdlib_channel_deadlines',
                    'stdlib_channel_deadline_only')
        negatives = ('bad_channel_send_timeout_type', 'bad_channel_send_timeout_named',
                     'bad_channel_send_timeout_value', 'bad_channel_send_timeout_arity',
                     'bad_channel_deadline_type', 'bad_channel_deadline_receive_type',
                     'bad_channel_deadline_named', 'bad_channel_deadline_arity')
        helpers = (*helpers[:3], 'xlir_channel_send_contract.c', 'stdlib_channel_native.c',
                   'stdlib53_pruning_plan.c')
        if args.stage in ('channel-select-native','channel-cancel'):
            helpers=(*helpers[:4],'stdlib_channel_select_native.c',*helpers[5:])
        if args.stage=='channel-select-native':
            helpers=(*helpers,'stdlib_channel_select_native_plan.c','stdlib_channel_select_pruned.c')
        helpers=(*helpers,'xlir_channel_deadline_contract.c')
        if args.stage=='channel-cancel':
            selected=(*selected,'stdlib_channel_cancellation','stdlib_channel_cancel_only','stdlib_channel_cancel_flow',
                      'stdlib_thread_only','stdlib_concurrent_queue_owned','stdlib_stack_try_owned','stdlib_tree_query_selection',
                      'stdlib_channel_materialization')
            negatives=(*negatives,'bad_channel_cancel_source','bad_channel_cancel_null',
                       'bad_channel_cancel_arity','bad_channel_cancel_named','bad_channel_cancel_authority')
            helpers=(*helpers,'xlir_channel_cancel_contract.c')
    if args.native_factory_inheritance:
        assert args.stage=='channel-cancel'
        selected=(*selected,'xlir_native_factory_inherited','xlir_native_static_factory','xlir_native_factory_ordinary_inheritance')
        negatives=(*negatives,'bad_xlir_native_inherited_default','bad_xlir_native_ambiguous_default',
                   'bad_xlir_native_factory_ambiguous','bad_xlir_native_factory_shadow','bad_xlir_native_generic_inheritance')
        helpers=(*helpers,'xlir_native_factory_inherited_contract.c','xlir_native_factory_inherited_host.c')
    if args.channel_case_language:
        assert args.stage=='channel-cancel'
        selected=(*selected,'stdlib_channel_cases','stdlib_channel_cases_threads','stdlib_channel_cases_owned',
            'xlir_generic_static_calls','stdlib_channel_cases_receive_only')
        negatives=(*negatives,'bad_channel_cases_missing_import','bad_channel_cases_private',
            'bad_channel_cases_send_type','bad_channel_cases_array','bad_channel_cases_null_token',
            'bad_generic_static_unbound','bad_generic_channel_unsupported')
        helpers=(*helpers,'xlir_channel_cases_contract.c','stdlib_channel_cases_oom_host.c')
    inputs = {Path(__file__).resolve(), *products,
              *[cases/(name+'.xl') for name in selected+negatives],
              *[cases/name for name in helpers],
              task/'test/support/module_product_memory.c', task/'test/support/module_product_memory.h'}
    donor=None
    if args.native_plan_report:
        assert win and args.stage=='channel-select-native'
        donor=json.loads(args.native_plan_report.read_text(encoding='utf8'))
        assert donor['Complete'] and donor['InputsUnchanged'] and donor['LogsUnchanged'] and not donor['Failures']
        assert donor['Stage']=='channel-select-native' and donor['NativeChannelPlanOrigin']=='actual-linux-compiler'
        rows=donor['NativeChannelPlanInputs']
        relative={row['RelativePath']:row['NormalizedSha256'] for row in rows}
        sources={path.relative_to(root).as_posix():path for path in (root/'src').rglob('*') if path.is_file()}
        assert sources.keys()==relative.keys()
        for path,source in sources.items():
            assert hashlib.sha256(source.read_bytes().replace(b'\r\n',b'\n')).hexdigest()==relative[path],path
        inputs.add(args.native_plan_report.resolve())
        for symbol,plan in donor['NativeChannelPlans'].items():
            row=next(row for row in donor['Rows'] if row['Name']==symbol+'.native-plan')
            log=args.native_plan_report.parent/(symbol+'.native-plan.log')
            assert digest(log)==row['Sha256'] and json.loads(log.read_bytes())==plan
            inputs.add(log.resolve())
    for folder in ('src', 'lib', 'tools'):
        inputs.update(path for path in (root/folder).rglob('*') if path.is_file())
    cc = shutil.which('gcc')
    assert cc
    inputs.add(Path(cc).resolve())
    out.mkdir(parents=True)
    isolated, relocated, sdk = out/'isolated', out/'relocated', out/'sdk'
    release = isolated/'release'
    for path in (release, relocated, sdk):
        path.mkdir(parents=True)
    for product in products:
        shutil.copy2(product, release/product.name)
        if win and product.suffix == '.dll':
            shutil.copy2(product, isolated/product.name)
    report = dict(Stage=args.stage, Complete=False, InputsUnchanged=False, LogsUnchanged=False,
                  NativeFactoryInheritance=args.native_factory_inheritance,
                  PublicChannelCaseLanguage=args.channel_case_language,
                  NetworkTests=False, NetworkSecurityTests=False, FullFiveGroupAcceptance=False,
                  Inputs=[dict(Path=str(path), Sha256=digest(path)) for path in sorted(inputs)], Rows=[])
    env = {key.upper(): value for key, value in os.environ.items()
           if not key.upper().startswith('XL6_') and key.upper() not in
           ('LD_LIBRARY_PATH', 'LD_PRELOAD', 'LD_AUDIT', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH')}

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, cwd=isolated, negative=False):
        command = list(map(str, command))
        result = subprocess.run(command, cwd=cwd, env=env, capture_output=True, timeout=180)
        log = out/(name+'.log')
        log.write_bytes(result.stdout+result.stderr)
        report['Rows'].append(dict(Name=name, Command=command, Exit=result.returncode,
                                   Log=str(log), Sha256=digest(log)))
        save()
        assert (result.returncode != 0 if negative else result.returncode == 0), (name, log.read_bytes()[-5000:])
        return log.read_bytes()

    def links(product):
        return [release/product] if win else ['-L'+str(release), '-l:'+product,
               '-Wl,-rpath,$ORIGIN/release', '-pthread', '-ldl', '-lm']

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED']
    try:
        sys.path.insert(0, str(root/'tools'))
        from gen_tcc_vfs_resources import load_resources
        build = root/('build/tmp' if win else 'build/linux')
        manifest = root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt')
        dist = root/'res/tcc/win64/x86_64' if win else build/'tcc-dist'
        for resource in load_resources(manifest, root, dist, build/'xrt.o', build):
            if resource.virtual.startswith('include/__xl_runtime/'):
                target = sdk/Path(resource.virtual).relative_to('include')
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(resource.source, target)
        for header in (root/'release/include').glob('*.h'):
            shutil.copy2(header, sdk/header.name)
        shutil.copy2(root/'lib/xruntime/xruntime.h', sdk/'xruntime.h')
        host = isolated/('host.exe' if win else 'host')
        observer = isolated/('observer.exe' if win else 'observer')
        for binary, source in ((host, helpers[0]), (observer, helpers[1])):
            run(binary.stem+'.build', [cc, *flags, '-O2', '-I'+str(root/'release/include'),
                                      cases/source, *links(names[2]), '-o', binary])
        if args.stage in ('atomic', 'once-event', 'thread-local', 'cancellation', 'promise', *CHANNEL_STAGES):
            family, prefix, entries = ('atomic', rb'xl6Atomic\w+V1', 50) if args.stage == 'atomic' else (
                'concurrent', rb'xl6(?:Once|Event|ThreadLocal|Cancellation|Promise)\w+V1', None)
            exports = set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',
                (root/f'src/runtime/xl6_{family}_symbols.inc').read_text()))
            declarations = set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',
                (root/f'src/runtime/xl6_{family}_abi.h').read_text()))
            contracts = set(re.findall(r'(?m)^\s*\{"([^"]+)"',
                (root/f'src/ir/xl6_ir_{family}_contracts.inc').read_text()))
            if family == 'concurrent':
                contracts.update(re.findall(r'(?m)^XL6_NATIVE_GENERIC_BRIDGE\("([^"]+)"\s*,\s*"xl6PromiseDestroyV1"\s*,',
                    (root/'src/abi/xl6_native_generic_contracts.inc').read_text()))
            assert exports == declarations == contracts
            if entries is not None: assert len(exports) == entries
            if args.stage == 'once-event': assert len([name for name in exports if name.startswith(('xl6Once','xl6Event'))]) == 10
            if args.stage == 'thread-local': assert len([name for name in exports if name.startswith('xl6ThreadLocal')]) == 9
            if args.stage == 'cancellation':
                stage_exports = {name for name in exports if name.startswith('xl6Cancellation')}
                assert len(stage_exports) == 16
                report['StageEntries'] = len(stage_exports)
            if args.stage == 'promise':
                stage_exports = {name for name in exports if name.startswith('xl6Promise')}
                assert len(stage_exports) == 10
                report['StageEntries'] = len(stage_exports)
            report['ResidentEntries'] = len(exports)
            inspect = shutil.which('objdump' if win else 'nm')
            assert inspect
            for product in products[1:]:
                body = run(product.stem+'.exports', [inspect, '-p', product] if win else
                    [inspect, '-D', '--defined-only', product])
                actual = {name.decode() for name in re.findall(rb'\b'+prefix+rb'\b', body)}
                assert exports <= actual
                if args.stage == 'thread-local':
                    for projection in (b'xl6ValueTryBytesV1', b'xl6ValueTryReadIntegerV1'):
                        assert re.search(rb'\b'+projection+rb'\b', body), projection
                    report['ProjectionEntries'] = 2
                if args.stage == 'cancellation':
                    for checked in (b'xlCancellationRetirementSnapshotCheckedV1', b'xlCancellationRetirementRetryCheckedV1'):
                        assert re.search(rb'\b'+checked+rb'\b', body), checked
                    report['RetirementEntries'] = 2
                binary = isolated/(product.stem+'.oom'+('.exe' if win else ''))
                run(product.stem+'.oom.build', [cc, *flags, '-O2', '-I'+str(root),
                    '-I'+str(root/'release'), '-I'+str(root/'release/include'),
                    cases/helpers[4], *([task/'test/support/module_product_memory.c'] if args.stage in CHANNEL_STAGES else []),
                    *links(product.name), '-o', binary])
                run(product.stem+'.oom.run', [binary])
                if args.stage in CHANNEL_STAGES:
                    run(product.stem+'.channel.lifetimes', [binary,'repeat'])
                    prefixes=[]
                    for operation in (('small','large','cancel') if args.stage in ('channel-select-native','channel-cancel') else ('create','send','send-timeout')):
                        for point in range(4097):
                            output=run(product.stem+'.channel.'+operation+'.'+str(point),[binary,operation,point])
                            if b'hit=0 complete-prefix balanced' in output:
                                prefixes.append(dict(Operation=operation,FirstNoHit=point)); break
                        else: raise AssertionError('Channel allocation prefix did not close')
                    report.setdefault('ChannelPrefixes',{})[product.name]=prefixes
                if args.stage in ('cancellation','promise'):
                    binary = isolated/(product.stem+'.future-regression'+('.exe' if win else ''))
                    run(product.stem+'.future.build', [cc, *flags, '-O2', '-I'+str(root),
                        '-I'+str(root/'release/include'), '-I'+str(sdk), cases/helpers[6],
                        *links(product.name), '-o', binary])
                    run(product.stem+'.future.run', [binary])
                if args.stage in ('once-event', 'thread-local'):
                    binary = isolated/(product.stem+'.sync-regression'+('.exe' if win else ''))
                    run(product.stem+'.sync.build', [cc, *flags, '-O2', '-I'+str(root),
                        '-I'+str(root/'release'), '-I'+str(root/'release/include'),
                        cases/helpers[6], task/'test/support/module_product_memory.c',
                        *links(product.name), '-o', binary])
                    run(product.stem+'.sync.run', [binary])
        strict = [*flags, '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
                  '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                  '-Wno-missing-field-initializers', '-I'+str(task), '-I'+str(root),
                  '-I'+str(root/'release'), '-I'+str(sdk)]
        cli = release/names[0]
        def positive(name):
            source = isolated/(name+'.xl')
            shutil.copy2(cases/source.name, source)
            run(name+'.jit', [cli, 'run', source])
            first, second = out/(name+'.c'), out/(name+'.repeat.c')
            run(name+'.cgen', [cli, 'cgen', source, '--emit', first])
            run(name+'.repeat', [cli, 'cgen', source, '--emit', second])
            assert first.read_bytes() == second.read_bytes()
            full = name == selected[0] or (args.stage in ('atomic', 'once-event', 'thread-local', 'cancellation') and
                name in selected[:(4 if args.stage in ('thread-local','cancellation') else 3)]) or (
                args.stage == 'cancellation' and name == 'stdlib_task_group_methods') or (
                args.stage == 'promise' and name in selected[:6]) or (
                args.stage in CHANNEL_STAGES and
                (name in selected[:3] or name in ('stdlib_channel_deadlines','stdlib_channel_deadline_only',
                                                 'stdlib_channel_cancellation','stdlib_channel_cancel_only','stdlib_channel_cancel_flow',
                                                 'stdlib_channel_materialization'))) or (
                args.native_factory_inheritance and name in ('xlir_native_factory_inherited',
                    'xlir_native_static_factory','xlir_native_factory_ordinary_inheritance')) or (
                args.channel_case_language and name in ('stdlib_channel_cases','stdlib_channel_cases_threads',
                    'stdlib_channel_cases_owned','xlir_generic_static_calls','stdlib_channel_cases_receive_only'))
            if not full:
                return
            work = out/(name+'.saved')
            work.mkdir()
            image = work/('program.exe' if win else 'program')
            run(name+'.aot', [observer, release/names[2], source, work, image])
            target = relocated/image.name
            shutil.copy2(image, target)
            run(name+'.relocated', [target], cwd=relocated)
            inspect = shutil.which('objdump' if win else 'readelf')
            assert inspect
            imports = run(name+'.imports', [inspect, '-p' if win else '-d', target])
            dependencies = re.findall(rb'DLL Name:\s*([^\s]+)', imports) if win else re.findall(
                rb'Shared library:\s*\[([^\]]+)\]', imports)
            assert dependencies and not {name.lower() for name in dependencies} & {
                value.lower().encode() for value in names[1:]}
            run(name+'.module', [host, release/names[2], source, 'module', out/(name+'.module')])
            for opt in ('O0', 'O2'):
                binary = isolated/(name+'.'+opt+('.exe' if win else ''))
                module = work/'module.c'
                run(name+'.'+opt+'.build', [cc, *strict, '-'+opt,
                    '-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"', cases/helpers[2],
                    task/'test/support/module_product_memory.c', *links(names[2]), '-o', binary])
                run(name+'.'+opt+'.run', [binary])
        def negative(name):
            source = isolated/(name+'.xl')
            shutil.copy2(cases/source.name, source)
            target = out/(name+'.c')
            target.write_bytes(b'UNCHANGED\n')
            output = run(name+'.reject', [cli, 'cgen', source, '--emit', target], negative=True)
            assert b'[ir]' in output or b'[sem]' in output or b'[checked]' in output
            assert target.read_bytes() == b'UNCHANGED\n'
            if name in ('bad_stdlib_atomic_private_handle', 'bad_stdlib_event_private_handle',
                        'bad_stdlib_thread_local_private', 'bad_stdlib_cancellation_private',
                        'bad_stdlib_promise_private','bad_channel_cases_private'): assert b'private' in output
            if name in ('bad_xlir_atomic_product', 'bad_xlir_once_callback'): assert b'exact declared ABI' in output
        failures = []
        report['Failures'] = failures
        for names_, check in ((selected, positive), (negatives, negative)):
            for name in names_:
                try:
                    check(name)
                except Exception as error:
                    failures.append(dict(Name=name, Error=repr(error)))
                    save()
        if args.native_factory_inheritance and not failures:
            module=out/'xlir_native_factory_inherited.saved/module.c'
            report['NativeFactoryPrefixes']={}
            for product in products[1:]:
                for opt in ('O0','O2'):
                    lane=product.stem+'.native-factory.'+opt
                    binary=isolated/(lane+('.exe' if win else ''))
                    run(lane+'.build',[cc,*strict,'-'+opt,
                        '-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"',cases/'xlir_native_factory_inherited_host.c',
                        task/'test/support/module_product_memory.c',*links(product.name),'-o',binary])
                    recovered=[]
                    for point in range(4097):
                        body=run(lane+'.'+str(point),[binary,point])
                        if b'recovered=1' in body:recovered.append(point)
                        if b'hit=0 complete-prefix balanced' in body:break
                    else:raise AssertionError('Inherited native language allocation prefix did not close')
                    report['NativeFactoryPrefixes'][lane]=dict(FirstNoHit=point,RecoveredRetirement=recovered)
                    save()
        if args.channel_case_language and not failures:
            module=out/'stdlib_channel_cases_owned.saved/module.c'
            report['ChannelCaseLanguagePrefixes']={}
            for product in products[1:]:
                for opt in ('O0','O2'):
                    lane=product.stem+'.channel-cases.'+opt
                    binary=isolated/(lane+('.exe' if win else ''))
                    run(lane+'.build',[cc,*strict,'-'+opt,
                        '-DXLIR_GENERATED_MODULE="'+module.as_posix()+'"',cases/'stdlib_channel_cases_oom_host.c',
                        task/'test/support/module_product_memory.c',*links(product.name),'-o',binary])
                    recovered=[]
                    for point in range(4097):
                        body=run(lane+'.'+str(point),[binary,point])
                        if b'recovered=1' in body:recovered.append(point)
                        if b'hit=0 complete-prefix balanced' in body:break
                    else:raise AssertionError('Public Channel case allocation prefix did not close')
                    report['ChannelCaseLanguagePrefixes'][lane]=dict(FirstNoHit=point,RecoveredRetirement=recovered)
                    save()
        if not win and not failures:
            if args.channel_case_language:
                contract=isolated/'channel-cases-contract'
                run('channel-cases-contract.build',[cc,*flags,'-O2','-I'+str(root),
                    '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),cases/'xlir_channel_cases_contract.c',
                    *links(names[2]),'-o',contract])
                run('channel-cases-contract.run',[contract,isolated/'stdlib_channel_cases.xl'])
            if args.native_factory_inheritance:
                contract=isolated/'native-factory-contract'
                run('native-factory-contract.build',[cc,*flags,'-O2','-I'+str(root),
                    '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),cases/'xlir_native_factory_inherited_contract.c',
                    *links(names[2]),'-o',contract])
                run('native-factory-contract.run',[contract,isolated/'xlir_native_factory_inherited.xl'])
            binary = isolated/'literal-contract'
            run('contract.build', [cc, *flags, '-O2', '-I'+str(root), '-I'+str(root/'release/include'),
                '-I'+str(root/'lib/tcc'), cases/helpers[3], *links(names[2]), '-o', binary])
            run('contract.run', [binary, isolated/(selected[0]+'.xl'),
                *([isolated/(selected[1]+'.xl')] if args.stage == 'once-event' else
                  [isolated/'xlir_class_tag_vocabulary.xl', isolated/'xlir_class_tag_narrow_first.xl']
                  if args.stage == 'cancellation' else
                  [isolated/(name+'.xl') for name in selected[1:3]] if args.stage == 'promise' else [])])
            if args.stage in CHANNEL_STAGES:
                deadline = isolated/'deadline-contract'
                run('deadline-contract.build',[cc,*flags,'-O2','-I'+str(root),
                    '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),cases/'xlir_channel_deadline_contract.c',
                    *links(names[2]),'-o',deadline])
                run('deadline-contract.run',[deadline,isolated/'stdlib_channel_deadlines.xl'])
                run('deadline-only-contract.run',[deadline,isolated/'stdlib_channel_deadline_only.xl','--minimal'])
            if args.stage=='channel-cancel':
                contract=isolated/'channel-cancel-contract'
                run('channel-cancel-contract.build',[cc,*flags,'-O2','-I'+str(root),
                    '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),cases/'xlir_channel_cancel_contract.c',
                    *links(names[2]),'-o',contract])
                run('channel-cancel-contract.run',[contract,isolated/'stdlib_channel_cancellation.xl',
                    isolated/'stdlib_channel_cancel_flow.xl'])
            if args.stage in ('atomic', 'once-event', 'thread-local', 'cancellation', 'promise', *CHANNEL_STAGES):
                planner = isolated/'planner'
                run('pruning.build', [cc, *flags, '-O2', '-I'+str(root), '-I'+str(root/'release/include'),
                    '-I'+str(root/'lib/tcc'), cases/helpers[5], *links(names[2]), '-o', planner])
                plans = (('stdlib_atomic_bool_only', {'xl6_atomic_bool.c', 'xl6_atomic_bool_domain.c'}),
                         ('stdlib_concurrent_import_only', set())) if args.stage == 'atomic' else (
                         ('stdlib_once', {'xl6_once.c', 'xl6_once_domain.c'}),
                         ('stdlib_event', {'xl6_event.c', 'xl6_event_domain.c'}),
                         ('stdlib_concurrent_import_only', set()))
                if args.stage == 'thread-local':
                    plans = (('stdlib_thread_local', {'xl6_thread_exit.c', 'xl6_thread_local.c',
                                                     'xl6_thread_local_domain.c'}),
                             ('stdlib_thread_only', {'xl6_thread_exit.c'}),
                             ('stdlib_concurrent_import_only', set()))
                if args.stage == 'cancellation':
                    core = {'xl6_cancellation_source.c','xl6_cancellation_source_domain.c'}
                    plans = (('stdlib_cancellation', core | {'xl6_cancellation_registration.c',
                                                            'xl6_cancellation_registration_domain.c'}),
                             ('stdlib_cancellation_source_only', core),
                             ('stdlib_concurrent_import_only', set()))
                if args.stage == 'promise':
                    core = {'xl6_promise.c','xl6_promise_domain.c','xl6_future_value.c'}
                    plans = (('stdlib_promise', core),
                             ('stdlib_promise_terminal', core | {'xl6_cancellation_source.c',
                                                                'xl6_cancellation_source_domain.c','xl6_thread_exit.c'}),
                             ('stdlib_concurrent_import_only', set()))
                if args.stage in CHANNEL_STAGES:
                    # Direct Channel operations do not erase/project Channel
                    # as a typed container element; that optional admission
                    # factory must stay absent from this executable closure.
                    core={'xl6_channel.c','xl6_ownership_domain_channel.c'}
                    plans=(('stdlib_channel_timeout',core|{'xl6_thread_exit.c'}),
                           ('stdlib_channel_deadlines',core|{'xl6_channel_wait.c','xl6_thread_exit.c'}),
                           ('stdlib_channel_deadline_only',core|{'xl6_channel_wait.c'}),
                           ('stdlib_concurrent_import_only',set()),('stdlib_channel_pruned',set()))
                    if args.stage=='channel-cancel':
                        cancellation=core|{'xl6_channel_cancel.c','xl6_cancellation_source.c','xl6_cancellation_source_domain.c'}
                        plans=(*plans,('stdlib_channel_cancel_only',cancellation),
                            ('stdlib_channel_cancel_flow',cancellation),
                            ('stdlib_channel_cancellation',cancellation|{'xl6_event.c','xl6_event_domain.c','xl6_thread_exit.c'}))
                for name, expected in plans:
                    plan = json.loads(run(name+'.plan', [planner, isolated/(name+'.xl')]))
                    sources = {Path(path).name for path in plan['Sources']}
                    scoped = {path for path in sources if path.startswith(('xl6_atomic_', 'xl6_cancellation_')) or path in {
                        'xl6_once.c', 'xl6_once_domain.c', 'xl6_event.c', 'xl6_event_domain.c',
                        'xl6_thread_exit.c', 'xl6_thread_local.c', 'xl6_thread_local_domain.c',
                        'xl6_promise.c', 'xl6_promise_domain.c', 'xl6_future_value.c',
                        'xl6_channel.c','xl6_channel_admission.c','xl6_ownership_domain_channel.c',
                        'xl6_channel_wait.c','xl6_channel_cancel.c','xl6_channel_select.c','xl6_channel_select_cancel.c'}}
                    assert scoped == expected, (name, scoped)
                if args.channel_case_language:
                    case_sources={'xl6_channel_case.c','xl6_channel_cases_select.c','xl6_channel_cases_select_cancel.c',
                        'xl6_ownership_domain_channel_case.c','xl6_channel_case_language.c','xl6_channel_case_array.c',
                        'xl6_channel_select_language.c','xl6_channel_select_cancel_language.c'}
                    for name,expected in (('stdlib_channel_cases',case_sources),
                        ('stdlib_channel_cases_receive_only',{'xl6_channel_case.c','xl6_ownership_domain_channel_case.c',
                            'xl6_channel_case_language.c'}),('stdlib_concurrent_import_only',set())):
                        plan=json.loads(run(name+'.cases-plan',[planner,isolated/(name+'.xl')]))
                        assert {Path(path).name for path in plan['Sources']} & case_sources == expected,(name,plan)
                        if name!='stdlib_channel_cases':
                            assert not {'XRT_MODULE_CHANNEL_SELECT','XRT_MODULE_CHANNEL_SELECT_CANCEL'} & set(plan['NativeModules'])
        if args.stage=='channel-select-native':
            # Native-only symbol-root plans. These do NOT certify a source
            # Channel select surface before its independent typed IR cutover.
            if not win:
                planner=isolated/'native-planner'
                run('channel.native-plan.build',[cc,*flags,'-O2','-I'+str(root),'-I'+str(root/'release/include'),
                    '-I'+str(root/'lib/tcc'),cases/helpers[6],*links(names[2]),'-o',planner])
                report['NativeChannelPlanOrigin']='actual-linux-compiler'
            else:
                assert donor,'Private planner is not a Windows DLL API; supply the actual portable-plan receipt'
                report['NativeChannelPlanOrigin']='actual-linux-compiler+identical-planning-sources'
                report['NativeChannelPlanDonorSha256']=digest(args.native_plan_report)
            # The registry/closure algorithm is platform-neutral data/code.
            # Pin ALL source bytes, not just four convenient component rows.
            # Actual per-platform native implementations are compiled/run below.
            report['NativeChannelPlanInputs']=[dict(RelativePath=path.relative_to(root).as_posix(),
                NormalizedSha256=hashlib.sha256(path.read_bytes().replace(b'\r\n',b'\n')).hexdigest())
                for path in sorted((root/'src').rglob('*')) if path.is_file()]
            report['NativeChannelPlans']={}
            native_cases=(('xl6ChannelReceiveWaitV2',{'xl6_channel_wait.c'},'XRT_MODULE_CHANNEL'),
                ('xl6ChannelReceiveCancelV2',{'xl6_channel_cancel.c'},'XRT_MODULE_CHANNEL_CANCEL'),
                ('xl6ChannelSelectV1',{'xl6_channel_select.c'},'XRT_MODULE_CHANNEL_SELECT'),
                ('xl6ChannelSelectCancelV1',{'xl6_channel_select.c','xl6_channel_select_cancel.c'},'XRT_MODULE_CHANNEL_SELECT_CANCEL'))
            selected_sources={'xl6_channel_wait.c','xl6_channel_cancel.c','xl6_channel_select.c','xl6_channel_select_cancel.c'}
            registry=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf8').split('xl6ArtifactComponents[] = {',1)[1].split('\n};',1)[0]
            catalog=set(re.findall(r'^\s*\{\s*"([^"\n]+\.c)"',registry,re.M))
            for kind,(symbol,expected,module) in enumerate(native_cases,1):
                plan=donor['NativeChannelPlans'][symbol] if win else json.loads(run(symbol+'.native-plan',[planner,symbol]))
                report['NativeChannelPlans'][symbol]=plan
                assert all(re.fullmatch(r'(?:XRT|XRUNTIME)_MODULE_[A-Z0-9_]+',name) for name in plan['NativeModules'])
                assert all(path in catalog and (root/path).resolve().is_relative_to(root/'src') and
                    Path(path).suffix=='.c' for path in plan['Sources'])
                assert {Path(p).name for p in plan['Sources']} & selected_sources == expected
                assert module in plan['NativeModules']
                allowed={module}|({'XRT_MODULE_CHANNEL_SELECT'} if module=='XRT_MODULE_CHANNEL_SELECT_CANCEL' else set())
                forbidden={'XRT_MODULE_CHANNEL_CANCEL','XRT_MODULE_CHANNEL_SELECT','XRT_MODULE_CHANNEL_SELECT_CANCEL'}-allowed
                assert not forbidden & set(plan['NativeModules'])
                native=out/(symbol+'.native.c')
                native.write_text(''.join('#define '+name+'\n' for name in plan['NativeModules'])+
                    '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8')
                binary=isolated/(symbol+'.pruned'+('.exe' if win else ''))
                systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if win else ['-pthread','-ldl','-lm']
                run(symbol+'.pruned.build',[cc,*flags,'-O2','-DTEST_KIND='+str(kind),'-I'+str(root),'-I'+str(root/'release'),
                    '-I'+str(root/'release/include'),*([] if win else ['-D_GNU_SOURCE']),
                    cases/helpers[7],native,*[root/p for p in plan['Sources']],*systems,'-o',binary])
                run(symbol+'.pruned.run',[binary])
                imports=run(symbol+'.pruned.imports',[shutil.which('objdump' if win else 'readelf'),'-p' if win else '-d',binary])
                assert not re.search(rb'(?:libxlc?\.so|\bxlc?\.dll)',imports,re.I)
                report.setdefault('NativeChannelProducts',[]).append(dict(Symbol=symbol,
                    NativeSource=str(native),NativeSourceSha256=digest(native),Binary=str(binary),BinarySha256=digest(binary)))
            report['PublicChannelSelectCutover']=False
        report['InputsUnchanged'] = all(digest(row['Path']) == row['Sha256'] for row in report['Inputs'])
        report['LogsUnchanged'] = all(digest(row['Log']) == row['Sha256'] for row in report['Rows'])
        assert report['InputsUnchanged'] and report['LogsUnchanged']
        assert not failures, failures
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        raise
    finally:
        save()
    print(f"{args.stage} offline acceptance passed: {len(report['Rows'])} phases; no network tests", flush=True)


if __name__ == '__main__':
    main()
