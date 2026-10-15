"""Exact 40-owner XLIR rejection/pruning and relocated native AOT finalizers.

Uses the complete selected source build and ordinary shipping CLI embedded VFS.
Fixtures and assertions may follow that frozen production build. No production
provider is replaced. Process-backed IO and the task-group protocol remain outstanding.
"""
from pathlib import Path
import argparse,json,os,shutil,subprocess,sys,time
from xlir_public_build_inputs import sha,verify_public_build
import stdlib_vfs_fixture

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve();assert not out.exists()
    build,pinned,compared=verify_public_build(root,public);cc=shutil.which('gcc');assert cc
    windows=os.name=='nt';suffix='.exe' if windows else '';task=Path(__file__).resolve().parents[1]
    ir=task/'test/support/native_resources_ir.c'
    owners=[task/'test/cases'/(name+'.xl') for name in ('stdlib_native_resource_owner_aot','stdlib_native_resource_process_owner_aot','stdlib_native_resource_scalar_owner_aot','stdlib_native_resource_vfs_owner_aot','stdlib_native_resource_io_owner_aot')]
    originals=[root/'test/cases'/(name+'.xl') for name in ('stdlib_pattern_builder_lifetime','stdlib_regex_replace_func','stdlib_regex_iterators',
        'stdlib_regex_split_only','stdlib_regex_set_only','stdlib_signal_class_surface','stdlib_time_datetime','stdlib_unicode_validator',
        'stdlib_vfs_memory','stdlib_vfs_disk','stdlib_vfs_pack','stdlib_vfs_lifetime','stdlib_vfs_minimal','stdlib_vfs_limits','stdlib_vfs_overlay',
        'stdlib_vfs_pack_limits','stdlib_vfs_pack_allocation')]
    fixtures=[owners[0],owners[2],owners[3],owners[4],root/'test/cases/stdlib_process_pipeline_bytes.xl',root/'test/cases/stdlib_signal_class_surface.xl',root/'test/cases/cgen_basic.xl']
    shipping=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert shipping['Complete'] and shipping['InputsUnchanged']
    products=[root/'release'/name for name in (('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in shipping['Products']};assert all(recorded[f]==sha(f) for f in products)
    for row in shipping['Inputs']+shipping['Products']:assert sha(row['Path'])==row['Sha256'];pinned.add(Path(row['Path']))
    runtime=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    child_source=root/'test/cases/xlir_process_child.c'
    pinned.update((Path(__file__).resolve(),Path(stdlib_vfs_fixture.__file__).resolve(),Path(sys.executable),Path(cc),a.build_report.resolve(),ir,runtime,child_source,*fixtures,*owners,*originals))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)];out.mkdir(parents=True)
    compiler=out/'compiler';(compiler/'release').mkdir(parents=True);relocated=out/'relocated';relocated.mkdir();tmp=out/'tmp';tmp.mkdir()
    for product in products:
        shutil.copy2(product,compiler/'release'/product.name)
        if windows:shutil.copy2(product,compiler/product.name)
    assert not any((compiler/f).exists() for f in ('src','lib','res','include'))
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(TMPDIR=str(tmp),TEMP=str(tmp),TMP=str(tmp));source_env=dict(env)
    if not windows:source_env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    if windows:shutil.copy2(runtime,out/'xl.dll')
    rows=[];cases=[];report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,ComparedSources=compared,Rows=rows,Cases=cases,
        ProviderOverrides=False,EmbeddedVfsWithoutFallback=True,IndependentAotAcceptance=False,IrPolicyAcceptance=False,Full53Acceptance=False)
    def run(name,command,environment=env,directory=compiler):
        start=time.monotonic();command=list(map(str,command))
        result=subprocess.run(command,cwd=directory,env=environment,capture_output=True,timeout=300);data=result.stdout+result.stderr
        log=out/(name+'.log');log.write_bytes(data);rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace'));return data
    try:
        fixtureRoot,fixturePack=stdlib_vfs_fixture.create(relocated/'build/test')
        archive=fixturePack.read_bytes();damaged=bytearray(archive);damaged[80]^=1
        (fixtureRoot/'vfs-pack-corrupt.bin').write_bytes(damaged)
        (fixtureRoot/'vfs-pack-slice.bin').write_bytes(b'prefix!'+archive+b'tail')
        env.update(XL6_VFS_FIXTURE_ROOT=str(fixtureRoot),XL6_VFS_FIXTURE_PACK=str(fixturePack))
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(public/'include')]
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        if not windows:flags+=['-D_GNU_SOURCE']
        for opt in ('O0','O2'):
            binary=out/('ir.'+opt+suffix);run(opt+'.ir.build',[cc,*flags,'-'+opt,ir,*objects,runtime,*systems,'-o',binary],source_env)
            data=run(opt+'.ir',[binary,*fixtures],source_env);assert b'targets=40 fields=40 rejected=160' in data
        report['IrPolicyAcceptance']=True
        child=relocated/('process-child'+suffix);run('child.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2',*(['-municode'] if windows else ['-D_GNU_SOURCE']),child_source,'-o',child])
        env['XL6_PROCESS_CHILD']=str(child)
        for index,fixture in enumerate(owners+originals):
            local=compiler/(fixture.stem+'.xl');shutil.copy2(fixture,local);image=compiler/(fixture.stem+suffix+'.aot')
            run(fixture.stem+'.build',[compiler/'release'/products[0].name,'build',local,'-o',image])
            image2=relocated/image.name;shutil.copy2(image,image2)
            marker=([b'native-owner-finalized',b'native-process-owner-finalized',b'native-scalar-owner-finalized',b'native-vfs-owner-finalized',b'native-io-owner-finalized'][index] if index<5 else None)
            for trial in range(4 if marker else 1):
                data=run(fixture.stem+'.run'+str(trial),[image2],directory=relocated)
                if marker:assert data.count(marker)==1,(fixture.stem,data)
            cases.append(dict(Name=fixture.stem,FinalizerMarker=marker.decode() if marker else None,Trials=4 if marker else 1,Passed=True))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,IndependentAotAcceptance=True)
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print('resource product phases='+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
