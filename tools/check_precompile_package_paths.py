"""SDK directory links keep package identity and still invalidate on changes."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
from precompile_builtin import source_dependencies
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);root=out/'project';sdk=out/'installed-sdk'
    root.mkdir();sdk.mkdir();link=root/'sdk'
    if os.name=='nt':
        quote=lambda value:"'"+str(value).replace("'","''")+"'"
        subprocess.run(['powershell','-NoProfile','-NonInteractive','-Command',
            'New-Item -ItemType Junction -Path '+quote(link)+' -Target '+quote(sdk)+' | Out-Null'],check=True)
    else:link.symlink_to(sdk,target_is_directory=True)
    source=root/'main.c';part=sdk/'part.c';source.write_text('#include "sdk/part.c"\n');part.write_text('int sdk_value=1;\n')
    before=source_dependencies(root,'main.c');assert before==source_dependencies(root,'./main.c')
    part.write_text('int sdk_value=2;\n');assert before!=source_dependencies(root,'main.c')
    try:source_dependencies(root,'../installed-sdk/part.c')
    except ValueError:pass
    else:raise AssertionError('lexical package escape was accepted')
    report=dict(complete=True,passed=True,scope=__doc__,checks=['external SDK directory link','stable dot aliases','invalidate changed bytes','refuse lexical escape'],
        toolSha256=hashlib.sha256((Path(__file__).parent/'precompile_builtin.py').read_bytes()).hexdigest())
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print('SDK package paths: 4 checks passed')
if __name__=='__main__':main()
