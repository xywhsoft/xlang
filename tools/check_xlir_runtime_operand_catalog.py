"""Compiled RuntimeOp effects and current-source resident Thread public gate.
The source compiler may be reused only with byte-identical production inputs.
This is not WaitQueue, shipping SDK, dlclose or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys

CASES=('xlir_thread_resource_abi','cgen_thread','cgen_thread_default',
    'stdlib_thread_resident','stdlib_thread_collections','stdlib_thread_artifact_absent')

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--catalog-only',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); public=args.public_directory.resolve(); out=args.out.resolve()
    assert not out.exists(); windows=os.name=='nt'
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig'))
    assert build.get('Complete',windows) and build['SourceXrt'] and not build['RuntimeOnly']
    driver=Path(build['Driver']); cc=shutil.which('gcc'); assert cc
    source_name='xlir_public_source_driver.c' if windows else 'xlir_public_source_linux_driver.c'
    driver_source=[Path(row['Path']) for row in build['Inputs'] if Path(row['Path']).name==source_name]
    assert len(driver_source)==1; compiled_root=driver_source[0].parents[2]
    compiled_sources={Path(name) if Path(name).is_absolute() else compiled_root/name for name in build['Sources']}
    pinned={Path(__file__).resolve(),public/'build.json',Path(cc),driver}
    production=[]
    for row in build['Inputs']+build['Headers']+build['Objects']:
        path=Path(row['Path']); assert sha(path)==row['Sha256'].lower(),str(path); pinned.add(path)
        if path.is_relative_to(compiled_root):
            relative=path.relative_to(compiled_root)
            if path in compiled_sources or relative.parts[0] in ('src','lib','res','include') or relative.parts[:2]==('release','include'):
                current=root/relative
                assert path.read_bytes().replace(b'\r\n',b'\n')==current.read_bytes().replace(b'\r\n',b'\n'),str(current)
                pinned.add(current); production.append(relative.as_posix())
    assert 'src/ir/xl6_ir.c' in production and 'src/ir/xl6_ir_ownership.c' in production
    assert sha(driver)==build['DriverSha256'].lower()
    runtime=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.add(runtime)
    objdump=shutil.which('objdump'); assert objdump; pinned.add(Path(objdump))
    pinned.update(root/'test/cases'/(name+'.xl') for name in CASES)
    pinned.update(root/path for path in ('test/cases/xlir_runtime_operand_catalog.c',
        'test/cases/xlir_thread_public_plan.c','tools/check_stdlib_thread_language.py'))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)]
    out.mkdir(); rows=[]; products=[]; env=dict(os.environ)
    if windows: shutil.copy2(runtime,out/'xl.dll')
    else:
        env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
        env['XL6_TCC_LIB']=str(Path(build['NativeRoot'])/'build/linux/tcc-dist')
    report=dict(Complete=False,InputsUnchanged=False,SourceInputsMatch=True,ProductionFiles=len(production),
        Scope=__doc__,Inputs=inputs,Rows=rows,Products=products,Catalog=[],ThreadLanguage=None,
        WaitQueueComplete=False,Full53Acceptance=False,ShippingArtifact=False,StandaloneImports=[])
    def run(name,command):
        command=list(map(str,command)); actual=command
        if windows and command[0]==cc:
            # External immutable cohorts have over a thousand long object
            # paths. Keep the real argument bytes without CreateProcess's
            # command-line limit; GCC owns response-file parsing, not a shell.
            response=out/(name+'.rsp')
            response.write_text('\n'.join('"'+arg.replace('\\','/').replace('"','\\"')+'"'
                                          for arg in command[1:])+'\n',encoding='utf-8')
            product(response); actual=[cc,'@'+str(response)]
        result=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        rows.append(dict(Name=name,Command=command,ActualCommand=actual,Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
        assert result.returncode==0,(name,log.read_bytes()[-6000:])
        print(name+' verified',flush=True)
        return log.read_text(encoding='utf-8',errors='replace')
    def product(path): products.append(dict(Path=str(path),Sha256=sha(path)))
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),
        '-I'+str(root/'release'),'-I'+str(root/'release/include'),'-I'+str(public/'include')]
    if not windows: flags+=['-D_GNU_SOURCE']
    unused=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers']
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
        ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
    links=[runtime,*(['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else
        ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)])]
    try:
        for optimization in ('O0','O2'):
            executable=out/('runtime-operand-'+optimization+('.exe' if windows else ''))
            run(optimization+'-catalog-build',[cc,*flags,'-'+optimization,root/'test/cases/xlir_runtime_operand_catalog.c',
                *objects,*links,'-o',executable]); product(executable)
            text=run(optimization+'-catalog',[executable])
            match=re.search(r'operations=(\d+) effects=(\d+) consumes=(\d+) invalid=(\d+) rejected',text)
            assert match,text
            operations,effects,consumes,invalid=map(int,match.groups())
            assert effects==operations*8 and consumes>0 and invalid==40
            report['Catalog'].append(dict(Optimization=optimization,Operations=operations,
                Effects=effects,Consumes=consumes,Invalid=invalid))
        if not args.catalog_only:
            plan=out/('thread-public-plan'+('.exe' if windows else ''))
            run('thread-plan-build',[cc,*flags,'-O2',root/'test/cases/xlir_thread_public_plan.c',*objects,*links,'-o',plan]); product(plan)
            for name in CASES:
                source=root/'test/cases'/(name+'.xl')
                required=name not in ('cgen_thread_default','stdlib_thread_artifact_absent')
                run(name+'-native-plan',[plan,source,'required' if required else 'absent'])
                for mode in ('run','module','ir-typed','ir-lowered'):
                    target=out/(name+'-'+mode+('.json' if mode.startswith('ir-') else ''))
                    run(name+'-'+mode,[driver,source,mode,target])
                    if mode.startswith('ir-'): product(target)
                generated=out/(name+'.c'); repeated=out/(name+'.repeat.c')
                for target in (generated,repeated):
                    run(name+'-cgen'+('-repeat' if target==repeated else ''),[driver,source,'cgen-default',target]); product(target)
                assert generated.read_bytes()==repeated.read_bytes()
                text=generated.read_text(encoding='utf-8-sig')
                assert 'xlir_thread_' not in text and 'xrtThreadCreate(' not in text
                if required: assert 'xl6ThreadCreateV1(' in text
                if name=='stdlib_thread_artifact_absent': assert 'xl6Thread' not in text
                for optimization in ('O0','O2'):
                    run(name+'-gcc-'+optimization,[cc,*flags,*unused,'-'+optimization,'-fsyntax-only',generated])
                executable=out/(name+'-aot'+('.exe' if windows else ''))
                run(name+'-aot-build',[driver,source,'exe',executable.with_suffix('') if windows else executable]); product(executable)
                run(name+'-aot-run',[executable])
                text=run(name+'-imports',[objdump,'-p',executable])
                assert not re.search(r'DLL Name:\s*x(?:l|lc)\.dll|NEEDED\s+libx(?:l|lc)\.so',text,re.I)
                report['StandaloneImports'].append(name)
            nested=out/'thread-language'
            run('thread-language',[sys.executable,root/'tools/check_stdlib_thread_language.py',
                '--root',root,'--compiler',public,'--out',nested])
            language=json.loads((nested/'thread.language.json').read_text(encoding='utf-8'))
            assert language['Complete'] and language['InputsUnchanged'] and language['LanguageCgenAcceptance']
            assert not language['Full53Acceptance'] and not language['ShippingLibraryAcceptance']
            report['ThreadLanguage']=dict(Report=str(nested/'thread.language.json'),Phases=len(language['Rows']))
            product(nested/'thread.language.json')
        report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']
    print('Compiled ownership/resident Thread gate: '+str(len(rows))+' phases',flush=True)

if __name__=='__main__': main()
