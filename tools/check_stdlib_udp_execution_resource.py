"""Selected resident UDP ownership closure; offline and ordinary loopback only.

No external endpoints, DNS, malformed/security probes or error-queue testing.
This is not public-language, shipping DLL, JIT or AOT acceptance.
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
    files={Path(__file__).resolve(),*compilers,*[root/('test/support/'+name+'.c')
        for name in ('net_engine_plan','executor_catalog_plan','udp_execution_resource')]}
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/area).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};out.mkdir(parents=True)
    rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,
        Inputs=inputs,Rows=rows,Products=products,Failures=failures,Plans={},NegativeControls=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        ErrorQueueProbes=False,PublicLanguageApiAccepted=False,ShippingLibraryAcceptance=False,Scope=__doc__)
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
        if reject:assert 'line ' in text,(name,'expected ownership assertion')
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
                for choice in ('udp-resource','udp-future','engine'):
                    sources=run(choice+'.sources',[plan,'--sources',choice]).splitlines()
                    modules=run(choice+'.modules',[plan,'--modules',choice])
                    domains=run(choice+'.domains',[plan,'--domains',choice])
                    assert len(sources)==len(set(sources))
                    assert not any('resolver' in source or 'task_pool' in source or 'tcp' in source for source in sources)
                    if choice!='engine':
                        assert 'src/runtime/xl6_execution_udp.c' in sources
                        assert 'xl6OwnershipDomainExecutionUdpV1Get(),\n' in domains
                        assert 'xl6OwnershipDomainNetUdpBaseNativeV1Get(),\n' in domains
                        assert ('XRT_MODULE_NET_UDP_FUTURE\n' in modules)==(choice=='udp-future')
                    else:assert 'XRT_MODULE_NET_UDP' not in modules
                    report['Plans'][choice]=dict(Sources=sources,Modules=modules,Domains=domains);save()
        for choice in ('udp-resource','udp-future'):
            selected=report['Plans'][choice];sources=selected['Sources']
            for kind in ('modules','domains'):
                header=out/('udp-'+kind+'.h');header.write_text(selected[kind.title()],encoding='utf8')
                # The same transient include name is deliberately rewritten
                # between disjoint build groups; immutable copies are hashed.
                frozen=out/(choice+'-'+kind+'.h');shutil.copyfile(header,frozen);product(frozen)
            extra=['-DUDP_FUTURE_ACCEPTANCE'] if choice=='udp-future' else []
            for index,cc in enumerate(compilers):
                modes=[('O0',['-O0']),('O2',['-O2'])]
                if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',
                    ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
                for opt,mode in modes:
                    name=f'{choice}.{index}.{opt}';binary=out/(name+ext)
                    try:
                        run(name+'.build',flags(cc,mode)+extra+[root/'test/support/udp_execution_resource.c',
                            *[root/source for source in sources],*libs,'-o',binary]);product(binary)
                        for backend,options in (('select',[]),('native',['native'])):
                            text=run(name+'.'+backend,[binary,*options],opt=='instrumented')
                            assert all(word in text for word in (('graphs=32 ordinary=4' if extra else 'graphs=8 ordinary=1'),
                                ('allocation-budgets=48 failures=' if extra else 'allocation-budgets=24 failures='),
                                'NUL roundtrip; balanced'))
                    except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        sources=report['Plans']['udp-future']['Sources']
        domain=root/'src/runtime/xl6_ownership_domain_execution_udp.c';body=domain.read_text(encoding='utf8')
        needle='xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionUdpFamilyV1Get())';assert body.count(needle)==1
        controls=[('missing-udp-family',domain,body.replace(needle,'((void)catalog,true)'))]
        resource=root/'src/runtime/xl6_execution_udp.c';body=resource.read_text(encoding='utf8')
        needle='(!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context))';assert body.count(needle)==1
        controls.append(('missing-parent-edge',resource,body.replace(needle,'((void)r,true)')))
        for name,original,body in controls:
            try:
                mutant=out/(name+'.c');mutant.write_text(body,encoding='utf8');product(mutant)
                binary=out/(name+ext);selected=[mutant if root/source==original else root/source for source in sources]
                run(name+'.build',flags(gcc,['-O0'])+['-DUDP_FUTURE_ACCEPTANCE','-I'+str(root/'src/runtime'),
                    root/'test/support/udp_execution_resource.c',*selected,*libs,'-o',binary]);product(binary)
                run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
            except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
    except Exception as error:failures.append(dict(Name='planning',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
    report['Complete']=not failures and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
    save();assert report['Complete'],failures


if __name__=='__main__':main()
