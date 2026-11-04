"""Offline native owned callback text results, cleanup and sealed IR.

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
    source = root/'test/cases/foreign_callback_text.xl'
    provider = root/'test/cases/foreign_callback_span_native.c'
    contract = root/'test/cases/foreign_callback_text_ir.c'
    mixed = root/'test/cases/foreign_callback_text_gcc_host.c'
    pinned.update((Path(__file__).resolve(), Path(__file__).with_name('runtime_layout.py'),
        source, provider, contract, mixed, root/'test/cases/foreign_callback_span.h',
        root/'test/cases/foreign_decl_module_host.c',root/'src/runtime/xl6_foreign_view_abi.h',Path(cc)))
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
    assert 'xrtFree((void*)result.Data)' in text
    # The typed C return is the owned language result, without a second copy.
    thunks=re.split(r'\bXL6_NativeCallback\s*\*\s*record\s*=\s*userdata\s*;',text)[1:]
    assert len(thunks)==2
    for body in thunks:
        hot=body.split('    return result;',1)[0]
        assert 'xlir_string_copy(' not in hot and 'memcpy(' not in hot and 'strlen(' not in hot
    runtime = runtime_directory(root)/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    sdk=runtime_directory(root) if os.name=='nt' else Path(build.get('SdkDirectory',root/'release/win_x64'))
    if os.name!='nt':env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    if os.name == 'nt':
        shutil.copy2(runtime, out/runtime.name)
    includes = ['-I'+str(root), '-I'+str(public/'include'), '-I'+str(sdk), '-I'+str(sdk/'include')]
    run('emit-header',[driver,source,'emit',out/'public-header',provider])
    run('header-C-ABI',[cc,'-std=c11','-Wall','-Wextra','-Werror','-x','c','-fsyntax-only',*includes,out/'public-header.h'])
    header=(root/'test/cases/foreign_callback_span.h').read_text(encoding='utf-8-sig')
    for name,old,new in (
        ('header-borrowed-pointer-return','typedef xstrview(*SpanTextCallback)','typedef const char*(*SpanTextCallback)'),
    ):
        assert old in header
        overlay=out/name;alternate=overlay/'test/cases/foreign_callback_span.h'
        alternate.parent.mkdir(parents=True);alternate.write_text(header.replace(old,new),encoding='utf-8')
        run(name,[cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(overlay),*includes,
            '-x','c','-fsyntax-only',out/'public-header.h'],refused=True,semantic=False)
    declarations=source.read_text(encoding='utf-8-sig').split('func cases()',1)[0]
    empty=out/'empty-only.xl';empty.write_text(declarations+'func main(...) as int\n Root root=create()\n on(root,null)\n root.close()\n return 0\nend\n',encoding='utf-8')
    run('empty-only-cgen',[driver,empty,'cgen-default',out/'empty-only.c'])
    run('empty-only-C-ABI',[cc,'-std=c11','-Wall','-Wextra','-Werror',*includes,'-fsyntax-only',out/'empty-only.c'])
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
        mixed_exe=out/('gcc-native-tcc-callback'+suffix)
        flags=[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-O2',*includes]
        run('mixed-build',[*flags,mixed,provider,*objects,runtime,*systems,'-o',mixed_exe])
        if os.name=='nt':
            shutil.copy2(public/'xlc.dll',out/'xlc.dll')
            tcc_library=root/'res/tcc/win64/x86_64'
            compiler_runtime=out/'xlc.dll'
        else:
            tcc_library=Path(build['NativeRoot'])/'build/linux/tcc-dist'
            compiler_runtime=Path(build['RuntimeLibrary'])
        for mode in ('run','module'):
            run('mixed-'+mode,[mixed_exe,source,root,public/'include',sdk,compiler_runtime,
                tcc_library,out/('mixed-'+mode+'-tmp'),mode])
        for mode in ('run', 'module', 'exe'):
            prefix = out/('tcc-'+mode)
            run('tcc-'+mode, [driver, source, mode, prefix, provider])
            if mode == 'exe':
                run('tcc-standalone', [prefix.with_suffix('.exe') if os.name == 'nt' else prefix])
    original = source.read_text(encoding='utf-8-sig')
    marker='@c_owned_callback(1,0,"SpanOwner",0,"owned_text")'
    negatives = {
        'missing-result-ownership': original.replace(marker,'@c_owned_callback(1,0,"SpanOwner",0)',1),
        'unknown-result-ownership': original.replace(marker,'@c_owned_callback(1,0,"SpanOwner",0,"owned_bytes")',1),
        'integer-result': original.replace('typedef TextHandler = func(Slice,c_int) as string','typedef TextHandler = func(Slice,c_int) as c_int',1),
        'void-result': original.replace('typedef TextHandler = func(Slice,c_int) as string','typedef TextHandler = func(Slice,c_int)',1),
        'nonempty-failure-result': original.replace(marker,'@c_owned_callback(1,0,"SpanOwner",-1,"owned_text")',1),
        'duplicate-ownership-marker': original.replace(marker,'@c_owned_callback(1,0,"SpanOwner",0,"owned_text","owned_text")',1),
        'unknown-order': original.replace(marker,'@c_owned_callback(1,0,"SpanOwner",0,"middle","owned_text")',1),
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
