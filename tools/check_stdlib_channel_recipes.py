"""Closed Channel source schema and owning receive IR plan prerequisite.
Real source type graph, schema factories and resident private result slots.
Includes owning receive typed-to-lowered IR and migrated public Channel smoke
tests. This prerequisite is not WaitQueue or shipping SDK acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build.get('Complete',os.name=='nt') and build['SourceXrt'] and not build['RuntimeOnly'] and not out.exists()
    cc=shutil.which('gcc'); assert cc
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    files={Path(__file__).resolve(),public/'build.json',Path(cc),runtime,
        root/'test/cases/xlir_channel_recipe_contract.c',root/'test/cases/stdlib_channel_recipes.xl'}
    files.update(root/'test/cases'/name for name in
        ('xlir_channel_receive_ir_contract.c','stdlib_channel_receive_ir.xl'))
    for name in ('cgen_channel','cgen_channel_string','xlir_channel_resource_abi','sem_channel_class_element'):
        files.add(root/'test/cases'/(name+'.xl'))
    files.update(root/'test/cases'/name for name in
        ('xlir_concurrent_queue_recipe_contract.c','stdlib_concurrent_queue_recipes.xl',
        'stdlib_concurrent_queue_owned.xl','stdlib_concurrent_queue_callable_future.xl'))
    for folder in ('src','lib','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; products=[]; env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Scope=__doc__,
        PublicLanguageCutover=False,WaitQueueComplete=False,ShippingArtifact=False,Full53Acceptance=False)
    report['NativeEnvironment']={key:env[key] for key in ('XL6_SOURCE_NATIVE_ROOT','XL6_TCC_LIB') if key in env}
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(public/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[runtime,*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    def run(name,command):
        command=list(map(str,command)); execution=command
        # Normal source manifests now contain hundreds of real units. Keep
        # GCC's actual arguments intact without Windows command-line limits.
        if command[0]==cc:
            response=out/(name+'.rsp')
            response.write_text(''.join('"'+v.replace('\\','/').replace('"','\\"')+'"\n' for v in command[1:]),encoding='utf8')
            execution=[cc,'@'+str(response)]
        result=subprocess.run(execution,cwd=root,env=env,capture_output=True,timeout=600)
        data=result.stdout+result.stderr; log=out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
        assert result.returncode==0,(name,data[-8000:]); return data.decode(errors='replace')
    def product(path): products.append(dict(Path=str(path),Sha256=sha(path)))
    ext='.exe' if os.name=='nt' else ''
    # The Cgen unity test includes established private helpers. Only their
    # unrelated unused definitions are exempt; type/conversion warnings fail.
    unity=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-const-variable',
        '-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    try:
        for opt in ('O0','O2'):
            contract=out/('receive-ir-'+opt+ext)
            run(opt+'-receive-ir-build',[cc,*flags,*unity,'-'+opt,root/'test/cases/xlir_channel_receive_ir_contract.c',
                *[o for o in objects if Path(o).name!='xl6_ir.o'],*links,'-o',contract]); product(contract)
            log=run(opt+'-receive-ir-contracts',[contract,root/'test/cases/stdlib_channel_recipes.xl',
                root/'test/cases/stdlib_channel_receive_ir.xl'])
            proof=re.search(r'Channel receive IR plans=(\d+) dependent=(\d+) pipelines=(\d+) mutations=(\d+)',log)
            assert proof,log
            counts=list(map(int,proof.groups())); assert counts[0]>=39 and counts[1]==6 and counts[2]==7 and counts[3]>=1600
            report['ReceiveIR']=dict(zip(('Plans','Dependent','Pipelines','Mutations'),counts))
        for opt in ('O0','O2'):
            contract=out/('contract-'+opt+ext); generated=out/('recipes-'+opt+'.c')
            run(opt+'-contract-build',[cc,*flags,*unity,'-'+opt,root/'test/cases/xlir_channel_recipe_contract.c',
                *[o for o in objects if Path(o).name!='xl6_ir_cgen.o'],*links,'-o',contract]); product(contract)
            log=run(opt+'-contracts',[contract,root/'test/cases/stdlib_channel_recipes.xl',generated]); product(generated)
            proof=re.search(r'Channel recipes=(\d+) plans=(\d+) pending=(\d+) unsupported=(\d+) mutations=(\d+)',log)
            assert proof,log
            counts=list(map(int,proof.groups())); assert counts[0]>=18 and counts[1]==counts[0]*3 and counts[2] and counts[3]==0
            report['Proof']=dict(zip(('Recipes','Plans','Pending','Unsupported','RefusedChecks'),counts))
            text=generated.read_text(encoding='utf-8')
            assert 'xl6ChannelCreateTakeV1(' in text and 'xl6ChannelReceiveV1(' in text
            assert 'xlir_channel_' not in text and 'recvInto' not in text and 'strlen(' not in text
            binary=out/('factory-'+opt+ext)
            run(opt+'-generated-build',[cc,*flags,*unity,'-'+opt,generated,*objects,*links,'-o',binary]); product(binary)
            run(opt+'-generated-schema-owned-slots',[binary])
        # Public source fixtures use the closed owning receive API. This
        # prerequisite gate still does not replace full source/AOT acceptance.
        driver=Path(build['Driver']); files.add(driver); product(driver)
        for name in ('cgen_channel','cgen_channel_string','xlir_channel_resource_abi','sem_channel_class_element'):
            fixture=root/'test/cases'/(name+'.xl')
            run('public-regression-'+name,[driver,fixture,'run',out/name])
        for name in ('stdlib_concurrent_queue_owned','stdlib_concurrent_queue_callable_future'):
            run('queue-regression-'+name,[driver,root/'test/cases'/(name+'.xl'),'run',out/name])
        queue=out/('queue-recipe-contract'+ext); generated=out/'queue-recipes.c'
        run('queue-contract-build',[cc,*flags,*unity,'-O0',root/'test/cases/xlir_concurrent_queue_recipe_contract.c',
            *[o for o in objects if Path(o).name!='xl6_ir_cgen.o'],*links,'-o',queue]); product(queue)
        log=run('queue-contracts',[queue,root/'test/cases/stdlib_concurrent_queue_recipes.xl',generated]); product(generated)
        proof=re.search(r'Queue recipes: (\d+) complete, (\d+) rejected mutations, (\d+) pending, (\d+) descendant rejections, layouts=(\d+) families=(\d+)',log)
        assert proof,log
        counts=list(map(int,proof.groups())); assert counts[0]>=61 and counts[1]==counts[0]*16 and counts[2]>=3 and counts[3] and counts[5]==15
        report['QueueRegression']=dict(zip(('Recipes','Mutations','Pending','DescendantRejections','Layouts','Families'),counts))
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']; print('Closed Channel receive/schema prerequisite verified',flush=True)
if __name__=='__main__': main()
