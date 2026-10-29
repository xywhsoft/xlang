"""Compile a genuinely independent C consumer of the emitted owned-string API.

--runtime-build accepts the Windows public source driver's build.json or a
Linux source compiler's result.json. Every compiler/runtime input and object
is hash-verified. The consumer is never an xlang sidecar; generated C, native
producers and the consumer are separate GCC translation units.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    compiler = parser.add_mutually_exclusive_group(required=True)
    compiler.add_argument('--driver', type=Path)
    compiler.add_argument('--cli', type=Path)
    parser.add_argument('--runtime-build', type=Path, required=True)
    parser.add_argument('--directory', type=Path, required=True)
    args = parser.parse_args()
    binary = (args.driver or args.cli).resolve()
    receipt = args.runtime_build.resolve()
    build = json.loads(receipt.read_text(encoding='utf-8-sig'))
    inputs = {receipt: sha(receipt), binary: sha(binary), Path(__file__).resolve(): sha(__file__)}
    if 'Inputs' in build:
        assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt']
        for row in build['Inputs'] + build['Objects'] + build['Headers']:
            assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']
            inputs[Path(row['Path'])] = sha(row['Path'])
        assert sha(binary).lower() == build['DriverSha256'].lower()
        links = [row['Path'] for row in build['Objects']
                 if Path(row['Path']).name not in ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
        tool_runtime = receipt.parent / 'xl.dll'
        if not tool_runtime.exists(): tool_runtime = ROOT / 'release/xl.dll'
        links += [str(tool_runtime), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
        includes = [ROOT, ROOT/'release', ROOT/'release/include', receipt.parent/'include']
    else:
        assert os.name != 'nt' and build['complete']
        for path, digest in build['inputs'].items():
            assert sha(path) == digest, path
            inputs[Path(path)] = digest
        for row in build['objects']:
            assert sha(row['object']) == row['sha256'], row['object']
            inputs[Path(row['object'])] = row['sha256']
        tool_runtime = binary.parent/'libxlc.so'
        assert sha(tool_runtime) == build['compilerSha256']
        links = [str(tool_runtime), '-Wl,-rpath,'+str(tool_runtime.parent), '-pthread', '-ldl', '-lm']
        includes = [ROOT, ROOT/'release', ROOT/'release/include']
    inputs[tool_runtime] = sha(tool_runtime)
    cc = shutil.which('gcc')
    assert cc
    inputs[Path(cc)] = sha(cc)
    work = args.directory.resolve()
    work.mkdir(parents=True, exist_ok=False)
    if os.name == 'nt': shutil.copy2(tool_runtime, work/'xl.dll')
    else:
        # The Linux compiler embeds this versioned private include tree in
        # VFS. A separate GCC consumer needs its physical SDK mirror; copying
        # headers supplies include files, never implicit C declarations.
        include_root = work/'include'
        private = include_root/'__xl_runtime'
        private.mkdir(parents=True)
        for path in (ROOT/'src/runtime').rglob('*'):
            if path.is_file() and path.suffix in ('.h', '.inc'):
                target = private/path.relative_to(ROOT/'src/runtime')
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path,target)
                inputs[path] = sha(path)
        runtime_header = ROOT/'release/include/xruntime_decl.h'
        shutil.copy2(runtime_header,private/runtime_header.name)
        inputs[runtime_header] = sha(runtime_header)
        includes.append(include_root)
    source = ROOT/'test/cases/extern_owned_string_public.xl'
    native = [ROOT/'test/cases/extern_owned_string_host.c', ROOT/'test/cases/extern_owned_string_public_host.c']
    template = ROOT/'test/cases/extern_owned_string_public_consumer.in'
    contract = ROOT/'test/cases/xlir_native_variadic_contract.c'
    oom_contract = ROOT/'test/cases/xlir_native_variadic_oom.c'
    task_scalar_contract = ROOT/'test/cases/xlir_task_c_scalar_contract.c'
    task_composition_contract = ROOT/'test/cases/xlir_task_composition_contract.c'
    task_optional_contract = ROOT/'test/cases/xlir_task_optional_contract.c'
    task_reference_contract = ROOT/'test/cases/xlir_task_reference_contract.c'
    error_array_contract = ROOT/'test/cases/xlir_error_array_admission.c'
    reference_boundary_contract = ROOT/'test/cases/xlir_error_transport_contract.c'
    bound_dispatch_contract = ROOT/'test/cases/xlir_bound_dispatch_contract.c'
    bound_constraint_contract = ROOT/'test/cases/xlir_bound_constraint_contract.c'
    bound_access_contract = ROOT/'test/cases/xlir_bound_access_contract.c'
    bound_c_owner_contract = ROOT/'test/cases/xlir_bound_c_owner_contract.c'
    bound_generic_owner_contract = ROOT/'test/cases/xlir_bound_generic_owner_contract.c'
    bound_nullable_generic_contract = ROOT/'test/cases/xlir_bound_nullable_generic_contract.c'
    producer_tail_contract = ROOT/'test/cases/xlir_task_producer_tail.c'
    for path in [source, *native, template, contract, oom_contract, task_scalar_contract, task_composition_contract, task_optional_contract,
            task_reference_contract,error_array_contract,
            reference_boundary_contract,bound_dispatch_contract,bound_constraint_contract,bound_access_contract,bound_c_owner_contract,bound_generic_owner_contract,bound_nullable_generic_contract,producer_tail_contract,
            ROOT/'test/cases/extern_owned_bound_nullable_generic_args.xl',
            ROOT/'test/cases/extern_owned_bound_constraint_casts.xl',
            ROOT/'test/cases/extern_owned_bound_nonpublic.xl',
            ROOT/'test/cases/extern_owned_bound_c_owners.xl',
            ROOT/'test/cases/extern_owned_bound_nested_generics.xl',
            ROOT/'test/cases/extern_owned_bound_base_dispatch.xl',
            ROOT/'test/cases/extern_owned_error_safe_index.xl',
            ROOT/'test/cases/extern_owned_bound_method_defaults.xl',
            ROOT/'test/cases/extern_owned_task_nullable_callbacks.xl',
            ROOT/'test/cases/extern_owned_task_dependent_defaults.xl',
            ROOT/'test/cases/extern_owned_task_error_arrays.xl',
            ROOT/'test/cases/extern_owned_task_generic.xl',
            ROOT/'test/cases/extern_owned_task_nullable.xl',
            ROOT/'test/cases/extern_owned_task_optional.xl',
            ROOT/'test/cases/extern_owned_task_optional_scalars.xl',
            ROOT/'test/cases/extern_owned_task_continuations.xl',
            ROOT/'test/cases/extern_owned_task_generic_callable.xl',
            ROOT/'test/cases/extern_owned_task_scalars.xl', ROOT/'test/cases/xlir_owner_consumption_contract.c',
                 ROOT/'test/cases/extern_c_variadic_value.xl']:
        inputs[path] = sha(path)
    checks = []

    def run(name, command):
        result = subprocess.run(list(map(str, command)), cwd=ROOT, capture_output=True, timeout=300)
        data = result.stdout + result.stderr
        (work/(name+'.log')).write_bytes(data)
        checks.append({'name': name, 'exitCode': result.returncode})
        assert result.returncode == 0, name+'\n'+data[-6000:].decode(errors='replace')
        print(name+': PASS', flush=True)
        return data.decode(errors='replace')

    prefix = work/'owned-public'
    run('emit', [binary, 'cgen', source, *native, '--emit', str(prefix)+'.c', '--emit-header', str(prefix)+'.h',
                 '--inc-path', ROOT/'release/include'] if args.cli else [binary, source, 'emit', prefix, *native])
    header = prefix.with_suffix('.h').read_text(encoding='utf-8-sig')
    pair_type = re.search(r'XLIR_C_API\s+(\w+)\s+owned_public_pair\(', header)
    assert pair_type, 'actual public header did not declare the product export'
    consumer = work/'consumer.c'
    consumer.write_text(template.read_text().replace('@PAIR_TYPE@', pair_type[1]), encoding='utf-8')
    flags = ['-std=c11', '-DXL_STATIC_EMBED', *['-I'+str(path) for path in includes]]
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']
    for optimization in ('O0', 'O2'):
        objects = []
        for index, unit in enumerate([prefix.with_suffix('.c'), *native, consumer]):
            obj = work/(optimization+'-'+str(index)+'.o')
            strict = ['-Wall', '-Wextra', '-Werror'] if unit == consumer else []
            run(optimization+'-compile-'+str(index), [cc, *flags, *strict, '-'+optimization, '-c', unit, '-o', obj])
            objects.append(obj)
        output = work/(optimization+('.exe' if os.name == 'nt' else ''))
        # A response file keeps the full verified object closure out of shell
        # command-length limits. It is GCC syntax, never shell-interpreted.
        response = work/(optimization+'-link.rsp')
        response.write_text('\n'.join('"'+str(value).replace('\\', '/')+'"' for value in
            [*objects, *links, '-o', output]), encoding='utf-8')
        run(optimization+'-link', [cc, '@'+str(response)])
        assert '120 OOM prefixes balanced' in run(optimization+'-run', [output])
    output = work/('native-variadic-contract'+('.exe' if os.name == 'nt' else ''))
    response = work/'native-variadic-contract.rsp'
    response.write_text('\n'.join('"'+str(value).replace('\\', '/')+'"' for value in
        [*flags, '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
         '-Wno-unused-parameter', '-Wno-missing-field-initializers', contract, *links, '-o', output]), encoding='utf-8')
    run('native-variadic-contract-build', [cc, '@'+str(response)])
    assert '6 mutations rejected' in run('native-variadic-contract', [output])
    output = work/('task-c-scalar-contract'+('.exe' if os.name == 'nt' else ''))
    response = work/'task-c-scalar-contract.rsp'
    response.write_text('\n'.join('"'+str(value).replace('\\', '/')+'"' for value in
        [*flags, '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
         '-Wno-unused-parameter', '-Wno-missing-field-initializers', task_scalar_contract, *links, '-o', output]), encoding='utf-8')
    run('task-c-scalar-contract-build', [cc, '@'+str(response)])
    assert '6 mutations rejected' in run('task-c-scalar-contract', [output])
    output = work/('task-composition-contract'+('.exe' if os.name == 'nt' else ''))
    response = work/'task-composition-contract.rsp'
    response.write_text('\n'.join('"'+str(value).replace('\\', '/')+'"' for value in
        [*flags, '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
         '-Wno-unused-parameter', '-Wno-missing-field-initializers', task_composition_contract, *links, '-o', output]), encoding='utf-8')
    run('task-composition-contract-build', [cc, '@'+str(response)])
    assert '9 mutations rejected' in run('task-composition-contract', [output])
    output = work/('task-optional-contract'+('.exe' if os.name == 'nt' else ''))
    response = work/'task-optional-contract.rsp'
    response.write_text('\n'.join('"'+str(value).replace('\\', '/')+'"' for value in
        [*flags, '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
         '-Wno-unused-parameter', '-Wno-missing-field-initializers', task_optional_contract, *links, '-o', output]), encoding='utf-8')
    run('task-optional-contract-build', [cc, '@'+str(response)])
    assert '10 mutations rejected' in run('task-optional-contract', [output])
    for name,unit,message in [
            ('bound-dispatch-contract',bound_dispatch_contract,'6 rejected mutations'),
            ('bound-constraint-contract',bound_constraint_contract,'missing constraint evidence rejected'),
            ('bound-access-contract',bound_access_contract,'access evidence checked, dynamic registry remains public'),
            ('bound-c-owner-contract',bound_c_owner_contract,'one receiver, one call, exact per-result copy plan'),
            ('bound-generic-owner-contract',bound_generic_owner_contract,'ordered owner/method keys authenticated'),
            ('bound-nullable-generic-contract',bound_nullable_generic_contract,'concrete argument ABI remains checked'),
            ('task-reference-contract',task_reference_contract,'8 mutations rejected'),
            ('reference-boundary-contract',reference_boundary_contract,'6 mutations rejected'),
            ('error-array-admission',error_array_contract,'64 OOM prefixes balanced')]:
        output=work/(name+('.exe' if os.name=='nt' else ''))
        response=work/(name+'.rsp')
        response.write_text('\n'.join('"'+str(value).replace('\\','/')+'"' for value in
            [*flags,'-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
             '-Wno-unused-parameter','-Wno-missing-field-initializers',unit,*links,'-o',output]),encoding='utf-8')
        run(name+'-build',[cc,'@'+str(response)])
        assert message in run(name,[output])
    # This pass-level test supplies the task-frame implementation so it can
    # construct the final producer reference deterministically. Every other
    # provider comes from the verified cohort, including native worker/join.
    frame_name='xl6_task_frame.c'
    tail_links=([row['Path'] for row in build['Objects'] if Path(row['Source']).name!=frame_name and
                 not Path(row['Source']).name.startswith('xlir_public_source_')]+[str(tool_runtime),
                 '-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']) if 'Inputs' in build else [
                 *[row['object'] for row in build['objects'] if Path(row['source']).name!=frame_name],'-pthread','-ldl','-lm']
    output=work/('producer-tail-contract'+('.exe' if os.name=='nt' else ''))
    response=work/'producer-tail-contract.rsp'
    response.write_text('\n'.join('"'+str(value).replace('\\','/')+'"' for value in
        [*flags,'-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter',
         '-Wno-missing-field-initializers',producer_tail_contract,*tail_links,'-o',output]),encoding='utf-8')
    run('producer-tail-contract-build',[cc,'@'+str(response)])
    assert '64 deterministic BUSY deferrals' in run('producer-tail-contract',[output])
    # GNU wrapping must reach calls inside the compiler, so link its verified
    # source objects on Linux too, rather than wrapping the shared-library API.
    oom_links = links if 'Inputs' in build else [
        *[row['object'] for row in build['objects']], '-pthread', '-ldl', '-lm']
    output = work/('native-variadic-oom'+('.exe' if os.name == 'nt' else ''))
    response = work/'native-variadic-oom.rsp'
    response.write_text('\n'.join('"'+str(value).replace('\\', '/')+'"' for value in
        [*flags, '-O2', '-Wall', '-Wextra', '-Werror', oom_contract, *oom_links,
         '-Wl,--wrap=xrtCallocAt,--wrap=xrtStrDup', '-o', output]), encoding='utf-8')
    run('native-variadic-oom-build', [cc, '@'+str(response)])
    assert '5 allocation failures and partial disposal safe' in run('native-variadic-oom', [output])
    assert all(sha(path) == digest for path, digest in inputs.items()), 'source or runtime changed during gate'
    (work/'result.json').write_text(json.dumps({'complete': True, 'platform': sys.platform,
        'independentTranslationUnit': True, 'checks': checks,
        'inputs': {str(path): digest for path, digest in inputs.items()},
        'products': {str(path): sha(path) for path in [prefix.with_suffix('.c'), prefix.with_suffix('.h'), consumer]}},
        indent=2), encoding='utf-8')


if __name__ == '__main__': main()
