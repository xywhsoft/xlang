"""Managed UDP vectors, messages, batches, controls and shared regressions: real-source JIT/module/AOT/IR/pruning.
Only normal traffic between self-owned literal loopback sockets. No DNS,
public network, security probes, or error-queue capture. No shipping claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

CASES=('stdlib_udp_message_only','stdlib_udp_transfer','stdlib_udp_controls','stdlib_udp_transfer_surface','stdlib_udp_vector_text_only','stdlib_udp_refs_only','stdlib_udp_async','stdlib_udp_async_pending','stdlib_tcp_data_vector','stdlib_tcp_send_refs')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-build',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve()
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    manifest=a.source_build.resolve();build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert not out.exists() and build.get('Complete',True) and build['SourceXrt'] and not build['RuntimeOnly']
    for row in build['Inputs']+build['Objects']+build['Headers']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower();cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    contract=root/'test/cases/xlir_udp_transfer_contract.c';artifact=root/'test/support/container_projection_artifact.c'
    files={Path(__file__).resolve(),manifest,driver,cc,runtime,contract,root/'test/cases/xlir_udp_async_contract.c',artifact,*[root/('test/cases/'+case+'.xl') for case in CASES]}
    inputs={str(path):sha(path) for path in files};out.mkdir(parents=True)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    env=dict(os.environ)
    if os.name!='nt':env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],Plans=[],SourceBuild=str(manifest),
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,ErrorQueueProbes=False,
        ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):report['Products'].append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,reject=False):
        command=list(map(str,command));invocation=command
        if command[0]==str(cc):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8');product(rsp);invocation=[cc,'@'+str(rsp)]
        log=out/(name+'.log')
        with log.open('wb') as stream:
            try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
            except subprocess.TimeoutExpired:code=None
        report['Rows'].append(dict(Name=name,Command=command,Invocation=list(map(str,invocation)),Exit=code,ExpectedRejection=reject,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==(1 if reject else 0),(name,str(log));return log.read_text(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(driver.parent/'include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    libraries=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)])]
    suffix='.exe' if os.name=='nt' else '';save()
    for case in CASES:
        try:
            source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c')
            run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated);code=generated.read_text(encoding='utf8')
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6Udp\w*\([^;]*\)\s*\{',code,re.M)
            assert 'xl6_xrt_udp_' not in code and 'static xnetengine* xl6_net_engine' not in code
            if case=='stdlib_udp_transfer_surface':
                names=re.findall(r'XL6_TEXT_API [^\n]*?\b(xl6Udp\w+V1)\(', (root/'src/runtime/xl6_udp_transfer_abi.h').read_text())
                assert len(names)==49 and all(re.search(r'^[ \t]+[^\n]*\b'+name+r'\(',code,re.M) for name in names),names
            run(case+'.jit',[driver,source,'run',out/(case+'.run')])
            for stage in ('typed','lowered'):
                target=out/(case+'.'+stage+'.json');run(case+'.'+stage,[driver,source,'ir-'+stage,target]);product(target)
            for opt in ('O0','O2'):
                obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*unused,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            run(case+'.module',[driver,source,'module',out/(case+'.module')])
            binary=out/(case+'.aot'+suffix);target=binary.with_suffix('') if os.name=='nt' else binary
            run(case+'.aot.build',[driver,source,'exe',target]);product(binary);run(case+'.aot.run',[binary])
            if case=='stdlib_udp_async_pending':
                # Short real-process teardown regression, not additional builds
                # or distinct language cases. Deterministic ticket-phase views
                # are independently checked by the native runtime fixture.
                for repetition in range(1,9):run(case+f'.aot.retire.{repetition}',[binary])
            native=Path(str(target)+'.temps')/'xl6_xrt_impl.c';product(native);assert '_MODULE_ALL' not in native.read_text(encoding='utf8')
            imports=run(case+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if os.name=='nt' else ['readelf','-d',binary])
            assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
        except Exception as error:report['Failures'].append(dict(Name=case,Error=repr(error)));save()
    negatives={
        'raw-message':'auto value=UdpMessage(null)\n',
        'default-message':'auto value=UdpMessage()\n',
        'private-message':'func bad(UdpMessage message)\n auto handle=message.handle\nend\n',
        'wrong-vector':'func bad(UdpSocket socket, array<int> values)\n socket.sendVector(values)\nend\n',
        'wrong-refs':'func bad(UdpSocket socket, array<Buffer> values)\n socket.sendRefs(values)\nend\n',
        'wrong-batch':'func bad(UdpSocket socket, array<NetBytes> values, NetBatchProgress progress)\n socket.sendBatch(values, progress)\nend\n',
        'readonly-message':'func bad(UdpMessage message)\n message.length=1\nend\n',
        'nullable-control':'func bad(UdpSocket socket)\n socket.setMulticastInterface(null)\nend\n',
    }
    for name,body in negatives.items():
        try:
            bad=out/('bad.'+name+'.xl');bad.write_text(body,encoding='utf8');product(bad);target=out/('bad.'+name+'.c')
            text=run('bad.'+name,[driver,bad,'cgen-default',target],True)
            assert '[sem]' in text or '[ir]' in text or '[checked]' in text,text
            assert not target.exists(),text
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    for opt in ('O0','O2'):
        try:
            obj=out/('contract.'+opt+'.o');binary=out/('contract.'+opt+suffix)
            run('contract.'+opt+'.compile',[cc,*flags,'-'+opt,'-c',contract,'-o',obj]);product(obj)
            run('contract.'+opt+'.link',[cc,*objects,obj,*libraries,'-o',binary]);product(binary)
            assert 'UDP transfer XLIR:' in run('contract.'+opt+'.run',[binary,root/'test/cases/stdlib_udp_transfer_surface.xl'])
        except Exception as error:report['Failures'].append(dict(Name='contract.'+opt,Error=repr(error)));save()
    try:
        planner=out/('planner'+suffix);run('planner.build',[cc,*flags,artifact,*objects,*libraries,'-o',planner]);product(planner)
        for case in CASES:
            prefix=out/(case+'.selected');run(case+'.plan',[planner,root/('test/cases/'+case+'.xl'),prefix]);path=Path(str(prefix)+'.plan.json');product(path)
            plan=json.loads(path.read_text());report['Plans'].append(dict(Fixture=case,**plan));save()
            assert not any(name in plan['NativeModules'] for name in ('XRT_MODULE_NET_RESOLVER','XRT_MODULE_TLS','XRT_MODULE_ALL','XRUNTIME_MODULE_ALL'))
            assert not any(Path(name).name in ('xl6_network_service.c','xl6_task_pool.c') for name in plan['Sources'])
            if not case.startswith('stdlib_tcp_'):assert 'XRT_MODULE_NET_TCP' not in plan['NativeModules']
            multicast='src/runtime/xl6_udp_multicast.c' in plan['Sources']
            assert multicast==(case in ('stdlib_udp_controls','stdlib_udp_transfer_surface'))
            if case=='stdlib_udp_message_only':
                assert not any(Path(name).name.startswith('xl6_execution_') for name in plan['Sources'])
                assert 'src/runtime/xl6_buffer_core.c' not in plan['Sources']
            if case in ('stdlib_udp_vector_text_only','stdlib_udp_refs_only'):
                assert not any(Path(name).name in ('xl6_udp_message.c','xl6_udp_vector_buffer.c','xl6_udp_vector_bytes.c') for name in plan['Sources'])
            if case=='stdlib_udp_controls':assert 'src/runtime/xl6_udp_future.c' not in plan['Sources']
    except Exception as error:report['Failures'].append(dict(Name='plans',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs'])
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
    report['Complete']=not report['Failures'] and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'));save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
