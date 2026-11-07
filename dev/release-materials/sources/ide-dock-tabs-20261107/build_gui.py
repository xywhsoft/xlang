from pathlib import Path
import concurrent.futures,hashlib,io,json,re,subprocess,tarfile
r=Path.cwd();ide=r/'ide';b=ide/'build/Release/dock-tabs-20261107';snapshot=b/'source';sdk=b/'xge-sdk/sdk';cc=Path('E:/software/w64devkit/bin/gcc.exe');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
owned=json.loads((b/'owned-paths.json').read_text())+['src/platform/settings_options.inc','src/services/preferences.c','src/services/preferences.h','src/ui/settings_view.c','src/ui/settings_view.h','src/ui/help_document.h','src/ui/catalog_view.h','src/ui/automatic_diagnostics.inc']
if not snapshot.exists():
 archive=subprocess.check_output(['git','archive','HEAD','ide/src','ide/tools','ide/data','ide/tests'])
 snapshot.mkdir();tarfile.open(fileobj=io.BytesIO(archive)).extractall(snapshot,filter='data')
overlap={'src/ui/language_view.c','src/ui/automatic_diagnostics.inc','src/ui/language_references.inc'};owned+=['src/ui/language_references.inc','src/platform/text_utf.h']
edits=json.loads((b/'substitutions.json').read_text(encoding='utf-8'))
for name in set(owned):
 target=snapshot/'ide'/name;target.parent.mkdir(parents=True,exist_ok=True)
 if name in overlap:
  text=subprocess.check_output(['git','show','HEAD:ide/'+name]).decode('utf-8').replace('\r\n','\n')
  for edit in edits:
   if edit['path']==name:
    
    if edit['new'] and edit['new'] in text:continue
    if edit['old'] not in text and not edit['new']:continue
    assert text.count(edit['old'])==1,(name,edit['old'][:50]);text=text.replace(edit['old'],edit['new'],1)
  if name.startswith('src/ui/') and (b/'fixed-source/ui'/Path(name).name).exists():text=(b/'fixed-source/ui'/Path(name).name).read_text(encoding='utf-8')
  target.write_text(text,encoding='utf-8',newline='\n')
 else:target.write_bytes((ide/name).read_bytes())
for source in (ide/'tests').glob('settings_*test.c'):
 (snapshot/'ide/tests'/source.name).write_bytes(source.read_bytes())
source_script=(ide/'tools/build.ps1').read_text(encoding='utf-8');sources=[]
for line in source_script.splitlines():
 if line.startswith('$sources = @')or line.startswith('$sources += '):sources+=re.findall("'([^']+\\.c)'",line)
excluded=re.findall("'([^']+\\.c)'",next(line for line in source_script.splitlines()if line.startswith('$cLspSources=')))
sources=[x.replace('\\','/')for x in sources if x not in excluded]
obj=b/'objects';obj.mkdir(exist_ok=True)
version=json.loads((ide/'data/version.json').read_text(encoding='utf-8'))['displayVersion'];(obj/'xide_version.h').write_text('#define XIDE_VERSION "'+version+'"\n#define XIDE_VERSION_RESOURCE '+','.join(version.split('.'))+',0\n')
flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-DXGE_DLL','-DXUI_DLL','-DXGE_XRT_PROFILE_IDE=','-DXGE_DEBUGMODE=0','-DXIDE_ENABLE_C_LSP=0','-finput-charset=UTF-8','-fexec-charset=UTF-8','-I'+str(snapshot/'ide/src'),'-I'+str(sdk),'-I'+str(sdk/'lib/xrt'),'-I'+str(sdk/'extlibs/xrt'),'-I'+str(obj),'-include',str(sdk/'xge_build_config.h')]
inputs={str(p):sha(p)for p in (snapshot/'ide/src').rglob('*')if p.is_file()};inputs[str(sdk/'sdk_manifest.json')]=sha(sdk/'sdk_manifest.json');inputs[str(sdk/'xge.dll')]=sha(sdk/'xge.dll');inputs[str(sdk/'xge.lib')]=sha(sdk/'xge.lib');inputs[str(cc)]=sha(cc)
inputs.update({str(p):sha(p)for p in sdk.rglob('*.h') if p.is_file()})
headers=hashlib.sha256(json.dumps({k:v for k,v in inputs.items()if k.endswith(('.h','.inc'))},sort_keys=True).encode()).hexdigest();cache=b/'compile-cache.json';old=json.loads(cache.read_text())if cache.exists()else{};cache_rows={}
def compile(name):
 source=snapshot/'ide'/name;target=obj/(name.replace('/','_')+'.o');key=sha(source)+headers
 if old.get(name,{}).get('key')==key and target.exists()and sha(target)==old[name]['sha256']:
  return dict(source=name,key=key,sha256=sha(target),object=str(target),reused=True)
 command=[str(cc),*flags,'-c',str(source),'-o',str(target)];v=subprocess.run(command,capture_output=True);log=b/(target.name+'.log');log.write_bytes(v.stdout+v.stderr)
 if v.returncode:raise RuntimeError(str(log)+'\n'+(v.stdout+v.stderr).decode('utf-8',errors='replace')[-5000:])
 return dict(source=name,key=key,sha256=sha(target),object=str(target),command=command)
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:
 for row in pool.map(compile,sources):cache_rows[row['source']]=row
cache.write_text(json.dumps(cache_rows,indent=2),encoding='utf-8')
resource=obj/'xlang_ide.res.o';subprocess.run([str(cc.with_name('windres.exe')),'-I',str(ide/'data'),'-I',str(obj),'-i',str(ide/'data/xlang_ide.rc'),'-O','coff','-o',str(resource)],check=True)
libraries=[str(sdk/'xge.lib'),'-lm','-lws2_32','-lwinhttp','-liphlpapi','-lgdi32','-luser32','-lshell32','-lopengl32','-lole32','-lwinmm','-lavrt','-lversion','-lbcrypt','-ladvapi32']
target=b/'xlang_ide.exe';command=[str(cc),*flags,'-DXIDE_GUI=1','-mwindows',str(snapshot/'ide/src/main.c'),*[row['object']for row in cache_rows.values()],str(resource),*libraries,'-o',str(target)];v=subprocess.run(command,capture_output=True);(b/'link.log').write_bytes(v.stdout+v.stderr);assert not v.returncode,(v.stdout+v.stderr).decode('utf-8',errors='replace')[-5000:]
for name,digest in inputs.items():assert sha(name)==digest,name
report=dict(passed=True,sourceScope='Committed IDE baseline plus owned settings changes; other uncommitted analysis/language-service changes excluded.',inputs=inputs,objects=cache_rows,flags=flags,libraries=libraries,compiler=str(cc),gui=dict(path=str(target),sha256=sha(target)),link=command)
(b/'build.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('PASS GUI O2 build:',len(sources),'units;',sum(not row.get('reused')for row in cache_rows.values()),'rebuilt',flush=True)
