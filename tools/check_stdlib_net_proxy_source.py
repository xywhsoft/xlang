"""Source-owned proxy endpoints and normal in-memory SOCKS5/HTTP negotiation.
No sockets, DNS, external proxy, protocol fuzzing, traffic or security tests.
JIT/module/AOT, strict contracts, actual pruning and caller-thread OOM prefixes.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

CASES=tuple('stdlib_net_proxy_'+name for name in ('values','socks5','authenticated','http','buffer','owned','limits','only','handshake_only'))+(
    'stdlib_net_frame_values','stdlib_signal_handle_initialization','sem_channel_typed_array_element')

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    build_path=args.source_build.resolve();build=json.loads(build_path.read_text(encoding='utf-8-sig'))
    assert not out.exists() and build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    assert all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc')).resolve();runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    memory=root/'test/support/module_product_memory.c';host=root/'test/cases/xlir_module_product_oom_host.c'
    contract=root/'test/cases/xlir_net_proxy_contract.c';artifact=root/'test/support/container_projection_artifact.c'
    files={Path(__file__).resolve(),build_path,driver,cc,runtime,memory,host,contract,artifact,root/'test/support/module_product_memory.h'}
    files.update(root/('test/cases/'+case+'.xl') for case in CASES)
    inputs={str(path):sha(path) for path in files};out.mkdir(parents=True)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    rows,products,failures,prefixes,plans=[],[],[],[],[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,SourceBuild=str(build_path),
        Rows=rows,Products=products,Failures=failures,ClosedPrefixes=prefixes,Plans=plans,NetworkTraffic=False,
        ExternalNetworkTests=False,NetworkSecurityTests=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,reject=False):
        command=list(map(str,command));invocation=command
        if command[0]==str(cc):
            response=out/(name+'.rsp');response.write_text('\n'.join('"'+part.replace('\\','/').replace('"','\\"')+'"' for part in command[1:])+'\n',encoding='utf-8')
            invocation=[str(cc),'@'+str(response)];product(response)
        log=out/(name+'.log')
        try:
            with log.open('wb') as stream:result=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        except subprocess.TimeoutExpired:
            rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=None,TimedOut=True,Log=str(log),Sha256=sha(log)));save();raise
        rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=result.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log)));save()
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode==(1 if reject else 0),(name,str(log))
        return log.read_text(encoding='utf-8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
           '-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable',
            '-Wno-unused-label','-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    libraries=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else
        ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    suffix='.exe' if os.name=='nt' else '';save()
    for case in CASES:
        try:
            source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c')
            run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated)
            code=generated.read_text(encoding='utf-8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6NetProxy\w*\([^;]*\)\s*\{',code,re.M)
            assert 'xl6_xrt_net_proxy_' not in code
            run(case+'.run',[driver,source,'run',out/(case+'.run')])
            for stage in ('typed','lowered'):
                target=out/(case+'.'+stage+'.json');run(case+'.'+stage,[driver,source,'ir-'+stage,target]);product(target)
            for opt in ('O0','O2'):
                obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*unused,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            run(case+'.module',[driver,source,'module',out/(case+'.module')])
            binary=out/(case+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
            run(case+'.aot.build',[driver,source,'exe',target]);product(binary);run(case+'.aot.run',[binary])
            native=Path(str(target)+'.temps')/'xl6_xrt_impl.c';product(native);assert '_MODULE_ALL' not in native.read_text(encoding='utf-8')
            imports=run(case+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
        except Exception as error:failures.append(dict(Name=case,Error=repr(error)));save()
    endpoint='auto p=NetProxy(NetProxyType.socks5,"127.0.0.1",1080)\n'
    handshake=endpoint+'auto h=NetProxyHandshake(p,"127.0.0.1",80)\n'
    negatives={
        'source-default':('class C\n NetProxy source\nend\nauto c=C()\n','[ir]','native resource field needs explicit initialization or nullable storage'),
        'wrong-type':('auto p=NetProxy(NetProxyAuth.none,"127.0.0.1",1080)\n','[sem]','create'),
        'wrong-auth':('auto p=NetProxy(NetProxyType.socks5,"127.0.0.1",1080,"","",NetProxyType.socks5)\n','[sem]','create'),
        'missing-constructor':('auto p=NetProxy()\n','[sem]','create'),
        'raw-constructor':('auto p=NetProxy(null)\n','[sem]','create'),
        'private-proxy':(endpoint+'auto value=p.handle\n','[sem]','private class member is not accessible'),
        'old-native-method':(endpoint+'auto value=p.nativeHandle()\n','[sem]','unknown class/union field'),
        'wrong-handshake-owner':('auto h=NetProxyHandshake(NetBuffer(),"127.0.0.1",80)\n','[sem]','create'),
        'private-handshake':(handshake+'auto value=h.handle\n','[sem]','private class member is not accessible'),
        'append-size':(handshake+'h.append("x","wrong")\n','[sem]','append'),
        'output-type':(handshake+'h.outputInto(NetBuffer())\n','[sem]','outputInto'),
        'move-type':(handshake+'h.moveRemaining(Buffer())\n','[sem]','moveRemaining'),
    }
    for name,(body,stage,reason) in negatives.items():
        try:
            source=out/('bad.'+name+'.xl');source.write_text(body,encoding='utf-8');product(source)
            target=out/('bad.'+name+'.c');log=run('bad.'+name,[driver,source,'cgen-default',target],True)
            assert stage in log and reason in log and not target.exists(),log
        except Exception as error:failures.append(dict(Name='bad.'+name,Error=repr(error)));save()
    for opt in ('O0','O2'):
        name=contract.stem+'.'+opt
        try:
            obj=out/(name+'.o');binary=out/(name+suffix)
            run(name+'.compile',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);product(obj)
            run(name+'.link',[cc,*objects,obj,*libraries,'-o',binary]);product(binary)
            assert 'Proxy XLIR:' in run(name+'.run',[binary,root/'test/cases/stdlib_net_proxy_socks5.xl'])
        except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
    try:
        planner=out/('planner'+suffix);run('planner.build',[cc,*flags,artifact,*objects,*libraries,'-o',planner]);product(planner)
        for case in ('stdlib_net_proxy_only','stdlib_net_proxy_handshake_only','stdlib_net_proxy_socks5','stdlib_net_proxy_buffer'):
            prefix=out/(case+'.selected');run(case+'.plan',[planner,root/('test/cases/'+case+'.xl'),prefix])
            plan_file=Path(str(prefix)+'.plan.json');product(plan_file);plan=json.loads(plan_file.read_text())
            assert not any('_MODULE_ALL' in name or name in ('XRT_MODULE_NET_SOCKET','XRT_MODULE_NET_ENGINE','XRT_MODULE_NET_RESOLVER','XRT_MODULE_TLS') for name in plan['NativeModules'])
            assert not any(name.endswith('/xl6_execution_source.c') for name in plan['Sources'])
            if case in ('stdlib_net_proxy_only','stdlib_net_proxy_handshake_only'):
                assert not any(name.endswith('/xl6_buffer_core.c') for name in plan['Sources'])
            if case=='stdlib_net_proxy_only':assert 'XRT_MODULE_NET_PROXY_HANDSHAKE' not in plan['NativeModules']
            plans.append(dict(Fixture=case,Sources=plan['Sources'],NativeModules=plan['NativeModules']));save()
        memory_object=out/'memory.o';run('memory.build',[cc,*flags,'-O0','-c',memory,'-o',memory_object]);product(memory_object)
        for opt in ('O0','O2'):
            binary=out/('owned.'+opt+suffix)
            run('owned.'+opt+'.build',[cc,*flags,*unused,'-'+opt,'-include',out/'stdlib_net_proxy_owned.c',host,memory_object,*objects,*libraries,'-o',binary]);product(binary)
            text=run('owned.'+opt+'.prefixes',[binary,'--all']);match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',text)
            assert match and int(match[1])==int(match[2])+1
            prefixes.append(dict(Optimization=opt,Runs=int(match[1]),FirstNoHit=int(match[2])));save()
    except Exception as error:failures.append(dict(Name='owned-prefixes',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs'])
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['Complete']=not failures and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'],failures

if __name__=='__main__':main()
