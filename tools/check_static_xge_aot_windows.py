"""Actual Windows static XGE PE from the qualified production compiler image.

CPU-only color, owned local DataObject/share/NUL and five volatile CRT math
numeric calls. Native compatibility definitions may satisfy these statically;
the separate math-only PE gate checks five actual DLL imports. This PE is inspected and executed from a relocated directory containing
only the program, with package/provider/SDK overrides removed. This explicitly
emits and executes TinyCC PE; use only when that action is authorized.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in('root','fixtures-root','production','headers','packages','out'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();assert os.name=='nt';root,fixtures,production,headers,packages,out=[v.resolve()for v in(a.root,a.fixtures_root,a.production,a.headers,a.packages,a.out)]
 out.mkdir(exist_ok=False,parents=True);host=out/'host';host.mkdir();work=out/'empty-work';(work/'build/test').mkdir(parents=True);relocated=out/'relocated';relocated.mkdir()
 cc=Path(shutil.which('gcc'));objdump=Path(shutil.which('objdump'));inputs={str(p):sha(p)for p in(Path(__file__).resolve(),cc,objdump)}
 for name in('xl.dll','xlc.dll'):shutil.copy2(production/name,host/name);inputs[str(production/name)]=sha(production/name)
 for source in headers.rglob('*'):
  if source.is_file()and source.suffix in('.h','.inc','.def'):
   destination=host/'include'/source.relative_to(headers);destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,destination);inputs[str(source)]=sha(source)
 for source in(root/'release/win_x64/include').glob('*.h'):
  shutil.copy2(source,host/'include'/source.name);inputs[str(source)]=sha(source)
 assert not any(p.suffix in('.c','.o')for p in(host/'include').rglob('*')if p.is_file())
 dist=root/'res/tcc/win64/x86_64';shutil.copytree(dist,work/'res/tcc/win64/x86_64');inputs.update((str(p),sha(p))for p in dist.rglob('*')if p.is_file())
 inputs.update((str(p),sha(p))for p in packages.rglob('*')if p.is_file())
 package=json.loads((packages/'_xge_native/xlang.package.json').read_text());assert package['loadlibs']==[]and package['native']['linkage']=='static'
 entry=root/'test/cases/xlir_public_source_driver.c';inputs[str(entry)]=sha(entry)
 for name,target in [('static_xge_aot_windows.xl',out/'program.xl'),('static_xge_aot_math.c',out/'math.c'),('static_xge_aot_math.h',host/'include/static_xge_aot_math.h')]:
  source=fixtures/'test/cases'/name;shutil.copy2(source,target);inputs[str(source)]=sha(source)
 env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k not in('XLANG_PACKAGE_PATH','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')};env.update(XL6_SOURCE_NATIVE_ROOT=str(work),XLANG_PACKAGE_PATH=str(packages),TEMP=str(out),TMP=str(out),PYTHONDONTWRITEBYTECODE='1')
 report=dict(scope=__doc__,complete=False,passed=False,inputs=inputs,rows=[],profile='Real production XL_BUILD_DLL/XL_STATIC_TCC compiler; GCC O2 public API diagnostic host only, no implementation objects.',productionImages={n:sha(production/n)for n in('xl.dll','xlc.dll')},packages=str(packages),peEmitted=False,peExecuted=False,shippingDelivery=False)
 def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 def run(name,command,cwd=work,environment=env):
  start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=environment,capture_output=True,timeout=600);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr);report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,r.returncode,flush=True)
  if r.returncode:raise RuntimeError(str(log))
  return r.stdout.decode(errors='replace')
 driver=host/'public_source.exe';run('public-host-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-O2','-I'+str(host),'-I'+str(host/'include'),entry,host/'xlc.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',driver])
 imports=run('public-host-imports',[objdump,'-p',driver]);assert'DLL Name: xlc.dll'in imports and'DLL Name: xl.dll'not in imports
 for mode in('emit','exe'):run('static-'+mode,[driver,out/'program.xl',mode,out/'static-program',out/'math.c'])
 binary=out/'static-program.exe';report['peEmitted']=True;pe=run('static-imports',[objdump,'-p',binary]);dlls=re.findall(r'DLL Name:\s*(\S+)',pe);assert dlls
 assert not any(re.match(r'^(?:lib)?(?:xge[^/]*|xlc?|tcc[^/]*)\.dll$',name.lower())for name in dlls),dlls
 allowed={match.group(1).lower()for path in(packages/'_xge_native/native').glob('*.def')if(match:=re.search(r'^LIBRARY\s+(\S+)',path.read_text(),re.M))}
 assert all(name.lower()in allowed and(Path(os.environ['SystemRoot'])/'System32'/name).is_file()for name in dlls),dlls
 report['importDlls']=dlls
 report['mathImports']=[name for name in('sin','cos','sqrt','pow','fmod')if re.search(r'\b'+name+r'\s*$',pe,re.M)]
 report['mathNumericCoverage']=['sin','cos','sqrt','pow','fmod']
 report['mathResolutionScope']='Volatile numeric assertions execute in the full program. Genuine Native CRT compatibility definitions are valid; actual DLL imports are recorded separately, not required for these definitions.'
 run('static-sections',[objdump,'-h',binary])
 target=relocated/'program.exe';shutil.copy2(binary,target);assert list(relocated.iterdir())==[target]
 standalone={k:v for k,v in env.items()if not k.startswith('XL6_')and k not in('XLANG_PACKAGE_PATH','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')};standalone['PATH']=str(Path(os.environ['SystemRoot'])/'System32')+';'+os.environ['SystemRoot']
 run('relocated-standalone',[target],relocated,standalone);report.update(complete=True,passed=True,peExecuted=True,inputsUnchanged=all(sha(p)==v for p,v in inputs.items()),binary=dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size),relocatedBinary=dict(path=str(target),sha256=sha(target)));assert report['inputsUnchanged'];save();print(out/'results.json',sha(out/'results.json'))
if __name__=='__main__':main()
