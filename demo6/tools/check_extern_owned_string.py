"""Verify @owned through the public compiler, native code, JIT and AOT.

Use --driver for a Windows public source driver or --cli for a normal xl CLI.
Each run owns a fresh evidence directory; compiled artifacts are never reused.
"""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--directory',type=Path,required=True)
    compiler=p.add_mutually_exclusive_group(required=True)
    compiler.add_argument('--driver',type=Path)
    compiler.add_argument('--cli',type=Path)
    args=p.parse_args();work=args.directory.resolve();work.mkdir(parents=True,exist_ok=False)
    binary=(args.driver or args.cli).resolve();checks=[]
    env=os.environ.copy();env['XLANG_PACKAGE_PATH']=os.pathsep.join([str(ROOT/'test/fixtures'),str(ROOT/'release/extlibs')])
    def run(name,argv,expected=True,diagnostic=None):
        if args.cli and argv[0]==binary:
            argv=[*argv,'--inc-path',ROOT/'release/include']
        result=subprocess.run([str(s) for s in argv],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
        log=result.stdout.decode('utf-8',errors='replace');(work/(name+'.log')).write_text(log,encoding='utf-8')
        passed=(result.returncode==0)==expected and (diagnostic is None or diagnostic in log)
        checks.append({'name':name,'passed':passed,'exitCode':result.returncode})
        if not passed:raise RuntimeError(name+'\n'+log[-5000:])
        print(name+': PASS',flush=True)
    cases=[
        ('extern_owned_string','test/cases/extern_owned_string.xl',['test/cases/extern_owned_string_host.c'],['owned_text','owned_empty','owned_null','owned_optional']),
        ('extern_owned_string_package','test/cases/extern_owned_string_package.xl',[],['owned_package_text','owned_package_optional']),
        ('extern_owned_string_export','test/cases/extern_owned_string_export.xl',['test/cases/extern_owned_string_host.c'],['owned_text']),
        ('extern_owned_string_forward','test/cases/extern_owned_string_forward.xl',['test/cases/extern_owned_string_host.c'],['owned_optional']),
        ('example','release/examples/extern_owned_string.xl',['release/examples/extern_owned_string_native.c'],['example_owned_text']),
        ('cstring-regression','test/cases/xlir_c_owned_basic.xl',['test/cases/xlir_c_owned_native.c'],[]),
        ('cstring-package-regression','test/cases/xlir_c_owned_import_exports.xl',[],[]),
        ('ordinary-string-export-regression','test/cases/sem_export_string_return.xl',[],[]),
    ]
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs=[ROOT/source for _,source,_,_ in cases]+[ROOT/path for _,_,paths,_ in cases for path in paths]
    inputs+=[Path(__file__),*sorted((ROOT/'test/fixtures/extern_owned_string').rglob('*'))]
    inputs+=[*sorted((ROOT/'test/cases').glob('bad_extern_owned_*.xl')),
        ROOT/'test/cases/sem_c_callback_string_return_invalid.xl']
    snapshot={str(f.relative_to(ROOT)):sha(f) for f in inputs if f.is_file()}
    for case,relative,sidecars,symbols in cases:
        source=ROOT/relative;prefix=work/case
        run(case+'-jit',[binary,'run',source,*sidecars] if args.cli else [binary,source,'run',str(prefix)+'-jit',*sidecars])
        run(case+'-aot-build',[binary,'build',source,*sidecars,'-o',prefix] if args.cli else [binary,source,'exe',prefix,*sidecars])
        run(case+'-aot',[prefix.with_suffix('.exe' if os.name=='nt' else '')])
        emit=Path(str(prefix)+'-emit')
        run(case+'-emit',[binary,'cgen',source,*sidecars,'--emit',str(emit)+'.c','--emit-header',str(emit)+'.h']
            if args.cli else [binary,source,'emit',emit,*sidecars])
        generated=Path(str(prefix)+'-emit.c').read_text(encoding='utf-8')
        for symbol in symbols:
            assert 'extern char* '+symbol+'(' in generated,symbol+' lost its exact C prototype'
            assert 'xlir_string_from_cstr('+symbol+'(' not in generated,symbol+' was copied'
            assert '.Data = '+symbol+'(' in generated,symbol+' was not adopted at its call site'
        if symbols:assert 'xl6CStringTakeV1' not in generated and 'XL6_COwnerV1' not in generated
        if case=='extern_owned_string':assert 'xlir_string_from_cstr(owned_borrowed(' in generated,'borrowed default changed'
        if case=='extern_owned_string_export':
            # Compile and run a C consumer against the actual emitted
            # public header, rather than guessing the export ABI.
            consumer=work/'export-consumer.c'
            consumer.write_text('#include "'+Path(str(emit)+'.h').as_posix()+'"\n'+
                'extern bool owned_original(const char*);\n'
                'bool owned_export_consumer(void) {\n'
                ' xl6_foreign_error_scope scope={0}; if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' xlir_string value=owned_export_text(); xl6_foreign_error_result outcome=xl6ForeignCallEnd(&scope);\n'
                ' bool ok=outcome.ok && value.Data && value.Size==10 && owned_original(value.Data);\n'
                ' xrtFree((void*)value.Data); xrtErrorFree(outcome.error);\n'
                ' if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' value=owned_export_optional(); outcome=xl6ForeignCallEnd(&scope);\n'
                ' ok=ok && outcome.ok && !value.Data && !value.Size; xrtErrorFree(outcome.error);\n'
                ' if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' value=owned_export_failed(); outcome=xl6ForeignCallEnd(&scope);\n'
                ' ok=ok && !outcome.ok && outcome.error && !value.Data && !value.Size; xrtErrorFree(outcome.error);\n'
                ' if(!xl6ForeignCallBegin(&scope)) return false;\n'
                ' value=owned_export_failed_without_diagnostic(); outcome=xl6ForeignCallEnd(&scope);\n'
                ' ok=ok && !outcome.ok && !value.Data && !value.Size; xrtErrorFree(outcome.error); return ok; }\n',encoding='utf-8')
            entry=work/'export-entry.xl'
            program=source.read_text(encoding='utf-8')
            assert '    return 0\n' in program
            entry.write_text(program.replace('    return 0\n','    assert consumer()\n    return 0\n',1)+
                '\nextern "owned_export_consumer" @no_raise func consumer() as bool\n',encoding='utf-8')
            extra=[*sidecars,consumer]
            run('export-C-consumer-jit',[binary,'run',entry,*extra] if args.cli else [binary,entry,'run',work/'consumer-jit',*extra])
            run('export-C-consumer-aot-build',[binary,'build',entry,*extra,'-o',work/'consumer-aot']
                if args.cli else [binary,entry,'exe',work/'consumer-aot',*extra])
            run('export-C-consumer-aot',[(work/'consumer-aot').with_suffix('.exe' if os.name=='nt' else '')])
    negatives={
        'test/cases/bad_extern_owned_integer.xl':'@owned requires',
        'test/cases/bad_extern_owned_product.xl':'@owned requires',
        'test/cases/bad_extern_owned_constructor.xl':'@owned requires',
        'test/cases/bad_extern_owned_no_raise.xl':'@owned @no_raise requires',
        'test/cases/bad_extern_owned_conflict.xl':'incompatible ABI',
        'test/cases/bad_extern_owned_member_conflict.xl':'conflicting string ownership',
        'test/cases/bad_extern_owned_registered.xl':'bridge',
        'test/cases/bad_extern_owned_unknown.xl':'unknown extern effect contract',
    }
    for relative,message in negatives.items():
        source=ROOT/relative;name=source.stem.removeprefix('bad_extern_owned_')
        run('reject-'+name,[binary,'cgen',source,'--emit',work/('reject-'+name+'.c')] if args.cli else [binary,source,'emit',work/('reject-'+name)],False,message)
    run('reject-raw-C-callback-string',[binary,'check',ROOT/'test/cases/sem_c_callback_string_return_invalid.xl']
        if args.cli else [binary,ROOT/'test/cases/sem_c_callback_string_return_invalid.xl','emit',work/'reject-callback'],False)
    assert all(sha(ROOT/f)==digest for f,digest in snapshot.items()),'Test inputs changed'
    (work/'result.json').write_text(json.dumps({'complete':True,'platform':sys.platform,'compiler':str(binary),
        'compilerSha256':sha(binary),'checks':checks,'inputs':snapshot},indent=2),encoding='utf-8')
    print('extern owned string checks complete',flush=True)

if __name__=='__main__':main()
