"""Fresh full-source strict C regression for existing collection foreach.
Actual generated programs and real runtime objects; old released CLI/DLL is not
the compiler under test. Legacy extern expectations retain their declared ABI.
Not packaged AOT/shipping/full53 or native List source-factory acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expect-support',type=Path,help='Explicit current test-only C expectations, recorded and hashed')
    a=p.parse_args(); root=a.root.resolve(); out=a.out.resolve(); public=a.public_directory.resolve()
    assert not out.exists()
    manifest=public/'build.json'; build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    if os.name!='nt': assert build['Complete'] and build['InputsUnchanged']
    cc=Path(shutil.which('gcc')); driver=Path(build['Driver']); assert sha(driver)==build['DriverSha256'].lower()
    product=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    fixtures=[('xlir_list_i64_iterator_abi',9,0,0),('xlir_xvalue_iterator_scalar_projection',3,0,2),
        ('xlir_xvalue_iterator_managed_projection',10,0,1),('xlir_erased_collection_iterator_abi',2,0,8),
        ('xlir_set_i64_iterator_abi',0,0,9),('xlir_dynamic_var_iterator_abi',0,13,0),
        ('stdlib_value_iterator_checked',1,1,4),('stdlib_value_iterator_checked_continuation',1,0,1)]
    scripts=['check_xlir_'+name+'.ps1' for name in ('xvalue_iterator_abi','xvalue_iterator_scalar_projection',
        'xvalue_iterator_managed_projection','erased_collection_iterator_abi','dynamic_var_iterator_abi')]
    host=root/'test/cases/xlir_module_product_oom_host.c'
    support=a.expect_support.resolve() if a.expect_support else root/'test/cases/stdlib_list_iterator_expect.c'
    memory=root/'test/support/module_product_memory.c'
    files={Path(__file__).resolve(),Path(sys.executable),manifest,cc,driver,product,host,support,memory}
    files.update(root/'test/cases'/(name+'.xl') for name,*_ in fixtures)
    files.update(root/'tools'/name for name in scripts)
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path']; files.add(Path(row['Path']))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(files)]
    out.mkdir(); rows=[]; report=dict(Complete=False,InputsUnchanged=False,Scope=__doc__,Inputs=inputs,Rows=rows,
        ShippingProductAcceptance=False,Full53Complete=False)
    env=dict(os.environ); suffix='.exe' if os.name=='nt' else ''
    if os.name=='nt': shutil.copy2(product,out/'xl.dll')
    else: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),
        '-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign']
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links=[product]+(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-Wl,-rpath,'+str(product.parent),'-pthread','-ldl','-lm'])
    def run(name,args,reject=False):
        args=list(map(str,args)); result=subprocess.run(args,cwd=root,env=env,capture_output=True,timeout=240)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        ok=result.returncode!=0 if reject else result.returncode==0
        rows.append(dict(Name=name,Command=args,Exit=result.returncode,Complete=ok,ExpectedRejection=reject))
        print(name+': '+str(result.returncode),flush=True)
        assert ok,(name,data[-6000:].decode(errors='replace')); return data.decode(errors='replace')
    try:
        for name,list_count,erased_count,wire_count in fixtures:
            source=root/'test/cases'/(name+'.xl'); generated=out/(name+'.c'); repeat=out/(name+'.repeat.c')
            run(name+'.cgen',[driver,source,'cgen-default',generated])
            run(name+'.repeat',[driver,source,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
            text=generated.read_text(encoding='utf-8-sig')
            assert 'xlir_xvalue_iterator_create_checked(' not in text
            for symbol,count in [('xl6ListValueIteratorCreateV1',list_count),
                ('xl6CollectionValueIteratorCreateV1',erased_count),('xlir_xvalue_iterator_create',wire_count)]:
                assert len(re.findall(r'=\s*'+symbol+r'\(v\d+\);',text))==count,(name,symbol)
            assert len(re.findall(r'=\s*xl6ArrayValueIteratorCreateCheckedV1\(v\d+\);',text))==int(
                name=='xlir_dynamic_var_iterator_abi'),name
            for level in (0,2):
                tag=name+'.O'+str(level); obj=out/(tag+'.memory.o'); binary=out/(tag+suffix)
                run(tag+'.memory',[cc,*flags,'-O'+str(level),'-c',memory,'-o',obj])
                run(tag+'.build',[cc,*flags,'-O'+str(level),'-include',generated,'-include',support,
                    host,obj,*objects,*links,'-o',binary])
                assert 'balanced' in run(tag+'.run',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); print(error,file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Iterator regression: '+str(len(rows))+' phases, complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
