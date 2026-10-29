"""Offline same-source owned ready-queue callback proof, not shipping acceptance.

The fixture source closure comes from a verbatim metadata-only slice of the
actual current component registry. It is explicitly not a
public language artifact plan or a minimal-pruning proof. Native implementation
and all private consumers are rebuilt together; no old provider DLL is linked.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',choices=('address,undefined','thread'))
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists(),'Preserve previous evidence'
    compiler=shutil.which('gcc');assert compiler;out.mkdir(parents=True)
    report={'Complete':False,'Scope':__doc__,'Rows':[],'NetworkTests':False,'NetworkSecurityTests':False}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,timeout=180):
        started=time.time();result=subprocess.run(list(map(str,command)),cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
        (out/(name+'.log')).write_bytes(result.stdout)
        report['Rows'].append({'Name':name,'Exit':result.returncode,'Command':list(map(str,command)),'Seconds':time.time()-started});save()
        assert result.returncode==0,(name,result.stdout.decode(errors='replace')[-1800:])
        return result.stdout
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
           '-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
           '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE','-pthread']
    inputs={Path(__file__).resolve(),Path(compiler).resolve()}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        inputs.update(path for path in (root/folder).rglob('*') if path.is_file())
    cases=('xlir_fixture_runtime_closure.c','xlir_future_callback_queue_native.c')
    inputs.update(root/'test/cases'/name for name in cases)
    def seal():return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs)}
    before=seal();report['Inputs']=before;save()
    try:
        catalog=out/('catalog'+('.exe' if os.name=='nt' else ''))
        registry=(root/'src/compile/xl6_compile_unit.c').read_bytes().decode('utf-8')
        begin='#define R(Name)';end='const XL6_ArtifactNativeSymbol* xl6ArtifactNativeSymbolLookup'
        assert registry.count(begin)==registry.count(end)==1
        section=registry[registry.index(begin):registry.index(end)]
        assert section.count('xl6ArtifactRuntimeComponentGet(unsigned id)')==1
        assert section.count('xl6ArtifactRuntimeSymbolOwner(const char* symbol)')==1
        metadata=out/'catalog-source.c'
        metadata.write_text('#include "src/compile/xl6_artifact_runtime.h"\n#include <string.h>\n'+section,encoding='utf-8')
        report['RegistrySliceSha256']=hashlib.sha256(section.encode('utf-8')).hexdigest()
        run('catalog.build',[compiler,*flags,'-O0',root/'test/cases'/cases[0],metadata,'-o',catalog])
        roots=('xl6FutureCallbackRuntimeV5Get',)
        plan=json.loads(run('catalog.run',[catalog,*roots]));report['Closure']=plan
        sources=plan['Sources'];assert len(sources)==len(set(sources))
        assert 'src/runtime/xl6_future_callback_queue.c' in sources
        modules=set(plan['NativeModules'])|{'XRT_MODULE_MEMORY_DEBUG'}
        assert all(re.fullmatch(r'(XRT|XRUNTIME)_MODULE_[A-Z0-9_]+',name) for name in modules)
        assert not ({'XRT_MODULE_NET','XRT_MODULE_TLS'}&modules)
        native=out/'native.c'
        native.write_text(''.join('#define '+name+'\n' for name in sorted(modules))+
            '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
        report['GeneratedNativeSha256']=hashlib.sha256(native.read_bytes()).hexdigest()
        if args.sanitize:
            flags+=['-g','-fno-omit-frame-pointer','-fsanitize='+args.sanitize]
            if args.sanitize=='thread':flags+=['-Wno-error=tsan']
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
        for level in ('0','2'):
            objects=[]
            for path in [native,*[root/name for name in sources]]:
                label='o'+level+'_'+path.stem;obj=out/(label+'.o')
                run(label,[compiler,*flags,'-O'+level,'-c',path,'-o',obj]);objects.append(obj)
            binary=out/('callback-o'+level+('.exe' if os.name=='nt' else ''))
            run('o'+level+'.link',[compiler,*flags,'-O'+level,root/'test/cases'/cases[1],*objects,*libraries,'-o',binary])
            run('o'+level+'.run',[binary])
        report['InputsUnchanged']=seal()==before;assert report['InputsUnchanged'];report['Complete']=True
    except Exception as error:report['Error']=repr(error);raise
    finally:save()
    print('Queued callback source/ownership/lifetime proof passed:',len(report['Rows']),'phases',flush=True)


if __name__=='__main__':main()
