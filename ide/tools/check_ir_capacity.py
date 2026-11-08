"""Reject real C capacity-policy omissions with the complete native fixture."""
from pathlib import Path
import argparse,json,subprocess
from build_lsp_lifecycle_profile import verify_build,sha


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 b=json.loads((build/'build-result.json').read_text(encoding='utf-8'));verify_build(b,build);ide=Path(__file__).resolve().parents[1]
 implementation=ide/'src/analysis/frontend_ir_capacity.inc';source=ide/'tests/analysis_ir_capacity_test.c';text=implementation.read_text(encoding='utf-8')
 compile=next(c for c in b['commands'] if '-c' in c and str(source) in c);fixture=str(build/'analysis_ir_capacity_test.o');link=next(c for c in b['commands'] if '-c' not in c and fixture in c)
 protected={**b['inputs'],str(Path(__file__).resolve()):sha(__file__)};report=dict(status='running',scope=__doc__,commands=[],cases=[],protected=protected)
 def save():(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
 def run(name,cmd,expected=0):
  result=subprocess.run(cmd,cwd=out,capture_output=True,timeout=240);log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr);report['commands'].append(dict(name=name,command=cmd,exit=result.returncode,expectedExit=expected,logSha256=sha(log)));save()
  assert result.returncode==expected,(name,result.returncode,result.stderr[-2000:])
  if expected:assert b'analysis_worker_cache_test:' in result.stderr,(name,result.stderr[-1800:])
  print(name,result.returncode,flush=True)
 variants=[('omit-current-input-identity','proof->identity!=identity ||',''),('publish-refused-allocation','if(grown){*items=grown;*capacity=next;}','{*items=grown;*capacity=next;}'),('round-capacity-down','while(next<count)','while(next<count/2)'),('reserve-sufficient-capacity','count<=*capacity','0')]
 try:
  run('complete-normal-control',[str(build/'analysis_ir_capacity_test.exe')]);report['cases'].append('complete-normal-control');save()
  for name,before,after in variants:
   assert text.count(before)==1 and before!=after,name
   folder=out/name;private=folder/'analysis/frontend_ir_capacity.inc';private.parent.mkdir(parents=True);private.write_text(text.replace(before,after,1),encoding='utf-8',newline='\n');protected[str(private)]=sha(private)
   obj=folder/'fixture.o';cmd=compile.copy();cmd.insert(1,'-I'+str(folder));cmd[cmd.index('-o')+1]=str(obj)
   for flag in ('-MF','-MT'):
    if flag in cmd:cmd[cmd.index(flag)+1]=str(folder/('fixture.d' if flag=='-MF' else 'fixture.o'))
   run(name+'-compile',cmd);cmd=[str(obj) if value==fixture else value for value in link];program=folder/'fixture.exe';cmd[cmd.index('-o')+1]=str(program);run(name+'-link',cmd);run(name+'-run',[str(program)],1);report['cases'].append(name+'-rejected');save()
  verify_build(b,build)
  for path,h in protected.items():assert sha(path)==h,path
  report['status']='passed'
 except BaseException as error:report.update(status='failed',error=repr(error));raise
 finally:save()
 print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']))))


if __name__=='__main__':main()
