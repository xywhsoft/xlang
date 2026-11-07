import pathlib,json,hashlib,subprocess,shutil
run=pathlib.Path('D:/GIT/x-lang/ide/build/Release/find-follow-20261108');mp=run/'objects/gui-candidate-build-inputs.json';m=json.loads(mp.read_text(encoding='utf-8-sig'));changed=[]
for p,h in m['inputs'].items():
 if hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()!=h:changed.append(p)
assert changed and {pathlib.Path(p).name for p in changed} <= {'find_view.c','ui_font.c'},changed
assert (run/'full-cohort-build-inputs.json').exists()
command=[m['compiler'],*m['flags'],'-DXIDE_GUI=1','-mwindows','F:/xlang-validation/ide-find-follow-20261108/source/ide/src/main.c',*m['objects'],m['resources'],*m['libraries'],'-o',str(run/'objects/xlang_ide.link.exe')]
r=subprocess.run(command,capture_output=True);(run/'private-final-relink.log').write_bytes(r.stdout+r.stderr);assert not r.returncode,r.stderr
for p in changed:m['inputs'][p]=hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
m['outputs']['xlang_ide']=hashlib.sha256((run/'objects/xlang_ide.link.exe').read_bytes()).hexdigest();m['privateFinalRelink']={'recompiled':['src/ui/find_view.c','src/services/ui_font.c'],'reason':'Final responsive content/footer extent and explicit overlay font lifetime integration.','command':command};mp.write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8');print('objects',len(m['objects']),'GUI',m['outputs']['xlang_ide'])
