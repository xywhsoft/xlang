"""Owned ordered-path foundation only; offline ordinary fixed chains and OOM.
No language/public ABI, building, store, identity, TLS or shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
import cryptography
from x509_path_fixtures import fixtures


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    sources=[root/'src/runtime'/('xl6_x509_'+s+'.c') for s in ('value','certificate','record_storage','anchor','path_result','path_validate')]
    sources+=[root/'src/runtime/xl6_native_handle.c',root/'src/api/xl_foreign_error.c']
    files=[Path(__file__),root/'tools/x509_path_fixtures.py',root/'tools/check_key_format_wire.py',
        root/'test/support/x509_path_owned.c',*sources,*root.glob('src/runtime/*.h'),root/'src/api/xl_foreign_error.h',
        *[root/'test/support'/('crypto_'+s+'_runtime.c') for s in ('rsa','nist','ed25519','xdh')],
        root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',
        root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',gcc,tcc]
    report=dict(Complete=False,Inputs={str(f):sha(f) for f in files},Rows=[],Failures=[],
        OrdinaryChains=fixtures(root,out),Oracle=cryptography.__version__,EveryFixtureSignatureIndependentlyVerified=True,
        PublicLanguageAcceptance=False,PathBuildAcceptance=False,StoreAcceptance=False,TlsAcceptance=False,
        ShippingAcceptance=False,NetworkTraffic=False,SecurityTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in (str(gcc),str(tcc)):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=240).returncode
        report['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    impl=out/'native.c';impl.write_text('#define XRT_MODULE_X509_PATH\n#define XRT_MODULE_X509_VERIFY_RSA\n#define XRT_MODULE_X509_VERIFY_ECDSA\n#define XRT_MODULE_X509_VERIFY_ED25519\n#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_VALUE\n#define XRT_MODULE_ONCE\n#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n')
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=8'] if cc==gcc else []),root/'test/support/x509_path_owned.c',impl,*sources,*libs,'-o',binary])
            assert 'X509 ordered path: 4 normal chains,' in run(name+'.run',[binary])
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(f)==s for f,s in report['Inputs'].items())
    report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in out.iterdir() if f.is_file() and f.name!='results.json']
    report['Complete']=not report['Failures'] and report['InputsUnchanged'];save();assert report['Complete'],report['Failures']


if __name__=='__main__':main()
