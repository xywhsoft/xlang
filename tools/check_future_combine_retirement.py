"""Check native FutureCombine and TaskGroup cyclic retirement, not automatic language collector admission.

Build the actual core and xruntime single headers, not canonical-XRT products.
Optional full-source objects are checked against their recorded inputs before
linking. No language owning domain or collector protocol is enabled by this test.
"""
from pathlib import Path
import argparse,json,os,shutil,subprocess,sys
from xlir_public_build_inputs import sha,verify_public_build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--public-directory',type=Path)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    cc=shutil.which('gcc');assert cc
    fixture=root/'test/support/future_combine_retirement_native.c'
    pinned={Path(__file__).resolve(),Path(sys.executable),Path(cc),fixture}
    for folder in ('lib/xrt','lib/xruntime','release/include'):
        pinned.update(path for path in (root/folder).rglob('*') if path.is_file())
    linked=None
    if args.public_directory:
        linked,more,compared=verify_public_build(root,args.public_directory);pinned.update(more)
    fixtures=[root/'test/cases'/name for name in ('stdlib_future_aggregate_array_only.xl','stdlib_future_aggregate_variadic_only.xl')]
    if linked:pinned.update(fixtures)
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);rows=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Failures=failures,LanguageAggregateRegressionAcceptance=False,
        NativeCyclicRetirementAcceptance=False,TaskGroupGraphAcceptance=False,Full53Acceptance=False,ShippingAcceptance=False)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE','-pthread']
    suffix='.exe' if os.name=='nt' else ''
    environment=dict(os.environ)
    if linked and os.name!='nt':environment['XL6_SOURCE_NATIVE_ROOT']=linked['NativeRoot']
    def run(name,command,expect_marker=False,directory=None):
        command=list(map(str,command));process=subprocess.run(command,cwd=directory or root,env=environment,capture_output=True,timeout=240)
        data=process.stdout+process.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=process.returncode,Log=str(log),LogSha256=sha(log)))
        assert process.returncode==0,(name,data[-5000:].decode(errors='replace'))
        if expect_marker:assert b'Combine retirement: 144 real ALL/ANY/RACE cycles' in data and b'full ledger balanced' in data
        print(name+': '+str(process.returncode),flush=True)
    try:
        for product in ('xrt','xruntime'):
            defines=['-DXL6_COMBINE_RETIREMENT_XRUNTIME'] if product=='xruntime' else []
            for opt in ('O0','O2'):
                name=product+'.'+opt;binary=out/(name+suffix)
                run(name+'.build',[cc,*flags,*defines,'-'+opt,fixture,'-o',binary])
                run(name+'.run',[binary],True)
        if linked:
            objects=[row['Path'] for row in linked['Objects'] if Path(row['Path']).stem not in
                ('xlir_public_source_driver','xlir_public_source_linux_driver')]
            runtime=root/'release/xl.dll' if os.name=='nt' else Path(linked['NativeRoot'])/'release/libxl.so'
            libraries=[str(runtime)]
            if os.name=='nt':
                libraries+=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'];shutil.copy2(runtime,out/runtime.name)
            else:libraries+=['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
            binary=out/('source-linked'+suffix)
            run('source-linked.build',[cc,*flags,'-DXL6_COMBINE_RETIREMENT_XRUNTIME','-DXL6_COMBINE_RETIREMENT_LINKED','-O2',
                fixture,*objects,*libraries,'-o',binary]);run('source-linked.run',[binary],True)
            report['ComparedSources']=compared;report['FullSourceLinkedAcceptance']=True
            strict=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
                '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
            moved=out/'relocated';moved.mkdir()
            for source in fixtures:
                name=source.stem;generated=out/(name+'.c');repeat=out/(name+'.repeat.c')
                run(name+'.cgen',[linked['Driver'],source,'cgen-default',generated])
                run(name+'.repeat',[linked['Driver'],source,'cgen-default',repeat]);assert sha(generated)==sha(repeat)
                run(name+'.strict',[cc,*flags,*strict,'-I'+str(args.public_directory/'include'),'-I'+str(root/'release'),
                    '-I'+str(root/'release/include'),'-O2','-fsyntax-only',generated])
                for mode in ('run','module'):run(name+'.'+mode,[linked['Driver'],source,mode,out/(name+'.'+mode)])
                target=out/(name+'.aot'+suffix);run(name+'.aot.build',[linked['Driver'],source,'exe',target])
                actual=Path(str(target)+'.exe') if os.name=='nt' and not target.exists() else target
                relocated=moved/(name+suffix);shutil.copy2(actual,relocated)
                run(name+'.aot.moved',[relocated],directory=moved)
            report['LanguageAggregateRegressionAcceptance']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,NativeCyclicRetirementAcceptance=True)
    except Exception as error:failures.append(repr(error));print(repr(error),file=sys.stderr)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
