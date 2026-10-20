"""Selected resident datagram contracts and own loopback lifecycle acceptance.

Independent value/control/IO roots and their union; GCC/TCC and optional Linux sanitizers. No
external names/peers, multicast traffic, security tests or full network/shipping
certification. Language enum-composition semantics are a separate decision.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out',type=Path,required=True);parser.add_argument('--compiler',action='append',required=True)
    parser.add_argument('--sanitize',action='store_true');a=parser.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists() and (not a.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in a.compiler]
    gcc=next(cc for cc in compilers if cc.stem.lower()!='tcc')
    planSource=root/'test/support/net_datagram_plan.c';runtimeSource=root/'test/support/net_datagram_runtime.c'
    files={Path(__file__).resolve(),*compilers,planSource,runtimeSource,root/'test/support/executor_catalog_plan.c'}
    for directory in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(f for f in (root/directory).rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc'))
    inputs={str(f):sha(f) for f in sorted(files)};out.mkdir(parents=True)
    rows=[];products=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,LogsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Failures=failures,Plans={},InstrumentedSelectedClosure=False,OwnLoopbackOnly=True,ExternalNetworkTests=False,
        NetworkSecurityTests=False,MulticastTraffic=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,proof=None,sanitize=False):
        log=out/(name+'.log');command=list(map(str,command));env=dict(os.environ);start=time.monotonic()
        if sanitize:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:r=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        rows.append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save()
        print(name+': '+str(r.returncode),flush=True);assert not r.returncode,str(log)
        text=log.read_text(encoding='utf8',errors='replace')
        if proof:assert proof in text,str(log)
        return text
    def flags(cc):
        result=[cc,'-std=c11','-Wall','-Werror','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
        if cc.stem.lower()!='tcc':result+=['-Wextra']
        if os.name!='nt':result+=['-D_GNU_SOURCE']
        return result
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    extension='.exe' if os.name=='nt' else '';save()
    try:
        for opt in ('O0','O2'):
            plan=out/('plan.'+opt+extension)
            run('plan.'+opt+'.build',[*flags(gcc),'-'+opt,'-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections',
                '-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',planSource,
                root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan]);product(plan)
            run('plan.'+opt+'.run',[plan],'Datagram plan: exact value/control/IO component identities')
            if opt=='O0':
                for name in ('values','control','io','integration'):
                    sources=run(name+'.sources',[plan,'--sources',name]).splitlines()
                    assert len(sources)==len(set(sources));report['Plans'][name]=sources
                    for kind in ('modules','domains'):
                        text=run(name+'.'+kind,[plan,'--'+kind,name]);assert '_MODULE_ALL' not in text
                        target=out/(name+'.'+kind+'.h');target.write_text(text,encoding='utf8');product(target)
                    # Use a separate implementation TU. The fixture and resident
                    # units consume full declaration headers, not pruned private
                    # data-layout definitions leaking from an implementation TU.
                    impl=out/(name+'.impl.c')
                    impl.write_text('#include "'+name+'.modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'
                        '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8');product(impl)
                    domains=out/(name+'.domain.inc');domains.write_text('#include "'+name+'.domains.h"\n',encoding='utf8');product(domains)
        defines={'values':['-DTEST_DATAGRAM_VALUES_ONLY'],'control':['-DTEST_DATAGRAM_CONTROL_ONLY'],
            'io':['-DTEST_DATAGRAM_IO'],'integration':['-DTEST_DATAGRAM_IO','-DTEST_DATAGRAM_INTEGRATION']}
        witness='Datagram runtime: exact values/graph/control/IO, checked errors and memory balance'
        for name in defines:
            directory=out/name;directory.mkdir()
            # The include name is fixed in the reusable C fixture, its consumed
            # bytes are copied verbatim from this exact closure's generated list.
            domain=directory/'net-datagram-domains.h';shutil.copy2(out/(name+'.domains.h'),domain);product(domain)
            providers=[root/path for path in report['Plans'][name]]
            for index,cc in enumerate(compilers):
                for opt in ('O0','O2'):
                    label=name+'.'+str(index)+'.'+opt;binary=directory/(str(index)+'.'+opt+extension)
                    command=[*flags(cc),'-I'+str(directory),*defines[name],'-'+opt,runtimeSource,out/(name+'.impl.c'),*providers,*libraries,'-o',binary]
                    assert sum(len(str(item))+3 for item in command)<28000
                    run(label+'.build',command);product(binary);run(label+'.run',[binary],witness)
            if a.sanitize:
                binary=directory/'instrumented';run(name+'.instrumented.build',[*flags(gcc),'-I'+str(directory),*defines[name],'-O1','-g',
                    '-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-pie','-no-pie',runtimeSource,out/(name+'.impl.c'),
                    *providers,*libraries,'-o',binary]);product(binary);run(name+'.instrumented.run',[binary],witness,sanitize=True)
        report['InstrumentedSelectedClosure']=a.sanitize
        report['InputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged'] and all(sha(row['Path'])==row['Sha256'] for row in products)
        report['Complete']=True
    except BaseException as error:failures.append(repr(error));save();raise
    finally:save()

if __name__=='__main__':main()
