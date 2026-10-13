"""Checked generator preparation and independent native SSA residents.
Real selected O0/O2 providers, complete/early close, copy/partial/Put/OOM prefixes,
first-error/finally/item rollback, IR corruptions and ordinary JIT/module/AOT.
Synthetic closed-IR SSA views are not a source byref feature. Not full53/shipping.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):parser.add_argument('--'+n,type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');a=parser.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    assert not out.exists() and (not a.sanitize or os.name!='nt');cc=shutil.which('gcc');assert cc
    build,pinned,compared=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    source=root/'test/cases/stdlib_native_ssa_generator.xl';contract=root/'test/cases/xlir_native_ssa_generator_contract.c'
    host=root/'test/cases/stdlib_native_ssa_generator_host.c';memory=root/'test/support/module_product_memory.c'
    files=pinned|{Path(__file__).resolve(),Path(cc),runtime,source,contract,host,memory,root/'test/support/module_product_memory.h'}
    cases=('stdlib_slot_map_generator_keys','stdlib_slot_map_generator_capture','xlir_generator_storage','xlir_generator_ownership',
        'xlir_generator_frame_loan','xlir_generator_close_first_outcome','xlir_generator_launch_allocation','xlir_generator_managed')
    files.update(root/('test/cases/'+n+'.xl') for n in cases)
    legacy=('xlir_generator_storage_contract','xlir_generator_managed_contract','xlir_native_generator_contract',
        'xlir_generator_frame_loan_contract','xlir_generator_close_routes_contract','xlir_generator_close_first_outcome_contract')
    files.update(root/'test/cases'/(n+'.c') for n in legacy)
    files.update(p for p in (root/'test/cases').glob('xlir_generator_*.xl'))
    files.update(p for p in (root/'test/cases').glob('xlir_*.h'))
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)];out.mkdir();rows=[];products=[];prefixes=[];counts={};proofs=[]
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env.update(XL6_SOURCE_NATIVE_ROOT=build['NativeRoot'],XL6_TCC_LIB=str(Path(build['NativeRoot'])/'build/linux/tcc-dist'))
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,
        NativeCopies=counts,Proofs=proofs,ComparedSourceInputs=compared,Scope=__doc__,GeneratorCheckedPreparation=False,
        IndependentNativeSsaGenerator=False,SourceByref=False,Full53Acceptance=False,ShippingArtifact=False,SelectedClosureSanitized=a.sanitize)
    def run(name,cmd):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Exit=r.returncode,Log=str(log),LogSha256=sha(log)));assert r.returncode==0,(name,data[-6000:])
        return data.decode(errors='replace')
    def product(p):products.append(dict(Path=str(p),Sha256=sha(p)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),'-I'+str(root/'lib/xrt'),'-I'+str(root/'lib/xruntime'),
        '-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    ext='.exe' if os.name=='nt' else ''
    try:
        tool=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',tool]);product(tool)
        suppliers=None;programs={};natives={}
        for mode in ('loan','copy'):
            generated=out/(mode+'.c');native=out/(mode+'-native.c')
            text=run(mode+'-contract',[tool,source,generated,mode,native]);assert 'verified=1 mutations=3' in text
            selected=[s[7:] for s in text.splitlines() if s.startswith('source:')];assert selected and (suppliers is None or suppliers==selected);suppliers=selected
            repeat=out/(mode+'-repeat.c');rn=out/(mode+'-repeat-native.c');run(mode+'-repeat',[tool,source,repeat,mode,rn])
            assert sha(generated)==sha(repeat) and sha(native)==sha(rn)
            for p in (generated,native,repeat,rn):product(p)
            programs[mode]=generated;natives[mode]=native
            assert 'XRT_MODULE_ALL' not in native.read_text() and 'XRUNTIME_MODULE_ALL' not in native.read_text()
        assert sha(natives['loan'])==sha(natives['copy'])
        configurations=[('O0',[]),('O2',[])]+([('san',['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'])] if a.sanitize else [])
        for opt,extra in configurations:
            optflag='-O1' if opt=='san' else '-'+opt;sanlinks=['-fsanitize=address,undefined','-no-pie'] if opt=='san' else []
            if opt=='san':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            providers=[]
            for i,s in enumerate([natives['copy'],*[root/p for p in suppliers],memory]):
                obj=out/(opt+'-provider-'+str(i)+'.o');run(opt+'-provider-'+str(i),[cc,*flags,*extra,optflag,'-c',s,'-o',obj]);providers.append(obj);product(obj)
            for mode in ('loan','copy'):
                binary=out/(mode+'-'+opt+ext);name=mode+'-'+opt
                run(name+'-build',[cc,*flags,*extra,optflag,'-DXLIR_NATIVE_SSA_SOURCE="'+programs[mode].as_posix()+'"',host,*providers,*systems,*sanlinks,
                    '-Wl,--wrap=xl6SlotMapKeyCopyValueV1','-Wl,--wrap=xrtTypeCopyValue','-Wl,--wrap=xl6GeneratorRuntimeV1Get','-o',binary]);product(binary)
                for close,args in [('complete',[]),('early',['early'])]:
                    text=run(name+'-'+close,[binary,*args]);m=re.search(r'lifetimes=20 early=\d native-copies=(\d+)',text);assert m
                    counts[name+'-'+close]=int(m[1]);proofs.append(dict(Mode=mode,Optimization=opt,Close=close,Lifetimes=20))
                for lane in ('copy','copy-error','partial-copy','true-error','put','put-error','oom'):
                    for at in range(1024):
                        text=run(name+'-'+lane+'-'+str(at),[binary,lane,at])
                        if 'hit=0' in text:break
                    else:raise AssertionError('incomplete '+name+' '+lane)
                    assert at>0;prefixes.append(dict(Mode=mode,Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
            for close in ('complete','early'):assert counts['copy-'+opt+'-'+close]==counts['loan-'+opt+'-'+close]+1
        for name in cases:
            fixture=root/('test/cases/'+name+'.xl');generated=out/(name+'-regression.c')
            run(name+'-cgen',[build['Driver'],fixture,'cgen-default',generated]);product(generated)
            for opt in ('O0','O2'):
                obj=out/(name+'-'+opt+'.o');run(name+'-'+opt+'-strict',[cc,*flags,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            if name not in ('stdlib_slot_map_generator_keys','stdlib_slot_map_generator_capture'):continue
            run(name+'-jit',[build['Driver'],fixture,'run',out/(name+'-jit')])
            run(name+'-module',[build['Driver'],fixture,'module',out/(name+'-module')])
            exe=out/(name+'-aot'+ext);run(name+'-aot-build',[build['Driver'],fixture,'exe',out/(name+'-aot') if os.name=='nt' else exe]);run(name+'-aot-execute',[exe]);product(exe)
        for name in legacy:
            exe=out/(name+ext);run(name+'-build',[cc,*flags,'-O0',root/'test/cases'/(name+'.c'),*objects,runtime,*links,'-o',exe]);product(exe)
            args=[root/'test/cases/stdlib_slot_map_generator_capture.xl'] if name=='xlir_native_generator_contract' else [out/(name+'.c'),out/(name+'.h'),out/(name+'.json')]
            run(name+'-execute',[exe,*args])
            for p in out.glob(name+'.*'):
                if p.suffix in ('.c','.h','.json'):product(p)
        report.update(GeneratorCheckedPreparation=True,IndependentNativeSsaGenerator=True,NativeSources=suppliers,
            DedicatedGeneratorHostsPending=True,LegacyIrContracts=list(legacy))
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Generator preparation/SSA: owned rollback and finally verified',flush=True)
if __name__=='__main__':main()
