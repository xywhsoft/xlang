#!/usr/bin/env python3
"""Selective additive TaskNet ownership sync, preserving the pinned clock ABI.
Never copies whole independent XRT headers. Only the owned declaration/source
fragments are translated; every other byte in the four pinned headers remains
unchanged. No published DLL/SO/VFS is rebuilt here.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path

def sha(data): return hashlib.sha256(data).hexdigest().upper()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--xrt-root',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();native=a.xrt_root.resolve()
    assert not a.out.exists(),'Preserve prior receipt'
    h=(native/'include/xrt/task_net.h').read_text(encoding='utf8')
    c=(native/'src/network/task_net.c').read_text(encoding='utf8')
    start=h.index('/* Closed resident contract for one user Data reference')
    end=h.index('XRT_EXTERN_C_END',start)+len('XRT_EXTERN_C_END')
    decl=h[start:end]+'\n'
    begin='#if defined(XRT_FEATURE_TASK_NET)\n/* ONE physical activation'
    start=c.index(begin);source=c[start:];assert source.rstrip().endswith('#endif')
    assert source.count('__xrtTaskNetUntilDataPolicyV1(')==2
    # Public For is only a relative-time alias in independent XRT. Pinned
    # Until takes an absolute microsecond deadline, not the relative alias.
    first=decl.index('XRT_API xfuture* xrtTaskNetForDataPolicyV1(')
    last=decl.index(';',first)+1
    decl=decl[:first]+decl[first:last].replace('xrtTaskNetForDataPolicyV1','xrtTaskNetUntilDataPolicyV1').replace('int64 iTimeout','xdeadline iDeadline')+decl[last:]
    decl=decl.replace('int64 iTimeout','uint64 iTimeout')
    first=source.index('XRT_API xfuture* xrtTaskNetForDataPolicyV1(')
    source=source[:first]+'#endif\n'
    source=source.replace('__xrtTaskNetUntilDataPolicyV1','xrtTaskNetUntilDataPolicyV1')
    source=source.replace('double deadline','xdeadline deadline')
    source=source.replace('int64 timeout','uint64 timeout')
    source=source.replace('__xrtWaitAfter(timeout)','xrtDeadlineAfter(timeout)')
    source=source.replace('__xrtNetEngineScheduleOwnedV1','xrtNetEngineScheduleOwnedV1')
    source=source.replace('    if (!__xrtWaitValid(deadline)) return NULL;','    /* uint64 deadlines include UINT64_MAX as never. */')
    assert '__xrtWait' not in source and 'double deadline' not in source
    paths=['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']
    records=[];pending=[]
    anchor='typedef xtaskoutcome (*xtasknetproc)(\n\txnetworker* pWorker,\n\txcancel* pCancel,\n\tptr pData,\n\txtaskvalue* pResult\n);'
    for name in paths:
        path=root/name;raw=path.read_bytes();ending=b'\r\n' if b'\r\n' in raw else b'\n'
        body=raw.decode('utf8').replace('\r\n','\n')
        assert body.count(anchor)==1 and 'xtasknetdataownershipv1' not in body,name
        before=body;at=body.index(anchor)+len(anchor);body=body[:at]+'\n\n'+decl+body[at:]
        if name.endswith('/xrt.h') or name.endswith('/xruntime.h'):
            start=body.index('/* source: src/network/task_net.c */')
            end=body.index('/* source: src/network/task_group_net.c */',start)
            at=body.rfind('\n#endif',start,end)
            assert at>start;body=body[:at]+'\n'+source+body[at:]
            restored=body.replace('\n'+source,'',1).replace('\n\n'+decl,'',1)
        else:restored=body.replace('\n\n'+decl,'',1)
        assert restored==before,(name,'out-of-scope change')
        encoded=body.encode('utf8').replace(b'\n',ending)
        assert len(encoded)>len(raw)
        pending.append((path,encoded))
        records.append(dict(Path=name,BeforeSha256=sha(raw),AfterSha256=sha(encoded),AddedBytes=len(encoded)-len(raw)))
    for path,data in pending:path.write_bytes(data)
    # Refresh identities and generated listings from the patched pinned assets;
    # never invoke --sync against the newer independent whole-header ABI.
    sys.path.insert(0,str(root/'tools'))
    import sync_xrt_snapshot as metadata
    from gen_xrt_symbol_table import _api_names,_render
    from gen_xrt_xlang_matrix import sync
    for product,impl,declpath,upstream,upstreamdecl in (
        ('xrt','lib/xrt/xrt.h','release/include/xrt_decl.h','xrt/single/xrt.h','xrt/single/xrt_decl.h'),
        ('xruntime','lib/xruntime/xruntime.h','release/include/xruntime_decl.h','xrt/extlibs/xruntime/single/xruntime.h','xrt/extlibs/xruntime/single/xruntime_decl.h')):
        names=set(_api_names(root/impl));core=set(_api_names(root/'lib/xrt/xrt.h'))
        value=metadata._metadata(product,upstream,upstreamdecl,(root/impl).read_bytes(),(root/declpath).read_bytes(),len(names),len(names-core) if product=='xruntime' else None)
        (root/('lib/'+product+'/'+product+'.snapshot.json')).write_text(json.dumps(value,indent=2)+'\n',encoding='utf8',newline='\n')
    (root/'src/api/xl_xrt_symbols.inc').write_text(_render(root/'lib/xruntime/xruntime.h'),encoding='utf8',newline='\n')
    sync(root)
    assert metadata._check_snapshot(root)
    value=dict(SelectiveSync=True,PreservedPinnedMicrosecondsUntil=True,IndependentMillisecondsForUnchanged=True,WholeTimeAbiUpgrade=False,Records=records,
        SourceHeaderSha256=sha((native/'include/xrt/task_net.h').read_bytes()),SourceImplementationSha256=sha((native/'src/network/task_net.c').read_bytes()))
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(value,indent=2)+'\n',encoding='utf8')
    print(json.dumps(value,indent=2))
if __name__=='__main__':main()

