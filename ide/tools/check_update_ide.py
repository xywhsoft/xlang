#!/usr/bin/env python3
"""Validate fully rebuilt GUI consumers without replacing a blocked official IDE."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    record=json.loads((ROOT/'ide/build/Release/gui-candidate-build-inputs.json').read_text(encoding='utf-8-sig'))
    for name,digest in record['inputs'].items():
        if sha(name)!=digest:raise ValueError('GUI candidate input changed: '+name)
    runtime=Path(record['runtime']);sdk=ROOT/'ide/third_party/xge_sdk'
    if sha(runtime/'xge.dll')!=sha(sdk/'xge.dll'):raise ValueError('runtime XGE differs from compiled SDK')
    if sha(runtime/'xlang-ls.exe')!=record['languageServerSha256']:raise ValueError('runtime language server changed')
    evidence=ROOT/'ide/build/Release/update/gui-validation';evidence.mkdir(parents=True,exist_ok=True)
    owned=[]
    try:
        for fixture in ('input_test','update_client_test'):
            target=runtime/('.xlang-update-'+fixture+'.exe')
            if target.exists():raise ValueError('test filename already exists: '+str(target))
            owned.append(target)
            command=[record['compiler'],*record['flags'],str(ROOT/'ide/tests'/f'{fixture}.c'),*record['objects'],record['resources'],*record['libraries'],'-o',str(target)]
            subprocess.run(command,check=True)
            run=subprocess.run([str(target),str(evidence)],capture_output=True,timeout=60)
            (evidence/(fixture+'.log')).write_bytes(run.stdout+run.stderr)
            if run.returncode:raise RuntimeError(f'{fixture} failed: '+(run.stdout+run.stderr).decode(errors='replace'))
            print(run.stdout.decode(errors='replace').strip())
        candidate=runtime/'.xlang-ide-update-candidate.exe'
        if candidate.exists():raise ValueError('candidate filename already exists')
        owned.append(candidate);shutil.copy2(ROOT/'ide/build/Release/xlang_ide.link.exe',candidate)
        r=subprocess.run([str(candidate),'--hidden','--frames','4','--no-session','--no-auto-diagnostics','--screenshot',str(evidence/'workbench.png')],capture_output=True,timeout=60)
        (evidence/'candidate.log').write_bytes(r.stdout+r.stderr)
        if r.returncode:raise RuntimeError('GUI candidate smoke failed')
        print('PASS candidate hidden render; official GUI/compiler/catalog unchanged')
    finally:
        for path in owned:path.unlink(missing_ok=True)
if __name__=='__main__':main()
