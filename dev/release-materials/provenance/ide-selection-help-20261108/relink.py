import hashlib,json,pathlib,subprocess,shutil,os
run=pathlib.Path('D:/GIT/x-lang/ide/build/Release/help-follow-20261108');mp=run/'objects/gui-candidate-build-inputs.json';m=json.loads(mp.read_text(encoding='utf-8-sig'));changed=[]
for p,h in m['inputs'].items():
 actual=hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
 if actual!=h:changed.append(p)
assert len(changed)==1 and changed[0].endswith('src\\services\\command_catalog.c'),changed
assert (run/'full-cohort-build-inputs.json').exists()
source=pathlib.Path('F:/xlang-validation/ide-help-follow-20261108/source/ide/src/main.c');out=run/'objects/xlang_ide.link.exe'
cmd=[m['compiler'],*m['flags'],'-DXIDE_GUI=1','-mwindows',str(source),*m['objects'],m['resources'],*m['libraries'],'-o',str(out)]
r=subprocess.run(cmd,capture_output=True);(run/'private-layout-relink.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr
m['inputs'][changed[0]]=hashlib.sha256(pathlib.Path(changed[0]).read_bytes()).hexdigest();m['outputs']['xlang_ide']=hashlib.sha256(out.read_bytes()).hexdigest();m['privateLayoutRelink']={'reason':'Private final catalog layout and native error isolation adjustments.','recompiled':['src/ui/catalog_view.c','src/services/command_catalog.c'],'fullCohortReceipt':'full-cohort-build-inputs.json','command':cmd}
mp.write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8');print(m['outputs'])
