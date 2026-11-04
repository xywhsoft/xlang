"""CPU-only scoped retained root gate over a hash-verified compiler cohort.

Accepts an actual complete source build or an explicitly partial frontend
overlay. Build qualification is recorded without certifying shipping/VFS.
No Native device, graphics, window, network or security tests are run.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
from foreign_validation_build import add_build_arguments,load_build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    add_build_arguments(parser)
    args=parser.parse_args();root,out=[p.resolve() for p in (args.root,args.out)]
    build=load_build(args.build,args.overlay)
    cohort,basePath,base,objects,inputs,driver,runtime=[build[k] for k in ('cohort','basePath','base','objects','inputs','driver','runtime')]
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    fixture=root/'test/cases/foreign_scoped_retain.xl';native=root/'test/cases/foreign_scoped_retain_native.c';host=root/'test/cases/foreign_decl_module_host.c'
    header=root/'test/cases/foreign_scoped_retain.h';irFixture=root/'test/cases/foreign_scoped_retain_ir.c'
    pinned=[driver,runtime,fixture,native,host,header,Path(__file__).resolve()]
    if irFixture.exists():pinned.append(irFixture)
    inputs.update((str(p),sha(p)) for p in pinned);out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(root/'tools'));from compiler_response import command_with_response
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=build["fullSourceRebuild"],buildManifest=build["buildManifest"],overlay=build["overlay"],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,accepted=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        started=time.monotonic();result=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,exitCode=result.returncode,accepted=accepted,logSha256=sha(log),seconds=round(time.monotonic()-started,3)));save()
        print(name,result.returncode,flush=True);assert (result.returncode==0)==accepted,log
    generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
    text=generated.read_text(encoding='utf-8-sig');assert 'ScopedRetainScope native_scope_' in text and 'xl6ForeignResultProbeReserveRootV1' in text and 'xl6ForeignResultProbeMaterializeRootV1' in text
    headers=out/'fixture-inputs/test/cases';headers.mkdir(parents=True);shutil.copy2(header,headers/header.name)
    sdk=Path(base.get('SdkDirectory') or base.get('RuntimeDirectory') or str(Path(base['Objects'][0]['Source']).parents[2]/'release'/'win_x64'))
    includes=['-I'+str(p) for p in (out/'fixture-inputs',cohort,basePath.parent/'include',sdk,sdk/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
    for standard in ('c++11','c++17'):run('header-'+standard,['g++','-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
    warnings=['-Wno-'+s for s in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for level in ('O0','O2'):
        exe=out/(level+('.exe' if os.name=='nt' else '.out'))
        run(level+'-build',['gcc',*common,*warnings,'-'+level,generated,native,host,*objects,runtime,*systems,'-o',exe]);run(level+'-run',[exe])
        if irFixture.exists():
            gate=out/('ir-'+level+('.exe' if os.name=='nt' else '.out'))
            run('ir-'+level+'-build',['gcc',*common,'-'+level,irFixture,*objects,runtime,*systems,'-o',gate]);run('ir-'+level+'-run',[gate,fixture])
    original=fixture.read_text(encoding='utf-8-sig')
    single='@c_output(1,0) @c_output_retain(1,retain,"zero",rollback)'
    scalar='@c_result("retained_view") @c_retain(retain,"zero",rollback)'
    changes={
        'missing-single-retain':(single,'@c_output(1,0)'),
        'single-text-instead-of-root':(single,'@c_output(1,0) @c_output_view(1,"text")'),
        'single-wrong-slot':(single,'@c_output(1,0) @c_output_retain(2,retain,"zero",rollback)'),
        'single-wrong-retainer':(single,'@c_output(1,0) @c_output_retain(1,rollback,"zero",rollback)'),
        'single-wrong-rollback':(single,'@c_output(1,0) @c_output_retain(1,retain,"zero",retain)'),
        'scalar-missing-retain':(scalar,'@c_result("retained_view")'),
        'scalar-wrong-retainer':(scalar,'@c_result("retained_view") @c_retain(rollback,"zero",rollback)'),
        'watch-scalar-and-output':('@scope("ScopedRetainScope")','@scope("ScopedRetainScope") @watch("ScopedRetainWatch")'),
        'caller-instead-of-root':('@lifetime("owned")','@lifetime("caller")'),
        'bad-scope-identifier':('@scope("ScopedRetainScope")','@scope("Scope;bad")'),
        'duplicate-scope':('@scope("ScopedRetainScope")','@scope("ScopedRetainScope") @scope("ScopedRetainScope")'),
        'unsafe-output-effect':('extern "scoped_retain_single" @no_raise','extern "scoped_retain_single" @no_raise @pure'),
        'raising-output':('extern "scoped_retain_single" @no_raise','extern "scoped_retain_single"'),
    }
    for name,(needle,replacement) in changes.items():
        assert needle in original,name
        case=out/(name+'.xl');case.write_text(original.replace(needle,replacement,1),encoding='utf8');target=out/(name+'.c')
        run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    for name,old,new in (
        ('hold-return','int scoped_retain_hold(ScopedRetainNode*);','float scoped_retain_hold(ScopedRetainNode*);'),
        ('single-output','int scoped_retain_single(ScopedRetainNode*,ScopedRetainNode**);','int scoped_retain_single(ScopedRetainNode*,void**);'),
        ('scope-type','} ScopedRetainScope;','} RenamedScope;'),
    ):
        bad=out/('bad-'+name)/'test/cases';bad.mkdir(parents=True);source=header.read_text(encoding='utf8');assert old in source
        (bad/header.name).write_text(source.replace(old,new),encoding='utf8')
        run('reject-native-'+name,['gcc','-I'+str(bad.parents[1]),*common,*warnings,'-fsyntax-only',generated],False)
    report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged']
    report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
