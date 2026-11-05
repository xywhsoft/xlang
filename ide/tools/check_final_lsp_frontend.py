"""Focused CPU checks over the normal final-cohort analysis archive."""
from pathlib import Path
import argparse,hashlib,json,subprocess,time
p=argparse.ArgumentParser(description=__doc__)
for name in ('build','core','out'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--packages',type=Path)
p.add_argument('--only-package',action='store_true',help='Supplement only the matching-package fixture after a harness correction')
a=p.parse_args();build,core,out=[getattr(a,n).resolve() for n in ('build','core','out')];out.mkdir(exist_ok=False)
ide=Path(__file__).resolve().parents[1];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();data=json.loads((build/'build-result.json').read_text());assert data['status']=='passed'
command=next(c for c in data['commands'] if '-c' in c and any(Path(v).name=='check.c' for v in c));flags=command[1:command.index('-c')]
link=next(c for c in data['commands'] if any(Path(v).name=='xlang-ls.exe' for v in c) and '-o' in c);libs=[v for v in link if v.startswith('-l')]
fixture=ide/'tests/analysis_final_interop_test.c';exe=out/'final-frontend.exe'
pins=[build/'build-result.json',build/'xlang-analysis-core.a',fixture,Path(__file__).resolve()]
inputs={str(p):sha(p) for p in pins};report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[])
def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,cmd,expected=0):
 start=time.monotonic();q=subprocess.run(list(map(str,cmd)),capture_output=True,cwd=core,timeout=120);log=out/(name+'.log');log.write_bytes(q.stdout+q.stderr)
 report['rows'].append(dict(name=name,exitCode=q.returncode,expected=expected,command=list(map(str,cmd)),seconds=time.monotonic()-start,logSHA256=sha(log)));save();print(name,q.returncode,flush=True)
 return q.returncode==expected
assert run('client-build',[command[0],*flags,fixture,build/'xlang-analysis-core.a','-Wl,--gc-sections',*libs,'-o',exe])
for name in (() if a.only_package else ('foreign_module_code_owner','foreign_array_projection','foreign_output_record','foreign_output_record_size','foreign_pack','foreign_pack_pod','native_sequence_contract')):
 source=core/'test/cases'/(name+'.xl');inputs[str(source)]=sha(source);run(name,[exe,source,'1'])
if not a.only_package:
 bad=out/'bad-code-owner.xl';bad.write_text('extern "bad" @header("native.h") @c_code_owner(-1) func bad()\nfunc main(...) as int\n return 0\nend\n');inputs[str(bad)]=sha(bad);run('bad-code-owner',[exe,bad,'0'])
if a.packages:
 packages=a.packages.resolve();sample=out/'package-sample.xl';sample.write_text('import xui\nimport xge\nexport func inspect(xge.DataObject data) as int\n return int(data.formatCount)\nend\nfunc main(...) as int\n xui.Rect bounds=xui.Rect()\n bounds.fW=c_int(10)\n return int(bounds.fW)\nend\n');inputs[str(sample)]=sha(sample)
 for path in packages.rglob('*'):
  if path.is_file() and path.suffix in ('.xl','.json'):inputs[str(path)]=sha(path)
 run('matching-xge-xui-package',[exe,sample,'1',packages])
report['complete']=True;report['inputsUnchanged']=all(sha(name)==value for name,value in inputs.items());report['passed']=report['inputsUnchanged'] and all(r['exitCode']==r['expected'] for r in report['rows']);save();raise SystemExit(not report['passed'])
