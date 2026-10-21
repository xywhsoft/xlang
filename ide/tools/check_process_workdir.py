"""Verify real existing-short-path working directories and genuine no-alias refusal."""
import argparse,ctypes,hashlib,json,os,re,shutil,subprocess,uuid
from pathlib import Path
from PIL import Image
from native_desktop import NativeDesktopProcess
from check_startup_alias import native,logical,sha

def check(fixture,output,ide=None,compiler=None):
 fixture=Path(fixture).absolute();root=Path(output).absolute()/('process-workdir-'+uuid.uuid4().hex);root.mkdir(parents=True)
 shutil.copy2(__file__,root/'checker-input.py')
 inputs={str(fixture):sha(fixture),str(fixture.parent/'xge.dll'):sha(fixture.parent/'xge.dll')}
 k=ctypes.WinDLL('kernel32',use_last_error=True)
 k.CreateFileW.restype=ctypes.c_void_p;k.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p]
 k.CloseHandle.argtypes=[ctypes.c_void_p];k.SetFileShortNameW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p];k.GetShortPathNameW.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_ulong]
 directories={};removed=[];protected={}
 for kind in ('aliased','noalias'):
  directory=root/kind
  for i in range(3):
   directory/=f'segment-{i}-'+('abcdefghijklmnop'*6)+'中文';native(directory).mkdir(parents=True)
   if kind=='noalias':
    assert str(directory).casefold().startswith(str(root).casefold()+os.sep)
    h=k.CreateFileW(str(native(directory)),0x10000,7,None,3,0x02000000,None);assert h!=ctypes.c_void_p(-1).value,ctypes.get_last_error()
    try:assert k.SetFileShortNameW(h,''),ctypes.get_last_error()
    finally:assert k.CloseHandle(h)
    removed.append(str(directory))
  short=ctypes.create_unicode_buffer(32768);assert 0<k.GetShortPathNameW(str(native(directory)),short,32768)<32768
  ordinary=logical(short.value);assert len(str(directory))>260
  assert (len(ordinary)<259 if kind=='aliased' else len(ordinary)>260),(kind,ordinary)
  directories[kind]={'original':str(directory),'existingShortPath':ordinary}
  p=directory/'input.txt';native(p).write_bytes('actual project 中文 😀\n'.encode('utf8'));protected[str(p)]=sha(p)
 u=ctypes.WinDLL('user32',use_last_error=True);u.CreateDesktopW.restype=ctypes.c_void_p;u.CreateDesktopW.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p];u.CloseDesktop.argtypes=[ctypes.c_void_p]
 calls=[]
 def invoke(label,args,expected=0):
  desktopName='XlangProcessWorkdir-'+uuid.uuid4().hex;desktop=u.CreateDesktopW(desktopName,None,None,0,0x10000000,None);assert desktop
  outpath=root/(label+'.stdout');errpath=root/(label+'.stderr')
  try:
   with outpath.open('xb') as out,errpath.open('xb') as err:
    child=NativeDesktopProcess(args,out,err,desktopName,cwd=str(root),env=os.environ.copy())
    try:code=child.wait(timeout=70)
    except subprocess.TimeoutExpired:child.kill();child.wait(timeout=10);raise
    finally:child.close()
  finally:assert u.CloseDesktop(desktop)
  row={'label':label,'args':args,'desktop':desktopName,'pid':child.pid,'exit':code,'stdout':str(outpath),'stdoutSha256':sha(outpath),'stderr':str(errpath),'stderrSha256':sha(errpath)};calls.append(row)
  (root/(label+'.invocation.json')).write_text(json.dumps(row,ensure_ascii=False,indent=2),encoding='utf8');assert code==expected,(label,code,errpath.read_text(encoding='utf8',errors='replace'))
  return outpath.read_text(encoding='utf8')
 text=invoke('native-process',[str(fixture),directories['aliased']['original'],directories['noalias']['original']]);assert 'Process working-directory checks: 19 passed' in text,text
 images=[]
 if ide and compiler:
  ide=Path(ide).absolute();compiler=Path(compiler).absolute();inputs.update({str(ide):sha(ide),str(compiler):sha(compiler)})
  for kind in ('aliased','noalias'):
   workingDirectory=Path(directories[kind]['original'])
   directory=root/'mixed-project' if kind=='aliased' else workingDirectory
   native(directory).mkdir(parents=True,exist_ok=True);project=directory/'项目.xlp'
   materials={'main.xl':'extern "native_answer" func nativeAnswer() as c_int\nfunc main(...) as int\n return nativeAnswer()-42\nend\n','native.c':'#include <stdio.h>\nint native_answer(void) { FILE *f=fopen("input.txt","rb"); if(!f)return -1; int c=fgetc(f); fclose(f); return c==\'a\' ? 42 : -2; }\n','项目.xlp':json.dumps({'format':'xlang-project-1','kind':'exe','entry':'main.xl','cSources':['native.c'],'ide':{'name':'工作目录检查 '+kind,'activeConfiguration':'development','configurations':[{'id':'development','output':'build/app.exe','run':{'workingDirectory':str(workingDirectory)}}]}},ensure_ascii=False)}
   for name,body in materials.items():p=directory/name;native(p).write_bytes(body.encode('utf8'));protected[str(p)]=sha(p)
   if kind=='noalias':
    p=directory/'build/app.exe';native(p).parent.mkdir();native(p).write_bytes(b'PREVIOUS-OUTPUT-MUST-SURVIVE');protected[str(p)]=sha(p)
   tasks=('build','run') if kind=='aliased' else ('run',)
   for task in tasks:
    label=kind+'-'+task;image=root/(label+'.png')
    text=invoke(label,[str(ide),'--frames','3','--no-session','--compiler',str(compiler),'--project',str(project),'--open-file',str(directory/'main.xl'),'--task',task,'--wait-task','--screenshot',str(image)],0 if kind=='aliased' else 1)
    if kind=='aliased':assert 'built=1' in text and ('ran=1' if task=='run' else 'ran=0') in text and 'code=0' in text,text
    else:assert 'built=0 ran=0' in text and 'no verified short path (Windows error 267)' in text and 'shorter location' in text,text
    with Image.open(image) as im:assert im.size==(1280,800) and max(im.convert('L').getextrema())-min(im.convert('L').getextrema())>50
    images.append({'path':str(image),'sha256':sha(image)})
 for path,digest in {**inputs,**protected}.items():assert sha(path)==digest,path
 files=[{'path':logical(p),'sha256':sha(p)} for p in sorted(native(root).rglob('*')) if p.is_file()]
 result={'status':'passed','directories':directories,'calls':calls,'images':images,'inputs':inputs,'protectedFiles':protected,'nativeChecks':19,'removedShortNamesOnlyForFreshPrivateDirectories':removed,'volumePolicyChanged':False,'actualCreateProcessWDesktop':True,'neverSwitchedDesktop':True,'mixedProjectBuiltAndRanWithDeepAliasedRunDirectory':bool(ide and compiler),'projectManifestDirectoryOrdinaryForSuccessfulMixedBuild':True,'noAliasFailureClearAndOldOutputPreserved':bool(ide and compiler),'noAliasWorkingDirectorySupported':False,'externalCompilerDeepProjectInputsVerified':False,'files':files,'goalStatus':'active','formalRelease':False}
 (root/'evidence.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8');print('Process working-directory acceptance:',root,flush=True);return result

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--fixture',required=True);parser.add_argument('--output',required=True);parser.add_argument('--ide');parser.add_argument('--compiler');args=parser.parse_args();check(args.fixture,args.output,args.ide,args.compiler)
