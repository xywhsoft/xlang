"""Typed Engine owned-policy registry plus existing config/graph acceptance against actual selected sources.
No sockets, DNS, external endpoints, probing or network-security tests. This is
not shipping DLL/VFS or complete transport acceptance. Mutation controls are
ordinary ownership/error unit checks, not protocol fuzzing.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();assert not a.sanitize or os.name!='nt'
    compilers=[Path(shutil.which(c) or c).resolve() for c in a.compiler];gcc=next(c for c in compilers if c.stem.lower()!='tcc')
    def sha(f):return hashlib.sha256(Path(f).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,*[root/('test/support/'+n+'.c') for n in ('net_engine_plan','net_engine_runtime','net_engine_config','executor_catalog_plan','net_engine_policy_plan','net_engine_policy_runtime')],
        root/'test/support/net_engine_policy_declarations.c',*[root/n for n in ('build_linux.sh','build_dll.bat','tools/compiler_dll.rsp.in','tools/runtime_dll.rsp.in','tools/build_xlir_public_source_driver.ps1')]}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc'))
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True);rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,
        Plans={},NegativeControls=[],NetworkTraffic=False,NetworkSecurityTests=False,Scope=__doc__,ShippingLibraryAcceptance=False,InstrumentedSelectedClosure=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,args,proof=None,reject=False,sanitized=False):
        env=dict(os.environ)
        if sanitized:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:r=subprocess.run(list(map(str,args)),cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180,env=env)
        rows.append(dict(Name=name,Command=list(map(str,args)),Exit=r.returncode,ExpectedRejection=reject,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name,r.returncode,flush=True);assert r.returncode!=0 if reject else r.returncode==0,log
        text=log.read_text(encoding='utf8',errors='replace')
        if reject:assert ('incompatible-pointer-types' in text if name.startswith('sdk.bad.') else 'assert' in text.lower()),log
        if proof:assert proof in text,log
        return text
    def flags(cc,folder):
        f=[cc,'-std=c11','-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(folder)]
        if cc.stem.lower()!='tcc':f+=['-Wextra']
        if os.name!='nt':f+=['-D_GNU_SOURCE']
        return f
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm'];ext='.exe' if os.name=='nt' else ''
    save()
    try:
        sdk=out/'sdk'/'__xl_runtime';sdk.mkdir(parents=True)
        for header in (root/'src/runtime').glob('*.h'):
            target=sdk/header.name;shutil.copyfile(header,target);product(target)
        for opt in ('O0','O2'):
            run('sdk.'+opt,flags(gcc,out)+['-I'+str(sdk.parent),'-'+opt,'-fsyntax-only',root/'test/support/net_engine_policy_declarations.c'])
        for group,wrong in (('NetTask','xnettimerownershipv1'),('NetTimer','xnetengineworkerexitownershipv1'),('NetWorkerExit','xnettaskownershipv1')):
            source=out/('sdk.bad.'+group+'.c')
            source.write_text('#include <__xl_runtime/xl6_ownership_catalog.h>\nbool bad(XL6_OwnershipCatalog* c,const struct '+wrong+'* p){return xl6OwnershipCatalogAdd'+group+'V1(c,p);}\n',encoding='utf8');product(source)
            run('sdk.bad.'+group,flags(gcc,out)+['-I'+str(sdk.parent),'-fsyntax-only',source],reject=True)
        run('api.symbols.syntax',flags(gcc,out)+['-DXL_STATIC_EMBED','-Wno-missing-field-initializers','-fsyntax-only',root/'src/api/xl_api_symbols.c'])
        abi=(root/'src/runtime/xl6_net_engine_abi.h').read_text();assert not re.search(r'^\s*#include\s+.*\.inc',abi,re.M)
        for field in re.findall(r'^XL6_NET_ENGINE_CONFIG_SIZE\((\w+),', (root/'src/runtime/xl6_net_engine_config_fields.inc').read_text(),re.M):
            assert 'uint64_t xl6NetEngineConfig'+field+'V1(const void*);' in abi
            assert 'void* xl6NetEngineConfigWith'+field+'V1(const void*,uint64_t);' in abi
        for field in re.findall(r'^XL6_NET_ENGINE_STAT\((\w+),',(root/'src/runtime/xl6_net_engine_stats_fields.inc').read_text(),re.M):
            assert 'uint64_t xl6NetEngineSnapshot'+field+'V1(const void*);' in abi
        report['SelfContainedSdkVocabulary']=True;save()
        for opt in ('O0','O2'):
            plan=out/('plan.'+opt+ext)
            run('plan.'+opt+'.build',flags(gcc,out)+['-'+opt,'-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections','-fno-jump-tables',
                '-fno-asynchronous-unwind-tables','-fno-unwind-tables',root/'test/support/net_engine_policy_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan]);product(plan)
            run('plan.'+opt+'.run',[plan],'Engine plan: typed source/native/work domains')
            if opt=='O0':
                for choice in ('all','config','engine','stats'):
                    sources=run(choice+'.sources',[plan,'--sources',choice]).splitlines();assert len(sources)==len(set(sources))
                    folder=out/choice;folder.mkdir();modules=folder/'net-engine-modules.h';domains=folder/'net-engine-domains.h'
                    modules.write_text(run(choice+'.modules',[plan,'--modules',choice]),encoding='utf8');product(modules)
                    domains.write_text(run(choice+'.domains',[plan,'--domains',choice]),encoding='utf8');product(domains)
                    report['Plans'][choice]=sources;save()
        proof='Engine policy registry: graphs=2'
        for i,cc in enumerate(compilers):
            for opt in ('O0','O2'):
                for fixture,choice,message in (('net_engine_policy_runtime','all',proof),('net_engine_config','config','Engine options: independent immutable storage')):
                    name=f'{i}.{opt}.{choice}';binary=out/(name+ext)
                    run(name+'.build',flags(cc,out/choice)+['-'+opt,root/('test/support/'+fixture+'.c'),*[root/s for s in report['Plans'][choice]],*libs,'-o',binary]);product(binary)
                    run(name+'.run',[binary],message)
        controls=(('duplicate-closure-finish','xl6_closure.c','    if (storage->environmentFinished) return true;','    /* incorrect: repeat consumed environment cleanup */'),
            ('copied-family-admitted','xl6_execution_resource.c','families[i]==r->family','families[i]->kind==r->family->kind'),
            ('lost-producer-failure','xl6_detached_work.c','job->body=(xl6_error_result){false,error};','job->body=(xl6_error_result){true,error};'),
            ('control-read-overlap','xl6_execution_resource.c','!resource->retiring&&!resource->control&&','!resource->retiring&&'),
            ('native-busy-refused','xl6_ownership_domain_net_engine_native.c','return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetEngineOwnershipQuiescenceV1Get());','(void)catalog;return true;'),
            ('pool-busy-refused','xl6_ownership_domain_net_buffer_native.c','return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetBufPoolOwnershipQuiescenceV1Get());','(void)catalog;return true;'),
            ('pool-owner-refused','xl6_ownership_domain_net_buffer_native.c','*adapter=xrtNetBufPoolOwnershipAdapterV1(ref,preparation);','*adapter=NULL;(void)preparation;'))
        for name,filename,old,new in controls:
            original=root/'src/runtime'/filename;body=original.read_text(encoding='utf8');assert body.count(old)==1,(name,'inexact control')
            mutant=out/(name+'.c');mutant.write_text(body.replace(old,new),encoding='utf8');product(mutant)
            sources=[mutant if root/s==original else root/s for s in report['Plans']['all']];binary=out/(name+ext)
            run(name+'.build',flags(gcc,out/'all')+['-O0','-I'+str(root/'src/runtime'),root/'test/support/net_engine_runtime.c',*sources,*libs,'-o',binary]);product(binary)
            run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
        # Exact typed view plumbing is checked at the real native ABI boundary,
        # then delegated to the original XRT implementation (no fake graph).
        controls2=[
            ('empty-task-list','xl6_ownership_domain_net_engine_native.c','policies->NetTask,policies->NetTaskCount','NULL,0'),
            ('empty-timer-list','xl6_ownership_domain_net_engine_native.c','policies->NetTimer,policies->NetTimerCount','NULL,0'),
            ('empty-exit-list','xl6_ownership_domain_net_engine_native.c','policies->NetWorkerExit,policies->NetWorkerExitCount','NULL,0'),
            ('duplicate-capabilities','xl6_ownership_catalog.c','if(catalog->Name[i]==policy)return true;','if(false && catalog->Name[i]==policy)return true;'),
            ('reopened-policy-set','xl6_ownership_catalog.c','!catalog->building || !policy','false || !policy'),
            ('ignored-policy-failure','xl6_ownership_catalog.c','!domains[i]->Load(catalog) || catalog->failed','!domains[i]->Load(catalog)'),
        ]
        for name,filename,old,new in controls2:
            original=root/'src/runtime'/filename;body=original.read_text(encoding='utf8');assert body.count(old)==1,(name,'inexact control')
            mutant=out/(name+'.c');mutant.write_text(body.replace(old,new),encoding='utf8');product(mutant)
            sources=[mutant if root/s==original else root/s for s in report['Plans']['all']];binary=out/(name+ext)
            run(name+'.build',flags(gcc,out/'all')+['-O0','-I'+str(root/'src/runtime'),root/'test/support/net_engine_policy_runtime.c',*sources,*libs,'-o',binary]);product(binary)
            run(name+'.refused',[binary],reject=True);report['NegativeControls'].append(name);save()
        if a.sanitize:
            binary=out/'instrumented'
            run('instrumented.build',flags(gcc,out/'all')+['-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie',
                root/'test/support/net_engine_policy_runtime.c',*[root/s for s in report['Plans']['all']],*libs,'-o',binary]);product(binary)
            run('instrumented.run',[binary],proof,sanitized=True);report['InstrumentedSelectedClosure']=True
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items());report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged'];report['Complete']=True;save()
    except BaseException as error:failures.append(repr(error));save();raise
if __name__=='__main__':main()
