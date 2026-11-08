"""Reject actual C allocation-policy omissions against full function-pool tests."""
from pathlib import Path
import argparse,json,subprocess,shutil
from build_lsp_lifecycle_profile import verify_build,sha
from check_native_bridge_catalog import function


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 b=json.loads((build/'build-result.json').read_text(encoding='utf-8'));verify_build(b,build);ide=Path(__file__).resolve().parents[1]
 materialize=ide/'src/analysis/ast_materialize.c';text=materialize.read_text(encoding='utf-8')
 compile=next(c for c in b['commands'] if '-c' in c and str(materialize) in c)
 fixtureObject=str(build/'analysis_function_pool_test.o');link=next(c for c in b['commands'] if '-c' not in c and fixtureObject in c)
 protected={**b['inputs'],str(Path(__file__).resolve()):sha(__file__)};report=dict(status='running',scope=__doc__,commands=[],cases=[],protected=protected)
 def save():(out/'result.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
 def run(name,cmd,expected=0):
  p=subprocess.run(cmd,cwd=out,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(p.stdout+p.stderr);report['commands'].append(dict(name=name,command=cmd,exit=p.returncode,expectedExit=expected,logSha256=sha(log)));save()
  assert p.returncode==expected,(name,p.returncode,p.stderr[-1800:])
  if expected:assert b'analysis_' in p.stderr and b'test:' in p.stderr,(name,p.stderr[-1800:])
  print(name,p.returncode,flush=True)
 selector=function(text,'separatePool')
 variants=[('disable-function-partition',selector,selector.replace('nodes>=XLS_MATERIALIZE_SPLIT_NODES','nodes>=XLS_MATERIALIZE_SPLIT_NODES && 0')),('omit-backing-charge','active->work.chargedBytes+=work.chargedBytes;','(void)work.chargedBytes;'),('omit-unpin','xlsAstStorageUnpin(scope->storage);scope->storage=NULL;','scope->storage=NULL;'),('ignore-parent-quota','xls_ast_materialize_scope tree;xlsAstMaterializeBegin(&tree,xlsAstMaterializeBudget());','xls_ast_materialize_scope tree;xlsAstMaterializeBegin(&tree,xlsAstMaterializeBudget());tree.limit=64u*1024u*1024u;')]
 try:
  run('complete-normal-control',[str(build/'analysis_function_pool_test.exe')]);report['cases'].append('complete-normal-control');save()
  for name,before,after in variants:
   assert text.count(before)==1 and before!=after,name
   folder=out/name;folder.mkdir();source=folder/'ast_materialize.c';source.write_text(text.replace(before,after,1),encoding='utf-8',newline='\n');protected[str(source)]=sha(source)
   obj=folder/'ast_materialize.o';cmd=compile.copy();cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(obj);run(name+'-compile',cmd)
   core=folder/'core.a';shutil.copy2(build/'xlang-analysis-core.a',core);run(name+'-archive',[str(Path(b['compiler']).with_name('ar.exe')),'rcs',str(core),str(obj)])
   cmd=[str(core) if v==str(build/'xlang-analysis-core.a') else v for v in link];program=folder/'fixture.exe';cmd[cmd.index('-o')+1]=str(program);run(name+'-link',cmd);run(name+'-run',[str(program)],1);report['cases'].append(name+'-rejected');save()
  verify_build(b,build)
  for path,h in protected.items():assert sha(path)==h,path
  report['status']='passed'
 except BaseException as error:report.update(status='failed',error=repr(error));raise
 finally:save()
 print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']))))


if __name__=='__main__':main()
