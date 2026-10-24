"""Normal offline TLS verifier owners and resource rollback; not TLS transport.
No network traffic, malformed cryptography, trust bypass or security suites.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from tls_verifier_fixtures import fixtures

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--root',type=Path);p.add_argument('--support',type=Path)
    a=p.parse_args();root=(a.root or Path(__file__).resolve().parents[1]).resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    support=(a.support or root/'test/support/tls_verifier_owned.c').resolve()
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    assert fixtures(root,out)==['rsa','p256','p384','ed25519']
    inputs=[Path(__file__),gcc,tcc,root/'tools/tls_verifier_fixtures.py',root/'tools/x509_path_fixtures.py',root/'tools/check_key_format_wire.py',
        *[root/'test/support'/('crypto_'+n+'_runtime.c') for n in ['rsa','nist','ed25519','xdh']],support,root/'test/support/tls_verifier_plan.c',root/'src/compile/xl6_compile_unit.c',
        *[p for p in (root/'src').rglob('*') if p.is_file() and p.suffix in ['.h','.inc']],root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',
        *[p for p in out.iterdir() if p.is_file()]]
    r=dict(Complete=False,Root=str(root),Inputs={str(p):sha(p) for p in inputs},Rows=[],Failures=[],NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in [str(gcc),str(tcc)]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    planner=out/('planner.exe' if os.name=='nt' else 'planner')
    run('planner.build',[gcc,*flags,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-ffunction-sections','-fdata-sections',
        '-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',root/'test/support/tls_verifier_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/s for s in run('planner.sources',[planner,'--sources']).splitlines()];modules=run('planner.modules',[planner,'--modules'])
    assert 'XRT_MODULE_TLS_VERIFY' in modules and not any(s in modules for s in ['_MODULE_ALL','MODULE_TLS_SESSION','MODULE_TLS_STREAM','MODULE_TLS_CLIENT','MODULE_TLS_SERVER','MODULE_NET_SOCKET'])
    r['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for s in sources:r['Inputs'][str(s)]=sha(s)
    impl=out/'native.c';impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+
        ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=5'] if cc==gcc else []),support,impl,*sources,*libs,'-o',binary])
            assert 'TLS verifier owned:' in run(name+'.run',[binary])
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(p)==d for p,d in r['Inputs'].items());r['Stages']=len(r['Rows'])
    r['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.iterdir() if p.is_file() and p.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']

if __name__=='__main__':main()
