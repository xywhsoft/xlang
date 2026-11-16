"""Current private XLP2 wire and real source/compiled import frontend at O0/O2.

Full compiler API registration, TCC/VFS, generated producer interfaces and context
reuse remain separate gates. No installed runtime/package directory changes.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','runtime','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,runtime,out=[v.resolve()for v in(a.root,a.runtime,a.out)]
    out.mkdir(parents=True,exist_ok=False);cc=Path(shutil.which('gcc')).resolve();sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    sources=['src/compile/xl6_imports.c','src/compile/xl6_source_overlay.c','src/compile/xl6_source_report.c','src/compile/xl6_build_inputs.c',
        'src/sem/xl6_completion.c','src/package/xl6_package.c','src/abi/xl6_abi_json.c','src/abi/xl6_callable.c','src/type/xl6_type_model.c',
        'src/sem/xl6_type_registry.c','src/common/xl6_console.c','src/common/xl6_output.c','src/parser/xl6_parser_api.c',
        'src/parser/xl6_parser.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c','src/lexer/xl6_token.c','src/ast/xl6_ast.c','src/diag/xl6_diag.c']
    files=[Path(__file__).resolve(),cc,runtime,root/'test/cases/package_compiled_native_features.c',*(root/s for s in sources)]
    files += [f for folder in(root/'src',runtime.parent/'include')for f in folder.rglob('*')if f.is_file()and f.suffix in('.h','.inc')]
    inputs={str(f):sha(f)for f in files};report=dict(complete=False,passed=False,shippingDelivery=False,fullCompilerQualification=False,scope=__doc__,inputs=inputs,rows=[])
    def save():
        p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n');p.replace(out/'results.json')
    def run(name,command,diagnostic=None):
        command=list(map(str,command));env=os.environ.copy();env['XLANG_PACKAGE_PATH']=str(out/'packages')
        key='PATH'if os.name=='nt'else'LD_LIBRARY_PATH';env[key]=str(runtime.parent)+os.pathsep+env.get(key,'')
        started=time.monotonic();z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-started,logSha256=sha(log)));save();print(name,z.returncode,flush=True)
        assert not z.returncode,log
        if diagnostic:assert diagnostic.encode()in log.read_bytes(),log
    def manifest(version='1'):
        folder=out/'packages/consumer';folder.mkdir(parents=True,exist_ok=True)
        (folder/'api.xl').write_text('export const int value=1\n')
        data=dict(format='xlang-package-1',name='consumer',version='1',abiVersion='1',artifact=dict(xlSources=['api.xl']),
            requiresNativeFeatures=['GPU3D'],dependencies=[dict(name='bridge',version=version,abiVersion='1')])
        (folder/'xlang.package.json').write_text(json.dumps(data)+'\n')
    entry=out/'main.xl';entry.write_text('import consumer\nfunc main(...) as int\n return 0\nend\n')
    unrelated=out/'unrelated.xl';unrelated.write_text('import provider\nimport consumer\nfunc main(...) as int\n return 0\nend\n')
    legacy=out/'legacy.xl';legacy.write_text('import provider\nfunc main(...) as int\n return 0\nend\n')
    for opt in('O0','O2'):
        exe=out/(opt+('.exe'if os.name=='nt'else''));flags=['-std=c11','-'+opt,'-Wall','-Wextra','-Werror',
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',*['-I'+str(d)for d in(root,runtime.parent,runtime.parent/'include')]]
        command=[cc,*flags,root/'test/cases/package_compiled_native_features.c',*(root/s for s in sources),runtime,'-o',exe]
        if os.name!='nt':command+=['-Wl,-rpath,'+str(runtime.parent),'-ldl','-lm','-pthread']
        run(opt+'-build',command);run(opt+'-wire-ownership',[exe])
        # The existing static artifact catalog intentionally zero-initializes
        # trailing fields by omission; keep other writer diagnostics fatal.
        run(opt+'-writer-syntax',[cc,*flags,'-Wno-missing-field-initializers','-fsyntax-only',root/'src/compile/xl6_compile_unit.c'])
        for mode,expected in [('transitive',1),('missing',0),('legacy-provider',0),('bridge-source-import-only',0),('consumer-source-import-only',0)]:
            run(opt+'-compiled-'+mode,[exe,entry,0,expected,mode,'compiled'],None if expected else'requires native feature GPU3D')
        run(opt+'-unrelated-import',[exe,unrelated,0,0,'consumer-source-import-only','compiled'],'requires native feature GPU3D')
        run(opt+'-legacy-no-claim',[exe,legacy,0,1,'legacy-provider','compiled'])
        manifest()
        for selection in(0,3):
            run(opt+'-mixed-source-'+str(selection),[exe,entry,selection,1,'transitive','source-consumer'])
        run(opt+'-mixed-missing',[exe,entry,0,0,'missing','source-consumer'],'requires native feature GPU3D')
        manifest('2');run(opt+'-mixed-version-mismatch',[exe,entry,0,0,'transitive','source-consumer'],'registered compiled dependency version mismatch')
    report.update(complete=True,inputsUnchanged=all(sha(f)==h for f,h in inputs.items()));report['passed']=report['inputsUnchanged'];save();assert report['passed']


if __name__=='__main__':main()
