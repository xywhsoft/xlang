"""Pinned XRT/xruntime Server ABI and actual selected resident graph acceptance.

Offline ownership/ABI/OOM and ordinary self-created literal loopback only.
No DNS, external endpoints, malformed protocols or network-security tests.
Not public TcpServer language or shipping DLL/SO/VFS acceptance.
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
        ('net_engine_plan','executor_catalog_plan','tcp_server_native_ownership','tcp_server_resident_ownership','tcp_server_resident_core')]}
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/area).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};rows=[];products=[];failures=[]
    out.mkdir(parents=True)
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,
        Inputs=inputs,Rows=rows,Products=products,Failures=failures,Plans={},NegativeControls=[],Pruning=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        ShippingLibraryAcceptance=False,PublicLanguageAcceptance=False,
        PinnedDeadlineAbi='uint64 microseconds/xdeadline',Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,instrumented=False,reject=False):
        command=list(map(str,command));invocation=command;env=dict(os.environ)
        if command[0] in map(str,compilers) and Path(command[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8')
            invocation=[command[0],'@'+str(rsp)];product(rsp)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');timeout=None
        with log.open('wb') as stream:
            try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
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
    nm=gcc.with_name('nm'+ext);assert nm.is_file()
    save()
    try:
        for mode in ('O0','O2'):
            plan=out/('plan.'+mode+ext)
            run('plan.'+mode+'.build',flags(gcc,['-'+mode])+['-Wno-missing-field-initializers',
                '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables',
                '-fno-unwind-tables',root/'test/support/net_engine_plan.c',root/'src/compile/xl6_compile_unit.c',
                '-Wl,--gc-sections','-o',plan]);product(plan);run('plan.'+mode+'.run',[plan])
            if mode=='O0':
                for choice in ('server-native','server-all-native','server-future-native'):
                    sources=run(choice+'.sources',[plan,'--sources',choice]).splitlines()
                    modules=run(choice+'.modules',[plan,'--modules',choice])
                    domains=run(choice+'.domains',[plan,'--domains',choice])
                    assert len(sources)==len(set(sources)) and 'xl6OwnershipDomainNetServerNativeV1Get(),\n' in domains
                    assert not any('resolver' in source or 'task_pool' in source for source in sources)
                    assert '#define XRT_MODULE_NET_TCP_SERVER\n' in modules
                    if choice=='server-native':
                        assert 'XRT_MODULE_NET_TCP_SERVER_FUTURE\n' not in modules and 'XRT_MODULE_NET_TCP_FUTURE\n' not in modules
                        assert 'xl6OwnershipDomainNativeFutureV1Get(),\n' not in domains
                    for suffix,text in (('modules',modules),('domains',domains)):
                        path=out/('net-'+choice+'-'+suffix+'.h');path.write_text(text,encoding='utf8');product(path)
                    report['Plans'][choice]=dict(Sources=sources,Modules=modules,Domains=domains);save()
        features=('ATOMIC','QUEUE','QUEUE_MPSC','MEMORY_DEBUG','TIME','WAIT','THREAD','SYNC','MUTEX','COND','CANCEL',
                  'FUTURE','NET','NET_SOCKET','NET_BUFFER','NET_PORT','NET_PORT_SELECT','NET_ENGINE','NET_TCP','NET_TCP_FUTURE',
                  'NET_TCP_SERVER','NET_TCP_SERVER_FUTURE','NET_TCP_SERVER_SYNC','NET_SYNC')
        features+=('NET_PORT_IOCP',) if os.name=='nt' else ('NET_PORT_EPOLL',)
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined',
                '-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for mode,options in modes:
                for kind in ('xrt','xruntime','resident','core'):
                    name=f'{index}.{mode}.{kind}';binary=out/(name+ext)
                    try:
                        fixture='tcp_server_'+('resident_core' if kind=='core' else 'resident_ownership' if kind=='resident' else 'native_ownership')
                        native_flags=['-DXRT_FEATURE_'+feature for feature in features] if kind in ('xrt','xruntime') else []
                        if kind=='xruntime':native_flags+=['-DSERVER_PINNED_XRUNTIME']
                        sources=[] if kind in ('xrt','xruntime') else report['Plans']['server-native' if kind=='core' else 'server-all-native']['Sources']
                        run(name+'.build',flags(cc,options)+native_flags+[root/('test/support/'+fixture+'.c'),
                            *[root/source for source in sources],*libs,'-o',binary]);product(binary)
                        if kind=='core':
                            symbols=run(name+'.nm',[nm,'--defined-only',binary])
                            assert all(symbol not in symbols for symbol in ('xrtNetServerAcceptAsync','xrtNetTcpFutureProducerPolicyV1Get',
                                'xrtNetServerFutureProducerPolicyV1Get','xl6OwnershipDomainNativeFutureV1Get','xl6FutureCallbackQueue'))
                            report['Pruning'].append(name);save()
                        for backend,extra in (('select',[]),('native',['native'])):
                            text=run(name+'.'+backend,[binary,*extra],mode=='instrumented')
                            expected=('8-node owned graph','cases=3','balanced') if kind=='core' else (
                                'graphs=6','refusals=13','OOM-budgets=15','OOM-failures=13','NUL-transfers=2','drop-races=9','balanced')
                            assert all(word in text for word in expected),(name,text)
                            if kind=='resident':assert 'catalog OOM prefixes=64' in text
                    except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        controls=(
            ('missing-server-runtime','net_server_native','xrtNetServerRuntimeOwnershipV1Get()','xrtNetListenerRuntimeOwnershipV1Get()'),
            ('copied-listener-events','net_server_native','xrtNetServerListenerEventPolicyV1Get()','copiedEvents()'),
            ('missing-producer','net_server_future_native','xl6OwnershipCatalogAddProducerV1(catalog,xrtNetServerFutureProducerPolicyV1Get())','true'),
            ('missing-cancel','net_server_future_native','xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetServerFutureCancelPolicyV1Get())','true'),
            ('missing-stream-payload','net_server_future_native','xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetServerFutureStreamPayloadPolicyV1Get())','true'),
            ('missing-activation','net_server_future_native','*adapter=xrtNetServerFutureOwnershipAdapterV1(ref,preparation);','(void)ref;(void)preparation;'))
        sources=report['Plans']['server-all-native']['Sources']
        for name,unit,old,new in controls:
            try:
                original=root/('src/runtime/xl6_ownership_domain_'+unit+'.c');body=original.read_text(encoding='utf8')
                assert body.count(old)==1,(name,'inexact control');changed=body.replace(old,new)
                if name=='copied-listener-events':
                    anchor='static bool load('
                    changed=changed.replace(anchor,'''static const xnetlistenereventsownershipv1* copiedEvents(void)
{static xnetlistenereventsownershipv1 policy;policy=*xrtNetServerListenerEventPolicyV1Get();return &policy;}
'''+anchor)
                mutant=out/(name+'.c');mutant.write_text(changed,encoding='utf8');product(mutant)
                binary=out/(name+ext);selected=[mutant if root/source==original else root/source for source in sources]
                run(name+'.build',flags(gcc,['-O0'])+['-I'+str(root/'src/runtime'),
                    root/'test/support/tcp_server_resident_ownership.c',*selected,*libs,'-o',binary]);product(binary)
                run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
    except Exception as error:failures.append(dict(Name='planning',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
    report['Complete']=not failures and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
    save();assert report['Complete'],failures


if __name__=='__main__':main()
