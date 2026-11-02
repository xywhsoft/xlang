from pathlib import Path
import hashlib,json,os,re,subprocess,sys,time
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
old=json.loads((O/'gui-build-inputs.before.json').read_text('utf-8-sig'))
script=(R/'ide/tools/build.ps1').read_text('utf-8-sig');prefix=script.split('$buildInputs=@{}')[0]
sourceText,excluded=prefix.split('$cLspSources=',1)
sources=re.findall(r"'((?:src|tools)\\[^']+\.c)'",sourceText)
skip=set(re.findall(r"'((?:src|tools)\\[^']+\.c)'",excluded.split('if (-not $EnableCLsp)')[0]))
sources=[R/'ide'/s.replace('\\','/') for s in sources if s not in skip]
assert len(sources)==len(set(sources)) and len(sources)==len(old['objects']),(len(sources),len(old['objects']))
assert old['cLspEnabled'] is False
inputs={str(p):sha(p) for p in map(Path,old['inputs'])}
for p in sources+[Path(__file__),R/'ide/tests/sha256_test.c']:inputs[str(p)]=sha(p)
for p in [R/'release/data/commands.json',R/'release/data/catalog-sources/index.json']:inputs[str(p)]=sha(p)
baseline=json.loads((O/'baseline.json').read_text())
report=dict(status='running',scope='Targeted GUI SHA-256 repair only. Standard build stopped on pre-existing stale catalog/compiler binding; catalog data retained byte-for-byte. Not full catalog/engine/release admission.',compiler=old['compiler'],compilerSha256=sha(old['compiler']),resourceCompiler=old['resourceCompiler'],configuration='Release',cLspEnabled=False,flags=old['flags'],libraries=old['libraries'],inputs=inputs,objects=[],resources=[],commands=[],release=str(R/'release'))
def save():(O/'gui-rebuild.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
def verify():
 for path,digest in inputs.items():assert sha(path)==digest,path
 for path,digest in baseline['protected'].items():assert sha(path)==digest,path
def run(name,args,timeout=120):
 log=O/(name+'.log');row=dict(name=name,command=list(map(str,args)),log=str(log));report['commands'].append(row);save()
 with log.open('wb') as stream:
  result=subprocess.run(list(map(str,args)),cwd=R,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout)
 row.update(exit=result.returncode,sha256=sha(log));save()
 if result.returncode:raise ValueError(name+': '+log.read_text('utf8',errors='replace')[-1600:])
try:
 verify();run('sdk-verification',['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',R/'ide/tools/verify_xge_sdk.ps1','-SdkRoot',R/'ide/third_party/xge_sdk'])
 resource=O/'xlang_ide.res.o';run('icon',[old['resourceCompiler'],'-I',R/'ide/data','-i',R/'ide/data/xlang_ide.rc','-O','coff','-o',resource]);report['resources']=[str(resource)]
 for i,source in enumerate(sources):
  target=R/'ide/build/Release'/((str(source.relative_to(R/'ide')).replace('/','_').replace('\\','_'))+'.o')
  run('compile-'+str(i),[old['compiler'],*old['flags'],'-c',source,'-o',target]);report['objects'].append(str(target))
 candidate=O/'xlang_ide.exe';run('link',[old['compiler'],*old['flags'],'-DXIDE_GUI=1','-mwindows',R/'ide/src/main.c',*report['objects'],resource,*old['libraries'],'-o',candidate])
 verify();report.update(status='passed',translationUnits=len(sources)+1,candidate=str(candidate),candidateSha256=sha(candidate));save()
 print(json.dumps({k:report[k] for k in ['status','translationUnits','candidate','candidateSha256']}))
except BaseException as error:
 report.update(status='failed',error=repr(error));save();raise
