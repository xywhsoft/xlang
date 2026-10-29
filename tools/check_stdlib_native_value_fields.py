"""Closed native owning values in class fields: exact IR classification,
checked projection, temporary receivers, independent results and scope cleanup.
Strict independent C O0/O2, caller-thread fault prefixes, real bad compiler and
runtime controls, JIT/module/AOT and selected Linux ASan/UBSan. Not independent
borrowed native SSA-copy spills, WaitQueue or full53/shipping acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc,nm=shutil.which('gcc'),shutil.which('nm');assert cc and nm and not out.exists() and (not a.sanitize or os.name!='nt')
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'));assert build['SourceXrt'] and not build['RuntimeOnly']
    driver=Path(build['Driver']);runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    sources=[root/'test/cases/stdlib_native_value_fields.xl',root/'test/cases/stdlib_native_borrowed_spill.xl']
    host=root/'test/cases/stdlib_native_value_fields_host.c';contract=root/'test/cases/xlir_native_value_fields_contract.c'
    artifact=root/'test/cases/xlir_artifact_runtime_contract.c';memory=root/'test/support/module_product_memory.c'
    manifest,sdk,skipped=stage_private_sdk(root)
    files={Path(__file__).resolve(),root/'tools/check_stdlib_slot_map.py',public/'build.json',Path(cc),Path(nm),runtime,driver,host,contract,artifact,memory,manifest,*sources}
    files.update(Path(r[k]) for r in sdk for k in ('Source','Path'))
    for r in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(r['Path'])==r['Sha256'].lower(),r['Path'];files.add(Path(r['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();rows=[];products=[];prefixes=[];plans=[];controls=[];env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,Plans=plans,NegativeControls=controls,
        NativeValueFieldReads=False,Sanitized=a.sanitize,SelectedNativeClosureInstrumented=a.sanitize,
        FaultPrefixScope='caller-thread only; full synchronous field fixture, no executor injection',
        Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Ilib/tcc','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),'-DXL_STATIC_EMBED',
        '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)]);ext='.exe' if os.name=='nt' else ''
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    def product(f):products.append(dict(Path=str(f),Sha256=sha(f)))
    def run(name,cmd,bad=False):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(Name=name,Exit=r.returncode,ExpectedRejection=bad,Log=str(log),LogSha256=sha(log)))
        assert (r.returncode!=0 if bad else r.returncode==0),(name,(r.stdout+r.stderr)[-6000:])
        if bad:assert b'assert' in (r.stdout+r.stderr).lower(),(name,r.returncode)
        return (r.stdout+r.stderr).decode(errors='replace')
    try:
        proof=out/('contract'+ext);run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);product(proof)
        report['IrContracts']=run('contract-execute',[proof,*sources])
        bad=out/'missing-native-field.c';body=(root/'src/ir/xl6_ir.c').read_text(encoding='utf-8-sig')
        old='    case XL6_IR_C_ABI_NATIVE_VALUE:\n        return xl6IrNativeValuePlanForType(resultType)\n            ? XL6_IR_MEMBER_ABI_CLASS_FIELD_EXACT_VALUE : XL6_IR_MEMBER_ABI_NONE;\n'
        assert body.count(old)==1;bad.write_text(body.replace(old,''),encoding='utf-8');obj=out/'missing-native-field.o';binary=out/('missing-native-field'+ext)
        run('bad-compiler-build',[cc,*flags,'-Isrc/ir','-O0','-c',bad,'-o',obj]);replaced=[obj if Path(f).name=='xl6_ir.o' else f for f in objects]
        run('bad-compiler-link',[cc,*flags,'-O0',contract,*replaced,runtime,*links,'-o',binary]);run('bad-compiler-refused',[binary,*sources],True)
        for f in (bad,obj,binary):product(f)
        controls.append('missing-native-field-classification')
        planner=out/('artifact-plan'+ext);run('planner-build',[cc,*flags,'-O0',artifact,*objects,runtime,*links,'-o',planner]);product(planner)
        for tag,source in enumerate(sources):
            tag=str(tag);generated=out/(tag+'-program.c');repeat=out/(tag+'-repeat.c')
            run(tag+'-generate',[driver,source,'cgen-default',generated]);run(tag+'-repeat',[driver,source,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
            assert 'xl6SlotMapKeyUnboxV1(' in generated.read_text(encoding='utf-8-sig')
            for f in (generated,repeat):product(f)
            implementation=out/(tag+'-native.c');text=run(tag+'-plan',[planner,'--emit-native',source.as_posix(),implementation])
            plan=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')];assert len(plan)==1
            suppliers=plan[0]['Sources'];plans.append(dict(Fixture=tag,Sources=suppliers))
            assert 'src/runtime/xl6_slot_map_key_value.c' in suppliers
            raw=implementation.read_text();assert 'XRT_MODULE_ALL' not in raw and 'XRUNTIME_MODULE_ALL' not in raw
            implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw);product(implementation)
            options=['-DXL6_NATIVE_FIELD_HAS_MAIN'] if tag=='0' else []
            for opt in ('O0','O2'):
                providers=[]
                for i,s in enumerate([implementation,*[root/f for f in suppliers],memory]):
                    obj=out/(tag+'-'+opt+'-'+str(i)+'.o');run(tag+'-'+opt+'-provider-'+str(i),[cc,*flags,'-'+opt,'-c',s,'-o',obj]);providers.append(obj);product(obj)
                binary=out/(tag+'-'+opt+ext);run(tag+'-'+opt+'-link',[cc,*flags,'-'+opt,*options,'-include',generated,host,*providers,*systems,'-o',binary]);run(tag+'-'+opt+'-lifetimes',[binary]);product(binary)
                assert not re.search(r'\bxlcCreateContext\b',run(tag+'-'+opt+'-symbols',[nm,'--defined-only',binary]))
                if tag=='0':
                    fault=out/(opt+'-fault'+ext);wrappers=('xl6SlotMapKeyUnboxV1','xrtTypeCopyValue')
                    run(opt+'-fault-link',[cc,*flags,'-'+opt,*options,'-DXL6_NATIVE_FIELD_FAULTS','-include',generated,host,*providers,*systems,*['-Wl,--wrap='+f for f in wrappers],'-o',fault]);product(fault)
                    for lane in ('project','typed-copy','typed-partial','typed-diagnostic','oom'):
                        for at in range(2048):
                            text=run(opt+'-'+lane+'-'+str(at),[fault,lane,at])
                            if 'hit=0' in text:break
                        else:raise AssertionError('unfinished fault prefix '+lane)
                        prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
                    if opt=='O0':
                        bad=out/'empty-native-projection.c';body=(root/'src/runtime/xl6_slot_map_key_value.c').read_text(encoding='utf-8-sig');old='return xl6NativeLeafValueUnboxV1(value,&profile,output);';assert body.count(old)==1
                        bad.write_text(body.replace(old,'return true;'),encoding='utf-8');obj=out/'empty-native-projection.o';binary=out/('empty-native-projection'+ext)
                        run('bad-runtime-build',[cc,*flags,'-Isrc/runtime','-O0','-c',bad,'-o',obj]);replaced=providers.copy();replaced[1+suppliers.index('src/runtime/xl6_slot_map_key_value.c')]=obj
                        run('bad-runtime-link',[cc,*flags,'-O0',*options,'-include',generated,host,*replaced,*systems,'-o',binary]);run('bad-runtime-refused',[binary],True)
                        for f in (bad,obj,binary):product(f)
                        controls.append('empty-native-projection')
            run(tag+'-jit',[driver,source,'run',out/(tag+'-jit')]);run(tag+'-module',[driver,source,'module',out/(tag+'-module')])
            exe=out/(tag+'-aot.exe');run(tag+'-aot-build',[driver,source,'exe',out/(tag+'-aot') if os.name=='nt' else exe]);run(tag+'-aot-execute',[exe]);product(exe)
            if a.sanitize:
                sf=flags+['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];sl=['-fsanitize=address,undefined','-no-pie'];env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
                providers=[]
                for i,s in enumerate([implementation,*[root/f for f in suppliers],memory]):
                    obj=out/(tag+'-san-'+str(i)+'.o');run(tag+'-san-provider-'+str(i),[cc,*sf,'-O1','-c',s,'-o',obj]);providers.append(obj);product(obj)
                binary=out/(tag+'-san');run(tag+'-san-link',[cc,*sf,'-O1',*options,'-include',generated,host,*providers,*systems,*sl,'-o',binary]);run(tag+'-san-execute',[binary]);product(binary)
        report.update(NativeValueFieldReads=True,LifetimesPerFixturePerOptimization=200)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs);report['Complete']=report['InputsUnchanged']
    finally:report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'];print('Native class fields, exact ownership and fault cleanup: verified',flush=True)
if __name__=='__main__':main()
