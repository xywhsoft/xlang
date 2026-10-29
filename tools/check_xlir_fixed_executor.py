"""Offline same-source fixed-worker/ready-queue contract (not shipping acceptance).

The actual artifact planner's --plan supplies the source closure. The native
implementation is rebuilt from the matching pinned header; no provider DLL or
old executor object is mixed with the current private ticket/frame layout.
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
    for name in ('root','out','plan'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',choices=('address,undefined','thread'))
    args=parser.parse_args();root,out,planfile=args.root.resolve(),args.out.resolve(),args.plan.resolve()
    assert not out.exists(),'Preserve previous evidence'
    compiler=shutil.which('gcc');assert compiler
    plan=json.loads(planfile.read_text(encoding='utf-8'));sources=list(plan['Sources'])
    # Explicit fixture dependencies over the public Channel planner closure.
    # This proves the shared executor source, not a minimal async artifact.
    for name in ('src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
                 'src/runtime/xl6_continuation_future.c'):
        if name not in sources:sources.append(name)
    registry=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf-8')
    known=set(re.findall(r'\{"(src/[^"\n]+\.c)"\s*,',registry))
    assert len(sources)==len(set(sources))
    assert 'src/runtime/xl6_continuation_executor.c' in sources
    assert all(name in known and (root/name).resolve().is_relative_to(root/'src') for name in sources)
    modules=set(plan['NativeModules'])|{'XRT_MODULE_MEMORY_DEBUG'}
    assert all(re.fullmatch(r'(XRT|XRUNTIME)_MODULE_[A-Z0-9_]+',name) for name in modules)
    assert not ({'XRT_MODULE_NET','XRT_MODULE_TLS'}&modules)
    out.mkdir(parents=True);native=out/'native.c'
    native.write_text(''.join('#define '+name+'\n' for name in sorted(modules))+
                      '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
    cases=('xlir_continuation_fixed_contract','xlir_continuation_executor_contract',
           'xlir_continuation_progress_contract','xlir_continuation_handoff_contract',
           'xlir_executor_ownership_contract')
    inputs={Path(__file__).resolve(),Path(compiler).resolve(),planfile,native}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        inputs.update(path for path in (root/folder).rglob('*') if path.is_file())
    inputs.update(root/'test/cases'/(name+'.c') for name in cases)
    def seal():return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs)}
    before=seal();report={'Complete':False,'NetworkTests':False,'NetworkSecurityTests':False,'Inputs':before,'Rows':[],'Scope':__doc__}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        started=time.time();result=subprocess.run(list(map(str,command)),cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
        (out/(name+'.log')).write_bytes(result.stdout)
        report['Rows'].append({'Name':name,'Exit':result.returncode,'Command':list(map(str,command)),'Seconds':time.time()-started});save()
        assert result.returncode==0,(name,result.stdout.decode(errors='replace')[-1800:])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
           '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE','-pthread']
    if args.sanitize:
        flags+=['-g','-fno-omit-frame-pointer','-fsanitize='+args.sanitize]
        if args.sanitize=='thread':flags+=['-Wno-error=tsan']
    libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
    save()
    try:
        for level in ('0','2'):
            objects=[];executor=None;transport=None
            for path in [native,*[root/name for name in sources]]:
                label='o'+level+'_'+path.stem;obj=out/(label+'.o')
                run(label,[compiler,*flags,'-O'+level,'-c',path,'-o',obj]);objects.append(obj)
                if path.name=='xl6_continuation_executor.c':executor=obj
                if path.name=='xl6_continuation_future.c':transport=obj
            assert executor and transport
            hook=out/('o'+level+'_hooked.o')
            run(hook.stem,[compiler,*flags,'-O'+level,'-DxrtThreadCreate=fixed_thread_create','-c',root/'src/runtime/xl6_continuation_executor.c','-o',hook])
            watch=out/('o'+level+'_watch.o')
            run(watch.stem,[compiler,*flags,'-O'+level,'-DxrtFutureWatchAdd=handoff_watch_add','-c',root/'src/runtime/xl6_continuation_future.c','-o',watch])
            for name in cases:
                selected=[hook if obj==executor else obj for obj in objects] if name==cases[0] else objects
                if name=='xlir_continuation_handoff_contract':selected=[watch if obj==transport else obj for obj in objects]
                label='o'+level+'_'+name;binary=out/(label+('.exe' if os.name=='nt' else ''))
                run(label+'.link',[compiler,*flags,'-O'+level,root/'test/cases'/(name+'.c'),*selected,*libraries,'-o',binary])
                run(label+'.run',[binary])
        report['InputsUnchanged']=seal()==before;assert report['InputsUnchanged'];report['Complete']=True
    except Exception as error:report['Error']=repr(error);raise
    finally:save()
    print('Fixed executor and existing lifecycle contracts passed:',len(report['Rows']),'phases',flush=True)


if __name__=='__main__':main()
