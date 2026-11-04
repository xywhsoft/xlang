"""Generic sizeof output initializer gate over a declared compiler overlay.

Includes a real Native XGE BuildInfo call. No window/GPU/network operations.
The parent full source objects are pinned; the compiler-only overlay is explicit.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','overlay','packages','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,overlay,packages,out=[x.resolve() for x in (a.root,a.overlay,a.packages,a.out)]
    d=json.loads(overlay.read_text());assert d['complete'] and d['passed'] and not d['fullRebuild']
    cohort=Path(d['sourceRoot']);base=json.loads(Path(d['parentBuild']).read_text());assert base['Complete'] and base['InputsUnchanged']
    sha=lambda x:hashlib.sha256(Path(x).read_bytes()).hexdigest()
    assert sha(d['parentBuild'])==d['parentBuildSha256'] and sha(d['Driver'])==d['DriverSha256']
    replacements={x['ParentObject']:x for x in d['Objects']};objects=[];inputs={str(overlay):sha(overlay)}
    for original in base['Objects']:
        row=replacements.get(original['Path'],original);assert sha(row['Path'])==row['Sha256'].lower();inputs[row['Path']]=sha(row['Path'])
        if Path(original['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c'):objects.append(row['Path'])
    driver=Path(d['Driver']);runtime=Path(d['RuntimeLibrary']);native=packages/'_xge_native';engine=native/'native'/('xge.dll' if os.name=='nt' else 'libxge.so')
    fixture=root/'test/cases/foreign_xge3_binding_output_size.xl'
    sources=[root/'test/cases'/n for n in ('foreign_xge3_binding_output_size.c','foreign_managed_view_native.c','foreign_decl_module_host.c')]
    pinned=[driver,runtime,fixture,*sources,root/'test/cases/foreign_xge3_binding_output_size.h',root/'test/cases/foreign_xge3_binding_output_size_ir.c',Path(__file__).resolve(),engine,*[q for q in (native/'include').rglob('*') if q.is_file()]]
    inputs.update((str(q),sha(q)) for q in pinned);out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(root/'tools'));from compiler_response import command_with_response
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(engine,out/engine.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=False,overlay=str(overlay),inputs=inputs,rows=[],networkTests=False,gpuTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        started=time.monotonic();r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,exitCode=r.returncode,accepted=accepted,logSha256=sha(log),seconds=round(time.monotonic()-started,3)));save()
        print(name,r.returncode,flush=True);assert (r.returncode==0)==accepted,log
    generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
    text=generated.read_text(encoding='utf-8-sig');assert 'native_raw.size = sizeof(native_raw)' in text and 'native_raw.e1.size = sizeof(native_raw.e1)' in text and 'native_raw.e2.size = sizeof(native_raw.e2)' in text
    sdk=Path(base.get('RuntimeDirectory',base.get('SdkDirectory','')))
    # Only test headers overlay the frozen compiler source, never live core headers.
    headers=out/'fixture-inputs/test/cases';headers.mkdir(parents=True)
    for n in ('foreign_xge3_binding_output_size.h','foreign_managed_view.h'):shutil.copy2(root/'test/cases'/n,headers/n)
    includes=['-I'+str(q) for q in (out/'fixture-inputs',cohort,Path(d['parentBuild']).parent/'include',sdk,sdk/'include',native/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
    for standard in ('c++11','c++17'):run('header-'+standard,['g++','-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
    warnings=['-Wno-'+n for n in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine.parent)]
    for level in ('O0','O2'):
        exe=out/(level+('.exe' if os.name=='nt' else '.out'))
        run(level+'-build',['gcc',*common,*warnings,'-'+level,generated,*sources,*objects,runtime,engine,*systems,'-o',exe]);run(level+'-run',[exe])
        ir=out/('ir-'+level+('.exe' if os.name=='nt' else '.out'))
        run('ir-'+level+'-build',['gcc',*common,'-'+level,root/'test/cases/foreign_xge3_binding_output_size_ir.c',*objects,runtime,*systems,'-o',ir]);run('ir-'+level+'-run',[ir,fixture])
    # The ABI may retain identical size/alignment while the member signedness differs.
    bad=out/'bad-header/test/cases';bad.mkdir(parents=True);header=(root/'test/cases/foreign_xge3_binding_output_size.h').read_text()
    (bad/'foreign_xge3_binding_output_size.h').write_text(header.replace('unsigned size; int value;','int size; int value;'))
    run('reject-native-member-type',['gcc','-I'+str(out/'bad-header'),*common,*warnings,'-fsyntax-only',generated],False)
    # A correctly typed unsigned member must also represent the complete record size.
    large=out/'bad-range/test/cases';large.mkdir(parents=True)
    (large/'foreign_xge3_binding_output_size.h').write_text(header.replace('#endif',
        'typedef struct BindingSizedTooLarge { unsigned char size; char padding[300]; } BindingSizedTooLarge;\n'
        'int binding_sized_too_large(BindingSizedTooLarge*);\n#endif'))
    largeCase=out/'too-large.xl';largeCase.write_text('''extern "BindingSizedTooLarge" struct TooLarge @header("test/cases/foreign_xge3_binding_output_size.h")
 c_uchar size
 c_char padding[300]
end
extern "binding_sized_too_large" @header("test/cases/foreign_xge3_binding_output_size.h") @no_raise @c_output(0,0) @c_output_size(0,"size") func tooLarge() as TooLarge
func main(...) as int
 auto value=tooLarge()
 return 0
end
''')
    largeC=out/'too-large.c';run('range-proof-cgen',[driver,largeCase,'cgen-default',largeC])
    run('reject-native-member-range',['gcc','-I'+str(out/'bad-range'),*common,*warnings,'-fsyntax-only',largeC],False)
    assert 'C output size field range' in (out/'reject-native-member-range.log').read_text(errors='replace')
    original=fixture.read_text();needle='@c_output_size(0,"size")'
    changes={'duplicate':needle+' '+needle,'wrong-slot':'@c_output_size(1,"size")','negative-slot':'@c_output_size(-1,"size")','empty-field':'@c_output_size(0,"")','injection':'@c_output_size(0,"size;bad")','unknown-field':'@c_output_size(0,"missing")','signed-field':'@c_output_size(0,"value")','wrong-arity':'@c_output_size(0,"size",1)','wrong-type':'@c_output_size(0,0)'}
    negatives={n:original.replace(needle,v,1) for n,v in changes.items()}
    negatives['no-output']='extern "native" @no_raise @c_output_size(0,"size") func native()\n'
    negatives['scalar-output']='extern "native" @no_raise @c_output(0,0) @c_output_size(0,"size") func native() as c_uint\n'
    for name,code in negatives.items():
        case=out/(name+'.xl');case.write_text(code,encoding='utf8');target=out/(name+'.c');run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    report['inputsUnchanged']=all(sha(q)==v for q,v in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
