"""Current-source managed TCP JIT/module/pruned AOT and sealed IR contracts.

Only valid ordinary communication over own literal loopback. No DNS, external
endpoints, malformed traffic or security tests. Does not certify a fresh
shipping DLL/SO/VFS or the remaining aggregate server/proxy/file/UDP/TLS work.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-build',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--native-root',type=Path)
    parser.add_argument('--dial',action='store_true',help='Include the public Dial overloads and private-entry rejection')
    parser.add_argument('--dial-interrupt',action='store_true',help='Include public parent/deadline Dial overloads and checked service contracts')
    parser.add_argument('--dial-sync-controls',action='store_true',help='Include synchronous parent/deadline overloads without a language scheduler')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    if args.dial_sync_controls:args.dial_interrupt=True
    if args.dial_interrupt:args.dial=True
    manifest=args.source_build.resolve();build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    assert not out.exists() and build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler']==1
    assert all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs']+build['Objects']+build['Headers'])
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();sdk=driver.parent/'include'
    cases=tuple('stdlib_tcp_managed_'+name for name in ('sync','async','cancellation','parent','enums'))
    negatives=tuple('bad_stdlib_tcp_managed_'+name for name in ('raw','default','enum','bytes','handle','snapshot'))
    if args.dial:
        cases+=tuple('stdlib_tcp_dial_'+name for name in ('sync','async','host_validation'))
        cases+=('xlir_continuation_unused','xlir_continuation_used')
        negatives+=tuple('bad_stdlib_tcp_dial_'+name for name in ('private','resolver','addresses'))
    if args.dial_interrupt:
        cases+=('stdlib_tcp_dial_interrupt',)
        negatives+=tuple('bad_stdlib_tcp_dial_interrupt_'+name for name in ('private','token','deadline'))
    if args.dial_sync_controls:
        cases+=('stdlib_tcp_dial_controls_sync',)
        negatives+=tuple('bad_stdlib_tcp_dial_sync_'+name for name in ('token','deadline'))
    contract=root/'test/cases/xlir_tcp_managed_contract.c'
    inputs={str(path):sha(path) for path in (Path(__file__).resolve(),manifest,driver,cc,contract,
        *[root/('test/cases/'+name+'.xl') for name in cases+negatives])}
    out.mkdir(parents=True);rows=[];products=[];failures=[];env=dict(os.environ)
    if os.name!='nt':assert args.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(args.native_root.resolve())
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        SourceBuild=str(manifest),SourceProductAcceptance=True,ShippingLibraryAcceptance=False,PublicDial=args.dial,
        PublicDialInterruption=args.dial_interrupt,PublicDialSyncControls=args.dial_sync_controls,
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,reject=False):
        command=list(map(str,command));log=out/(name+'.log');started=time.monotonic()
        with log.open('wb') as stream:
            result=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,ExpectedRejection=reject,
            Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name,result.returncode,flush=True)
        assert result.returncode==(1 if reject else 0),(name,str(log))
        return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(sdk)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable',
        '-Wno-unused-label','-Wno-missing-field-initializers']
    save()
    try:
        for case in cases:
            source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c');ir=out/(case+'.json')
            run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated)
            run(case+'.ir',[driver,source,'ir-lowered',ir]);product(ir)
            data=json.loads(ir.read_text());transport=case.startswith('stdlib_tcp_') and not case.endswith('enums')
            asynchronous=not case.endswith(('_sync','_enums','_unused'))
            assert data['moduleExecutionOwnershipAbi']==int(transport)
            assert data['moduleContinuationAbi']==(2 if asynchronous else 0)
            assert data['moduleTaskPoolAbi']=='none'
            text=generated.read_text()
            if args.dial_interrupt:
                assert bool(data['moduleDeadlineOwnershipAbi'])==(case=='stdlib_tcp_dial_interrupt')
                if case=='stdlib_tcp_dial_interrupt':
                    required=('xl6TcpDial'+part+'Start'+kind+default+'V1('
                        for part in ('','Addresses') for kind in ('Cancel','Until','UntilCancel') for default in ('','Default'))
                    assert all(name in text for name in required)
                    assert 'xlir_deadline_source' in text and 'xrtCancelWatch(' not in text
                else:assert 'xlir_deadline_source' not in text
            if args.dial_sync_controls and case=='stdlib_tcp_dial_controls_sync':
                required=('xl6TcpDial'+part+kind+default+'V1('
                    for part in ('','Addresses') for kind in ('Cancel','Until','UntilCancel') for default in ('','Default'))
                assert all(name in text for name in required) and 'xlir_continuation_executor' not in text
            assert ('static XL6_ExecutionSourceV1* xlir_execution_source;' in text)==transport
            assert not any(symbol in text for symbol in ('xl6NetworkService','xl6_xrt_tcp','xlir_async_pool',
                'xrtNetStreamRecvAsync(','xrtNetListenerAcceptAsync(','xrtNetEngineCreate('))
            if transport:assert 'xl6Tcp' in text
            for opt in ('O0','O2'):
                obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'):run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
            prefix=out/(case+'.aot');exe=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'));argument=prefix if os.name=='nt' else exe
            run(case+'.aot.build',[driver,source,'exe',argument]);product(exe);run(case+'.aot.run',[exe])
            native=Path(str(argument)+'.temps')/'xl6_xrt_impl.c';assert native.is_file();nativeText=native.read_text();product(native)
            assert '_MODULE_ALL' not in nativeText
            for module in ('NET_RESOLVER','NET_DNS','TLS','TASK_POOL','COROUTINE_SCHEDULER'):
                if case.startswith('stdlib_tcp_dial_') and module in ('NET_RESOLVER','NET_DNS'):continue
                assert '#define XRT_MODULE_'+module+'\n' not in nativeText,module
            if not transport:assert '#define XRT_MODULE_NET_TCP\n' not in nativeText
            # XRT's blocking TCP methods themselves reuse native Futures.
            # A synchronous language program must not acquire the language
            # ready-queue executor just because that native dependency exists.
            if not asynchronous:assert 'xlir_continuation_executor' not in text
            imports=run(case+'.imports',(['E:/software/w64devkit/bin/objdump.exe','-p',exe] if os.name=='nt' else ['readelf','-d',exe]))
            assert not any(name in imports.lower() for name in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
        reasons=('create','no verified initializer ABI',"no matching callable class member 'TcpStream.wait'",
            'target string, value bytes','unknown class/union field','verified zero-argument layout ABI')
        # Global overload diagnostics are localized (including native Windows
        # code-page output). Require the semantic boundary and NO generated
        # artifact, not an assumed English rendering of that diagnostic.
        if args.dial:reasons+=('private','[sem]','[sem]')
        if args.dial_interrupt:reasons+=('private','[sem]','[sem]')
        if args.dial_sync_controls:reasons+=('[sem]','[sem]')
        for case,reason in zip(negatives,reasons):
            output=out/(case+'.c');diagnostic=run(case+'.refused',[driver,root/('test/cases/'+case+'.xl'),'cgen-default',output],True)
            assert reason in diagnostic and not output.exists(),(case,diagnostic)
        objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
            ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1
        if args.dial:
            requirements=root/'test/cases/xlir_continuation_requirements.c'
            inputs[str(requirements)]=sha(requirements)
            binary=out/('continuation-requirements'+('.exe' if os.name=='nt' else ''))
            # Link the pure classifier against the actual current compiler
            # object, independently of the language-driver callback machinery.
            obj=out/'continuation-requirements.o'
            run('continuation-requirements.build',[cc,*flags,'-Wno-missing-field-initializers','-O0','-c',requirements,'-o',obj]);product(obj)
            links=[*objects,obj]
            if os.name=='nt':links+=[root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
            else:
                lib=args.native_root.resolve()/'release';links+=['-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
            links+=['-o',binary];rsp=out/'continuation-requirements.rsp'
            rsp.write_text('\n'.join('"'+str(arg).replace('\\','/')+'"' for arg in links)+'\n',encoding='utf8');product(rsp)
            run('continuation-requirements.link',[cc,'@'+str(rsp)]);product(binary)
            if os.name=='nt' and not (out/'xl.dll').exists():shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
            run('continuation-requirements.run',[binary])
        for opt in ('O0','O2'):
            obj=out/('contract.'+opt+'.o');run('contract.'+opt+'.build',[cc,*flags,'-Wno-missing-field-initializers','-'+opt,'-c',contract,'-o',obj]);product(obj)
            binary=out/('contract.'+opt+('.exe' if os.name=='nt' else ''))
            if os.name=='nt':link=[*objects,obj,root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',binary]
            else:
                lib=args.native_root.resolve()/'release';link=[*objects,obj,'-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',binary]
            response=out/('contract.'+opt+'.rsp');response.write_text('\n'.join('"'+str(arg).replace('\\','/')+'"' for arg in link)+'\n',encoding='utf8');product(response)
            run('contract.'+opt+'.link',[cc,'@'+str(response)]);product(binary)
            if os.name=='nt' and not (out/'xl.dll').exists():shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
            family='stdlib_tcp_dial_' if args.dial else 'stdlib_tcp_managed_'
            proof=run('contract.'+opt+'.run',[binary,root/('test/cases/'+family+('controls_sync' if args.dial_sync_controls else 'sync')+'.xl'),
                root/('test/cases/'+family+('interrupt' if args.dial_interrupt else 'async')+'.xl')])
            assert 'TCP XLIR:' in proof and 'before C/header output' in proof
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items()) and all(sha(row['Path'])==row['Sha256'].lower() for row in build['Inputs'])
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
        assert all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
        report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise

if __name__=='__main__':main()
