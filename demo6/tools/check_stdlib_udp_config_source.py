"""Real UDP config declarations: JIT/module/AOT/IR/pruning and owned OOM.
Offline only; no Engine/socket construction, traffic or security acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

CASES=tuple('stdlib_udp_config'+name for name in ('','_only','_enum_only','_owned','_limits'))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-build',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve()
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    build_path=a.source_build.resolve();build=json.loads(build_path.read_text(encoding='utf-8-sig'))
    assert not out.exists() and build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc')).resolve();runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    memory=root/'test/support/module_product_memory.c';host=root/'test/cases/xlir_module_product_oom_host.c'
    contract=root/'test/cases/xlir_udp_config_contract.c';artifact=root/'test/support/container_projection_artifact.c'
    files={Path(__file__).resolve(),build_path,driver,cc,runtime,memory,host,contract,artifact,root/'test/support/module_product_memory.h'}
    files.update(root/('test/cases/'+case+'.xl') for case in CASES)
    inputs={str(path):sha(path) for path in files};out.mkdir(parents=True)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],Plans=[],Prefixes=[],
        SourceBuild=str(build_path),NetworkTraffic=False,ExternalNetworkTests=False,NetworkSecurityTests=False,
        ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):report['Products'].append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,reject=False):
        command=list(map(str,command));invocation=command
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8')
            product(rsp);invocation=[cc,'@'+str(rsp)]
        log=out/(name+'.log')
        with log.open('wb') as stream:r=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        report['Rows'].append(dict(Name=name,Command=command,Invocation=list(map(str,invocation)),Exit=r.returncode,
            ExpectedRejection=reject,Log=str(log),Sha256=sha(log)));save();print(name,r.returncode,flush=True)
        assert r.returncode==(1 if reject else 0),(name,str(log));return log.read_text(errors='replace')
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
            code=generated.read_text(encoding='utf8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6UdpConfig\w*\([^;]*\)\s*\{',code,re.M)
            assert 'xl6_xrt_udp_' not in code and 'static xnetengine* xl6_net_engine' not in code
            run(case+'.jit',[driver,source,'run',out/(case+'.run')])
            for stage in ('typed','lowered'):
                target=out/(case+'.'+stage+'.json');run(case+'.'+stage,[driver,source,'ir-'+stage,target]);product(target)
            for opt in ('O0','O2'):
                obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*unused,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            run(case+'.module',[driver,source,'module',out/(case+'.module')])
            binary=out/(case+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
            run(case+'.aot.build',[driver,source,'exe',target]);product(binary);run(case+'.aot.run',[binary])
            native=Path(str(target)+'.temps')/'xl6_xrt_impl.c';product(native);assert '_MODULE_ALL' not in native.read_text(encoding='utf8')
            imports=run(case+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
        except Exception as error:report['Failures'].append(dict(Name=case,Error=repr(error)));save()
    source='auto c=UdpConfig()\n'
    negatives={
        'default-field':('class C\n UdpConfig source\nend\nauto c=C()\n','[ir]','native resource field needs explicit initialization or nullable storage'),
        'raw-constructor':('auto c=UdpConfig(null)\n','[sem]','create'),
        'old-constructor':('auto c=UdpConfig("127.0.0.1",0)\n','[sem]','create'),
        'old-mutation':(source+'c.queueLimits(1,1)\n','[sem]','unknown class/union field'),
        'private':(source+'auto value=c.handle\n','[sem]','private class member is not accessible'),
        'readonly':(source+'c.receiveSize=1\n','[sem]','class property assignment requires matching let'),
        'truncation-enum':(source+'auto value=c.withTruncation(UdpOverflow.dropNewest)\n','[sem]','withTruncation'),
        'metadata-enum':(source+'auto value=c.withReceiveMetadata(NetDatagramControlFlags.none)\n','[sem]','withReceiveMetadata'),
        'pmtu-enum':(source+'auto value=c.withPathMtuMode(UdpOverflow.error)\n','[sem]','withPathMtuMode'),
        'size-type':(source+'auto value=c.withReceive("wrong",1,1)\n','[sem]','withReceive'),
    }
    for name,(body,stage,reason) in negatives.items():
        try:
            bad=out/('bad.'+name+'.xl');bad.write_text(body,encoding='utf8');product(bad)
            target=out/('bad.'+name+'.c');text=run('bad.'+name,[driver,bad,'cgen-default',target],True)
            assert stage in text and reason in text and not target.exists(),text
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    for opt in ('O0','O2'):
        try:
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+suffix)
            run('contract.'+opt+'.compile',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);product(obj)
            run('contract.'+opt+'.link',[cc,*objects,obj,*libraries,'-o',binary]);product(binary)
            assert 'UDP config XLIR:' in run('contract.'+opt+'.run',[binary,root/'test/cases/stdlib_udp_config.xl'])
        except Exception as error:report['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
    try:
        planner=out/('planner'+suffix);run('planner.build',[cc,*flags,artifact,*objects,*libraries,'-o',planner]);product(planner)
        for case in ('stdlib_udp_config_only','stdlib_udp_config_enum_only'):
            prefix=out/(case+'.selected');run(case+'.plan',[planner,root/('test/cases/'+case+'.xl'),prefix])
            path=Path(str(prefix)+'.plan.json');product(path);plan=json.loads(path.read_text());report['Plans'].append(dict(Fixture=case,**plan));save()
            assert not any(name in plan['NativeModules'] for name in ('XRT_MODULE_NET_TCP','XRT_MODULE_NET_UDP_FUTURE','XRT_MODULE_NET_RESOLVER','XRT_MODULE_TLS','XRT_MODULE_ALL','XRUNTIME_MODULE_ALL'))
            assert not any(Path(name).name.startswith('xl6_execution_') or Path(name).name=='xl6_network_service.c' for name in plan['Sources'])
            if case.endswith('enum_only'):assert 'XRT_MODULE_NET_UDP' not in plan['NativeModules']
        mem=out/'memory.o';run('memory.build',[cc,*flags,'-O0','-c',memory,'-o',mem]);product(mem)
        for opt in ('O0','O2'):
            binary=out/('owned.'+opt+suffix)
            run('owned.'+opt+'.build',[cc,*flags,*unused,'-'+opt,'-include',out/'stdlib_udp_config_owned.c',host,mem,*objects,*libraries,'-o',binary]);product(binary)
            text=run('owned.'+opt+'.prefixes',[binary,'--all']);match=re.search(r'full prefixes=(\d+) first-no-hit=(\d+) balanced',text)
            assert match and int(match[1])==int(match[2])+1;report['Prefixes'].append(dict(Optimization=opt,Runs=int(match[1]),FirstNoHit=int(match[2])));save()
    except Exception as error:report['Failures'].append(dict(Name='plans-owned',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs'])
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
    report['Complete']=not report['Failures'] and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'));save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
