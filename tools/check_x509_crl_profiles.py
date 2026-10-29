"""Offline immutable X.509 profiles: ordinary fixed-key data, lifetime and OOM.
Not trust/TLS/shipping acceptance; no network, malformed-input or security tests.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse,hashlib,json,os,re,shutil,subprocess,time
import cryptography
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import padding
from check_key_format_wire import vectors
from check_x509_owned import children,content,tlv


from x509_crl_fixtures import fixtures


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',(root/'src/runtime/xl6_x509_crl_profiles_abi.h').read_text()))
    exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',(root/'src/runtime/xl6_x509_crl_profiles_symbols.inc').read_text()))
    contracted=set(re.findall(r'^\{"(\w+)"',(root/'src/ir/xl6_ir_x509_crl_profiles_contracts.inc').read_text(),re.M))
    assert len(declared)==45 and declared==exported==contracted
    files=[Path(__file__),root/'tools/check_key_format_wire.py',root/'tools/check_x509_owned.py',root/'tools/x509_crl_fixtures.py',gcc,tcc,
        root/'test/support/x509_crl_profiles.c',root/'test/support/x509_crl_profiles_plan.c',root/'src/compile/xl6_compile_unit.c',
        *root.glob('src/runtime/*.h'),*root.glob('src/runtime/*.inc'),*root.glob('src/compile/*.inc'),*root.glob('src/compile/*.h'),
        root/'src/ir/xl6_ir_x509_crl_profiles_contracts.inc',root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',
        root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h']
    report=dict(Complete=False,Rows=[],Failures=[],Inputs={str(f):sha(f) for f in files},Vectors=len(fixtures(root,out)[0]),
        ResidentEntries=45,Oracle=cryptography.__version__,EveryFixtureSignatureIndependentlyVerified=True,
        TrustAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False,NetworkTraffic=False,SecurityTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in (str(gcc),str(tcc)):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=240).returncode
        report['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    planner=out/('planner.exe' if os.name=='nt' else 'planner')
    run('planner.build',[gcc,*flags,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',root/'test/support/x509_crl_profiles_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/s for s in run('planner.sources',[planner,'--sources']).splitlines()];modules=run('planner.modules',[planner,'--modules'])
    assert not any(s in modules for s in ('_MODULE_ALL','MODULE_TLS','MODULE_CRYPTO','MODULE_X509_VERIFY','MODULE_X509_PATH','MODULE_X509_STORE'))
    report['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for f in sources:report['Inputs'][str(f)]=sha(f)
    impl=out/'native.c';impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=4'] if cc==gcc else []),root/'test/support/x509_crl_profiles.c',impl,*sources,*libs,'-o',binary])
            assert 'X509 CRL profiles: 4 valid vectors,' in run(name+'.run',[binary])
        except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(f)==s for f,s in report['Inputs'].items());report['LogsUnchanged']=all(sha(r['Log'])==r['Sha256'] for r in report['Rows'])
    report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in out.iterdir() if f.is_file() and f.name!='results.json']
    report['Complete']=not report['Failures'] and report['InputsUnchanged'] and report['LogsUnchanged'];save();assert report['Complete'],report['Failures']


if __name__=='__main__':main()
