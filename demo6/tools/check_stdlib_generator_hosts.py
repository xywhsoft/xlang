"""Portable dedicated generator hosts using current closed IR and real native
providers. Preserve first-outcome and frame-loan runtime assertions/counters,
complete O0/O2 runs, exact selected source closure and optional ASan/UBSan.
No shipping, module-unmap, worker-wide OOM or full53 completion claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true')
    p.add_argument('--family',choices=('first-outcome','frame-loan','close-routes','ownership'))
    a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt');cc=shutil.which('gcc');assert cc
    build,pinned,compared=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    specs=[('first-outcome','xlir_generator_close_first_outcome','xlir_generator_close_first_outcome_adapter.c',
        'xlir_generator_close_first_outcome_contract','generator close first outcome: 900 source executions, 2700 repeated-close sequences, 0 semantic failures; memory checks balanced'),
        ('frame-loan','xlir_generator_frame_loan','xlir_generator_frame_loan_host.c','xlir_generator_frame_loan_contract',
        'generator frame loan:'),
        ('close-routes','xlir_generator_close_routes','xlir_generator_close_routes_adapter.c','xlir_generator_close_routes_contract',
        'generator close routes: 19 executions, 0 semantic failures; memory checks balanced'),
        ('ownership','xlir_generator_ownership','xlir_generator_ownership_host.c','xlir_generator_ownership_contract',
        'Generated generator ownership: 2400 lifecycles, 1000 async transactions, 2000 real suspensions, 1800 running inspection refusals, 16000 inspection probes, 6600 actual failures, exact memory balance')]
    files=pinned|{Path(__file__).resolve(),Path(cc),runtime,root/'test/cases/xlir_artifact_runtime_contract.c'}
    for family,name,host,contract,_ in specs:
        files.update(root/'test/cases'/n for n in (name+'.xl',host,contract+'.c'))
        if family!='ownership':files.add(root/'test/cases'/(name+'_runtime.c'))
    files.update(f for f in (root/'test/cases').glob('xlir_*.h'))
    if a.family:specs=[s for s in specs if s[0]==a.family]
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)];out.mkdir();rows=[];products=[];proofs=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env.update(XL6_SOURCE_NATIVE_ROOT=build['NativeRoot'],XL6_TCC_LIB=str(Path(build['NativeRoot'])/'build/linux/tcc-dist'))
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Proofs=proofs,
        Scope=__doc__,ComparedSourceInputs=compared,SelectedClosureSanitized=a.sanitize,
        SourceProviderOverrides=False,ExplicitSubset=bool(a.family),Full53Acceptance=False,ShippingArtifact=False,ModuleUnmapAcceptance=False)
    def run(name,cmd):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,cmd)),Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        assert r.returncode==0,(name,data[-6000:]);return data.decode(errors='replace')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/xrt'),'-I'+str(root/'lib/xruntime'),'-I'+str(root/'lib/tcc'),
        '-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);ext='.exe' if os.name=='nt' else ''
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    try:
        planner=out/('artifact'+ext);run('artifact-build',[cc,*flags,'-O0',root/'test/cases/xlir_artifact_runtime_contract.c',*objects,runtime,*links,'-o',planner]);product(planner)
        for family,fixture,host,contract,marker in specs:
            tool=out/(family+'-contract'+ext);run(family+'-contract-build',[cc,*flags,'-O0',root/'test/cases'/(contract+'.c'),*objects,runtime,*links,'-o',tool]);product(tool)
            generated=out/(family+'.c');header=out/(family+'.h');ir=out/(family+'.json')
            if family=='ownership':
                run(family+'-generate',[build['Driver'],root/'test/cases'/(fixture+'.xl'),'cgen-default',generated])
                run(family+'-binding',[tool,header])
                run(family+'-ir',[build['Driver'],root/'test/cases'/(fixture+'.xl'),'ir-lowered',ir])
                run(family+'-repeat',[build['Driver'],root/'test/cases'/(fixture+'.xl'),'cgen-default',out/(family+'-repeat.c')])
                run(family+'-binding-repeat',[tool,out/(family+'-repeat.h')])
                run(family+'-ir-repeat',[build['Driver'],root/'test/cases'/(fixture+'.xl'),'ir-lowered',out/(family+'-repeat.json')])
            else:
                run(family+'-generate',[tool,generated,header,ir]);run(family+'-repeat',[tool,out/(family+'-repeat.c'),out/(family+'-repeat.h'),out/(family+'-repeat.json')])
            for suffix in ('c','h','json'):
                first=out/(family+'.'+suffix);repeat=out/(family+'-repeat.'+suffix);assert sha(first)==sha(repeat);product(first);product(repeat)
            native=out/(family+'-native.c');text=run(family+'-plan',[planner,'--emit-native','test/cases/'+fixture+'.xl',native])
            plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plans)==1;sources=plans[0]['Sources']
            raw=native.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
            native.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(native)
            definitions=['-DXLIR_GENERATOR_STORAGE_SOURCE="'+generated.as_posix()+'"','-DXLIR_GENERATOR_LOAN_SOURCE="'+generated.as_posix()+'"']
            if family=='ownership':definitions+=['-DXLIR_GENERATOR_OWNERSHIP_SOURCE="'+generated.as_posix()+'"','-DXLIR_GENERATOR_OWNERSHIP_BINDING="'+header.as_posix()+'"']
            if family=='first-outcome':
                binding=Path(str(header)+'.bind.h');assert binding.exists();product(binding)
                definitions+=['-DXLIR_GENERATOR_STORAGE_BINDING="'+binding.as_posix()+'"']
            configurations=[('O0',[]),('O2',[])]+([('san',['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'])] if a.sanitize else [])
            for opt,extra in configurations:
                level='-O1' if opt=='san' else '-'+opt;sanlinks=['-fsanitize=address,undefined','-no-pie'] if opt=='san' else []
                if opt=='san':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                providers=[]
                for i,s in enumerate([native,*[root/f for f in sources]]):
                    obj=out/(family+'-'+opt+'-provider-'+str(i)+'.o');run(family+'-'+opt+'-provider-'+str(i),[cc,*flags,*extra,level,'-c',s,'-o',obj]);providers.append(obj);product(obj)
                binary=out/(family+'-'+opt+ext)
                run(family+'-'+opt+'-build',[cc,*flags,*extra,level,*definitions,root/'test/cases'/host,
                    *([] if family=='ownership' else [root/'test/cases'/(fixture+'_runtime.c')]),*providers,*systems,*sanlinks,'-o',binary]);product(binary)
                text=run(family+'-'+opt+'-execute',[binary]);assert marker in text and ('balanced' in text or 'exact memory balance' in text)
                proofs.append(dict(Family=family,Optimization=opt,Output=text))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Dedicated generator hosts: complete assertions and native memory balance passed',flush=True)
if __name__=='__main__':main()
