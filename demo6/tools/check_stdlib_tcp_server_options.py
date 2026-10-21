"""Immutable Server options, actual independent storage and transactional OOM, parent owners and accepted-result lifecycle.

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
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    gcc=next(cc for cc in compilers if cc.stem!='tcc')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,*[root/('test/support/'+name+'.c')
        for name in ('net_engine_plan','executor_catalog_plan','tcp_server_options_runtime')]}
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/area).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};out.mkdir(parents=True)
    rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,ProductsUnchanged=False,
        Inputs=inputs,Rows=rows,Products=products,Failures=failures,Plans={},NegativeControls=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        PublicLanguageApiAccepted=False,NoEngineOrSocketCreated=True,ShippingLibraryAcceptance=False,Scope=__doc__)
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
        if reject:assert 'line ' in text,(name,'expected lifecycle/ownership assertion')
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
                for choice in ('server-options',):
                    sources=run(choice+'.sources',[plan,'--sources',choice]).splitlines()
                    modules=run(choice+'.modules',[plan,'--modules',choice])
                    domains=run(choice+'.domains',[plan,'--domains',choice])
                    assert len(sources)==len(set(sources)) and not any('resolver' in source or 'task_pool' in source for source in sources)
                    assert 'xl6OwnershipDomainTcpServerOptionsV1Get(),\n' in domains
                    assert not any('execution_' in source for source in sources)
                    for kind,text in (('modules',modules),('domains',domains)):
                        header=out/('tcp-server-options-'+kind+'.h');header.write_text(text,encoding='utf8');product(header)
                    report['Plans'][choice]=dict(Sources=sources,Modules=modules,Domains=domains);save()
        sources=report['Plans']['server-options']['Sources']
        implementation=out/'options-implementation.c'
        implementation.write_text('#include "tcp-server-options-modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8');product(implementation)
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',
                ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for opt,mode in modes:
                name=f'{index}.{opt}.resource';binary=out/(name+ext)
                try:
                    run(name+'.build',flags(cc,mode)+[root/'test/support/tcp_server_options_runtime.c',implementation,
                        *[root/source for source in sources],*libs,'-o',binary]);product(binary)
                    text=run(name+'.run',[binary],opt=='instrumented')
                    assert 'OOM budgets=16 failures=8, exact leaf, balanced; no Engine/I/O' in text
                except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
    except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
    except Exception as error:failures.append(dict(Name='planning',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
    report['Complete']=not failures and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
    save();assert report['Complete'],failures


if __name__=='__main__':main()
