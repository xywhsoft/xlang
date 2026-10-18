"""Offline same-source generic deadline proof, not public Channel deadline acceptance.
Rebuilds selected resident/native sources; no Net/TLS source or per-wait worker.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sanitize',choices=('address,undefined','thread'))
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve();cc=shutil.which('gcc')
    assert cc and not out.exists();out.mkdir(parents=True)
    inputs={Path(__file__).resolve(),Path(cc).resolve(),root/'test/cases/stdlib_deadline_service_native.c',
        root/'test/cases/xlir_fixture_runtime_closure.c'}
    for name in ('src','lib/xrt','lib/xruntime','release/include'):
        inputs.update(path for path in (root/name).rglob('*') if path.is_file())
    def seal():return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs)}
    before=seal();report={'Scope':__doc__,'Complete':False,'Inputs':before,'Rows':[],
        'NetworkTests':False,'NetworkSecurityTests':False,'PublicChannelDeadlinesAccepted':False}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        result=subprocess.run(list(map(str,command)),cwd=root,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['Rows'].append({'Name':name,'Exit':result.returncode,'Command':list(map(str,command)),
            'Sha256':hashlib.sha256(log.read_bytes()).hexdigest()});save()
        assert result.returncode==0,(name,log.read_bytes()[-3000:]);return result.stdout
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE','-pthread']
    try:
        registry=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf8')
        begin='#define R(Name)';end='const XL6_ArtifactNativeSymbol* xl6ArtifactNativeSymbolLookup'
        assert registry.count(begin)==registry.count(end)==1
        source=registry[registry.index(begin):registry.index(end)]
        metadata=out/'catalog.c';metadata.write_text('#include "src/compile/xl6_artifact_runtime.h"\n#include <string.h>\n'+source,encoding='utf8')
        observer=out/('catalog.exe' if os.name=='nt' else 'catalog')
        run('catalog.build',[cc,*flags,root/'test/cases/xlir_fixture_runtime_closure.c',metadata,'-o',observer])
        plan=json.loads(run('catalog.run',[observer,'xl6DeadlineServiceCreateV1','xl6OwnershipDomainDeadlineServiceV1Get']))
        report['Closure']=plan;assert 'src/runtime/xl6_deadline_service.c' in plan['Sources']
        assert not any(name.startswith(('XRT_MODULE_NET','XRT_MODULE_TLS','XRT_MODULE_COROUTINE','XRT_MODULE_TASK')) for name in plan['NativeModules'])
        modules=sorted(set(plan['NativeModules'])|{'XRT_MODULE_MEMORY_DEBUG'})
        assert all(re.fullmatch(r'(?:XRT|XRUNTIME)_MODULE_[A-Z0-9_]+',name) for name in modules)
        native=out/'native.c';native.write_text(''.join('#define '+name+'\n' for name in modules)+
            '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8')
        if args.sanitize:flags+=['-g','-fno-omit-frame-pointer','-fsanitize='+args.sanitize]
        if args.sanitize=='thread':flags+=['-Wno-error=tsan']
        links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
        for level in ('0','2'):
            objects=[]
            for path in (native,*[root/name for name in plan['Sources']]):
                name='o'+level+'_'+path.stem;obj=out/(name+'.o')
                run(name,[cc,*flags,'-O'+level,'-c',path,'-o',obj]);objects.append(obj)
            binary=out/('deadline-o'+level+('.exe' if os.name=='nt' else ''))
            run('o'+level+'.link',[cc,*flags,'-O'+level,'-DXL6_DEADLINE_SOURCE_FAULT_PROBE',root/'test/cases/stdlib_deadline_service_native.c',*objects,
                '-Wl,--wrap=xrtCondWaitUntil','-Wl,--wrap=xrtMutexCreate','-Wl,--wrap=xrtCondCreate',
                *links,'-o',binary]);run('o'+level+'.run',[binary])
        report['InputsUnchanged']=seal()==before;assert report['InputsUnchanged']
        report['LogsUnchanged']=all(hashlib.sha256((out/(row['Name']+'.log')).read_bytes()).hexdigest()==row['Sha256'] for row in report['Rows'])
        assert report['LogsUnchanged'];report['Complete']=True
    except Exception as error:report['Error']=repr(error);raise
    finally:save()
    print('Generic native deadline source passed:',len(report['Rows']),'phases; public finite Channel API remains separate',flush=True)
if __name__=='__main__':main()
