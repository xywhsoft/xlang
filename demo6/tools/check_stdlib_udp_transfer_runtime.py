"""UDP transfer/message/Worker-control selected resident closure. Offline and literal loopback only.
No public endpoints, DNS, malformed/security tests, or OS error-queue probes.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--compiler',action='append',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();assert not out.exists()
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in a.compiler];gcc=next(cc for cc in compilers if cc.stem!='tcc')
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,root/'test/support/udp_message_runtime.c',root/'test/support/udp_transfer_runtime.c',root/'test/support/udp_transfer_plan.c',root/'test/support/udp_transfer_test.h',*[root/('test/support/udp_language_'+name+'.c') for name in ('native','runtime','plan')]}
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/area).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};out.mkdir(parents=True)
    report=dict(Complete=False,Inputs=inputs,Rows=[],Products=[],Failures=[],Plans={},OrdinaryLiteralLoopback=True,
        ExternalNetworkTests=False,NetworkSecurityTests=False,ErrorQueueProbes=False,ShippingLibraryAcceptance=False,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):report['Products'].append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,instrumented=False):
        command=list(map(str,command));invocation=command;env=dict(os.environ)
        if Path(command[0]) in compilers and Path(command[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"' for arg in command[1:])+'\n',encoding='utf8')
            product(rsp);invocation=[command[0],'@'+str(rsp)]
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log')
        with log.open('wb') as stream:
            try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180).returncode
            except subprocess.TimeoutExpired:code=None
        report['Rows'].append(dict(Name=name,Command=command,Invocation=invocation,Exit=code,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code==0,(name,str(log));return log.read_text(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    suffix='.exe' if os.name=='nt' else '';save()
    try:
        planner=out/('plan'+suffix);run('plan.build',[gcc,*flags,'-O0','-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections',
            '-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
            root/'test/support/udp_transfer_plan.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',planner]);product(planner);run('plan.run',[planner])
        for choice in ('message','text','refs','mixed','copy','control','all'):
            sources=run(choice+'.sources',[planner,'--sources',choice]).splitlines()
            modules=run(choice+'.modules',[planner,'--modules',choice]);domains=run(choice+'.domains',[planner,'--domains',choice])
            report['Plans'][choice]=dict(Sources=sources,Modules=modules,Domains=domains);save()
            assert len(sources)==len(set(sources)) and not any('tcp' in name or 'resolver' in name or 'task_pool' in name for name in sources)
            if choice!='all':assert 'XRT_MODULE_NET_UDP_FUTURE\n' not in modules
        for choice in ('message','all'):
            for kind in ('modules','domains'):
                path=out/('udp-language-'+kind+'.h');path.write_text(report['Plans'][choice][kind.title()],encoding='utf8')
                frozen=out/(choice+'-'+kind+'.h');shutil.copyfile(path,frozen);product(frozen)
            fixture='udp_message_runtime.c' if choice=='message' else 'udp_transfer_runtime.c'
            for index,cc in enumerate(compilers):
                modes=[('O0',['-O0']),('O2',['-O2'])]
                if a.sanitize and cc.stem!='tcc':modes.append(('instrumented',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
                for opt,mode in modes:
                    name=f'{choice}.{index}.{opt}';binary=out/(name+suffix)
                    try:
                        run(name+'.build',[cc,*flags,*mode,root/('test/support/'+fixture),root/'test/support/udp_language_native.c',
                            *[root/source for source in report['Plans'][choice]['Sources']],*libs,'-o',binary]);product(binary)
                        for backend,options in (('select',[]),('native',['native'])) if choice=='all' else (('offline',[]),):
                            text=run(name+'.'+backend,[binary,*options],opt=='instrumented')
                            assert ('UDP transfer runtime:' in text and 'balanced' in text) if choice=='all' else 'UDP immutable message:' in text
                    except Exception as error:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    except Exception as error:report['Failures'].append(dict(Name='planning',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in report['Rows'])
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in report['Products'])
    report['Complete']=not report['Failures'] and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'));save();assert report['Complete'],report['Failures']

if __name__=='__main__':main()
