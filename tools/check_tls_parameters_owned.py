"""Real, pruned resident TLS configuration owners; no network or cryptography."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--root',type=Path)
    p.add_argument('--support-root',type=Path);a=p.parse_args();root=(a.root or Path(__file__).resolve().parents[1]).resolve()
    support=(a.support_root or root).resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    inputs=[Path(__file__),gcc,tcc,support/'test/support/tls_parameters_owned.c',support/'test/support/tls_parameters_plan.c',root/'src/compile/xl6_compile_unit.c',
        *root.glob('src/runtime/*.h'),*root.glob('src/runtime/*.inc'),*root.glob('src/compile/*.h'),*root.glob('src/compile/*.inc'),
        root/'src/abi/xl6_tls_numeric_enums.inc',root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h',root/'release/include/xrt_decl.h',root/'release/include/xruntime_decl.h']
    r=dict(Complete=False,Root=str(root),Inputs={str(p):sha(p) for p in inputs},Rows=[],Failures=[],NetworkTraffic=False,SecurityTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n')
    def run(name,args):
        args=list(map(str,args));command=args
        if args[0] in [str(gcc),str(tcc)]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in args[1:])+'\n');command=[args[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:code=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert not code,str(log);return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release')]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    wire={
        'TlsVersion':dict(v12='XTLS_VERSION_12',v13='XTLS_VERSION_13'),
        'TlsCipherSuite':dict(aes128GcmSha256='XTLS_AES_128_GCM_SHA256',aes256GcmSha384='XTLS_AES_256_GCM_SHA384',chacha20Poly1305Sha256='XTLS_CHACHA20_POLY1305_SHA256',
            ecdheEcdsaAes128GcmSha256='XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256',ecdheRsaAes128GcmSha256='XTLS_ECDHE_RSA_AES_128_GCM_SHA256',
            ecdheEcdsaAes256GcmSha384='XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384',ecdheRsaAes256GcmSha384='XTLS_ECDHE_RSA_AES_256_GCM_SHA384',
            ecdheRsaChacha20Poly1305Sha256='XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256',ecdheEcdsaChacha20Poly1305Sha256='XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256'),
        'TlsGroup':dict(p256='XTLS_GROUP_SECP256R1',p384='XTLS_GROUP_SECP384R1',p521='XTLS_GROUP_SECP521R1',x25519='XTLS_GROUP_X25519',x448='XTLS_GROUP_X448',
            ffdhe2048='XTLS_GROUP_FFDHE2048',ffdhe3072='XTLS_GROUP_FFDHE3072',ffdhe4096='XTLS_GROUP_FFDHE4096',ffdhe6144='XTLS_GROUP_FFDHE6144',ffdhe8192='XTLS_GROUP_FFDHE8192'),
        'TlsSignature':dict(rsaPkcs1Sha256='XTLS_SIGNATURE_RSA_PKCS1_SHA256',ecdsaP256Sha256='XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256',
            rsaPkcs1Sha384='XTLS_SIGNATURE_RSA_PKCS1_SHA384',ecdsaP384Sha384='XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384',rsaPkcs1Sha512='XTLS_SIGNATURE_RSA_PKCS1_SHA512',
            ecdsaP521Sha512='XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512',rsaPssRsaeSha256='XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256',rsaPssRsaeSha384='XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384',
            rsaPssRsaeSha512='XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512',ed25519='XTLS_SIGNATURE_ED25519',ed448='XTLS_SIGNATURE_ED448',rsaPssPssSha256='XTLS_SIGNATURE_RSA_PSS_PSS_SHA256',
            rsaPssPssSha384='XTLS_SIGNATURE_RSA_PSS_PSS_SHA384',rsaPssPssSha512='XTLS_SIGNATURE_RSA_PSS_PSS_SHA512'),
        'TlsKeySharePolicy':dict(preferGroup='XTLS_KEY_SHARE_PREFER_GROUP',preferReady='XTLS_KEY_SHARE_PREFER_READY')}
    lines=['#include "src/runtime/xl6_tls_parameters_internal.h"'];count=0;seen=set();kind=None
    for line in (root/'src/abi/xl6_tls_numeric_enums.inc').read_text().splitlines():
        begin=re.fullmatch(r'XL6_TLS_ENUM_BEGIN\((\w+),\w+,\d+\)',line)
        item=re.fullmatch(r'XL6_TLS_ENUM_ITEM\((\w+),(\d+)\)',line)
        if begin:kind=begin[1]
        if item:
            name,value=item.groups();native=wire[kind][name];seen.add((kind,name));count+=1
            lines.append('_Static_assert('+value+' == '+native+', "exact '+kind+'.'+name+'");')
    assert count==37 and seen=={(kind,name) for kind,items in wire.items() for name in items}
    lines.append('int main(void){return 0;}');wireSource=out/'enum-wire.c';wireSource.write_text('\n'.join(lines)+'\n');wireBinary=out/('enum-wire.exe' if os.name=='nt' else 'enum-wire')
    run('wire.build',[gcc,*flags,'-O0','-Wall','-Wextra','-Werror',wireSource,'-o',wireBinary]);run('wire.run',[wireBinary]);r['ExactNativeEnumMembers']=count
    planner=out/('planner.exe' if os.name=='nt' else 'planner')
    run('planner.build',[gcc,*flags,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-O0','-ffunction-sections','-fdata-sections',
        '-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',support/'test/support/tls_parameters_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/s for s in run('planner.sources',[planner,'--sources']).splitlines()];modules=run('planner.modules',[planner,'--modules'])
    assert 'XRT_MODULE_TLS_CONTEXT' in modules and not any(s in modules for s in ['_MODULE_ALL','MODULE_TLS_SESSION','MODULE_TLS_STREAM','MODULE_TLS_CLIENT','MODULE_TLS_SERVER','MODULE_TLS_VERIFY','MODULE_CRYPTO_','MODULE_NET_SOCKET'])
    r['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    for s in sources:r['Inputs'][str(s)]=sha(s)
    impl=out/'native.c';impl.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+
        ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if '#define XRUNTIME_MODULE_' in modules else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'))
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc.O0',gcc,['-O0']),('gcc.O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes.append(('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    for name,cc,options in modes:
        try:
            binary=out/(name+('.exe' if os.name=='nt' else ''))
            run(name+'.build',[cc,*flags,*options,*(['-Wall','-Wextra','-Werror','-fmax-errors=5'] if cc==gcc else []),support/'test/support/tls_parameters_owned.c',impl,*sources,*libs,'-o',binary])
            assert 'TLS parameters owned:' in run(name+'.run',[binary])
        except Exception as e:r['Failures'].append(dict(Name=name,Error=repr(e)));save()
    r['InputsUnchanged']=all(sha(p)==d for p,d in r['Inputs'].items());r['Stages']=len(r['Rows'])
    r['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in out.iterdir() if p.is_file() and p.name!='results.json']
    r['Complete']=not r['Failures'] and r['InputsUnchanged'];save();assert r['Complete'],r['Failures']

if __name__=='__main__':main()
