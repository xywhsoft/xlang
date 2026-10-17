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


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', choices=('p0',), required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
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
    inputs = {Path(__file__).resolve(), *products,
              *[cases/(name+'.xl') for name in selected+negatives],
              *[cases/name for name in helpers],
              task/'test/support/module_product_memory.c', task/'test/support/module_product_memory.h'}
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
        strict = [*flags, '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
                  '-Wno-unused-label', '-Wno-unused-const-variable', '-Wno-unused-but-set-variable',
                  '-Wno-missing-field-initializers', '-I'+str(task), '-I'+str(root),
                  '-I'+str(root/'release'), '-I'+str(sdk)]
        cli = release/names[0]
        for name in selected:
            source = isolated/(name+'.xl')
            shutil.copy2(cases/source.name, source)
            run(name+'.jit', [cli, 'run', source])
            first, second = out/(name+'.c'), out/(name+'.repeat.c')
            run(name+'.cgen', [cli, 'cgen', source, '--emit', first])
            run(name+'.repeat', [cli, 'cgen', source, '--emit', second])
            assert first.read_bytes() == second.read_bytes()
            if name != selected[0]:
                continue
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
        for name in negatives:
            source = isolated/(name+'.xl')
            shutil.copy2(cases/source.name, source)
            target = out/(name+'.c')
            target.write_bytes(b'UNCHANGED\n')
            output = run(name+'.reject', [cli, 'cgen', source, '--emit', target], negative=True)
            assert b'[ir]' in output or b'[sem]' in output
            assert target.read_bytes() == b'UNCHANGED\n'
        if not win:
            binary = isolated/'literal-contract'
            run('contract.build', [cc, *flags, '-O2', '-I'+str(root), '-I'+str(root/'release/include'),
                '-I'+str(root/'lib/tcc'), cases/helpers[3], *links(names[2]), '-o', binary])
            run('contract.run', [binary, isolated/(selected[0]+'.xl')])
        report['InputsUnchanged'] = all(digest(row['Path']) == row['Sha256'] for row in report['Inputs'])
        report['LogsUnchanged'] = all(digest(row['Log']) == row['Sha256'] for row in report['Rows'])
        assert report['InputsUnchanged'] and report['LogsUnchanged']
        report['Complete'] = True
    except Exception as error:
        report['Error'] = repr(error)
        raise
    finally:
        save()
    print(f"P0 offline acceptance passed: {len(report['Rows'])} phases; no network tests", flush=True)


if __name__ == '__main__':
    main()
