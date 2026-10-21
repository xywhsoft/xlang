"""Verify actual GUI children after terminating only their owned startup parent."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import time
import uuid
from PIL import Image
from native_desktop import NativeDesktopProcess
from check_startup_alias import native, logical, sha

class ProcessEntry(c.Structure):
    _fields_=[('size',w.DWORD),('usage',w.DWORD),('pid',w.DWORD),
              ('heap',c.c_size_t),('module',w.DWORD),('threads',w.DWORD),
              ('parent',w.DWORD),('priority',w.LONG),('flags',w.DWORD),
              ('name',w.WCHAR*260)]

def check(ide_root,package_root,fixture,output):
    ide_root=Path(ide_root).absolute();fixture=Path(fixture).absolute()
    proof=Path(output).absolute()/('startup-gui-'+uuid.uuid4().hex);proof.mkdir(parents=True)
    (proof/'checker-input.py').write_bytes(Path(__file__).read_bytes())
    k=c.WinDLL('kernel32',use_last_error=True);u=c.WinDLL('user32',use_last_error=True)
    def api(lib,name,result,args):
        f=getattr(lib,name);f.restype=result;f.argtypes=args;return f
    close=api(k,'CloseHandle',w.BOOL,[w.HANDLE])
    create_file=api(k,'CreateFileW',w.HANDLE,[w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE])
    short_name=api(k,'SetFileShortNameW',w.BOOL,[w.HANDLE,w.LPCWSTR])
    get_short=api(k,'GetShortPathNameW',w.DWORD,[w.LPCWSTR,w.LPWSTR,w.DWORD])
    create_desktop=api(u,'CreateDesktopW',w.HANDLE,[w.LPCWSTR,w.LPCWSTR,c.c_void_p,w.DWORD,w.DWORD,c.c_void_p])
    close_desktop=api(u,'CloseDesktop',w.BOOL,[w.HANDLE])
    callback=c.WINFUNCTYPE(w.BOOL,w.HWND,c.c_ssize_t)
    enum_windows=api(u,'EnumDesktopWindows',w.BOOL,[w.HANDLE,callback,c.c_ssize_t])
    window_pid=api(u,'GetWindowThreadProcessId',w.DWORD,[w.HWND,c.POINTER(w.DWORD)])
    title=api(u,'GetWindowTextW',c.c_int,[w.HWND,w.LPWSTR,c.c_int])
    open_process=api(k,'OpenProcess',w.HANDLE,[w.DWORD,w.BOOL,w.DWORD])
    image_name=api(k,'QueryFullProcessImageNameW',w.BOOL,[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)])
    process_times=api(k,'GetProcessTimes',w.BOOL,[w.HANDLE,c.POINTER(w.FILETIME),c.POINTER(w.FILETIME),c.POINTER(w.FILETIME),c.POINTER(w.FILETIME)])
    wait=api(k,'WaitForSingleObject',w.DWORD,[w.HANDLE,w.DWORD])
    exit_code=api(k,'GetExitCodeProcess',w.BOOL,[w.HANDLE,c.POINTER(w.DWORD)])
    send=api(u,'SendMessageTimeoutW',c.c_ssize_t,[w.HWND,w.UINT,c.c_size_t,c.c_ssize_t,w.UINT,w.UINT,c.POINTER(c.c_size_t)])
    snapshot=api(k,'CreateToolhelp32Snapshot',w.HANDLE,[w.DWORD,w.DWORD])
    first=api(k,'Process32FirstW',w.BOOL,[w.HANDLE,c.POINTER(ProcessEntry)])
    next_process=api(k,'Process32NextW',w.BOOL,[w.HANDLE,c.POINTER(ProcessEntry)])
    invalid=c.c_void_p(-1).value
    def identity(handle):
        buf=c.create_unicode_buffer(32768);length=w.DWORD(len(buf));times=[w.FILETIME() for _ in range(4)]
        assert image_name(handle,0,buf,c.byref(length)),c.get_last_error()
        assert process_times(handle,*[c.byref(t) for t in times]),c.get_last_error()
        return {'image':logical(buf.value),'createdFiletime':(times[0].dwHighDateTime<<32)|times[0].dwLowDateTime}
    def parent_pid(pid):
        h=snapshot(2,0);assert h!=invalid,c.get_last_error()
        entry=ProcessEntry(size=c.sizeof(ProcessEntry))
        try:
            more=first(h,c.byref(entry))
            while more:
                if entry.pid==pid:return entry.parent
                more=next_process(h,c.byref(entry))
            raise AssertionError(('Child process was not in snapshot',pid))
        finally:assert close(h)
    def responsive(hwnd):
        result=c.c_size_t();return bool(send(hwnd,0,0,0,2,1000,c.byref(result)))
    calls=[];cases=[];inputs={str(fixture):sha(fixture)};protected={};images=[];status='failed';failure=None
    roots=[('development',ide_root)]
    if package_root:roots.append(('packaged',Path(package_root).absolute()))
    def prune(case_root,desktop_name,cache_root,label):
        args=[str(fixture),'--prune',str(cache_root),str((7*24*60*60+60)*10000000)]
        out_path=case_root/(label+'.stdout');err_path=case_root/(label+'.stderr')
        row={'label':label,'arguments':args,'desktop':desktop_name,'stdout':str(out_path),'stderr':str(err_path)}
        with out_path.open('xb') as out,err_path.open('xb') as err:
            process=NativeDesktopProcess(args,out,err,desktop_name,cwd=str(case_root),env=os.environ.copy());row['pid']=process.pid;calls.append(row)
            try:row['identity']=identity(process.handle);row['exit']=process.wait(timeout=30)
            finally:
                if process.poll() is None:process.kill();process.wait(timeout=10)
                process.close()
        assert row['exit']==0,(label,row['exit'],err_path.read_text())
        return json.loads(out_path.read_text(encoding='utf8'))
    try:
        for origin,source_root in roots:
            for language in ('c','xlang'):
                label=origin+'-'+language;case_root=proof/label;case_root.mkdir();cache_root=case_root/'cache';cache_root.mkdir()
                source_files=[source_root/'xlang_ide.exe',source_root/'xge.dll',source_root/'data/commands.json',*sorted((source_root/'data/fonts').iterdir())]
                original=case_root/'original';removed=[]
                for i in range(3):
                    original/=f'segment-{i}-'+('abcdefghijklmnop'*6)+'中文'
                    native(original).mkdir(parents=True)
                    assert native(original).is_relative_to(native(proof))
                    h=create_file(str(native(original)),0x10000,7,None,3,0x02000000,None);assert h!=invalid,c.get_last_error()
                    try:assert short_name(h,''),c.get_last_error()
                    finally:assert close(h)
                    removed.append(str(original))
                for source in source_files:
                    digest=sha(source);inputs[str(source)]=digest;target=original/source.relative_to(source_root)
                    native(target).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,native(target));assert sha(target)==digest
                    protected[str(target)]=digest
                program=original/'xlang_ide.exe';short=c.create_unicode_buffer(32768)
                assert 0<get_short(str(native(program)),short,len(short))<len(short)
                acp=len((short.value.rsplit('\\',1)[0]+'\\').encode('mbcs'));assert acp>=256,acp
                font=str(original/'data/fonts/NotoSansMonoCJKsc-Regular.otf')
                settings=original/'option/settings.json';native(settings).parent.mkdir(parents=True)
                native(settings).write_text(json.dumps({'format':'xlang-ide-settings-1','editor':{'font':{'path':font,'size':20}},'ui':{'font':{'path':font,'size':16}}},ensure_ascii=False),encoding='utf8')
                source=original/('编辑文件.c' if language=='c' else '编辑文件.xl')
                native(source).write_text('// 中文注释\nint main(void) { return 0; }\n' if language=='c' else '#doc\n描述：父进程结束后继续渲染\n#end\nfunc main(...) as int\n var 值 as int = 1\n return 值 - 1\nend\n',encoding='utf8')
                protected.update({str(settings):sha(settings),str(source):sha(source)})
                desktop_name='XlangStartupGui-'+uuid.uuid4().hex;desktop=create_desktop(desktop_name,None,None,0,0x10000000,None);assert desktop,c.get_last_error()
                env={key:value for key,value in os.environ.items() if not key.upper().startswith('XLANG_IDE_STARTUP_')}
                env.update({'TEMP':str(cache_root),'TMP':str(cache_root)})
                png=case_root/'after-parent-exit.png';out_path=case_root/'gui.stdout';err_path=case_root/'gui.stderr'
                args=[str(native(program)),'--frames','600','--no-session','--open-file',str(source),'--demo',language,'--command','c:keyword:inline' if language=='c' else 'language:func','--screenshot',str(png),'--diagnostic-root',str(case_root/'diagnostics')]
                row={'label':label+'-startup-parent','arguments':args,'desktop':desktop_name,'stdout':str(out_path),'stderr':str(err_path)}
                process=None;survivor=None;survivor_row=None
                case={'label':label,'language':language,'originalProgram':str(program),'shortPath':short.value,'shortDirectoryAcpBytes':acp,'removedShortNamesOnlyFromFreshPrivateDirectories':removed,'tempRoot':str(cache_root),'desktop':desktop_name};cases.append(case)
                try:
                    with out_path.open('xb') as out,err_path.open('xb') as err:
                        process=NativeDesktopProcess(args,out,err,desktop_name,cwd=str(case_root),env=env);row['pid']=process.pid;row['identity']=identity(process.handle);calls.append(row)
                        until=time.monotonic()+30;window=None;empty_polls=0
                        while time.monotonic()<until and process.poll() is None:
                            windows=[];visits=[]
                            @callback
                            def collect(hwnd,_):
                                visits.append(int(hwnd))
                                pid=w.DWORD();window_pid(hwnd,c.byref(pid));text=c.create_unicode_buffer(512);title(hwnd,text,len(text))
                                if pid.value!=process.pid and text.value=='xlang IDE':windows.append((hwnd,pid.value))
                                return True
                            c.set_last_error(0);enumerated=enum_windows(desktop,collect,0);enum_error=c.get_last_error()
                            if not enumerated:
                                assert not visits and enum_error==0,(label,'Desktop enumeration error',enum_error)
                                empty_polls+=1;time.sleep(.025);continue
                            if windows:
                                assert len(windows)==1,windows
                                if responsive(windows[0][0]):window=windows[0];break
                            time.sleep(.025)
                        assert window,(label,'No responsive actual GUI child',process.poll())
                        case.update({'emptyDesktopPolls':empty_polls,'actualWindowEnumerationCompleted':True})
                        hwnd,pid=window;assert parent_pid(pid)==process.pid
                        survivor=open_process(0x1000|0x100000,False,pid);assert survivor,c.get_last_error()
                        child_identity=identity(survivor);staged=Path(child_identity['image']);assert staged.parent.parent==cache_root and staged.parent.name.startswith('xlang-ide-start-v2-')
                        assert sha(staged)==inputs[str(source_root/'xlang_ide.exe')]
                        assert sha(staged.parent/'xge.dll')==inputs[str(source_root/'xge.dll')]
                        marker=staged.parent/'.xide-startup';assert marker.is_file();marker_digest=sha(marker)
                        assert wait(survivor,0)==258 and process.poll() is None
                        assert process.terminate(process.handle,1),c.get_last_error();assert process.wait(timeout=10)==1
                        row.update({'exit':1,'onlyExplicitOwnedParentTerminated':True,'jobKeptOpenUntilGuiChildExited':True})
                        response_until=time.monotonic()+10;response_attempts=[];is_responsive=False
                        while time.monotonic()<response_until:
                            state=wait(survivor,0);item={'processWaitResult':state,'utc':datetime.now(timezone.utc).isoformat()};response_attempts.append(item)
                            case['afterParentTerminationObservations']=response_attempts
                            if state!=258:
                                child_exit=w.DWORD();assert exit_code(survivor,c.byref(child_exit));item['childExit']=child_exit.value;break
                            item['sameProcessIdentity']=identity(survivor)==child_identity
                            assert item['sameProcessIdentity']
                            c.set_last_error(0);item['windowResponded']=responsive(hwnd);item['responseLastError']=c.get_last_error()
                            if item['windowResponded']:is_responsive=True;break
                            time.sleep(.025)
                        assert is_responsive,(label,'Actual child failed bounded post-parent response check',response_attempts)
                        h=create_file(str(marker),0x10000,7,None,3,0,None);error=c.get_last_error()
                        if h!=invalid:close(h)
                        assert h==invalid and error==32,(label,'Actual child did not independently deny marker deletion',error)
                        alive=prune(case_root,desktop_name,cache_root,label+'-while-child-live')
                        assert alive['scanned']==1 and alive['removed']==0 and alive['retained']==1
                        after_prune_until=time.monotonic()+10;prune_observations=[];after_prune_responsive=False
                        case['afterLivePruneObservations']=prune_observations
                        while time.monotonic()<after_prune_until:
                            state=wait(survivor,0);item={'processWaitResult':state,'utc':datetime.now(timezone.utc).isoformat()};prune_observations.append(item)
                            if state!=258:
                                child_exit=w.DWORD();assert exit_code(survivor,c.byref(child_exit));item['childExit']=child_exit.value;break
                            item['sameProcessIdentity']=identity(survivor)==child_identity
                            assert item['sameProcessIdentity']
                            c.set_last_error(0);item['windowResponded']=responsive(hwnd);item['responseLastError']=c.get_last_error()
                            item['markerUnchanged']=marker.is_file() and sha(marker)==marker_digest
                            if item['windowResponded'] and item['markerUnchanged']:after_prune_responsive=True;break
                            time.sleep(.025)
                        assert after_prune_responsive,(label,'Actual child failed bounded post-prune response check',prune_observations)
                        case.update({'childPid':pid,'childIdentity':child_identity,'window':int(hwnd),'windowResponsiveAfterParentTermination':True,'markerDeleteBlockedByIndependentChildLease':True,'markerSha256':marker_digest,'whileChildLive':alive})
                        assert wait(survivor,60000)==0,c.get_last_error();code=w.DWORD();assert exit_code(survivor,c.byref(code));assert code.value==0,(label,code.value,err_path.read_text())
                        survivor_row={'label':label+'-actual-gui-child','pid':pid,'parentPid':process.pid,'identity':child_identity,'exit':code.value,'desktop':desktop_name,'stdout':str(out_path),'stderr':str(err_path),'inheritedOriginalCommandLine':args};calls.append(survivor_row)
                    text=out_path.read_text(encoding='utf8');assert 'rendered 600 frames' in text and 'C and xlang registered' in text
                    assert 'preferences:' not in err_path.read_text(encoding='utf8',errors='replace')
                    with Image.open(png) as im:assert im.size==(1280,800) and max(im.convert('L').getextrema())-min(im.convert('L').getextrema())>50
                    images.append({'path':str(png),'sha256':sha(png),'width':1280,'height':800,'label':label})
                    assert sha(marker)==marker_digest and staged.exists()
                    expired=prune(case_root,desktop_name,cache_root,label+'-after-child-exit')
                    assert expired['scanned']==1 and expired['removed']==1 and not staged.parent.exists()
                    case.update({'guiChildCompletedFrames':600,'guiChildExit':0,'afterChildExit':expired,'onlyVerifiedExpiredCopyRemoved':True})
                    print('Completed actual GUI interruption case:',label,'600 frames; responsive child and expired-copy cleanup verified',flush=True)
                finally:
                    if process:
                        process.close() # Own job only; failure cleanup also covers its actual child.
                    if survivor:assert close(survivor)
                    assert close_desktop(desktop),c.get_last_error()
        for path,digest in {**inputs,**protected}.items():assert sha(path)==digest,path
        assert len(cases)==len(roots)*2 and len(images)==len(cases);status='passed'
    except BaseException as e:
        failure=repr(e);raise
    finally:
        for row in calls:
            for stream in ('stdout','stderr'):row[stream+'Sha256']=sha(row[stream])
        files=[{'path':logical(p),'sha256':sha(p)} for p in sorted(native(proof).rglob('*')) if p.is_file()]
        report={'status':status,'failure':failure,'utc':datetime.now(timezone.utc).isoformat(),'fixture':str(fixture),'fixtureSha256':sha(fixture),'inputs':inputs,'protectedFiles':protected,'cases':cases,'calls':calls,'images':images,'files':files,'actualCreateProcessW':True,'privateDesktopNeverSwitched':True,'actualGuiInterruptedParentSurvivalVerified':status=='passed','onlyOwnedStartupParentsTerminated':status=='passed','systemClockChanged':False,'volumePolicyChanged':False,'driverOrSystemChanged':False,'unrecordedPartialCopiesReclaimed':False,'fullNativeRegressionRepeated':False,'goalStatus':'active','formalRelease':False}
        (proof/'evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
        print('Actual GUI startup parent interruption:',status,len(cases),'cases;',proof,flush=True)
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ide-root',required=True);parser.add_argument('--package-root');parser.add_argument('--fixture',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args();check(args.ide_root,args.package_root,args.fixture,args.output)
