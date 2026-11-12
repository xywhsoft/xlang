"""Freeze the real provider; exercise duplicate ELF/object/archive error delivery.

The before control requires the old incorrect success return with real recorded
diagnostics. The fixed control requires a negative public API result, including
the library-search path. Fresh valid object JIT execution remains mandatory.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys
p=argparse.ArgumentParser(description=__doc__)
for n in ('provider-root','out'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--before',action='store_true');a=p.parse_args()
r=a.provider_root.resolve();out=a.out.resolve();case_root=Path(__file__).resolve().parents[1]
out.mkdir(parents=True,exist_ok=False);source=out/'source';shutil.copytree(r/'lib/tcc',source/'lib/tcc')
for n in ('test/cases/tcc_binary_error_propagation.c','tools/check_tcc_binary_errors.py'):
 q=source/n;q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(case_root/n,q)
sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
inputs={v.relative_to(source).as_posix():sha(v)for v in source.rglob('*')if v.is_file()}
report=dict(complete=False,passed=False,beforeControl=a.before,shippingDelivery=False,scope=__doc__,inputs=inputs,steps=[])
save=lambda:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
def run(name,cmd):
 cmd=list(map(str,cmd));z=subprocess.run(cmd,cwd=out,capture_output=True,timeout=180)
 log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr);report['steps'].append(dict(name=name,command=cmd,exitCode=z.returncode,logSha256=sha(log)));save();print(name,z.returncode,flush=True);assert not z.returncode,log
 return z.stdout
try:
 cc=shutil.which('gcc');assert cc
 for opt in ('O0','O2'):
  dest=out/opt;dest.mkdir();exe=dest/('host.exe'if os.name=='nt'else'host')
  cmd=[cc,'-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(source/'lib/tcc'),source/'test/cases/tcc_binary_error_propagation.c','-o',exe]
  cmd+=([source/'lib/tcc/tcc_utf8_io.c','-lws2_32']if os.name=='nt'else['-ldl','-lpthread','-lm'])
  run(opt+'-build',cmd);run(opt+'-emit',[exe,source/'lib/tcc',dest])
  run(opt+'-archive',['ar','rcs',dest/'libduplicate.a',dest/'first.o',dest/'second.o'])
  text=run(opt+'-verify',[exe,source/'lib/tcc',dest,'verify-before'if a.before else'verify'])
  m=re.search(rb'BINARY_ERROR_CHECKS=(\d+) FAILURES=0 BEFORE=(\d+)',text);assert m and int(m[2])==a.before
  report['steps'][-1]['checks']=int(m[1])
 report.update(complete=True,passed=True,checks=sum(s.get('checks',0)for s in report['steps']),inputsUnchanged=all(sha(source/n)==h for n,h in inputs.items()));assert report['inputsUnchanged']
except Exception as e:report.update(complete=True,error=str(e));raise
finally:save()
