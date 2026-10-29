"""Current public network services in one actual mixed module owning graph.
Only own literal loopback, numeric-literal resolution and own ordinary files.
No group membership, external DNS/endpoints, error queues or security tests.
Tests active external class roots, real held Engine entries, context destruction,
finalizer driver reentry and checked failure, without native-owner rescue.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse,hashlib,json,os,shutil,subprocess,threading,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source-build','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--native-root',type=Path)
    p.add_argument('--jobs',type=int,default=2,help='Independent test processes; each has its own module, ports, files and evidence')
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();manifest=a.source_build.resolve();assert not out.exists()
    assert a.jobs>0
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    assert all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs']+build['Headers']+build['Objects'])
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();sdk=driver.parent/'include'
    source=root/'test/cases/stdlib_network_services_owned.xl';host=root/'test/support/network_services_host.c'
    # Only the host fixture uses permanent externally registered callbacks.
    # Standalone/JIT use an explicit no-XRT source package, not an undeclared
    # C sidecar (which correctly selects the full runtime by its own contract).
    standalone=root/'test/cases/stdlib_network_services_owned_aot.xl'
    package=root/'test/fixtures/network_services_noop'
    package_inputs=[package/n for n in ('xlang.package.json','anchor.xl','observer.c')]
    package_inputs.append(root/'test/support/network_services_noop.c')
    probes=[root/'test/cases'/n for n in ('package_probe_error_host.c','package_manifest_strict_host.c')]
    probe_inputs=[root/'test/cases/import_source_math.xl',*sorted((root/'test/fixtures/package_manifest_strict').glob('*.json'))]
    if os.name=='nt':runtime=root/'release/xl.dll';libraries=root/'res/tcc/win64/x86_64'
    else:
        assert a.native_root;native=a.native_root.resolve();runtime=native/'release/libxl.so';libraries=native/'build/linux/tcc-dist'
    inputs={str(f):sha(f) for f in [Path(__file__).resolve(),manifest,driver,cc,source,standalone,host,runtime,*package_inputs,*probes,*probe_inputs]}
    rows=[];products=[];failures=[];out.mkdir(parents=True);env=dict(os.environ)
    if os.name=='nt':env['PATH']=str(root/'release')+os.pathsep+env.get('PATH','')
    else:env['XL6_SOURCE_NATIVE_ROOT']=str(native)
    backends=['select','native']+(['ring'] if os.name!='nt' else [])
    report=dict(Complete=False,Inputs=inputs,SourceBuild=str(manifest),Rows=rows,Products=products,Failures=failures,
        SourceProductAcceptance=True,ShippingLibraryAcceptance=False,OwnLiteralLoopback=True,ExternalNetworkTests=False,
        NetworkSecurityTests=False,NativeOwnerRescue=False,Backends=backends,Scope=__doc__,
        CycleCarrier='actual DynamicFields self-reference; ordinary Value containers remain acyclic',IndependentProcesses=a.jobs)
    lock=threading.RLock()
    def save():
        with lock:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def product(path):
        with lock:products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def checked(name,action):
        try:action()
        except Exception as error:
            with lock:failures.append(dict(Case=name,Error=repr(error)));save()
    def batch(jobs):
        # Native resources never cross these process boundaries. Only the
        # evidence ledger is shared, and every update is serialized.
        with ThreadPoolExecutor(max_workers=a.jobs) as workers:
            list(workers.map(lambda job:checked(*job),jobs))
    def run(name,args,backend='select'):
        log=out/(name+'.log');work=out/(name+'.data');work.mkdir();started=time.monotonic()
        runenv=dict(env,XL6_NETWORK_BACKEND=backend,XL6_NETWORK_FILE=str(work/'service.bin'))
        with log.open('wb') as f:r=subprocess.run(list(map(str,args)),cwd=root,env=runenv,stdout=f,stderr=subprocess.STDOUT,timeout=240)
        with lock:
            rows.append(dict(Name=name,Backend=backend,Exit=r.returncode,Log=str(log),Sha256=sha(log),Command=list(map(str,args)),Seconds=time.monotonic()-started));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,(name,str(log));return log.read_text(encoding='utf-8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(sdk)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
    objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    assert len(objects)==len(build['Objects'])-1
    links=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent),'-Wl,--export-dynamic','-pthread','-ldl','-lm'])]
    save()
    try:
        generated=out/'services.c';run('emit',[driver,standalone,'cgen-default',generated]);product(generated)
        ir=out/'services.json';run('ir',[driver,standalone,'ir-lowered',ir]);product(ir)
        data=json.loads(ir.read_text());assert data['moduleExecutionOwnershipAbi']==1
        for opt in ['O0','O2']:
            obj=out/('services.'+opt+'.o');run('strict.'+opt,[cc,*flags,*unused,'-'+opt,'-c',generated,'-o',obj]);product(obj)
    except Exception as error:failures.append(dict(Case='emit/IR/C',Error=repr(error)));save()
    def language(backend,mode):
        assert 'Network services:' in run(backend+'.'+mode,[driver,standalone,mode,out/(backend+'.'+mode)],backend)
    batch([(backend+'.'+mode,lambda b=backend,m=mode:language(b,m)) for backend in backends for mode in ['run','module']])
    try:
        prefix=out/'services.aot';binary=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'));target=prefix if os.name=='nt' else binary
        run('aot.build',[driver,standalone,'exe',target]);product(binary)
        impl=Path(str(target)+'.temps')/'xl6_xrt_impl.c';assert impl.is_file();product(impl);assert '_MODULE_ALL' not in impl.read_text()
        def aot(backend):assert 'Network services:' in run(backend+'.aot',[binary],backend)
        batch([(backend+'.aot',lambda b=backend:aot(b)) for backend in backends])
    except Exception as error:failures.append(dict(Case='aot.build',Error=repr(error)));save()
    try:
        obj=out/'host.o';binary=out/('service-host.exe' if os.name=='nt' else 'service-host')
        run('host.build',[cc,*flags,'-O0','-c',host,'-o',obj]);product(obj)
        response=out/'host.rsp';response.write_text('\n'.join('"'+str(v).replace('\\','/')+'"' for v in [*objects,obj,*links,'-o',binary])+'\n',encoding='utf-8');product(response)
        run('host.link',[cc,'@'+str(response)]);product(binary)
        def hosted(backend,mode):
            name=backend+'.host.'+str(mode)
            log=run(name,[binary,root,source,sdk,runtime,libraries,out/(name+'.temps'),str(mode)],backend)
            assert 'Network service host:' in log and 'no rescue' in log
        batch([(backend+'.host.'+str(mode),lambda b=backend,m=mode:hosted(b,m)) for backend in backends for mode in [1,2,3]])
    except Exception as error:failures.append(dict(Case='host build/link',Error=repr(error)));save()
    # Package probing is a compile-time metadata operation, not a runtime
    # error producer. Check its exact prior-error contract and retain strict
    # explicit-manifest rejection alongside the imported language program.
    for probe in probes:
        name=probe.stem
        try:
            obj=out/(name+'.o');binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,'-O0','-c',probe,'-o',obj]);product(obj)
            response=out/(name+'.rsp')
            response.write_text('\n'.join('"'+str(v).replace('\\','/')+'"' for v in [*objects,obj,*links,'-o',binary])+'\n',encoding='utf-8');product(response)
            run(name+'.link',[cc,'@'+str(response)]);product(binary)
            run(name+'.run',[binary])
        except Exception as error:failures.append(dict(Case=name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items()) and all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs']+build['Headers']+build['Objects'])
    report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
    report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
    report['Complete']=not failures and report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged'];save();assert report['Complete'],failures
if __name__=='__main__':main()
