"""Offline bounded native callback byte spans, epochs and sealed IR.

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
    source = root/'test/cases/foreign_callback_span.xl'
    provider = root/'test/cases/foreign_callback_span_native.c'
    contract = root/'test/cases/foreign_callback_span_ir.c'
    pinned.update((Path(__file__).resolve(), Path(__file__).with_name('runtime_layout.py'),
        source, provider, contract, root/'test/cases/foreign_callback_span.h',
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
    assert 'callback span requires const xbytesview pointer' in text
    assert 'native_span_0=(xbytesview)' in text and 'native_span_1=(xbytesview)' in text
    assert 'xl6ForeignScopeViewMakeV1' in text and 'raw0_count<0' in text
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
        ('header-count-width','size_t,int,void*','int,int,void*'),
        ('header-pointer-type','const unsigned char*,size_t','const float*,size_t'),
    ):
        assert old in header
        overlay=out/name;alternate=overlay/'test/cases/foreign_callback_span.h'
        alternate.parent.mkdir(parents=True);alternate.write_text(header.replace(old,new),encoding='utf-8')
        run(name,[cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(overlay),*includes,
            '-x','c','-fsyntax-only',out/'public-header.h'],refused=True,semantic=False)
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
    annotation='@c_callback_span(1,0,"const unsigned char*","size_t")'
    negatives = {
        'duplicate-span': original.replace(annotation,annotation+' '+annotation,1),
        'missing-callback-owner': original.replace(' @c_owned_callback(1,0,"SpanOwner",-7)','',1),
        'parameter-out-of-range': original.replace(annotation,'@c_callback_span(3,0,"const unsigned char*","size_t")',1),
        'argument-out-of-range': original.replace(annotation,'@c_callback_span(1,3,"const unsigned char*","size_t")',1),
        'argument-scalar': original.replace(annotation,'@c_callback_span(1,1,"const unsigned char*","size_t")',1),
        'mutable-pointer': original.replace(annotation,'@c_callback_span(1,0,"unsigned char*","size_t")',1),
        'non-byte-pointer': original.replace(annotation,'@c_callback_span(1,0,"const float*","size_t")',1),
        'count-abi': original.replace(annotation,'@c_callback_span(1,0,"const unsigned char*","uint32_t")',1),
        'nullable-view': original.replace('typedef ByteHandler = func(Slice,c_int)','typedef ByteHandler = func(Slice?,c_int)',1),
        'owned-view': original.replace('typedef ByteHandler = func(Slice,c_int)','typedef ByteHandler = func(Root,c_int)',1),
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
