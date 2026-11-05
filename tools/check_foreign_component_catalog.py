"""Compile the exact native-storage/callback dependency catalog independently."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from runtime_layout import runtime_directory
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--sdk-directory',type=Path);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    sdk=a.sdk_directory.resolve() if a.sdk_directory else runtime_directory(root)
    assert (sdk/'include/xrt_decl.h').is_file(),sdk
    cc=shutil.which('gcc');assert cc
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in [Path(__file__).resolve(),root/'test/support/foreign_component_catalog.c',
        root/'src/compile/xl6_compile_unit.c',*sorted((root/'src/compile').glob('*.h')),
        *sorted((root/'src/compile').glob('*.inc')),*sorted((root/'src/runtime').glob('*_symbols.inc')),
        root/'tools/runtime_layout.py',*sorted((sdk/'include').rglob('*.h'))]}
    rows=[];env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    for opt in ('O0','O2'):
        exe=out/(opt+('.exe' if os.name=='nt' else '.out'))
        command=[cc,'-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(sdk),
            '-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections','-fno-jump-tables',
            '-fno-asynchronous-unwind-tables','-fno-unwind-tables',root/'test/support/foreign_component_catalog.c',
            root/'src/compile/xl6_compile_unit.c','-Wl,--gc-sections','-o',exe]
        for name,cmd in ((opt+'-build',command),(opt+'-run',[exe])):
            r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=180)
            log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
            rows.append(dict(name=name,exitCode=r.returncode,logSha256=sha(log)))
            print(name,r.returncode,flush=True)
            if r.returncode:break
    report=dict(complete=True,passed=len(rows)==4 and all(row['exitCode']==0 for row in rows),networkTests=False,
        inputs=inputs,inputsUnchanged=all(sha(Path(path))==value for path,value in inputs.items()),rows=rows)
    report['passed'] &= report['inputsUnchanged'];(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    assert report['passed'],out/'results.json'
if __name__=='__main__':main()
