"""PNG owning-byte declarations through source JIT and dynamic/static executables.

Explicit source-driver preflight with unchanged released TCC/fixed objects;
not a rebuilt shipping compiler/VFS or final SDK/IDE delivery certification.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in('root','source','public','dynamic-packages','static-packages','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root,source,public,dynamic,static,out=[v.resolve()for v in(a.root,a.source,a.public,a.dynamic_packages,a.static_packages,a.out)]
    out.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();win=os.name=='nt'
    d=json.loads((public/'build.json').read_text(encoding='utf-8-sig'));assert d['Complete']and d['InputsUnchanged']and d['SourceXrt']
    for row in d['Inputs']+d['Headers']+d['Objects']:assert sha(row['Path'])==row['Sha256'].lower(),row['Path']
    driver=Path(d['Driver']);assert sha(driver)==d['DriverSha256'].lower();objdump=shutil.which('objdump');assert objdump
    case=root/'test/cases/foreign_xge3_image_owned_bytes.xl';example=root/'lib/bindings/xge/examples/encode_image_png.xl'
    files=[driver,public/'build.json',Path(__file__).resolve(),Path(objdump),case,example,root/'test/cases/foreign_xge3_image_io.h',root/'test/cases/foreign_xge3_image_io.c']
    for folder in(dynamic,static):files.extend(v for v in folder.rglob('*')if v.is_file())
    inputs={str(v):sha(v)for v in files};report=dict(complete=False,passed=False,fullCompilerBuild=False,shippingDelivery=False,scope=__doc__,inputs=inputs,rows=[],programs=[])
    save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,cmd,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env.update(XL6_SOURCE_NATIVE_ROOT=str(source),XLANG_PACKAGE_PATH=str(packages))
        variable='PATH'if win else'LD_LIBRARY_PATH';env[variable]=str(packages/'_xge_native/native')+os.pathsep+str(Path(d['TccCore']).parent)+os.pathsep+env.get(variable,'')
        start=time.monotonic();cmd=list(map(str,cmd));z=subprocess.run(cmd,cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    try:
        for file in(case,example):
            sidecars=[root/'test/cases/foreign_xge3_image_io.c']if file==case else[]
            name=file.stem;generated=out/(name+'.c');run(name+'-emit',[driver,file,'cgen-default',generated],dynamic)
            assert 'xgeImageEncodePNGEx('in generated.read_text(encoding='utf-8-sig')and 'xl6COutputBytesTakeV1('in generated.read_text(encoding='utf-8-sig')
            run(name+'-jit',[driver,file,'run',out/(name+'-jit-unused'),*sidecars],dynamic)
            for mode,packages in(('dynamic',dynamic),('static',static)):
                stem=out/(name+'-'+mode);program=stem.with_suffix('.exe')if win else stem
                run(name+'-'+mode+'-build',[driver,file,'exe',stem,*sidecars],packages)
                run(name+'-'+mode+'-execute',[program],packages);data=run(name+'-'+mode+'-imports',[objdump,'-p',program],packages)
                names=re.findall(rb'DLL Name:\s*(\S+)',data)if win else re.findall(rb'\bNEEDED\s+(\S+)',data);lower={v.lower()for v in names}
                assert not{b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host'in v for v in lower)==(mode=='dynamic')
                report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[v.decode()for v in names]))
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as e:report.update(complete=True,error=str(e));raise
    finally:save()

if __name__=='__main__':main()
