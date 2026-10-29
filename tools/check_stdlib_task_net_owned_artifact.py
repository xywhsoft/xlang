"""Verify the nine owned TaskNet native mappings at O0/O2: root out gcc.
No language/shipping or network-traffic acceptance is claimed.
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
root=Path(sys.argv[1]);out=Path(sys.argv[2]);cc=Path(sys.argv[3]);assert not out.exists();out.mkdir(parents=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
files=[root/'test/support/net_task_owned_artifact.c',Path(__file__).resolve(),cc]
files+=[f for f in (root/'src/compile').rglob('*') if f.is_file() and f.suffix in ('.c','.h','.inc')]
files+=[f for f in (root/'src/ir').glob('*.h')]
inputs={str(p):sha(p) for p in files};rows=[];products=[]
def run(name,args):
 log=out/(name+'.log')
 with log.open('wb') as f:r=subprocess.run(list(map(str,args)),cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=60)
 rows.append(dict(Name=name,Exit=r.returncode,Log=str(log),Sha256=sha(log),Command=list(map(str,args))));assert r.returncode==0,log
for opt in ('O0','O2'):
 binary=out/(opt+('.exe' if os.name=='nt' else ''))
 run(opt+'.build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-'+opt,
 '-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
 '-I'+str(root),root/'test/support/net_task_owned_artifact.c',root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',binary])
 products.append(dict(Path=str(binary),Sha256=sha(binary)));run(opt+'.run',[binary])
 assert 'TaskNet native artifact map: 9 exact APIs' in (out/(opt+'.run.log')).read_text()
assert all(sha(p)==h for p,h in inputs.items())
report=dict(Complete=True,InputsUnchanged=True,LogsUnchanged=True,ProductsUnchanged=True,Inputs=inputs,Rows=rows,Products=products,NetworkTraffic=False)
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(Complete=True,Stages=len(rows),Report=str(out/'results.json'),Sha256=sha(out/'results.json')),indent=2))
