"""Check the targeted native empty-trust backport in all four snapshot consumers.
Offline normal certificate fixtures and ordinary declaration/ABI checks only;
not public candidate-building, language/JIT/AOT, TLS or shipping acceptance.
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
    paths=[Path(__file__),gcc,tcc,root/'test/support/x509_empty_trust_source.c',
        root/'tools/x509_path_fixtures.py',root/'tools/check_key_format_wire.py',root/'tools/check_x509_owned.py',
        *[root/'test/support'/('crypto_'+name+'_runtime.c') for name in ('rsa','nist','ed25519','xdh')],
        root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',
        root/'lib/xrt/backports/x509_empty_trust_source.json']
    report=dict(Complete=False,Inputs={str(f):sha(f) for f in paths},Rows=[],Failures=[],
        OrdinaryChains=fixtures(root,out),Oracle=cryptography.__version__,EveryFixtureSignatureIndependentlyVerified=True,
        PublicLanguageAcceptance=False,PathBuildAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False,
        NetworkTraffic=False,SecurityTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in (str(gcc),str(tcc)):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n')
            command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=240).returncode
        report['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==0,str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for runtime in (False,True):
        implementation=out/('runtime.c' if runtime else 'core.c')
        implementation.write_text('#define XRT_MODULE_X509_STORE\n#define XRT_MODULE_MEMORY_DEBUG\n'+
            ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n' if runtime else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
        for declaration in (False,True):
            prefix=('runtime' if runtime else 'core')+('.xrdecl' if declaration else '.xdecl')
            for mode,cc,options in modes:
                name=prefix+'.'+mode;binary=out/(name+('.exe' if os.name=='nt' else ''))
                try:
                    run(name+'.build',[cc,*flags,*(['-DXL6_TEST_XRUNTIME_DECL'] if declaration else []),*options,
                        *(['-Wall','-Wextra','-Werror','-fmax-errors=5'] if cc==gcc else []),
                        root/'test/support/x509_empty_trust_source.c',implementation,*libs,'-o',binary])
                    assert 'X509 empty trust source: 4 normal families;' in run(name+'.run',[binary])
                except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(f)==digest for f,digest in report['Inputs'].items())
    report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in out.iterdir() if f.is_file() and f.name!='results.json']
    report['Complete']=not report['Failures'] and report['InputsUnchanged'];save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
