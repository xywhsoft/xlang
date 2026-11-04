"""Exercise callable C fields using an explicitly declared compiler overlay.

This is a development gate. It does not certify a full compiler/VFS rebuild or
change release products. Every reused object comes from the pinned full parent;
compiler/front-end replacements and their immutable source root are explicit.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','overlay','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();overlay=a.overlay.resolve();out=a.out.resolve()
    d=json.loads(overlay.read_text());assert d['complete'] and d['passed'] and not d['fullRebuild']
    cohort=Path(d['sourceRoot']);base=json.loads(Path(d['parentBuild']).read_text())
    assert base['Complete'] and base['InputsUnchanged'] and base['SourceXrt']
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    assert sha(d['parentBuild'])==d['parentBuildSha256'] and sha(d['Driver'])==d['DriverSha256']
    replacements={v['ParentObject']:v for v in d['Objects']}
    objects=[];inputs={str(overlay):sha(overlay)}
    for v in base['Objects']:
        row=replacements.get(v['Path'],v);assert sha(row['Path'])==row['Sha256'].lower()
        inputs[row['Path']]=sha(row['Path'])
        if Path(v['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c'):
            objects.append(row['Path'])
    driver=Path(d['Driver']);runtime=Path(d['RuntimeLibrary'])
    fixture=root/'test/cases/foreign_callable_field.xl'
    sources=[root/'test/cases'/n for n in ('foreign_callable_field_native.c','foreign_decl_module_host.c')]
    pinned=[driver,runtime,fixture,*sources,root/'test/cases/foreign_callable_field.h',root/'test/cases/foreign_callable_field_ir.c',Path(__file__).resolve()]
    inputs.update((str(v),sha(v)) for v in pinned)
    out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(root/'tools'))
    from compiler_response import command_with_response
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,overlay=str(overlay),inputs=inputs,rows=[],
                fullSourceRebuild=False,networkTests=False,gpuTests=False,desktopTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        if command[0] in ('gcc','g++'):command=command_with_response(command,'gcc',out/(name+'.rsp'))
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,accepted=accepted,logSha256=sha(log),seconds=round(time.monotonic()-start,3)))
        save();print(name,r.returncode,flush=True);assert (r.returncode==0)==accepted,log
    generated=out/'generated.c'
    run('cgen',[driver,fixture,'cgen-default',generated])
    run('ir',[driver,fixture,'ir-lowered',out/'lowered.json'])
    run('header',[driver,fixture,'emit',out/'public'])
    text=generated.read_text(encoding='utf-8-sig')
    assert 'native callable field ABI' in text and 'native_callee(' in text
    assert 'extern int add(' not in text
    sdk=Path(base.get('RuntimeDirectory',base.get('SdkDirectory','')))
    includes=['-I'+str(v) for v in (cohort,root,Path(d['parentBuild']).parent/'include',sdk,sdk/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
    for standard in ('c++11','c++17'):
        run('header-'+standard,['g++','-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
    warnings=['-Wno-'+v for v in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0','O2'):
        exe=out/(opt+('.exe' if os.name=='nt' else '.out'))
        run(opt+'-build',['gcc',*common,*warnings,'-'+opt,generated,*sources,*objects,runtime,*systems,'-o',exe])
        run(opt+'-run',[exe])
        mutation=out/('ir-'+opt+('.exe' if os.name=='nt' else '.out'))
        run('ir-'+opt+'-build',['gcc',*common,'-'+opt,root/'test/cases/foreign_callable_field_ir.c',*objects,runtime,*systems,'-o',mutation])
        run('ir-'+opt+'-run',[mutation,fixture])
    bad=out/'bad-header/test/cases';bad.mkdir(parents=True)
    header=(root/'test/cases/foreign_callable_field.h').read_text()
    assert 'int (*add)(BindingFieldTable*,int);' in header
    (bad/'foreign_callable_field.h').write_text(header.replace('int (*add)(BindingFieldTable*,int);','int (*add)(BindingFieldTable*,long long);'))
    run('reject-native-field-signature',['gcc','-I'+str(out/'bad-header'),*common,*warnings,'-fsyntax-only',generated],False)
    original=fixture.read_text()
    needle='extern "add" @c_field_call @no_raise func add(c_int value) as c_int'
    negatives={name:original.replace(needle,replacement,1) for name,replacement in {
        'duplicate-field-contract':'extern "add" @c_field_call @c_field_call @no_raise func add(c_int value) as c_int',
        'attribute-arguments':'extern "add" @c_field_call(0) @no_raise func add(c_int value) as c_int',
        'field-injection':'extern "add;bad" @c_field_call @no_raise func add(c_int value) as c_int',
        'field-projection-overlap':'extern "add" @c_field_call @c_deref @no_raise func add(c_int value) as c_int',
    }.items()}
    negatives['free-function']='extern "add" @c_field_call @no_raise func add(c_int value) as c_int\n'
    for name,text in negatives.items():
        case=out/(name+'.xl');case.write_text(text);target=out/(name+'.c')
        run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    report['inputsUnchanged']=all(sha(v)==h for v,h in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()


if __name__=='__main__':main()
