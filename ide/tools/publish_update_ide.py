#!/usr/bin/env python3
"""GUI-only publication against the explicitly retained, previously accepted runtime.

Does not relax normal compiler/catalog coverage checks or replace runtime assets.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    runtime=ROOT/'release/win_x64';index=ROOT/'dev/release-materials/provenance/current-delivery.json'
    delivery=json.loads(index.read_text(encoding='utf-8'));prior_path=ROOT/delivery['ide']['currentBuildProvenance']
    prior=json.loads(prior_path.read_text(encoding='utf-8'))
    if prior.get('guiBuild',{}).get('existingCatalogRetained') is not True:
        raise ValueError('GUI-only retained-asset publication requires its prior explicit acceptance record')
    products=prior['publication']['products'];gui_before=next(p for p in products if Path(p['path'].replace('\\','/')).name=='xlang_ide.exe')
    if sha(runtime/'xlang_ide.exe')!=gui_before['sha256']:raise ValueError('GUI changed since prior acceptance; refusing replacement')
    protected={}
    for name,expected in prior['publication']['protectedRuntime'].items():
        path=name.replace('\\','/');relative=path.split('/release/',1)[1]
        if relative.startswith('win_x64/'):relative=relative[len('win_x64/'):]
        if relative.startswith('option/'):continue # Preferences are intentionally mutable.
        current=runtime/relative
        if sha(current)!=expected:raise ValueError('retained runtime asset changed: '+relative)
        protected[relative]=expected
    xge=next(p for p in products if Path(p['path'].replace('\\','/')).name=='xge.dll')
    if sha(runtime/'xge.dll')!=xge['sha256'] or sha(runtime/'xge.dll')!=sha(ROOT/'ide/third_party/xge_sdk/xge.dll'):
        raise ValueError('XGE/SDK no longer matches the accepted runtime')
    protected['xge.dll']=xge['sha256']
    settings=runtime/'option/settings.json'
    if settings.exists():protected['option/settings.json']=sha(settings)
    receipt=json.loads((ROOT/'ide/build/Release/gui-candidate-build-inputs.json').read_text(encoding='utf-8-sig'))
    for name,expected in receipt['inputs'].items():
        if sha(name)!=expected:raise ValueError('compiled GUI input changed: '+name)
    candidate=ROOT/'ide/build/Release/xlang_ide.link.exe'
    if sha(candidate)!=receipt['outputs']['xlang_ide']:raise ValueError('candidate changed')
    evidence=ROOT/'ide/build/Release/update/gui-validation'
    for name,needle in [('input_test.log','input_test passed'),('update_client_test.log','PASS update action registry')]:
        if needle not in (evidence/name).read_text(encoding='utf-8',errors='replace'):raise ValueError('required GUI check missing: '+name)
    for name,expected in protected.items():
        if sha(runtime/name)!=expected:raise ValueError('runtime changed before GUI publication: '+name)
    backup=evidence/'previous-xlang_ide.exe';shutil.copy2(runtime/'xlang_ide.exe',backup)
    staged=runtime/'.xlang_ide.update.new';shutil.copy2(candidate,staged)
    if sha(staged)!=receipt['outputs']['xlang_ide']:raise ValueError('staged GUI differs')
    os.replace(staged,runtime/'xlang_ide.exe')
    try:
        smoke=subprocess.run([str(runtime/'xlang_ide.exe'),'--hidden','--frames','4','--no-session','--no-auto-diagnostics','--screenshot',str(evidence/'published-workbench.png')],capture_output=True,timeout=60)
        (evidence/'published-gui.log').write_bytes(smoke.stdout+smoke.stderr)
        if smoke.returncode:raise RuntimeError('published GUI smoke failed')
    except BaseException:
        if sha(runtime/'xlang_ide.exe')==receipt['outputs']['xlang_ide']:
            shutil.copy2(backup,staged);os.replace(staged,runtime/'xlang_ide.exe')
        raise
    for name,expected in protected.items():
        if sha(runtime/name)!=expected:raise ValueError('protected runtime changed during publication: '+name)
    report={'status':'published','guiOnly':True,'catalogRetained':True,'completeCompilerCatalogAcceptance':False,
            'priorAcceptance':str(prior_path.relative_to(ROOT)).replace('\\','/'),'priorAcceptanceSha256':sha(prior_path),
            'beforeGui':gui_before['sha256'],'afterGui':sha(runtime/'xlang_ide.exe'),'protected':protected,
            'compiledInputs':receipt,'externalPublishPerformed':False}
    (evidence/'publication.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Published GUI only; compiler, language server, XGE, catalog/source pair and user settings preserved.')
if __name__=='__main__':main()
