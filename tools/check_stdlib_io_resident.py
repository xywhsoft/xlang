"""Resident IO providers: real factories, failure prefixes and original products.

The selected IO/native-core implementations may be instrumented on Linux.
This does not certify IO cycle retirement, transitive process Future ownership,
all runtime providers, or full53 acceptance.
"""
from pathlib import Path
import argparse,json,os,re,shutil,subprocess,sys,time
from xlir_public_build_inputs import sha,verify_public_build
import stdlib_io_dependencies

FIXTURES=('stdlib_io_allocation','stdlib_io_buffer','stdlib_io_console_constants',
    'stdlib_io_files','stdlib_io_protocol','stdlib_io_reader_only','stdlib_io_stdin',
    'stdlib_io_streams','stdlib_io_writer_only','stdlib_fs_resources','stdlib_fs_allocation','stdlib_fs_temp')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve();assert not out.exists()
    windows=os.name=='nt';assert not(a.sanitize and windows)
    build,pinned,compared=verify_public_build(root,public)
    task=Path(__file__).resolve().parents[1];native=task/'test/support/io_resident_native.c'
    cc=shutil.which('gcc');assert cc
    shipping=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert shipping['Complete'] and shipping['InputsUnchanged']
    products=[root/'release'/name for name in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    actual={Path(row['Path']).resolve():row['Sha256'] for row in shipping['Products']}
    assert all(actual[path]==sha(path) for path in products)
    for row in shipping['Inputs']+shipping['Products']:assert sha(row['Path'])==row['Sha256'];pinned.add(Path(row['Path']))
    selected=['tools/xrt_single_header_impl.c','src/runtime/xl6_fs_file_owner.c',
        'src/runtime/xl6_fs_map_owner.c','src/runtime/xl6_fs_lock_owner.c','src/runtime/xl6_fs_root_owner.c',
        *[path.relative_to(root).as_posix() for path in sorted((root/'src/runtime').glob('xl6_io_*.c'))]]
    assert len(selected)==22
    runtime=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    fixtures=[root/'test/cases'/(name+'.xl') for name in FIXTURES]
    pinned.update((Path(__file__).resolve(),Path(stdlib_io_dependencies.__file__).resolve(),Path(sys.executable),Path(cc),Path(shutil.which('nm')),a.build_report.resolve(),native,runtime,*fixtures,*[root/name for name in selected]))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)];out.mkdir(parents=True)
    compiler=out/'compiler';(compiler/'release').mkdir(parents=True);relocated=out/'relocated';relocated.mkdir();tmp=out/'tmp';tmp.mkdir()
    for product in products:
        shutil.copy2(product,compiler/'release'/product.name)
        if windows:shutil.copy2(product,compiler/product.name)
    assert not any((compiler/name).exists() for name in ('src','lib','res','include'))
    for directory in (compiler/'build/test',relocated/'build/test'):directory.mkdir(parents=True)
    env={key:value for key,value in os.environ.items() if not key.upper().startswith('XL6_') and key not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp));native_env=dict(env)
    if not windows:native_env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if a.sanitize:native_env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if windows:shutil.copy2(runtime,out/'xl.dll')
    rows=[];proofs=[];cases=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,ComparedSources=compared,Scope=__doc__,Rows=rows,FactoryProofs=proofs,Cases=cases,
        SelectedProvidersInstrumented=a.sanitize,InstrumentedSources=selected if a.sanitize else [],ProviderOverrides=False,IoCycleRetirementAcceptance=False,Full53Acceptance=False)
    def run(name,command,environment=env,directory=compiler,data=b''):
        command=list(map(str,command));start=time.monotonic()
        r=subprocess.run(command,cwd=directory,env=environment,input=data,capture_output=True,timeout=600)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert r.returncode==0,(name,(r.stdout+r.stderr)[-6000:]);return r.stdout+r.stderr
    try:
        report['NativeDependencyProofs']=stdlib_io_dependencies.audit(root,build,lambda name,command:run(name,command,directory=root))
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
        libs=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
        if not windows:flags+=['-D_GNU_SOURCE']
        san=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if a.sanitize else []
        suffix='.exe' if windows else ''
        for opt in ('O0','O2'):
            actual=list(objects)
            if a.sanitize:
                actual=[path for path in objects if Path(path).stem not in {Path(name).stem for name in selected}]
                for index,name in enumerate(selected):
                    obj=out/(opt+'.provider'+str(index)+'.o')
                    run(opt+'.provider'+str(index),[cc,*flags,*san,'-'+opt,'-DXL_ENABLE_EXE_COMPILER=1','-DTCC_DISABLE_BUILTIN_VFS',
                        '-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label','-Wno-missing-field-initializers','-c',root/name,'-o',obj],native_env)
                    actual.append(str(obj))
            image=out/('native.'+opt+suffix)
            run(opt+'.native.build',[cc,*flags,*san,'-'+opt,native,*actual,*libs,'-o',image],native_env)
            data=run(opt+'.native',[image,out/'native-data.bin'],native_env).decode()
            matches=re.findall(r'io factory (\d+) prefixes=(\d+) first-no-hit=(\d+) balanced',data);assert len(matches)==10
            assert all(int(runs)==int(first)+1 and int(first)>0 for _,runs,first in matches)
            proofs.extend(dict(Optimization=opt,Factory=int(which),Prefixes=int(runs),FirstNoHit=int(first)) for which,runs,first in matches)
            assert 'io first-failure including NULL; explicit close and exactly-once cleanup passed' in data
            assert 'io unknown and process provenance refused: 6 controls, no hidden edge visited' in data
            assert 'io retained File map, lock and root native destruction balanced' in data
        for fixture in fixtures:
            local=compiler/fixture.name;shutil.copy2(fixture,local);name=fixture.stem
            data=b'abc' if name=='stdlib_io_stdin' else b''
            c=out/(name+'.c');repeat=out/(name+'.repeat.c')
            run(name+'.emit',[compiler/'release'/products[0].name,'cgen',local,'--emit',c])
            run(name+'.repeat',[compiler/'release'/products[0].name,'cgen',local,'--emit',repeat])
            text=c.read_text();assert c.read_bytes()==repeat.read_bytes()
            assert not re.search(r'^(?:static\s+)?(?:bool|void\*?|xvalue\*|int64_t)\s+(?:xl6_xrt_io_\w+|xl6_xrt_fs_file_(?:reader|writer)|xlir_io_\w+|xlir_fs_stream_\w+)\([^;]*\)\s*\{',text,re.M)
            if name=='stdlib_io_reader_only':assert 'xl6_xrt_io_writer_' not in text and 'XRT_MODULE_IO_STANDARD' not in text
            strict=out/(name+'.strict.o')
            run(name+'.strict',[cc,*flags,'-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label','-Wno-missing-field-initializers','-c',c,'-o',strict],native_env)
            run(name+'.jit',[compiler/'release'/products[0].name,'run',local],data=data)
            image=compiler/(name+suffix+'.aot');run(name+'.build',[compiler/'release'/products[0].name,'build',local,'-o',image])
            moved=relocated/image.name;shutil.copy2(image,moved)
            run(name+'.aot',[moved],directory=relocated,data=data)
            cases.append(dict(Name=name,Passed=True))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,NativeIoAcceptance=True,OriginalIoProductAcceptance=True,IndependentAotAcceptance=True)
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print('resident IO phases='+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
