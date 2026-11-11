"""Shape/ShapeScene four-corner output through real matched XGE products.

Uses a verified complete compiler and production native SDKs. CPU only; no
window, GPU, device, network, benchmark or private setup entry point.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
from full_compiler_qualification import verify_full_compiler


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','dynamic-packages','static-packages','out','objdump'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,compiler,cohort,dynamic,static,out,objdump=[p.resolve()for p in
        (args.root,args.compiler,args.compiler_report,args.dynamic_packages,args.static_packages,args.out,args.objdump)]
    verify_full_compiler(cohort,compiler);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    cases={'obb':root/'test/cases/foreign_xge3_fixed_output.xl','example':root/'lib/bindings/xge/examples/shape_bounds.xl'}
    files=[Path(__file__).resolve(),compiler,cohort,objdump,*cases.values()]
    for package in (dynamic,static):files.extend(p for p in package.rglob('*')if p.is_file())
    files.extend(compiler.parent/name for name in (('xl.dll','xlc.dll')if os.name=='nt'else('libxl.so','libxlc.so')))
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(passed=False,fullCompilerBuild=True,shippingDelivery=False,scope=__doc__,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,packages):
        env=dict(os.environ,XLANG_PACKAGE_PATH=str(packages));image=packages/'_xge_native/native'
        if os.name=='nt':env['PATH']=str(image)+';'+str(compiler.parent)+';'+env.get('PATH','')
        else:env['LD_LIBRARY_PATH']=str(image)+':'+str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
        command=list(map(str,command));r=subprocess.run(command,cwd=compiler.parent,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=r.returncode,logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
        return r.stdout+r.stderr
    include=['--inc-path',root,'--inc-path',compiler.parent/'include']
    for label,source in cases.items():
        c=out/(label+'.c');run(label+'-emit',[compiler,'cgen',source,'--emit',c,*include],dynamic)
        text=c.read_text()
        for name in ('xgeShapeExGetOBB','xgeShapeExSceneGetOBB')if label=='obb'else('xgeShapeExGetOBB',):
            calls=[line for line in text.splitlines()if ' = '+name+'('in line]
            assert calls and all('native_raw.Elements)'in line for line in calls),name
        run(label+'-jit',[compiler,'run',source,*include],dynamic)
        for kind,packages in [('dynamic',dynamic),('static',static)]:
            binary=out/(label+'-'+kind+('.exe'if os.name=='nt'else''))
            run(label+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*include],packages)
            raw=run(label+'-'+kind+'-run',[binary],packages)
            if label=='obb':assert b'all four corners'in raw
            else:assert all(('Corner '+str(i)).encode()in raw for i in range(4))
            if kind=='static':
                imports=run(label+'-imports',[objdump,'-p',binary],packages).decode(errors='replace').lower()
                assert all(p not in imports for p in ('xge3_host.dll','libxge3_host.so','xl.dll','xlc.dll','libxl.so','libxlc.so','libtcc.dll','libtcc.so'))
    report.update(passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));assert report['inputsUnchanged'];save()


if __name__=='__main__':main()
