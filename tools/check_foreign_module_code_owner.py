"""Borrowed current-module CodeOwner inputs through sealed physical signatures.

Tests source declarations, actual bound/static execution, native field and
protocol calls, C/C++ proofs and malformed IR. No COM/OS/provider qualification.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('root','out'):parser.add_argument('--'+key,type=Path,required=True)
    add_build_arguments(parser);args=parser.parse_args();root=args.root.resolve();out=args.out.resolve();b=load_build(args.build,args.overlay)
    cohort=b['cohort'];driver=b['driver'];runtime=b['runtime'];windows=os.name=='nt'
    stem='foreign_module_code_owner';fixture=root/'test/cases'/(stem+'.xl');header=fixture.with_suffix('.h');native=fixture.with_name(stem+'_native.c');host=fixture.with_name(stem+'_host.c');mutator=root/'test/cases/foreign_nested_pack_null_ir.c'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs=dict(b['inputs']);inputs.update((str(p),sha(p)) for p in (Path(__file__),fixture,header,native,host,mutator))
    out.mkdir(exist_ok=False);env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if windows:shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,shippingDelivery=False,fullCompilerRebuild=False,compilerOverlay=b['overlay'],inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,success=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        start=time.monotonic();value=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=300);log=out/(name+'.log');log.write_bytes(value.stdout+value.stderr)
        report['rows'].append(dict(name=name,command=actual,exitCode=value.returncode,expectedSuccess=success,logSha256=sha(log),seconds=round(time.monotonic()-start,3)));save()
        print(name,value.returncode,flush=True);assert (not value.returncode)==success,log
    try:
        generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('public',[driver,fixture,'emit',out/'public'])
        text=generated.read_text();assert '(xlir_code_owner.size ? &xlir_code_owner : NULL)' in text
        ir=json.loads((out/'lowered.json').read_text());assert ir['foreignFixedInputs'] and any(row['kind']==2 for row in ir['foreignFixedInputs'])
        priority=out/'fixture-inputs/test/cases';priority.mkdir(parents=True);shutil.copy2(header,priority/header.name)
        sdk=Path(b['base'].get('SdkDirectory') or b['base'].get('RuntimeDirectory') or runtime.parent)
        includes=['-I'+str(p) for p in (priority.parents[1],root,cohort,b['basePath'].parent/'include',sdk,sdk/'include')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes];warnings=['-Wno-'+s for s in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
        system=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        run('header-C',['gcc',*flags,'-x','c','-fsyntax-only',out/'public.h'])
        for standard in ('c++11','c++17'):run('header-'+standard,['g++','-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
        for opt in ('O0','O2'):
            exe=out/(opt+('.exe' if windows else '.out'));run(opt+'-build',['gcc',*flags,*warnings,'-'+opt,generated,native,host,*b['objects'],runtime,*system,'-o',exe])
            for mode in ('static','bound'):run(opt+'-'+mode,[exe,mode])
            exe=out/('ir-'+opt+('.exe' if windows else '.out'));run('ir-'+opt+'-build',['gcc',*flags,'-'+opt,mutator,*b['objects'],runtime,*system,'-o',exe]);run('ir-'+opt+'-run',[exe,fixture])
        code=fixture.read_text();cases={
            'negative-slot':code.replace('@c_code_owner(0)','@c_code_owner(-1)',1),
            'overflow-slot':code.replace('@c_code_owner(0)','@c_code_owner(64)',1),
            'missing-slot':code.replace('@c_code_owner(0)','@c_code_owner()',1),
            'extra-type':code.replace('@c_code_owner(0)','@c_code_owner(0,"const void*")',1),
            'unreachable-slot':code.replace('@c_code_owner(0)','@c_code_owner(63)',1),
            'duplicate':code.replace('@c_code_owner(0)','@c_code_owner(0) @c_code_owner(0)',1),
            'overlap-null':code.replace('@c_code_owner(1) @c_null(2,"const void*")','@c_code_owner(1) @c_null(1,"const void*")',1),
            'receiver':code.replace('@c_code_owner(1) func method','@c_code_owner(0) func method',1),
            'output':code.replace('@c_code_owner(0) @c_output(1,0)','@c_code_owner(1) @c_output(1,0)',1),
            'missing-header':code.replace('@header("test/cases/foreign_module_code_owner.h")',''),
            'missing-effect':code.replace('@no_raise @c_code_owner(0)','@c_code_owner(0)',1)}
        for name,bad in cases.items():
            assert bad!=code;path=out/(name+'.xl');path.write_text(bad);target=out/(name+'.c');run('reject-'+name,[driver,path,'cgen-default',target],False);assert not target.exists()
        bad=out/'bad-physical/test/cases';bad.mkdir(parents=True);old=header.read_text();needle='int module_owner_capture(const XL6_CodeOwner*,int);';assert needle in old
        (bad/header.name).write_text(old.replace(needle,'int module_owner_capture(const void*,int);'))
        run('reject-native-pointer',['gcc','-I'+str(bad.parents[1]),*flags,*warnings,'-fsyntax-only',generated],False)
        run('reject-header-pointer',['g++','-std=c++17','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(bad.parents[1]),*includes,'-x','c++','-fsyntax-only',out/'public.h'],False)
        report['inputsUnchanged']=all(sha(p)==v for p,v in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True)
    finally:save()
if __name__=='__main__':main()
