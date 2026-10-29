"""Stage only a test observer's private layout from the selected full product.
No hand-maintained xprocess replica, implementation, provider or public API.
The observer uses it to join a real waiter after public process completion.
"""
from pathlib import Path
import argparse,hashlib,json,re


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stage(root,out):
    root,out=Path(root).resolve(),Path(out).resolve();assert not out.exists()
    single=root/'lib/xruntime/xruntime.h';xrt=root/'lib/xrt/xrt.h';provider=root/'src/api/xl_xrt_dll.c'
    # This observer is intentionally limited to the ordinary full DLL/SO ABI.
    # Trimmed AOT objects can have different conditional private fields.
    implementation=provider.read_text(encoding='utf-8-sig')
    for macro in ('XRT_MODULE_ALL','XRUNTIME_MODULE_ALL','XRUNTIME_IMPLEMENTATION'):
        assert re.search(r'^#define '+macro+r'\s*$',implementation,re.M),(provider,macro)
    assert '#include "lib/xruntime/xruntime_features.h"\n#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)\n#define XRT_IMPLEMENTATION\n#endif\n#include "lib/xrt/xrt.h"\n#include "lib/xruntime/xruntime.h"' in implementation
    pattern=r'^struct xprocess \{\n.*?^\};'
    matches=re.findall(pattern,single.read_text(encoding='utf-8-sig'),re.M|re.S)
    other=re.findall(pattern,xrt.read_text(encoding='utf-8-sig'),re.M|re.S)
    assert len(matches)==len(other)==1 and matches==other,'Different or unknown native process layouts'
    body=matches[0]
    assert 'xthread* Waiter;' in body and '#if defined(XRT_FEATURE_PROCESS_FUTURE)' in body
    out.mkdir(parents=True)
    header=out/'process_observer_layout.h'
    header.write_text('/* Test-only exact layout from the pinned full-product XRT snapshot. */\n'
        '#ifndef XL6_TEST_PROCESS_OBSERVER_LAYOUT_H\n#define XL6_TEST_PROCESS_OBSERVER_LAYOUT_H\n'
        '#if !defined(XRT_FEATURE_PROCESS_TERMINAL) || !defined(XRT_FEATURE_PROCESS_FUTURE)\n'
        '#error "Observer requires the ordinary full product, not trimmed AOT"\n#endif\n'+body+'\n#endif\n',encoding='utf-8')
    report=dict(Complete=True,Scope=__doc__,PrivateLayoutOnly=True,ProductionApiChanged=False,
        Inputs=[dict(Path=str(p),Sha256=sha(p)) for p in (Path(__file__).resolve(),provider,single,xrt)],
        Header=dict(Path=str(header),Sha256=sha(header)))
    (out/'observer.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();print(json.dumps(stage(args.root,args.out)))
