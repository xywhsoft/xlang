"""Compare IDE paths with real Win32/XRT subprocess CWD and drive environments."""
import argparse,ctypes,datetime,hashlib,json,os,subprocess
from pathlib import Path

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--fixture',required=True,type=Path);parser.add_argument('--root',required=True,type=Path);args=parser.parse_args()
    root=args.root.resolve();root.mkdir(parents=True,exist_ok=False);fixture=args.fixture.resolve();project=root/'consumer 中文😀';shell=root/'shell/nested';alternate=root/'inherited 中文😀'
    for directory in (project,shell,alternate):directory.mkdir(parents=True,exist_ok=True)
    system=Path(os.environ['SystemRoot']);drive=root.drive;assert system.is_dir()
    if system.drive.casefold()==drive.casefold():
        kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.GetDriveTypeW.argtypes=[ctypes.c_wchar_p]
        system=next((Path(f'{chr(letter)}:\\') for letter in range(ord('A'),ord('Z')+1) if f'{chr(letter)}:'.casefold()!=drive.casefold() and kernel.GetDriveTypeW(f'{chr(letter)}:\\')==3),system)
    cross_drive=system.drive.casefold()!=drive.casefold()
    native_base='\\\\?\\'+str(project)
    cases=[('ordinary',project,'cache 中文😀'),('ordinary-parent',project,'../cache 中文😀'),('ordinary-dot',project,'.'),('drive-relative',project,drive+'cache 中文😀'),('drive-lowercase',project,drive.lower()+'cache 中文😀'),('drive-parent',project,drive+'../cache 中文😀'),('drive-only',project,drive),('drive-dot',project,drive+'.'),('drive-root',project,drive+'\\'),('absolute',project,str(alternate/'cache 中文😀')),('absolute-cross-drive',project,str(system/'cache 中文😀')),('root-cross-drive',system,'\\cache 中文😀'),('root-slash-cross-drive',system,'/cache 中文😀'),('root-same-drive',project,'\\cache 中文😀'),('device-absolute',project,'\\\\?\\'+str(alternate/'cache 中文😀')),('extended-base-relative',native_base,'cache 中文😀'),('extended-base-drive',native_base,drive+'cache 中文😀'),('extended-base-root',native_base,'\\cache 中文😀')]
    rows=[]
    def compare(name,base,raw,env):
        outputs={}
        for mode,cwd in [('ide',shell),('canonical',shell),('win32',base),('sdk',shell)]:
            command=[str(fixture),mode,str(base),raw];out=subprocess.run(command,cwd=cwd,env=env,capture_output=True,timeout=15)
            (root/(name+'-'+mode+'.stdout')).write_bytes(out.stdout);(root/(name+'-'+mode+'.stderr')).write_bytes(out.stderr)
            assert out.returncode==0,(name,mode,out.returncode,out.stderr.decode('utf8',errors='replace'))
            outputs[mode]=out.stdout.decode('utf8').replace('\\','/').rstrip('/').casefold()
        row={'name':name,'base':str(base),'input':raw,'outputs':outputs,'passed':len(set(outputs.values()))==1};rows.append(row)
        return row['passed']
    env=dict(os.environ)
    for name,base,raw in cases:compare(name,base,raw,env)
    # Launch isolated native processes with controlled hidden entries; no parent
    # environment or system drive mapping is changed, and no values are logged.
    for label,value in ([('present',str(alternate)),('missing',None)] if cross_drive else []):
        controlled=dict(env);controlled.pop('='+drive.upper(),None)
        if value is not None:controlled['='+drive.upper()]=value
        for suffix,raw in [('relative',drive+'cache 中文😀'),('only',drive),('parent',drive+'../cache 中文😀')]:compare('other-drive-'+label+'-'+suffix,system,raw,controlled)
    # A nonexisting base stays lexical; both resolvers must retain the explicit
    # project anchor even though the child CWD cannot be entered.
    base=project/'missing parent';expected=(base/'cache 中文😀').as_posix().casefold();out=subprocess.run([str(fixture),'ide',str(base),'cache 中文😀'],cwd=shell,capture_output=True,timeout=15);(root/'missing-base.stdout').write_bytes(out.stdout);(root/'missing-base.stderr').write_bytes(out.stderr);rows.append({'name':'missing-base','passed':out.returncode==0 and out.stdout.decode('utf8').casefold()==expected,'actualChildCwdAvailable':False})
    report={'status':'passed' if all(row['passed'] for row in rows) else 'failed','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'fixture':str(fixture),'fixtureSha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'root':str(root),'checks':len(rows),'results':rows,'crossDriveWorkingDirectoryVerified':cross_drive,'inheritedOtherDriveEntriesVerified':cross_drive,'changedParentEnvironment':False,'changedParentWorkingDirectory':False,'networkFileSystemVerified':False}
    (root/'evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    assert report['status']=='passed',[row for row in rows if not row['passed']]
    print(f"Windows path context checks: {len(rows)} passed; actual Win32/XRT child CWD, drive-relative/root-relative/cross-drive paths, controlled inherited drive environments")
if __name__=='__main__':main()
