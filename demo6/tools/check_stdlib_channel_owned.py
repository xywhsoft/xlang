"""Public Channel (status,T?) source/native cutover. Not WaitQueue or shipping-SDK acceptance."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(); root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build.get('Complete',os.name=='nt') and build['SourceXrt'] and not build['RuntimeOnly'] and not out.exists()
    cc=shutil.which('gcc'); assert cc
    driver=Path(build['Driver']); runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=('stdlib_channel_owned','stdlib_channel_owned_wait')
    negative=('retired','extra','timeout_named','timeout_type')
    files={Path(__file__).resolve(),public/'build.json',Path(cc),runtime,driver}
    files.update(root/'test/cases'/(name+'.xl') for name in cases)
    files.update(root/'test/cases'/('sem_channel_owned_'+name+'.xl') for name in negative)
    files.update(root/'test/cases'/name for name in ('xlir_channel_receive_ir_contract.c',
        'xlir_channel_public_plan.c','stdlib_channel_pruned.xl',
        'stdlib_channel_recipes.xl','stdlib_channel_receive_ir.xl','stdlib_concurrent_queue_owned.xl',
        'stdlib_concurrent_queue_callable_future.xl'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    assert sha(driver)==build['DriverSha256'].lower()
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir(); rows=[]; products=[]; env=dict(os.environ)
    if os.name=='nt': shutil.copy2(runtime,out/'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        PublicChannelCutover=True,WaitQueueComplete=False,ShippingArtifact=False,Full53Acceptance=False,Scope=__doc__)
    def run(name,command,success=True):
        command=list(map(str,command)); result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=180)
        log=out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,ExpectedSuccess=success,Log=str(log),LogSha256=sha(log)))
        assert (result.returncode==0)==success,(name,log.read_bytes()[-8000:])
        return log.read_text(encoding='utf-8',errors='replace')
    def product(path): products.append(dict(Path=str(path),Sha256=sha(path)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(public/'include')]
    if os.name!='nt': flags+=['-D_GNU_SOURCE']
    unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-const-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    links=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    try:
        for name in cases:
            source=root/'test/cases'/(name+'.xl')
            for mode in ('run','module','ir-lowered'):
                output=out/(name+'-'+mode+('.json' if mode=='ir-lowered' else ''))
                run(name+'-'+mode,[driver,source,mode,output])
                if mode=='ir-lowered':
                    product(output); ir=json.loads(output.read_text(encoding='utf-8-sig'))
                    receives=[i for i in ir['instructions'] if i['runtimeOpName'].startswith('channel-owned-')]
                    assert receives and all(i['operandCount'] in (1,2) for i in receives)
                    report[name+'ReceiveCount']=len(receives)
            generated=out/(name+'.c'); run(name+'-cgen',[driver,source,'cgen-default',generated]); product(generated)
            text=generated.read_text(encoding='utf-8-sig')
            assert 'xl6ChannelReceiveV1(' in text and 'xl6ChannelCreateTakeV1(' in text
            assert 'xlir_channel_' not in text and 'recvInto' not in text and 'strlen(v' not in text
            assert 'xl6_channel_abi.h' in text and 'xl6_channel.h' not in text
            for opt in ('O0','O2'): run(name+'-gcc-'+opt,[cc,*flags,*unused,'-'+opt,'-fsyntax-only',generated])
            executable=out/(name+'-aot'+('.exe' if os.name=='nt' else ''))
            output=executable.with_suffix('') if os.name=='nt' else executable
            run(name+'-aot-build',[driver,source,'exe',output]); product(executable)
            run(name+'-aot-run',[executable])
        for name in negative:
            output=out/('negative-'+name+'.c')
            text=run('negative-'+name,[driver,root/'test/cases'/('sem_channel_owned_'+name+'.xl'),'cgen-default',output],False)
            assert 'channel' in text.lower() and not output.exists()
        for opt in ('O0','O2'):
            contract=out/('receive-ir-'+opt+('.exe' if os.name=='nt' else ''))
            run(opt+'-receive-ir-build',[cc,*flags,*unused,'-'+opt,root/'test/cases/xlir_channel_receive_ir_contract.c',
                *[o for o in objects if Path(o).name!='xl6_ir.o'],*links,'-o',contract]); product(contract)
            text=run(opt+'-receive-ir-contract',[contract,root/'test/cases/stdlib_channel_recipes.xl',
                root/'test/cases/stdlib_channel_receive_ir.xl'])
            assert 'plans=39 dependent=6 pipelines=7 mutations=1695' in text
        for name in ('stdlib_concurrent_queue_owned','stdlib_concurrent_queue_callable_future'):
            run(name+'-regression',[driver,root/'test/cases'/(name+'.xl'),'run',out/name])
        plan=out/('public-plan'+('.exe' if os.name=='nt' else ''))
        run('public-plan-build',[cc,*flags,'-O2',root/'test/cases/xlir_channel_public_plan.c',*objects,*links,'-o',plan]); product(plan)
        for name in (*cases,'stdlib_channel_pruned'):
            run(name+'-native-plan',[plan,root/'test/cases'/(name+'.xl'),'pruned' if name.endswith('pruned') else 'required'])
        source=root/'test/cases/stdlib_channel_pruned.xl'; generated=out/'pruned.c'
        run('pruned-cgen',[driver,source,'cgen-default',generated]); product(generated)
        text=generated.read_text(encoding='utf-8-sig'); assert 'xl6Channel' not in text and 'xl6_channel' not in text
        run('pruned-run',[driver,source,'run',out/'pruned'])
        executable=out/('pruned-aot'+('.exe' if os.name=='nt' else ''))
        run('pruned-aot-build',[driver,source,'exe',executable.with_suffix('') if os.name=='nt' else executable]); product(executable)
        run('pruned-aot-run',[executable])
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']; print('Public Channel owned receive cutover verified',flush=True)

if __name__=='__main__': main()
