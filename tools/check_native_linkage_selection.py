"""Exercise declared native variants through a real compiler (no engine/UI required)."""
from pathlib import Path
import argparse,copy,ctypes,hashlib,json,os,subprocess,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cli',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();cli=a.cli.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    packages=out/'packages';base=packages/'nativeProbe';alternate=out/'static/nativeProbe'
    for folder,value in ((base,11),(alternate,22)):
        (folder/'src').mkdir(parents=True)
        (folder/'src/api.xl').write_text('export extern "native_linkage_value" @no_raise func value() as c_int\n',encoding='utf-8')
        (folder/'native.c').write_text('int native_linkage_value(void) { return '+str(value)+'; }\n',encoding='utf-8')
    def manifest(linkage):
        return dict(format='xlang-package-1',name='nativeProbe',version='1.0.0',abiVersion='1',
            artifact=dict(kind='source',cSources=['native.c'],xlSources=['src/api.xl']),links=[],loadlibs=[],dependencies=[],
            native=dict(linkage=linkage,singleton=True,loadName='nativeProbe-'+linkage,version='1.0.0'))
    dynamic=manifest('dynamic');static=manifest('static')
    dynamic['native']['variants']={'static':'../../static/nativeProbe/xlang.package.json'}
    static['native']['variants']={'dynamic':'../../packages/nativeProbe/xlang.package.json'}
    def put(folder,data):(folder/'xlang.package.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
    put(base,dynamic);put(alternate,static)
    entry=out/'main.xl';entry.write_text('import nativeProbe\nfunc main(...) as int\n return int(nativeProbe.value())\nend\n',encoding='utf-8')
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
    env['XLANG_PACKAGE_PATH']=str(packages);rows=[]
    def run(name,args,expected=0,contains=None):
        start=time.monotonic();r=subprocess.run([str(v)for v in args],cwd=out,env=env,capture_output=True,timeout=120)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        assert r.returncode==expected,(name,r.returncode,log)
        if contains:assert contains.encode()in log.read_bytes(),(name,log)
        rows.append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),log=log.name))
    def build(name,expected,options=(),project=None):
        binary=out/(name+('.exe'if os.name=='nt'else''))
        run(name,[cli,'build',project or entry,*options,'-o',binary])
        run(name+'-execute',[binary],expected)
        return binary
    run('jit-auto',[cli,'run',entry],11)
    run('jit-static',[cli,'run',entry,'--native-linkage','static'],22)
    build('aot-auto',22,['--input-report',out/'inputs.json'])
    build('aot-dynamic',11,['--native-linkage','dynamic'])
    build('aot-static',22,['--native-linkage','static'])
    project=out/'app.xlp';project.write_text(json.dumps(dict(format='xlang-project-1',kind='exe',entry='main.xl',nativeLinkage='dynamic')),encoding='utf-8')
    build('project-dynamic',11,project=project)
    build('project-override',22,['--native-linkage','static'],project)
    run('invalid-mode',[cli,'build',entry,'--native-linkage','wrong'],1, '--native-linkage requires')
    run('missing-mode',[cli,'build',entry,'--native-linkage'],1, '--native-linkage requires')
    run('duplicate-mode',[cli,'build',entry,'--native-linkage','static','--native-linkage','dynamic'],1, '--native-linkage requires')
    # Mutating a chosen declaration must fail, never silently use the DLL form.
    changed=copy.deepcopy(static);changed['abiVersion']='2';put(alternate,changed)
    run('identity-mismatch',[cli,'build',entry,'-o',out/'bad.exe'],1,'native variant changes')
    changed=copy.deepcopy(static);changed['artifact']['xlSources']=['src/other.xl'];put(alternate,changed)
    run('language-mismatch',[cli,'build',entry,'-o',out/'bad.exe'],1,'native variant changes')
    changed=copy.deepcopy(static);changed['native']['linkage']='dynamic';put(alternate,changed)
    run('linkage-mismatch',[cli,'build',entry,'-o',out/'bad.exe'],1,'native variant changes')
    changed=copy.deepcopy(static);changed['dependencies']=[dict(name='other',version='1.0.0',abiVersion='1')];put(alternate,changed)
    run('dependency-mismatch',[cli,'build',entry,'-o',out/'bad.exe'],1,'native variant changes')
    put(alternate,static);changed=copy.deepcopy(dynamic);changed['native']['variants']['static']='missing.json';put(base,changed)
    run('missing-artifact',[cli,'build',entry,'-o',out/'bad.exe'],1,'unavailable static native manifest')
    changed=copy.deepcopy(dynamic);changed['native']['variants']['static']='/absolute/not-allowed.json';put(base,changed)
    run('absolute-variant',[cli,'build',entry,'-o',out/'bad.exe'],1,'must be a relative manifest path')
    changed=copy.deepcopy(dynamic);changed['native'].pop('variants');put(base,changed)
    run('explicit-unavailable',[cli,'build',entry,'--native-linkage','static','-o',out/'bad.exe'],1,'has no static native artifact')
    build('auto-no-alternative',11)
    put(base,dynamic)
    recorded=json.loads((out/'inputs.json').read_text(encoding='utf-8'))
    assert any(Path(v['path']).resolve()==(alternate/'xlang.package.json').resolve() for v in recorded['files'])
    assert any(Path(v['path']).resolve()==(alternate/'native.c').resolve() for v in recorded['files'])
    # Public API context reuse must reselect native inputs each time.
    class Options(ctypes.Structure):
        _fields_=[('size',ctypes.c_uint),('version',ctypes.c_uint),('workDir',ctypes.c_char_p),('tempDir',ctypes.c_char_p),('tccDllPath',ctypes.c_char_p),('tccLibPath',ctypes.c_char_p),('saveTemps',ctypes.c_int)]
    lib=ctypes.CDLL(str(cli.parent/('xlc.dll'if os.name=='nt'else'libxlc.so')))
    lib.xlcDefaultContextOptions.restype=Options
    lib.xlcCreateContext.argtypes=[ctypes.POINTER(Options)];lib.xlcCreateContext.restype=ctypes.c_void_p
    lib.xlcDestroyContext.argtypes=[ctypes.c_void_p]
    for name,args in [('xlcCompile',[ctypes.c_void_p]),('xlcSetNativeLinkage',[ctypes.c_void_p,ctypes.c_int]),('xlcAddSourceFile',[ctypes.c_void_p,ctypes.c_char_p]),('xlcEmitExe',[ctypes.c_void_p,ctypes.c_char_p]),('xlcRun',[ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p]),('xlcGetLastExitCode',[ctypes.c_void_p])]:
        getattr(lib,name).argtypes=args;getattr(lib,name).restype=ctypes.c_int
    lib.xlcGetLastError.argtypes=[ctypes.c_void_p];lib.xlcGetLastError.restype=ctypes.c_char_p
    previous=os.environ.get('XLANG_PACKAGE_PATH');os.environ['XLANG_PACKAGE_PATH']=str(packages)
    options=lib.xlcDefaultContextOptions();ctx=lib.xlcCreateContext(ctypes.byref(options));assert ctx
    def okay(v):assert v,lib.xlcGetLastError(ctx)
    try:
        okay(lib.xlcAddSourceFile(ctx,str(entry).encode()));okay(lib.xlcCompile(ctx))
        for mode,expected in ((0,22),(2,11),(1,22),(0,22)):
            okay(lib.xlcSetNativeLinkage(ctx,mode));binary=out/('context-'+str(len(rows))+('.exe'if os.name=='nt'else''))
            okay(lib.xlcEmitExe(ctx,str(binary).encode()));run('context-'+str(len(rows)),[binary],expected)
            okay(lib.xlcSetNativeLinkage(ctx,0));okay(lib.xlcRun(ctx,0,None));assert lib.xlcGetLastExitCode(ctx)==11
        assert not lib.xlcSetNativeLinkage(ctx,99);assert b'native linkage must be' in lib.xlcGetLastError(ctx)
        rows.append(dict(name='context-cache-and-invalid-mode',passed=True))
    finally:
        lib.xlcDestroyContext(ctx)
        if previous is None:os.environ.pop('XLANG_PACKAGE_PATH',None)
        else:os.environ['XLANG_PACKAGE_PATH']=previous
    # Legacy packages with no linkage annotation retain their implementation.
    legacy=copy.deepcopy(dynamic);legacy.pop('native');put(base,legacy)
    build('legacy-auto',11);build('legacy-explicit',11,['--native-linkage','static'])
    put(base,dynamic)
    report=dict(passed=True,complete=True,compiler=str(cli),compilerSha256=hashlib.sha256(cli.read_bytes()).hexdigest(),rows=rows,scope=__doc__)
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print('Passed',len(rows),'native linkage checks')

if __name__=='__main__':main()
