"""Pinned native + actual selected resident TCP Future graph acceptance.

Only offline ownership/OOM/contract checks and ordinary self-created literal
loopback. No DNS, external endpoint, malformed traffic or security tests.
This is not complete language transport or shipping DLL/SO/VFS acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    gcc=next(cc for cc in compilers if cc.stem!='tcc')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,*[root/('test/support/'+name+'.c') for name in
        ('net_engine_plan','executor_catalog_plan','tcp_native_future_ownership','tcp_resident_future_ownership')]}
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/area).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};rows=[];products=[];failures=[]
    out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,
        Inputs=inputs,Rows=rows,Products=products,Failures=failures,Sources=[],Domains='',Modules='',NegativeControls=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        ShippingLibraryAcceptance=False,PinnedDeadlineAbi='absolute uint64 milliseconds; native int64 milliseconds/For',Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,instrumented=False,reject=False):
        command=list(map(str,command));invocation=command;env=dict(os.environ)
        if command[0] in map(str,compilers) and Path(command[0]).stem!='tcc':
            rsp=out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8')
            invocation=[command[0],'@'+str(rsp)];product(rsp)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');timeout=None
        with log.open('wb') as stream:
            try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180).returncode
            except subprocess.TimeoutExpired as error:code=None;timeout=repr(error)
        rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=code,Timeout=timeout,
            ExpectedRejection=reject,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==(1 if reject else 0),(name,str(log),timeout)
        text=log.read_text(encoding='utf8',errors='replace')
        if reject:assert '[FAIL]' in text,log
        return text
    def flags(cc,mode):
        result=[cc,'-Wall','-Wextra','-Werror',*mode,'-I'+str(root),'-I'+str(root/'release'),
                '-I'+str(root/'release/include'),'-I'+str(root/'lib/xrt'),'-I'+str(out)]
        if cc.stem!='tcc':result+=['-std=c11']
        if os.name!='nt':result+=['-D_GNU_SOURCE']
        return result
    ext='.exe' if os.name=='nt' else ''
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    save()
    try:
        for mode in ('O0','O2'):
            plan=out/('plan.'+mode+ext)
            run('plan.'+mode+'.build',flags(gcc,['-'+mode])+['-Wno-missing-field-initializers',
                '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables',
                '-fno-unwind-tables',root/'test/support/net_engine_plan.c',root/'src/compile/xl6_compile_unit.c',
                '-Wl,--gc-sections','-o',plan]);product(plan)
            run('plan.'+mode+'.run',[plan])
            if mode=='O0':
                sources=run('tcp.sources',[plan,'--sources','tcp-native']).splitlines()
                modules=run('tcp.modules',[plan,'--modules','tcp-native'])
                domains=run('tcp.domains',[plan,'--domains','tcp-native'])
                assert len(sources)==len(set(sources)) and 'XRT_MODULE_NET_TCP_FUTURE\n' in modules
                assert 'xl6OwnershipDomainNetTcpNativeV1Get(),\n' in domains
                assert not any('resolver' in source or 'task_pool' in source for source in sources)
                for leaf,text in (('net-tcp-native-modules.h',modules),('net-tcp-native-domains.h',domains)):
                    (out/leaf).write_text(text,encoding='utf8');product(out/leaf)
                report.update(Sources=sources,Modules=modules,Domains=domains);save()
        features=('ATOMIC','QUEUE','QUEUE_MPSC','MEMORY_DEBUG','TIME','WAIT','THREAD','SYNC','MUTEX','COND','CANCEL',
                  'FUTURE','NET','NET_SOCKET','NET_BUFFER','NET_PORT','NET_PORT_SELECT','NET_ENGINE','NET_TCP','NET_TCP_FUTURE')
        features+=('NET_PORT_IOCP',) if os.name=='nt' else ('NET_PORT_EPOLL',)
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined',
                '-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for mode,options in modes:
                for kind in ('native','resident'):
                    name=f'{index}.{mode}.{kind}';binary=out/(name+ext)
                    try:
                        fixture=root/('test/support/tcp_'+('native' if kind=='native' else 'resident')+'_future_ownership.c')
                        native_flags=['-DXRT_FEATURE_'+feature for feature in features] if kind=='native' else []
                        selected=[] if kind=='native' else [root/source for source in sources]
                        run(name+'.build',flags(cc,options)+native_flags+[fixture,*selected,*libs,'-o',binary]);product(binary)
                        for backend,extra in (('select',[]),('native',['native'])):
                            text=run(name+'.'+backend,[binary,*extra],mode=='instrumented')
                            assert all(word in text for word in ('graphs=24','real-external-roots=24',
                                'concurrent-final-drops=24','allocation-budgets=16 failures=6','balanced'))
                            if kind=='resident':assert 'catalog OOM prefixes=32' in text
                    except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        base_original=root/'src/runtime/xl6_ownership_domain_net_tcp_base_native.c'
        controls=(
            ('missing-stream-runtime','xrtNetStreamRuntimeOwnershipV1Get()','xrtNetListenerRuntimeOwnershipV1Get()'),
            ('missing-producer','xl6OwnershipCatalogAddProducerV1(catalog,xrtNetTcpFutureProducerPolicyV1Get())','true'),
            ('missing-cancel','xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetTcpFutureCancelPolicyV1Get())','true'),
            ('missing-accept-payload','xrtNetTcpFutureStreamPayloadPolicyV1Get()','xrtNetTcpFutureBytesPayloadPolicyV1Get()'),
            ('missing-bytes-payload','xrtNetTcpFutureBytesPayloadPolicyV1Get()','xrtNetTcpFutureStreamPayloadPolicyV1Get()'),
            ('missing-activation','*adapter=xrtNetTcpFutureOwnershipAdapterV1(ref,preparation);','(void)ref;(void)preparation;'))
        original=root/'src/runtime/xl6_ownership_domain_net_tcp_native.c';body=original.read_text(encoding='utf8')
        for name,old,new in controls:
            try:
                target=base_original if name=='missing-stream-runtime' else original
                target_body=target.read_text(encoding='utf8')
                assert target_body.count(old)==1,(name,'inexact control')
                mutant=out/(name+'.c');mutant.write_text(target_body.replace(old,new),encoding='utf8');product(mutant)
                binary=out/(name+ext);selected=[mutant if root/source==target else root/source for source in sources]
                run(name+'.build',flags(gcc,['-O0'])+['-I'+str(root/'src/runtime'),
                    root/'test/support/tcp_resident_future_ownership.c',*selected,*libs,'-o',binary]);product(binary)
                run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
    except Exception as error:failures.append(dict(Name='planning',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
    report['Complete']=not failures and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
    save();assert report['Complete'],failures


if __name__=='__main__':main()
