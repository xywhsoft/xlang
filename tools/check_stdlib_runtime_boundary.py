"""Checked Dict namespace and runtime-created PatternBuilder state boundaries.
Strict C O0/O2, genuine native factory/edit allocation prefixes, IR corruption
refusal, JIT/module/AOT execution, optional selected native ASan/UBSan.
Does not attest the nested compiler SDK, dedicated graph hosts or full53.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import verify_public_build
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public-directory','out'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    root,public,out=a.root.resolve(),a.public_directory.resolve(),a.out.resolve()
    cc=shutil.which('gcc');assert cc and not out.exists() and (not a.sanitize or os.name!='nt')
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver'])
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    cases=('stdlib_dict_namespace_native','stdlib_dict','stdlib_pattern_builder',
           'stdlib_pattern_builder_lifetime','stdlib_pattern_builder_cleanup')
    sources=('test/cases/xlir_pattern_builder_host.c','test/support/pattern_builder_native.c',
        'src/runtime/xl6_pattern_builder.c','src/runtime/xl6_pattern_core.c',
        'src/runtime/xl6_pattern_options.c','src/runtime/xl6_array_input.c','src/runtime/xl6_sequence.c',
        'src/api/xl_foreign_error.c')
    contract=root/'test/cases/xlir_dict_namespace_contract.c'
    manifest,sdk,_=stage_private_sdk(root)
    pinned.update((Path(__file__).resolve(),Path(cc),runtime,contract,manifest))
    pinned.update(root/s for s in sources)
    pinned.update(root/('test/cases/'+n+'.xl') for n in cases)
    pinned.update(Path(row[k]) for row in sdk for k in ('Source','Path'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(pinned)]
    out.mkdir();rows=[];products=[];env=dict(os.environ);ext='.exe' if os.name=='nt' else ''
    if os.name=='nt':shutil.copy2(runtime,out/'xl.dll')
    else:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include',
        '-Ilib/tcc','-I'+str(public/'include'),'-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
        '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation']
    if os.name!='nt':flags+=['-D_GNU_SOURCE']
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    links=systems+([] if os.name=='nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects=[r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        NativeSources=sources,SelectedNativeSanitized=a.sanitize,Full53Acceptance=False,ShippingArtifact=False,Scope=__doc__)
    def run(name,cmd):
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=300)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,cmd)),Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        assert r.returncode==0,(name,data[-6000:]);return data.decode(errors='replace')
    def product(path):products.append(dict(Path=str(path),Sha256=sha(path)))
    try:
        proof=out/('contract'+ext)
        run('contract-build',[cc,*flags,'-O0',contract,*objects,runtime,*links,'-o',proof]);product(proof)
        report['IrContracts']=run('contract-execute',[proof,root/('test/cases/'+cases[0]+'.xl')])
        for opt in ('O0','O2'):
            binary=out/('native-'+opt+ext)
            run('native-'+opt+'-build',[cc,*flags,'-'+opt,*sources,*systems,'-o',binary]);product(binary)
            text=run('native-'+opt+'-execute',[binary])
            assert 'pattern state factory OOM:' in text and '10 first no-hit completions' in text and 'balanced' in text
        for name in cases:
            case=root/('test/cases/'+name+'.xl');generated=out/(name+'.c')
            run(name+'-generate',[driver,case,'cgen-default',generated]);product(generated)
            if name==cases[0]:
                text=generated.read_text();assert 'xl6DictValueRemoveV1' in text and 'xl6_xrt_std_table_remove' not in text
            for opt in ('O0','O2'):
                obj=out/(name+'-'+opt+'.o');run(name+'-'+opt+'-strict-c',[cc,*flags,'-'+opt,'-c',generated,'-o',obj]);product(obj)
            for mode in ('run','module'):run(name+'-'+mode,[driver,case,mode,out/(name+'-'+mode)])
            binary=out/(name+'.exe');run(name+'-aot-build',[driver,case,'exe',out/name if os.name=='nt' else binary])
            run(name+'-aot-run',[binary]);product(binary)
        if a.sanitize:
            sf=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie'];binary=out/'native-san'
            env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            run('native-san-build',[cc,*flags,*sf,'-O1',*sources,*systems,'-fsanitize=address,undefined','-no-pie','-o',binary]);product(binary)
            assert 'pattern state factory OOM:' in run('native-san-execute',[binary])
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'];assert report['Complete']
    finally:
        report['PhaseCount']=len(rows);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Dict namespace and PatternBuilder runtime boundaries verified',flush=True)
if __name__=='__main__':main()
