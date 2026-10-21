"""Managed TCP resident ABI and exclusive queued Future acceptance.

Only offline ownership/OOM checks and ordinary literal own loopback; no DNS,
external endpoint, malformed traffic or network-security tests. This is not
generated-language or shipping DLL/SO acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler',action='append',required=True)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--dial',action='store_true',help='Include real managed Dial class projection and its sync/async closure')
    parser.add_argument('--interrupt',action='store_true',help='Include optional parent/deadline transport and its offline source-terminal acceptance')
    parser.add_argument('--dial-interrupt',action='store_true',help='Include real managed Dial interruption and the offline interruption baseline')
    parser.add_argument('--probe',action='store_true',help='Discovery only: first GCC O0 TCP fixture, not acceptance')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.out.resolve()
    if args.dial_interrupt:args.dial=args.interrupt=True
    assert not out.exists() and (not args.sanitize or os.name!='nt')
    compilers=[Path(shutil.which(cc) or cc).resolve() for cc in args.compiler]
    gcc=next(cc for cc in compilers if cc.stem!='tcc')
    if args.probe:compilers=[gcc]
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files={Path(__file__).resolve(),*compilers,root/'test/cases/xlir_future_callback_queue_native.c'}
    files.update(root/('test/support/'+name+'.c') for name in
        ('tcp_language_plan','tcp_language_runtime','future_callback_plan','future_callback_exclusive_queue'))
    if args.dial:files.update(root/('test/support/'+name+'.c') for name in ('tcp_dial_language_plan','tcp_dial_language_runtime'))
    if args.interrupt:files.update(root/('test/support/'+name+'.c') for name in ('future_callback_interrupt_plan','future_callback_interrupt'))
    if args.dial_interrupt:files.update(root/('test/support/'+name+'.c') for name in ('tcp_dial_interrupt_plan','tcp_dial_interrupt_runtime'))
    for area in ('src','lib/xrt','lib/xruntime','release/include'):
        files.update(path for path in (root/area).rglob('*') if path.is_file() and path.suffix in ('.c','.h','.inc'))
    inputs={str(path):sha(path) for path in sorted(files)};out.mkdir(parents=True)
    rows=[];products=[];failures=[]
    report=dict(Complete=False,Inputs=inputs,Rows=rows,Products=products,Failures=failures,Plans={},
        SharedCallbackFixtureCompiler='GCC on Linux: system TCC lacks C11 _Atomic/stdatomic.h; resident closure still uses each selected compiler',
        NegativeControls=[],OrdinaryLiteralLoopback=True,ExternalNetworkTests=False,NetworkSecurityTests=False,
        PublicLanguageApiAccepted=False,ShippingLibraryAcceptance=False,DialResidentAbi=args.dial,
        InterruptResidentAbi=args.interrupt,DialInterruptResidentAbi=args.dial_interrupt,DiscoveryOnly=args.probe,Scope=__doc__)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)));save()
    def run(name,command,instrumented=False,reject=False):
        command=list(map(str,command));invocation=command;env=dict(os.environ)
        if command[0] in map(str,compilers) and Path(command[0]).stem!='tcc':
            rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"'
                for arg in command[1:])+'\n',encoding='utf8');product(rsp);invocation=[command[0],'@'+str(rsp)]
        if instrumented:env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        log=out/(name+'.log');timeout=None
        with log.open('wb') as stream:
            try:code=subprocess.run(invocation,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=180).returncode
            except subprocess.TimeoutExpired as error:code=None;timeout=repr(error)
        rows.append(dict(Name=name,Command=command,Invocation=invocation,Exit=code,Timeout=timeout,
            ExpectedRejection=reject,Log=str(log),Sha256=sha(log)));save();print(name,code,flush=True)
        assert code is not None and (code!=0 if reject else code==0),(name,str(log),timeout)
        text=log.read_text(encoding='utf8',errors='replace')
        if reject:assert 'line ' in text,(name,'expected exclusive cancellation assertion')
        return text
    def flags(cc,mode):
        result=[cc,'-std=c11','-Wall','-Wextra','-Werror',*mode,'-I'+str(root),
            '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(out)]
        if os.name!='nt':result+=['-D_GNU_SOURCE']
        return result
    ext='.exe' if os.name=='nt' else ''
    libs=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    save()
    try:
        # Public SDK must not depend on private resident field tables.
        header=(root/'src/runtime/xl6_tcp_abi.h').read_text()
        fields=(root/'src/runtime/xl6_tcp_stats_fields.inc').read_text()
        for kind,field in re.findall(r'^XL6_TCP_(STREAM_UINT|STREAM_BOOL|LISTENER_UINT)\((\w+),',fields,re.M):
            name='Stream' if kind.startswith('STREAM') else 'Listener'
            result='bool' if kind=='STREAM_BOOL' else 'uint64_t'
            assert f'XL6_TEXT_API {result} xl6Tcp{name}Stats{field}V1(const void*);' in header
        assert '.inc"' not in header
        for kind in ('tcp','callback'):
            fixture=root/('test/support/'+(('tcp_dial_interrupt_plan' if args.dial_interrupt else 'tcp_dial_language_plan' if args.dial else 'tcp_language_plan') if kind=='tcp' else
                'future_callback_interrupt_plan' if args.interrupt else 'future_callback_plan')+'.c')
            for opt in ('O0','O2'):
                plan=out/(kind+'.plan.'+opt+ext)
                run(kind+'.plan.'+opt+'.build',flags(gcc,['-'+opt])+['-Wno-missing-field-initializers',
                    '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables',
                    '-fno-unwind-tables',fixture,root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',plan])
                product(plan);run(kind+'.plan.'+opt+'.run',[plan])
                if opt!='O0':continue
                extra=['all'] if kind=='tcp' else []
                selected={field:run(kind+'.'+field,[plan,'--'+field,*extra]) for field in ('sources','modules','domains')}
                selected['sources']=selected['sources'].splitlines()
                assert len(selected['sources'])==len(set(selected['sources']))
                for field in ('modules','domains'):
                    path=out/(('tcp-language' if kind=='tcp' else 'callback')+'-'+field+'.h')
                    path.write_text(selected[field],encoding='utf8');product(path)
                report['Plans'][kind]=selected;save()
        impl=out/'shared-callback-impl.c'
        impl.write_text('#include "callback-modules.h"\n#define XRT_MODULE_MEMORY_DEBUG\n'
            '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8');product(impl)
        for index,cc in enumerate(compilers):
            modes=[('O0',['-O0']),('O2',['-O2'])]
            if args.sanitize and cc.stem!='tcc':modes.append(('instrumented',
                ['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']))
            if args.probe:modes=modes[:1]
            for opt,mode in modes:
                for kind,fixture,extra in (
                    ('tcp',root/('test/support/'+('tcp_dial_interrupt_runtime' if args.dial_interrupt else 'tcp_dial_language_runtime' if args.dial else 'tcp_language_runtime')+'.c'),[]),
                    ('callback',root/('test/support/'+('future_callback_interrupt' if args.interrupt else 'future_callback_exclusive_queue')+'.c'),[]),
                    ('callback-shared',root/'test/cases/xlir_future_callback_queue_native.c',[impl,'-DTEST_PRODUCT_CATALOG'])):
                    name=f'{index}.{opt}.{kind}';binary=out/(name+ext)
                    if args.probe and kind!='tcp':continue
                    sources=report['Plans']['tcp' if kind=='tcp' else 'callback']['sources']
                    try:
                        if os.name!='nt' and cc.stem=='tcc' and kind=='callback-shared':
                            # The historical regression harness uses C11
                            # _Atomic, unsupported by distro TCC. Compile only
                            # that harness with GCC; all resident providers,
                            # native implementation and final link remain TCC.
                            fixtureObject=out/(name+'.fixture.o')
                            run(name+'.fixture.build',flags(gcc,mode)+['-DTEST_PRODUCT_CATALOG',
                                '-fno-asynchronous-unwind-tables','-fno-unwind-tables','-c',fixture,'-o',fixtureObject])
                            product(fixtureObject);fixture=fixtureObject
                        run(name+'.build',flags(cc,mode)+[fixture,*extra,*[root/s for s in sources],*libs,'-o',binary]);product(binary)
                        for backend,arguments in ([('select',[]),('native',['native'])] if kind=='tcp' else [('offline',[])]):
                            text=run(name+'.'+backend,[binary,*arguments],opt=='instrumented')
                            assert ('balanced' in text if kind!='callback-shared' else '1024 shared waits' in text)
                            if args.dial and kind=='tcp':assert 'Dial class Future: cases=3 prefixes=96' in text
                            if args.dial_interrupt and kind=='tcp':assert 'Dial interruption: cases=7 prefixes=64' in text
                            if args.interrupt and kind=='callback':assert 'interruptible callbacks: cases=112 prefixes=96' in text
                    except Exception as error:failures.append(dict(Name=name,Error=repr(error)));save()
        # Independently prove the new test detects losing exclusive source
        # cancellation, without changing production sources or shared V5.
        if args.probe:
            report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
            report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
            report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
            report['Complete']=not failures and report['InputsUnchanged'] and report['LogsUnchanged'] and report['ProductsUnchanged']
            save();assert report['Complete'],failures;return
        original=root/'src/runtime/xl6_future_callback_queue.c';body=original.read_text()
        needle='start_body(executor,data,source,true)';assert body.count(needle)==1
        mutant=out/'shared-instead-of-exclusive.c';mutant.write_text(body.replace(needle,'start_body(executor,data,source,false)'),encoding='utf8');product(mutant)
        binary=out/('exclusive-negative'+ext)
        sources=[mutant if root/s==original else root/s for s in report['Plans']['callback']['sources']]
        run('exclusive-negative.build',flags(gcc,['-O0'])+['-I'+str(root/'src/runtime'),
            root/'test/support/future_callback_exclusive_queue.c',*sources,*libs,'-o',binary]);product(binary)
        run('exclusive-negative.refused',[binary],reject=True);report['NegativeControls'].append('exclusive-source-cancellation');save()
        if args.interrupt:
            original=root/'src/runtime/xl6_future_callback_interrupt.c';body=original.read_text(encoding='utf8')
            needle='(void)xrtFutureCancel(source);';assert body.count(needle)==2
            mutant=out/'no-interrupt-source-cancellation.c'
            mutant.write_text(body.replace(needle,'/* deliberately lose the real source cancellation */'),encoding='utf8');product(mutant)
            binary=out/('interrupt-source-negative'+ext)
            sources=[mutant if root/s==original else root/s for s in report['Plans']['callback']['sources']]
            run('interrupt-source-negative.build',flags(gcc,['-O0'])+['-I'+str(root/'src/runtime'),
                root/'test/support/future_callback_interrupt.c',*sources,*libs,'-o',binary]);product(binary)
            run('interrupt-source-negative.refused',[binary],reject=True)
            report['NegativeControls'].append('interrupt-real-source-cancellation');save()
        if args.dial:
            original=root/'src/runtime/xl6_tcp_dial_future.c';body=original.read_text(encoding='utf8')
            assert body.count('xl6ExecutionTcpPromoteDialV1')==2
            mutant=out/'wrong-dial-promotion.c'
            mutant.write_text(body.replace('xl6ExecutionTcpPromoteDialV1','xl6ExecutionTcpPromoteAcceptV1'),encoding='utf8');product(mutant)
            binary=out/('dial-promotion-negative'+ext)
            sources=[mutant if root/s==original else root/s for s in report['Plans']['tcp']['sources']]
            run('dial-promotion-negative.build',flags(gcc,['-O0'])+['-I'+str(root/'src/runtime'),
                root/'test/support/tcp_dial_language_runtime.c',*sources,*libs,'-o',binary]);product(binary)
            run('dial-promotion-negative.refused',[binary],reject=True)
            report['NegativeControls'].append('dial-is-not-accept');save()
        if args.dial_interrupt:
            original=root/'src/runtime/xl6_tcp_dial_interrupt_future.c';body=original.read_text(encoding='utf8')
            needle='xfuture* source=stopped?NULL:byAddresses?';assert body.count(needle)==1
            mutant=out/'submit-already-stopped.c'
            mutant.write_text(body.replace(needle,'xfuture* source=false?NULL:byAddresses?'),encoding='utf8');product(mutant)
            binary=out/('dial-pre-submit-negative'+ext)
            sources=[mutant if root/s==original else root/s for s in report['Plans']['tcp']['sources']]
            run('dial-pre-submit-negative.build',flags(gcc,['-O0'])+['-I'+str(root/'src/runtime'),
                root/'test/support/tcp_dial_interrupt_runtime.c',*sources,*libs,'-o',binary]);product(binary)
            run('dial-pre-submit-negative.refused',[binary],reject=True)
            report['NegativeControls'].append('stopped-before-native-dial-submission');save()
    except Exception as error:failures.append(dict(Name='planning',Error=repr(error)));save()
    report['InputsUnchanged']=all(sha(path)==digest for path,digest in inputs.items())
    report['LogsUnchanged']=all(sha(row['Log'])==row['Sha256'] for row in rows)
    report['ProductsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in products)
    report['Complete']=not failures and all(report[key] for key in ('InputsUnchanged','LogsUnchanged','ProductsUnchanged'))
    save();assert report['Complete'],failures

if __name__=='__main__':main()
