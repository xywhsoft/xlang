"""Fresh-source owned Array/open index regression, including continuations.

Links every real current compiler/runtime object. Strict generated C, native
char projection, complete module OOM prefixes and pre-output IR rejection.
No released compiler or packaged AOT/shipping/full53 acceptance claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve(); assert not out.exists()
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    cc=Path(shutil.which('gcc')); driver=Path(build['Driver']); assert sha(driver)==build['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    fixtures=[root/'test/cases'/(n+'.xl') for n in ('stdlib_array_value_read','stdlib_array_value_read_continuation')]
    contract=root/'test/cases/xlir_array_value_read_contract.c'; host=root/'test/cases/xlir_module_product_oom_host.c'
    projected=root/'test/cases/stdlib_array_value_read_projection_host.c'; memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),cc,manifest,driver,product,contract,host,projected,memory,*fixtures}
    for folder in ('src','lib','release/include','test/support'): files.update((root/folder).rglob('*.h'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(); rows=[]; prefixes=[]; report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,
        Inputs=inputs,Rows=rows,ClosedModulePrefixes=prefixes,ShippingProductAcceptance=False,Full53Complete=False)
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(public/'include'),
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links=[product]+(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-Wl,-rpath,'+str(product.parent),'-pthread','-ldl','-lm'])
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Seconds=time.monotonic()-start,Complete=result.returncode==0))
        assert result.returncode==0,(name,data[-6000:].decode(errors='replace'))
        print(name+': 0',flush=True); return data.decode(errors='replace')
    try:
        binary=out/('contract'+suffix)
        run('contract.build',[cc,*flags,'-O0',contract,*objects,*links,'-o',binary])
        for i,fixture in enumerate(fixtures):
            tag=('owned','continuation')[i]; generated=out/(tag+'.c'); repeat=out/(tag+'.repeat.c'); bindings=out/(tag+'.h')
            run(tag+'.cgen',[driver,fixture,'cgen-default',generated]); run(tag+'.repeat',[driver,fixture,'cgen-default',repeat])
            before=sha(generated); assert before==sha(repeat)
            proof=run(tag+'.IR-rejections',[binary,fixture,generated,*([] if i else [bindings])])
            assert 'mutations refused before C/header output' in proof and sha(generated)==before
            for level in (0,2):
                lane=tag+'.O'+str(level); native=out/(lane+suffix); ledger=out/(lane+'.memory.o')
                run(lane+'.memory',[cc,*flags,'-O'+str(level),'-c',memory,'-o',ledger])
                run(lane+'.build',[cc,*flags,'-O'+str(level),'-include',generated,host,ledger,*objects,*links,'-o',native])
                assert 'balanced' in run(lane+'.run',[native])
                if not i:
                    proof=run(lane+'.oom-batch',[native,'--all'])
                    batch=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',proof)
                    assert batch and int(batch[1])==int(batch[2])+1 and int(batch[2])>0
                    prefixes.append(dict(Lane=lane,FirstNoHit=int(batch[2]),Runs=int(batch[1])))
                    test=out/(lane+'.native-projection'+suffix)
                    run(lane+'.projection.build',[cc,*flags,'-O'+str(level),'-include',generated,'-include',bindings,
                        projected,*objects,*links,'-o',test])
                    assert '100 physical cycles' in run(lane+'.projection.run',[test])
            run(tag+'.jit',[driver,fixture,'run',out/(tag+'.jit-result')])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Owned index regression: '+str(len(rows))+' phases, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
