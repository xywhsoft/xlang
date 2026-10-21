"""Exercise real deep C/xlang project inputs, receipts and output; own fresh paths only."""
import argparse, ctypes, datetime, json, os, shutil, subprocess, uuid
from pathlib import Path
from PIL import Image
from native_desktop import NativeDesktopProcess
from check_startup_alias import native, logical, sha

def check(compiler, output, ide=None, baseline=None):
    compiler = Path(compiler).absolute(); ide = Path(ide).absolute() if ide else None
    baseline = Path(baseline).absolute() if baseline else None
    root = Path(output).absolute()/('compiler-deep-'+uuid.uuid4().hex); root.mkdir(parents=True)
    shutil.copy2(__file__, root/'checker-input.py')
    protected = {str(compiler): sha(compiler)}; calls = []; images = []; checks = []; directories = {}
    for executable in (compiler, baseline):
        if executable:
            for name in ('xl.exe','xl.dll','xlc.dll'):
                p = executable.parent/name; protected[str(p)] = sha(p)
    if ide:
        protected[str(ide)] = sha(ide); protected[str(ide.parent/'xge.dll')] = sha(ide.parent/'xge.dll')
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.CreateFileW.restype = ctypes.c_void_p
    k.CreateFileW.argtypes = [ctypes.c_wchar_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p]
    k.CloseHandle.argtypes = [ctypes.c_void_p]; k.SetFileShortNameW.argtypes = [ctypes.c_void_p,ctypes.c_wchar_p]
    k.GetShortPathNameW.argtypes = [ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_ulong]
    u = ctypes.WinDLL('user32', use_last_error=True); u.CreateDesktopW.restype = ctypes.c_void_p
    u.CreateDesktopW.argtypes = [ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_ulong,ctypes.c_ulong,ctypes.c_void_p]
    u.CloseDesktop.argtypes = [ctypes.c_void_p]
    def done(name): checks.append(name)
    def invoke(label, args, cwd=None, expected=0):
        args = [str(x) for x in args]; desktop_name = 'XlangDeepCompiler-'+uuid.uuid4().hex
        desktop = u.CreateDesktopW(desktop_name,None,None,0,0x10000000,None); assert desktop
        stdout = root/(label+'.stdout'); stderr = root/(label+'.stderr')
        started = datetime.datetime.now(datetime.timezone.utc).isoformat()
        try:
            with stdout.open('xb') as out, stderr.open('xb') as err:
                child = NativeDesktopProcess(args,out,err,desktop_name,cwd=str(cwd or root),env=os.environ.copy())
                try: code = child.wait(timeout=90)
                except subprocess.TimeoutExpired: child.kill(); child.wait(timeout=10); raise
                finally: child.close()
        finally: assert u.CloseDesktop(desktop)
        row = {'label':label,'arguments':args,'cwd':str(cwd or root),'desktop':desktop_name,'pid':child.pid,'exit':code,'startedUtc':started,
            'stdout':str(stdout),'stdoutSha256':sha(stdout),'stderr':str(stderr),'stderrSha256':sha(stderr)}
        calls.append(row); (root/(label+'.invocation.json')).write_text(json.dumps(row,ensure_ascii=False,indent=2),encoding='utf8')
        text = stdout.read_text(encoding='utf8')+stderr.read_text(encoding='utf8')
        assert code == expected, (label,code,text)
        done(label+'-actual-exit-'+str(code)); return text
    status = 'failed'; removed = []
    try:
        for kind in ('aliased','noalias'):
            directory = root/kind
            for i in range(3):
                directory /= 'segment-'+str(i)+'-'+('abcdefghijklmnop'*6)+'中文😀'
                native(directory).mkdir(parents=True)
                if kind == 'noalias':
                    assert str(directory).casefold().startswith(str(root).casefold()+os.sep)
                    h = k.CreateFileW(str(native(directory)),0x10000,7,None,3,0x02000000,None)
                    assert h != ctypes.c_void_p(-1).value, ctypes.get_last_error()
                    try: assert k.SetFileShortNameW(h,''), ctypes.get_last_error()
                    finally: assert k.CloseHandle(h)
                    removed.append(str(directory))
            short = ctypes.create_unicode_buffer(32768)
            assert 0 < k.GetShortPathNameW(str(native(directory)),short,32768) < 32768
            ordinary = logical(short.value)
            assert len(str(directory)) > 260 and (len(ordinary) < 259 if kind == 'aliased' else len(ordinary) > 260)
            directories[kind] = {'original':str(directory),'existingShortPath':ordinary}
            done(kind+'-real-deep-directory-verified')
            project = directory/'项目.xlp'; native(directory/'headers').mkdir()
            materials = {'main.xl':'extern "native_answer" func nativeAnswer() as c_int\nfunc main(...) as int\n return nativeAnswer()-42\nend\n',
                'native.c':'#include <stdio.h>\n#include "headers/../headers/数据 😀.h"\nint native_answer(void) { FILE *f=fopen("input.txt","rb"); if(!f)return -1; int c=fgetc(f); fclose(f); if(c!=\'a\')return -2; puts("DEEP-MIXED-42"); return DEEP_ANSWER; }\n',
                'headers/数据 😀.h':'#define DEEP_ANSWER 42\n','input.txt':'actual deep original project 中文 😀\n',
                '项目.xlp':json.dumps({'format':'xlang-project-1','kind':'exe','entry':'main.xl','cSources':['native.c'],
                    'ide':{'name':'深工程 '+kind,'activeConfiguration':'development','configurations':[{'id':'development','output':'build/app.exe'}]}},ensure_ascii=False)}
            for name, body in materials.items():
                p = directory/name; native(p).write_bytes(body.encode('utf8')); protected[str(p)] = sha(p)
            output_dir = directory/'direct-output'; native(output_dir).mkdir()
            artifact = output_dir/'app.exe'; receipt = output_dir/'input-report.json'; header = output_dir/'public.h'
            if baseline:
                previous = output_dir/'previous.exe'; native(previous).write_bytes(b'OLD-ARTIFACT-REMAINS'); protected[str(previous)] = sha(previous)
                text = invoke(kind+'-baseline-read-failure',[baseline,'build',project,'-o',previous,'--input-report',output_dir/'old-report.json'],expected=1)
                assert 'cannot read xlp:' in text and not native(output_dir/'old-report.json').exists(), text
                done(kind+'-baseline-failed-before-output-write')
            invoke(kind+'-direct-build',[compiler,'build',project,'-o',artifact,'--input-report',receipt])
            invoke(kind+'-deep-cgen',[compiler,'cgen',directory/'main.xl','--emit-header',header,'--emit',output_dir/'module.c'])
            value = json.loads(native(receipt).read_text(encoding='utf-8-sig'))
            assert value['complete'] and value['scope'] == 'xlang-native-build-disk-inputs'
            assert value['artifact']['sha256'] == sha(artifact)
            normalize = lambda p: str(p).replace('\\','/').casefold()
            files = {normalize(row['path']):row for row in value['files']}
            for name in ('项目.xlp','main.xl','native.c','headers/数据 😀.h'):
                p = directory/name; row = files[normalize(p)]
                assert row['sha256'] == sha(p) and row['size'] == native(p).stat().st_size
                assert not row['path'].startswith('\\\\?\\')
            assert normalize(value['artifact']['path']) == normalize(artifact)
            assert native(header).stat().st_size > 0
            done(kind+'-original-deep-inputs-and-artifact-digests-bound')
            invoke(kind+'-deep-semantic-check',[compiler,'check',project,'--json','--source-report','--semantic-index'])
            semantic = json.loads((root/(kind+'-deep-semantic-check.stdout')).read_text(encoding='utf-8-sig'))
            assert semantic['ok']; done(kind+'-real-deep-xlang-semantic-check')
            if kind == 'aliased':
                text = invoke('aliased-direct-execute',[native(artifact)],cwd=ordinary)
                assert 'DEEP-MIXED-42' in text; done('aliased-direct-execute-read-original-relative-file')
            if ide:
                if kind == 'noalias':
                    old_output = directory/'build/app.exe'; native(old_output).parent.mkdir()
                    native(old_output).write_bytes(b'OLD-IDE-OUTPUT-REMAINS'); protected[str(old_output)] = sha(old_output)
                for task in (('build','run') if kind == 'aliased' else ('run',)):
                    label = kind+'-ide-'+task; png = root/(label+'.png')
                    text = invoke(label,[ide,'--frames','3','--no-session','--compiler',compiler,'--project',project,
                        '--open-file',directory/('native.c' if task=='build' else 'main.xl'),'--task',task,'--wait-task','--screenshot',png],expected=0 if kind=='aliased' else 1)
                    if kind == 'aliased':
                        assert 'built=1' in text and 'code=0' in text and ('ran=1' if task=='run' else 'ran=0') in text,text
                        if task=='run':
                            run_logs = [p for p in native(directory/'build/.xlang-ide-build').rglob('run.log')
                                if 'DEEP-MIXED-42' in p.read_text(encoding='utf8')]
                            assert len(run_logs)==1,run_logs
                            done('aliased-ide-run-raw-log-proves-original-relative-input')
                    else: assert 'built=0 ran=0' in text and 'no verified short path (Windows error 267)' in text,text
                    with Image.open(png) as im: assert im.size==(1280,800) and im.convert('L').getextrema()[1]-im.convert('L').getextrema()[0]>50
                    images.append({'path':str(png),'sha256':sha(png)}); done(label+'-native-image-and-task-result')
        boundary = root/'oversize-manifest'
        while len(str(boundary/'request.xlp').encode('utf8')) <= 4095: boundary /= 'native-path-boundary-0123456789'
        native(boundary).mkdir(parents=True); manifest = boundary/'request.xlp'
        native(manifest).write_text(json.dumps({'format':'xlang-project-1','kind':'exe','entry':'main.xl'}),encoding='utf8')
        protected[str(manifest)] = sha(manifest); previous = root/'boundary-previous.exe'
        previous.write_bytes(b'BOUNDARY-OLD-ARTIFACT-REMAINS'); protected[str(previous)] = sha(previous)
        for mode in ('untracked','tracked'):
            args = [compiler,'build',manifest,'-o',previous]
            if mode=='tracked': args += ['--input-report',root/'boundary-inputs.json']
            text = invoke('over-limit-manifest-'+mode+'-clear-failure',args,expected=1)
            assert 'xlp path is too long (limit: 4095 UTF-8 bytes)' in text,text
            assert not (root/'boundary-inputs.json').exists()
            done('over-limit-'+mode+'-explicit-error-and-old-output-preserved')
        for p, digest in protected.items(): assert sha(p) == digest,p
        done('all-original-inputs-toolchains-and-old-outputs-unchanged'); status = 'passed'
    finally:
        files = [{'path':logical(p),'sha256':sha(p)} for p in sorted(native(root).rglob('*')) if p.is_file()]
        report = {'status':status,'compiler':str(compiler),'baseline':str(baseline) if baseline else None,'ide':str(ide) if ide else None,
            'directories':directories,'calls':calls,'images':images,'checks':checks,'protectedFiles':protected,'files':files,
            'removedShortNamesOnlyForFreshPrivateDirectories':removed,'volumePolicyChanged':False,'actualCreateProcessWDesktop':True,
            'neverSwitchedDesktop':True,'deepNoAliasFileInputsFromOrdinaryWorkingDirectory':status=='passed',
            'deepAliasedProjectBuiltAndRanInIde':bool(ide) and status=='passed','noAliasWorkingDirectorySupported':False,
            'formalRelease':False,'goalStatus':'active'}
        (root/'evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
        print('Deep compiler path acceptance:',status,root,flush=True)
    return report

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('--compiler',required=True)
    parser.add_argument('--output',required=True); parser.add_argument('--ide'); parser.add_argument('--baseline')
    a = parser.parse_args(); check(a.compiler,a.output,a.ide,a.baseline)
