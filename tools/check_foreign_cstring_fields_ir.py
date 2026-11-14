"""Sealed CString field IR damage checks over a recorded compiler cohort.

This tests the supplied frontend objects; full O2/VFS delivery is qualified
separately by check_foreign_cstring_fields.py.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from foreign_validation_build import add_build_arguments,load_build
from compiler_response import command_with_response

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','out'):p.add_argument('--'+name,type=Path,required=True)
 add_build_arguments(p);a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();b=load_build(a.build,a.overlay)
 cohort,runtime=b['cohort'],b['runtime'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 out.mkdir(parents=True,exist_ok=False);env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort));win=os.name=='nt'
 if win:shutil.copy2(runtime,out/runtime.name)
 else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+env.get('LD_LIBRARY_PATH','')
 inputs=dict(b['inputs']);inputs[str(Path(__file__).resolve())]=sha(__file__)
 report=dict(complete=False,passed=False,scope=__doc__,fullSourceRebuild=b['fullSourceRebuild'],overlay=b['overlay'],inputs=inputs,rows=[])
 def save():
  t=out/'results.json.tmp';t.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(t,out/'results.json')
 def run(name,cmd):
  cmd=list(map(str,cmd));actual=command_with_response(cmd,'gcc',out/(name+'.rsp'))if cmd[0]=='gcc'else cmd
  start=time.monotonic();z=subprocess.run(actual,cwd=cohort,env=env,capture_output=True,timeout=360)
  log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
  report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
  save();print(name,z.returncode,flush=True);assert not z.returncode,log
 try:
  sdk=Path(b['base'].get('SdkDirectory')or b['base'].get('RuntimeDirectory')or runtime.parent)
  includes=['-I'+str(v)for v in(cohort,root,b['basePath'].parent/'include',sdk,sdk/'include')]
  flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*includes]
  systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']if win else['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
  for stem in ('foreign_cstring_fields','foreign_cstring_result'):
   case=root/'test/cases'/(stem+'.xl');gate=case.with_name(stem+'_ir.c')
   inputs.update((str(v),sha(v))for v in(case,gate,case.with_suffix('.h')))
   for level in ('O0','O2'):
    name=stem+'-'+level;binary=out/(name+('.exe'if win else''))
    run(name+'-build',['gcc',*flags,'-'+level,gate,*b['objects'],runtime,*systems,'-o',binary])
    run(name,[binary,case])
  report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
 except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
 finally:save()
if __name__=='__main__':main()
