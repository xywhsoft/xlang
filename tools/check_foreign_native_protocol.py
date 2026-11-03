"""Offline native/object protocol witnesses and exact C pointer-contract aliases.

Uses one verified source compiler/runtime cohort. Development-only --skip-tcc
does not certify the precompiled provider or shipping XGE/XUI packages.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time, re
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
    source = root/'test/cases/foreign_native_protocol.xl'
    provider = root/'test/cases/foreign_managed_view_native.c'
    contract = root/'test/cases/foreign_native_protocol_ir.c'
    scope_source = root/'test/cases/foreign_native_protocol_scope.xl'
    scope_provider = root/'test/cases/foreign_scoped_view_native.c'
    regressions=sorted((root/'test/cases').glob('protocol*.xl'))
    legacy_provider=root/'test/cases/protocol_native_backed_host.c'
    pinned.update((Path(__file__).resolve(), Path(__file__).with_name('runtime_layout.py'),
                   source, provider, contract, scope_source, scope_provider, root/'test/cases/foreign_scoped_view.h', root/'test/cases/foreign_managed_view.h', root/'test/cases/foreign_decl_module_host.c', Path(cc)))
    pinned.update([*regressions,legacy_provider])
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs = {str(p): sha(p) for p in pinned}
    out.mkdir(parents=True, exist_ok=False)
    (out/'tmp').mkdir()
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root), TEMP=str(out/'tmp'), TMP=str(out/'tmp'))
    report = dict(complete=False, passed=False, scope=__doc__, networkTests=False,
                  sourceBuild=str(public/'build.json'), tccIncluded=not args.skip_tcc, inputs=inputs, rows=[])

    def save():
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')

    def run(name, command, *, refused=False, semantic=True):
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
        if refused and semantic:
            assert b'[sem]' in result.stdout+result.stderr, log

    generated = out/'managed.c'
    run('cgen', [driver, source, 'cgen-default', generated])
    run('ir', [driver, source, 'ir-lowered', out/'ir.json'])
    text = generated.read_text(encoding='utf-8-sig')
    assert 'declared C function ABI' in text and 'switch (xrtValueTypeId(boxed_receiver))' in text
    assert 'extern int managed_child_create(' not in text
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
                '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-Wno-unused-parameter']
    scope_generated=out/'scope.c'
    run('scope-cgen',[driver,scope_source,'cgen-default',scope_generated])
    assert 'xl6ScopeViewUnboxV1' in scope_generated.read_text(encoding='utf-8-sig')
    suffix = '.exe' if os.name == 'nt' else ''
    for optimization in ('O0', 'O2'):
        flags = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-'+optimization, *includes]
        executable = out/('managed-'+optimization+suffix)
        run(optimization+'-build', [*flags, *warnings, generated, provider,
            root/'test/cases/foreign_decl_module_host.c', *objects, runtime, *systems, '-o', executable])
        run(optimization+'-run', [executable])
        executable=out/('scope-'+optimization+suffix)
        run('scope-'+optimization+'-build',[*flags,*warnings,scope_generated,scope_provider,
            root/'test/cases/foreign_decl_module_host.c',*objects,runtime,*systems,'-o',executable])
        run('scope-'+optimization+'-run',[executable])
        executable = out/('IR-'+optimization+suffix)
        run('IR-'+optimization+'-build', [*flags, contract, *objects, runtime, *systems, '-o', executable])
        run('IR-'+optimization+'-run', [executable, source])
    if not args.skip_tcc:
        for mode in ('run', 'module', 'exe'):
            prefix = out/('tcc-'+mode)
            run('tcc-'+mode, [driver, source, mode, prefix, provider])
            if mode == 'exe':
                run('tcc-standalone', [prefix.with_suffix('.exe') if os.name == 'nt' else prefix])
            prefix=out/('scope-tcc-'+mode)
            run('scope-tcc-'+mode,[driver,scope_source,mode,prefix,scope_provider])
            if mode=='exe':run('scope-tcc-standalone',[prefix.with_suffix('.exe') if os.name=='nt' else prefix])
    original = source.read_text(encoding='utf-8-sig')
    # Protocols use the same nominal identity domain for every storage family.
    # Exercise existing object, inherited, generic, property, async, container,
    # weak and lifetime fixtures as well as the new mixed native-value paths.
    for case in regressions:
        generated=out/(case.stem+'.c')
        test_source=case
        original_case=case.read_text(encoding='utf-8-sig')
        if not re.search(r'\bfunc\s+main\s*\(',original_case):
            # Top-level-only fixtures need a script entrypoint for this host.
            # Their existing initializer, including top-level await, is intact.
            test_source=out/(case.stem+'.xl')
            test_source.write_text(original_case+'\nfunc main(...) as int\n return 0\nend\n',encoding='utf8')
        run(case.stem+'-cgen',[driver,test_source,'cgen-default',generated])
        executable=out/(case.stem+suffix)
        extra=[legacy_provider] if case.stem=='protocol_native_backed_dispatch' else []
        run(case.stem+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-O2',
            *includes,*warnings,generated,*extra,root/'test/cases/foreign_decl_module_host.c',*objects,runtime,*systems,'-o',executable])
        run(case.stem+'-run',[executable])
    negatives = {
        'incomplete-protocol': original.replace('func read() as c_int', 'func unread() as c_int', 1),
        'no-implicit-downcast': original.replace('Root root=create(0)', 'DerivedRoot root=create(0)', 1),
        'caller-protocol-erasure': 'extern "ManagedRoot*" class Raw @header("test/cases/foreign_managed_view.h") @lifetime("caller")\n native point handle\n extern "managed_root_read" @no_raise func read() as c_int\nend\nprotocol Reader\n func read() as c_int\nend\nfunc invalid(Raw pointer)\n Reader value=pointer\nend\n',
        'incompatible-view-contract': original+'\nextern "ManagedRoot*" class Alien @header("test/cases/foreign_managed_view.h") @lifetime("owned") @policy("managedChildPolicy") @scope("ManagedScope") @watch("ManagedWatch")\n native point handle\nend\nextern "managed_root_destroy" @no_raise func alienDispose(Alien value)\n',
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
