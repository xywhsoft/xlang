"""Real static-role managed TLS resources, ordinary literal loopback only.

Fixed local certificate chains, memory/ownership/publication checks. No public
endpoints, DNS, probing, malformed protocol/crypto data or security campaigns.
This does not certify shipping libraries or the complete language TLS facade.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time,sys

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sys.path.insert(0,str(root/'tools'));from tls_role_context_fixtures import fixtures
    assert len(fixtures(root,out))==4
    compilers=[Path(shutil.which(s) or s).resolve() for s in a.compiler];gcc=next(q for q in compilers if q.stem!='tcc')
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,*[q for q in (root/'src').rglob('*') if q.is_file() and q.suffix in ['.c','.h','.inc']],
        root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',
        root/'test/support/tls_execution_plan.c',root/'test/support/tls_execution_resource.c'}
    files.update(Path(m.__file__) for m in sys.modules.values() if getattr(m,'__file__',None) and Path(m.__file__).is_relative_to(root/'tools'))
    files.update(q for q in out.iterdir() if q.is_file())
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],
        OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False,PublicLanguageApiAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def product(q):r['Products'].append(dict(Path=str(q),Sha256=sha(q)));save()
    def write(q,text):q.write_text(text,encoding='utf8');product(q)
    def run(name,command,instrumented=False):
        command=list(map(str,command));invocation=command
        if Path(command[0]) in compilers:
            rsp=out/(name+'.rsp');write(rsp,'\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n');invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic();env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as f:code=subprocess.run(invocation,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240).returncode
        row=dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start);r['Rows'].append(row);save()
        print(name,code,flush=True);assert not code,str(log);return log.read_text(encoding='utf8',errors='replace')
    def flags(cc,opt):return [cc,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',*opt,*(['-std=c11'] if cc.stem!='tcc' else []),
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    try:
        ext='.exe' if os.name=='nt' else ''
        for opt in ['O0','O2']:
            plan=out/('plan.'+opt+ext)
            run('plan.'+opt+'.build',flags(gcc,['-'+opt,'-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables'])+
                [root/'test/support/tls_execution_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan]);product(plan)
            assert 'five exact role/resource closures' in run('plan.'+opt+'.run',[plan])
        sources=run('plan.sources',[plan,'--sources']).splitlines()
        assert len(sources)==len(set(sources))
        for mode in ['modules','domains']:write(out/('tls-resource-'+mode+'.h'),run('plan.'+mode,[plan,'--'+mode]))
        implementation=out/'native-implementation.c'
        write(implementation,'#include "tls-resource-modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_TLS_IDENTITY_RSA\n#define XRT_MODULE_NET_PORT_SELECT\n'+
            ('#define XRT_MODULE_NET_PORT_IOCP\n' if os.name=='nt' else '#define XRT_MODULE_NET_PORT_EPOLL\n')+
            '#define XRT_IMPLEMENTATION\n#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n')
        r['Sources']=sources;save()
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
        for index,cc in enumerate(compilers):
            modes=[('tcc',[])] if cc.stem=='tcc' else [('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':
                assert os.name!='nt';modes.append(('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for mode,opt in modes:
                name=str(index)+'.'+mode;binary=out/(name+ext)
                run(name+'.build',flags(cc,opt)+[root/'test/support/tls_execution_resource.c',implementation,*[root/q for q in sources],*libraries,'-o',binary]);product(binary)
                for backend in ['select','native']:
                    text=run(name+'.'+backend,[binary,*(['native'] if backend=='native' else [])],mode=='asan')
                    match=re.search(r'managed TLS resources: graphs=(\d+) ordinary=(\d+) scoped-last-owner=(\d+) deferred=(\d+) budgets=(\d+) failures=(\d+);.*balanced backend=(\d+)',text)
                    assert match,text;n=list(map(int,match.groups()));assert n[:4]==[4,2,1,1] and 0<n[5]<=n[4]-2<96
                    r['Rows'][-1]['ObservedCounters']=n;save()
        r['InputsUnchanged']=all(sha(q)==d for q,d in r['Inputs'].items())
        r['ProductsUnchanged']=all(sha(q['Path'])==q['Sha256'] for q in r['Products'])
        r['LogsUnchanged']=all(sha(q['Log'])==q['Sha256'] for q in r['Rows'])
        r['Complete']=all(r[k] for k in ['InputsUnchanged','ProductsUnchanged','LogsUnchanged']);save();assert r['Complete']
    except Exception as e:r['Failures'].append(repr(e));save();raise
if __name__=='__main__':main()
