"""Actual managed aggregate Server resources, parent owners and accepted-result lifecycle.

Offline ownership/OOM checks and ordinary literal loopback only. No external
endpoint, DNS or network-security tests. Not public-language or shipping DLL
acceptance: the selected resident closure is compiled independently.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--profile',choices=('server','owned-sends','owned-receives'),default='server')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    gcc=next(cc for cc in compilers if cc.stem!='tcc')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,*[root/('test/support/'+name+'.c')
        for name in ('net_engine_plan','executor_catalog_plan','tcp_server_execution_resource','tcp_server_poll_runtime',
            'net_bytes_runtime','tcp_send_owned_runtime','tcp_recv_owned_runtime')]}
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/area).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};out.mkdir(parents=True)
    rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,
        Inputs=inputs,Rows=rows,Products=products,Failures=failures,Plans={},NegativeControls=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        PublicLanguageApiAccepted=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,instrumented=False,reject=False):
        command=list(map(str,command));invocation=command;env=dict(os.environ)
        if command[0] in map(str,compilers) and Path(command[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"'
                for arg in command[1:])+'\n',encoding='utf8');product(rsp);invocation=[command[0],'@'+str(rsp)]
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');timeout=None
        with log.open('wb') as stream:
            try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180).returncode
            except subprocess.TimeoutExpired as error:code=None;timeout=repr(error)
        rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=code,Timeout=timeout,
            ExpectedRejection=reject,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code is not None and (code!=0 if reject else code==0),(name,str(log),timeout)
        text=log.read_text(encoding='utf8',errors='replace')
        if reject:assert 'line ' in text or (args.profile!='server' and 'Assertion' in text),(name,'expected lifecycle/ownership assertion')
        return text
    def flags(cc,mode):
        result=[cc,'-std=c11','-Wall','-Wextra','-Werror',*mode,'-I'+str(root),
            '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
        if os.name!='nt':result+=['-D_GNU_SOURCE']
        return result
    ext='.exe' if os.name=='nt' else ''
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    save()
    try:
        for opt in ('O0','O2'):
            plan=out/('plan.'+opt+ext)
            run('plan.'+opt+'.build',flags(gcc,['-'+opt])+['-Wno-missing-field-initializers',
                '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables',
                '-fno-unwind-tables',root/'test/support/net_engine_plan.c',root/'src/compile/xl6_compile_unit.c',
                '-Wl,--gc-sections','-o',plan]);product(plan)
            run('plan.'+opt+'.run',[plan])
            if opt=='O0':
                choices=('server-resource','server-language','server-options') if args.profile=='server' else ('net-bytes','net-bytes-buffer','tcp-owned')
                if args.profile=='owned-receives':choices=('tcp-recv-owned',)
                for choice in choices:
                    sources=run(choice+'.sources',[plan,'--sources',choice]).splitlines()
                    modules=run(choice+'.modules',[plan,'--modules',choice])
                    domains=run(choice+'.domains',[plan,'--domains',choice])
                    assert len(sources)==len(set(sources)) and not any('resolver' in source or 'task_pool' in source for source in sources)
                    if choice in ('server-resource','tcp-owned','tcp-recv-owned'):
                        assert 'xl6OwnershipDomainExecutionTcpServerV1Get(),\n' in domains and 'xl6OwnershipDomainExecutionTcpV1Get(),\n' in domains
                        assert 'xl6OwnershipDomainNetTcpNativeV1Get(),\n' in domains
                        assert 'XRT_MODULE_NET_TCP_FUTURE\n' in modules
                        for kind,text in (('modules',modules),('domains',domains)):
                            header=out/('tcp-server-resource-'+kind+'.h');header.write_text(text,encoding='utf8');product(header)
                    else:assert not any('execution_tcp.c' in source for source in sources)
                    if choice.startswith('net-bytes'):
                        assert 'xl6OwnershipDomainNetBytesV1Get(),\n' in domains and 'xl6OwnershipDomainNetBufferNativeV1Get(),\n' in domains
                        assert not any('execution_' in source for source in sources)
                        assert all(module not in modules for module in ('XRT_MODULE_NET_ENGINE\n','XRT_MODULE_NET_TCP\n','XRT_MODULE_FUTURE\n'))
                    if choice=='net-bytes-buffer':
                        header=out/'net-bytes-modules.h';header.write_text(modules,encoding='utf8');product(header)
                    if choice=='server-options':assert 'xl6OwnershipDomainExecutionTcpServerV1Get(),\n' not in domains
                    report['Plans'][choice]=dict(Sources=sources,Modules=modules,Domains=domains);save()
        selected='server-resource' if args.profile=='server' else 'tcp-owned' if args.profile=='owned-sends' else 'tcp-recv-owned'
        sources=report['Plans'][selected]['Sources']
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',
                ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for opt,mode in modes:
                name=f'{index}.{opt}.resource';binary=out/(name+ext)
                try:
                    if args.profile=='owned-receives':
                        run(name+'.build',flags(cc,mode)+[root/'test/support/tcp_recv_owned_runtime.c',
                            *[root/source for source in sources],*libs,'-o',binary]);product(binary)
                        for backend,extra in (('select',[]),('native',['native'])):
                            text=run(name+'.'+backend,[binary,*extra],opt=='instrumented')
                            assert 'TCP owned receives: budgets=24' in text and 'complete graph' in text and 'balanced' in text
                        continue
                    if args.profile=='owned-sends':
                        pure=out/(f'{index}.{opt}.net-bytes'+ext)
                        run(f'{index}.{opt}.net-bytes.build',flags(cc,mode)+[root/'test/support/net_bytes_runtime.c',
                            *[root/source for source in report['Plans']['net-bytes-buffer']['Sources']],*libs,'-o',pure]);product(pure)
                        text=run(f'{index}.{opt}.net-bytes.run',[pure],opt=='instrumented')
                        assert 'exact two-node owning graph' in text and '16 allocation budgets' in text
                        run(name+'.build',flags(cc,mode)+[root/'test/support/tcp_send_owned_runtime.c',
                            *[root/source for source in sources],*libs,'-o',binary]);product(binary)
                        for backend,extra in (('select',[]),('native',['native'])):
                            text=run(name+'.'+backend,[binary,*extra],opt=='instrumented')
                            assert 'TCP owned sends: budgets=16' in text and 'source unchanged on OOM/close, balanced' in text
                        continue
                    run(name+'.build',flags(cc,mode)+[root/'test/support/tcp_server_execution_resource.c',
                        *[root/source for source in sources],*libs,'-o',binary]);product(binary)
                    for backend,extra in (('select',[]),('native',['native'])):
                        text=run(name+'.'+backend,[binary,*extra],opt=='instrumented')
                        assert all(word in text for word in ('graphs=24 ordinary=3 abandoned=1 deferred=1',
                            'allocation-budgets=80','NUL roundtrip; balanced'))
                    poll_name=f'{index}.{opt}.poll';poll_binary=out/(poll_name+ext)
                    run(poll_name+'.build',flags(cc,mode)+[root/'test/support/tcp_server_poll_runtime.c',
                        *[root/source for source in sources],root/'src/runtime/xl6_tcp_server_poll.c',*libs,'-o',poll_binary]);product(poll_binary)
                    for backend,extra in (('select',[]),('native',['native'])):
                        text=run(poll_name+'.'+backend,[poll_binary,*extra],opt=='instrumented')
                        assert 'empty=16000 allocation-free, budgets=16 failures=' in text and 'abort rollback and balanced' in text
                except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        domain=root/'src/runtime/xl6_ownership_domain_execution_tcp.c';body=domain.read_text(encoding='utf8')
        controls=[]
        needle='xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionTcpStreamFamilyV1Get())'
        assert body.count(needle)==1
        controls.append(('missing-stream-family',domain,body.replace(needle,'true')))
        domain=root/'src/runtime/xl6_ownership_domain_execution_tcp_server.c';body=domain.read_text(encoding='utf8')
        needle='XL6_OWNERSHIP_DOMAIN_POLICY(ExecutionTcpServer,ExecutionFamily,xl6ExecutionTcpServerFamilyV1Get)'
        assert body.count(needle)==1
        controls.append(('missing-server-family',domain,body.replace(needle,
            'static bool empty(XL6_OwnershipCatalog* c){(void)c;return true;}\nXL6_OWNERSHIP_DOMAIN(ExecutionTcpServer,empty,NULL)')))
        server=root/'src/runtime/xl6_execution_tcp_server.c';body=server.read_text(encoding='utf8')
        needle='(!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context))'
        assert body.count(needle)==1
        controls.append(('missing-server-parent',server,body.replace(needle,'(!r->parent||true)')))
        resource=root/'src/runtime/xl6_execution_tcp.c';body=resource.read_text(encoding='utf8')
        guard='if(xrtFutureState(pending)==XFUTURE_PENDING)abort();'
        wait='if(xrtFutureState(pending)==XFUTURE_PENDING)return XL6_MODULE_PREPARE_BUSY;'
        assert body.count(guard)==1 and body.count(wait)==1
        if args.profile=='server':controls.append(('abandon-pending-accept',resource,body.replace(guard,'').replace(wait,'')))
        if args.profile!='server':
            payload=root/'src/runtime/xl6_ownership_domain_net_bytes.c';body=payload.read_text(encoding='utf8')
            needle='XL6_OWNERSHIP_DOMAIN_POLICY(NetBytes,Handle,xl6NetBytesHandlePolicyV1Get)';assert body.count(needle)==1
            controls.append(('missing-bytes-policy',payload,body.replace(needle,
                'static bool empty(XL6_OwnershipCatalog* c){(void)c;return true;}\nXL6_OWNERSHIP_DOMAIN(NetBytes,empty,NULL)')))
            payload=root/'src/runtime/xl6_net_bytes.c';body=payload.read_text(encoding='utf8')
            needle='XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetBytesHandlePolicyV1Get,xl6NetBytesDestroyV1,trace)';assert body.count(needle)==1
            controls.append(('bytes-false-leaf',payload,body.replace(needle,
                'XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetBytesHandlePolicyV1Get,xl6NetBytesDestroyV1)')))
        for name,original,body in controls:
            try:
                mutant=out/(name+'.c');mutant.write_text(body,encoding='utf8');product(mutant)
                pure_control=args.profile=='owned-sends' and name in ('missing-bytes-policy','bytes-false-leaf')
                selected_sources=report['Plans']['net-bytes-buffer']['Sources'] if pure_control else sources
                selected=[mutant if root/source==original else root/source for source in selected_sources]
                fixture='net_bytes_runtime' if pure_control else 'tcp_send_owned_runtime' if args.profile=='owned-sends' else 'tcp_server_execution_resource'
                if args.profile=='owned-receives':fixture='tcp_recv_owned_runtime'
                binary=out/(name+ext)
                run(name+'.build',flags(gcc,['-O0'])+['-Wno-unused-function','-I'+str(root/'src/runtime'),
                    root/('test/support/'+fixture+'.c'),*selected,*libs,'-o',binary]);product(binary)
                run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
    except Exception as error:failures.append(dict(Name='planning',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
    report['Complete']=not failures and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
    save();assert report['Complete'],failures


if __name__=='__main__':main()
