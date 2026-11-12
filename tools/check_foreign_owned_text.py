"""Source-driver owned-text output ABI, zero-copy ownership, rollback and sealed IR.

This is a compiler/runtime source preflight. It does not qualify a rebuilt
shipping VFS, package shared initialization or the complete XGE SDK/IDE delivery.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','source','public','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,source,public,out=[v.resolve()for v in(a.root,a.source,a.public,a.out)]
    out.mkdir(parents=True,exist_ok=False);win=os.name=='nt';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    d=json.loads((public/'build.json').read_text(encoding='utf-8-sig'));assert d['Complete']and d['InputsUnchanged']and d['SourceXrt']
    for row in d['Inputs']+d['Headers']+d['Objects']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(d['Driver']);assert sha(driver)==d['DriverSha256'].lower()
    runtime=public/'xl.dll'if win else Path(d['RuntimeLibrary']);sdk=Path(d['RuntimeDirectory']if win else d['SdkDirectory'])
    objects=[row['Path']for row in d['Objects']if Path(row.get('Source','')).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    if win:
        # Windows records Source in compiler cache receipts; its canonical
        # source order is also part of the manifest.
        objects=[row['Path']for row,name in zip(d['Objects'],d['Sources'])if Path(name).name!='xlir_public_source_driver.c']
        shutil.copy2(runtime,out/runtime.name)
    case=root/'test/cases/foreign_owned_text.xl';provider=case.with_suffix('.c');ircase=case.with_name('foreign_owned_text_ir.c');header=case.with_suffix('.h');host=root/'test/cases/foreign_decl_module_host.c'
    inputs={str(v):sha(v)for v in(driver,public/'build.json',runtime,case,provider,ircase,header,host,Path(__file__).resolve())}
    for file in (source/'src').rglob('*'):
        if file.is_file() and file.suffix in ('.c','.h','.inc'):
            name=file.relative_to(source);assert (root/name).read_bytes().replace(b'\r\n',b'\n')==file.read_bytes().replace(b'\r\n',b'\n'),name
            inputs[str(file)]=sha(file);inputs[str(root/name)]=sha(root/name)
    report=dict(complete=False,passed=False,fullCompilerBuild=False,shippingDelivery=False,scope=__doc__,sourceBuild=str(public/'build.json'),inputs=inputs,rows=[])
    save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'};env['XL6_SOURCE_NATIVE_ROOT']=str(source)
    if not win:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    def run(name,cmd,refused=False):
        cmd=list(map(str,cmd));actual=command_with_response(cmd,cmd[0],out/(name+'.rsp'))if Path(cmd[0]).name.lower() in ('gcc','gcc.exe','g++','g++.exe')else cmd
        start=time.monotonic();z=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,refused=refused,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert (z.returncode!=0)==refused,log;return z.stdout+z.stderr
    cc=shutil.which('gcc');cpp=shutil.which('g++');assert cc and cpp
    includes=['-I'+str(v)for v in(root,source,public/'include',sdk,sdk/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    warnings=['-Wno-'+n for n in('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if win else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    try:
        generated=out/'generated.c';run('cgen',[driver,case,'cgen-default',generated]);run('ir',[driver,case,'ir-lowered',out/'lowered.json']);run('header',[driver,case,'emit',out/'public'])
        run('jit',[driver,case,'run',out/'jit-unused'])
        text=generated.read_text(encoding='utf-8-sig');assert 'xl6COutputTextTakeV1('in text and 'fixtureOwnedText('in text
        for level in ('O0','O2'):
            exe=out/(level+('.exe'if win else'.out'))
            run(level+'-build',[cc,*common,*warnings,'-'+level,generated,provider,host,*objects,runtime,*systems,'-o',exe]);run(level+'-execute',[exe])
            ir=out/('ir-'+level+('.exe'if win else'.out'))
            run('ir-'+level+'-build',[cc,*common,'-'+level,ircase,*objects,runtime,*systems,'-o',ir]);run('ir-'+level+'-execute',[ir,case])
        for standard,tool,language in(('c11',cc,'c'),('c++11',cpp,'c++'),('c++17',cpp,'c++')):
            run('header-'+standard,[tool,'-std='+standard,'-Wall','-Wextra','-Werror',*includes,'-x',language,'-fsyntax-only',out/'public.h'])
        bad=out/'bad-header/test/cases';bad.mkdir(parents=True)
        (bad/header.name).write_text(header.read_text(encoding='utf-8').replace('char** data,uint64_t* size','void** data,uint64_t* size',1))
        run('reject-physical-prototype',[cc,'-I'+str(out/'bad-header'),*common,*warnings,'-fsyntax-only',generated],True)
        original=case.read_text(encoding='utf-8');needle='@c_output_text(1,2,"char*","uint64_t","xrt",0)'
        changes={'duplicate':needle+' '+needle,'slot-overlap':needle.replace('(1,2','(1,1'),'negative-slot':needle.replace('(1,2','(-1,2'),'far-slot':needle.replace('(1,2','(1,63'),'pointer':needle.replace('char*','void*'),'length':needle.replace('uint64_t','int64_t'),'allocator':needle.replace('"xrt"','"crt"'),'arity':needle[:-3]+')','pure':needle+' @pure','missing-header':needle}
        for name,replacement in changes.items():
            code=original.replace(needle,replacement,1)
            if name=='missing-header':code=code.replace('@header("test/cases/foreign_owned_text.h")','',1)
            file=out/(name+'.xl');file.write_text(code,encoding='utf-8');target=out/(name+'.c');run('reject-'+name,[driver,file,'cgen-default',target],True);assert not target.exists()
        for name,before,after in(('nullable','as string','as string?'),('bytes','as string','as bytes'),('raising','@no_raise ','')):
            file=out/(name+'.xl');file.write_text(original.replace(before,after,1),encoding='utf-8');target=out/(name+'.c')
            run('reject-'+name,[driver,file,'cgen-default',target],True);assert not target.exists()
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,error=str(e));raise
    finally:save()

if __name__=='__main__':main()
