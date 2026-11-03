"""Offline managed foreign-view declarations, ownership propagation and sealed IR.

Uses one verified source compiler/runtime cohort. Development-only --skip-tcc
does not certify the precompiled provider or shipping XGE/XUI packages.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build
from runtime_layout import runtime_directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'public-directory', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--skip-tcc', action='store_true')
    args = parser.parse_args()
    root, public, out = [p.resolve() for p in (args.root, args.public_directory, args.out)]
    build, pinned, _ = verify_public_build(root, public)
    driver = Path(build['Driver'])
    cc = shutil.which('gcc')
    assert cc
    source = root/'test/cases/foreign_managed_view.xl'
    provider = root/'test/cases/foreign_managed_view_native.c'
    contract = root/'test/cases/foreign_managed_view_ir.c'
    pinned.update((Path(__file__).resolve(), Path(__file__).with_name('runtime_layout.py'),
                   source, provider, contract, root/'test/cases/foreign_managed_view.h', Path(cc)))
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs = {str(p): sha(p) for p in pinned}
    out.mkdir(parents=True, exist_ok=False)
    (out/'tmp').mkdir()
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root), TEMP=str(out/'tmp'), TMP=str(out/'tmp'))
    report = dict(complete=False, passed=False, scope=__doc__, networkTests=False,
                  sourceBuild=str(public/'build.json'), tccIncluded=not args.skip_tcc, inputs=inputs, rows=[])

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, *, refused=False):
        command = command_with_response(command, cc, out/(name+'.rsp'))
        started = time.monotonic()
        result = subprocess.run(list(map(str, command)), cwd=root, env=env, capture_output=True, timeout=240)
        log = out/(name+'.log')
        log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name, exitCode=result.returncode, refused=refused,
            seconds=round(time.monotonic()-started, 3), logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert result.returncode == (1 if refused else 0), log
        if refused:
            assert b'[sem]' in result.stdout+result.stderr, log

    generated = out/'managed.c'
    run('cgen', [driver, source, 'cgen-default', generated])
    run('ir', [driver, source, 'ir-lowered', out/'ir.json'])
    text = generated.read_text(encoding='utf-8-sig')
    assert 'managed_root_create(' in text and 'xl6ForeignViewCallBeginV1' in text
    assert 'xl6ForeignViewCreateRootProbedV1' in text and 'xl6ForeignViewBorrowChildV1' in text
    runtime = runtime_directory(root)/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    sdk=runtime_directory(root) if os.name=='nt' else Path(build.get('SdkDirectory',root/'release/win_x64'))
    if os.name!='nt':env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    if os.name == 'nt':
        shutil.copy2(runtime, out/runtime.name)
    includes = ['-I'+str(root), '-I'+str(public/'include'), '-I'+str(sdk), '-I'+str(sdk/'include')]
    run('emit-header',[driver,source,'emit',out/'public-header',provider])
    run('header-C-ABI',[cc,'-std=c11','-Wall','-Wextra','-Werror','-x','c','-fsyntax-only',*includes,out/'public-header.h'])
    objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
               ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else \
        ['-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-Wl,-rpath,'+str(runtime.parent)]
    warnings = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
                '-Wno-unused-const-variable', '-Wno-unused-but-set-variable']
    suffix = '.exe' if os.name == 'nt' else ''
    for optimization in ('O0', 'O2'):
        flags = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-'+optimization, *includes]
        executable = out/('managed-'+optimization+suffix)
        run(optimization+'-build', [*flags, *warnings, generated, provider,
            root/'test/cases/foreign_decl_module_host.c', *objects, runtime, *systems, '-o', executable])
        run(optimization+'-run', [executable])
        executable = out/('IR-'+optimization+suffix)
        run('IR-'+optimization+'-build', [*flags, contract, *objects, runtime, *systems, '-o', executable])
        run('IR-'+optimization+'-run', [executable, source])
    if not args.skip_tcc:
        for mode in ('run', 'module', 'exe'):
            prefix = out/('tcc-'+mode)
            run('tcc-'+mode, [driver, source, mode, prefix, provider])
            if mode == 'exe':
                run('tcc-standalone', [prefix.with_suffix('.exe') if os.name == 'nt' else prefix])
    original = source.read_text(encoding='utf-8-sig')
    negatives = {
        'missing-policy': original.replace(' @policy("managedRootPolicy")', '', 1),
        'owner-on-root': original.replace('@lifetime("owned")', '@lifetime("owned") @owner(Child)', 1),
        'owner-is-child': original.replace('@owner(Root)', '@owner(Child)', 1),
        'missing-owner': original.replace(' @owner(Root)', '', 1),
        'missing-root-result': original.replace(' @c_result("owned_view")', '', 1),
        'bad-owner-source': original.replace('@c_result("owner_view",0)', '@c_result("owner_view",63)', 1),
        'bad-control-index': original.replace('@c_control(0)', '@c_control(63)', 1),
        'control-owner-source': original.replace('@c_result("owner_view",0) func first', '@c_control(0) @c_result("owner_view",0) func first', 1),
        'root-pure-factory': original.replace('"managed_root_create" @no_raise', '"managed_root_create" @pure @no_raise', 1),
        'root-nullable-factory': original.replace('func create(c_int mode) as Root', 'func create(c_int mode) as Root?', 1),
    }
    for name, content in negatives.items():
        assert content != original
        case, output = out/(name+'.xl'), out/(name+'.c')
        case.write_text(content, encoding='utf-8')
        run(name, [driver, case, 'cgen-default', output], refused=True)
        assert not output.exists()
    report['inputsUnchanged'] = all(sha(p) == digest for p, digest in inputs.items())
    assert report['inputsUnchanged']
    report.update(complete=True, passed=True)
    save()


if __name__ == '__main__':
    main()
