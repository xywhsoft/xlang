"""Verified independent native SSA async spill and zero-copy protected loan.
Closed IR test edit, strict selected O0/O2 native providers, actual ready/pending
Watch/Commit success/reject/cancel/close, checked-copy/allocation fault prefixes.
Not source byref, generator independent spills, shipping or full53 acceptance.
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
    assert not out.exists() and (not a.sanitize or os.name!='nt')
    cc=shutil.which('gcc');assert cc
    build,pinned,compared=verify_public_build(root,public)
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    contract=root/'test/cases/xlir_native_ssa_async_contract.c'
    source=root/'test/cases/stdlib_native_ssa_async.xl'
    host=root/'test/cases/stdlib_native_ssa_async_host.c'
    faults=root/'test/cases/stdlib_native_ssa_async_fault_host.c'
    memory=root/'test/support/module_product_memory.c'
    files=pinned|{Path(__file__).resolve(),Path(cc),runtime,source,contract,host,faults,memory}
    files.update(root/'test/cases'/n for n in ('xlir_multi_await_host_prelude.h','xlir_multi_await_host_support.h'))
    files.add(root/'test/support/module_product_memory.h')
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir();rows=[];products=[];proofs=[];prefixes=[];counts={};env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        # This is an unbundled full-source driver, not the shipping VFS. Use
        # the same genuine TCC support archive already pinned by its build.
        env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Proofs=proofs,
        Prefixes=prefixes,NativeCopies=counts,ComparedSourceInputs=compared,Scope=__doc__,
        IndependentNativeSsaAsync=False,ProtectedLoanZeroExtraCopy=False,GeneratorIndependentSpill=False,
        SourceByref=False,ShippingArtifact=False,Full53Acceptance=False,SelectedClosureSanitized=a.sanitize)
    def run(name,command,bad=False):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=600)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Exit=r.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if bad else r.returncode==0),(name,data[-6000:])
        return data.decode(errors='replace')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(root/'lib/tcc'),
        '-I'+str(root/'lib/xrt'),'-I'+str(root/'lib/xruntime'),
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
            text=run(mode+'-contract',[tool,source,generated,mode,native]);product(generated);product(native)
            assert 'verified=1 mutations='+('4' if mode=='copy' else '0') in text
            selected=[line[7:] for line in text.splitlines() if line.startswith('source:')]
            assert selected and (suppliers is None or suppliers==selected);suppliers=selected
            repeat=out/(mode+'-repeat.c');repeat_native=out/(mode+'-repeat-native.c')
            run(mode+'-repeat',[tool,source,repeat,mode,repeat_native]);assert sha(generated)==sha(repeat) and sha(native)==sha(repeat_native)
            product(repeat);product(repeat_native);programs[mode]=generated;natives[mode]=native
            body=generated.read_text(encoding='utf-8-sig')
            assert 'xl6SlotMapKeyBoxV1(' not in body and 'xl6SlotMapKeyUnboxV1(' not in body
            assert 'XRT_MODULE_ALL' not in native.read_text() and 'XRUNTIME_MODULE_ALL' not in native.read_text()
        assert sha(natives['loan'])==sha(natives['copy'])
        # Compile real selected runtime once per optimization. These sources
        # contain the checked copy/transport/retirement; no provider overrides.
        configurations=[('O0',[]),('O2',[])]+([('san',['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'])] if a.sanitize else [])
        for opt,extra in configurations:
            optflag='-O1' if opt=='san' else '-'+opt
            sanlinks=['-fsanitize=address,undefined','-no-pie'] if opt=='san' else []
            if opt=='san':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            providers=[]
            for i,s in enumerate([natives['copy'],*[root/p for p in suppliers],memory]):
                obj=out/(opt+'-provider-'+str(i)+'.o');run(opt+'-provider-'+str(i),[cc,*flags,*extra,optflag,'-c',s,'-o',obj]);providers.append(obj);product(obj)
            for mode in ('loan','copy'):
                define='-DXLIR_NATIVE_SSA_SOURCE="'+programs[mode].as_posix()+'"'
                fault=out/(mode+'-'+opt+'-fault'+ext)
                run(mode+'-'+opt+'-fault-build',[cc,*flags,*extra,optflag,define,faults,*providers,*systems,*sanlinks,
                    '-Wl,--wrap=xl6SlotMapKeyCopyValueV1','-Wl,--wrap=xrtTypeCopyValue','-o',fault]);product(fault)
                text=run(mode+'-'+opt+'-lifetimes',[fault]);match=re.search(r'lifetimes=20 native-copies=(\d+)',text);assert match
                counts[mode+'-'+opt]=int(match[1])
                for lane in ('copy','copy-error','partial-copy','true-error','oom'):
                    for at in range(1024):
                        text=run(mode+'-'+opt+'-'+lane+'-'+str(at),[fault,lane,at])
                        if 'hit=0' in text:break
                    else:raise AssertionError('incomplete '+mode+' '+lane)
                    assert at>0;prefixes.append(dict(Mode=mode,Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
                for timing,defs in [('ready',[]),('watch',['-DXLIR_MULTI_AWAIT_PENDING']),('commit',['-DXLIR_MULTI_AWAIT_PENDING','-DXLIR_MULTI_AWAIT_AFTER_COMMIT'])]:
                    for terminal,termdefs in [('ok',[]),('reject',['-DXLIR_SSA_REJECT']),('cancel',['-DXLIR_SSA_CANCEL']),('close',['-DXLIR_SSA_CLOSE'])]:
                        name=mode+'-'+opt+'-'+timing+'-'+terminal;binary=out/(name+ext)
                        run(name+'-build',[cc,*flags,*extra,optflag,define,*defs,*termdefs,host,*providers,*systems,*sanlinks,'-o',binary]);product(binary)
                        text=run(name+'-execute',[binary]);assert 'native SSA: finally and ownership balanced' in text
                        if timing!='ready':assert 'every input completed after actual pending Watch registration' in text
                        if timing=='commit':assert 'deferred completion after actual activation Commit' in text
                        proofs.append(dict(Mode=mode,Optimization=opt,Timing=timing,Terminal=terminal,Lifetimes=2))
            assert counts['copy-'+opt]==counts['loan-'+opt]+1
        # Ordinary source regressions run on the same full compiler build.
        for name in ('stdlib_native_borrowed_spill','stdlib_slot_map_async_keys','cgen_async_string_ownership'):
            fixture=root/('test/cases/'+name+'.xl');inputs.append(dict(Path=str(fixture),Sha256=sha(fixture)))
            run(name+'-jit',[build['Driver'],fixture,'run',out/(name+'-jit')])
            run(name+'-module',[build['Driver'],fixture,'module',out/(name+'-module')])
            exe=out/(name+'-aot'+ext)
            run(name+'-aot-build',[build['Driver'],fixture,'exe',out/(name+'-aot') if os.name=='nt' else exe])
            run(name+'-aot-execute',[exe]);product(exe)
        report.update(IndependentNativeSsaAsync=True,ProtectedLoanZeroExtraCopy=True,NativeSources=suppliers)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Native SSA async: independent copy and protected loans verified',flush=True)
if __name__=='__main__':main()
