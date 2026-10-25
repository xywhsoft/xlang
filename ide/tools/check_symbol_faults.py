"""Replace only the optional lexical-vector TU to check XRT failure fallback."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    native=a.native.resolve(strict=True);source=a.source.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    read=lambda p:json.loads(Path(p).read_text(encoding='utf8'));sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    raw=read(native/'build-result.json');assert raw['status']=='passed';commands=[]
    compile=next(c for c in raw['commands'] if '-c' in c and c[c.index('-o')+1].endswith('analysis_ir_memo_test.o'));compile=compile.copy()
    fixture=source/'tests/analysis_symbol_fault_test.c';implementation=source/'src/analysis/frontend_symbols.c';obj=out/'symbol_fault.o';exe=out/'symbol_fault.exe';dependency=out/'symbol_fault.d';mapfile=out/'symbol_fault.map'
    compile[compile.index('-c')+1]=str(fixture);compile[compile.index('-o')+1]=str(obj)
    compile=[compile[0],'-I'+str(source/'src'),*compile[1:compile.index('-c')],'-DXLS_SYMBOL_FIXTURE="'+implementation.as_posix()+'"','-MMD','-MT','symbolObject','-MF',str(dependency),*compile[compile.index('-c'):]]
    link=next(c for c in raw['commands'] if '-c' not in c and '-o' in c and c[c.index('-o')+1].endswith('analysis_ir_memo_test.exe'));link=[str(obj) if x.endswith('analysis_ir_memo_test.o') else str(native/'xlang-analysis-core.a') if x.endswith('xlang-analysis-core.a') else x for x in link];link[link.index('-o')+1]=str(exe);link+=['-Wl,-Map,'+str(mapfile)]
    report=dict(status='failed',scope=__doc__,normalProductInstrumented=False,commands=commands,inputs={str(p):sha(p) for p in [fixture,implementation,source/'tests/analysis_ir_memo_test.c',native/'xlang-analysis-core.a']})
    try:
        for name,command in [('compile',compile),('link',link),('run',[str(exe)])]:
            r=subprocess.run(command,capture_output=True,cwd=out,timeout=1200);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);commands.append(dict(name=name,command=command,exit=r.returncode,log=str(log),logSha256=sha(log)));print(name,r.returncode,(r.stdout+r.stderr).decode('utf8','replace')[-1800:],flush=True);assert not r.returncode,name
        # The diagnostic object supplies the real public helper definitions.
        # Its normal archive member must not also enter the resulting image.
        assert '(frontend_symbols.o)' not in mapfile.read_text()
        sys.path.insert(0,str(source/'tools'));from build_lsp_core import dependencies
        report['inputs'].update({str(p):sha(p) for p in dependencies(dependency.read_text())})
        assert all(sha(p)==h for p,h in report['inputs'].items());report['outputs']={str(p):sha(p) for p in [obj,exe,dependency,mapfile]};report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if __name__=='__main__':main()
