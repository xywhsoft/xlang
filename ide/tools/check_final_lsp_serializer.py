"""Exercise the actual generated IR codec with its native shared warning profile."""
from pathlib import Path
import argparse,hashlib,json,subprocess,time
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();build=a.build.resolve();out=a.out.resolve();out.mkdir(exist_ok=False);ide=Path(__file__).resolve().parents[1]
data=json.loads((build/'build-result.json').read_text());assert data['status']=='passed'
compile=next(c for c in data['commands'] if '-c' in c and any(Path(v).name=='xl6_ir.c' for v in c));flags=['-O0' if v=='-O2' else v for v in compile[1:compile.index('-c')]]
link=next(c for c in data['commands'] if '-o' in c and Path(c[-1]).name=='xlang-ls.exe');libs=[v for v in link if v.startswith('-l')]
fixture=ide/'tests/analysis_ir_sparse_decode_test.c';exe=out/'serializer.exe';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins=[Path(__file__).resolve(),fixture,build/'frontend/xl6_ir.c',build/'xlang-analysis-core.a',build/'build-result.json',ide/'src/analysis/frontend_ir_memo.inc',ide/'src/analysis/frontend_ir_persistent.inc']
inputs={str(path):sha(path) for path in pins};report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[])
for name,command in [('build',[compile[0],*flags,fixture,build/'xlang-analysis-core.a','-Wl,--gc-sections',*libs,'-o',exe]),('run',[exe])]:
 start=time.monotonic();q=subprocess.run(list(map(str,command)),cwd=out,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(q.stdout+q.stderr)
 report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=q.returncode,seconds=time.monotonic()-start,logSHA256=sha(log)));(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 if q.returncode:break
report.update(complete=True,inputsUnchanged=all(sha(name)==value for name,value in inputs.items()));report['passed']=report['inputsUnchanged'] and len(report['rows'])==2 and all(row['exitCode']==0 for row in report['rows']);(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');raise SystemExit(not report['passed'])
