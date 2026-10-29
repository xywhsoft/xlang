"""Pinned native Dial and the actual managed resource closure.

Offline graph/constructor checks and ordinary own literal loopback only. No
DNS, external endpoints, protocol attacks, malformed traffic or security tests.
Not a public-language, shipping library or VFS acceptance claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True)
    p.add_argument('--sanitize',action='store_true')
    args=p.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(c) or c).resolve() for c in args.compiler]
    gcc=next(c for c in compilers if c.stem!='tcc')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers}
    for area in ('src','lib/xrt','lib/xruntime','release/include','test/support'):
        files.update(f for f in (root/area).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc'))
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True)
    rows=[];products=[]
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Plans={},
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        PublicLanguageApiAccepted=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,instrumented=False):
        command=list(map(str,command));invocation=command;env=dict(os.environ)
        if command[0] in map(str,compilers) and Path(command[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+a.replace('\\','/')+'"' for a in command[1:])+'\n',encoding='utf8')
            product(rsp);invocation=[command[0],'@'+str(rsp)]
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');code=None;error=None
        with log.open('wb') as stream:
            try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180).returncode
            except subprocess.TimeoutExpired as caught:error=repr(caught)
        rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=code,Error=error,Log=str(log),Sha256=sha(log)))
        save();print(name,code,flush=True);assert code==0,(name,str(log),error)
        return log.read_text(encoding='utf8',errors='replace')
    def flags(cc,mode):
        return [cc,'-std=c11','-Wall','-Wextra','-Werror',*mode,'-I'+str(root),'-I'+str(root/'release'),
            '-I'+str(root/'release/include'),'-I'+str(out),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    ext='.exe' if os.name=='nt' else ''
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    try:
        for opt in ('O0','O2'):
            plan=out/('plan.'+opt+ext)
            run('plan.'+opt+'.build',flags(gcc,['-'+opt])+['-Wno-missing-field-initializers',
                '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables',
                '-fno-unwind-tables',root/'test/support/tcp_dial_plan.c',root/'src/compile/xl6_compile_unit.c',
                '-Wl,--gc-sections','-o',plan]);product(plan);run('plan.'+opt+'.run',[plan])
            if opt=='O0':
                sources=run('plan.sources',[plan,'--sources']).splitlines()
                assert len(sources)==len(set(sources)) and 'src/runtime/xl6_execution_tcp_dial.c' in sources
                for kind in ('modules','domains'):
                    text=run('plan.'+kind,[plan,'--'+kind]);path=out/('tcp-resource-'+kind+'.h')
                    path.write_text(text,encoding='utf8');product(path)
                    report['Plans'][kind]=text
                assert 'xl6OwnershipDomainNetDialNativeV1Get(),\n' in report['Plans']['domains']
                report['Plans']['sources']=sources;save()
        features=['NET_TCP_DIAL_FUTURE','NET_TCP_SYNC','MEMORY_DEBUG','THREAD_KEY','NET_PORT_SELECT',
                  'NET_PORT_IOCP' if os.name=='nt' else 'NET_PORT_EPOLL']
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',
                ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for opt,mode in modes:
                for kind in ('native','resource'):
                    name=f'{index}.{opt}.{kind}';binary=out/(name+ext)
                    extra=(['-I'+str(root/'lib/xrt')]+['-DXRT_MODULE_'+f for f in features]) if kind=='native' else []
                    sources=[] if kind=='native' else [root/s for s in report['Plans']['sources']]
                    run(name+'.build',flags(cc,mode)+extra+[root/('test/support/tcp_dial_'+
                        ('native_ownership' if kind=='native' else 'resource')+'.c'),*sources,*libs,'-o',binary]);product(binary)
                    for backend,argv in (('select',[]),('native',['native'])):
                        text=run(name+'.'+backend,[binary,*argv],opt=='instrumented')
                        expected=('Dial graph cases=136','ordinary=8','pending-resolver=8','budgets=96','failures=12') if kind=='native' else (
                            'graphs=24 ordinary=3 abandoned=1 deferred=1','managed Dial: graphs=32 NUL-roundtrips=8 abandoned=1 budgets=64',
                            'balanced backend=')
                        assert all(word in text for word in expected),(name,text)
        # Observe the actual sealed policy population when resolving a real
        # resource node. This is a catalog wiring control, not a substituted
        # native graph proof; the native fixture independently checks exact
        # adapter policy identities and the managed suite retires real graphs.
        original=root/'src/runtime/xl6_ownership_domain_net_dial_native.c'
        body=original.read_text(encoding='utf8')
        report['NegativeControls']=[]
        report['NegativeControlsScope']='Sealed catalog policy population; observer grants no graph capability.'
        for method,getter in (('NetRuntime','xrtNetDialRuntimeOwnershipV1Get'),
                ('Producer','xrtNetDialFutureProducerPolicyV1Get'),
                ('CancelWatch','xrtNetDialFutureCancelPolicyV1Get'),
                ('DialDone','xrtNetDialFutureDialPolicyV1Get')):
            needle=f'xl6OwnershipCatalogAdd{method}V1(catalog,{getter}())'
            assert body.count(needle)==1
            name='missing-'+method.lower();mutant=out/(name+'.c')
            mutant.write_text(body.replace(needle,'true'),encoding='utf8');product(mutant)
            selected=[mutant if root/s==original else root/s for s in report['Plans']['sources']]
            binary=out/(name+ext)
            run(name+'.build',flags(gcc,['-O0'])+['-I'+str(root/'src/runtime'),
                root/'test/support/tcp_dial_resource.c',*selected,*libs,'-o',binary]);product(binary)
            log=out/(name+'.refused.log')
            with log.open('wb') as stream:
                code=subprocess.run([str(binary)],cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=30).returncode
            text=log.read_text(encoding='utf8',errors='replace')
            rows.append(dict(Name=name+'.refused',Exit=code,ExpectedRejection=True,Log=str(log),Sha256=sha(log)))
            assert code!=0 and 'line ' in text,(name,code,text)
            report['NegativeControls'].append(name);save();print(name,'rejected',flush=True)
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['ProductsUnchanged']=all(sha(item['Path'])==item['Sha256'] for item in products)
        report['LogsUnchanged']=all(sha(item['Log'])==item['Sha256'] for item in rows)
        report['Complete']=all(report[k] for k in ('InputsUnchanged','ProductsUnchanged','LogsUnchanged'))
        save();assert report['Complete']
    except Exception as error:report['Failure']=repr(error);save();raise

if __name__=='__main__':main()
