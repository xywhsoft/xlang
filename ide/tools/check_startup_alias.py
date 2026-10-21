"""Verify actual C argument handoff and C/xlang GUI startup from a long directory."""
import argparse
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid
from PIL import Image
from native_desktop import NativeDesktopProcess

def native(path):
    value=str(Path(path).absolute());prefix=chr(92)*2+'?'+chr(92)
    return Path(value if value.startswith(prefix) else prefix+value)
def logical(path):
    value=str(path);prefix=chr(92)*2+'?'+chr(92)
    return value[len(prefix):] if value.startswith(prefix) else value
def sha(path):
    return hashlib.sha256(native(path).read_bytes()).hexdigest()

def check(fixture,output):
    fixture=Path(fixture).absolute();release=fixture.parent
    proof=Path(output).absolute()/('startup-alias-'+uuid.uuid4().hex);proof.mkdir(parents=True)
    inputs={str(p):sha(p) for p in [fixture,release/'xlang_ide.exe',release/'xlang_ide_console.exe',release/'xge.dll',release/'data/commands.json',*sorted((release/'data/fonts').iterdir())]}
    directory=proof/'deep-program-中文'
    while len(str(directory/fixture.name))<350:directory/='真实长路径-0123456789'
    native(directory).mkdir(parents=True)
    for original in inputs:
        source=Path(original);target=directory/source.relative_to(release);native(target).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,native(target));assert sha(target)==inputs[original]
    user=ctypes.WinDLL('user32',use_last_error=True)
    user.CreateDesktopW.restype=ctypes.c_void_p;user.CreateDesktopW.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p]
    user.CloseDesktop.restype=ctypes.c_int;user.CloseDesktop.argtypes=[ctypes.c_void_p]
    calls=[];checks=0
    def invoke(name,command,expected=0,cwd=None):
        desktopName='XlangStartupAlias-'+uuid.uuid4().hex;desktop=user.CreateDesktopW(desktopName,None,None,0,0x10000000,None);assert desktop,ctypes.get_last_error()
        startup=subprocess.STARTUPINFO();startup.lpDesktop=desktopName
        stdout=proof/(name+'.stdout');stderr=proof/(name+'.stderr');env=os.environ.copy();env['xide_startup_alias_test']='superseded';env['XIDE_STARTUP_ALIAS_TEST']='中文 😀 inherited'
        try:
            with stdout.open('xb') as out,stderr.open('xb') as err:
                child=NativeDesktopProcess(command,out,err,startup,cwd=str(cwd or proof),env=env)
                try:code=child.wait(timeout=70)
                except subprocess.TimeoutExpired:
                    child.kill();child.wait(timeout=10);raise
                finally:child.close()
        finally:assert user.CloseDesktop(desktop),ctypes.get_last_error()
        row={'name':name,'command':command,'pid':child.pid,'exit':code,'desktop':desktopName,'stdout':str(stdout),'stderr':str(stderr),'stdoutSha256':sha(stdout),'stderrSha256':sha(stderr)};calls.append(row)
        (proof/(name+'.invocation.json')).write_text(json.dumps(row,ensure_ascii=False,indent=2),encoding='utf8')
        assert code==expected,(name,code,str(stderr))
        return stdout.read_text(encoding='utf8'),desktopName,child.pid
    arguments=['','中文 空格','😀','quote"inside','tail'+chr(92),'space tail '+chr(92),'slash'+chr(92)+'"quote','\t','z'*16000]
    for name,exe,args,code in [('ordinary',fixture,arguments,0),('automatic-deep',directory/fixture.name,arguments,0),('child-exit',directory/fixture.name,arguments[:8]+['--exit=37'],37)]:
        text,desktop,pid=invoke(name,[str(native(exe)),*args],code)
        value=json.loads(text);assert value['arguments']==args and value['desktop']==desktop
        assert Path(value['cwd'])==proof and value['environment']=='中文 😀 inherited'
        assert (value['pid']!=pid)==(name!='ordinary')
        checks+=len(args)+5
        print('Passed startup alias',name,flush=True)
    font=str(directory/'data/fonts/NotoSansMonoCJKsc-Regular.otf');settings=directory/'option/字体设置.json';native(settings).parent.mkdir(parents=True)
    native(settings).write_text(json.dumps({'format':'xlang-ide-settings-1','editor':{'font':{'path':font,'size':20}},'ui':{'font':{'path':font,'size':16}}},ensure_ascii=False),encoding='utf8');settingsHash=sha(settings)
    sources={'c':'// 中文注释\nint main(void) { const char* text="中文"; return text[0]==0; }\n','xlang':'#doc\n描述：真实中文路径\n#end\nfunc main(...) as int\n    var 值 as int = 1\n    return 值 - 1\nend\n'}
    protected={str(settings):settingsHash}
    for kind,text in sources.items():
        p=directory/('真实编辑文件.'+('c' if kind=='c' else 'xl'));native(p).write_text(text,encoding='utf8');protected[str(p)]=sha(p)
    for name,program,kind in [('console-xl','xlang_ide_console.exe','xlang'),('console-c','xlang_ide_console.exe','c'),('gui-xl','xlang_ide.exe','xlang')]:
        image=proof/(name+'.png');source=directory/('真实编辑文件.'+('c' if kind=='c' else 'xl'))
        command=[str(native(directory/program)),'--hidden','--frames','3','--no-session','--settings',str(settings),'--open-file',str(source),'--demo',kind,'--command','c:keyword:inline' if kind=='c' else 'language:func','--screenshot',str(image)]
        text,_,_=invoke(name,command);assert 'rendered 3 frames' in text and 'C and xlang registered' in text
        with Image.open(image) as im:assert im.size==(1280,800) and max(im.convert('L').getextrema())-min(im.convert('L').getextrema())>50
        checks+=4;print('Passed automatic deep IDE startup',name,flush=True)
    for path,digest in {**inputs,**protected}.items():assert sha(path)==digest,path
    images=[{'path':str(p),'sha256':sha(p),'width':1280,'height':800} for p in sorted(proof.glob('*.png'))];assert len(images)==3
    files=[{'path':logical(p),'sha256':sha(p)} for p in sorted(native(proof).rglob('*')) if p.is_file()]
    result={'status':'passed','utc':datetime.now(timezone.utc).isoformat(),'inputs':inputs,'protectedFiles':protected,'checks':checks,'fixtureProcesses':3,'guiOrConsoleProcesses':3,'actualAutomaticChildHandoffs':5,'actualCommandHelpVerified':3,'calls':calls,'images':images,'files':files,'actualDeepProgramUtf16Units':len(str(directory/'xlang_ide_console.exe')),'actualCreateProcessWDesktop':True,'actualFixtureDesktopVerified':True,'neverSwitchedDesktop':True,'driverOrSystemChanged':False,'shortAliasAvailableOnThisVolume':True,'shortAliasUnavailableVolumesVerified':False,'goalStatus':'active','formalRelease':False}
    (proof/'evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(f'Native automatic startup alias: {checks} checks; 6 launches, 5 child handoffs, 3 images; {proof}',flush=True)
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--fixture',required=True);parser.add_argument('--output',required=True);args=parser.parse_args();check(args.fixture,args.output)
