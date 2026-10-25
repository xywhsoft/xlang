"""Check both ordinary shipping source closures against the runtime registry.

This is a source-list guard, not binary linkage or function execution proof.
Only explicit source commands enter the Windows shared-object response; no
old build-directory objects or filename glob may satisfy a required provider.
"""
from collections import Counter
from pathlib import Path
import argparse,hashlib,json,re
from runtime_sources import source_names

OBJECT=re.compile(r'^"%XL6_RELEASE_CC%"[^\n]* -c ([^\n]+\.c) -o "%XL6_BUILD_TMP%\\([A-Za-z0-9_]+\.o)"\s*$',re.M)
SOURCE=re.compile(r'\b((?:src|lib|tools)[\\/][^\s"]+\.c)')
LABELS={'runtime':'echo Building DEMO6 xl.dll runtime...',
        'compiler':'echo Building DEMO6 xlc.dll compiler runtime...'}
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inspect(root,batch=None,linux=None,unit=None,linux_sources=None):
    unit=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf-8-sig') if unit is None else unit
    body=unit.split('xl6ArtifactComponents[] = {',1)[1].split('\n};',1)[0]
    native_headers=[root/'lib/xrt/xrt.h',root/'lib/xruntime/xruntime.h']
    known=set().union(*(set(re.findall(r'\b(?:XRT|XRUNTIME)_MODULE_[A-Z0-9_]+\b',p.read_text(encoding='utf-8-sig'))) for p in native_headers))
    # Historical registry labels for the unconditional foundation. These two
    # are not optional selectors and disappear during header amalgamation.
    known.update(('XRT_MODULE_ERROR','XRT_MODULE_MEMORY'))
    requested=set(re.findall(r'"((?:XRT|XRUNTIME)_MODULE_[A-Z0-9_]+)"',body))
    assert not requested-known,('Unknown native feature declarations',sorted(requested-known))
    expected=re.findall(r'^\s*\{\s*"([^"\n]+\.c)"',body,re.M)
    assert len(expected)>=100 and len(expected)==len(set(expected)), 'Unrecognized runtime registry'
    batch=(root/'build_dll.bat').read_text(encoding='utf-8-sig') if batch is None else batch
    linux=(root/'build_linux.sh').read_text(encoding='utf-8-sig') if linux is None else linux
    shared=[m.group(1).replace('\\','/') for m in OBJECT.finditer(batch)]
    assert shared and len(shared)==len(set(shared)), 'Missing or duplicate explicit object commands'
    assert 'dll_objects.rsp' in batch, 'Shared objects are not linked'
    kernel=root/'src/api/xl_xrt_dll.c'
    assert re.search(r'^#include "xl_foreign_error.c"$',kernel.read_text(encoding='utf-8-sig'),re.M), 'Missing embedded foreign-error provider'
    errors=[];profiles=[];inputs=[root/'src/compile/xl6_compile_unit.c',root/'build_dll.bat',root/'build_linux.sh',kernel,*native_headers]
    def profile(name,direct):
        counts=Counter(direct)
        # The two shipping libraries each compile one shared XRT/error unit.
        # Foreign-error TLS is deliberately included there, not instantiated
        # a second time as an independent source on their link command.
        counts['src/api/xl_foreign_error.c']+=counts['src/api/xl_xrt_dll.c']
        missing=sorted(set(expected)-counts.keys())
        duplicate=sorted(p for p in expected if counts[p]>1)
        for kind,paths in (('missing',missing),('duplicate',duplicate)):
            errors.extend(dict(Profile=name,Kind=kind,Source=p) for p in paths)
        profiles.append(dict(Name=name,Required=len(expected),Provided=len(set(expected)&counts.keys()),Missing=missing,Duplicate=duplicate))
    for kind,label in LABELS.items():
        # The checked-in build uses inline invocations. A workspace refactor
        # may move the identical arguments to an explicit response template.
        if re.search(r'build_release_dll\.ps1[^\n]* -Target '+kind+r'\b',batch):
            template=root/'tools'/(kind+'_dll.rsp.in');text=template.read_text(encoding='utf-8-sig');inputs.append(template)
        else:
            text=batch.split(label,1)[1].split('\nif errorlevel',1)[0]
            assert '"%XL6_RELEASE_CC%"' in text,(kind,'Missing explicit DLL link invocation')
        direct=[p.replace('\\','/') for p in SOURCE.findall(text)]
        profile('windows-'+kind,shared+direct)
    assert '"$ROOT/tools/runtime_sources.py" --root "$ROOT"' in linux, 'Missing shared Linux source manifest'
    assert 'mapfile -t RUNTIME_SOURCES <<< "$RUNTIME_SOURCE_TEXT"' in linux, 'Missing Linux source array'
    direct=source_names(root) if linux_sources is None else linux_sources
    inputs.extend((root/'tools/runtime_sources.py',root/'tools/runtime_dll.rsp.in'))
    assert linux.count('"${RUNTIME_SOURCES[@]}"')>=2,'Both Linux products must consume the runtime source array'
    profile('linux-runtime',direct);profile('linux-compiler',direct)
    assert all((root/p).is_file() for p in expected),'Registry source missing'
    return errors,profiles,inputs

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,required=True);parser.add_argument('--out',type=Path)
    args=parser.parse_args();root=args.root.resolve();errors,profiles,inputs=inspect(root)
    batch=(root/'build_dll.bat').read_text(encoding='utf-8-sig');linux=(root/'build_linux.sh').read_text(encoding='utf-8-sig')
    controls=[]
    for source in ('src/runtime/xl6_task_group_future.c','src/runtime/xl6_ownership_domain_process_future.c'):
        matches=[m for m in OBJECT.finditer(batch) if m.group(1).replace('\\','/')==source]
        assert len(matches)==1,(source,'Required single explicit provider')
        match=matches[0];changed=batch[:match.start()]+batch[match.end():]
        rejected,_,_=inspect(root,batch=changed)
        controls.append(dict(Name='missing-'+Path(source).stem,Rejected=any(e['Source']==source and e['Kind']=='missing' for e in rejected)))
    source='src/runtime/xl6_task_group_future.c';match=next(m for m in OBJECT.finditer(batch) if m.group(1).replace('\\','/')==source)
    duplicate=batch+'\n'+match.group(0).replace('xl6_task_group_future.o','duplicate_task_group_future.o')+'\n'
    try:rejected,_,_=inspect(root,batch=duplicate);caught=any(e['Source']==source and e['Kind']=='duplicate' for e in rejected)
    except AssertionError:caught=True
    controls.append(dict(Name='duplicate-explicit-provider',Rejected=caught))
    direct=source_names(root);assert source in direct
    rejected,_,_=inspect(root,linux_sources=[p for p in direct if p!=source])
    controls.append(dict(Name='missing-linux-provider',Rejected=any(e['Profile'].startswith('linux-') and e['Source']==source for e in rejected)))
    unit=(root/'src/compile/xl6_compile_unit.c').read_text(encoding='utf-8-sig')
    changed=unit.replace('"XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE"','"XRT_MODULE_MISSING_REQUIRED_TEST_V1"',1);assert changed!=unit
    caught=False
    try:inspect(root,unit=changed)
    except AssertionError as error:caught='Unknown native feature declarations' in str(error)
    controls.append(dict(Name='unknown-native-feature',Rejected=caught))
    report=dict(Complete=not errors and all(c['Rejected'] for c in controls),Profiles=profiles,Errors=errors,NegativeControls=controls,
        Inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(set(inputs+[Path(__file__).resolve()]))],Scope=__doc__,BinaryLinkageAcceptance=False,Full53Acceptance=False)
    if args.out:
        assert not args.out.exists();args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(Complete=report['Complete'],Profiles=profiles,Errors=errors,NegativeControls=controls)))
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
