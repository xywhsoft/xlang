"""Exercise native feature decisions in the real source import frontend.

This links current parser/import/diagnostic units to a qualified XRT runtime.
It is not full compiler/TCC/VFS or binary compiled-package qualification.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,subprocess,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','runtime','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,runtime,out=[v.resolve()for v in(a.root,a.runtime,a.out)]
    out.mkdir(parents=True,exist_ok=False);cc=Path(shutil.which('gcc')).resolve();sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    sources=['src/compile/xl6_imports.c','src/compile/xl6_source_overlay.c','src/compile/xl6_source_report.c','src/compile/xl6_build_inputs.c',
        'src/sem/xl6_completion.c','src/package/xl6_package.c','src/abi/xl6_abi_json.c','src/abi/xl6_callable.c','src/type/xl6_type_model.c',
        'src/sem/xl6_type_registry.c','src/common/xl6_console.c','src/common/xl6_output.c','src/parser/xl6_parser_api.c',
        'src/parser/xl6_parser.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c','src/lexer/xl6_token.c','src/ast/xl6_ast.c','src/diag/xl6_diag.c']
    files=[Path(__file__).resolve(),cc,runtime,root/'test/cases/package_native_features_import.c',*(root/f for f in sources)]
    files += [f for folder in (root/'src',runtime.parent/'include')for f in folder.rglob('*')if f.is_file()and f.suffix in('.h','.inc')]
    inputs={str(f):sha(f)for f in files};report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerQualification=False,inputs=inputs,rows=[])
    def save():
        temp=out/'results.json.tmp';temp.write_text(json.dumps(report,indent=2)+'\n');temp.replace(out/'results.json')
    def run(name,command,expected_text=None):
        command=list(map(str,command));env=os.environ.copy();env['XLANG_PACKAGE_PATH']=str(out/'packages')
        key='PATH'if os.name=='nt'else'LD_LIBRARY_PATH';env[key]=str(runtime.parent)+os.pathsep+env.get(key,'')
        start=time.monotonic();z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert not z.returncode,log
        if expected_text:assert expected_text.encode()in log.read_bytes(),log
    def dependency(name):return dict(name=name,version='1',abiVersion='1')
    def package(name,requirements=(),features=(),dependencies=()):
        return dict(format='xlang-package-1',name=name,version='1',abiVersion='1',
            artifact=dict(xlSources=['api.xl']),requiresNativeFeatures=list(requirements),
            native=dict(features=list(features)),dependencies=[dependency(n)for n in dependencies])
    def put(data):
        folder=out/'packages'/data['name'];folder.mkdir(parents=True,exist_ok=True)
        (folder/'xlang.package.json').write_text(json.dumps(data)+'\n');(folder/'api.xl').write_text('export const int value=1\n')
    entry=out/'main.xl';entry.write_text('import consumer\nfunc main(...) as int\n return 0\nend\n')
    provider=package('provider',features=['GPU3D']);bridge=package('bridge',dependencies=['provider'])
    consumer=package('consumer',requirements=['GPU3D'],dependencies=['bridge']);unrelated=package('unrelated',features=['GPU3D'])
    ci=['-I'+str(v)for v in(root,runtime.parent,runtime.parent/'include')]
    for opt in('O0','O2'):
        # Existing generated lexer/parser and legacy import helpers contain
        # unused declarations. Keep other warnings fatal in this frontend gate.
        exe=out/(opt+('.exe'if os.name=='nt'else''));command=[cc,'-std=c11','-'+opt,'-Wall','-Wextra','-Werror',
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',*ci,
            root/'test/cases/package_native_features_import.c',*(root/f for f in sources),runtime,'-o',exe]
        if os.name!='nt':command+=['-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
        run(opt+'-frontend-build',command)
        for item in(provider,bridge,consumer,unrelated):put(item)
        run(opt+'-transitive',[exe,entry,0,1])
        changed=copy.deepcopy(provider);changed['native']['features']=[];put(changed)
        run(opt+'-missing',[exe,entry,0,0],'requires native feature GPU3D')
        extra=out/'unrelated.xl';extra.write_text('import unrelated\nimport consumer\nfunc main(...) as int\n return 0\nend\n')
        run(opt+'-unrelated-import',[exe,extra,0,0],'requires native feature GPU3D')
        put(provider);changed=copy.deepcopy(bridge);changed['dependencies'].append(dependency('consumer'));put(changed)
        run(opt+'-cycle-with-provider',[exe,entry,0,1])
        put(bridge)
        dynamic=copy.deepcopy(provider);dynamic['native'].update(linkage='dynamic',variants=dict(static='selected.json'));put(dynamic)
        selected=copy.deepcopy(provider);selected['native'].update(linkage='static',features=[])
        path=out/'packages/provider/selected.json';path.write_text(json.dumps(selected)+'\n')
        run(opt+'-jit-selects-enabled',[exe,entry,0,1])
        run(opt+'-aot-selects-disabled',[exe,entry,3,0],'requires native feature GPU3D')
        selected['native']['features']=['GPU3D'];path.write_text(json.dumps(selected)+'\n')
        run(opt+'-aot-selects-enabled',[exe,entry,3,1])
        empty=package('consumer');put(empty);run(opt+'-legacy-no-requirements',[exe,entry,0,1])
    report.update(complete=True,inputsUnchanged=all(sha(k)==v for k,v in inputs.items()));report['passed']=report['inputsUnchanged'];save();assert report['passed']


if __name__=='__main__':main()
