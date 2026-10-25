"""Current-source network generator retirement, not shipping-product acceptance.

Only literal self-owned loopback, numeric resolver input and ordinary language /
ABI / lifetime checks. No DNS probes, security campaigns or source substitution.
The complete compiler and runtime object cohort must come from one frozen build.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','runtime','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--contracts-only',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    assert not out.exists(), 'Use a fresh evidence directory'
    manifest=args.public_directory.resolve()/'build.json'
    build=json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['SourceXrt']
    assert not build.get('RuntimeOnly',False)
    runtime=args.runtime.resolve(); controls=Path(__file__).resolve().parents[1]
    inputs={str(manifest):sha(manifest),str(Path(__file__).resolve()):sha(__file__)}
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(), row['Path']
        inputs[str(Path(row['Path']).resolve())]=row['Sha256'].lower()
    assert inputs.get(str(runtime))==sha(runtime), 'Runtime must match the built cohort'
    driver=Path(build['Driver']); assert sha(driver)==build['DriverSha256'].lower()
    inputs[str(driver)]=sha(driver)
    cc=Path(shutil.which('gcc')).resolve(); inputs[str(cc)]=sha(cc)
    inputs[str(Path(sys.executable).resolve())]=sha(sys.executable)
    windows=os.name=='nt'
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
             ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
           '-I'+str(root/'release'),'-I'+str(root/'release/include'),
           '-I'+str(driver.parent/'include'),'-Wno-unused-function','-Wno-unused-variable',
           '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable',
           '-Wno-unused-const-variable','-Wno-missing-field-initializers','-Wno-pointer-sign',
           '-Wno-format-truncation']
    links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else [
        '-pthread','-ldl','-lm','-Wl,--export-dynamic','-Wl,-rpath,'+str(runtime.parent)]
    if not windows: flags.append('-D_GNU_SOURCE')
    env=dict(os.environ)
    if not windows: env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    out.mkdir(parents=True)
    # These ordinary file fixtures name this private build root explicitly.
    # Directory creation is not a compiler input or source-file modification.
    (root/'build/test').mkdir(parents=True,exist_ok=True)
    if windows: shutil.copy2(runtime,out/runtime.name)
    report=dict(Complete=False,Inputs=inputs,Rows=[],Failures=[],SourceObjectOverrides=False,
                ShippingAccepted=False,FullGoalComplete=False,Scope=__doc__)

    def pin(path):
        path=Path(path).resolve(); digest=sha(path)
        assert str(path) not in inputs or inputs[str(path)]==digest
        inputs[str(path)]=digest
        return path

    def save():
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')

    def run(name,command,marker=None):
        command=list(map(str,command)); actual=command
        if command[0]==str(cc):
            response=out/(name+'.rsp')
            response.write_text('\n'.join('"'+p.replace('\\','/')+'"' for p in command[1:]),encoding='utf8')
            actual=[str(cc),'@'+str(response)]
        log=out/(name+'.log'); started=time.monotonic()
        try:
            with log.open('wb') as stream:
                result=subprocess.run(actual,cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=600)
            code=result.returncode
        except subprocess.TimeoutExpired: code='timeout'
        data=log.read_bytes()
        ok=code==0 and (marker is None or marker.encode() in data)
        report['Rows'].append(dict(Name=name,Command=command,Exit=code,Complete=ok,
                                  Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started))
        save(); print(name,code,flush=True)
        assert ok,(name,str(log))

    def attempt(name,body):
        try: body()
        except Exception as error: report['Failures'].append(dict(Name=name,Error=repr(error))); save()

    layout=pin(controls/'tools/check_network_retirement_layout.py')
    attempt('retired-layout',lambda: run('retired-layout',[sys.executable,layout,'--root',root]))
    units=['xlir_cgen','xlir_network_service_contract','xlir_native_future_map_contract',
           'xlir_network_service_runtime','xlir_future_callback_queue_native']
    if windows: units.append('xlir_future_callback_exclusive')
    for name in units:
        source=pin(controls/'test/cases'/(name+'.c'))
        for header in (controls/'test/cases').glob('xlir_*owner*.h'): pin(header)
        for opt in ('O0','O2'):
            def unit(name=name,source=source,opt=opt):
                binary=out/(name+'-'+opt+('.exe' if windows else ''))
                run(name+'-'+opt+'-build',[cc,*flags,'-'+opt,source,*objects,runtime,*links,'-o',binary])
                run(name+'-'+opt+'-run',[binary])
            attempt(name+'-'+opt,unit)
    if not args.contracts_only:
        fixtures=('stdlib_net_engine','stdlib_net_resolver_numeric','stdlib_net_datagram_io',
                  'stdlib_tcp_managed_async','stdlib_tcp_recv_owned_async','stdlib_net_frame_owned',
                  'stdlib_net_bytes_value','stdlib_fs_async_minimal','stdlib_five_groups_joint',
                  'xlir_network_prefix_external','stdlib_net_fast_path','stdlib_net_send_file')
        for fixture in fixtures:
            source=pin(controls/'test/cases'/(fixture+'.xl'))
            sidecar=[pin(controls/'test/cases/xlir_network_prefix_provider.c')] if fixture=='xlir_network_prefix_external' else []
            def language(fixture=fixture,source=source,sidecar=sidecar):
                prefix=out/fixture; generated=Path(str(prefix)+'.c')
                run(fixture+'-emit',[driver,source,'cgen-default',generated,*sidecar])
                text=generated.read_text(encoding='utf-8-sig')
                assert not any(token in text for token in ('xlir_network_services','xl6_net_engine(',
                    'xl6_net_resolver(','xl6_net_future_map(','xlir_datagram_future_map('))
                if fixture=='xlir_network_prefix_external':
                    assert 'xl6_xrt_tcp_stream_user_hook' in text and 'xl6_xrt_net_runtime_stats' in text
                for opt in ('O0','O2'):
                    run(fixture+'-'+opt+'-strict',[cc,*flags,'-'+opt,'-fsyntax-only',generated])
                for mode in ('run','module','exe'):
                    destination=Path(str(prefix)+('.aot' if not windows else '.aot'))
                    run(fixture+'-'+mode,[driver,source,mode,destination,*sidecar])
                    if mode=='exe':
                        implementation=Path(str(destination)+'.temps')/'xl6_xrt_impl.c'
                        selected=set(re.findall(r'^\s*#define\s+(XRT_MODULE_\w+)',
                                                implementation.read_text(encoding='utf-8-sig'),re.M))
                        if not sidecar: assert 'XRT_MODULE_ALL' not in selected
                        if fixture=='stdlib_net_frame_owned':
                            assert not selected.intersection({'XRT_MODULE_NET_ENGINE','XRT_MODULE_NET_TCP',
                                                              'XRT_MODULE_NET_UDP','XRT_MODULE_TLS'})
                        executable=Path(str(destination)+'.exe') if windows else destination
                        run(fixture+'-aot-run',[executable])
            attempt(fixture,language)
    report['InputsUnchanged']=all(sha(p)==h for p,h in inputs.items())
    expected=1+len(units)*4+(0 if args.contracts_only else len(fixtures)*7)
    report['Complete']=report['InputsUnchanged'] and not report['Failures'] and len(report['Rows'])==expected
    report['ExpectedStages']=expected
    save(); print(json.dumps(dict(Complete=report['Complete'],Failures=report['Failures'])),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':
    raise SystemExit(main())
