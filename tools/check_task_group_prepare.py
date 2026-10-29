"""Check native TaskGroup preparation and actual production wait dependencies, not automatic language collector admission.

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
    parser.add_argument('--tcc',type=Path)
    parser.add_argument('--catalog',type=Path)
    parser.add_argument('--catalog-report',type=Path)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();assert not out.exists()
    assert not args.tcc or (os.name=='nt' and args.public_directory and args.catalog and args.catalog_report)
    cc=shutil.which('gcc');assert cc
    fixture=root/'test/support/task_group_prepare_native.c'
    oracle=root/'test/cases/xlir_future_wait_oracle.c'
    pinned={Path(__file__).resolve(),Path(sys.executable),Path(cc),fixture,oracle}
    if args.tcc:pinned.update((args.tcc.resolve(),Path('C:/Windows/System32/ucrtbase.dll'),args.catalog.resolve(),args.catalog_report.resolve()))
    for folder in ('lib/xrt','lib/xruntime','release/include'):
        pinned.update(path for path in (root/folder).rglob('*') if path.is_file())
    linked=None
    if args.public_directory:
        linked,more,compared=verify_public_build(root,args.public_directory);pinned.update(more)
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(parents=True);rows=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Failures=failures,
        NativeGroupPreparationAcceptance=False,TaskGroupGraphAcceptance=False,Full53Acceptance=False,ShippingAcceptance=False)
    flags=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(root)]
    if os.name!='nt':flags+=['-D_GNU_SOURCE','-pthread']
    suffix='.exe' if os.name=='nt' else ''
    def run(name,command,expect_marker=False):
        command=list(map(str,command));process=subprocess.run(command,cwd=root,capture_output=True,timeout=240)
        data=process.stdout+process.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=command,Exit=process.returncode,Log=str(log),LogSha256=sha(log)))
        assert process.returncode==0,(name,data[-5000:].decode(errors='replace'))
        if expect_marker:assert b'TaskGroup preparation: 32 real nested/duplicate/ALL/ANY/open/feeder graphs' in data and b'full allocation ledger balanced' in data
        print(name+': '+str(process.returncode),flush=True)
    try:
        for product in ('xrt','xruntime'):
            defines=['-DXL6_TASK_GROUP_PREPARE_XRUNTIME'] if product=='xruntime' else []
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
            run('source-linked.build',[cc,*flags,'-DXL6_TASK_GROUP_PREPARE_XRUNTIME','-DXL6_TASK_GROUP_PREPARE_LINKED','-O2',
                fixture,*objects,*libraries,'-o',binary]);run('source-linked.run',[binary],True)
            solver_objects=[path for path in objects if Path(path).stem!='xl6_future_wait']
            oracle_binary=out/('wait-oracle'+suffix)
            run('wait-oracle.build',[cc,*flags,'-DXL_STATIC_EMBED','-I'+str(root/'release'),'-I'+str(root/'release/include'),'-O2',
                oracle,*solver_objects,*libraries,'-o',oracle_binary]);run('wait-oracle.run',[oracle_binary])
            assert b'66064 exhaustive graphs, 197648 root decisions' in (out/'wait-oracle.run.log').read_bytes()
            report['IndependentWaitOracleAcceptance']=True
            if args.tcc:
                catalog_report=json.loads(args.catalog_report.read_text(encoding='utf-8-sig'))
                assert catalog_report['Complete'] and catalog_report['PublicBuildVerified'] and not catalog_report['RegistryErrors']
                assert catalog_report['ComparedSources']==compared
                catalog=[json.loads(line) for line in args.catalog.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
                entries={row['Id']:row for row in catalog};assert len(entries)==len(catalog)==catalog_report['ComponentCount']
                selected=[row for row in catalog if row['Source']=='src/runtime/xl6_future_wait.c'];assert len(selected)==1
                closure=selected[0];assert not closure['ClosureHostAbi'] and closure['Id'] in closure['Closure']
                resident=[entries[index]['Source'] for index in closure['Closure']]
                # An included C provider (foreign_error) is a real hashed
                # build input even when it is not a standalone compiler TU.
                assert all(name in compared and (root/name).resolve() in pinned for name in resident)
                assert all(not entries[index]['HostAbi'] for index in closure['Closure'])
                tcc_binary=out/'production-wait-tcc.exe'
                run('production-wait-tcc.build',[args.tcc,*flags,'-DXL_STATIC_EMBED','-I'+str(root/'release'),'-I'+str(root/'release/include'),
                    '-DXL6_TASK_GROUP_PREPARE_XRUNTIME','-DXL6_TASK_GROUP_PREPARE_LINKED','-DXL6_TASK_GROUP_PREPARE_KERNEL',fixture,*resident,
                    'C:/Windows/System32/ucrtbase.dll',*libraries[1:],'-o',tcc_binary]);run('production-wait-tcc.run',[tcc_binary],True)
                assert b'96 real decisions; no mock providers' in (out/'production-wait-tcc.run.log').read_bytes()
                report['TccDeclaredClosureAcceptance']=True;report['TccComponentCount']=len(resident);report['TccHostAbi']=0
                report['TccSources']=resident
            report['ComparedSources']=compared;report['FullSourceLinkedAcceptance']=True
            report['ProductionWaitSolverAcceptance']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,NativeGroupPreparationAcceptance=True)
    except Exception as error:failures.append(repr(error));print(repr(error),file=sys.stderr)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
