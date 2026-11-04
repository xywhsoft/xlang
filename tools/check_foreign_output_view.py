"""Borrowed output conversion: exact ABI, owned tuple transaction and OOM cleanup.

Compiler-only overlay is explicit. No windows, GPU, network or security tests.
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
    driver=Path(d['Driver']);runtime=Path(d['RuntimeLibrary']);native=packages/'_xge_native'
    fixture=root/'test/cases/foreign_xge3_binding_output_view.xl'
    provider=root/'test/cases/foreign_xge3_binding_output_view.c';host=root/'test/cases/foreign_decl_module_host.c'
    header=root/'test/cases/foreign_xge3_binding_output_view.h';irSource=root/'test/cases/foreign_xge3_binding_output_view_ir.c'
    inputs.update((str(q),sha(q)) for q in [driver,runtime,fixture,provider,host,header,irSource,Path(__file__).resolve(),*[q for q in (native/'include').rglob('*') if q.is_file()]])
    out.mkdir(parents=True,exist_ok=False);sys.path.insert(0,str(root/'tools'));from compiler_response import command_with_response
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort))
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,fullSourceRebuild=False,overlay=str(overlay),inputs=inputs,rows=[],networkTests=False,gpuTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        command=list(map(str,command));actual=command_with_response(command,'gcc',out/(name+'.rsp')) if command[0] in ('gcc','g++') else command
        started=time.monotonic();r=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,exitCode=r.returncode,accepted=accepted,logSha256=sha(log),seconds=round(time.monotonic()-started,3)));save()
        print(name,r.returncode,flush=True);assert (r.returncode==0)==accepted,log
    generated=out/'generated.c';run('cgen',[driver,fixture,'cgen-default',generated]);run('ir',[driver,fixture,'ir-lowered',out/'lowered.json']);run('header',[driver,fixture,'emit',out/'public'])
    text=generated.read_text(encoding='utf-8-sig');assert 'xstrview e0;' in text and 'xbytesview e1;' in text and 'native_admitted' in text
    sdk=Path(base.get('RuntimeDirectory',base.get('SdkDirectory','')))
    headers=out/'fixture-inputs/test/cases';headers.mkdir(parents=True);shutil.copy2(header,headers/header.name)
    includes=['-I'+str(q) for q in (out/'fixture-inputs',cohort,Path(d['parentBuild']).parent/'include',sdk,sdk/'include',native/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    run('header-C',['gcc',*common,'-x','c','-fsyntax-only',out/'public.h'])
    for standard in ('c++11','c++17'):run('header-'+standard,['g++','-std='+standard,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes,'-x','c++','-fsyntax-only',out/'public.h'])
    warnings=['-Wno-'+n for n in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for level in ('O0','O2'):
        exe=out/(level+('.exe' if os.name=='nt' else '.out'))
        run(level+'-build',['gcc',*common,*warnings,'-'+level,generated,provider,host,*objects,runtime,*systems,'-o',exe]);run(level+'-run',[exe])
        ir=out/('ir-'+level+('.exe' if os.name=='nt' else '.out'))
        run('ir-'+level+'-build',['gcc',*common,'-'+level,irSource,*objects,runtime,*systems,'-o',ir]);run('ir-'+level+'-run',[ir,fixture])
    bad=out/'bad-header/test/cases';bad.mkdir(parents=True)
    (bad/header.name).write_text(header.read_text().replace('binding_output_view_pair(xstrview*','binding_output_view_pair(xbytesview*'))
    run('reject-native-view-type',['gcc','-I'+str(out/'bad-header'),*common,*warnings,'-fsyntax-only',generated],False)
    original=fixture.read_text();needle='@c_output_view(3,"text")'
    changes={'duplicate':needle+' '+needle,'wrong-slot':'@c_output_view(0,"text")','negative-slot':'@c_output_view(-1,"text")','empty-kind':'@c_output_view(3,"")','unknown-kind':'@c_output_view(3,"cstr")','wrong-kind':'@c_output_view(3,"bytes")','wrong-arity':'@c_output_view(3,"text",0)','wrong-type':'@c_output_view(3,0)','size-overlap':needle+' @c_output_size(3,"size")','missing-view':''}
    negatives={n:original.replace(needle,v,1) for n,v in changes.items()}
    negatives['no-output']='extern "native" @no_raise @c_output_view(0,"text") func native() as string\n'
    negatives['scalar-output']='extern "native" @no_raise @c_output(0,0) @c_output_view(0,"text") func native() as c_uint\n'
    negatives['split-span']=original.replace('@c_output_view(2,"text")','@c_output_view(1,"text")',1)
    for name,code in negatives.items():
        case=out/(name+'.xl');case.write_text(code,encoding='utf8');target=out/(name+'.c');run('reject-'+name,[driver,case,'cgen-default',target],False);assert not target.exists()
    report['inputsUnchanged']=all(sha(q)==v for q,v in inputs.items());assert report['inputsUnchanged'];report.update(complete=True,passed=True);save()
if __name__=='__main__':main()
