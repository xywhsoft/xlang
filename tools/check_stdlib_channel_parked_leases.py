"""Offline same-source proof of private asynchronous Channel lease primitives.

--plan is a JSON output of the actual artifact runtime closure planner (as
produced by the public ChannelCase acceptance). This is a standalone native
source-closure proof, NOT a shipping-DLL, language Future, or full-goal claim.
All native implementation and private frame consumers are rebuilt together;
never mix new frame headers with an older resident provider DLL.
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
    args=parser.parse_args()
    root,out,planfile=args.root.resolve(),args.out.resolve(),args.plan.resolve()
    assert not out.exists(),'Preserve prior evidence'
    compiler=shutil.which('gcc');assert compiler
    plan=json.loads(planfile.read_text(encoding='utf-8'))
    sources=list(plan['Sources'])
    # The legacy native fixture explicitly creates a C float32 scalar schema;
    # it is not an extra public select dependency inferred by the planner.
    scalar='src/runtime/xl6_array_c_scalar_admission.c'
    if scalar not in sources:sources.append(scalar)
    registry=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf-8')
    known=set(re.findall(r'\{"(src/[^"\n]+\.c)"\s*,',registry))
    assert len(set(sources))==len(sources)
    required={'src/runtime/xl6_channel.c','src/runtime/xl6_channel_case.c','src/runtime/xl6_channel_select.c'}
    assert required<=set(sources)
    for name in sources:
        assert name in known and (root/name).resolve().is_relative_to(root/'src'),name
    modules=set(plan['NativeModules'])|{'XRT_MODULE_CHANNEL_SUBSCRIBE','XRT_MODULE_MEMORY_DEBUG'}
    assert all(re.fullmatch(r'(XRT|XRUNTIME)_MODULE_[A-Z0-9_]+',name) for name in modules)
    assert not ({'XRT_MODULE_NET','XRT_MODULE_TLS'}&modules)
    out.mkdir(parents=True)
    native=out/'native.c'
    native.write_text(''.join('#define '+name+'\n' for name in sorted(modules))+
        '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"\n',encoding='utf-8')
    cases=('stdlib_channel_parked_leases.c','stdlib_channel_cases_native.c','stdlib_channel_cases_pruned.c')
    files={Path(__file__).resolve(),Path(compiler).resolve(),planfile,native}
    for folder in ('src','lib/xrt','lib/xruntime','release/include','test/support'):
        files.update(path for path in (root/folder).rglob('*') if path.is_file())
    files.update(root/'test/cases'/name for name in cases)
    def seal():return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(files)}
    inputs=seal()
    report={'complete':False,'inputs':inputs,'phases':[],'NetworkTests':False,'NetworkSecurityTests':False,
            'scope':__doc__,'sources':sources,'nativeModules':sorted(modules)}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command,timeout=120):
        started=time.time()
        result=subprocess.run(list(map(str,command)),cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=timeout)
        (out/(name+'.log')).write_bytes(result.stdout)
        report['phases'].append({'name':name,'command':list(map(str,command)),'exit':result.returncode,'seconds':time.time()-started})
        save();assert result.returncode==0,(name,result.stdout.decode(errors='replace')[-1500:])
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
            objects=[]
            for name in [native,*[root/name for name in sources]]:
                label='o'+level+'_'+name.stem;obj=out/(label+'.o')
                run(label,[compiler,*flags,'-O'+level,'-c',name,'-o',obj]);objects.append(obj)
            for name in cases:
                label='o'+level+'_'+Path(name).stem;binary=out/(label+('.exe' if os.name=='nt' else ''))
                extra=[root/'test/support/module_product_memory.c'] if name=='stdlib_channel_cases_native.c' else []
                defines=['-DTEST_KIND=3'] if name=='stdlib_channel_cases_pruned.c' else []
                run(label+'.link',[compiler,*flags,'-O'+level,*defines,root/'test/cases'/name,*extra,*objects,*libraries,'-o',binary])
                run(label+'.run',[binary])
        report['inputsUnchanged']=seal()==inputs
        assert report['inputsUnchanged']
        report['complete']=True
    except Exception as error:
        report['Error']=repr(error);raise
    finally:save()
    print('Private parked Channel leases passed:',len(report['phases']),'phases; no language Future claim',flush=True)


if __name__=='__main__':main()
