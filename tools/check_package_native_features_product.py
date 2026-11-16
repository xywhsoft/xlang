"""Qualify native capabilities in full compiler DLL/API, TCC/VFS and nested AOT.

Requires an explicit successful full O2 build receipt. Uses isolated local
source packages, public API clients and independent source-free producer output.
It installs nothing and performs no graphics/network/backend operation.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,struct,subprocess,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler-report','out'):p.add_argument('--'+name,type=Path,required=True)
    for name in('dynamic-packages','static-packages'):p.add_argument('--'+name,type=Path)
    a=p.parse_args();root,build,out=[v.resolve()for v in(a.root,a.compiler_report,a.out)]
    out.mkdir(parents=True,exist_ok=False);sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    receipt=json.loads(build.read_text());assert receipt['complete']and receipt['passed']and receipt['fullTccXrtAndVfs']and receipt['optimization']=='O2'
    source=Path(receipt['source']);runtime=source/'release'/('win_x64'if os.name=='nt'else'linux_x64')
    cli=runtime/('xl.exe'if os.name=='nt'else'xl');cc=Path(shutil.which('gcc')).resolve()
    files=[Path(__file__).resolve(),build,cc,root/'test/cases/package_native_features_api.c',root/'test/cases/compiled_package_registry.c']
    freeze=json.loads(Path(receipt['sourceFreeze']).read_text());assert sha(receipt['sourceFreeze'])==receipt['sourceFreezeSha256']
    files.append(Path(receipt['sourceFreeze']))
    core=['src/package/xl6_package.c','src/package/xl6_package.h','src/package/xl6_package_native_variants.inc',
        'src/package/xl6_compiled_package.h','src/package/xl6_compiled_interface.inc','src/package/xl6_compiled_native_features.inc',
        'src/compile/xl6_imports.c','src/compile/xl6_imports.h','src/compile/xl6_import_native_features.inc',
        'src/compile/xl6_import_compiled_features.inc','src/compile/xl6_compiled_packages.inc','src/api/xl_compiled_package.inc',
        'src/ir/xl6_ir_compiled_package_verify.c']
    for name in core:
        assert sha(root/name)==sha(source/name)==freeze['files'][name],name
        files.extend((root/name,source/name))
    for item in receipt['products'].values():assert sha(item['path'])==item['sha256'];files.append(Path(item['path']))
    files.extend(f for f in(runtime/'include').rglob('*')if f.is_file())
    inputs={str(f):sha(f)for f in files};report=dict(complete=False,passed=False,fullCompilerBuild=True,shippingDelivery=False,scope=__doc__,inputs=inputs,rows=[],programs=[],generatedInputs={})
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
    scratch=out/'tmp';scratch.mkdir();env.update(TMP=str(scratch),TEMP=str(scratch),TMPDIR=str(scratch),PYTHONDONTWRITEBYTECODE='1',PYTHONUTF8='1')
    key='PATH'if os.name=='nt'else'LD_LIBRARY_PATH';env[key]=str(runtime)+os.pathsep+env.get(key,'')
    packages=out/'packages';packages.mkdir();env['XLANG_PACKAGE_PATH']=str(packages)
    def save():
        target=out/'results.json.tmp';target.write_text(json.dumps(report,indent=2)+'\n');target.replace(out/'results.json')
    def put(path,data):
        path.parent.mkdir(parents=True,exist_ok=True)
        if isinstance(data,bytes):path.write_bytes(data)
        else:path.write_text(data)
        report['generatedInputs'][str(path)]=sha(path)
    def run(name,command,expected=0,diagnostic=None,cwd=out,override=None):
        command=list(map(str,command));local=env.copy();local.update(override or{});started=time.monotonic()
        z=subprocess.run(command,cwd=cwd,env=local,capture_output=True,timeout=900)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        row=dict(name=name,command=command,exitCode=z.returncode,expected=expected,passed=z.returncode==expected,seconds=time.monotonic()-started,logSha256=sha(log));report['rows'].append(row);save();print(name,z.returncode,flush=True)
        assert row['passed'],log
        if diagnostic:assert diagnostic.encode()in log.read_bytes(),log
    u32=lambda value:struct.pack('<I',value)
    text=lambda value:u32(len(value.encode()))+value.encode()
    def wire(name,provided=(),required=(),dependencies=(),imports=()):
        payload=b'NFM1'+text(name)+text('1')+text('1')
        for items in(required,provided,dependencies):payload+=u32(len(items))+b''.join(text(v)for v in sorted(items))
        data=b'XLP2'+struct.pack('<9I',0,0,0,len(payload),len(imports),0,0,0x31464e58,1)+payload
        for name in imports:data+=struct.pack('<9I',2,0,len(name),0,0,0,0,0,0)+name.encode()
        return data
    put(out/'provider.xlp',wire('provider',provided=['GPU3D']))
    put(out/'missing.xlp',wire('provider'))
    put(out/'consumer.xlp',wire('consumer',required=['GPU3D'],dependencies=['provider'],imports=['provider']))
    put(out/'legacy.xlp',b'XLP1'+bytes(36))
    def manifest(name,required=(),provided=(),dependencies=()):
        return dict(format='xlang-package-1',name=name,version='1',abiVersion='1',artifact=dict(xlSources=['api.xl']),
            requiresNativeFeatures=list(required),native=dict(features=list(provided)),
            dependencies=[dict(name=d,version='1',abiVersion='1')for d in dependencies])
    provider=manifest('sourceProvider',provided=['GPU3D']);provider['native'].update(linkage='dynamic',variants=dict(static='static.json'))
    selected=copy.deepcopy(provider);selected['native']=dict(linkage='static',features=[])
    put(packages/'sourceProvider/xlang.package.json',json.dumps(provider)+'\n')
    put(packages/'sourceProvider/static.json',json.dumps(selected)+'\n')
    put(packages/'sourceProvider/api.xl','export const int value=1\n')
    put(packages/'sourceConsumer/xlang.package.json',json.dumps(manifest('sourceConsumer',required=['GPU3D'],dependencies=['sourceProvider']))+'\n')
    put(packages/'sourceConsumer/api.xl','export const int answer=42\n')
    tcc=source/'res/tcc/win64/x86_64'if os.name=='nt'else Path(receipt['externalBuildDirectory'])/'build/tcc-dist'
    if not tcc.is_dir()and os.name!='nt':tcc=build.parent/'native-build-materials/tcc-dist'
    assert tcc.is_dir(),tcc
    for lib in('xl','xlc'):
        library=runtime/(lib+'.dll'if os.name=='nt'else'lib'+lib+'.so')
        for opt in('O0','O2'):
            for test in(('registry','compiled_package_registry.c'),('features','package_native_features_api.c')):
                # The feature client uses XLC as well as XL; XL's registry lane
                # remains independent of compiler-only APIs.
                if lib=='xl'and test[0]=='features':continue
                folder=out/(lib+'-'+opt+'-'+test[0]);folder.mkdir();exe=folder/('client.exe'if os.name=='nt'else'client')
                command=[cc,'-std=c11','-'+opt,'-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(runtime),
                    root/'test/cases'/test[1],library,'-o',exe]
                if os.name!='nt':command+=['-Wl,-rpath,'+str(runtime),'-ldl','-lm','-pthread']
                run(lib+'-'+opt+'-'+test[0]+'-build',command)
                args=[exe,library,tcc]+([out]if test[0]=='features'else[])
                run(lib+'-'+opt+'-'+test[0]+'-run',args,cwd=folder)
                report['programs'].append(dict(path=str(exe),sha256=sha(exe)))
    dynamic=out/'reused-dynamic.bin';assert dynamic.is_file();run('reused-dynamic-execute',[dynamic])
    assert not(out/'reused-auto-rejected.bin').exists()and not(out/'reused-auto-rejected-again.bin').exists()
    for name,data in [('provider',manifest('producerProvider',provided=['GPU3D'])),
        ('bridge',manifest('producerBridge',dependencies=['producerProvider'])),
        ('consumer',manifest('producerConsumer',required=['GPU3D'],dependencies=['producerBridge']))]:
        folder=packages/data['name'];put(folder/'xlang.package.json',json.dumps(data)+'\n')
        put(folder/'api.xl','export const int answer=42\n')
    host=out/'producer.xl';put(host,'import xlang\nimport producerConsumer\nauto engine=xlang.Context()\nauto package=xlang.compiledPackage("producerConsumer")\nassert engine.importPackage(package)\nassert engine.run("import producerConsumer\\nassert producerConsumer.answer==42\\n")\nprintln("native_features_source_free=ok")\n')
    exe=out/('producer.exe'if os.name=='nt'else'producer');run('producer-aot-build',[cli,'build',host,'-o',exe])
    deploy=out/'source-free';deploy.mkdir();shutil.copy2(exe,deploy/exe.name)
    run('source-free-nested-compiler',[deploy/exe.name],diagnostic='native_features_source_free=ok',cwd=deploy,override={'XLANG_PACKAGE_PATH':''})
    report['programs'].extend(dict(path=str(f),sha256=sha(f))for f in(dynamic,exe,deploy/exe.name))
    if a.dynamic_packages or a.static_packages:
        assert a.dynamic_packages and a.static_packages
        featureSource=out/'matched-sdk.xl';put(featureSource,'import xge3d\nfunc main(...) as int\n return 0\nend\n')
        debug=manifest('sdkDebugConsumer',required=['XGE_HAS_DEBUGMODE'],dependencies=['_xge_native'])
        debug['dependencies'][0].update(version='3.0.0',abiVersion='2')
        put(packages/'sdkDebugConsumer/xlang.package.json',json.dumps(debug)+'\n')
        put(packages/'sdkDebugConsumer/api.xl','export const int value=1\n')
        debugSource=out/'missing-debug.xl';put(debugSource,'import sdkDebugConsumer\n')
        for kind,path in(('dynamic',a.dynamic_packages),('static',a.static_packages)):
            path=path.resolve()
            # Preserve hashes of every matched package byte consulted by this
            # optional lane; metadata decisions do not execute its backend.
            for f in path.rglob('*'):
                if f.is_file():inputs[str(f)]=sha(f)
            native=json.loads((path/'_xge_native/xlang.package.json').read_text())
            assert 'XGE_ENABLE_3D'in native['native']['features']and 'XGE_HAS_DEBUGMODE'not in native['native']['features']
            override={'XLANG_PACKAGE_PATH':os.pathsep.join((str(packages),str(path)))}
            run(kind+'-matched-sdk-check',[cli,'check',featureSource,'--native-linkage',kind],override=override)
            run(kind+'-matched-sdk-debug-reject',[cli,'check',debugSource,'--native-linkage',kind],expected=1,
                diagnostic='requires native feature XGE_HAS_DEBUGMODE',override=override)
    report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()),generatedInputsUnchanged=all(sha(f)==h for f,h in report['generatedInputs'].items()))
    report['passed']=report['inputsUnchanged']and report['generatedInputsUnchanged'];save();assert report['passed']


if __name__=='__main__':main()
