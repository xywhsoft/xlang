"""Selected actual resident resume-chain adapters, no language/provider stubs.
Normal DER, sealed arrays, ownership and allocation failures; no sockets.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time
from tls_verifier_fixtures import fixtures


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['root','out','tcc']:parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve()
    assert not out.exists();out.mkdir(parents=True)
    controls=Path(__file__).resolve().parents[1]
    assert fixtures(root,out)==['rsa','p256','p384','ed25519']
    gcc=Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve();tcc=args.tcc.resolve()
    plan=controls/'test/support/tls_resume_peer_plan.c';support=controls/'test/support/tls_resume_peer_runtime.c'
    files={Path(__file__),gcc,tcc,plan,support,root/'src/compile/xl6_compile_unit.c',
        *[p for p in (root/'src').rglob('*') if p.is_file() and p.suffix in ['.h','.inc']],
        *[root/p for p in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']],
        *[p for p in out.iterdir() if p.is_file()]}
    files.update(controls/'tools'/name for name in ['tls_verifier_fixtures.py','x509_path_fixtures.py','check_key_format_wire.py'])
    report=dict(Complete=False,Inputs={str(p):sha(p) for p in sorted(files)},Rows=[],NetworkTraffic=False,
        SecurityTests=False,ShippingAccepted=False,FullTlsAccepted=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));executed=command
        if Path(command[0]) in [gcc,tcc]:
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+s.replace('\\','/').replace('"','\\"')+'"'
                for s in command[1:])+'\n',encoding='utf8');executed=[command[0],'@'+str(rsp)]
        log=out/(name+'.log');start=time.monotonic()
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        with log.open('wb') as stream:code=subprocess.run(executed,cwd=root,env=env,stdout=stream,
            stderr=subprocess.STDOUT,timeout=900).returncode
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start))
        save();print(name,code,flush=True);assert code==0,str(log)
        return log.read_text(errors='replace')
    flags=['-std=c11','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(out)]+([] if os.name=='nt' else ['-D_GNU_SOURCE'])
    planner=out/('planner.exe' if os.name=='nt' else 'planner')
    run('planner.build',[gcc,*flags,'-O0','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
        '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables',
        '-fno-unwind-tables',plan,root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner])
    sources=[root/p for p in run('planner.sources',[planner,'--sources']).splitlines()]
    modules=run('planner.modules',[planner,'--modules'])
    assert 'XRT_MODULE_TLS_CLIENT_RESUME' in modules and '_MODULE_ALL' not in modules
    report['Closure']=dict(Sources=list(map(str,sources)),Modules=modules)
    report['Inputs'].update({str(p):sha(p) for p in sources})
    implementation=out/'native.c'
    implementation.write_text(modules+'\n#define XRT_MODULE_MEMORY_DEBUG\n'+
        ('#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n' if 'XRUNTIME_MODULE_' in modules
         else '#define XRT_IMPLEMENTATION\n#include "lib/xrt/xrt.h"\n'),encoding='utf8')
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    modes=[('gcc-O0',gcc,['-O0']),('gcc-O2',gcc,['-O2']),('tcc',tcc,[])]
    if os.name!='nt':modes+=[('asan',gcc,['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'])]
    for name,compiler,options in modes:
        binary=out/(name+('.exe' if os.name=='nt' else ''))
        run(name+'.build',[compiler,*flags,*options,*(['-Wall','-Wextra','-Werror'] if compiler==gcc else []),
            support,implementation,*sources,*libraries,'-o',binary])
        assert '256 capture/import OOM prefixes; memory balanced' in run(name+'.run',[binary])
    report['InputsUnchanged']=all(sha(p)==h for p,h in report['Inputs'].items());report['Complete']=report['InputsUnchanged']
    report['Products']={str(p):sha(p) for p in out.iterdir() if p.is_file() and p.name!='results.json'}
    save();assert report['Complete']


if __name__=='__main__':main()
