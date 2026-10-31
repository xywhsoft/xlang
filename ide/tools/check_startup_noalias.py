"""Run real GUI startup after removing short names only from fresh NTFS fixture directories."""
import argparse
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
from PIL import Image
from native_desktop import NativeDesktopProcess
from check_startup_alias import native, logical, sha

def check(fixture, output, compiler=None):
    fixture=Path(fixture).absolute();release=fixture.parent
    proof=Path(output).absolute()/('startup-noalias-'+uuid.uuid4().hex);proof.mkdir(parents=True)
    sources=[fixture,release/'xlang_ide.exe',release/'xlang-ls.exe',release/'xge.dll',release/'data/commands.json',*sorted((release/'data/fonts').iterdir())]
    inputs={str(p):sha(p) for p in sources};directory=proof/'deep-program'
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.CreateFileW.restype=ctypes.c_void_p;k.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p]
    k.CloseHandle.restype=ctypes.c_int;k.CloseHandle.argtypes=[ctypes.c_void_p]
    k.SetFileShortNameW.restype=ctypes.c_int;k.SetFileShortNameW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p]
    k.GetShortPathNameW.restype=ctypes.c_ulong;k.GetShortPathNameW.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_ulong]
    removed=[]
    for i in range(3):
        directory/=f'segment-{i}-'+('abcdefghijklmnop'*6)+'中文'
        native(directory).mkdir(parents=True)
        assert str(directory).casefold().startswith(str(proof).casefold()+os.sep)
        h=k.CreateFileW(str(native(directory)),0x10000,7,None,3,0x02000000,None)
        assert h!=ctypes.c_void_p(-1).value,ctypes.get_last_error()
        try:assert k.SetFileShortNameW(h,''),ctypes.get_last_error()
        finally:assert k.CloseHandle(h)
        removed.append(str(directory))
    for p in sources:
        q=directory/p.relative_to(release);native(q).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,native(q));assert sha(q)==sha(p)
    short=ctypes.create_unicode_buffer(32768)
    assert 0<k.GetShortPathNameW(str(native(directory/fixture.name)),short,32768)<32768
    acp=len((short.value.rsplit('\\',1)[0]+'\\').encode('mbcs'));assert acp>=256,acp
    u=ctypes.WinDLL('user32',use_last_error=True)
    u.CreateDesktopW.restype=ctypes.c_void_p;u.CreateDesktopW.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p]
    u.CloseDesktop.restype=ctypes.c_int;u.CloseDesktop.argtypes=[ctypes.c_void_p]
    calls=[];checks=0;staged=[]
    def invoke(name,command,expected=0,env=None,input_text=None):
        desktopName='XlangStartupAlias-NoAlias-'+uuid.uuid4().hex;desktop=u.CreateDesktopW(desktopName,None,None,0,0x10000000,None);assert desktop
        stdout=proof/(name+'.stdout');stderr=proof/(name+'.stderr');environment=os.environ.copy();environment['XIDE_STARTUP_ALIAS_TEST']='中文 😀 inherited';environment.update(env or {})
        incoming=None
        if input_text is not None:
            source=proof/(name+'.stdin');source.write_bytes(input_text.encode('utf8'));incoming=source.open('rb')
        try:
            with stdout.open('xb') as out,stderr.open('xb') as err:
                child=NativeDesktopProcess(command,out,err,desktopName,cwd=str(proof),env=environment,stdin=incoming)
                try:code=child.wait(timeout=70)
                except subprocess.TimeoutExpired:child.kill();child.wait(timeout=10);raise
                finally:child.close()
        finally:
            if incoming:incoming.close()
            assert u.CloseDesktop(desktop)
        row={'name':name,'arguments':command,'pid':child.pid,'exit':code,'desktop':desktopName,'stdout':str(stdout),'stderr':str(stderr),'stdoutSha256':sha(stdout),'stderrSha256':sha(stderr)};calls.append(row)
        (proof/(name+'.invocation.json')).write_text(json.dumps(row,ensure_ascii=False,indent=2),encoding='utf8')
        assert code==expected,(name,hex(code),stderr.read_text(encoding='utf8',errors='replace'))
        return stdout.read_text(encoding='utf8'),row
    arguments=['','中文 空格','😀','quote"inside','tail'+chr(92),'space tail '+chr(92),'slash'+chr(92)+'"quote','\t','z'*16000]
    for name,args,code,stream in [('arguments',arguments,0,None),('streams-and-exit',arguments[:8]+['--stream-check','--exit=37'],37,'pipe 中文 😀\n')]:
        text,row=invoke(name,[str(native(directory/fixture.name)),*args],code,input_text=stream);observed=json.loads(text)
        assert observed['arguments']==args and observed['desktop']==row['desktop'] and observed['pid']!=row['pid']
        assert Path(observed['cwd'])==proof and observed['environment']=='中文 😀 inherited'
        assert logical(observed['resourceModule'])==str(directory/fixture.name)
        assert not observed['internalContextPresent']
        module=Path(observed['module']);cache=module.parent
        assert cache.name.startswith('xlang-ide-start-') and not native(cache).exists()
        if stream:assert observed['stdin']==stream and Path(row['stderr']).read_text()=='startup-stream-check\n'
        staged.append({'program':str(module),'original':str(directory/fixture.name),'normalExitCleanupVerified':True});checks+=len(args)+8
    # Keep TEMP itself below MAX_PATH: GetTempPathW may skip oversized values.
    temporary=proof/('temp-'+('x'*max(1,220-len(str(proof))-6)));temporary.mkdir()
    assert 200<=len(str(temporary).encode('mbcs'))<245
    _,row=invoke('unusable-temp',[str(native(directory/fixture.name))],5,env={'TMP':str(temporary),'TEMP':str(temporary)})
    assert 'startup alias failed:' in Path(row['stderr']).read_text();checks+=2
    _,row=invoke('partial-context',[str(native(directory/fixture.name))],5,env={'XLANG_IDE_STARTUP_ORIGIN':str(native(directory/fixture.name))})
    assert 'startup alias failed:' in Path(row['stderr']).read_text();checks+=2
    settings=directory/'option/settings.json';native(settings).parent.mkdir(parents=True)
    font=str(directory/'data/fonts/NotoSansMonoCJKsc-Regular.otf')
    preferences={'format':'xlang-ide-settings-1','editor':{'font':{'path':font,'size':20}},'ui':{'font':{'path':font,'size':16}}}
    if compiler:preferences['toolchain']={'compiler':str(Path(compiler).absolute())};inputs[str(Path(compiler).absolute())]=sha(compiler)
    native(settings).write_text(json.dumps(preferences,ensure_ascii=False),encoding='utf8')
    protected={str(settings):sha(settings)}
    for kind,text in {'c':'// 中文注释\nint main(void) { return 0; }\n','xlang':'#doc\n描述：无可用短名启动\n#end\nfunc main(...) as int\n    var 值 as int = 1\n    return 值 - 1\nend\n'}.items():
        p=directory/('编辑文件.'+('c' if kind=='c' else 'xl'));native(p).write_text(text,encoding='utf8');protected[str(p)]=sha(p)
    for name,program,kind in [('gui-c','xlang_ide.exe','c'),('gui-xl-primary','xlang_ide.exe','xlang'),('gui-xl','xlang_ide.exe','xlang')]:
        image=proof/(name+'.png');source=directory/('编辑文件.'+('c' if kind=='c' else 'xl'))
        text,row=invoke(name,[str(native(directory/program)),'--frames','3','--no-session','--open-file',str(source),'--demo',kind,'--command','c:keyword:inline' if kind=='c' else 'language:func','--screenshot',str(image)])
        assert 'rendered 3 frames' in text and 'C and xlang registered' in text
        assert 'preferences:' not in Path(row['stderr']).read_text(encoding='utf8')
        with Image.open(image) as im:assert im.size==(1280,800) and max(im.convert('L').getextrema())-min(im.convert('L').getextrema())>50
        checks+=5
    if compiler:
        projectDirectory=proof/'mixed-project';projectDirectory.mkdir()
        project=projectDirectory/'混合项目.xlp'
        materials={'main.xl':'extern "native_answer" func nativeAnswer() as c_int\nfunc main(...) as int\n return nativeAnswer()-42\nend\n','native.c':'int native_answer(void) { return 42; }\n','混合项目.xlp':json.dumps({'format':'xlang-project-1','kind':'exe','entry':'main.xl','cSources':['native.c'],'ide':{'name':'无短名混合项目','activeConfiguration':'development','configurations':[{'id':'development','output':'build/app.exe'}]}},ensure_ascii=False)}
        for name,body in materials.items():p=projectDirectory/name;native(p).write_bytes(body.encode('utf8'));protected[str(p)]=sha(p)
        for task in ('build','run'):
            image=proof/('mixed-'+task+'.png')
            text,_=invoke('mixed-'+task,[str(native(directory/'xlang_ide.exe')),'--frames','3','--no-session','--project',str(project),'--open-file',str(projectDirectory/'main.xl'),'--task',task,'--wait-task','--screenshot',str(image)])
            assert 'built=1' in text and ('ran=1' if task=='run' else 'ran=0') in text and 'code=0' in text
            assert native(projectDirectory/'build/app.exe').is_file()
            checks+=4
    for path,digest in {**inputs,**protected}.items():assert sha(path)==digest,path
    assert not native(directory/'data/fonts/NotoSansMonoCJKsc-Regular.otf').is_symlink()
    images=[{'path':str(p),'sha256':sha(p),'width':1280,'height':800} for p in sorted(proof.glob('*.png'))]
    files=[{'path':logical(p),'sha256':sha(p)} for p in sorted(native(proof).rglob('*')) if p.is_file()]
    result={'status':'passed','utc':datetime.now(timezone.utc).isoformat(),'inputs':inputs,'protectedFiles':protected,'checks':checks,'calls':calls,'stagedPrograms':staged,'images':images,'files':files,'shortDirectoryAcpBytes':acp,'shortProgramPath':short.value,'removedShortNamesOnlyForFreshPrivateDirectories':removed,'volumePolicyChanged':False,'driverOrSystemChanged':False,'wholeNoShortNameVolumeVerified':False,'actualCreateProcessWDesktop':True,'neverSwitchedDesktop':True,'realNoUsableAliasGuiStartupVerified':True,'defaultOriginalSettingsCatalogAndFonts':True,'mixedProjectBuiltAndRanFromOriginalSettings':bool(compiler),'goalStatus':'active','formalRelease':False}
    (proof/'evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8');print(f'Native no-alias startup: {checks} checks; {len(calls)} launches, {len(images)} images; {proof}',flush=True)
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--fixture',required=True);parser.add_argument('--output',required=True);parser.add_argument('--compiler');args=parser.parse_args();check(args.fixture,args.output,args.compiler)
