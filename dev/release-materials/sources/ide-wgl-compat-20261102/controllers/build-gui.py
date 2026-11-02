from pathlib import Path
import hashlib,json,os,re,shutil,subprocess
R=Path('D:/GIT/x-lang');E=Path(__file__).resolve().parent;SDK=E/'sdk';T=E/'runtime';T.mkdir();C=E/'gui-objects';C.mkdir()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
old=json.loads((R/'ide/build/Release/gui-build-inputs.json').read_text('utf-8-sig'));prefix=(R/'ide/tools/build.ps1').read_text('utf-8-sig').split('$buildInputs=@{}')[0];text,skip=prefix.split('$cLspSources=',1)
sources=re.findall(r"'((?:src|tools)\\[^']+\.c)'",text);excluded=set(re.findall(r"'((?:src|tools)\\[^']+\.c)'",skip.split('if (-not $EnableCLsp)')[0]));sources=[R/'ide'/s.replace('\\','/') for s in sources if s not in excluded];assert len(sources)==100
parent=str(R/'ide/third_party/xge_sdk');flags=[v.replace(parent,str(SDK)) for v in old['flags']];libraries=[v.replace(parent,str(SDK)) for v in old['libraries']]
inputs={str(p):sha(p) for p in (R/'ide/src').rglob('*') if p.is_file() and p.suffix in ['.c','.h','.inc']}
for p in [Path(__file__),R/'ide/tools/build.ps1',R/'ide/data/xlang_ide.rc',R/'ide/data/xlang_ide.ico',SDK/'sdk_manifest.json']:inputs[str(p)]=sha(p)
for row in json.loads((SDK/'sdk_manifest.json').read_text())['files']:inputs[str(SDK/row['path'])]=row['sha256']
baseline=json.loads((E/'baseline.json').read_text())['protected']
record=dict(status='running',scope='Matched candidate GUI;existing catalog retained;not complete compiler/catalog release acceptance',compiler=old['compiler'],resourceCompiler=old['resourceCompiler'],configuration='Release',cLspEnabled=False,flags=flags,libraries=libraries,inputs=inputs,objects=[],resources=[],commands=[],release=str(T))
def save():(E/'gui-build.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
def verify():
 for p,d in {**inputs,**baseline}.items():assert sha(p)==d,p
def run(name,args):
 log=E/(name+'.log');row=dict(name=name,command=list(map(str,args)),log=str(log));record['commands'].append(row);save()
 with log.open('wb') as stream:p=subprocess.run(list(map(str,args)),cwd=R,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
 row.update(exit=p.returncode,sha256=sha(log));save()
 if p.returncode:raise ValueError(name+': '+log.read_text('utf8',errors='replace')[-2000:])
try:
 verify();resource=C/'xlang_ide.res.o';run('gui-icon',[old['resourceCompiler'],'-I',R/'ide/data','-i',R/'ide/data/xlang_ide.rc','-O','coff','-o',resource]);record['resources']=[str(resource)]
 for i,p in enumerate(sources):
  target=C/(str(i)+'.o');run('gui-compile-'+str(i),[old['compiler'],*flags,'-c',p,'-o',target]);record['objects'].append(str(target))
 run('gui-link',[old['compiler'],*flags,'-DXIDE_GUI=1','-mwindows',R/'ide/src/main.c',*record['objects'],resource,*libraries,'-o',T/'xlang_ide.exe'])
 verify();shutil.copyfile(SDK/'xge.dll',T/'xge.dll');shutil.copyfile(R/'release/xlang-ls.exe',T/'xlang-ls.exe');shutil.copytree(R/'release/data',T/'data')
 record.update(status='passed',translationUnits=101,outputs=dict(xlang_ide=sha(T/'xlang_ide.exe'),xlang_ls=sha(T/'xlang-ls.exe'),xge=sha(T/'xge.dll')));save();print('GUI passed',record['outputs'])
except BaseException as error:record.update(status='failed',error=repr(error));save();raise
