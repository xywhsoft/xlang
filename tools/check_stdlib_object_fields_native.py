"""Standalone resident object-field closure: O0/O2 and full-closure Linux sanitizers.
No released compiler/library, generated stand-ins or object overrides. Generic
index adapters are covered separately by the public source gate; this lane
isolates the actual shared object-field provider, CodeLease and selected XRT.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args(); root=Path(__file__).resolve().parents[1]; out=args.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists()
    sources=['test/cases/stdlib_object_fields_native_host.c','test/support/module_product_memory.c',
        'src/runtime/xl6_value_object_fields.c','src/runtime/xl6_dynamic_fields.c',
        'src/runtime/xl6_native_handle.c','src/runtime/xl6_code_finalizer.c',
        'src/runtime/xl6_code_lease.c','src/api/xl_foreign_error.c']
    files={Path(__file__),Path(cc),Path(sys.executable)}
    files.update(root/name for name in sources)
    for directory in ('src/runtime','src/api','include','release/include','lib/xrt','lib/xruntime'):
        files.update(p for p in (root/directory).rglob('*') if p.suffix in ('.h','.inc'))
    files.add(root/'test/support/module_product_memory.h')
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(files)]
    out.mkdir(parents=True); runtime=out/'runtime.c'
    runtime.write_text('#define XRT_MODULE_MEMORY_DEBUG\n#define XRT_MODULE_VALUE_GRAPH\n'
        '#define XRT_MODULE_ATOMIC\n#define XRT_MODULE_THREAD\n#define XRT_MODULE_ONCE\n'
        '#define XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD\n#define XRUNTIME_IMPLEMENTATION\n'
        '#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
    rows=[]; products=[]; env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
        UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,
        Scope=__doc__,IndexAdapters=False,ShippingArtifact=False,Full53Acceptance=False,Sanitized=False)
    levels=[('O0',['-O0']),('O2',['-O2'])]
    if os.name!='nt': levels.append(('san',['-O1','-g','-fsanitize=address,undefined',
        '-fno-omit-frame-pointer','-fno-pie','-no-pie']))
    libs=[] if os.name=='nt' else ['-pthread','-ldl','-lm']
    def run(name,command):
        result=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=240)
        text=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(text)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode))
        print(name+': '+str(result.returncode),flush=True)
        assert result.returncode==0,text[-6000:].decode(errors='replace')
        return text
    try:
        for name,flags in levels:
            binary=out/(name+('.exe' if os.name=='nt' else '.host'))
            run(name+'.compile',[cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,
                '-DXL6_OBJECT_FIELDS_NO_INDEX','-I.','-Irelease','-Irelease/include',
                *sources,runtime,*libs,'-o',binary])
            result=run(name+'.run',[binary])
            assert b'checked object-field SetTake OOM prefixes balanced' in result and b'forged/reentry/first-error balanced' in result
            products.append(dict(Path=str(binary),Sha256=sha(binary)))
        products.append(dict(Path=str(runtime),Sha256=sha(runtime)))
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        report['Sanitized']=os.name!='nt'
        report['Complete']=report['InputsUnchanged']
    except Exception as error:
        report['Failure']=repr(error); print(report['Failure'],file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0 if report['Complete'] else 1
if __name__=='__main__': raise SystemExit(main())
