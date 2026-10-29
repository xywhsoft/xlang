#!/usr/bin/env python3
"""Selective owned-file family sync. Never upgrades the pinned time ABI."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--xrt-root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--pinned-base',default='9cf1fc88c007f78c8733e870259c7af133cfedf1')
    p.add_argument('--native-base',default='ee5aa93a338bfc6f23104e5792925802a0d22bdf')
    a=p.parse_args();root=a.root.resolve();native=a.xrt_root.resolve();assert not a.out.exists()
    sha=lambda b:hashlib.sha256(b).hexdigest().upper()
    def git(repo,ref,path):return subprocess.check_output(['git','show',ref+':'+path],cwd=repo).decode('utf-8').replace('\r\n','\n')
    def stripped(path):return re.sub(r'(?m)^\s*#\s*include[^\n]*\n','',(native/path).read_text(encoding='utf-8-sig'))
    sources=['include/xrt/net_file_future.h','src/internal/xrt_net_file.h','src/network/file.c','src/network/file_future.c']
    declaration=stripped(sources[0]).rstrip()+'\n'
    private=stripped(sources[1]).rstrip()+'\n'
    source=stripped(sources[3]).rstrip()+'\n'
    old=git(native,a.native_base,'src/network/file.c');old=old[old.index('#if defined(XRT_FEATURE_NET_FILE)'):]
    new=stripped('src/network/file.c');new=new[new.index('#if defined(XRT_FEATURE_NET_FILE)'):]
    features=('/* net_file_future 及其直接依赖。 */\n'
        '#if defined(XRT_MODULE_ALL) || defined(XRT_MODULE_NET_FILE_FUTURE)\n'
        '#ifndef XRT_FEATURE_NET_FILE_FUTURE\n#define XRT_FEATURE_NET_FILE_FUTURE\n#endif\n'
        '#ifndef XRT_MODULE_NET_FILE\n#define XRT_MODULE_NET_FILE\n#endif\n'
        '#ifndef XRT_MODULE_FUTURE_BRIDGE\n#define XRT_MODULE_FUTURE_BRIDGE\n#endif\n'
        '#ifndef XRT_MODULE_BUFFER\n#define XRT_MODULE_BUFFER\n#endif\n#endif\n\n')
    def section(kind,name,body):return ('/* ========================================================================== */\n'
        f'/* {kind}: {name} */\n/* ========================================================================== */\n\n'+body+'\n\n')
    changes=[('/* net_file 及其直接依赖。 */',features+'/* net_file 及其直接依赖。 */'),
        (section('public','include/xrt/task_net.h','').split('\n\n')[0],
         section('public','include/xrt/net_file_future.h',declaration)+section('public','include/xrt/task_net.h','').split('\n\n')[0])]
    pending=[];records=[]
    for name in ['lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/include/xrt_decl.h','release/include/xruntime_decl.h']:
        path=root/name;raw=path.read_bytes();ending=b'\r\n' if b'\r\n' in raw else b'\n'
        baseline=git(root,a.pinned_base,''+name);body=baseline;selected=list(changes)
        if name.startswith('lib/'):
            selected += [(old,new)]
            anchor=section('source','src/network/task_net.c','').split('\n\n')[0]
            selected += [(anchor,section('source','src/network/file_future.c',source)+anchor)]
            anchor=section('source','src/network/file.c','').split('\n\n')[0]
            selected += [(anchor,section('internal','src/internal/xrt_net_file.h',private)+anchor)]
        else:
            # Declaration products force ALL temporarily, then restore and
            # reapply the caller's original selection. Extend BOTH passes and
            # preserve the newly introduced macro states as well.
            for macro in ['XRT_FEATURE_NET_FILE','XRT_MODULE_NET_FILE']:
                futureMacro=macro+'_FUTURE'
                before=f'#if !defined({macro})\n#define XRT_DECLARATIONS_RESTORE_{macro} 1\n#endif'
                after=f'#if !defined({futureMacro})\n#define XRT_DECLARATIONS_RESTORE_{futureMacro} 1\n#endif\n'+before
                selected.append((before,after))
                before=f'#if defined(XRT_DECLARATIONS_RESTORE_{macro})\n#undef {macro}\n#undef XRT_DECLARATIONS_RESTORE_{macro}\n#endif'
                after=f'#if defined(XRT_DECLARATIONS_RESTORE_{futureMacro})\n#undef {futureMacro}\n#undef XRT_DECLARATIONS_RESTORE_{futureMacro}\n#endif\n'+before
                selected.append((before,after))
        for index,(before,after) in enumerate(selected):
            count=2 if index==0 and not name.startswith('lib/') else 1
            assert body.count(before)==count,(name,'not unique',before[:90]);body=body.replace(before,after)
        restored=body
        for index in reversed(range(len(selected))):
            before,after=selected[index];count=2 if index==0 and not name.startswith('lib/') else 1
            assert restored.count(after)==count,(name,'reverse not unique');restored=restored.replace(after,before)
        assert restored==baseline,(name,'out of scope')
        beforeRaw=baseline.encode('utf-8').replace(b'\n',ending)
        data=body.encode('utf-8').replace(b'\n',ending)
        assert sha(raw) in {sha(beforeRaw),sha(data)},(name,'unrecorded workspace changes')
        records.append(dict(Path=name,BeforeSha256=sha(beforeRaw),AfterSha256=sha(data),AddedBytes=len(data)-len(beforeRaw)))
        pending.append((path,data))
    for path,data in pending:path.write_bytes(data)
    sys.path.insert(0,str(root/'tools'));import sync_xrt_snapshot as metadata
    from gen_xrt_symbol_table import _api_names,_render
    from gen_xrt_xlang_matrix import sync
    for product in ['xrt','xruntime']:
        impl=root/f'lib/{product}/{product}.h';decl=root/f'release/include/{product}_decl.h'
        names=set(_api_names(impl));core=set(_api_names(root/'lib/xrt/xrt.h'))
        upstream='xrt/single/xrt.h' if product=='xrt' else 'xrt/extlibs/xruntime/single/xruntime.h'
        upstreamDecl=upstream.replace('.h','_decl.h')
        value=metadata._metadata(product,upstream,upstreamDecl,impl.read_bytes(),decl.read_bytes(),len(names),len(names-core) if product=='xruntime' else None)
        (root/f'lib/{product}/{product}.snapshot.json').write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
    (root/'src/api/xl_xrt_symbols.inc').write_text(_render(root/'lib/xruntime/xruntime.h'),encoding='utf-8')
    sync(root);assert metadata._check_snapshot(root)
    result=dict(Complete=True,SelectiveSync=True,PinnedBase=a.pinned_base,NativeBase=a.native_base,
        TimeAbiUpgraded=False,PreservedPinnedMicrosecondsUntil=True,IndependentMillisecondsForUnchanged=True,
        Records=records,Sources=[dict(Path=n,Sha256=sha((native/n).read_bytes())) for n in sources])
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
