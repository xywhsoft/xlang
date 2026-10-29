"""Offline immutable UDP config, exact leaf graph, widths, OOM and pruning.
No Engine/socket construction, traffic, DNS or network-security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists() and (not a.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in a.compiler];gcc=next(cc for cc in compilers if cc.stem!='tcc')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,*[root/('test/support/'+name+'.c')
        for name in ('udp_config_plan','udp_config_runtime','executor_catalog_plan')]}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};out.mkdir(parents=True)
    report=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],NetworkTraffic=False,
        ExternalNetworkTests=False,NetworkSecurityTests=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):report['Products'].append(dict(Path=str(path),Sha256=sha(path)));save()
    def flags(cc,extra):return [cc,'-std=c11','-Wall','-Wextra','-Werror',*extra,'-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out),*(['-D_GNU_SOURCE'] if os.name!='nt' else [])]
    def run(name,command,instrumented=False):
        command=list(map(str,command));env=dict(os.environ)
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log')
        with log.open('wb') as stream:r=subprocess.run(command,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        report['Rows'].append(dict(Name=name,Command=command,Exit=r.returncode,Log=str(log),Sha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert r.returncode==0,(name,str(log));return log.read_text(errors='replace')
    ext='.exe' if os.name=='nt' else '';libraries=['-lws2_32','-liphlpapi','-ladvapi32','-lshell32','-lole32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    save()
    try:
        for opt in ('O0','O2'):
            plan=out/('plan.'+opt+ext)
            run('plan.'+opt+'.build',flags(gcc,['-'+opt])+['-Wno-missing-field-initializers',
                '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
                root/'test/support/udp_config_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan]);product(plan)
            run('plan.'+opt+'.run',[plan])
            if opt=='O0':
                sources=run('sources',[plan,'sources']).splitlines()
                for kind in ('modules','domains'):
                    content=run(kind,[plan,kind]);header=out/('udp-config-'+kind+'.h');header.write_text(content,encoding='utf8');product(header)
                report['Sources']=sources;assert not any('execution_' in source for source in sources);save()
                implementation=out/'udp-config-implementation.c'
                implementation.write_text('#include "udp-config-modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'
                    '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8');product(implementation)
        for ci,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',
                ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            for mode,options in modes:
                name=f'{ci}.{mode}';binary=out/(name+ext)
                run(name+'.build',flags(cc,options)+[root/'test/support/udp_config_runtime.c',implementation,
                    *[root/source for source in sources],*libraries,'-o',binary]);product(binary)
                text=run(name+'.run',[binary],mode=='instrumented')
                assert 'builders=12 invalid=18 OOM-budgets=36' in text
        report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
        report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
        report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
        report['Complete']=all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'));save();assert report['Complete']
    except BaseException as error:report['Failures'].append(repr(error));save();raise

if __name__=='__main__':main()
