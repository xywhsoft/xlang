"""WaitQueue public owned receive cutover. Exact-source C/JIT/module/AOT,
closed IR mutation refusal, concurrent/async and scalar/string/bytes/char/class/
callable/Future/nested-domain cases, actual demand plan and unused-body pruning.
No shipping SDK, graph/worker fault-prefix or full53 acceptance claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc=shutil.which('gcc');assert cc and not out.exists()
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=('stdlib_wait_queue_owned','stdlib_wait_queue_values','stdlib_special_collections',
        'stdlib_special_collections_concurrent','stdlib_special_collections_async')
    pruned=root/'test/cases/stdlib_wait_queue_pruned.xl';contract=root/'test/cases/xlir_wait_queue_owned_contract.c'
    planner=root/'test/cases/xlir_artifact_runtime_contract.c'
    negatives=dict(into_retired='[SC-QUEUE-OWNED]',timeout_type='等待超时必须是整数毫秒',extra='参数数量')
    manifest,sdk,_=stage_private_sdk(root)
    pinned.update((Path(__file__).resolve(),Path(cc),runtime,pruned,contract,planner,manifest))
    pinned.update(root/('test/cases/'+n+'.xl') for n in cases)
    pinned.update(root/('test/cases/sem_wait_queue_'+n+'.xl') for n in negatives)
    pinned.update(Path(row[k]) for row in sdk for k in ('Source','Path'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(pinned)]
    out.mkdir();env=dict(os.environ);rows=[];products=[];ext='.exe' if os.name=='nt' else ''
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env.update(XL6_SOURCE_NATIVE_ROOT=build['NativeRoot'],XL6_TCC_LIB=str(Path(build['NativeRoot'])/'build/linux/tcc-dist'))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(public/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-const-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=[runtime,*systems]+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Plans=[],
        PublicWaitQueueCutover=True,ShippingArtifact=False,Full53Acceptance=False,Scope=__doc__)
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    def run(name,cmd,error=None):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=300)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,cmd)),Exit=r.returncode,ExpectedRejection=bool(error),Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 and error.encode() in data) if error else r.returncode==0,(name,data[-6000:])
        return data.decode(errors='replace')
    try:
        proof=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',contract,*objects,*links,'-o',proof]);product(proof)
        planExe=out/('planner'+ext);run('planner-build',[cc,*flags,'-O0',planner,*objects,*links,'-o',planExe]);product(planExe)
        for name in cases:
            case=root/('test/cases/'+name+'.xl');generated=out/(name+'.c')
            run(name+'-generate',[driver,case,'cgen-default',generated]);product(generated)
            text=generated.read_text();assert 'xl6WaitQueueReceiveV1' in text and 'xlir_special_queue_' not in text and 'xlir_channel_handle' not in text
            if name.startswith('stdlib_wait_queue'):run(name+'-contract',[proof,case])
            for opt in ('O0','O2'):
                obj=out/(name+'-'+opt+'.o');run(name+'-'+opt+'-strict-c',[cc,*flags,*unused,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'):run(name+'-'+mode,[driver,case,mode,out/(name+'-'+mode)])
            binary=out/(name+'.exe');run(name+'-aot-build',[driver,case,'exe',out/name if os.name=='nt' else binary]);product(binary)
            run(name+'-aot-run',[binary])
            native=out/(name+'-native.c');message=run(name+'-native-plan',[planExe,'--emit-native',case.as_posix(),native]);product(native)
            plans=[json.loads(line) for line in message.splitlines() if line.startswith('{"Fixture":')];assert len(plans)==1
            assert 'src/runtime/xl6_wait_queue.c' in plans[0]['Sources']
            assert 'src/runtime/xl6_channel.c' not in plans[0]['Sources'];report['Plans'].append(plans[0])
        for name,error in negatives.items():run('reject-'+name,[driver,root/('test/cases/sem_wait_queue_'+name+'.xl'),'cgen-default',out/name],error)
        native=out/'pruned-native.c';message=run('pruned-plan',[planExe,'--emit-native',pruned.as_posix(),native]);product(native)
        plans=[json.loads(line) for line in message.splitlines() if line.startswith('{"Fixture":')];assert len(plans)==1
        assert 'src/runtime/xl6_wait_queue.c' not in plans[0]['Sources'] and 'XRT_MODULE_CHANNEL' not in plans[0]['NativeModules']
        report['PrunedPlan']=plans[0]
        for mode in ('run','module'):run('pruned-'+mode,[driver,pruned,mode,out/('pruned-'+mode)])
        binary=out/'pruned.exe';run('pruned-aot-build',[driver,pruned,'exe',out/'pruned' if os.name=='nt' else binary]);product(binary);run('pruned-aot-run',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'];assert report['Complete']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('WaitQueue public owned cutover verified',flush=True)
if __name__=='__main__':main()
