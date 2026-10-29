"""Current-source NetFile JIT/module/standalone AOT and sealed IR acceptance.
Self-owned ordinary files on actual IOCP/io_uring; two private-handle lifetime
regressions use own literal loopback without application payload. No external
endpoints, security tests or shipping-library acceptance.
All independent language cases run before the batch is reported as failed.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source-build','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--native-root',type=Path)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();manifest=a.source_build.resolve()
    assert not out.exists();build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    assert build.get('SourceXrt') and not build.get('RuntimeOnly') and build['EnableExeCompiler']==1
    assert all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs']+build['Objects']+build['Headers'])
    driver=Path(build['Driver']);assert sha(driver)==build['DriverSha256'].lower()
    cc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();sdk=driver.parent/'include'
    cases=tuple('stdlib_net_file'+n for n in ('','_errors','_unobserved','_unsupported'))
    regressions=('stdlib_net_task','stdlib_net_task_unobserved','stdlib_tcp_unobserved_accept','stdlib_tcp_unobserved_receive_into')
    negatives=tuple('bad_stdlib_net_file_'+n for n in ('engine','token','size','result'))
    contract=root/'test/cases/xlir_net_file_contract.c'
    files={Path(__file__).resolve(),manifest,cc,driver,contract,*[root/('test/cases/'+n+'.xl') for n in cases+regressions+negatives]}
    inputs={str(f):sha(f) for f in sorted(files)};rows=[];products=[];failures=[];out.mkdir(parents=True);env=dict(os.environ)
    if os.name!='nt':assert a.native_root;env['XL6_SOURCE_NATIVE_ROOT']=str(a.native_root.resolve())
    else:env['PATH']=str(root/'release')+os.pathsep+env.get('PATH','')
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        SourceBuild=str(manifest),SourceBuildSha256=sha(manifest),SourceProductAcceptance=True,
        ShippingLibraryAcceptance=False,ApplicationNetworkTraffic=False,OwnLiteralLoopbackRegression=True,ExternalNetworkTests=False,NetworkSecurityTests=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,args,reject=False):
        log=out/(name+'.log');cwd=out/(name+'.cwd');cwd.mkdir();(cwd/'build/test').mkdir(parents=True)
        runenv=dict(env,XL6_NET_FILE_PATH=str(cwd/'unsupported.bin'))
        if name.startswith('stdlib_net_file_unsupported.'):(cwd/'unsupported.bin').write_bytes(b'keep')
        started=time.monotonic()
        # Public source drivers deliberately resolve their compiler root from
        # cwd. Program file data is independently isolated via the input path;
        # moving cwd would silently select obsolete SDK/VFS fallback assets.
        with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,env=runenv,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name,r.returncode,flush=True);assert r.returncode==(1 if reject else 0),(name,str(log))
        if name.startswith('stdlib_net_file_unsupported.'):
            assert (cwd/'unsupported.bin').read_bytes()==b'keep',name
        return log.read_text(encoding='utf-8',errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(sdk)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-unused-label','-Wno-missing-field-initializers']
    def language(case):
        source=root/('test/cases/'+case+'.xl');generated=out/(case+'.c')
        run(case+'.emit',[driver,source,'cgen-default',generated]);product(generated)
        text=generated.read_text(encoding='utf-8')
        if case in cases:
            ir=out/(case+'.json');run(case+'.ir',[driver,source,'ir-lowered',ir]);product(ir);data=json.loads(ir.read_text())
            assert data['moduleExecutionOwnershipAbi']==1 and data['moduleTaskPoolAbi']=='none'
            assert 'xl6NetFileCreateV1(' in text and 'xlir_net_file_' not in text
            if case.endswith('_unsupported'):assert data['moduleContinuationAbi']==0
            for opt in ('O0','O2'):
                obj=out/(case+'.'+opt+'.o');run(case+'.'+opt,[cc,*flags,*compat,'-'+opt,'-c',generated,'-o',obj]);product(obj)
        for mode in ('run','module'):
            log=run(case+'.'+mode,[driver,source,mode,out/(case+'.'+mode)])
            if case in cases:assert 'NetFile:' in log
        prefix=out/(case+'.aot');exe=Path(str(prefix)+('.exe' if os.name=='nt' else '.out'));arg=prefix if os.name=='nt' else exe
        run(case+'.aot.build',[driver,source,'exe',arg]);product(exe);run(case+'.aot.run',[exe])
        if case in cases:
            native=Path(str(arg)+'.temps')/'xl6_xrt_impl.c';assert native.is_file();product(native);body=native.read_text()
            assert '_MODULE_ALL' not in body and '#define XRT_MODULE_NET_FILE_FUTURE\n' in body
            for forbidden in ('TASK_POOL','EXECUTOR','NET_TCP','NET_UDP'):
                assert '#define XRT_MODULE_'+forbidden+'\n' not in body
    def abi():
        obj=out/'contract.o';run('contract.build',[cc,*flags,'-O0','-c',contract,'-o',obj]);product(obj)
        objects=[r['Path'] for r in build['Objects'] if Path(r['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
        assert len(objects)==len(build['Objects'])-1;binary=out/('contract.exe' if os.name=='nt' else 'contract')
        if os.name=='nt':links=[root/'release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']
        else:
            lib=a.native_root.resolve()/'release';links=['-L'+str(lib),'-lxl','-Wl,-rpath,'+str(lib),'-Wl,--export-dynamic','-pthread','-ldl','-lm']
        response=out/'contract.rsp';response.write_text('\n'.join('"'+str(v).replace('\\','/')+'"' for v in [*objects,obj,*links,'-o',binary])+'\n',encoding='utf-8');product(response)
        run('contract.link',[cc,'@'+str(response)]);product(binary)
        if os.name=='nt':shutil.copy2(root/'release/xl.dll',out/'xl.dll');product(out/'xl.dll')
        assert 'NetFile XLIR:' in run('contract.run',[binary,root/'test/cases/stdlib_net_file.xl'])
    save()
    for case in cases+regressions:
        try:language(case)
        except BaseException as error:failures.append(dict(Case=case,Error=repr(error)));save();print('FAILED',case,flush=True)
    for case in negatives:
        try:
            generated=out/(case+'.c');run(case+'.refused',[driver,root/('test/cases/'+case+'.xl'),'cgen-default',generated],True);assert not generated.exists()
        except BaseException as error:failures.append(dict(Case=case,Error=repr(error)));save()
    try:abi()
    except BaseException as error:failures.append(dict(Case='IR contract',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(f)==h for f,h in inputs.items()) and all(sha(r['Path'])==r['Sha256'].lower() for r in build['Inputs'])
    report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in rows)
    report['ProductsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in products)
    report['Complete']=not failures and report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged'];save()
    assert report['Complete'],failures
if __name__=='__main__':main()
