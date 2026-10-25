"""Actual-source private TLS selector request packet: no network/security tests.

The two snapshots are compiled independently with GCC O0/O2 and genuine TCC.
This is a prerequisite unit gate, not typed-callback or shipping acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    tcc=Path(shutil.which('tcc') or 'E:/software/tcc/tcc.exe').resolve()
    source=root/'src/runtime/xl6_tls_server_request.c'
    case=root/'test/cases/xlir_tls_server_request_owned.c'
    plan=root/'test/support/tls_server_request_plan.c'
    files={Path(__file__).resolve(),gcc,tcc,source,case,plan,
        root/'tools/compiler_dll.rsp.in',root/'tools/runtime_dll.rsp.in'}
    files.update(q for q in (root/'src').rglob('*') if q.is_file() and q.suffix in ['.h','.inc'])
    files.update(root/q for q in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xruntime_decl.h','release/include/xrt_decl.h'])
    files.add(root/'src/compile/xl6_compile_unit.c')
    report=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(files)},Rows=[],Products=[],Failures=[],
        NetworkTraffic=False,NetworkSecurityTests=False,MalformedCryptoTests=False,
        TypedSelectorAccepted=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));invocation=command
        if Path(command[0]) in [gcc,tcc]:
            rsp=out/(name+'.rsp')
            rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"' for s in command[1:])+'\n',encoding='utf8')
            invocation=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as f:
            code=subprocess.run(invocation,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=300).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start))
        save();print(name,code,flush=True);assert code==0,str(log)
        return log.read_text(encoding='utf8',errors='replace')
    flags=['-std=c11','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    try:
        for template in ['compiler_dll.rsp.in','runtime_dll.rsp.in']:
            rows=(root/'tools'/template).read_text(encoding='utf8').replace('\\','/').splitlines()
            assert sum(q.strip()=='src/runtime/xl6_tls_server_request.c' for q in rows)==1
        for label in ['O0','O2']:
            binary=out/('plan.'+label+('.exe' if os.name=='nt' else ''))
            run('plan.'+label+'.build',[gcc,*flags,'-'+label,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
                '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
                plan,root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',binary])
            assert 'TLS server request plan: one resident source, Hello/Value only;' in run('plan.'+label+'.run',[binary])
        for product,header,macro in [('core','lib/xrt/xrt.h','XRT_IMPLEMENTATION'),('runtime','lib/xruntime/xruntime.h','XRUNTIME_IMPLEMENTATION')]:
            implementation=out/(product+'.c')
            implementation.write_text('#define '+macro+'\n#define XRT_MODULE_TLS_HELLO\n#define XRT_MODULE_VALUE\n#define XRT_MODULE_MEMORY_DEBUG\n#include "'+header+'"\n',encoding='utf8')
            selected=out/(product+'.selectors.c')
            selected.write_text('#define XRT_MODULE_TLS_HELLO\n#define XRT_MODULE_VALUE\n#include "'+header+'"\n'
                '#if defined(XRT_FEATURE_TLS_CLIENT) || defined(XRT_FEATURE_TLS_SERVER) || defined(XRT_FEATURE_TLS_RESUME) || defined(XRT_FEATURE_TLS_STREAM) || defined(XRT_FEATURE_NET_TCP)\n'
                '#error request_packet_pulled_unrelated_tls_or_transport\n#endif\nint main(void){return 0;}\n',encoding='utf8')
            for label,cc,options in [('O0',gcc,['-O0']),('O2',gcc,['-O2']),('tcc',tcc,[])]:
                name=product+'.'+label;binary=out/(name+('.exe' if os.name=='nt' else ''))
                strict=['-Wall','-Wextra','-Werror'] if cc==gcc else []
                run(name+'.selectors',[cc,*flags,*options,*strict,'-c',selected,'-o',out/(name+'.selectors.o')])
                # Compile the real implementation, adapter and case separately;
                # the adapter must work with declarations-only dependencies.
                objects=[]
                for suffix,path in [('native',implementation),('adapter',source),('case',case)]:
                    obj=out/(name+'.'+suffix+'.o');objects.append(obj)
                    run(name+'.'+suffix,[cc,*flags,*options,*strict,'-c',path,'-o',obj])
                run(name+'.link',[cc,*objects,*libraries,'-o',binary])
                assert 'TLS server request: detached name/ALPN; one allocation; 2048 ordered protocols; 96 OOM prefixes balanced' in run(name+'.run',[binary])
        report['InputsUnchanged']=all(sha(q)==digest for q,digest in report['Inputs'].items())
        report['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.iterdir() if q.is_file() and q.name!='results.json']
        report['Stages']=len(report['Rows']);report['Complete']=report['InputsUnchanged'];save();assert report['Complete']
    except Exception as e:
        report['Failures'].append(repr(e));save();raise


if __name__=='__main__':
    main()
