"""Native integer checked outcomes in async and generator execution machines.

Partial source JIT and O0/O2 generated C. The generator publishes int values;
this does not add or certify the currently unsupported generator<c_int> carrier.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('root','public','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,public,out=[v.resolve()for v in(a.root,a.public,a.out)]
    d=json.loads((public/'build.json').read_text(encoding='utf-8-sig'));assert d['Complete']and d['InputsUnchanged']
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    for row in d['Inputs']+d['Headers']+d['Objects']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    out.mkdir(parents=True,exist_ok=False);win=os.name=='nt';driver=Path(d['Driver']);source=Path(d['SourceRoot'])
    runtime=public/'xl.dll'if win else Path(d['RuntimeLibrary']);sdk=Path(d['RuntimeDirectory']if win else d['SdkDirectory'])
    if win:shutil.copy2(runtime,out/runtime.name)
    objects=[row['Path']for row in d['Objects']if Path(row.get('Source','')).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    if win:objects=[row['Path']for row,name in zip(d['Objects'],d['Sources'])if Path(name).name!='xlir_public_source_driver.c']
    case=root/'test/cases/xlir_c_integer_continuation.xl';cc=shutil.which('gcc');assert cc
    inputs={str(v):sha(v)for v in (case,driver,Path(cc),public/'build.json',Path(__file__).resolve())}
    report=dict(complete=False,passed=False,fullCompilerBuild=False,shippingDelivery=False,scope=__doc__,inputs=inputs,rows=[])
    save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'};env['XL6_SOURCE_NATIVE_ROOT']=str(source)
    if not win:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
    def run(name,args):
        args=list(map(str,args));actual=command_with_response(args,args[0],out/(name+'.rsp'))if args[0]==cc else args;start=time.monotonic()
        z=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=240);log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=args,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
    try:
        generated=out/'generated.c';run('cgen',[driver,case,'cgen-default',generated]);run('jit',[driver,case,'run',out/'jit'])
        entry=out/'entry.c';entry.write_text('#include <assert.h>\ntypedef struct xvalue xvalue;\nextern int xl_script_module_init(xvalue*);\nextern int xl_script_module_unit_checked(void);\nextern long long native_continuation_count(void);\nint main(void){assert(xl_script_module_init(0));assert(native_continuation_count()==1);assert(xl_script_module_unit_checked());return 0;}\n')
        includes=['-I'+str(v)for v in (root,source,public/'include',sdk,sdk/'include')]
        warnings=['-Wno-'+n for n in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
        systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if win else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        for opt in ('O0','O2'):
            program=out/(opt+('.exe'if win else''));run(opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+opt,*warnings,*includes,generated,entry,*objects,runtime,*systems,'-o',program]);run(opt+'-execute',[program])
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,passed=False,error=str(e));raise
    finally:save()

if __name__=='__main__':main()
