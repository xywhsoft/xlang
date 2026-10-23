"""Offline TLS verifier limits in both implementation/declaration products.
No transport, security campaign, public language or shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
import cryptography
from x509_path_fixtures import fixtures

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);a=p.parse_args()
    root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    paths=[Path(__file__),gcc,tcc,root/'test/support/tls_verify_limits_snapshot.c',root/'tools/x509_path_fixtures.py',root/'tools/check_key_format_wire.py',
        *[root/'test/support'/('crypto_'+name+'_runtime.c') for name in ['rsa','nist','ed25519','xdh']],
        root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',
        root/'lib/xrt/backports/tls_verifier_limits.json']
    assert fixtures(root,out)==4
    report=dict(Complete=False,Inputs={str(p):sha(p) for p in paths},Rows=[],Failures=[],CryptographyVersion=cryptography.__version__,
        NetworkTraffic=False,SecurityTests=False,PublicTlsAccepted=False,ShippingAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in [str(gcc),str(tcc)]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=600).returncode
        report['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    modules=''.join('#define XRT_MODULE_'+n+'\n' for n in ['TLS_VERIFY','X509_VERIFY_RSA','X509_VERIFY_ECDSA','X509_VERIFY_ED25519','MEMORY_DEBUG'])
    for runtime in [False,True]:
        product='runtime' if runtime else 'core';implementation=out/(product+'.c')
        implementation.write_text(modules+('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n' if runtime else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
        for mode,cc,options in modes:
            prefix=product+'.'+mode;obj=out/(prefix+'.o');strict=['-Wall','-Wextra','-Werror','-fmax-errors=5'] if cc==gcc else []
            try:run(prefix+'.implementation',[cc,*flags,*options,*strict,'-c',implementation,'-o',obj])
            except Exception as e:report['Failures'].append(dict(Name=prefix,Error=repr(e)));save();continue
            for declaration in [False,True]:
                name=prefix+('.xrdecl' if declaration else '.xdecl');binary=out/(name+('.exe' if os.name=='nt' else ''))
                try:
                    run(name+'.build',[cc,*flags,*options,*strict,*(['-DXL6_TEST_XRUNTIME_DECL'] if declaration else []),root/'test/support/tls_verify_limits_snapshot.c',obj,*libs,'-o',binary])
                    assert 'TLS verifier budgets: 4 normal families; 48 exact boundaries;' in run(name+'.run',[binary])
                except Exception as e:report['Failures'].append(dict(Name=name,Error=repr(e)));save()
    report['InputsUnchanged']=all(sha(p)==d for p,d in report['Inputs'].items());report['Stages']=len(report['Rows'])
    report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.iterdir() if p.is_file() and p.name!='results.json']
    report['Complete']=not report['Failures'] and report['InputsUnchanged'];save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
