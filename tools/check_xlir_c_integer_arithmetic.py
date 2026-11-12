"""Checked native integer language/IR/runtime source qualification.

Checks all 17 target C integer domains, error categories, transactional writes,
export ABI, JIT/generated C O0/O2, malformed IR and error allocation failure.
A partial source gate does not certify shipping VFS/embedded compiler bytes.
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
        objects=[row['Path']for row,name in zip(d['Objects'],d['Sources'])if Path(name).name!='xlir_public_source_driver.c']
        shutil.copy2(runtime,out/runtime.name)
    case=root/'test/cases/xlir_c_integer_arithmetic.xl';provider=case.with_suffix('.c');header=case.with_suffix('.h');host=case.with_name(case.stem+'_host.c');ircase=case.with_name(case.stem+'_ir.c')
    inputs={str(v):sha(v)for v in(driver,public/'build.json',runtime,case,provider,header,host,ircase,Path(__file__).resolve())}
    oomHost=case.with_name(case.stem+'_oom_host.c')
    inputs[str(oomHost)]=sha(oomHost)
    for name in json.loads((public/'freeze.json').read_text())['overrides']:
        assert sha(source/name)==sha(root/name),name
        inputs[str(source/name)]=sha(source/name);inputs[str(root/name)]=sha(root/name)
    report=dict(complete=False,passed=False,fullCompilerBuild=False,shippingDelivery=False,scope=__doc__,sourceBuild=str(public/'build.json'),inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'};env['XL6_SOURCE_NATIVE_ROOT']=str(source)
    if not win:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    def run(name,cmd,refused=False):
        cmd=list(map(str,cmd));actual=command_with_response(cmd,cmd[0],out/(name+'.rsp'))if Path(cmd[0]).name.lower()in('gcc','gcc.exe','g++','g++.exe')else cmd
        start=time.monotonic();z=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,refused=refused,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert (z.returncode!=0)==refused,log
        return z.stdout+z.stderr
    cc=shutil.which('gcc');cpp=shutil.which('g++');assert cc and cpp
    includes=['-I'+str(v)for v in(out,root,source,public/'include',sdk,sdk/'include')]
    common=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
    warnings=['-Wno-'+n for n in('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if win else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    try:
        generated=out/'generated.c';run('cgen',[driver,case,'cgen-default',generated]);run('ir',[driver,case,'ir-lowered',out/'lowered.json']);run('header',[driver,case,'emit',out/'public']);run('jit',[driver,case,'run',out/'jit-unused'])
        for level in('O0','O2'):
            exe=out/(level+('.exe'if win else'.out'))
            run(level+'-build',[cc,*common,*warnings,'-'+level,generated,provider,host,*objects,runtime,*systems,'-o',exe]);run(level+'-execute',[exe])
            oom=out/('oom-'+level+('.exe'if win else''))
            run('oom-'+level+'-build',[cc,*common,*warnings,'-'+level,generated,provider,oomHost,*objects,runtime,*systems,'-Wl,--wrap=xl6ErrorCreateV1','-o',oom]);run('oom-'+level+'-execute',[oom])
            ir=out/('ir-'+level+('.exe'if win else'.out'))
            run('ir-'+level+'-build',[cc,*common,'-'+level,ircase,*objects,runtime,*systems,'-o',ir]);run('ir-'+level+'-execute',[ir,case])
        for standard,tool,language in(('c11',cc,'c'),('c++11',cpp,'c++'),('c++17',cpp,'c++')):
            run('header-'+standard,[tool,'-std='+standard,'-Wall','-Wextra','-Werror',*includes,'-x',language,'-fsyntax-only',out/'public.h'])
        failure=root/'test/cases/xlir_c_integer_arithmetic_failure.c'
        inputs[str(failure)]=sha(failure)
        for level in ('O0','O2'):
            program=out/('failed-allocation-'+level+('.exe'if win else''))
            run('failed-allocation-'+level+'-build',[cc,*common,'-'+level,failure,'-o',program])
            run('failed-allocation-'+level+'-execute',[program])
        for standard,tool,language in (('c11',cc,'c'),('c++11',cpp,'c++'),('c++17',cpp,'c++')):
            run('runtime-header-'+standard,[tool,'-std='+standard,'-Wall','-Wextra','-Werror',*includes,'-x',language,'-fsyntax-only',public/'include/__xl_runtime/xl6_c_integer_arithmetic.h'])
        text=generated.read_text(encoding='utf-8')
        assert 'xl6CIntArithmeticV1(' in text and 'xl6CIntShiftV1(' in text
        for name in ('xlir_checked_integer_arithmetic_abi','xlir_checked_integer_compound_error_abi','xlir_scalar_numeric_closure'):
            regression=root/'test/cases'/(name+'.xl');inputs[str(regression)]=sha(regression)
            run('regression-'+name,[driver,regression,'run',out/name])
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,passed=False,error=str(e));raise
    finally:save()
if __name__=='__main__':main()
