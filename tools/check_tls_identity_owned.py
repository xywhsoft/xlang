"""Offline normal TLS identities, real ownership catalog and allocation rollback.
No network traffic, malformed crypto, original security suite or full TLS claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec,padding
from tls_identity_fixtures import fixtures

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--root',type=Path);p.add_argument('--support',type=Path);a=p.parse_args()
    root=(a.root or Path(__file__).resolve().parents[1]).resolve();support=(a.support or root/'test/support/tls_identity_owned.c').resolve()
    controls=Path(__file__).resolve().parent;out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    fixtures(root,out)
    declared=set(re.findall(r'XL6_TEXT_API [^;\n]*?\b(\w+)\(',(root/'src/runtime/xl6_tls_identity_abi.h').read_text()))
    exported=set(re.findall(r'XL6_TEXT_EXPORT\((\w+),',(root/'src/runtime/xl6_tls_identity_symbols.inc').read_text()))
    contracted=set(re.findall(r'^\{"(\w+)"',(root/'src/ir/xl6_ir_tls_identity_contracts.inc').read_text(),re.M))
    contracted.update(re.findall(r'XL6_NATIVE_COLLECTION_BRIDGE\("(\w+)"',(root/'src/abi/xl6_tls_identity_collection_contracts.inc').read_text()))
    assert len(declared)==8 and declared==exported==contracted
    files=[Path(__file__),gcc,tcc,*[controls/name for name in ('tls_identity_fixtures.py','x509_identity_fixtures.py','check_key_format_wire.py')],
        support,root/'test/support/tls_identity_plan.c',root/'src/compile/xl6_compile_unit.c',
        *[root/'test/support'/('crypto_'+name+'_runtime.c') for name in ('rsa','nist','ed25519','xdh')],
        *root.glob('src/runtime/*.h'),*root.glob('src/runtime/*.inc'),*root.glob('src/compile/*.h'),*root.glob('src/compile/*.inc'),
        root/'src/ir/xl6_ir_tls_identity_contracts.inc',root/'src/abi/xl6_tls_identity_collection_contracts.inc',
        root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h',
        *[f for f in out.iterdir() if f.is_file()]]
    r=dict(Complete=False,Inputs={str(f):sha(f) for f in files},Rows=[],Failures=[],NormalFamilies=4,ResidentEntries=8,
        EverySignatureIndependentlyVerified=True,NetworkTraffic=False,SecurityTests=False,MalformedCryptoTests=False,
        TrustAcceptance=False,TlsAcceptance=False,ShippingAcceptance=False,Root=str(root),Support=str(support),NormalSignatureVerifications=[])
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in (str(gcc),str(tcc)):
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(command,cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),'-I'+str(out)]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    planner=out/('planner.exe' if os.name=='nt' else 'planner')
    run('planner.build',[gcc,*flags,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-ffunction-sections','-fdata-sections',
        '-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',root/'test/support/tls_identity_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/s for s in run('planner.sources',[planner,'--sources']).splitlines()];modules=run('planner.modules',[planner,'--modules'])
    assert all('#define XRT_MODULE_TLS_IDENTITY'+name in modules for name in ('_RSA','_P256','_P384','_ED25519'))
    forbidden=('_MODULE_ALL','MODULE_TLS_SESSION','MODULE_TLS_STREAM','MODULE_TLS_CLIENT','MODULE_TLS_SERVER','MODULE_TLS_VERIFY',
        'MODULE_X509_VERIFY','MODULE_X509_PATH','MODULE_X509_STORE','MODULE_NET_SOCKET','MODULE_NET_DNS','MODULE_NET_ENGINE')
    assert not any(name in modules for name in forbidden)
    r['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for f in sources:r['Inputs'][str(f)]=sha(f)
    impl=out/'native.c';impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+
        ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes+=[('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'])]
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=5'] if cc==gcc else []),
                support,impl,*sources,*libs,'-o',binary])
            text=run(name+'.run',[binary]);assert 'TLS identity owned:' in text
            signatures=re.findall(r'^TLS identity signature ([0-3]) ([0-9a-f]+)$',text,re.M)
            assert len(signatures)==4 and {int(i) for i,_ in signatures}==set(range(4))
            for index,encoded in signatures:
                index=int(index);family=('rsa','p256','p384','ed25519')[index];signature=bytes.fromhex(encoded)
                public=x509.load_der_x509_certificate((out/(family+'.identity.der')).read_bytes()).public_key()
                message=b'ordinary offline TLS identity'
                if index==0:public.verify(signature,message,padding.PKCS1v15(),hashes.SHA256())
                elif index in (1,2):public.verify(signature,message,ec.ECDSA(hashes.SHA256() if index==1 else hashes.SHA384()))
                else:public.verify(signature,message)
                r['NormalSignatureVerifications'].append(dict(Mode=name,Family=family,SignatureSha256=hashlib.sha256(signature).hexdigest()))
        except Exception as error:r['Failures'].append(dict(Name=name,Error=repr(error)));save()
    r['InputsUnchanged']=all(sha(f)==digest for f,digest in r['Inputs'].items())
    r['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in out.iterdir() if f.is_file() and f.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
