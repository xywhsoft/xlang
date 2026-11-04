"""CPU-only borrow variants gate over a hash-verified compiler cohort.

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
    fixture=root/'test/cases/foreign_borrow_variants.xl';native=root/'test/cases/foreign_borrow_variants_native.c';host=root/'test/cases/foreign_decl_module_host.c'
    pinned=[driver,runtime,fixture,native,host,root/'test/cases/foreign_borrow_variants.h',Path(__file__).resolve()]
    irFixture=root/'test/cases/foreign_borrow_variants_ir.c'
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
    text=generated.read_text(encoding='utf-8-sig');assert 'xl6ForeignScopeViewGetV2' in text and 'variant_take(' in text
    headers=out/'fixture-inputs/test/cases';headers.mkdir(parents=True);shutil.copy2(root/'test/cases/foreign_borrow_variants.h',headers/'foreign_borrow_variants.h')
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
    negatives={
        'raw':original.replace('@lifetime("callback")','@lifetime("caller")'),
        'const':original.replace('extern "BorrowRoot*" class Epoch','extern "const BorrowRoot*" class Epoch'),
        'header':original.replace('class Epoch @header("test/cases/foreign_borrow_variants.h")','class Epoch @header("other.h")'),
        'effect':original.replace('@no_raise func accept(Root root,Epoch value)','@no_raise @nonblocking func accept(Root root,Epoch value)'),
        'return':original.replace('func accept(Root root,Epoch value) as c_int','func accept(Root root,Epoch value) as Epoch'),
        'implicit-conversion':original.replace('saved.value=value','Root forbidden=value'),
    }
    for name,code in negatives.items():
        path=out/(name+'.xl');path.write_text(code,encoding='utf8');target=out/(name+'.c');run('reject-'+name,[driver,path,'cgen-default',target],False);assert not target.exists()
    report['inputsUnchanged']=all(sha(p)==s for p,s in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
