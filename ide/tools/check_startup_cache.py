from pathlib import Path
import argparse,ctypes,datetime,hashlib,json,os,sys,time,uuid
from ctypes import wintypes
from native_desktop import NativeDesktopProcess
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
parser=argparse.ArgumentParser(description='Real startup cache ownership and interrupted-parent lease checks')
parser.add_argument('--fixture',required=True);parser.add_argument('--output',required=True);args=parser.parse_args()
fixture=Path(args.fixture).absolute();root=Path(args.output).absolute()/('startup-cache-'+uuid.uuid4().hex);root.mkdir(parents=True)
(root/'checker-input.py').write_bytes(Path(__file__).read_bytes())
fixtureDigest=sha(fixture)
u=ctypes.WinDLL('user32',use_last_error=True);u.CreateDesktopW.restype=wintypes.HANDLE;u.CreateDesktopW.argtypes=[wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p];u.CloseDesktop.argtypes=[wintypes.HANDLE]
desktopName='XlangStartupCache-'+uuid.uuid4().hex;desktop=u.CreateDesktopW(desktopName,None,None,0,0x10000000,None);assert desktop
calls=[];children=[];status='failed'
def start(label,args,env=None):
    out=(root/(label+'.stdout')).open('xb');err=(root/(label+'.stderr')).open('xb');command=[str(fixture),*[str(x) for x in args]]
    child=NativeDesktopProcess(command,out,err,desktopName,cwd=str(root),env=env or os.environ.copy());children.append((child,out,err))
    calls.append({'label':label,'arguments':command,'pid':child.pid,'desktop':desktopName,'stdout':str(root/(label+'.stdout')),'stderr':str(root/(label+'.stderr'))});return child
def run(label,args,env=None):
    child=start(label,args,env);code=child.wait(timeout=40);calls[-1]['exit']=code;assert code==0,(label,code,(root/(label+'.stderr')).read_text());return (root/(label+'.stdout')).read_text()
def waitfile(path):
    end=time.monotonic()+20
    while time.monotonic()<end:
        try:
            text=path.read_text(encoding='utf8')
            if text:return text
        except (FileNotFoundError,PermissionError):pass
        time.sleep(.025)
    raise TimeoutError(str(path))
try:
    text=run('native-cases',['--cases',root/'cases']);assert 'startup_cache_test passed:' in text
    temp=root/'parent-cache';temp.mkdir();original=root/'parent-source';original.mkdir();(original/'original.exe').write_bytes(b'EXE1');(original/'xge.dll').write_bytes(b'DLL1')
    env=os.environ.copy();env.update({'TEMP':str(temp),'TMP':str(temp)})
    parent=start('live-parent',['--prepare',original/'original.exe',root/'parent.ready',root/'parent.stop'],env)
    staged=Path(waitfile(root/'parent.ready'));assert staged.parent.is_relative_to(temp)
    survivor=start('live-child',['--child',staged,root/'child.ready',root/'child.stop'],env);assert Path(waitfile(root/'child.ready'))==staged
    assert parent.poll() is None and survivor.poll() is None
    assert parent.terminate(parent.handle,1);assert parent.wait(timeout=10)==1;calls[-2]['exit']=1;calls[-2]['onlyExplicitOwnedParentTerminated']=True
    assert survivor.poll() is None
    expiry=str((7*24*60*60+60)*10000000)
    alive=json.loads(run('prune-while-child-live',['--prune',temp,expiry]));assert alive['removed']==0 and alive['retained']==1 and staged.exists()
    assert survivor.poll() is None
    (root/'child.stop').write_bytes(b'stop');assert survivor.wait(timeout=10)==0;calls[-2]['exit']=0
    expired=json.loads(run('prune-after-parent-crash-child-exit',['--prune',temp,expiry]));assert expired['removed']==1 and not staged.parent.exists()
    assert (original/'original.exe').read_bytes()==b'EXE1' and (original/'xge.dll').read_bytes()==b'DLL1';status='passed'
finally:
    for child,out,err in children:
        if child.poll() is None:child.kill();child.wait(timeout=10)
        child.close();out.close();err.close()
    assert u.CloseDesktop(desktop)
    for row in calls:
        for stream in ('stdout','stderr'):row[stream+'Sha256']=sha(row[stream])
    report={'status':status,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'fixture':str(fixture),'fixtureSha256':sha(fixture),'calls':calls,'actualCreateProcessW':True,'privateDesktopNeverSwitched':True,'ownedParentOnlyTerminated':status=='passed','childRetainedItsOwnLease':status=='passed','oldFilesFromOriginalPreserved':status=='passed','formalRelease':False,'goalStatus':'active'}
    assert sha(fixture)==fixtureDigest
    p=root/'evidence.json';p.write_text(json.dumps(report,indent=2),encoding='utf8')
    print('Startup cache focused native/process acceptance:',status,root,flush=True)
