"""Exercise actual optional IR ownership-index API failure seams.

Use a frozen build_lsp_core output. The diagnostic IR copy changes only the
private prepare function name; the fixture wraps its XRT array calls. The
normal production object has no instrumentation. No language rule changes.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=Path,required=True);parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();native=args.native.resolve(strict=True);source=args.source.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    read=lambda p:json.loads(Path(p).read_text(encoding='utf8'));sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    raw=read(native/'build-result.json');assert raw['status']=='passed'
    compiled=native/'frontend/xl6_ir.c';memo=source/'src/analysis/frontend_ir_memo.inc';alternate=out/'include/analysis/frontend_ir_memo.inc';alternate.parent.mkdir(parents=True)
    original=memo.read_text();old='static int xlsIrOwnerPrepare(';assert original.count(old)==1
    alternate.write_text(original.replace(old,'static int xlsIrOwnerPrepareOriginal('),encoding='utf8')
    template=next(c for c in raw['commands'] if '-c' in c and c[c.index('-o')+1].endswith('analysis_ir_memo_test.o'))
    prefix=[template[0],'-I'+str(out/'include'),'-I'+str(source/'src'),*template[1:template.index('-c')]]
    fixture=source/'tests/analysis_ir_owner_fault_test.c';obj=out/'owner_fault.o';exe=out/'owner_fault.exe';dependency=out/'owner_fault.d'
    cmd=[*prefix,'-DXLS_IR_OWNER_FIXTURE="'+compiled.as_posix()+'"','-MMD','-MT','ownerObject','-MF',str(dependency),'-c',str(fixture),'-o',str(obj)]
    link=next(c for c in raw['commands'] if '-c' not in c and '-o' in c and c[c.index('-o')+1].endswith('analysis_ir_memo_test.exe'))
    link=[str(obj) if x.endswith('analysis_ir_memo_test.o') else str(native/'xlang-analysis-core.a') if x.endswith('xlang-analysis-core.a') else x for x in link];link[link.index('-o')+1]=str(exe)
    commands=[];report=dict(status='failed',scope=__doc__,commands=commands,inputs={str(p):sha(p) for p in [fixture,memo,alternate,compiled,native/'xlang-analysis-core.a',source/'tests/analysis_ir_memo_test.c']})
    try:
        for name,command in [('compile',cmd),('link',link),('run',[str(exe)])]:
            cp=subprocess.run(command,capture_output=True,cwd=out,timeout=900);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr)
            commands.append(dict(name=name,command=command,exit=cp.returncode,log=str(log),logSha256=sha(log)))
            print(name,cp.returncode,(cp.stdout+cp.stderr).decode('utf8','replace')[-1600:],flush=True);assert not cp.returncode,name
        # Reuse the producer's compiler dependency parser for the real .d
        # closure,including this diagnostic copy andall unchanged headers.
        import sys
        sys.path.insert(0,str(source/'tools'));from build_lsp_core import dependencies
        report['inputs'].update({str(p):sha(p) for p in dependencies(dependency.read_text())})
        assert all(sha(p)==h for p,h in report['inputs'].items());report['outputs']={str(p):sha(p) for p in [obj,exe,dependency]};report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if __name__=='__main__':main()
