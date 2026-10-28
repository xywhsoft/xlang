"""Native Linux compiled-package acceptance; keep source/sanitizer and shipping lanes distinct."""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import time
from xlir_public_build_inputs import sha, verify_public_build


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native-root','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--public-directory',type=Path)
    parser.add_argument('--source-only',action='store_true')
    args=parser.parse_args()
    root,native,out=args.root.resolve(),args.native_root.resolve(),args.out.resolve()
    assert os.name!='nt' and not out.exists()
    assert not args.source_only or args.public_directory
    out.mkdir(parents=True)
    cc=shutil.which('gcc'); assert cc
    cli=native/'release/xl'; assert cli.is_file()
    tcc=native/'build/linux/tcc-dist'
    env=dict(os.environ,XLANG_PACKAGE_PATH=str(root/'test/fixtures'),
        ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    files={Path(__file__).resolve(),Path(cc),cli}
    files.update((root/'test/cases').glob('*compiled_package*'))
    files.add(root/'test/cases/xlir_artifact_runtime_contract.c')
    for parent in ('release/include','release/examples/embedded_package'):
        files.update(p for p in (root/parent).rglob('*') if p.is_file())
    for parent in (root/'test/fixtures').glob('compiled_*'):
        files.update(p for p in parent.rglob('*') if p.is_file())
    files.update(native/'release'/name for name in ('libxl.so','libxlc.so'))
    rows=[]
    report=dict(Complete=False,InputsUnchanged=False,SourceAcceptance=False,ShippingAcceptance=False,
        AllSourceProvidersInstrumented=False,ShippingProvidersInstrumented=False,Rows=rows,Scope=__doc__)
    def run(name,command,cwd=root,expected=0):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=600)
        log=out/(name+'.log');log.write_bytes(result.stdout)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Expected=expected,
                         Log=str(log),LogSha256=sha(log),Seconds=time.monotonic()-start))
        assert result.returncode==expected,(name,result.stdout[-6000:].decode(errors='replace'))
        return result.stdout
    def build(name,source,binary):
        run(name,[cli,'build',root/source,'-o',binary]);assert binary.is_file()
    try:
        if args.public_directory:
            public=args.public_directory.resolve()
            cohort,pinned,compared=verify_public_build(root,public);files.update(pinned)
            report['ComparedSources']=compared
        files=sorted(files);inputs=[dict(Path=str(p),Sha256=sha(p)) for p in files]
        report['Inputs']=inputs
        if args.public_directory:
            objects=[row['Path'] for row in cohort['Objects'] if Path(row['Path']).stem not in
                     ('xlir_public_source_driver','xlir_public_source_linux_driver')]
            flags=cohort['CompileFlags']
            links=['-L'+str(native/'release'),'-lxl','-Wl,-rpath,'+str(native/'release'),'-pthread','-ldl','-lm']
            if cohort.get('Sanitize'):links+=['-no-pie']
            compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
                    '-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers']
            binary=out/'source-ir'
            run('source-ir-build',[cc,*flags,*compat,root/'test/cases/compiled_package_ir_contract.c',
                                  *objects,*links,'-o',binary])
            generated=out/'consumer.c'
            run('source-ir',[binary,generated,out/'metadata.json'])
            for opt in ('O0','O2'):
                run('consumer-strict-'+opt,[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,*compat,
                    '-I'+str(public/'include'),'-I'+str(root/'release/include'),'-c',generated,'-o',out/(opt+'.o')])
            contract=out/'artifact-plan'
            run('artifact-plan-build',[cc,*flags,*compat,root/'test/cases/xlir_artifact_runtime_contract.c',
                                      *objects,*links,'-o',contract])
            run('artifact-plan',[contract,root/'test/cases/cgen_basic.xl'])
            report['SourceAcceptance']=True
            report['AllSourceProvidersInstrumented']=cohort.get('Sanitize',False)
            if args.source_only:
                verify_public_build(root,public)
                report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
                assert report['InputsUnchanged']
                report['Complete']=True
                print('Linux compiled package source/sanitizer acceptance passed',flush=True)
                return
        for library in ('xl','xlc'):
            dll=native/'release'/('lib'+library+'.so')
            for opt in ('O0','O2'):
                cwd=out/(library+'-'+opt);cwd.mkdir()
                poison=cwd/'__xl_compiled__';poison.mkdir()
                shutil.copy2(root/'test/cases/compiled_package_poison.c',poison/'leaf.c')
                binary=cwd/'registry'
                run(library+'-'+opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,
                    '-I'+str(root),'-I'+str(root/'release'),root/'test/cases/compiled_package_registry.c',
                    dll,'-Wl,-rpath,'+str(dll.parent),'-o',binary])
                run(library+'-'+opt+'-registry',[binary,dll,tcc],cwd)
        binary=out/'compiler';dll=native/'release/libxlc.so'
        run('compiler-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-I'+str(root/'release'),
            root/'test/cases/compiled_package_compiler.c',dll,'-Wl,-rpath,'+str(dll.parent),'-o',binary])
        run('compiler',[binary,dll,tcc,out/'compiler-consumer.c'])
        host,baseline,exposure=out/'host',out/'baseline',out/'exposure'
        build('host-build','test/cases/compiled_package_host.xl',host)
        build('baseline-build','test/cases/compiled_package_baseline.xl',baseline)
        baseline_source=(root/'test/cases/compiled_package_baseline.xl').read_text().strip()
        exposure_source=(root/'test/cases/compiled_package_exposure.xl').read_text().strip()
        assert exposure_source.replace('assert engine.importPackage(xlang.compiledPackage("compiled_demo"))\n','')==baseline_source
        build('exposure-build','test/cases/compiled_package_exposure.xl',exposure)
        run('baseline',[baseline]);run('exposure',[exposure])
        for name,fragment in (('dynamic_name','constant package name'),('missing_import','imported by the host'),
                              ('static_state','mutable static')):
            data=run(name,[cli,'build',root/('test/cases/bad_compiled_package_'+name+'.xl'),'-o',out/name],expected=1)
            assert fragment.encode() in data
        data=run('provider-lifetime',[cli,'run',root/'test/cases/compiled_package_host.xl'],expected=1)
        assert b'process-resident' in data
        deploy=out/'deploy';deploy.mkdir();shutil.copy2(host,deploy/'host')
        shutil.copy2(root/'test/cases/compiled_package_script.xl',deploy)
        env['XLANG_PACKAGE_PATH']=''
        data=run('source-free-host',[deploy/'host'],deploy)
        assert b'compiled_package_script=ok' in data and b'compiled_package_host=ok' in data
        assert len(list(deploy.iterdir()))==2
        env['XLANG_PACKAGE_PATH']=str(root/'test/fixtures/compiled_package_decoy')
        data=run('registered-before-disk',[deploy/'host'],deploy)
        assert b'compiled_package_host=ok' in data
        elf=run('host-elf',['readelf','-d',host]).decode()
        needed=re.findall(r'Shared library: \[([^]]+)\]',elf)
        assert needed and all(name in ('libc.so.6','libm.so.6','libmvec.so.1','libdl.so.2','libpthread.so.0','libgcc_s.so.1','ld-linux-x86-64.so.2') for name in needed),needed
        env['XLANG_PACKAGE_PATH']=str(root/'release/examples/embedded_package/packages')
        example=out/'example';build('example-build','release/examples/embedded_package/host.xl',example)
        deploy=out/'example-deploy';deploy.mkdir();shutil.copy2(example,deploy/'host')
        shutil.copy2(root/'release/examples/embedded_package/external.xl',deploy)
        env['XLANG_PACKAGE_PATH']='';data=run('example-source-free',[deploy/'host'],deploy)
        assert b'embedded_package=ok' in data and len(list(deploy.iterdir()))==2
        if args.public_directory:verify_public_build(root,public)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']
        report.update(Complete=True,ShippingAcceptance=True,HostBytes=exposure.stat().st_size,
            BaselineBytes=baseline.stat().st_size,DeltaBytes=exposure.stat().st_size-baseline.stat().st_size,
            DeltaPercent=100*(exposure.stat().st_size-baseline.stat().st_size)/baseline.stat().st_size,
            SystemLibraries=needed)
    except Exception as error:
        report['Error']=repr(error);raise
    finally:
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Linux compiled packages passed:',len(rows),'stages',flush=True)


if __name__=='__main__':
    main()
