"""Same-source Channel Future provider proof; not language/shipping acceptance.

Actual registry metadata is extracted verbatim only to close this native fixture.
All native implementation, catalog and private consumers are rebuilt together.
No external provider, per-wait worker, stack, network or security test is used.
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
    parser.add_argument('--direct',action='store_true',help='Direct single Channel provider, no public Case or select adapter')
    parser.add_argument('--deadline',action='store_true',help='Finite provider using the shared non-network deadline source')
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists();out.mkdir(parents=True);cc=shutil.which('gcc');assert cc
    report={'Complete':False,'Scope':__doc__,'Rows':[],'NetworkTests':False,'NetworkSecurityTests':False}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,timeout=180):
        started=time.monotonic();result=subprocess.run(list(map(str,command)),cwd=root,capture_output=True,timeout=timeout)
        (out/(name+'.log')).write_bytes(result.stdout+result.stderr)
        report['Rows'].append({'Name':name,'Exit':result.returncode,'Command':list(map(str,command)),
            'Sha256':hashlib.sha256((out/(name+'.log')).read_bytes()).hexdigest(),'Seconds':time.monotonic()-started});save()
        assert result.returncode==0,(name,(result.stdout+result.stderr).decode(errors='replace')[-2000:])
        return result.stdout
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-I'+str(root),'-I'+str(root/'release'),'-I'+str(root/'release/include')]
    if os.name!='nt':flags+=['-D_GNU_SOURCE','-pthread']
    fixture='stdlib_channel_deadline_native.c' if args.deadline else 'stdlib_channel_direct_future_native.c' if args.direct else 'stdlib_channel_future_native.c'
    inputs={Path(__file__).resolve(),Path(cc).resolve(),root/'test/cases/xlir_fixture_runtime_closure.c',root/'test/cases'/fixture}
    for folder in ('src','lib/xrt','lib/xruntime','release/include'):
        inputs.update(path for path in (root/folder).rglob('*') if path.is_file())
    def seal():return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs)}
    before=seal();report['Inputs']=before;save()
    try:
        registry=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf8')
        begin='#define R(Name)';end='const XL6_ArtifactNativeSymbol* xl6ArtifactNativeSymbolLookup'
        assert registry.count(begin)==registry.count(end)==1
        section=registry[registry.index(begin):registry.index(end)]
        metadata=out/'catalog-source.c';metadata.write_text('#include "src/compile/xl6_artifact_runtime.h"\n#include <string.h>\n'+section,encoding='utf8')
        report['RegistrySliceSha256']=hashlib.sha256(section.encode()).hexdigest()
        catalog=out/('catalog.exe' if os.name=='nt' else 'catalog')
        run('catalog.build',[cc,*flags,'-O0',root/'test/cases/xlir_fixture_runtime_closure.c',metadata,'-o',catalog])
        # The fixture constructs its own scalar schema. This extra root is NOT
        # an inferred requirement of the producer or a public pruning claim.
        report['FixtureRoots']=['xl6SequencePrimitiveCreateV1','xl6SequenceReferenceCreateV1']
        if args.direct or args.deadline:report['FixtureRoots'].append('xl6StringTypeV1')
        provider=('xl6ChannelDirectDeadlineRuntimeV1Get' if args.direct else 'xl6ChannelSelectDeadlineRuntimeV1Get') if args.deadline else (
            'xl6ChannelDirectFutureRuntimeV1Get' if args.direct else 'xl6ChannelFutureRuntimeV1Get')
        domain='xl6OwnershipDomainChannelDirectFutureV1Get' if args.direct else 'xl6OwnershipDomainChannelFutureV1Get'
        plan=json.loads(run('catalog.run',[catalog,provider,domain,
            *(['xl6OwnershipDomainDeadlineServiceV1Get'] if args.deadline else []),
            *report['FixtureRoots']]))
        report['Closure']=plan;sources=plan['Sources']
        assert len(sources)==len(set(sources)) and 'src/runtime/xl6_channel_future_core.c' in sources
        assert ('src/runtime/xl6_channel_direct_future.c' if args.direct else 'src/runtime/xl6_channel_future.c') in sources
        if args.direct:
            assert not {'src/runtime/xl6_channel_case.c','src/runtime/xl6_channel_select.c',
                'src/runtime/xl6_channel_future.c','src/runtime/xl6_channel_case_array.c'}&set(sources)
        modules=set(plan['NativeModules'])|{'XRT_MODULE_MEMORY_DEBUG'}
        assert 'XRT_MODULE_CHANNEL_SUBSCRIBE' in modules
        assert all(re.fullmatch(r'(?:XRT|XRUNTIME)_MODULE_[A-Z0-9_]+',name) for name in modules)
        assert not any(name.startswith(('XRT_MODULE_NET','XRT_MODULE_TLS')) for name in modules)
        if args.deadline:
            assert 'src/runtime/xl6_channel_future_deadline.c' in sources
            assert not any(name.startswith(('XRT_MODULE_TASK','XRT_MODULE_COROUTINE')) for name in modules)
            flags+=['-DXL6_CHANNEL_DEADLINE_SOURCE_PROBE','-DTEST_SELECT='+str(int(not args.direct))]
        native=out/'native.c';native.write_text(''.join('#define '+name+'\n' for name in sorted(modules))+
            '#define XRUNTIME_IMPLEMENTATION\n#include "lib/xruntime/xruntime.h"\n',encoding='utf8')
        report['NativeSha256']=hashlib.sha256(native.read_bytes()).hexdigest()
        if args.sanitize:flags+=['-g','-fno-omit-frame-pointer','-fsanitize='+args.sanitize]
        if args.sanitize=='thread':flags+=['-Wno-error=tsan']
        libraries=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
        for level in ('0','2'):
            objects=[]
            for path in (native,*[root/name for name in sources]):
                lane='o'+level+'_'+path.stem;obj=out/(lane+'.o')
                run(lane,[cc,*flags,'-O'+level,'-c',path,'-o',obj]);objects.append(obj)
            binary=out/('future-o'+level+('.exe' if os.name=='nt' else ''))
            wraps=['-Wl,--wrap='+name for name in ('xrtCondCreate','xrtCondWaitUntil','xrtThreadCreate',
                'xrtChannelSubscribeStart','xrtChannelSubscribeStop','xl6ChannelFutureCoreStopV1',
                'xl6ChannelFutureCoreNotifyV1')] if args.deadline else []
            run('o'+level+'.link',[cc,*flags,'-O'+level,root/'test/cases'/fixture,*objects,*wraps,*libraries,'-o',binary])
            run('o'+level+'.run',[binary])
        report['InputsUnchanged']=seal()==before;assert report['InputsUnchanged']
        report['LogsUnchanged']=all(hashlib.sha256((out/(row['Name']+'.log')).read_bytes()).hexdigest()==row['Sha256'] for row in report['Rows'])
        assert report['LogsUnchanged'];report['Complete']=True
    except Exception as error:report['Error']=repr(error);raise
    finally:save()
    print('Native Channel Future source proof passed:',len(report['Rows']),'phases; language cutover still separate',flush=True)

if __name__=='__main__':main()
