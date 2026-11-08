"""Check native parameter bit enumeration using actual C producer mutations."""
from pathlib import Path
import argparse,json,subprocess
from build_lsp_lifecycle_profile import verify_build,sha
from check_native_bridge_catalog import function


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 b=json.loads((build/'build-result.json').read_text(encoding='utf-8'));verify_build(b,build)
 ide=Path(__file__).resolve().parents[1];fixture=ide/'tests/analysis_ir_parameters_test.c'
 compile=next(c for c in b['commands'] if '-c' in c and str(fixture) in c);obj=compile[compile.index('-o')+1];link=next(c for c in b['commands'] if '-c' not in c and obj in c)
 report=dict(status='running',scope=__doc__,cases=[],commands=[],protected={**b['inputs'],str(Path(__file__).resolve()):sha(__file__)})
 def save():(out/'result.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
 def run(name,cmd,expected=0):
  p=subprocess.run(cmd,cwd=out,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(p.stdout+p.stderr);report['commands'].append(dict(name=name,command=cmd,exit=p.returncode,expectedExit=expected,logSha256=sha(log)));save()
  assert p.returncode==expected,(name,p.returncode,p.stderr[-1800:])
  if expected:assert b'analysis_ir_parameters_test:' in p.stderr,(name,p.stderr[-1000:])
  print(name,p.returncode,flush=True)
 source=out/'supplement.c'
 source.write_text('#define main xlsParameterBaseMain\n#include '+json.dumps(fixture.as_posix())+'\n#undef main\n'+'''
int main(void){
#ifdef XLS_NATIVE_IR_PARAMETER_BITS
 if(!run())return 1;
 XL6_IrNode nodes[3]={0};XL6_IrModule module={0};module.nodes=nodes;module.nodeCount=module.nodeCapacity=3;
 for(unsigned bit=0;bit<64;++bit){
  nodes[2].foreignCall.retainedCallbacks=UINT64_C(1)<<bit;
  if(!compare(&module,0,UINT64_C(1)<<bit,1))return 1;
 }
 printf("parameter supplement passed: %u checks; retained-only missing rows on every bit\\n",checks);
#endif
 return 0;
}
''',encoding='utf-8',newline='\n');report['protected'][str(source)]=sha(source)
 ir=(build/'frontend/xl6_ir.c').read_text(encoding='utf-8');body=function(ir,'xl6IrVerifyForeignParameters')
 variants=[('control',None),('omit-retained-mask',body.replace('declaration->foreignCall.physicalPairs|declaration->foreignCall.retainedCallbacks','declaration->foreignCall.physicalPairs')),('truncate-high-bits',body.replace('uint64 xlsParameterBits=mask','uint32 xlsParameterBits=(uint32)mask')),('omit-row-requirement',body.replace('if(found!=1)goto invalid;','(void)found;'))]
 try:
  run('normal-full-control',[str(build/'analysis_ir_parameters_test.exe')]);report['cases'].append('complete-normal-control');save()
  for name,mutated in variants:
   folder=out/name;include=folder/'include';include.mkdir(parents=True);target=folder/'fixture.o';program=folder/'fixture.exe'
   if mutated is not None:
    assert body!=mutated,name;path=include/'xl6_ir.c';path.write_text(ir.replace(body,mutated,1),encoding='utf-8',newline='\n');report['protected'][str(path)]=sha(path)
   cmd=compile.copy();cmd.insert(1,'-I'+str(include));cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(target);run(name+'-compile',cmd)
   cmd=[str(target) if v==obj else v for v in link];cmd[cmd.index('-o')+1]=str(program);run(name+'-link',cmd);run(name+'-run',[str(program)],0 if mutated is None else 1)
   report['cases'].append(name+'-passed' if mutated is None else name+'-rejected');save()
  verify_build(b,build)
  for path,h in report['protected'].items():assert sha(path)==h,path
  report['status']='passed'
 except BaseException as error:report.update(status='failed',error=repr(error));raise
 finally:save()
 print(json.dumps(dict(status=report['status'],cases=len(report['cases']),commands=len(report['commands']))))


if __name__=='__main__':main()
